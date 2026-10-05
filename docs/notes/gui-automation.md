# Driving the GUI from outside it

`src/app/gui/Automation.{h,cpp}` opens a loopback HTTP surface that lists the
widgets currently on screen, injects mouse and keyboard input at them, and
hands back the framebuffer as an image. `tools/guictl.py` is the shell client
and `tools/gui_mcp.py` the same thing as an MCP server.

It exists because the editing work
([gui-editing-plan.md](gui-editing-plan.md)) is interaction code — a drag has
to produce the selection the user meant — and the only way to check that used
to be a human at the keyboard. It is also the cheapest regression test this
GUI has ever had: a script and a screenshot.

## Why not the usual approaches

Screen automation from outside the process (`xdotool` and friends) needs a real
display server, knows nothing about widgets, and on Wayland is mostly blocked
by design. Dear ImGui's own test engine does all of this properly, but it is a
separate dependency under its own licence, and the part of it that matters here
is three functions.

Those three functions are the ones ImGui already calls: build with
`IMGUI_ENABLE_TEST_ENGINE` and `ItemAdd()` reports every widget's id and
rectangle, the widgets report their label and status flags, and the hooks are
ours to implement. That is the whole item registry, from four symbols and no
call-site changes.

It costs one never-taken branch per widget per frame while
`ctx->TestEngineHookItems` is false, which is every run that did not ask for
this.

## Addressing a widget

The GUI hands ImGui `"text###<message name>"` for everything with an id
(`src/app/gui/Ui.h`), so that a language switch does not change a widget's
identity and collapse every open header. The same property is what makes
scripting it pleasant: the part after `###` is the i18n message name, it is the
same in all thirteen languages, and it is a name a person can read.

```
$ python3 tools/guictl.py tree -q open
home_open_dataset     Open a Dataset...        800,264  ##host/##home_...
home_open_splat       View a Trained Model     800,348  ##host/##home_...
$ python3 tools/guictl.py click home_open_splat
```

Widgets ImGui was given a hidden label (`"##fov"`) are addressable under that
spelling. Anything with no id at all — plain text, custom drawing, the viewport
image — is reached by coordinates instead, which is also how the viewport
gestures are driven.

## How a click happens

Injecting input is not one event. ImGui trickles its input queue so that a
mouse move and the button press after it land on *different* frames, and an
item is only hovered on the frame after the pointer reaches it. So a command
expands into a short script of steps, and the frame loop applies exactly one
step per frame: move, move, press, release, settle.

`begin_frame()` runs between `ImGui_ImplGlfw_NewFrame()` and
`ImGui::NewFrame()` — after the backend's own mouse update, so an injected
position is the later event and wins. `end_frame()` runs after the draw data is
rendered and before the buffers are swapped, which is where the item table for
the finished frame is published and where a screenshot reads the back buffer.

An HTTP request blocks until its last step has been applied and two more frames
have been drawn, so a command returns when its effect is on screen. `nowait=1`
opts out.

Two smaller details, both of which were bugs first: `ConfigDebugIgnoreFocusLoss`
is set, because a driven window is rarely the focused one and ImGui otherwise
clears the input state it was just handed; and a frame that applied a step
counts as busy, so a scripted run paces at the busy frame rate instead of the
15 Hz idle one.

## Running it

```bash
python3 tools/guictl.py launch --offscreen          # or: SS_GUI_AUTOMATION=1 ./build_cuda/spirula
python3 tools/guictl.py state
python3 tools/guictl.py tree -q mesh
python3 tools/guictl.py click menu_file
python3 tools/guictl.py key Escape
python3 tools/guictl.py drag 800,600 1000,550       # orbit the viewport
python3 tools/guictl.py text '##fov' 120
python3 tools/guictl.py shot out.png --width 1280 --format jpg
python3 tools/guictl.py script my-run.txt           # one command per line
```

`launch` passes any trailing arguments to the binary, which takes file paths
exactly as a drag-and-drop would — the shortest way to get a model open.

| variable | |
|---|---|
| `SS_GUI_AUTOMATION=1` | arm it; without this nothing binds and no hook runs |
| `SS_GUI_AUTOMATION_PORT` | default 7777 |
| `SS_GUI_AUTOMATION_TOKEN` | fixed token instead of a fresh random one |
| `SS_GUI_OFFSCREEN=1` | create the window invisible (still a real GL context) |

The server binds `127.0.0.1` only and requires a token. That is not paranoia
about the network: these are GETs with side effects, and any page in any
browser can issue one at localhost. The token is written to
`<config dir>/automation.json`, which is where both clients read it from.

Offscreen still needs a display to create a context against. For a machine with
none, run it under `Xvfb`; the rest behaves identically.

## Endpoints

All GET, all under `/ui/`, all taking `token=`. `id=` names a widget and
`at=x,y` a point in ImGui display coordinates; `index=` picks between items
sharing an id.

| | |
|---|---|
| `/ui/state` | frame counter, item count, queue depth, display and framebuffer size, plus what `GuiApp::state_json()` reports: screen, training phase and step, whether a dialog is open, how many models are loaded. `app_ready` is false until that half exists; `launch` waits for it |
| `/ui/tree` | the widgets of the last finished frame: `q=` substring, `window=`, `named=0` to include unnamed items |
| `/ui/click` | `button=` 0/1/2, `double=1`, `settle=` |
| `/ui/move` | hover, for tooltips and hover-only state |
| `/ui/drag` | `from=`, `to=`, `steps=` — more steps for a path a tool has to follow; `shift=1`, `ctrl=1`, `space=1` hold a key throughout, `esc_mid=1` presses Escape halfway |
| `/ui/scroll` | `dy=`; `shift=1`, `ctrl=1`, `alt=1` hold a key |
| `/ui/key` | `keys=Ctrl+Shift+A` |
| `/ui/text` | focus, select all, type `value=`, `enter=0` to leave it open |
| `/ui/wait` | `frames=` |
| `/ui/drop` | `paths=`, one per line: what dropping them on the window does |
| `/ui/screenshot` | `width=` to downscale (area average), `format=jpg`, `quality=`, `path=` to write it server-side |

Display coordinates are not framebuffer pixels on a HiDPI screen; `/ui/state`
reports both and the scale between them.

## As an MCP server

`tools/gui_mcp.py` wraps the same endpoints as MCP tools over stdio, in the
standard library alone — no dependency, and MCP over stdio is newline-delimited
JSON-RPC. `.mcp.json` at the repo root registers it for Claude Code; anything
else wants `python3 tools/gui_mcp.py` as the command.

The reason to prefer it over the shell client is `gui_screenshot`, which
returns the picture as an image rather than as a path to one. It downscales to
1280 and encodes JPEG by default: the full-size PNG is two megabytes of base64,
which costs far more than the picture is worth.

## What it is not

It drives the *interface*. It does not know what a tool means, and a test that
asserts "this drag selected 412 splats" wants the op-script path in
[gui-editing-plan.md](gui-editing-plan.md) instead, which needs no window at
all. Use this for the part that genuinely needs a window: that the gesture
reaches the tool, that the panel is laid out, that the picture is right.
