#!/usr/bin/env python3
"""Turn a tools/perf/record_run.ps1 session into report.html and a text summary.

Reads, from the session folder: train_perf.csv (the trainer, SS_TRAIN_PERF)
or dense_perf.csv (the dense step, `spirula dense --perf-dir`), system.csv, gpu_engines.csv, nvidia.csv and meta.json. Rows are matched by
their Unix-millisecond timestamps. Standard library only; nothing in the build
or the application uses this.

    python tools/perf/perf_report.py build_vulkan/perf/20261006-213000

What it answers, per progressive-resolution stage: how fast training ran, and
whether the GPU was the limit, or waited -- for data (decode on the CPU, or the
disk) or for other host work in the step. For a dense step, the same per phase
(prepare, match, refine, fuse): GPU-bound, CPU-bound on all cores or on one
thread, or disk-bound.
"""

import bisect
import csv
import html
import json
import math
import statistics
import sys
from pathlib import Path

GIB = 1024.0 ** 3
MIB = 1024.0 ** 2

# nvidia-smi clocks_event_reasons bits worth naming.
REASONS = {0x4: "power cap", 0x8: "hardware slowdown", 0x20: "thermal (software)",
           0x40: "thermal (hardware)", 0x80: "power brake"}


def read_csv(path):
    if not path.exists() or path.stat().st_size == 0:
        return []
    with path.open(newline="", encoding="utf-8-sig") as f:
        rows = []
        for r in csv.DictReader(f):
            out = {}
            for k, v in r.items():
                if k is None:
                    continue
                k = k.strip()
                try:
                    out[k] = float(v) if v not in ("", None) and k not in ("name", "pstate", "luid", "engine") else v
                except ValueError:
                    out[k] = v
            rows.append(out)
        return rows


def mean(xs):
    xs = [x for x in xs if isinstance(x, (int, float)) and not math.isnan(x)]
    return statistics.fmean(xs) if xs else float("nan")


def nearest(times, rows, t, tol_ms=1500):
    """The row whose timestamp is closest to t, within tol_ms."""
    if not times:
        return None
    i = bisect.bisect_left(times, t)
    best = None
    for j in (i - 1, i):
        if 0 <= j < len(times) and abs(times[j] - t) <= tol_ms:
            if best is None or abs(times[j] - t) < abs(times[best] - t):
                best = j
    return rows[best] if best is not None else None


def fmt(x, digits=1, suffix=""):
    return "–" if x is None or (isinstance(x, float) and math.isnan(x)) else f"{x:.{digits}f}{suffix}"


# ---------------------------------------------------------------------------
# Loading and joining
# ---------------------------------------------------------------------------

def load(session):
    meta = json.loads((session / "meta.json").read_text(encoding="utf-8-sig")) if (session / "meta.json").exists() else {}
    train = [r for r in read_csv(session / "train_perf.csv") if r.get("interval_s")]
    system = read_csv(session / "system.csv")
    nvidia = read_csv(session / "nvidia.csv")
    engines = read_csv(session / "gpu_engines.csv")

    # Spirula's busiest engine per adapter per sample: an adapter is as busy as its busiest queue.
    eng_by_t = {}
    for r in engines:
        key = (r["unix_ms"], r["luid"])
        v = r["util_pct"] if isinstance(r["util_pct"], float) and 0.0 <= r["util_pct"] <= 100.5 else 0.0
        eng_by_t[key] = max(eng_by_t.get(key, 0.0), v)   # Windows glitches when an engine's history resets
    luids = sorted({k[1] for k in eng_by_t})
    adapter_series = {l: sorted((t, v) for (t, ll), v in eng_by_t.items() if ll == l) for l in luids}
    return meta, train, system, nvidia, adapter_series


def derive(meta, train, system, nvidia):
    cores = int(meta.get("logical_processors") or 0) or None
    t0_candidates = [r["unix_ms"] for r in train[:1]] + [r["unix_ms"] for r in system[:1]]
    t0 = min(t0_candidates) if t0_candidates else 0
    sys_t = [r["unix_ms"] for r in system]
    nv_t = [r["unix_ms"] for r in nvidia]

    for r in system:
        core_vals = [v for k, v in r.items() if k.startswith("cpu_") and k != "cpu_total" and isinstance(v, float)]
        r["cpu_max_core"] = max(core_vals) if core_vals else float("nan")
        if cores is None:
            cores = len(core_vals) or None
        r["spirula_cores"] = r["spirula_cpu_pct"] / 100.0 if isinstance(r.get("spirula_cpu_pct"), float) else float("nan")
        r["ram_used_gib"] = (r["ram_total_bytes"] - r["ram_avail_bytes"]) / GIB if isinstance(r.get("ram_avail_bytes"), float) else float("nan")

    for r in train:
        steps = r["steps"]
        r["t"] = (r["unix_ms"] - t0) / 1000.0
        r["steps_per_s"] = steps / r["interval_s"] if r["interval_s"] > 0 else float("nan")
        r["ms_per_step"] = 1000.0 * r["step_s"] / steps if steps else float("nan")
        r["wait_share"] = min(1.0, r["data_wait_s"] / r["step_s"]) if r["step_s"] > 0 else float("nan")
        per_gpu = r["gpu_sampled_s"] / r["gpu_sampled_steps"] if r["gpu_sampled_steps"] else float("nan")
        r["gpu_share"] = min(1.0, per_gpu * steps / r["step_s"]) if r["step_s"] > 0 and not math.isnan(per_gpu) else float("nan")
        s = nearest(sys_t, system, r["unix_ms"])
        n = nearest(nv_t, nvidia, r["unix_ms"])
        r["sys"] = s or {}
        r["nv"] = n or {}
    return t0, cores


# ---------------------------------------------------------------------------
# Diagnosis
# ---------------------------------------------------------------------------

def verdict(st, cores):
    wait, gpu, cpu = st["wait_share"], st["gpu_share"], st["cpu_total"]
    nv = st["nv_util"]
    if not math.isnan(wait) and wait >= 0.3:
        busy_cores = st["spirula_cores"]
        if (not math.isnan(cpu) and cpu >= 85) or (cores and not math.isnan(busy_cores) and busy_cores >= 0.75 * cores):
            return ("data-bound: CPU", "Steps wait for images and the CPU is saturated, so decoding and "
                    "resizing on the CPU is the limit. Keeping images in RAM (cache_images cpu) or reusing "
                    "decoded copies removes most of it.")
        if not math.isnan(st["disk_busy"]) and st["disk_busy"] >= 80:
            return ("data-bound: disk", "Steps wait for images while the disk is busy and the CPU is not: "
                    "storage throughput is the limit.")
        return ("data-bound", "Steps wait for images though neither the CPU nor the disk is saturated: "
                "too little decode parallelism or prefetch for this step rate.")
    if (not math.isnan(gpu) and gpu >= 0.75) or (not math.isnan(nv) and nv >= 85):
        return ("GPU-bound", "The GPU is busy for most of each step. This is the expected state.")
    return ("host overhead", "The GPU is mostly idle but steps are not waiting for data: other "
            "per-step host work (synchronization, densification, logging, the viewer) dominates.")


def stages(train, cores):
    out = []
    active = [r for r in train if r["steps"] > 0 and not r.get("paused")]
    for divisor in sorted({int(r["divisor"]) for r in active}, reverse=True):
        rows = [r for r in active if int(r["divisor"]) == divisor]
        steps = sum(r["steps"] for r in rows)
        step_s = sum(r["step_s"] for r in rows)
        wait = sum(r["data_wait_s"] for r in rows)
        gpu_n = sum(r["gpu_sampled_steps"] for r in rows)
        gpu_per = sum(r["gpu_sampled_s"] for r in rows) / gpu_n if gpu_n else float("nan")
        throttle = {}
        for r in rows:
            bits = r["nv"].get("clocks_event_reasons_active")
            try:
                v = int(str(bits), 16)
            except (TypeError, ValueError):
                continue
            for bit, name in REASONS.items():
                if v & bit:
                    throttle[name] = throttle.get(name, 0) + 1
        st = {
            "divisor": divisor,
            "seconds": sum(r["interval_s"] for r in rows),
            "steps": steps,
            "steps_per_s": steps / max(1e-9, sum(r["interval_s"] for r in rows)),
            "ms_per_step": 1000.0 * step_s / steps if steps else float("nan"),
            "wait_share": wait / step_s if step_s else float("nan"),
            "gpu_share": min(1.0, gpu_per * steps / step_s) if step_s and not math.isnan(gpu_per) else float("nan"),
            "cpu_total": mean(r["sys"].get("cpu_total") for r in rows),
            "spirula_cores": mean(r["sys"].get("spirula_cores") for r in rows),
            "disk_read_mb": mean((r["sys"].get("disk_read_bps") or 0) / MIB for r in rows if r["sys"]),
            "disk_busy": mean(r["sys"].get("disk_busy_pct") for r in rows),
            "nv_util": mean(r["nv"].get("utilization_gpu") for r in rows),
            "nv_power": mean(r["nv"].get("power_draw") for r in rows),
            "nv_temp": mean(r["nv"].get("temperature_gpu") for r in rows),
            "throttle": {k: v / len(rows) for k, v in throttle.items()},
            "ram_used": mean(r["sys"].get("ram_used_gib") for r in rows),
        }
        st["verdict"], st["why"] = verdict(st, cores)
        out.append(st)
    return out


# ---------------------------------------------------------------------------
# SVG charts
# ---------------------------------------------------------------------------

PALETTE = ["#2a78d6", "#d9611e", "#1f9e6a", "#9b4fd1", "#c7334a", "#7a7a7a"]


DENSE_PHASES = ["prepare", "load", "match", "refine", "fuse", "outliers", "export"]


def dense_verdict(st, cores):
    gpu, cpu, busiest, used = st["gpu"], st["cpu_total"], st["cpu_max_core"], st["spirula_cores"]
    if not math.isnan(gpu) and gpu >= 75:
        return ("GPU-bound", "The GPU is busy for most of this phase: the expected state for matching, and the limit here.")
    if (not math.isnan(cpu) and cpu >= 85) or (cores and not math.isnan(used) and used >= 0.75 * cores):
        return ("CPU-bound: all cores", "Every core is busy and the GPU waits: the phase is limited by "
                "CPU work it already spreads across threads (decoding, sampling, refinement, fusion).")
    if not math.isnan(busiest) and busiest >= 90 and not math.isnan(used) and used < 2.5:
        return ("CPU-bound: one thread", "One core is saturated while the rest idle: a serial part of this "
                "phase is the limit, and spreading it across threads would speed it up.")
    if not math.isnan(st["disk_busy"]) and st["disk_busy"] >= 80:
        return ("disk-bound", "The disk is busy while the CPU and GPU are not: reading images, caches or "
                "spilled data is the limit.")
    return ("waiting", "Neither the CPU, the GPU nor the disk is saturated: time goes to synchronization, "
            "memory latency or work this sampling cannot see.")


def dense_phases(dense, cores):
    names = [p for p in DENSE_PHASES if any(r["phase"] == p for r in dense)]
    names += sorted({r["phase"] for r in dense} - set(DENSE_PHASES) - {"complete"})
    out = []
    for name in names:
        rows = [r for r in dense if r["phase"] == name]
        nv = lambda r: r["nv"].get("utilization_gpu") if isinstance(r["nv"].get("utilization_gpu"), float) else float("nan")
        both = lambda r: max((v for v in (nv(r), r["adapter"]) if not math.isnan(v)), default=float("nan"))
        st = {
            "phase": name,
            "seconds": sum(r["interval_s"] for r in rows),
            "done": max((r["done"] for r in rows), default=0),
            "total": max((r["total"] for r in rows), default=0),
            "rate": mean(r["done_per_s"] for r in rows if r["done_per_s"] > 0),
            "gpu": mean(both(r) for r in rows),
            "nv_util": mean(nv(r) for r in rows),
            "cpu_total": mean(r["sys"].get("cpu_total") for r in rows),
            "cpu_max_core": mean(r["sys"].get("cpu_max_core") for r in rows),
            "spirula_cores": mean(r["sys"].get("spirula_cores") for r in rows),
            "disk_read_mb": mean((r["sys"].get("disk_read_bps") or 0) / MIB for r in rows if r["sys"]),
            "disk_busy": mean(r["sys"].get("disk_busy_pct") for r in rows),
            "ram_used": mean(r["sys"].get("ram_used_gib") for r in rows),
        }
        st["verdict"], st["why"] = dense_verdict(st, cores)
        out.append(st)
    return out


def chart(title, series, t_max, y_label, bands=(), y_max=None, height=170):
    """series: [(name, [(t, y), ...])]."""
    W, H, L, R, T, B = 900, height, 56, 12, 22, 26
    pts = [y for _, s in series for _, y in s if isinstance(y, (int, float)) and not math.isnan(y)]
    if not pts:
        return f"<section><h3>{html.escape(title)}</h3><p class='dim'>No data.</p></section>"
    top = y_max if y_max is not None else max(pts) * 1.08 or 1.0
    x = lambda t: L + (W - L - R) * (t / t_max if t_max > 0 else 0)
    y = lambda v: T + (H - T - B) * (1 - min(max(v / top, 0), 1))
    parts = [f"<svg viewBox='0 0 {W} {H}' role='img' aria-label='{html.escape(title)}'>"]
    for i, (t_a, t_b, label) in enumerate(bands):
        parts.append(f"<rect x='{x(t_a):.1f}' y='{T}' width='{max(0.5, x(t_b) - x(t_a)):.1f}' height='{H - T - B}' "
                     f"class='band{i % 2}'/><text x='{x(t_a) + 4:.1f}' y='{T + 12}' class='bandlabel'>{html.escape(label)}</text>")
    for k in range(5):
        v = top * k / 4
        parts.append(f"<line x1='{L}' x2='{W - R}' y1='{y(v):.1f}' y2='{y(v):.1f}' class='grid'/>"
                     f"<text x='{L - 6}' y='{y(v) + 4:.1f}' class='axis' text-anchor='end'>{v:.3g}</text>")
    for k in range(6):
        t = t_max * k / 5
        label = f"{t / 60:.0f} min" if t_max >= 120 else f"{t:.0f} s"
        parts.append(f"<text x='{x(t):.1f}' y='{H - 8}' class='axis' text-anchor='middle'>{label}</text>")
    legend = []
    for i, (name, s) in enumerate(series):
        c = PALETTE[i % len(PALETTE)]
        path = " ".join(f"{x(t):.1f},{y(v):.1f}" for t, v in s if isinstance(v, (int, float)) and not math.isnan(v))
        if path:
            parts.append(f"<polyline fill='none' stroke='{c}' stroke-width='1.6' points='{path}'/>")
        legend.append(f"<span><i style='background:{c}'></i>{html.escape(name)}</span>")
    parts.append(f"<text x='{L}' y='14' class='axis'>{html.escape(y_label)}</text></svg>")
    return f"<section><h3>{html.escape(title)}</h3>{''.join(parts)}<div class='legend'>{''.join(legend)}</div></section>"


# ---------------------------------------------------------------------------
# Report
# ---------------------------------------------------------------------------

def main(argv):
    if len(argv) != 2:
        print(__doc__)
        return 2
    session = Path(argv[1])
    if hasattr(sys.stdout, "reconfigure"):
        sys.stdout.reconfigure(encoding="utf-8", errors="replace")
    # A run's perf folder holds one session per start (a resume adds one); take the newest.
    if not (session / "system.csv").exists():
        sessions = sorted(p for p in session.glob("*") if (p / "system.csv").exists())
        if sessions:
            if len(sessions) > 1:
                print(f"{len(sessions)} sessions in {session}; reading the newest. Earlier: "
                      + ", ".join(p.name for p in sessions[:-1]))
            session = sessions[-1]
    meta, train, system, nvidia, adapters = load(session)
    dense = [r for r in read_csv(session / "dense_perf.csv") if isinstance(r.get("interval_s"), float)]
    meta["cpu"] = str(meta.get("cpu", "")).strip()
    if not system and not train and not dense:
        print(f"No samples in {session}")
        return 1
    t0, cores = derive(meta, train, system, nvidia)
    rel = lambda ms: (ms - t0) / 1000.0
    t_max = max([rel(r["unix_ms"]) for r in system] + [r["t"] for r in train] + [1.0])

    # Resolution stages as shaded bands.
    bands, cur = [], None
    for r in train:
        d = int(r["divisor"])
        if cur is None or d != cur[2]:
            if cur:
                bands.append((cur[0], r["t"], cur[1]))
            cur = (r["t"], "1/%d" % d if d > 1 else "full", d)
    if cur:
        bands.append((cur[0], t_max, cur[1]))
    if len(bands) == 1 and bands[0][2] == "full":
        bands = []
    # The built-in recorder starts at setup, before the first step: loading and caching images.
    setup = [r for r in system if train and r["unix_ms"] < train[0]["unix_ms"]]
    if setup and train[0]["t"] > 2.0:
        bands.insert(0, (0.0, train[0]["t"], "setup"))

    st = stages(train, cores)

    # A dense step: its phases are the bands and the table.
    sys_t, nv_t = [r["unix_ms"] for r in system], [r["unix_ms"] for r in nvidia]
    for r in dense:
        r["t"] = rel(r["unix_ms"])
        r["sys"] = nearest(sys_t, system, r["unix_ms"]) or {}
        r["nv"] = nearest(nv_t, nvidia, r["unix_ms"]) or {}
        # Spirula's busiest adapter, the driver's per-process view for any GPU vendor.
        near = [v for series in adapters.values() for t_, v in series if abs(t_ - r["unix_ms"]) <= 1500]
        r["adapter"] = max(near) if near else float("nan")
    phases = dense_phases(dense, cores)
    if dense and not train:
        cur = None
        for r in dense:
            if cur is None or r["phase"] != cur[1]:
                if cur:
                    bands.append((cur[0], r["t"], cur[1]))
                cur = (r["t"], r["phase"])
        if cur:
            bands.append((cur[0], t_max, cur[1]))
    sys_series = lambda key, scale=1.0: [(rel(r["unix_ms"]), r[key] * scale) for r in system if isinstance(r.get(key), float)]
    nv_series = lambda key, scale=1.0: [(rel(r["unix_ms"]), r[key] * scale) for r in nvidia if isinstance(r.get(key), float)]

    charts = []
    if dense:
        charts.append(chart("Dense progress", [
            ("% of the phase done", [(r["t"], 100.0 * r["done"] / r["total"]) for r in dense if r["total"] > 0])],
            t_max, "%", bands, 100))
        charts.append(chart("Dense rate", [("items per second", [(r["t"], r["done_per_s"]) for r in dense])],
                            t_max, "/s", bands))
    if train:
        charts.append(chart("Training throughput", [("steps per second", [(r["t"], r["steps_per_s"]) for r in train])],
                            t_max, "steps/s", bands))
        charts.append(chart("Where each step's time goes", [
            ("waiting for data", [(r["t"], 100 * r["wait_share"]) for r in train]),
            ("GPU busy (sampled)", [(r["t"], 100 * r["gpu_share"]) for r in train])], t_max, "% of step time", bands, 100))
        charts.append(chart("Step time", [("ms per step", [(r["t"], r["ms_per_step"]) for r in train])], t_max, "ms", bands))
    charts.append(chart("CPU", [("system total %", sys_series("cpu_total")),
                                ("busiest core %", sys_series("cpu_max_core")),
                                ("Spirula, % of all cores", [(rel(r["unix_ms"]), 100 * r["spirula_cores"] / cores)
                                                             for r in system if cores and not math.isnan(r["spirula_cores"])])],
                        t_max, "%", bands, 100))
    gpu_lines = [("NVIDIA utilization %", nv_series("utilization_gpu"))] if nvidia else []
    for i, (luid, s) in enumerate(adapters.items()):
        gpu_lines.append((f"Spirula on adapter {i + 1} ({luid[-6:]})", [(rel(t), v) for t, v in s]))
    charts.append(chart("GPU", gpu_lines, t_max, "%", bands, 100))
    mem = [("system RAM used GiB", sys_series("ram_used_gib")),
           ("Spirula private GiB", sys_series("spirula_private_bytes", 1 / GIB))]
    if nvidia:
        mem.append(("NVIDIA VRAM used GiB", nv_series("memory_used", 1 / 1024)))
    if train:
        mem.append(("trainer pool VRAM GiB", [(r["t"], r["vram_bytes"] / GIB) for r in train]))
    charts.append(chart("Memory", mem, t_max, "GiB", bands))
    charts.append(chart("Disk", [("system read MB/s", sys_series("disk_read_bps", 1 / MIB)),
                                 ("Spirula read MB/s", sys_series("spirula_read_bps", 1 / MIB))], t_max, "MB/s", bands))
    if nvidia:
        charts.append(chart("NVIDIA power, clock and temperature", [
            ("power W", nv_series("power_draw")), ("SM clock / 30 MHz", nv_series("clocks_sm", 1 / 30)),
            ("temperature °C", nv_series("temperature_gpu"))], t_max, "", bands))
    if train:
        charts.append(chart("Splats", [("splats (millions)", [(r["t"], r["splats"] / 1e6) for r in train])], t_max, "M", bands))

    # Text summary.
    lines = [f"Session: {session}", f"Machine: {meta.get('cpu', '?')}, {cores} threads, "
             f"{fmt((meta.get('ram_bytes') or 0) / GIB, 0)} GiB RAM",
             f"GPUs: {', '.join(g.get('name', '?') for g in meta.get('gpus', []))}", ""]
    config_path = next((d / "config.json" for d in (session.parent, session.parent.parent)
                        if (d / "config.json").exists()), session.parent / "config.json")
    if config_path.exists():
        try:
            cfg = json.loads(config_path.read_text(encoding="utf-8-sig"))
            keys = ["num_iterations", "cap_max", "batch_size", "cache_images", "train_resolution_divisor",
                    "progressive_resolution", "progressive_resolution_start", "progressive_resolution_full_at",
                    "progressive_resolution_schedule", "progressive_splat_budget", "progressive_splat_budget_schedule"]
            lines += ["Settings: " + ", ".join(f"{k} {cfg[k]}" for k in keys if cfg.get(k) not in (None, "")), ""]
        except (ValueError, OSError):
            pass
    # A dense run's cloud is the generation published as it ended: its manifest says
    # which settings ran and how many pairs came from the prediction cache.
    if dense:
        end_s = dense[-1]["unix_ms"] / 1000.0
        found = []
        for m in (session.parent.parent / "generations").glob("*/manifest.json"):
            if abs(m.stat().st_mtime - end_s) < 120:
                found.append(m)
        for m in found[:1]:
            try:
                man = json.loads(m.read_text(encoding="utf-8-sig"))
                s = man.get("settings", {})
                pairs, cached = man.get("matched_pairs", 0), man.get("cached_pairs", 0)
                lines += [f"Settings: preset {s.get('preset')}, {s.get('low_width')}x{s.get('low_height')}, "
                          f"{s.get('matching_space')} matching, {man.get('inference_precision')} precision",
                          f"Pairs: {pairs}, {cached} reused from the prediction cache; model inference "
                          f"{fmt(man.get('inference_seconds'), 0)} s, loading {fmt(man.get('model_load_seconds'), 0)} s"]
                if pairs and cached == pairs:
                    lines.append("  Every pair came from the cache: matching here is reading and triangulating, "
                                 "not the model, so its time is not comparable with a run that inferred.")
                lines.append("")
            except (ValueError, OSError):
                pass
    if not train and not dense:
        lines.append("No train_perf.csv: tick Log performance stats in the GUI (or start Spirula through "
                     "record_run.ps1) to see where step time goes.")
    if setup:
        secs = (train[0]["unix_ms"] - setup[0]["unix_ms"]) / 1000.0
        lines += [f"Before the first step (loading, caching images): {fmt(secs / 60)} min, "
                  f"CPU {fmt(mean(r.get('cpu_total') for r in setup), 0, '%')}, "
                  f"Spirula {fmt(mean(r.get('spirula_cores') for r in setup))} cores, "
                  f"disk {fmt(mean((r.get('disk_read_bps') or 0) / MIB for r in setup), 0)} MB/s, "
                  f"RAM up to {fmt(max((r.get('ram_used_gib') or 0) for r in setup), 1)} GiB", ""]
    for s in phases:
        lines += [f"Phase {s['phase']}: {fmt(s['seconds'] / 60)} min, {int(s['done'])} of {int(s['total'])} done, "
                  f"{fmt(s['rate'], 2)} per second while advancing",
                  f"  GPU {fmt(s['gpu'], 0, '%')} (NVIDIA {fmt(s['nv_util'], 0, '%')}), CPU {fmt(s['cpu_total'], 0, '%')}, "
                  f"busiest core {fmt(s['cpu_max_core'], 0, '%')}, Spirula {fmt(s['spirula_cores'])} cores, "
                  f"disk {fmt(s['disk_read_mb'], 0)} MB/s, RAM {fmt(s['ram_used'], 1)} GiB",
                  f"  -> {s['verdict']}: {s['why']}", ""]
    for s in st:
        label = "1/%d" % s["divisor"] if s["divisor"] > 1 else "full size"
        thr = ", ".join(f"{k} {100 * v:.0f}%" for k, v in s["throttle"].items()) or "none"
        lines += [f"Stage {label}: {fmt(s['seconds'] / 60)} min, {int(s['steps'])} steps, "
                  f"{fmt(s['steps_per_s'], 2)} steps/s, {fmt(s['ms_per_step'])} ms/step",
                  f"  waiting for data {fmt(100 * s['wait_share'], 0, '%')}, GPU busy {fmt(100 * s['gpu_share'], 0, '%')}, "
                  f"NVIDIA {fmt(s['nv_util'], 0, '%')}, CPU {fmt(s['cpu_total'], 0, '%')}, "
                  f"Spirula {fmt(s['spirula_cores'])} cores, disk {fmt(s['disk_read_mb'], 0)} MB/s, "
                  f"GPU power {fmt(s['nv_power'], 0)} W, {fmt(s['nv_temp'], 0)} °C, clock limits: {thr}",
                  f"  -> {s['verdict']}: {s['why']}", ""]
    summary = "\n".join(lines)
    print(summary)

    rows = "".join(
        f"<tr><td>{'1/%d' % s['divisor'] if s['divisor'] > 1 else 'full'}</td><td>{fmt(s['seconds'] / 60)}</td>"
        f"<td>{int(s['steps'])}</td><td>{fmt(s['steps_per_s'], 2)}</td><td>{fmt(s['ms_per_step'])}</td>"
        f"<td>{fmt(100 * s['wait_share'], 0, '%')}</td><td>{fmt(100 * s['gpu_share'], 0, '%')}</td>"
        f"<td>{fmt(s['nv_util'], 0, '%')}</td><td>{fmt(s['cpu_total'], 0, '%')}</td><td>{fmt(s['spirula_cores'])}</td>"
        f"<td>{fmt(s['disk_read_mb'], 0)}</td><td><b>{html.escape(s['verdict'])}</b><br>"
        f"<span class='dim'>{html.escape(s['why'])}</span></td></tr>" for s in st)
    dense_rows = "".join(
        f"<tr><td>{html.escape(s['phase'])}</td><td>{fmt(s['seconds'] / 60)}</td>"
        f"<td>{int(s['done'])} / {int(s['total'])}</td><td>{fmt(s['rate'], 2)}</td><td>{fmt(s['gpu'], 0, '%')}</td>"
        f"<td>{fmt(s['cpu_total'], 0, '%')}</td><td>{fmt(s['cpu_max_core'], 0, '%')}</td><td>{fmt(s['spirula_cores'])}</td>"
        f"<td>{fmt(s['disk_read_mb'], 0)}</td><td><b>{html.escape(s['verdict'])}</b><br>"
        f"<span class='dim'>{html.escape(s['why'])}</span></td></tr>" for s in phases)
    dense_table = ("<table><thead><tr><th>Phase</th><th>min</th><th>done</th><th>per s</th><th>GPU</th><th>CPU</th>"
                   "<th>busiest core</th><th>Spirula cores</th><th>disk MB/s</th><th>Verdict</th></tr></thead>"
                   f"<tbody>{dense_rows}</tbody></table>") if phases else ""
    title = "Dense step performance" if phases and not st else "Training performance"
    table = ("<table><thead><tr><th>Stage</th><th>min</th><th>steps</th><th>steps/s</th><th>ms/step</th>"
             "<th>data wait</th><th>GPU busy</th><th>NVIDIA</th><th>CPU</th><th>Spirula cores</th><th>disk MB/s</th>"
             f"<th>Verdict</th></tr></thead><tbody>{rows}</tbody></table>") if st else ""
    page = f"""<!doctype html><html lang="en"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1"><title>{title}</title>
<style>
:root {{ --bg:#fbfbfa; --fg:#1d1d1f; --dim:#6b6b70; --grid:#e3e3e0; --band0:#eef3fb; --band1:#f6f1ea; }}
@media (prefers-color-scheme: dark) {{ :root {{ --bg:#161618; --fg:#ececee; --dim:#9a9aa0; --grid:#2c2c30;
  --band0:#1c2533; --band1:#2a241c; }} }}
body {{ background:var(--bg); color:var(--fg); font:14px/1.45 system-ui, sans-serif; margin:0 auto; max-width:960px; padding:16px; }}
h1 {{ font-size:22px; margin:8px 0 4px; }} h3 {{ font-size:15px; margin:22px 0 4px; }}
.dim {{ color:var(--dim); }} svg {{ width:100%; height:auto; display:block; }}
.grid {{ stroke:var(--grid); }} .axis, .bandlabel {{ fill:var(--dim); font-size:11px; }}
.band0 {{ fill:var(--band0); }} .band1 {{ fill:var(--band1); }}
.legend span {{ margin-right:14px; font-size:12px; color:var(--dim); white-space:nowrap; }}
.legend i {{ display:inline-block; width:10px; height:10px; border-radius:2px; margin-right:5px; vertical-align:-1px; }}
table {{ border-collapse:collapse; width:100%; font-size:13px; margin-top:8px; }}
th, td {{ text-align:left; padding:6px 8px; border-bottom:1px solid var(--grid); vertical-align:top; }}
pre {{ white-space:pre-wrap; background:var(--band0); padding:10px; border-radius:6px; font-size:12px; }}
.wrap {{ overflow-x:auto; }}
</style></head><body>
<h1>{title}</h1>
<p class="dim">{html.escape(meta.get('cpu', ''))} · {', '.join(html.escape(g.get('name', '')) for g in meta.get('gpus', []))}</p>
<div class="wrap">{dense_table}{table}</div>
{''.join(charts)}
<h3>Summary</h3><pre>{html.escape(summary)}</pre>
</body></html>"""
    out = session / "report.html"
    out.write_text(page, encoding="utf-8")
    print(f"Wrote {out}")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
