#!/usr/bin/env python3
"""MCP stdio server over the GUI control surface.

The same endpoints tools/guictl.py calls, exposed as MCP tools so a coding
agent can list the widgets on screen, click them, and get the framebuffer back
as an image without shelling out and reading files.

Standard library only, on purpose: this repo takes no Python dependencies, and
MCP over stdio is newline-delimited JSON-RPC 2.0.

Register it for Claude Code with .mcp.json at the repo root; anything else
that speaks MCP wants `python3 tools/gui_mcp.py` as the stdio command.
"""

import base64
import json
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import guictl  # noqa: E402

PROTOCOL = "2025-06-18"

TOOLS = [
    {
        "name": "gui_launch",
        "description": "Start the Spirula Studio GUI with the automation "
                       "surface armed. Returns once it answers.",
        "inputSchema": {
            "type": "object",
            "properties": {
                "offscreen": {"type": "boolean",
                              "description": "Create the window invisible."},
                "exe": {"type": "string",
                        "description": "Path to the spirula binary; defaults "
                                       "to build_cuda/spirula."},
                "args": {"type": "array", "items": {"type": "string"},
                         "description": "Files to open, as a drop would."},
            },
        },
    },
    {
        "name": "gui_state",
        "description": "Screen, training phase, dialog and queue state, "
                       "display and framebuffer sizes.",
        "inputSchema": {"type": "object", "properties": {}},
    },
    {
        "name": "gui_tree",
        "description": "The widgets on screen: id (the i18n message name, "
                       "stable across languages), visible label, rect, window.",
        "inputSchema": {
            "type": "object",
            "properties": {
                "q": {"type": "string",
                      "description": "Substring of the id or the label."},
                "window": {"type": "string"},
                "all": {"type": "boolean",
                        "description": "Include items with no id of their own."},
            },
        },
    },
    {
        "name": "gui_click",
        "description": "Click a widget by id, or a point with at=\"x,y\".",
        "inputSchema": {
            "type": "object",
            "properties": {
                "id": {"type": "string"},
                "at": {"type": "string", "description": "x,y"},
                "button": {"type": "integer",
                           "description": "0 left, 1 right, 2 middle."},
                "double": {"type": "boolean"},
                "index": {"type": "integer",
                          "description": "Which of several items sharing an id."},
            },
        },
    },
    {
        "name": "gui_move",
        "description": "Move the pointer onto a widget or point (hover, "
                       "tooltips).",
        "inputSchema": {
            "type": "object",
            "properties": {"id": {"type": "string"}, "at": {"type": "string"},
                           "index": {"type": "integer"}},
        },
    },
    {
        "name": "gui_drag",
        "description": "Press, move and release: viewport orbit/pan, sliders, "
                       "and the selection gestures.",
        "inputSchema": {
            "type": "object",
            "properties": {
                "from": {"type": "string", "description": "x,y"},
                "to": {"type": "string", "description": "x,y"},
                "button": {"type": "integer"},
                "steps": {"type": "integer",
                          "description": "Intermediate positions; more for a "
                                         "path a tool has to follow."},
            },
            "required": ["from", "to"],
        },
    },
    {
        "name": "gui_scroll",
        "description": "Wheel over a widget or point (zoom, scrolling panels).",
        "inputSchema": {
            "type": "object",
            "properties": {"id": {"type": "string"}, "at": {"type": "string"},
                           "dy": {"type": "number"}},
        },
    },
    {
        "name": "gui_key",
        "description": "A key chord, e.g. \"Escape\", \"Ctrl+S\", "
                       "\"Ctrl+Shift+A\".",
        "inputSchema": {
            "type": "object",
            "properties": {"keys": {"type": "string"}},
            "required": ["keys"],
        },
    },
    {
        "name": "gui_text",
        "description": "Focus a text field, select everything in it, and type "
                       "a replacement.",
        "inputSchema": {
            "type": "object",
            "properties": {
                "id": {"type": "string"},
                "at": {"type": "string"},
                "value": {"type": "string"},
                "enter": {"type": "boolean",
                          "description": "Press Enter afterwards (default true)."},
            },
            "required": ["value"],
        },
    },
    {
        "name": "gui_wait",
        "description": "Let N frames pass, for work that finishes on the GUI "
                       "thread.",
        "inputSchema": {
            "type": "object",
            "properties": {"frames": {"type": "integer"}},
        },
    },
    {
        "name": "gui_drop",
        "description": "Drop files or folders on the window, as a drag from a "
                       "file manager would.",
        "inputSchema": {
            "type": "object",
            "properties": {"paths": {"type": "array", "items": {"type": "string"}}},
            "required": ["paths"],
        },
    },
    {
        "name": "gui_screenshot",
        "description": "The current framebuffer, as an image.",
        "inputSchema": {
            "type": "object",
            "properties": {
                "width": {"type": "integer",
                          "description": "Downscale to this width (default 1280)."},
                "format": {"type": "string", "enum": ["jpg", "png"],
                           "description": "Default jpg; png for exact pixels."},
                "quality": {"type": "integer", "description": "jpg, 1-100."},
                "path": {"type": "string",
                         "description": "Also write the image here."},
            },
        },
    },
]


def point_args(a):
    out = {}
    if a.get("at"):
        out["at"] = a["at"]
    elif a.get("id"):
        out["id"] = a["id"]
        out["index"] = a.get("index", 0)
    return out


def run_tool(name, a):
    """(content list, is_error)."""
    if name == "gui_launch":
        class Args:
            pass
        args = Args()
        args.exe = a.get("exe")
        args.offscreen = bool(a.get("offscreen", True))
        args.port = None
        args.log = None
        args.wait = 60.0
        args.rest = list(a.get("args", []))
        # cmd_launch prints its own JSON and exits the process on failure, so
        # it is the argument shapes that are reused here, not the function.
        import io
        import contextlib
        buf = io.StringIO()
        with contextlib.redirect_stdout(buf):
            guictl.cmd_launch(args)
        return [{"type": "text", "text": buf.getvalue().strip()}], False

    if name == "gui_screenshot":
        # Downscaled and JPEG by default: a 1600x950 PNG is 2 MB of base64,
        # which costs more context than the picture is worth.
        fmt = a.get("format", "jpg")
        img = guictl.call("/ui/screenshot",
                          {"width": int(a.get("width", 1280)), "format": fmt,
                           "quality": int(a.get("quality", 80))}, raw=True)
        content = [{"type": "image",
                    "data": base64.b64encode(img).decode("ascii"),
                    "mimeType": "image/png" if fmt == "png" else "image/jpeg"}]
        if a.get("path"):
            with open(a["path"], "wb") as f:
                f.write(img)
            content.append({"type": "text", "text": a["path"]})
        return content, False

    routes = {
        "gui_state": ("/ui/state", lambda a: {}),
        "gui_tree": ("/ui/tree", lambda a: {
            k: v for k, v in (("q", a.get("q")), ("window", a.get("window")),
                              ("named", "0" if a.get("all") else "1"))
            if v is not None}),
        "gui_click": ("/ui/click", lambda a: dict(
            point_args(a), button=a.get("button", 0),
            double="1" if a.get("double") else "0")),
        "gui_move": ("/ui/move", point_args),
        "gui_drag": ("/ui/drag", lambda a: {
            "from": a["from"], "to": a["to"], "button": a.get("button", 0),
            "steps": a.get("steps", 8)}),
        "gui_scroll": ("/ui/scroll", lambda a: dict(
            point_args(a), dy=a.get("dy", -1.0))),
        "gui_key": ("/ui/key", lambda a: {"keys": a["keys"]}),
        "gui_text": ("/ui/text", lambda a: dict(
            point_args(a), value=a["value"],
            enter="1" if a.get("enter", True) else "0")),
        "gui_wait": ("/ui/wait", lambda a: {"frames": a.get("frames", 4)}),
        "gui_drop": ("/ui/drop", lambda a: {"paths": "\n".join(a["paths"])}),
    }
    if name not in routes:
        return [{"type": "text", "text": "unknown tool: " + name}], True
    path, build = routes[name]
    body = guictl.call(path, build(a))
    return [{"type": "text",
             "text": json.dumps(body, ensure_ascii=False, indent=2)}], False


def handle(msg):
    """The response to one request, or None for a notification."""
    method = msg.get("method")
    if method == "initialize":
        want = (msg.get("params") or {}).get("protocolVersion") or PROTOCOL
        return {"protocolVersion": want,
                "capabilities": {"tools": {}},
                "serverInfo": {"name": "spirula-gui", "version": "1"}}
    if method == "tools/list":
        return {"tools": TOOLS}
    if method == "tools/call":
        params = msg.get("params") or {}
        try:
            content, is_error = run_tool(params.get("name"),
                                         params.get("arguments") or {})
        except SystemExit as e:
            content, is_error = [{"type": "text", "text": str(e)}], True
        except Exception as e:                       # noqa: BLE001
            content, is_error = [{"type": "text",
                                  "text": "%s: %s" % (type(e).__name__, e)}], True
        return {"content": content, "isError": is_error}
    if method == "ping":
        return {}
    return None


def main():
    out = sys.stdout
    for line in sys.stdin:
        line = line.strip()
        if not line:
            continue
        try:
            msg = json.loads(line)
        except ValueError:
            continue
        if "id" not in msg:
            continue                                  # a notification
        try:
            result = handle(msg)
        except Exception as e:                        # noqa: BLE001
            reply = {"jsonrpc": "2.0", "id": msg["id"],
                     "error": {"code": -32603, "message": str(e)}}
        else:
            if result is None:
                reply = {"jsonrpc": "2.0", "id": msg["id"],
                         "error": {"code": -32601,
                                   "message": "method not found: %s"
                                              % msg.get("method")}}
            else:
                reply = {"jsonrpc": "2.0", "id": msg["id"], "result": result}
        out.write(json.dumps(reply) + "\n")
        out.flush()


if __name__ == "__main__":
    main()
