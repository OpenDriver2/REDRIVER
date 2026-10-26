"""Flatten the Glide 2.x SDK headers into plain C declarations that IDA's built-in parser accepts.

Usage (plain Python, not IDA):
  python glide_decls.py <out_dir>

Writes:
  <out_dir>/glide_types.h   Fx* typedefs, Glide structs, and every `typedef FxI32 X;` that is
                            followed by GR_* #defines turned into `enum X { ... };`
  <out_dir>/glide_funcs.txt one prototype per line: `ret __stdcall grX(args);`
Load both into the database with `idacall.py py load_glide.py DIR=<out_dir>`.
"""
import os, re, sys

INC = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), r"..\..\src_rebuild\dependency\glideSDK\glide2x\include"))
HEADERS = ["3DFX.H", "SST1VID.H", "glide.h", "glideutl.h"]
PREDEF = {"GLIDE_NUM_TMU": 2}


def preprocess(text, macros):
    """Resolve #if/#ifdef/#else/#endif (nothing is defined except PREDEF) and collect numeric #defines."""
    out, stack = [], []
    for line in text.splitlines():
        s = line.strip()
        m = re.match(r"#\s*(ifdef|ifndef|if|elif|else|endif)\b(.*)", s)
        if m:
            kw, arg = m.group(1), m.group(2).split("/*")[0].strip()
            if kw in ("ifdef", "ifndef"):
                cond = (arg in macros) == (kw == "ifdef")
                stack.append([cond, cond])
            elif kw == "if":
                cond = eval_cond(arg, macros)
                stack.append([cond, cond])
            elif kw == "elif":
                cond = not stack[-1][1] and eval_cond(arg, macros)
                stack[-1] = [cond, stack[-1][1] or cond]
            elif kw == "else":
                stack[-1] = [not stack[-1][1], True]
            else:
                stack.pop()
            continue
        if not all(c for c, _ in stack):
            continue
        out.append(line)
    return "\n".join(out)


def eval_cond(expr, macros):
    expr = re.sub(r"defined\s*\(?\s*(\w+)\s*\)?", lambda m: "1" if m.group(1) in macros else "0", expr)
    expr = re.sub(r"\b[A-Za-z_]\w*\b", lambda m: str(macros.get(m.group(0), 0)), expr)
    expr = expr.replace("&&", " and ").replace("||", " or ").replace("!", " not ").replace(" not =", "!=")
    try:
        return bool(eval(expr))
    except Exception:
        return False


def macro_value(expr, macros):
    expr = expr.split("/*")[0].strip()
    expr = re.sub(r"FXBIT\s*\(\s*(\w+)\s*\)", r"(1<<(\1))", expr)
    expr = re.sub(r"\((Fx\w+|Gr\w+)\)", "", expr)          # casts
    expr = re.sub(r"\b(0x[0-9A-Fa-f]+|\d+)[uUlL]+\b", r"\1", expr)
    expr = re.sub(r"\b[A-Za-z_]\w*\b", lambda m: str(macros[m.group(0)]) if m.group(0) in macros else m.group(0), expr)
    if re.search(r"[A-Za-z_.]", expr.replace("0x", "")):
        return None                                          # float or unresolved
    try:
        v = eval(expr)
    except Exception:
        return None
    return v if isinstance(v, int) else None


def main():
    out_dir = sys.argv[1]
    macros = dict(PREDEF)
    read = lambda h: re.sub(r"\\\s*\n", " ", open(os.path.join(INC, h), encoding="latin-1").read())   # join "\" continuations
    text = "\n".join(preprocess(read(h), macros) for h in HEADERS)
    text = re.sub(r"/\*.*?\*/", "", text, flags=re.S)
    text = re.sub(r"//[^\n]*", "", text)

    types, funcs = [], []
    enum = None        # [name, [(member, value)], base type]
    pending = []       # lines of a multi-line declaration

    def flush_enum():
        nonlocal enum
        if enum:
            if enum[1]:
                types.append("enum %s\n{\n%s\n};" % (enum[0], ",\n".join("  %s = 0x%X" % (n, v & 0xFFFFFFFF) for n, v in enum[1])))
            else:
                types.append("typedef %s %s;" % (enum[2], enum[0]))
        enum = None

    for line in text.splitlines():
        s = line.strip()
        if not s:
            continue
        m = re.match(r"#\s*define\s+(\w+)(\([^)]*\))?\s*(.*)", s)
        if m:
            if not m.group(2):
                v = macro_value(m.group(3), macros)
                if v is not None:
                    macros[m.group(1)] = v
                    if enum is not None and m.group(1).startswith(("GR_", "GU_")):
                        enum[1].append((m.group(1), v))
            continue
        if s.startswith("#"):
            continue
        m = re.match(r"typedef\s+(FxI32|FxU32)\s+(\w+)\s*;$", s)
        if m and not pending:
            flush_enum()
            enum = [m.group(2), [], m.group(1)]
            continue
        pending.append(s)
        joined = " ".join(pending)
        if joined.count("{") != joined.count("}") or not joined.endswith(";"):
            continue
        pending = []
        flush_enum()
        joined = re.sub(r"\b[A-Za-z_]\w*\b", lambda m: str(macros[m.group(0)]) if m.group(0) in macros and m.group(0).isupper() and not m.group(0).startswith("FX") else m.group(0), joined)
        if "FX_ENTRY" in joined or "FX_CALL" in joined:
            joined = re.sub(r"\s+", " ", joined.replace("FX_ENTRY", "").replace("FX_CALL", "__stdcall")).strip()
            funcs.append(joined)
        elif joined.startswith(("typedef", "struct", "union", "enum")):
            # typedef struct [tag] { ... } Name;  ->  struct Name { ... };  (IDA names the struct itself)
            m = re.match(r"typedef\s+(struct|union)\s*\w*\s*(\{.*\})\s*(\w+)\s*;$", joined)
            types.append("%s %s %s;" % (m.group(1), m.group(3), m.group(2)) if m else joined)
    flush_enum()

    # enums must be known before structs/typedefs that use them; keep header order otherwise
    enums = [t for t in types if t.startswith("enum ")]
    rest = [t for t in types if not t.startswith("enum ")]
    os.makedirs(out_dir, exist_ok=True)
    open(os.path.join(out_dir, "glide_types.h"), "w").write("\n\n".join(enums + rest) + "\n")
    open(os.path.join(out_dir, "glide_funcs.txt"), "w").write("\n".join(funcs) + "\n")
    print("%d enums, %d other types, %d functions -> %s" % (len(enums), len(rest), len(funcs), out_dir))


main()
