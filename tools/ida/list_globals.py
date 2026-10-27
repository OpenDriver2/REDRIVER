# IDAPython script, run through idacall.py:
#   python idacall.py py list_globals.py [OUT=<file>]
# Writes one line per global data item (IDA name "_Name"; statics "file._name" and unnamed items
# are skipped):
#   address<TAB>name<TAB>size<TAB>declaration<TAB>mark<TAB>users
# declaration: the IDA type printed as a C declaration ("-" when untyped); mark: the user's
# "@file..." anterior line, if any; users: function folders whose functions reference the item,
# with the number of references ("folder:count"), plus "<data>" when other data refers to it
# (pointer tables, menus).
import collections, idautils, idc, ida_dirtree, ida_funcs, ida_lines, ida_name, ida_nalt, ida_segment, ida_typeinf, ida_bytes

OUT = globals().get("OUT", "")

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


def mark(ea):
    i = 0
    while True:
        l = ida_lines.get_extra_cmt(ea, ida_lines.E_PREV + i)
        if l is None:
            return ""
        if l.startswith("@"):
            return l.strip()
        i += 1


rows = []
for s in idautils.Segments():
    seg = ida_segment.getseg(s)
    if ida_segment.get_segm_name(seg) not in (".data", ".rdata", ".bss"):
        continue
    for ea in idautils.Heads(seg.start_ea, seg.end_ea):
        name = ida_name.get_name(ea)
        if not name.startswith("_") or name.startswith("__") or "." in name or "$" in name or "@" in name:
            continue
        cname = name[1:]
        tif = ida_typeinf.tinfo_t()
        if ida_nalt.get_tinfo(tif, ea):
            decl = ida_typeinf.print_tinfo("", 0, 0, ida_typeinf.PRTYPE_1LINE, tif, cname, "") or "-"
        else:
            decl = "-"
        users = collections.Counter()
        for x in idautils.XrefsTo(ea):
            f = ida_funcs.get_func(x.frm)
            if f:
                users[folders.get(f.start_ea, "<unsorted>")] += 1
            else:
                users["<data>"] += 1
        rows.append("%s\t%s\t%d\t%s\t%s\t%s" % (hex(ea), cname, ida_bytes.get_item_size(ea), decl, mark(ea),
                                               ",".join("%s:%d" % (u, users[u]) for u in sorted(users))))

text = "\n".join(rows) + "\n"
if OUT:
    open(OUT, "w", encoding="utf-8").write(text)
    print("wrote %s (%d globals)" % (OUT, len(rows)))
else:
    print(text)
