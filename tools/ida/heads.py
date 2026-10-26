# IDAPython script, run through idacall.py:
#   python idacall.py py heads.py RANGES="0x54cac0-0x54caf0,0x5fa0a0-0x5fa0f0" [OUT=<file>]
# Lists the data items in each address range: address, name, type, size, user marks (anterior
# lines and comments), first xrefs. For checking data ownership around a file's block.
import idautils, idc, ida_bytes, ida_lines, ida_nalt, ida_typeinf, ida_name

RANGES = globals().get("RANGES", "")
OUT = globals().get("OUT", "")
lines = []

for r in RANGES.split(","):
    a, b = [int(x, 16) for x in r.strip().split("-")]
    lines.append("== %s - %s" % (hex(a), hex(b)))
    for h in idautils.Heads(a, b):
        tif = ida_typeinf.tinfo_t()
        t = tif._print(None, 0) if ida_nalt.get_tinfo(tif, h) else "-"
        marks = []
        i = 0
        while True:
            l = ida_lines.get_extra_cmt(h, ida_lines.E_PREV + i)
            if l is None:
                break
            marks.append(l)
            i += 1
        for c in (idc.get_cmt(h, 0), idc.get_cmt(h, 1)):
            if c:
                marks.append(c)
        xr = sorted(set(idc.get_func_name(x.frm) or hex(x.frm) for x in idautils.XrefsTo(h)))
        lines.append("%s %-28s %-22s size=%-6d %s %s" % (hex(h), ida_name.get_name(h), t, ida_bytes.get_item_size(h),
                                                        ("[" + " | ".join(marks) + "]") if marks else "", ", ".join(xr[:6])))

text = "\n".join(lines)
if OUT:
    open(OUT, "w", encoding="utf-8").write(text)
    print("wrote %s (%d lines)" % (OUT, len(lines)))
else:
    print(text)
