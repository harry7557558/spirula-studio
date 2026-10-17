#!/usr/bin/env python3
"""Drive the Spirula Studio GUI from a shell.

A thin client for the loopback control surface in src/app/gui/Automation.cpp,
which the GUI opens when SS_GUI_AUTOMATION=1. Widgets are addressed by their
i18n message name -- the part after "###" in an ImGui label -- so a script
does not care which language the window is in.

    python3 tools/guictl.py launch --offscreen
    python3 tools/guictl.py tree -q open
    python3 tools/guictl.py click open_dataset
    python3 tools/guictl.py shot /tmp/gui.png

Design and endpoint reference: docs/notes/gui-automation.md
"""

import argparse
import json
import os
import subprocess
import sys
import time
import urllib.error
import urllib.parse
import urllib.request


def config_dir():
    """Must match config_dir() in src/app/AppPaths.cpp."""
    if os.name == "nt":
        base = os.environ.get("APPDATA")
    else:
        base = os.environ.get("XDG_CONFIG_HOME")
        if not base and os.environ.get("HOME"):
            base = os.path.join(os.environ["HOME"], ".config")
    base = base or "."
    current = os.path.join(base, "spirula-studio")
    legacy = os.path.join(base, "spirulae-splat")
    if not os.path.isdir(current) and os.path.isdir(legacy):
        return legacy
    return current


def endpoint():
    """(base url, token), from the file the GUI writes when it starts."""
    host = os.environ.get("SS_GUI_AUTOMATION_HOST", "127.0.0.1")
    port = os.environ.get("SS_GUI_AUTOMATION_PORT")
    token = os.environ.get("SS_GUI_AUTOMATION_TOKEN", "")
    path = os.path.join(config_dir(), "automation.json")
    try:
        with open(path) as f:
            saved = json.load(f)
        port = port or str(saved.get("port", 7777))
        token = token or saved.get("token", "")
    except (OSError, ValueError):
        port = port or "7777"
    return "http://%s:%s" % (host, port), token


def call(path, params=None, raw=False, timeout=30.0):
    base, token = endpoint()
    q = dict(params or {})
    if token:
        q["token"] = token
    url = "%s%s?%s" % (base, path, urllib.parse.urlencode(q))
    try:
        with urllib.request.urlopen(url, timeout=timeout) as r:
            body = r.read()
    except urllib.error.HTTPError as e:
        body = e.read()
        sys.stderr.write(body.decode("utf-8", "replace") + "\n")
        sys.exit(1)
    except urllib.error.URLError as e:
        sys.stderr.write("cannot reach %s: %s\n" % (base, e.reason))
        sys.stderr.write("is the GUI running with SS_GUI_AUTOMATION=1? "
                         "(guictl.py launch)\n")
        sys.exit(2)
    if raw:
        return body
    return json.loads(body.decode("utf-8"))


# ---------------------------------------------------------------------------
# Commands
# ---------------------------------------------------------------------------

def target(args):
    """id= / at= for the commands that act somewhere."""
    if getattr(args, "at", None):
        return {"at": args.at}
    return {"id": args.id, "index": args.index}


def cmd_launch(args):
    exe = args.exe or os.path.join(
        os.path.dirname(os.path.abspath(__file__)), "..", "build_cuda", "spirula")
    exe = os.path.abspath(exe)
    if not os.path.exists(exe):
        sys.exit("no such executable: %s" % exe)
    env = dict(os.environ)
    env["SS_GUI_AUTOMATION"] = "1"
    if args.port:
        env["SS_GUI_AUTOMATION_PORT"] = str(args.port)
    if args.offscreen:
        env["SS_GUI_OFFSCREEN"] = "1"
    log = open(args.log, "ab") if args.log else subprocess.DEVNULL
    rest = args.rest[1:] if args.rest[:1] == ["--"] else args.rest
    p = subprocess.Popen([exe] + rest, env=env, stdout=log, stderr=log,
                         start_new_session=True)
    # The token file is written once the server is bound; polling /ui/state is
    # what actually says the first frame has been drawn.
    deadline = time.time() + args.wait
    while time.time() < deadline:
        if p.poll() is not None:
            sys.exit("the GUI exited with code %s" % p.returncode)
        try:
            base, token = endpoint()
            q = urllib.parse.urlencode({"token": token} if token else {})
            with urllib.request.urlopen("%s/ui/state?%s" % (base, q), timeout=1) as r:
                # app_ready is false until the first frame has published the
                # app's own fields; an older build has no such key.
                if json.loads(r.read().decode("utf-8")).get("app_ready", True):
                    print(json.dumps({"ok": True, "pid": p.pid}))
                    return
                time.sleep(0.1)
        except Exception:
            time.sleep(0.25)
    sys.exit("the GUI did not answer within %gs" % args.wait)


def cmd_state(args):
    print(json.dumps(call("/ui/state"), indent=2, ensure_ascii=False))


def cmd_tree(args):
    params = {"named": "0" if args.all else "1"}
    if args.q:
        params["q"] = args.q
    if args.window:
        params["window"] = args.window
    items = call("/ui/tree", params)["items"]
    if args.json:
        print(json.dumps(items, indent=2, ensure_ascii=False))
        return
    for it in items:
        x0, y0, x1, y1 = it["rect"]
        print("%-38s %-28s %4.0f,%-4.0f %s"
              % (it["id"], it["label"][:28], (x0 + x1) / 2, (y0 + y1) / 2,
                 it["window"]))
    print("(%d items)" % len(items), file=sys.stderr)


def cmd_click(args):
    p = target(args)
    p["button"] = args.button
    p["double"] = "1" if args.double else "0"
    print(json.dumps(call("/ui/click", p)))


def cmd_move(args):
    print(json.dumps(call("/ui/move", target(args))))


def cmd_drag(args):
    print(json.dumps(call("/ui/drag", {"from": args.start, "to": args.end,
                                       "button": args.button,
                                       "steps": args.steps,
                                       "shift": "1" if args.shift else "0",
                                       "ctrl": "1" if args.ctrl else "0",
                                       "space": "1" if args.space else "0",
                                       "esc_mid": "1" if args.esc_mid else "0"})))


def cmd_scroll(args):
    p = target(args)
    p["dy"] = args.dy
    for m in ("shift", "ctrl", "alt"):
        if getattr(args, m):
            p[m] = 1
    print(json.dumps(call("/ui/scroll", p)))


def cmd_key(args):
    print(json.dumps(call("/ui/key", {"keys": args.keys})))


def cmd_text(args):
    p = target(args)
    p["value"] = args.value
    p["enter"] = "0" if args.no_enter else "1"
    print(json.dumps(call("/ui/text", p)))


def cmd_drop(args):
    paths = "\n".join(os.path.abspath(p) for p in args.paths)
    print(json.dumps(call("/ui/drop", {"paths": paths})))


def cmd_wait(args):
    print(json.dumps(call("/ui/wait", {"frames": args.frames})))


def cmd_shot(args):
    params = {"format": args.format, "quality": args.quality}
    if args.width:
        params["width"] = args.width
    img = call("/ui/screenshot", params, raw=True)
    with open(args.out, "wb") as f:
        f.write(img)
    print(json.dumps({"ok": True, "path": args.out, "bytes": len(img)}))


def cmd_script(args):
    """One command per line: `click open_dataset`, `key Escape`, `wait 4`."""
    src = sys.stdin if args.file == "-" else open(args.file)
    for raw in src:
        line = raw.split("#", 1)[0].strip()
        if not line:
            continue
        print("+", line, file=sys.stderr)
        main(line.split())


# ---------------------------------------------------------------------------

def add_target(p):
    p.add_argument("id", nargs="?", help="widget id (the i18n message name)")
    p.add_argument("--at", help="x,y in ImGui display coordinates instead")
    p.add_argument("--index", type=int, default=0,
                   help="which of several items sharing the id")


def build_parser():
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    sub = ap.add_subparsers(dest="cmd", required=True)

    p = sub.add_parser("launch", help="start the GUI with automation armed")
    p.add_argument("--exe", help="path to the spirula binary")
    p.add_argument("--offscreen", action="store_true")
    p.add_argument("--port", type=int)
    p.add_argument("--log", help="append the GUI's stdout/stderr here")
    p.add_argument("--wait", type=float, default=30.0)
    p.add_argument("rest", nargs=argparse.REMAINDER)
    p.set_defaults(func=cmd_launch)

    p = sub.add_parser("state", help="screen, phase, queue depth, sizes")
    p.set_defaults(func=cmd_state)

    p = sub.add_parser("tree", help="the widgets on screen")
    p.add_argument("-q", help="substring of the id or the visible label")
    p.add_argument("--window")
    p.add_argument("--all", action="store_true", help="include unnamed items")
    p.add_argument("--json", action="store_true")
    p.set_defaults(func=cmd_tree)

    p = sub.add_parser("click")
    add_target(p)
    p.add_argument("--button", type=int, default=0, help="0 left 1 right 2 mid")
    p.add_argument("--double", action="store_true")
    p.set_defaults(func=cmd_click)

    p = sub.add_parser("move", help="hover, e.g. to raise a tooltip")
    add_target(p)
    p.set_defaults(func=cmd_move)

    p = sub.add_parser("drag", help="press, move, release -- viewport gestures")
    p.add_argument("start", help="x,y")
    p.add_argument("end", help="x,y")
    p.add_argument("--button", type=int, default=0)
    p.add_argument("--steps", type=int, default=8)
    p.add_argument("--shift", action="store_true", help="hold Shift for the gesture")
    p.add_argument("--ctrl", action="store_true", help="hold Ctrl for the gesture")
    p.add_argument("--space", action="store_true", help="hold Space for the gesture")
    p.add_argument("--esc-mid", dest="esc_mid", action="store_true",
                   help="press Escape halfway through, to test cancel")
    p.set_defaults(func=cmd_drag)

    p = sub.add_parser("scroll")
    add_target(p)
    p.add_argument("--dy", type=float, default=-1.0)
    for m in ("shift", "ctrl", "alt"):
        p.add_argument("--" + m, action="store_true")
    p.set_defaults(func=cmd_scroll)

    p = sub.add_parser("key", help='a chord, e.g. "Ctrl+S" or "Escape"')
    p.add_argument("keys")
    p.set_defaults(func=cmd_key)

    p = sub.add_parser("text", help="focus an input and replace its contents")
    add_target(p)
    p.add_argument("value")
    p.add_argument("--no-enter", action="store_true")
    p.set_defaults(func=cmd_text)

    p = sub.add_parser("drop", help="drop files or folders on the window")
    p.add_argument("paths", nargs="+")
    p.set_defaults(func=cmd_drop)

    p = sub.add_parser("wait", help="let N frames pass")
    p.add_argument("--frames", type=int, default=4)
    p.set_defaults(func=cmd_wait)

    p = sub.add_parser("shot", help="write the framebuffer to an image file")
    p.add_argument("out")
    p.add_argument("--width", type=int, help="downscale to this width")
    p.add_argument("--format", choices=["png", "jpg"], default="png")
    p.add_argument("--quality", type=int, default=85, help="jpg only")
    p.set_defaults(func=cmd_shot)

    p = sub.add_parser("script", help="run one command per line")
    p.add_argument("file", nargs="?", default="-")
    p.set_defaults(func=cmd_script)
    return ap


def main(argv=None):
    # Labels carry CJK ("文A English"); a cp1252 console or pipe cannot encode it.
    sys.stdout.reconfigure(encoding="utf-8")
    args = build_parser().parse_args(argv)
    args.func(args)


if __name__ == "__main__":
    main()
