# IDAPython script, run through idacall.py:
#   python idacall.py py dump_folder.py                                   -> list function folders
#   python idacall.py py dump_folder.py FOLDER="frontend\cars.c" OUT=<file> -> dump one folder to a text report
#   python idacall.py py dump_folder.py FUNCS="MainInit,MainExit" OUT=<file> -> dump chosen functions (any folders)
#
# The report contains everything needed to rebuild the source file: decompiled functions,
# the data they reference (decoded field by field), called externals and the struct types used.
import ida_dirtree, ida_hexrays, ida_funcs, ida_name, ida_nalt, ida_typeinf, ida_bytes, ida_segment, idautils, idc

FOLDER = globals().get("FOLDER", "")
FUNCS = globals().get("FUNCS", "")
OUT = globals().get("OUT", "")
tree = ida_dirtree.get_std_dirtree(ida_dirtree.DIRTREE_FUNCS)


def entries(path):
    it = ida_dirtree.dirtree_iterator_t()
    ok = tree.findfirst(it, path.rstrip("/") + "/*")
    while ok:
        yield tree.get_abspath(it.cursor), tree.resolve_cursor(it.cursor)
        ok = tree.findnext(it)


def list_folders(path="", depth=0):
    for p, de in entries(path):
        if de.isdir:
            n = sum(1 for _, d in entries(p) if not d.isdir)
            print("%s%s  (%d funcs)" % ("  " * depth, p.lstrip("/"), n))
            list_folders(p, depth + 1)


def tinfo_at(ea):
    t = ida_typeinf.tinfo_t()
    return t if ida_nalt.get_tinfo(t, ea) else None


def sym(v):
    n = ida_name.get_name(v)
    if not n:
        return hex(v)
    s = ida_bytes.get_strlit_contents(v, -1, ida_nalt.STRTYPE_C) if ida_bytes.is_strlit(ida_bytes.get_flags(v)) else None
    return "%s %r" % (n, s.decode("latin-1")) if s is not None else n


def decode(ea, t, indent=""):
    """Render the initialized value of a data item of type t."""
    size = t.get_size()
    if size in (0, idc.BADADDR) or size > 0x10000:
        size = ida_bytes.get_item_size(ea)
    if not ida_bytes.is_loaded(ea):
        return "<uninitialized (bss)>"
    if t.is_array():
        el = t.get_array_element()
        if el.get_size() == 1 and el.is_integral():
            raw = ida_bytes.get_bytes(ea, size) or b""
            return repr(raw.split(b"\0")[0].decode("latin-1")) + ("" if raw.strip(b"\0") else "  (zeroed)")
        n = size // max(el.get_size(), 1)
        if n > 64:
            return "{ %d elements, first: %s ... }" % (n, decode(ea, el, indent))
        return "{\n" + "".join("%s  [%d] %s,\n" % (indent, i, decode(ea + i * el.get_size(), el, indent + "  ")) for i in range(n)) + indent + "}"
    if t.is_udt():
        udt = ida_typeinf.udt_type_data_t()
        t.get_udt_details(udt)
        parts = ["%s=%s" % (m.name, decode(ea + m.offset // 8, m.type, indent)) for m in udt]
        return "{ " + ", ".join(parts) + " }"
    if t.is_ptr() or t.is_funcptr():
        v = ida_bytes.get_dword(ea)
        return "NULL" if v == 0 else sym(v)
    val = {1: ida_bytes.get_byte, 2: ida_bytes.get_word, 4: ida_bytes.get_dword, 8: ida_bytes.get_qword}.get(size)
    if not val:
        return (ida_bytes.get_bytes(ea, size) or b"").hex()
    v = val(ea)
    if t.is_signed() and v >= 1 << (size * 8 - 1):
        v -= 1 << (size * 8)
    return str(v)


def raw_dwords(ea, size):
    if not ida_bytes.is_loaded(ea):
        return "<uninitialized (bss)>"
    return "[" + ", ".join(sym(ida_bytes.get_dword(ea + i)) if ida_name.get_name(ida_bytes.get_dword(ea + i)) else hex(ida_bytes.get_dword(ea + i))
                           for i in range(0, min(size, 0x100), 4)) + "]"


def user_comments(f):
    """Non-blank comments the user typed: Hex-Rays pseudocode comments (blank ones are just
    line spacing) and anterior/posterior lines in the listing. Auto comments are not included."""
    out = []
    uc = ida_hexrays.restore_user_cmts(f)
    if uc:
        it = ida_hexrays.user_cmts_begin(uc)
        while it != ida_hexrays.user_cmts_end(uc):
            t = str(ida_hexrays.user_cmts_second(it)).strip()
            if t:
                out.append((ida_hexrays.user_cmts_first(it).ea, t))
            it = ida_hexrays.user_cmts_next(it)
        ida_hexrays.user_cmts_free(uc)
    for h in idautils.FuncItems(f):
        out += [(h, l) for l in item_extra_lines(h)]
    return sorted(out)


def item_extra_lines(ea):
    lines = []
    for base in (idc.E_PREV, idc.E_NEXT):
        for i in range(32):
            l = idc.get_extra_cmt(ea, base + i)
            if l is None:
                break
            if l.strip():
                lines.append(l.strip())
    return lines


def collect_udts(t, acc):
    while t.is_ptr() or t.is_array():
        t = t.get_pointed_object() if t.is_ptr() else t.get_array_element()
    if t.is_func():
        fd = ida_typeinf.func_type_data_t()
        t.get_func_details(fd)
        collect_udts(fd.rettype, acc)
        for a in fd:
            collect_udts(a.type, acc)
        return
    if t.is_udt() and t.get_type_name() and t.get_type_name() not in acc:
        acc[t.get_type_name()] = t
        udt = ida_typeinf.udt_type_data_t()
        t.get_udt_details(udt)
        for m in udt:
            collect_udts(m.type, acc)


def folder_of(ea):
    try:
        return tree.get_abspath(tree.find_entry(ida_dirtree.direntry_t(ea, False))).rsplit("/", 1)[0].lstrip("/")
    except Exception:
        return ""


def dump(folder, names=""):
    if names:
        funcs = []
        for n in names.split(","):
            n = n.strip()
            ea = ida_name.get_name_ea(idc.BADADDR, n)
            if ea == idc.BADADDR:
                ea = ida_name.get_name_ea(idc.BADADDR, "_" + n)
            if ea == idc.BADADDR:
                raise SystemExit("function %r not found" % n)
            funcs.append(ea)
        funcs = sorted(funcs)
        title = "functions: " + ", ".join("%s [%s]" % (ida_name.get_name(f), folder_of(f)) for f in funcs)
    else:
        path = "/" + folder.strip("/")
        funcs = sorted(de.idx for _, de in entries(path) if not de.isdir)
        if not funcs:
            raise SystemExit("no functions in folder %r (run without FOLDER to list folders)" % folder)
        title = "IDA folder: %s  (%d functions)" % (folder, len(funcs))
    lines = ["# " + title, ""]
    data, calls, udts = set(), set(), {}
    for f in funcs:
        for h in idautils.FuncItems(f):
            for x in idautils.DataRefsFrom(h):
                seg = ida_segment.getseg(x)
                # skip switch tables inside code and import thunks (.idata)
                if seg and seg.type == ida_segment.SEG_DATA and ida_segment.get_segm_name(seg) != ".idata":
                    data.add(ida_bytes.get_item_head(x))
            for x in idautils.CodeRefsFrom(h, 0):
                fn = ida_funcs.get_func(x)
                if fn and fn.start_ea == x and x not in funcs:
                    calls.add(x)
    # data tables may point to other data (strings, buffers) and to functions of this folder
    for ea in list(data):
        t = tinfo_at(ea)
        for i in range(0, ida_bytes.get_item_size(ea), 4):
            v = ida_bytes.get_dword(ea + i)
            if ida_segment.getseg(v) and not ida_funcs.get_func(v) and ida_name.get_name(v) and v not in data:
                data.add(v)

    lines.append("## Functions")
    for f in funcs:
        t = tinfo_at(f)
        if t:
            collect_udts(t, udts)
        cmt = idc.get_func_cmt(f, 0) or idc.get_func_cmt(f, 1)
        lines.append("### [%s] %s  size=%d%s" % (hex(f), ida_name.get_name(f), ida_funcs.get_func(f).size(), ("  // " + cmt) if cmt else ""))
        err = ida_hexrays.hexrays_failure_t()
        try:
            cf = ida_hexrays.decompile_func(ida_funcs.get_func(f), err, 0)
        except Exception as e:
            cf, err.str = None, str(e)
        cmts = user_comments(f)
        if cmts:
            lines.append("// IDA user comments (carry over, see CLAUDE.md):")
            lines.extend("//   [%s] %s" % (hex(a), t.replace("\n", "\n//     ")) for a, t in cmts)
        if cf:
            lines.append(str(cf))
        else:
            # translate from the disassembly (e.g. Hex-Rays gives up on some switch jump tables)
            lines.append("// decompile failed: %s at %s; disassembly follows" % (err.str, hex(err.errea)))
            fn = ida_funcs.get_func(f)
            a = fn.start_ea
            while a < fn.end_ea:
                lines.append("%s  %s" % (hex(a), idc.generate_disasm_line(a, 0)))
                a = idc.next_head(a, fn.end_ea)
        lines.append("")

    lines.append("## Data referenced (in address order)")
    for ea in sorted(data):
        t = tinfo_at(ea)
        size = ida_bytes.get_item_size(ea)
        seg = ida_segment.get_segm_name(ida_segment.getseg(ea))
        lines.append("### [%s] %s  type=%s size=%d seg=%s" % (hex(ea), ida_name.get_name(ea) or "(unnamed)", t or "-", size, seg))
        extra = item_extra_lines(ea) + [c for c in (idc.get_cmt(ea, 0), idc.get_cmt(ea, 1)) if c]
        if extra:
            lines.append("// IDA comments: " + " | ".join(extra))
        if t:
            collect_udts(t, udts)
            lines.append(decode(ea, t))
        else:
            lines.append("raw dwords: " + raw_dwords(ea, size))
        lines.append("xrefs: " + ", ".join(sorted({idc.get_func_name(x.frm) or hex(x.frm) for x in idautils.XrefsTo(ea)}))[:400])
        lines.append("")

    lines.append("## Called externals")
    for ea in sorted(calls, key=lambda e: ida_name.get_name(e)):
        t = tinfo_at(ea)
        if t:
            collect_udts(t, udts)
        tgt = ea
        # resolve j__ thunks to the real function
        fn = ida_funcs.get_func(ea)
        if fn and fn.flags & ida_funcs.FUNC_THUNK:
            tgt = next(iter(idautils.CodeRefsFrom(fn.start_ea, 0)), idc.BADADDR)
            if tgt != idc.BADADDR:
                t2 = tinfo_at(tgt)
                if t2:
                    collect_udts(t2, udts)
                    t = t2
        try:
            p = tree.get_abspath(tree.find_entry(ida_dirtree.direntry_t(tgt, False)))
        except Exception:
            p = ""
        lines.append("%s -> %s : %s   [folder %s]" % (ida_name.get_name(ea), ida_name.get_name(tgt), t or "?", p.rsplit("/", 1)[0]))
    lines.append("")

    lines.append("## Struct types used")
    for n, t in sorted(udts.items()):
        nt = ida_typeinf.tinfo_t()
        if not nt.get_named_type(None, n):
            nt = t
        decl = nt._print(None, ida_typeinf.PRTYPE_MULTI | ida_typeinf.PRTYPE_TYPE | ida_typeinf.PRTYPE_DEF)
        decl = (decl or "struct\n{ /* could not print */ }").replace("\n{", " %s\n{" % n, 1)
        lines.append(decl.rstrip() + ";")
        lines.append("")
    text = "\n".join(lines)
    if OUT:
        open(OUT, "w", encoding="utf-8").write(text)
        print("wrote %s (%d functions, %d data items, %d externals, %d types)" % (OUT, len(funcs), len(data), len(calls), len(udts)))
    else:
        print(text)


if FUNCS:
    dump("", FUNCS)
elif FOLDER:
    dump(FOLDER)
else:
    list_folders()
