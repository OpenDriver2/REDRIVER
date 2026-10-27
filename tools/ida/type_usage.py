# IDAPython script, run through idacall.py:
#   python idacall.py py type_usage.py [OUT=<file>] [FOLDERS=<folder>,<folder>...]
# For every function in a function folder, collects the named local types it uses: prototype,
# Hex-Rays local variables, referenced globals, struct members accessed, casts. Then lists, for
# every named local type, the folders using it and the named types its definition needs
# (struct/union members, typedef targets, function pointer signatures).
# Writes "type<TAB>folder,folder...<TAB>dep,dep..." (one line per type; folders may be empty).
# Decompiles every function: a full run takes several minutes.
import ida_dirtree, ida_funcs, ida_hexrays, ida_nalt, ida_typeinf, ida_bytes

OUT = globals().get("OUT", "")
ONLY = set(f for f in globals().get("FOLDERS", "").split(",") if f)

til = ida_typeinf.get_idati()


def named(t, acc, depth=0):
    """Add the names of named types reachable from t without entering their definitions."""
    if depth > 20 or t is None:
        return
    name = t.get_type_name()
    if name:
        acc.add(name)
        return
    inner(t, acc, depth)


def inner(t, acc, depth=0):
    """Like named(), but looks inside t itself (t may be a named type)."""
    if t.is_ptr():
        named(t.get_pointed_object(), acc, depth + 1)
    elif t.is_array():
        named(t.get_array_element(), acc, depth + 1)
    elif t.is_func():
        fd = ida_typeinf.func_type_data_t()
        if t.get_func_details(fd):
            named(fd.rettype, acc, depth + 1)
            for a in fd:
                named(a.type, acc, depth + 1)
    elif t.is_udt():
        # anonymous struct/union: its members
        u = ida_typeinf.udt_type_data_t()
        if t.get_udt_details(u):
            for m in u:
                named(m.type, acc, depth + 1)


def definition_deps(name):
    t = ida_typeinf.tinfo_t()
    if not t.get_named_type(til, name):
        return set()
    acc = set()
    nxt = t.get_next_type_name() if t.is_typedef() else None
    if nxt:
        acc.add(nxt)     # typedef of a named type
    else:
        inner(t, acc)    # struct/union members, pointer/array/function target
    acc.discard(name)
    return acc


class Collector(ida_hexrays.ctree_visitor_t):
    def __init__(self, acc):
        ida_hexrays.ctree_visitor_t.__init__(self, ida_hexrays.CV_FAST)
        self.acc = acc

    def visit_expr(self, e):
        named(e.type, self.acc)
        if e.op == ida_hexrays.cot_obj:
            t = ida_typeinf.tinfo_t()
            if ida_nalt.get_tinfo(t, e.obj_ea):
                named(t, self.acc)
        return 0


# function folders
funcs = {}
tree = ida_dirtree.get_std_dirtree(ida_dirtree.DIRTREE_FUNCS)


def walk(path):
    it = ida_dirtree.dirtree_iterator_t()
    ok = tree.findfirst(it, path.rstrip("/") + "/*")
    while ok:
        de = tree.resolve_cursor(it.cursor)
        if de.isdir:
            walk(tree.get_abspath(it.cursor))
        elif path:
            funcs[de.idx] = path.lstrip("/")
        ok = tree.findnext(it)


walk("")

usage = {}
failed = 0
for ea, folder in sorted(funcs.items()):
    if folder == "[THUNKS]" or (ONLY and folder not in ONLY):
        continue
    acc = set()
    t = ida_typeinf.tinfo_t()
    if ida_nalt.get_tinfo(t, ea):
        named(t, acc)
    try:
        cf = ida_hexrays.decompile(ea)
    except ida_hexrays.DecompilationFailure:
        cf = None
    if cf:
        named(cf.type, acc)
        for lv in cf.get_lvars():
            named(lv.type(), acc)
        Collector(acc).apply_to(cf.body, None)
    else:
        failed += 1
    for name in acc:
        usage.setdefault(name, set()).add(folder)

rows = []
seen = set()
for o in range(1, ida_typeinf.get_ordinal_limit(til)):
    t = ida_typeinf.tinfo_t()
    if not t.get_numbered_type(til, o):
        continue
    name = t.get_type_name() or ""
    if not name or "::" in name or "$" in name or name in seen:
        continue
    seen.add(name)
    rows.append("%s\t%s\t%s" % (name, ",".join(sorted(usage.get(name, ()))), ",".join(sorted(definition_deps(name)))))

text = "\n".join(rows) + "\n"
if OUT:
    open(OUT, "w", encoding="utf-8").write(text)
    print("wrote %s (%d types, %d functions, %d not decompiled)" % (OUT, len(rows), len(funcs), failed))
else:
    print(text)
