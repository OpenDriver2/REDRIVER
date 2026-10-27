# IDAPython script, run through idacall.py:
#   python idacall.py py list_types.py [OUT=<file>] [GLIDE=<glide include dir>]
# Writes "folder<TAB>kind<TAB>size<TAB>origin<TAB>name" for every named local type (one per line).
# folder: the local types folder ("" for the root); kind: struct, union, enum, typedef, func;
# origin: "system" if a standard type library of the database has the name (Windows, DirectX, CRT),
# "glide" if the Glide SDK headers name it, else "game". Anonymous member types (X::$...) are skipped.
import os, re
import ida_dirtree, ida_typeinf

OUT = globals().get("OUT", "")
GLIDE = globals().get("GLIDE", "") or os.path.normpath(os.path.join(
    os.path.dirname(OUT or "."), r"..\..\src_rebuild\dependency\glideSDK\glide2x\include"))

til = ida_typeinf.get_idati()

glide_names = set()
if os.path.isdir(GLIDE):
    for fn in os.listdir(GLIDE):
        if fn.lower().endswith(".h"):
            text = open(os.path.join(GLIDE, fn), encoding="latin-1").read()
            glide_names.update(re.findall(r"\b(Gr\w+|Gu\w+|Fx\w+|FX\w+|Sst\w+|sst\w+)\b", text))


# Windows/DirectX/compiler types that came from parsed headers rather than a type library
SYSTEM_RE = re.compile(r"^(_?(SCOPETABLE|EH3|PEH3|PSCOPETABLE)\w*|REF\w+|VOID|L?P(R?G)?A?L?PVOID\w*|LPVOIDV|"
                       r"L?P?SGBUFFER|L?PDIRECT\w+|IDirect\w+|L?P?_?DP\w+|L?P?_?DPLMSG\w+)$")


def in_base_til(name):
    for i in range(til.nbases):
        t = ida_typeinf.tinfo_t()
        if t.get_named_type(til.base(i), name):
            return True
    return False


def kind(t):
    if t.is_typedef():
        return "typedef"
    if t.is_struct():
        return "struct"
    if t.is_union():
        return "union"
    if t.is_enum():
        return "enum"
    if t.is_func():
        return "func"
    return "other"


folders = {}
tree = ida_dirtree.get_std_dirtree(ida_dirtree.DIRTREE_LOCAL_TYPES)


def walk(path):
    it = ida_dirtree.dirtree_iterator_t()
    ok = tree.findfirst(it, path.rstrip("/") + "/*")
    while ok:
        de = tree.resolve_cursor(it.cursor)
        if de.isdir:
            walk(tree.get_abspath(it.cursor))
        else:
            folders[de.idx] = path.lstrip("/")
        ok = tree.findnext(it)


walk("")

rows = []
for o in range(1, ida_typeinf.get_ordinal_limit(til)):
    t = ida_typeinf.tinfo_t()
    if not t.get_numbered_type(til, o):
        continue
    name = t.get_type_name() or ""
    if not name or "::" in name or "$" in name:
        continue
    folder = folders.get(o, "")
    if name in glide_names or folder == "<glide.h>":
        origin = "glide"
    elif not folder and (in_base_til(name) or SYSTEM_RE.match(name)):
        origin = "system"
    else:
        origin = "game"
    size = t.get_size()
    rows.append("%s\t%s\t%d\t%s\t%s" % (folder, kind(t), size if size != ida_typeinf.BADSIZE else 0, origin, name))

text = "\n".join(rows) + "\n"
if OUT:
    open(OUT, "w", encoding="utf-8").write(text)
    print("wrote %s (%d types)" % (OUT, len(rows)))
else:
    print(text)
