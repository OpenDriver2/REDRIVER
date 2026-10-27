"""Minimal stdio client for the Hex-Rays ida-mcp plugin, so agents without IDA MCP tools can still drive IDA.

Usage:
  python idacall.py list_databases                 # show databases IDA has open
  python idacall.py py <script.py> [NAME=VALUE ...] # run an IDAPython script in the open GUI database
  python idacall.py <tool> '<json args>'           # call any other ida-mcp tool directly

For `py`, each NAME=VALUE is prepended to the script as `NAME = r"VALUE"`.
Prints the script's stdout (and stderr/traceback if any).
"""
import json, os, subprocess, sys

PLUGIN = os.path.expanduser(r"~\AppData\Roaming\Hex-Rays\IDA Pro\plugins\ida-mcp")
DATABASE = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), r"..\..\Driver_sym2.i64"))

proc = subprocess.Popen(
    ["uv", "run", "--no-dev", "--project", PLUGIN, "ida-mcp", "stdio", "--agent=claude"],
    stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.DEVNULL,
)
_id = 0


def rpc(method, params=None, notify=False):
    global _id
    msg = {"jsonrpc": "2.0", "method": method}
    if params is not None:
        msg["params"] = params
    if not notify:
        _id += 1
        msg["id"] = _id
    proc.stdin.write((json.dumps(msg) + "\n").encode())
    proc.stdin.flush()
    if notify:
        return None
    while True:
        line = proc.stdout.readline()
        if not line:
            raise SystemExit("ida-mcp server closed the connection")
        r = json.loads(line)
        if r.get("id") == _id:
            return r


def call(tool, args):
    r = rpc("tools/call", {"name": tool, "arguments": args})
    if "error" in r:
        raise SystemExit("error: %s" % r["error"])
    return r["result"]


def out(s):
    sys.stdout.buffer.write(s.encode("utf-8"))


rpc("initialize", {"protocolVersion": "2025-06-18", "capabilities": {}, "clientInfo": {"name": "idacall", "version": "1"}})
rpc("notifications/initialized", notify=True)

if len(sys.argv) < 2:
    raise SystemExit(__doc__)
tool = sys.argv[1]

def gui_database():
    """The database open in the IDA GUI (if exactly one), else the default DATABASE."""
    res = call("list_databases", {})
    data = res.get("structuredContent")
    if not data:
        text = "".join(c.get("text", "") for c in res.get("content", []) if c.get("type") == "text")
        data = json.loads(text) if text.strip().startswith("{") else {}
    paths = [i["path"] for i in data.get("instances", []) if i.get("backend") == "gui" and i.get("status") == "available"]
    return paths[0] if len(paths) == 1 else DATABASE


if tool == "py":
    res = call("open_database", {"path": gui_database()})
    if res.get("isError"):
        raise SystemExit(json.dumps(res, indent=1))
    code = "".join('%s = r"%s"\n' % tuple(a.split("=", 1)) for a in sys.argv[3:])
    code += open(sys.argv[2], encoding="utf-8").read()
    res = call("execute_python", {"code": code, "timeout": 600})
    sc = res.get("structuredContent") or {}
    if sc:
        out(sc.get("stdout") or "")
        if sc.get("stderr"):
            out("\n--- stderr ---\n" + sc["stderr"])
    for c in res.get("content", []):
        if not sc and c.get("type") == "text":
            out(c["text"] + "\n")
    if res.get("isError"):
        sys.exit(1)
else:
    args = json.loads(sys.argv[2]) if len(sys.argv) > 2 else {}
    res = call(tool, args)
    for c in res.get("content", []):
        if c.get("type") == "text":
            out(c["text"] + "\n")
