"""Translation progress map: IDA function folders against the `// [0xADDR] [marks]` markers in
src_rebuild/game6.4g. Plain Python; works from the exported function list, IDA is not needed.

Usage:
  python tools/progress.py                         # per-folder table (Markdown) to stdout
  python tools/progress.py "frontend\\main.c"       # functions of one folder: translated or not
  python tools/progress.py --todo                  # untranslated functions of partly translated folders

The function list is tools/ida/functions.tsv (folder, address, name), exported from IDA with
  python tools/ida/idacall.py py tools/ida/list_functions.py OUT=<repo>/tools/ida/functions.tsv
Re-export it when the function folders in the database change.
"""
import collections, os, re, sys

ROOT = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), ".."))
SRC = os.path.join(ROOT, "src_rebuild", "game6.4g")
FUNCS = os.path.join(ROOT, "tools", "ida", "functions.tsv")
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


def main(args):
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
