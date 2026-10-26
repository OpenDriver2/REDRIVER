# IDAPython script, run through idacall.py:
#   python idacall.py py call_tree.py ROOTS="InitFrontend,HandleMenu" [DEPTH=4] [SRC=<src_rebuild\game6.4g>] [OUT=<file>]
# Prints the call tree below the given functions: address, size, IDA folder, and "+" for functions
# already translated (an "// [0xADDR]" marker exists in SRC), "-" for untranslated ones.
# Thunks (j__X) are followed to their target. A function seen before is printed once, then "...".
import os, re
import idautils, idc, ida_funcs, ida_name, ida_dirtree

ROOTS = globals().get("ROOTS", "")
DEPTH = int(globals().get("DEPTH", "4"))
SRC = globals().get("SRC", r"D:\Projects\OpenDriver1\Driver\src_rebuild\game6.4g")
OUT = globals().get("OUT", "")

done = set()
for dirpath, _, files in os.walk(SRC):
    for f in files:
        if f.endswith(".c"):
            for m in re.finditer(r"^// \[0x([0-9a-fA-F]+)\]", open(os.path.join(dirpath, f), encoding="utf-8", errors="replace").read(), re.M):
                done.add(int(m.group(1), 16))

folders = {}
tree = ida_dirtree.get_std_dirtree(ida_dirtree.DIRTREE_FUNCS)


def walk(path):
    it = ida_dirtree.dirtree_iterator_t()
    ok = tree.findfirst(it, path.rstrip("/") + "/*")
    while ok:
        de = tree.resolve_cursor(it.cursor)
        if de.isdir:
            walk(tree.get_abspath(it.cursor))
        elif path:
            folders[de.idx] = path.lstrip("/")
        ok = tree.findnext(it)


walk("")


def thunk_target(ea):
    f = ida_funcs.get_func(ea)
    # calc_thunk_func_target() gives wrong addresses here; follow the thunk's jmp instead
    if f and (f.flags & ida_funcs.FUNC_THUNK):
        for x in idautils.XrefsFrom(f.start_ea, 0):
            if x.type in (idc.fl_JN, idc.fl_JF):
                return x.to
    return ea


def callees(ea):
    f = ida_funcs.get_func(ea)
    out = []
    if not f:
        return out
    for item in idautils.FuncItems(f.start_ea):
        for x in idautils.XrefsFrom(item, 0):
            if x.type in (idc.fl_CN, idc.fl_CF, idc.fl_JN, idc.fl_JF) or (x.type == idc.dr_O and ida_funcs.get_func(x.to) and ida_funcs.get_func(x.to).start_ea == x.to):
                g = ida_funcs.get_func(x.to)
                if g and g.start_ea == x.to and g.start_ea != f.start_ea:
                    t = thunk_target(x.to)
                    if t not in out:
                        out.append(t)
    return out


lines = []
seen = set()


def show(ea, depth, indent):
    f = ida_funcs.get_func(ea)
    size = f.end_ea - f.start_ea if f else 0
    name = ida_name.get_name(ea)
    mark = "+" if ea in done else "-"
    again = ea in seen
    lines.append("%s%s %s %s  [%s] size=%d%s" % ("  " * indent, mark, hex(ea), name, folders.get(ea, "?"), size, " ..." if again and callees(ea) else ""))
    if again or depth >= DEPTH:
        return
    seen.add(ea)
    for c in callees(ea):
        if folders.get(c) == "[THUNKS]" or idc.get_segm_name(c) in ("extern", ".idata"):
            continue
        g = ida_funcs.get_func(c)
        if not g or g.flags & ida_funcs.FUNC_LIB:
            continue
        show(c, depth + 1, indent + 1)


for r in ROOTS.split(","):
    r = r.strip()
    ea = ida_name.get_name_ea(idc.BADADDR, "_" + r)
    if ea == idc.BADADDR:
        ea = ida_name.get_name_ea(idc.BADADDR, r)
    if ea == idc.BADADDR:
        lines.append("? " + r)
        continue
    show(ea, 0, 0)

text = "\n".join(lines)
if OUT:
    open(OUT, "w", encoding="utf-8").write(text)
    print("wrote %s (%d lines)" % (OUT, len(lines)))
else:
    print(text)
