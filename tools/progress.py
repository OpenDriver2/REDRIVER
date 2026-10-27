"""Translation progress map: IDA function folders against the `// [0xADDR] [marks]` markers in
src_rebuild/game6.4g. Plain Python; works from the exported function list, IDA is not needed.

Usage:
  python tools/progress.py                         # per-folder table (Markdown) to stdout
  python tools/progress.py "frontend\\main.c"       # functions of one folder: translated or not
  python tools/progress.py --todo                  # untranslated functions of partly translated folders
  python tools/progress.py --types                 # game types: where used, where defined, where they belong
  python tools/progress.py --types FE_POLY         # one type in full

The function list is tools/ida/functions.tsv (folder, address, name), exported from IDA with
  python tools/ida/idacall.py py tools/ida/list_functions.py OUT=<repo>/tools/ida/functions.tsv
Re-export it when the function folders in the database change. The types come from
tools/ida/types.tsv (list_types.py) and tools/ida/type_usage.tsv (type_usage.py), exported the same way.
"""
import collections, os, re, sys

ROOT = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), ".."))
SRC = os.path.join(ROOT, "src_rebuild", "game6.4g")
FUNCS = os.path.join(ROOT, "tools", "ida", "functions.tsv")
TYPES = os.path.join(ROOT, "tools", "ida", "types.tsv")
TYPE_USAGE = os.path.join(ROOT, "tools", "ida", "type_usage.tsv")
SHARED_HEADER = "dr_types.h"
MARKER = re.compile(r"^// \[0x([0-9a-fA-F]+)\]((?: \[[DTFA]\])*)")


def folder_file(folder):
    """IDA folder -> source file relative to SRC (see docs/ida.md, "IDA folders -> files")."""
    names = folder.split("=")
    for name in names:
        path = name.replace("\\", "/")
        if path.endswith(".asm"):
            path = path[:-4] + ".c"
        if os.path.exists(os.path.join(SRC, path)):
            return path
    return names[-1].replace("\\", "/") + " (missing)"


def markers():
    found = {}
    for root, dirs, files in os.walk(SRC):
        for fn in files:
            if not fn.endswith(".c"):
                continue
            path = os.path.join(root, fn)
            rel = os.path.relpath(path, SRC).replace("\\", "/")
            for line in open(path, encoding="utf-8", errors="replace"):
                m = MARKER.match(line)
                if m:
                    found[int(m.group(1), 16)] = (rel, m.group(2).split())
    return found


def load_functions():
    funcs = collections.defaultdict(list)
    for line in open(FUNCS, encoding="utf-8"):
        folder, ea, name = line.rstrip("\n").split("\t")
        if folder == "[THUNKS]":
            continue
        funcs[folder].append((int(ea, 16), name))
    return funcs


def source_texts():
    for root, dirs, files in os.walk(SRC):
        for fn in files:
            if fn.endswith((".c", ".h")):
                path = os.path.join(root, fn)
                yield os.path.relpath(path, SRC).replace("\\", "/"), open(path, encoding="utf-8", errors="replace").read()


def type_definitions(names):
    """Type name -> source files that define it (struct/union/enum body or typedef)."""
    found = collections.defaultdict(set)
    word = re.compile(r"\b(\w+)\b")
    pats = [
        re.compile(r"\b(?:struct|union|enum)\s+(\w+)\s*\{"),       # struct NAME {
        re.compile(r"\}\s*(\w+)\s*(?:,[^;]*)?;"),                   # } NAME;
        re.compile(r"\btypedef\b[^;{}]*?\b(\w+)\s*(?:\[[^\]]*\]\s*)*;"),  # typedef ... NAME;
        re.compile(r"\btypedef\b[^;{}]*\(\s*[\w\s]*\*\s*(\w+)\s*\)"),    # typedef ret (CC *NAME)(...)
    ]
    for rel, text in source_texts():
        text = re.sub(r"//[^\n]*|/\*.*?\*/", "", text, flags=re.S)
        for p in pats:
            for m in p.finditer(text):
                if re.match(r"typedef\s+(?:struct|union|enum)\s+\w+\s+\w+\s*;", m.group(0)):
                    continue    # forward declaration
                if m.group(1) in names:
                    found[m.group(1)].add(rel)
    return found


def types_report(args):
    types = {}
    for line in open(TYPES, encoding="utf-8"):
        folder, kind, size, origin, name = line.rstrip("\n").split("\t")
        if origin == "game" and name not in types:
            types[name] = (folder, kind, int(size))
    usage, deps = {}, {}
    for line in open(TYPE_USAGE, encoding="utf-8"):
        name, used, dep = line.rstrip("\n").split("\t")
        usage[name] = set(folder_file(f) for f in used.split(",") if f)
        deps[name] = set(d for d in dep.split(",") if d in types)

    # files that need a type: its users plus, transitively, the files that need a type built on it
    need = {n: set(usage.get(n, ())) for n in types}
    changed = True
    while changed:
        changed = False
        for n in types:
            for d in deps.get(n, ()):
                if not need[n] <= need[d]:
                    need[d] |= need[n]
                    changed = True
    defined = type_definitions(set(types))

    def place(n):
        f = need[n]
        if not f:
            return "unused"
        return SHARED_HEADER if len(f) > 1 else next(iter(f))

    if args:
        for n in args:
            if n not in types:
                print("%s: not a game type in %s" % (n, os.path.relpath(TYPES, ROOT)))
                continue
            folder, kind, size = types[n]
            print("%s %s (%d bytes), IDA folder: %s" % (kind, n, size, folder or "-"))
            print("  belongs in: %s" % place(n))
            print("  defined in: %s" % (", ".join(sorted(defined.get(n, ()))) or "-"))
            print("  needs:      %s" % (", ".join(sorted(deps.get(n, ()))) or "-"))
            print("  used in:    %s" % (", ".join(sorted(usage.get(n, ()))) or "-"))
            others = sorted(need[n] - usage.get(n, set()))
            if others:
                print("  needed by types used in: %s" % ", ".join(others))
        return

    def files(s):
        s = sorted(s)
        return ", ".join("`%s`" % f for f in s) if len(s) <= 3 else "%d files" % len(s)

    rows = sorted(types, key=lambda n: (place(n) != SHARED_HEADER, place(n) == "unused", place(n).lower(), n.lower()))
    print("| Type | Kind | IDA folder | Belongs in | Defined in | Needed in |")
    print("|---|---|---|---|---|---|")
    for n in rows:
        folder, kind, size = types[n]
        d = defined.get(n, set())
        mark = "" if not d else " ⚠" if len(d) > 1 or (place(n) not in d and place(n) != SHARED_HEADER) else ""
        print("| `%s` | %s | %s | %s | %s%s | %s |" % (n, kind, folder or "", place(n), files(d) if d else "-", mark, files(need[n])))
    counts = collections.Counter(SHARED_HEADER if place(n) == SHARED_HEADER else "unused" if place(n) == "unused" else "one file" for n in types)
    print()
    print("%d game types: %d defined in the source, %d shared (%s), %d in one file, %d unused by functions. "
          "⚠ = defined in several files or not where it belongs." % (
              len(types), sum(1 for n in types if n in defined), counts[SHARED_HEADER], SHARED_HEADER, counts["one file"], counts["unused"]))


def main(args):
    if args and args[0] == "--types":
        types_report(args[1:])
        return
    funcs = load_functions()
    done = markers()
    known = set(ea for fl in funcs.values() for ea, _ in fl)

    if args and args[0] != "--todo":
        folder = args[0]
        for ea, name in sorted(funcs.get(folder, [])):
            print("%s %s %s" % ("+" if ea in done else "-", hex(ea), name), " ".join(done.get(ea, ("", []))[1]))
        return

    rows = []
    totals = [0, 0, 0, 0]
    for folder in sorted(funcs, key=str.lower):
        fl = funcs[folder]
        d = [ea for ea, _ in fl if ea in done]
        t = [ea for ea in d if "[T]" in done[ea][1]]
        a = [ea for ea in d if "[A]" in done[ea][1]]
        rows.append((folder, folder_file(folder), len(fl), len(d), len(t), len(a)))
        for i, v in enumerate((len(fl), len(d), len(t), len(a))):
            totals[i] += v

    if args and args[0] == "--todo":
        for folder, f, n, d, t, a in rows:
            if 0 < d < n:
                print("## %s (%d/%d)" % (f, d, n))
                for ea, name in sorted(funcs[folder]):
                    if ea not in done:
                        print("- %s %s" % (hex(ea), name))
        return

    print("| File | IDA folder | Functions | [D] | [T] | [A] |")
    print("|---|---|---:|---:|---:|---:|")
    for folder, f, n, d, t, a in rows:
        if d:
            print("| `%s` | `%s` | %d | %d | %d | %d |" % (f, folder, n, d, t, a))
    untouched = [f for folder, f, n, d, t, a in rows if not d]
    print("| **total** | | %d | %d | %d | %d |" % tuple(totals))
    print()
    print("Not started (%d files): %s" % (len(untouched), ", ".join("`%s`" % f for f in untouched)))

    stale = sorted(ea for ea in done if ea not in known)
    if stale:
        print()
        print("Markers with no IDA function in a folder: %s" % ", ".join("%s (%s)" % (hex(ea), done[ea][0]) for ea in stale))


if __name__ == "__main__":
    main(sys.argv[1:])
