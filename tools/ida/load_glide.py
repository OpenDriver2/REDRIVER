# IDAPython script, run through idacall.py after glide_decls.py:
#   python idacall.py py load_glide.py DIR=<out_dir of glide_decls.py> [DRY=1]
#
# 1. Parses glide_types.h into the database's local types (existing same-named types are replaced;
#    their old definitions are printed first so a mismatch is visible).
# 2. Applies glide.h prototypes to the emulation functions d3dgl_X / d3dgu_X (= grX / guX)
#    and to the function-pointer table grX / gr_X / guX that UseD3DDevice fills.
#    Functions whose argument count differs from the current IDA prototype are skipped and reported.
# DRY=1 only parses into a scratch til and reports, without touching the database.
import os, re
import ida_typeinf, ida_name, ida_nalt, ida_funcs, idc

DIR = globals()["DIR"]
DRY = globals().get("DRY", "") == "1"
idati = ida_typeinf.get_idati()
types_text = open(os.path.join(DIR, "glide_types.h")).read()
funcs = [l.strip() for l in open(os.path.join(DIR, "glide_funcs.txt")) if l.strip()]
type_names = re.findall(r"^(?:enum|struct|union) (\w+)|^typedef [^;{]*?(\w+)\s*;$", types_text, re.M)
type_names = [a or b for a, b in type_names]


def decl(til, name):
    t = ida_typeinf.tinfo_t()
    if not t.get_named_type(til, name):
        return None
    return t._print(None, ida_typeinf.PRTYPE_1LINE | ida_typeinf.PRTYPE_DEF)


# --- 2. prototypes ---
def nargs(t):
    fd = ida_typeinf.func_type_data_t()
    return fd.size() if t.get_func_details(fd) else -1


def apply_prototypes(til, dry):
    applied, skipped, missing = [], [], []
    for line in funcs:
        tif = ida_typeinf.tinfo_t()
        name = ida_typeinf.parse_decl(tif, til, line, ida_typeinf.PT_SIL)
        if not name:
            skipped.append("cannot parse: " + line)
            continue
        m = re.match(r"(gr|gu)(\w+)$", name)
        if not m:
            continue
        impl = "_d3dg%s_%s" % ("l" if m.group(1) == "gr" else "u", m.group(2))
        ptrs = ["_%s" % name, "_%s_%s" % (m.group(1), m.group(2))]
        hit = False
        for target, is_ptr in [(impl, False)] + [(p, True) for p in ptrs]:
            ea = ida_name.get_name_ea(idc.BADADDR, target)
            if ea == idc.BADADDR:
                continue
            hit = True
            if is_ptr == bool(ida_funcs.get_func(ea)):
                # e.g. _grLfbLock is an engine wrapper function, not the pointer slot
                skipped.append("%s: is %s, expected %s" % (target, "code" if is_ptr else "data", "pointer" if is_ptr else "function"))
                continue
            new = tif
            if is_ptr:
                new = ida_typeinf.tinfo_t()
                new.create_ptr(tif)
            old = ida_typeinf.tinfo_t()
            if ida_nalt.get_tinfo(old, ea):
                o = old.get_pointed_object() if is_ptr and old.is_ptr() else old
                if o.is_func() and nargs(o) != nargs(tif):
                    skipped.append("%s: args %d in IDA vs %d in glide.h (%s)" % (target, nargs(o), nargs(tif), o))
                    continue
            if dry or ida_typeinf.apply_tinfo(ea, new, ida_typeinf.TINFO_DEFINITE):
                applied.append(target)
            else:
                skipped.append("%s: apply_tinfo failed" % target)
        if not hit:
            missing.append(name)

    print("applied %d: %s" % (len(applied), " ".join(applied)))
    print("skipped %d:\n  %s" % (len(skipped), "\n  ".join(skipped)))
    print("not in database %d: %s" % (len(missing), " ".join(missing)))


# --- 1. types ---
scratch = ida_typeinf.new_til("glide_check", "")
errors = ida_typeinf.parse_decls(scratch, types_text, None, ida_typeinf.HTI_DCL)
print("scratch parse errors:", errors)
existing = {n: decl(idati, n) for n in type_names if decl(idati, n)}
for n, d in existing.items():
    new = decl(scratch, n)
    print("replace %s:\n   old: %s\n   new: %s" % (n, d, new))
if errors:
    print("not loading: fix glide_types.h first")
elif DRY:
    apply_prototypes(scratch, True)
    print("dry run, database not changed")
else:
    errors = ida_typeinf.parse_decls(idati, types_text, None, ida_typeinf.HTI_DCL)
    print("database parse errors:", errors)
    apply_prototypes(idati, False)

