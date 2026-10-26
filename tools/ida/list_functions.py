# IDAPython script, run through idacall.py:
#   python idacall.py py list_functions.py [OUT=<file>]
# Writes "folder<TAB>address<TAB>name" for every function that sits in a folder (one per line).
import ida_dirtree, ida_name

OUT = globals().get("OUT", "")
tree = ida_dirtree.get_std_dirtree(ida_dirtree.DIRTREE_FUNCS)
rows = []


def walk(path):
    it = ida_dirtree.dirtree_iterator_t()
    ok = tree.findfirst(it, path.rstrip("/") + "/*")
    while ok:
        de = tree.resolve_cursor(it.cursor)
        if de.isdir:
            walk(tree.get_abspath(it.cursor))
        elif path:
            rows.append("%s\t%s\t%s" % (path.lstrip("/"), hex(de.idx), ida_name.get_name(de.idx)))
        ok = tree.findnext(it)


walk("")
text = "\n".join(rows)
if OUT:
    open(OUT, "w", encoding="utf-8").write(text)
    print("wrote %s (%d functions)" % (OUT, len(rows)))
else:
    print(text)
