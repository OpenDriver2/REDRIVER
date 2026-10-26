"""Insert translated functions into a source file in address order.

Usage: python insert_functions.py <target .c> <snippets file>

The snippets file holds one or more functions, each starting with its "// [0xADDR] ..." marker line
(comment lines right above a marker belong to it). Each snippet goes before the first function in
the target whose marker address is higher, or at the end. Functions already in the target (same
address) are replaced.
"""
import re, sys

MARK = re.compile(r"^// \[0x([0-9a-fA-F]+)\]")


def split(text):
    """-> (head, [(addr, block)]) where a block starts at the comment run above its marker."""
    lines = text.splitlines(keepends=True)
    starts = []
    for i, l in enumerate(lines):
        m = MARK.match(l)
        if m:
            j = i
            while j > 0 and lines[j - 1].startswith("//") and not MARK.match(lines[j - 1]):
                j -= 1
            starts.append((j, int(m.group(1), 16)))
    if not starts:
        return text, []
    head = "".join(lines[:starts[0][0]])
    blocks = []
    for k, (s, addr) in enumerate(starts):
        e = starts[k + 1][0] if k + 1 < len(starts) else len(lines)
        blocks.append((addr, "".join(lines[s:e])))
    return head, blocks


def main():
    target, snippets = sys.argv[1], sys.argv[2]
    head, blocks = split(open(target, encoding="utf-8").read())
    _, new = split(open(snippets, encoding="utf-8").read())
    merged = {addr: b for addr, b in blocks}
    for addr, b in new:
        merged[addr] = b
    out = head
    for addr in sorted(merged):
        b = merged[addr]
        if not b.endswith("\n\n"):
            b = b.rstrip("\n") + "\n\n"
        out += b
    open(target, "w", encoding="utf-8").write(out.rstrip("\n") + "\n")
    print("%s: %d functions (%d new or replaced)" % (target, len(merged), len(new)))


main()
