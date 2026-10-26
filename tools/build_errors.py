"""Summarise an MSBuild log: compile errors per file, then unresolved externals.

Usage: python build_errors.py <build log> [<unresolved symbols out file>]
"""
import re, sys, collections

log = open(sys.argv[1], encoding="utf-8", errors="replace").read().splitlines()
errors = collections.OrderedDict()
unresolved = set()
for line in log:
    m = re.search(r"game6\.4g\\(.+?)\((\d+)(?:,\d+)?\): (error|warning) (C\d+): (.*?)(?: \[D:.*)?$", line)
    if m and (m.group(3) == "error" or m.group(4) in ("C4013", "C4047", "C4020", "C4024", "C4029", "C4133", "C4028")):
        errors.setdefault(m.group(1), []).append("%s %s %s: %s" % (m.group(2), m.group(3), m.group(4), m.group(5)))
    if "LNK2019" in line or "LNK2001" in line:
        # English and Russian linker messages
        sym = re.search(r"(?:symbol|символ) \"?([^\s\"]+?)\"?[.,]?(?: |$)", line)
        if sym:
            unresolved.add(sym.group(1))
for f, es in errors.items():
    print("==", f)
    for e in dict.fromkeys(es):
        print("  ", e)
if len(sys.argv) > 2:
    open(sys.argv[2], "w").write("\n".join(sorted(unresolved)) + "\n")
print("unresolved externals:", len(unresolved))
for s in sorted(unresolved):
    print("  ", s)
