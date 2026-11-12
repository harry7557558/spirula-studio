// Stacked time-series plots for the IMU/GPS streams, on a 2D canvas.
//
// Each channel keeps a min/max pyramid (each level 8x coarser than the one
// below), so a zoomed-out view of a million-sample gyro reads a few thousand
// buckets instead of the raw array, and a zoomed-in view falls back to raw
// samples. Redraw cost is bounded by the canvas width, not the capture length.

import { indexAt } from './geo.js';

const FACTOR = 8;
const CSS = {
  bg: '#12141b', grid: '#222532', axis: '#5a5e72', text: '#c8cad4',
  dim: '#5a5e72', bright: '#e8eaf2', play: '#e05555',
};

class Channel {
  constructor(name, color, t, v, scale) {
    this.name = name; this.color = color; this.t = t;
    this.v = scale && scale !== 1 ? v.map((x) => x * scale) : v;
    this.n = this.v.length;
    this.levels = null;
  }
  build() {
    if (this.levels) return;
    this.levels = [];
    let lo = this.v, hi = this.v;
    while (lo.length > 512) {
      const m = Math.ceil(lo.length / FACTOR);
      const nlo = new Float32Array(m), nhi = new Float32Array(m);
      for (let b = 0; b < m; b++) {
        let a = Infinity, z = -Infinity;
        const s = b * FACTOR, e = Math.min(lo.length, s + FACTOR);
        for (let i = s; i < e; i++) { if (lo[i] < a) a = lo[i]; if (hi[i] > z) z = hi[i]; }
        nlo[b] = a; nhi[b] = z;
      }
      this.levels.push({ min: nlo, max: nhi, step: FACTOR ** (this.levels.length + 1) });
      lo = nlo; hi = nhi;
    }
  }
}

export class Charts {
  constructor(canvas) {
    this.canvas = canvas;
    this.ctx = canvas.getContext('2d');
    this.rows = [];
    this.t0 = 0; this.t1 = 1;
    this.tMin = 0; this.tMax = 1;
    this.play = 0;
    this.hover = null;
    this.onPlay = () => {};
    this.onHover = () => {};
    this.gutter = 58;
    this.axisH = 20;
    this.xMode = 'time';
    this._wire();
  }

  setRows(rows, tMin, tMax) {
    this.rows = rows;
    this.tMin = tMin; this.tMax = tMax > tMin ? tMax : tMin + 1;
    this.t0 = this.tMin; this.t1 = this.tMax;
  }
  setPlay(t) { this.play = t; }
  resetZoom() { this.t0 = this.tMin; this.t1 = this.tMax; this.draw(); }

  xOf(t) {
    const w = this.canvas.clientWidth - this.gutter - 8;
    return this.gutter + (t - this.t0) / (this.t1 - this.t0) * w;
  }
  tOf(x) {
    const w = this.canvas.clientWidth - this.gutter - 8;
    return this.t0 + (x - this.gutter) / w * (this.t1 - this.t0);
  }

  _wire() {
    const c = this.canvas;
    let dragging = null;
    const local = (e) => {
      const r = c.getBoundingClientRect();
      return [e.clientX - r.left, e.clientY - r.top];
    };
    c.addEventListener('pointerdown', (e) => {
      const [x] = local(e);
      c.setPointerCapture(e.pointerId);
      dragging = (e.button === 0 && !e.shiftKey) ? 'scrub' : 'pan';
      this._panX = x;
      if (dragging === 'scrub') { this.play = this._clampT(this.tOf(x)); this.onPlay(this.play); this.draw(); }
      e.preventDefault();
    });
    c.addEventListener('pointermove', (e) => {
      const [x, y] = local(e);
      this.hover = [x, y];
      if (dragging === 'scrub') { this.play = this._clampT(this.tOf(x)); this.onPlay(this.play); }
      else if (dragging === 'pan') {
        const w = c.clientWidth - this.gutter - 8;
        const dt = (this._panX - x) / w * (this.t1 - this.t0);
        this._panX = x;
        this._shift(dt);
      }
      this.onHover(this.tOf(x));
      this.draw();
    });
    const end = () => { dragging = null; };
    c.addEventListener('pointerup', end);
    c.addEventListener('pointercancel', end);
    c.addEventListener('pointerleave', () => { this.hover = null; this.draw(); });
    c.addEventListener('dblclick', () => this.resetZoom());
    c.addEventListener('wheel', (e) => {
      e.preventDefault();
      const [x] = local(e);
      const t = this.tOf(x);
      const k = Math.exp(e.deltaY * 0.0015);
      let a = t + (this.t0 - t) * k, b = t + (this.t1 - t) * k;
      if (b - a > this.tMax - this.tMin) { a = this.tMin; b = this.tMax; }
      if (b - a < 1e-3) return;
      this.t0 = a; this.t1 = b;
      this._clampRange();
      this.draw();
    }, { passive: false });
  }
  _clampT(t) { return Math.max(this.tMin, Math.min(this.tMax, t)); }
  _shift(dt) { this.t0 += dt; this.t1 += dt; this._clampRange(); }
  _clampRange() {
    const span = this.t1 - this.t0;
    if (this.t0 < this.tMin) { this.t0 = this.tMin; this.t1 = this.tMin + span; }
    if (this.t1 > this.tMax) { this.t1 = this.tMax; this.t0 = this.tMax - span; }
    if (this.t0 < this.tMin) this.t0 = this.tMin;
  }

  valueAt(ch, t) {
    const i = indexAt(t, ch.t, ch.n);
    return i < 0 ? NaN : ch.v[i];
  }

  // Column min/max for one channel over the visible span; null when the row
  // has no sample in view.
  _columns(ch, width) {
    const i0 = indexAt(this.t0, ch.t, ch.n), i1 = Math.min(ch.n - 1, indexAt(this.t1, ch.t, ch.n) + 1);
    if (i1 < i0) return null;
    const count = i1 - i0 + 1;
    ch.build();
    let lvl = -1, step = 1;
    while (lvl + 1 < ch.levels.length && count / (step * FACTOR) > width) { lvl++; step *= FACTOR; }
    const lo = new Float32Array(width).fill(Infinity);
    const hi = new Float32Array(width).fill(-Infinity);
    const span = this.t1 - this.t0;
    let any = false;
    const put = (t, a, b) => {
      let c = Math.floor((t - this.t0) / span * width);
      if (c < 0) c = 0; else if (c >= width) c = width - 1;
      if (a < lo[c]) lo[c] = a;
      if (b > hi[c]) hi[c] = b;
      any = true;
    };
    if (lvl < 0) {
      for (let i = i0; i <= i1; i++) put(ch.t[i], ch.v[i], ch.v[i]);
    } else {
      const L = ch.levels[lvl];
      const b0 = Math.floor(i0 / step), b1 = Math.min(L.min.length - 1, Math.floor(i1 / step));
      for (let b = b0; b <= b1; b++) put(ch.t[Math.min(ch.n - 1, b * step)], L.min[b], L.max[b]);
    }
    return any ? { lo, hi } : null;
  }

  draw() {
    const c = this.canvas, ctx = this.ctx;
    const dpr = Math.min(window.devicePixelRatio || 1, 2);
    const W = c.clientWidth, H = c.clientHeight;
    if (c.width !== Math.round(W * dpr) || c.height !== Math.round(H * dpr)) {
      c.width = Math.round(W * dpr); c.height = Math.round(H * dpr);
    }
    ctx.setTransform(dpr, 0, 0, dpr, 0, 0);
    ctx.clearRect(0, 0, W, H);
    ctx.fillStyle = CSS.bg;
    ctx.fillRect(0, 0, W, H);
    const plotW = Math.max(1, Math.floor(W - this.gutter - 8));
    if (!this.rows.length) {
      ctx.fillStyle = CSS.dim;
      ctx.font = '11px ui-monospace, monospace';
      ctx.fillText('No stream selected', this.gutter, H / 2);
      return;
    }
    const rowH = Math.max(28, (H - this.axisH) / this.rows.length);
    ctx.font = '10px ui-monospace, monospace';
    for (let r = 0; r < this.rows.length; r++) {
      const row = this.rows[r];
      const yTop = r * rowH + 2, yBot = yTop + rowH - 6;
      let vmin = Infinity, vmax = -Infinity;
      const cols = [];
      for (const ch of row.channels) {
        const col = this._columns(ch, plotW);
        cols.push(col);
        if (!col) continue;
        for (let i = 0; i < plotW; i++) {
          if (col.lo[i] < vmin) vmin = col.lo[i];
          if (col.hi[i] > vmax) vmax = col.hi[i];
        }
      }
      if (!isFinite(vmin) || !isFinite(vmax)) { vmin = -1; vmax = 1; }
      if (row.sym) { const m = Math.max(Math.abs(vmin), Math.abs(vmax)) || 1; vmin = -m; vmax = m; }
      if (vmax - vmin < 1e-9) { vmax += 1e-9 + Math.abs(vmax) * 1e-3; vmin -= 1e-9; }
      const pad = (vmax - vmin) * 0.08;
      vmin -= pad; vmax += pad;
      const yOf = (v) => yBot - (v - vmin) / (vmax - vmin) * (yBot - yTop);

      ctx.strokeStyle = CSS.grid;
      ctx.lineWidth = 1;
      ctx.beginPath();
      ctx.moveTo(this.gutter, yBot + 0.5); ctx.lineTo(W - 8, yBot + 0.5);
      ctx.moveTo(this.gutter, yTop + 0.5); ctx.lineTo(W - 8, yTop + 0.5);
      ctx.stroke();
      if (vmin < 0 && vmax > 0) {
        ctx.strokeStyle = '#2c3040';
        ctx.beginPath();
        ctx.moveTo(this.gutter, Math.round(yOf(0)) + 0.5); ctx.lineTo(W - 8, Math.round(yOf(0)) + 0.5);
        ctx.stroke();
      }

      for (let k = 0; k < row.channels.length; k++) {
        const col = cols[k];
        if (!col) continue;
        ctx.strokeStyle = row.channels[k].color;
        ctx.lineWidth = 1;
        ctx.beginPath();
        let prev = null, used = 0;
        const marks = [];
        for (let i = 0; i < plotW; i++) {
          if (!isFinite(col.lo[i])) continue;
          const x = this.gutter + i + 0.5;
          const a = yOf(col.hi[i]), b = yOf(col.lo[i]), m = (a + b) / 2;
          if (prev !== null) { ctx.moveTo(prev.x, prev.m); ctx.lineTo(x, m); }
          ctx.moveTo(x, a); ctx.lineTo(x, b);
          marks.push([x, m]);
          prev = { x, m };
          used++;
        }
        ctx.stroke();
        // A few samples across the width (a photo folder, a zoomed-in stream)
        // are joined by straight lines; each sample gets a dot so it shows.
        if (used * 3 < plotW) {
          ctx.fillStyle = row.channels[k].color;
          for (const [x, y] of marks) ctx.fillRect(x - 1.5, y - 1.5, 3, 3);
        }
      }

      // row.fmt reads a value in the row's own terms, e.g. log2 seconds as 1/250 s.
      const fmt = (v) => (isFinite(v) ? (row.fmt || fmtNum)(v) : '—');
      ctx.fillStyle = CSS.text;
      ctx.textAlign = 'right';
      ctx.fillText(fmt(vmax), this.gutter - 6, yTop + 9);
      ctx.fillText(fmt(vmin), this.gutter - 6, yBot - 1);
      ctx.textAlign = 'left';
      const head = row.label + (row.unit ? `  [${row.unit}]` : '');
      const parts = [[head, CSS.dim]];
      for (const ch of row.channels)
        parts.push([this.hover ? `${ch.name} ${fmt(this.valueAt(ch, this.tOf(this.hover[0])))}` : ch.name, ch.color]);
      let lx = this.gutter + 5;
      const wTotal = parts.reduce((s2, [t]) => s2 + ctx.measureText(t).width + 11, 0);
      ctx.fillStyle = 'rgba(18,20,27,0.82)';
      ctx.fillRect(lx - 3, yTop + 2, wTotal, 13);
      for (const [txt, col] of parts) {
        ctx.fillStyle = col;
        ctx.fillText(txt, lx, yTop + 12);
        lx += ctx.measureText(txt).width + 11;
      }
    }

    // time axis
    const yA = this.rows.length * rowH;
    ctx.strokeStyle = CSS.grid;
    ctx.fillStyle = CSS.dim;
    ctx.textAlign = 'center';
    const span = this.t1 - this.t0;
    let step = niceStep(span / 8);
    if (this.xMode === 'index') step = Math.max(1, Math.round(step));
    ctx.beginPath();
    for (let t = Math.ceil(this.t0 / step) * step; t <= this.t1; t += step) {
      const x = Math.round(this.xOf(t)) + 0.5;
      ctx.moveTo(x, 0); ctx.lineTo(x, yA);
      ctx.fillText(this.xMode === 'index' ? String(Math.round(t) + 1) : fmtTime(t, step), x, yA + 13);
    }
    ctx.stroke();

    if (this.play >= this.t0 && this.play <= this.t1) {
      const x = Math.round(this.xOf(this.play)) + 0.5;
      ctx.strokeStyle = CSS.play;
      ctx.lineWidth = 1;
      ctx.beginPath(); ctx.moveTo(x, 0); ctx.lineTo(x, yA); ctx.stroke();
    }
    if (this.hover) {
      const x = Math.round(this.hover[0]) + 0.5;
      ctx.strokeStyle = '#ffffff33';
      ctx.beginPath(); ctx.moveTo(x, 0); ctx.lineTo(x, yA); ctx.stroke();
    }
    ctx.textAlign = 'left';
  }
}

function niceStep(x) {
  const e = Math.pow(10, Math.floor(Math.log10(Math.max(x, 1e-9))));
  const m = x / e;
  return (m < 1.5 ? 1 : m < 3.5 ? 2 : m < 7.5 ? 5 : 10) * e;
}
function fmtNum(v) {
  if (!isFinite(v)) return '—';
  const a = Math.abs(v);
  if (a >= 1000) return v.toFixed(0);
  if (a >= 10) return v.toFixed(1);
  if (a >= 1) return v.toFixed(2);
  return v.toFixed(3);
}
// Seconds while the window is short enough to read them, m:ss once it is not.
function fmtTime(t, step) {
  if (step < 10) return `${t.toFixed(step < 0.1 ? 2 : step < 1 ? 1 : 0)} s`;
  const s = Math.round(t), m = Math.floor(s / 60);
  return `${m}:${String(s % 60).padStart(2, '0')}`;
}

export { Channel };
