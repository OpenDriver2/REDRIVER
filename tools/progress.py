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
GLOBALS = os.path.join(ROOT, "tools", "ida", "globals.tsv")
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


IDA_C_TYPES = [(r"\bunsigned __int8\b", "unsigned char"), (r"\bunsigned __int16\b", "unsigned short"),
               (r"\bunsigned __int32\b", "unsigned int"), (r"\b__int8\b", "char"), (r"\b__int16\b", "short"),
               (r"\b__int32\b", "int"), (r"\b_BYTE\b", "char"), (r"\b_WORD\b", "short"), (r"\b_DWORD\b", "int"),
               (r"\b_BOOL4\b", "int"), (r"\bsigned (char|short|int)\b", r"\1"), (r"\b__cdecl \b", ""),
               (r"\bconst \b", "")]


def c_decl(decl, name, size):
    """IDA declaration -> C declaration (IDA-only integer types replaced)."""
    if decl == "-":
        return "int %s;\t// NOTE: untyped in IDA, %d bytes" % (name, size) if size == 4 else \
            "char %s[%d];\t// NOTE: untyped in IDA" % (name, size)
    for pat, rep in IDA_C_TYPES:
        decl = re.sub(pat, rep, decl)
    return decl + ";"


_mark_files = None


def mark_file(mark):
    """'@motel\\flat.c;static' -> source file, via the IDA folder names (either side of '=') and
    the files in SRC, case-insensitively."""
    global _mark_files
    if _mark_files is None:
        _mark_files = {}
        for line in open(FUNCS, encoding="utf-8"):
            folder = line.split("\t", 1)[0]
            for part in folder.split("="):
                _mark_files[part.lower()] = folder_file(folder)
        for root, dirs, files in os.walk(SRC):
            for fn in files:
                rel = os.path.relpath(os.path.join(root, fn), SRC).replace("\\", "/")
                _mark_files.setdefault(rel.replace("/", "\\").lower(), rel)
    name = mark.lstrip("@").split(";")[0].strip()
    f = _mark_files.get(name.lower())
    if f is None:
        f = name.replace("\\", "/")
    for root, dirs, files in os.walk(SRC):     # fix the case of an existing file
        for fn in files:
            rel = os.path.relpath(os.path.join(root, fn), SRC).replace("\\", "/")
            if rel.lower() == f.replace(" (missing)", "").lower():
                return rel
    return f.replace(" (missing)", "")


def load_globals():
    """-> {name: (addr, size, decl, mark, files, {file: references})} for game globals (used by
    functions in folders)."""
    out = {}
    for line in open(GLOBALS, encoding="utf-8"):
        addr, name, size, decl, mark, users = line.rstrip("\n").split("\t")
        refs = collections.Counter()
        for u in users.split(","):
            if u and not u.startswith("<"):
                folder, count = u.rsplit(":", 1)
                refs[folder_file(folder)] += int(count)
        if not refs:
            continue    # CRT / library data, or unreferenced
        out[name] = (int(addr, 16), int(size), decl, mark, set(refs), refs)
    return out


def global_sources(names):
    """-> (definitions {name: {file}}, externs {name: {file}}) of data globals written in the source."""
    defs = collections.defaultdict(set)
    exts = collections.defaultdict(set)
    word = r"([A-Za-z_]\w*)\s*(?:\[[^\]]*\]\s*)*"
    p_def = re.compile(r"^(?!extern\b|static\b|typedef\b|return\b)[A-Za-z_][\w \t\*]*?\b" + word + r"(?:=|;)", re.M)
    p_ext = re.compile(r"^extern\s+[^;(]*?\b" + word + r";", re.M)
    p_ptr = re.compile(r"^(?!extern\b|static\b|typedef\b|return\b)[A-Za-z_][\w \t\*]*\(\s*\*+\s*" + word
                       + r"\)\s*\([^()]*\)\s*(?:=|;)", re.M)
    for rel, text in source_texts():
        if rel.startswith("../"):
            continue
        text = re.sub(r"//[^\n]*|/\*.*?\*/", "", text, flags=re.S)
        # top level only: drop brace blocks (function bodies, initialisers)
        flat, depth = [], 0
        for ch in text:
            if ch == "{":
                depth += 1
                if depth == 1:
                    flat.append("=")
            elif ch == "}":
                depth -= 1
            elif depth == 0:
                flat.append(ch)
        flat = "".join(flat)
        for m in list(p_def.finditer(flat)) + list(p_ptr.finditer(flat)):
            if m.group(1) in names:
                defs[m.group(1)].add(rel)
        for m in p_ext.finditer(flat):
            if m.group(1) in names:
                exts[m.group(1)].add(rel)
    return defs, exts


def global_owners():
    """-> (globals, {name: owner file}, guessed names, defs, exts). Owner: the file defining it in the
    source, else the user's @file mark, else its only user, else (guessed) the file referencing it most."""
    g = load_globals()
    defs, exts = global_sources(set(g))
    owner = {}
    guessed = set()
    for n, (addr, size, decl, mark, folders, refs) in g.items():
        d = defs.get(n)
        if d:
            owner[n] = sorted(d)[0]
        elif mark.startswith("@"):
            owner[n] = mark_file(mark)
        elif len(folders) == 1:
            owner[n] = next(iter(folders))
        else:
            owner[n] = refs.most_common(1)[0][0]
            guessed.add(n)
    return g, owner, guessed, defs, exts


def header_declarations(names):
    """-> {name: header} for globals already declared in a header of the source tree."""
    found = {}
    for rel, text in source_texts():
        if not rel.endswith(".h"):
            continue
        text = re.sub(r"//[^\n]*|/\*.*?\*/", "", text, flags=re.S)
        decl = re.findall(r"^\s*(?:extern|GLIDE_EXTERN)\b[^;]*?(?:\*\s*(\w+)\s*\)\s*\(|\b(\w+)\s*(?:\[[^\]]*\]\s*)*;)",
                          text, re.M)
        for a, b in decl:
            w = a or b
            if w in names:
                found.setdefault(w, rel)
    return found


SHARED_GLOBALS = "dr_globals.h"


def header_of(cfile):
    return cfile[:-2] + ".h" if cfile.endswith(".c") else cfile + ".h"


def globals_report(args):
    g, owner, guessed, defs, exts = global_owners()

    def own(n):
        return "`%s`%s" % (owner[n], " (guess)" if n in guessed else "")

    if args and args[0] == "--externs":
        # every hand-written extern: where the declaration would come from
        print("| Global | Declared extern in | Owner | Header |")
        print("|---|---|---|---|")
        in_headers = header_declarations(set(g))
        for n in sorted(exts, key=str.lower):
            hdr = in_headers.get(n) or (SHARED_GLOBALS if n in guessed else header_of(owner[n]))
            print("| `%s` | %s | %s | `%s` |" % (n, ", ".join("`%s`" % f for f in sorted(exts[n])), own(n), hdr))
        return

    if args:
        # header draft for one source file: its globals used by other files
        cfile = args[0].replace("\\", "/")
        rows = sorted((g[n][0], n) for n in g if owner[n] == cfile)
        print("/* %s - globals of %s used by other files (draft from tools/ida/globals.tsv) */" % (header_of(cfile), cfile))
        in_headers = header_declarations(set(g))
        for addr, n in rows:
            addr, size, decl, mark, folders, refs = g[n]
            others = folders - {cfile}
            if not others:
                continue
            if n in in_headers:
                print("// %s: declared in %s" % (n, in_headers[n]))
                continue
            state = "" if n in defs else "\t// not defined in the source yet"
            if n in guessed:
                state += "\t// NOTE: owner guessed (most references): %s" % ", ".join(
                    "%s %d" % (f, c) for f, c in refs.most_common(3))
            print("extern %s%s" % (c_decl(decl, n, size), state))
        local = [n for a, n in rows if not (g[n][4] - {cfile})]
        if local:
            print("\n// only used by %s (no extern needed): %s" % (cfile, ", ".join(local)))
        return

    # per owner file: globals, used elsewhere, defined in the source, externs written in other files
    per = collections.defaultdict(lambda: [0, 0, 0, 0, 0])
    for n in g:
        o = owner[n]
        r = per[o]
        r[0] += 1
        r[1] += bool(g[n][4] - {o})
        r[2] += n in defs
        r[3] += len(exts.get(n, ()))
        r[4] += n in guessed
    print("| Owner | Header | Globals | Used elsewhere | Defined in source | Hand-written externs | Owner guessed |")
    print("|---|---|---:|---:|---:|---:|---:|")
    for o in sorted(per, key=str.lower):
        print("| `%s` | `%s` | %d | %d | %d | %d | %d |" % ((o, header_of(o)) + tuple(per[o])))
    print("\n%d globals, %d with the owner guessed (several users, no @mark, not defined yet: the file "
          "referencing them most); their externs go to %s until the owner is known." % (
              len(g), len(guessed), SHARED_GLOBALS))
    dup = sorted(n for n in defs if len(defs[n]) > 1)
    if dup:
        print("\nDefined in several files: %s" % ", ".join("`%s` (%s)" % (n, ", ".join(sorted(defs[n]))) for n in dup))
    wrong = sorted(n for n in defs if g[n][3].startswith("@") and mark_file(g[n][3]) not in defs[n]
                   and ";extern" not in g[n][3])
    if wrong:
        print("\nDefined in another file than the IDA mark says: %s" % ", ".join(
            "`%s` (%s, mark %s)" % (n, ", ".join(sorted(defs[n])), g[n][3]) for n in wrong))


def main(args):
    if args and args[0] == "--types":
        types_report(args[1:])
        return
    if args and args[0] == "--globals":
        globals_report(args[1:])
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

    # a marker with no status is a data address
    stale = sorted(ea for ea in done if ea not in known and done[ea][1])
    if stale:
        print()
        print("Markers with no IDA function in a folder: %s" % ", ".join("%s (%s)" % (hex(ea), done[ea][0]) for ea in stale))


if __name__ == "__main__":
    main(sys.argv[1:])
