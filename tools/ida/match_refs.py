"""Find Driver 1 functions that have a same-named counterpart in the reference sources:
REDRIVER2.

Usage (plain Python, not IDA):
  python idacall.py py list_functions.py OUT=<scratch>/d1funcs.tsv      # once, export Driver 1 function list
  python match_refs.py <scratch>/d1funcs.tsv                        # per-folder summary
  python match_refs.py <scratch>/d1funcs.tsv "frontend\\cars.c"     # per-function matches for one folder

Matching is by name only (IDA prefixes `_`, `file._`, `@N` are stripped), so it finds shared
code, not every analogy: renamed or rewritten functions need a manual search in the references.
"""
import collections, os, re, sys

REFS = {
    "REDRIVER2": r"D:\Projects\OpenDriver2\ReDriver2\code\src_rebuild\Game",
}
DEF = re.compile(r"^[A-Za-z_][\w \*]*?[ \*](\w+)\s*\([^;]*$")


def ref_definitions():
    defs = collections.defaultdict(list)
    for ref, base in REFS.items():
        for root, dirs, files in os.walk(base):
            dirs[:] = [d for d in dirs if d not in (".git", "Backup", "dont_use", "Debug", "Release")]
            for fn in files:
                if not fn.lower().endswith(".c"):
                    continue
                path = os.path.join(root, fn)
                for i, line in enumerate(open(path, encoding="utf-8", errors="replace"), 1):
                    m = DEF.match(line)
                    if m and m.group(1) not in ("if", "while", "for", "switch", "return"):
                        defs[m.group(1)].append("%s/%s:%d" % (ref, os.path.relpath(path, base), i))
    return defs


def d1_name(name):
    if "._" in name:
        name = name.split("._", 1)[1]
    name = re.sub(r"@\d+$", "", name.lstrip("_"))
    # D3D Glide emulation: d3dgl_X / d3dgu_X in the game are grX / guX in the reference glide2x.c
    return re.sub(r"^d3dg([lu])_", lambda m: "gr" if m.group(1) == "l" else "gu", name)


def main():
    d1, folder = sys.argv[1], (sys.argv[2] if len(sys.argv) > 2 else None)
    defs = ref_definitions()
    stat = collections.OrderedDict()
    for line in open(d1, encoding="utf-8"):
        f, ea, name = line.rstrip("\n").split("\t")
        if f == "[THUNKS]":
            continue
        n = d1_name(name)
        if folder:
            if f == folder:
                print("%s %-32s %s" % (ea, n, ", ".join(defs.get(n, [])) or "-"))
            continue
        s = stat.setdefault(f, [0, 0, collections.Counter()])
        s[0] += 1
        if n in defs:
            s[1] += 1
            s[2].update(set(p.split(":")[0] for p in defs[n]))
    for f, (total, hit, files) in stat.items():
        print("%-45s %4d funcs %4d matched  %s" % (f, total, hit, ", ".join("%s:%d" % kv for kv in files.most_common(4))))


main()
