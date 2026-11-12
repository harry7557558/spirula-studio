// IMU/GPS viewer: drop captures, see what they carry.
//
// Parsing happens in scanworker.js (its own WASM module); this file owns the
// 3D viewport, the plots, and the options. Nothing leaves the browser.

import { Camera, Nav } from '../camera.js';
import { v3, quat } from '../linalg.js';
import { Scene, LineBuf } from './scene.js';
import { TileLayer, SOURCES } from './tiles.js';
import { Charts, Channel } from './charts.js';
import { Frame, makeClip, makePhotoClip, addPhoto, projectClip, indexAt } from './geo.js';

const $ = (id) => document.getElementById(id);
const dom = {
  canvas: $('glcanvas'), viewport: $('viewport'), hint: $('drop-hint'),
  legend: $('legend'), readout: $('readout'), credit: $('credit'),
  scale: $('scalebar'), scaleLabel: $('scale-label'), toast: $('toast'),
  colorbar: $('colorbar'), cbCanvas: $('cb-canvas'), cbLo: $('cb-lo'), cbHi: $('cb-hi'),
  list: $('file-list'), report: $('report'), rowToggles: $('row-toggles'),
  statusText: $('status-text'), statusDot: $('status-dot'),
  chart: $('chart-canvas'), charts: $('charts'), splitter: $('splitter'),
};

const opts = {
  colorBy: 'file', connect: true, points: true, stale: false, drops: false,
  circles: false, sigma: 5, lw: 2.5, altScale: 1,
  basemap: 'osm', customUrl: '', mapAlpha: 1, mapDesat: 0, ground: 'first',
  triad: true, accelVec: false, gravVec: false, trail: 0, triadSize: 0.12,
  grid: true, axes: true, follow: false, privacy: false, gyroDeg: true,
  rows: { gyro: true, gyroMag: false, accel: true, accelMag: false, gravity: false,
          att: true, speed: true, alt: true, dop: false,
          expo: true, shutter: false, iso: false, fnum: false },
};

const state = {
  clips: [], photoSets: new Map(), active: -1, frame: null, ranges: {}, scanned: 0, found: 0,
  play: 0, playing: false, rate: 1, lastFrame: 0, needRender: true, userMoved: false,
};

const camera = new Camera();
const nav = new Nav(camera);
let scene, tiles, charts;
try {
  scene = new Scene(dom.canvas);
} catch (e) {
  dom.hint.innerHTML = `<div class="big">${e.message}</div>`;
}
if (scene) {
  tiles = new TileLayer(scene.gl);
  charts = new Charts(dom.chart);
  charts.onPlay = (t) => { state.play = t; state.playing = false; $('btn-play').textContent = '▶'; refreshPlay(); };
  charts.onHover = () => { updateReadout(); };
}

const AX_COLORS = { x: [0.88, 0.33, 0.33], y: [0.31, 0.80, 0.44], z: [0.31, 0.55, 0.97] };

// ---------------------------------------------------------------------------
// Scanning
// ---------------------------------------------------------------------------
const POOL = Math.max(1, Math.min(3, (navigator.hardwareConcurrency || 4) - 1));
const workers = [];
let nextId = 1;
let scanAbort = false;

function makeWorker() {
  const w = new Worker(new URL('./scanworker.js', import.meta.url), { type: 'module' });
  w._waiting = new Map();
  w.onmessage = (ev) => {
    const r = ev.data;
    if (r.type === 'fatal') {
      console.error('scan worker:', r.reason);
      toast(`Scanner failed to start: ${r.reason}`);
      for (const [k, fn] of w._waiting) { w._waiting.delete(k); fn({ type: 'empty', reason: r.reason }); }
      return;
    }
    const done = w._waiting.get(r.id);
    if (done) { w._waiting.delete(r.id); done(r); }
  };
  w.onerror = (e) => {
    console.error('scan worker error', e.message);
    toast(`Scanner error: ${e.message}`);
    for (const [k, fn] of w._waiting) { w._waiting.delete(k); fn({ type: 'empty', reason: e.message }); }
  };
  return w;
}

function ask(w, path, file) {
  const id = nextId++;
  return new Promise((resolve) => {
    w._waiting.set(id, resolve);
    w.postMessage({ type: 'scan', id, path, file });
  });
}

// Recursively collect dropped files. Only directory listings happen here;
// entry.file() is deferred so a folder of thousands of images is not stat'ed
// until its turn to be scanned.
async function gatherEntries(dt) {
  const roots = [];
  for (const it of dt.items ? Array.from(dt.items) : []) {
    if (it.kind === 'file' && it.webkitGetAsEntry) { const en = it.webkitGetAsEntry(); if (en) roots.push(en); }
  }
  if (!roots.length)
    return Array.from(dt.files || []).map((f) => ({ path: f.webkitRelativePath || f.name, getFile: async () => f }));
  const out = [];
  const walk = (entry, prefix) => new Promise((resolve) => {
    if (entry.isFile) {
      out.push({ path: prefix + entry.name, getFile: () => new Promise((res, rej) => entry.file(res, rej)) });
      if (out.length % 500 === 0) setStatus(`Listing… ${out.length.toLocaleString()} files`);
      resolve();
    } else if (entry.isDirectory) {
      const reader = entry.createReader();
      const batch = () => reader.readEntries(async (ents) => {
        if (!ents.length) { resolve(); return; }
        await Promise.all(ents.map((en) => walk(en, prefix + entry.name + '/')));
        batch();
      }, () => resolve());
      batch();
    } else resolve();
  });
  setStatus('Listing folder…');
  await Promise.all(roots.map((r) => walk(r, '')));
  return out;
}

async function scanEntries(entries) {
  entries = entries.filter((e) => !/(^|\/)\./.test(e.path));
  if (!entries.length) return;
  scanAbort = false;
  while (workers.length < POOL) workers.push(makeWorker());
  let i = 0, done = 0;
  const total = entries.length;
  const lane = async (w) => {
    while (i < total && !scanAbort) {
      const e = entries[i++];
      let file = null;
      try { file = await e.getFile(); } catch (err) { file = null; }
      done++;
      if (file && file.size > 64) {
        const r = await ask(w, e.path, file);
        state.scanned++;
        if (r.type === 'result') addClip(r);
      } else state.scanned++;
      if (done % 5 === 0 || done === total) {
        setStatus(`Scanning ${done.toLocaleString()} / ${total.toLocaleString()} — ${state.found} with telemetry`);
        updateCounts();
      }
    }
  };
  setStatus(`Scanning ${total.toLocaleString()} file${total === 1 ? '' : 's'}…`);
  await Promise.all(workers.slice(0, POOL).map(lane));
  setStatus(state.found ? `${state.found} file${state.found === 1 ? '' : 's'} with telemetry` : 'No telemetry found', state.found ? 'ok' : 'err');
  updateCounts();
  if (!state.userMoved) fitAll();
}

function addClip(msg) {
  if (msg.kind === 'image') { addPhotoResult(msg); return; }
  const clip = makeClip(msg, state.clips.length);
  if (!clip.gps && !clip.gyro && !clip.accel && !clip.att && !clip.expo) return;
  state.found++;
  registerClip(clip);
}

function addPhotoResult(msg) {
  const folder = msg.path.slice(0, msg.path.lastIndexOf('/') + 1);
  let clip = state.photoSets.get(folder);
  const fresh = !clip;
  if (fresh) { clip = makePhotoClip(folder, state.clips.length); state.photoSets.set(folder, clip); }
  addPhoto(clip, msg);
  state.found++;
  if (fresh) { registerClip(clip); return; }
  clip._ch = null;
  ensureFrame(clip);
  if (state.frame) projectClip(clip, state.frame);
  updateRanges();
  rebuildClip(clip);
  renderList();
  if (state.clips[state.active] === clip) { refreshCharts(); updateLegend(); updateReadout(); }
  state.needRender = true;
}

function registerClip(clip) {
  state.clips.push(clip);
  clip.gpu = { track: new LineBuf(scene.gl), drops: new LineBuf(scene.gl), circles: new LineBuf(scene.gl),
               trail: [new LineBuf(scene.gl), new LineBuf(scene.gl), new LineBuf(scene.gl)] };
  if (!ensureFrame(clip) && clip.gps && state.frame) projectClip(clip, state.frame);
  updateRanges();
  rebuildAll();
  if (state.active < 0) selectClip(state.clips.length - 1);
  renderList();
  dom.hint.style.display = 'none';
  state.needRender = true;
}

// The scene's origin, from the first clip with a fix; a photo folder may get
// its first one late. True when this call set it.
function ensureFrame(clip) {
  if (state.frame || !clip.gps || !clip.gps.n) return false;
  let k = 0;
  while (k < clip.gps.n - 1 && !clip.gps.fix[k]) k++;
  state.frame = new Frame(clip.gps.lat[k], clip.gps.lon[k], clip.gps.hasAlt[k] ? clip.gps.alt[k] : 0);
  for (const c of state.clips) projectClip(c, state.frame);
  return true;
}

// ---------------------------------------------------------------------------
// Derived geometry
// ---------------------------------------------------------------------------
function groundZ() {
  if (!state.frame) return 0;
  if (opts.ground === 'sea') return -state.frame.alt0 * opts.altScale;
  if (opts.ground === 'min') {
    let m = Infinity;
    for (const c of state.clips) if (c.local) for (let i = 2; i < c.local.length; i += 3) m = Math.min(m, c.local[i]);
    return isFinite(m) ? m * opts.altScale : 0;
  }
  return 0;
}

function updateRanges() {
  const r = { speed: 0, altMin: Infinity, altMax: -Infinity, dop: 0, gyro: 0, accel: 0, evMin: 0, evMax: 0 };
  for (const c of state.clips) {
    if (c.gps) for (let i = 0; i < c.gps.n; i++) {
      const s = c.gps.speed[i] >= 0 ? c.gps.speed[i] : c.gps.dspeed[i];
      if (isFinite(s)) r.speed = Math.max(r.speed, s);
      r.dop = Math.max(r.dop, c.gps.dop[i]);
      if (c.gps.hasAlt[i]) { r.altMin = Math.min(r.altMin, c.gps.alt[i]); r.altMax = Math.max(r.altMax, c.gps.alt[i]); }
    }
    if (c.gyro) for (let i = 0; i < c.gyro.n; i += 17) r.gyro = Math.max(r.gyro, c.gyro.mag[i]);
    if (c.accel) for (let i = 0; i < c.accel.n; i += 17) r.accel = Math.max(r.accel, c.accel.mag[i]);
    const ev = expoLine(c);
    if (ev) for (const v of ev) if (v === v) { r.evMin = Math.min(r.evMin, v); r.evMax = Math.max(r.evMax, v); }
  }
  if (!isFinite(r.altMin)) { r.altMin = 0; r.altMax = 1; }
  state.ranges = r;
}

function colorScalar(clip, i) {
  const g = clip.gps, r = state.ranges;
  switch (opts.colorBy) {
    case 'time': return clip.duration > 0 ? g.t[i] / clip.duration : 0;
    case 'speed': {
      const s = g.speed[i] >= 0 ? g.speed[i] : g.dspeed[i];
      return r.speed > 0 ? s / r.speed : 0;
    }
    case 'alt': return r.altMax > r.altMin ? (g.alt[i] - r.altMin) / (r.altMax - r.altMin) : 0.5;
    case 'dop': return r.dop > 0 ? g.dop[i] / r.dop : 0;
    case 'gyro': {
      if (!clip.gyro || !r.gyro) return 0;
      return clip.gyro.mag[indexAt(g.t[i], clip.gyro.t, clip.gyro.n)] / r.gyro;
    }
    case 'accel': {
      if (!clip.accel || !r.accel) return 0;
      return clip.accel.mag[indexAt(g.t[i], clip.accel.t, clip.accel.n)] / r.accel;
    }
    case 'expo': {
      const j = expoIndex(clip, g.t[i]), ev = expoLine(clip);
      if (j < 0 || !(r.evMax > r.evMin) || !(ev[j] === ev[j])) return 0;
      return (ev[j] - r.evMin) / (r.evMax - r.evMin);
    }
    default: return 0;
  }
}

// The summed exposure, or the one factor there is when no factor is shared.
function expoLine(clip) {
  const e = clip.expo;
  if (!e) return null;
  return e.total || e.stops[[0, 1, 2].find((k) => e.any >> k & 1)];
}

// The exposure record at `t`: the frame's, or exactly that photo's; -1 if none.
function expoIndex(clip, t) {
  const e = clip.expo;
  if (!e || !e.n) return -1;
  const j = indexAt(t, e.t, e.n);
  return clip.kind === 'photos' && e.t[j] !== Math.round(t) ? -1 : j;
}

const CIRCLE_SEGS = 24;
const MAX_CIRCLES = 4000;

function rebuildClip(clip) {
  const g = clip.gps;
  if (!g || !g.n || !clip.local || !clip.gpu) return;
  const sel = [];
  for (let i = 0; i < g.n; i++) if (opts.stale || g.fresh[i] || i === 0) sel.push(i);
  clip.sel = sel;
  const m = sel.length;
  const pos = new Float32Array(m * 3), val = new Float32Array(m);
  for (let k = 0; k < m; k++) {
    const i = sel[k];
    pos[k * 3] = clip.local[i * 3];
    pos[k * 3 + 1] = clip.local[i * 3 + 1];
    pos[k * 3 + 2] = clip.local[i * 3 + 2] * opts.altScale;
    val[k] = colorScalar(clip, i);
  }
  clip.pos = pos;
  clip.gpu.track.setStrip(pos, val);

  const gz = groundZ();
  if (opts.drops && m) {
    const d = new Float32Array(m * 6), dv = new Float32Array(m * 2);
    for (let k = 0; k < m; k++) {
      d[k * 6] = pos[k * 3]; d[k * 6 + 1] = pos[k * 3 + 1]; d[k * 6 + 2] = pos[k * 3 + 2];
      d[k * 6 + 3] = pos[k * 3]; d[k * 6 + 4] = pos[k * 3 + 1]; d[k * 6 + 5] = gz;
      dv[k * 2] = val[k]; dv[k * 2 + 1] = val[k];
    }
    clip.gpu.drops.setSegments(d, dv);
  } else clip.gpu.drops.n = 0;

  if (opts.circles && m) {
    const stride = Math.max(1, Math.ceil(m / MAX_CIRCLES));
    const count = Math.ceil(m / stride);
    const c = new Float32Array(count * CIRCLE_SEGS * 6), cv = new Float32Array(count * CIRCLE_SEGS * 2);
    let o = 0, ov = 0;
    for (let k = 0; k < m; k += stride) {
      const i = sel[k];
      const rad = (g.dop[i] > 0 ? g.dop[i] : 1) * opts.sigma;
      const x = pos[k * 3], y = pos[k * 3 + 1], z = pos[k * 3 + 2];
      for (let s = 0; s < CIRCLE_SEGS; s++) {
        const a0 = s / CIRCLE_SEGS * 2 * Math.PI, a1 = (s + 1) / CIRCLE_SEGS * 2 * Math.PI;
        c[o++] = x + rad * Math.cos(a0); c[o++] = y + rad * Math.sin(a0); c[o++] = z;
        c[o++] = x + rad * Math.cos(a1); c[o++] = y + rad * Math.sin(a1); c[o++] = z;
        cv[ov++] = val[k]; cv[ov++] = val[k];
      }
    }
    clip.gpu.circles.setSegments(c, cv);
  } else clip.gpu.circles.n = 0;

  buildTrail(clip);
}

// Sensor axes drawn along the track every `trail` seconds: where the camera
// was pointing, in the same picture as where it was.
function buildTrail(clip) {
  const bufs = clip.gpu.trail;
  if (!opts.trail || !clip.att || !clip.pos || !clip.sel) { for (const b of bufs) b.n = 0; return; }
  const pts = [];
  let last = -1e9;
  for (let k = 0; k < clip.sel.length; k++) {
    const t = clip.gps.t[clip.sel[k]];
    if (t - last < opts.trail) continue;
    last = t;
    pts.push([k, t]);
  }
  const len = opts.triadSize * nav._sceneScale * 0.4;
  const arr = [[], [], []];
  for (const [k, t] of pts) {
    const q = attitudeAt(clip, t);
    if (!q) continue;
    const p = [clip.pos[k * 3], clip.pos[k * 3 + 1], clip.pos[k * 3 + 2]];
    for (let a = 0; a < 3; a++) {
      const e = [0, 0, 0]; e[a] = len;
      const w = quat.rotVec(q, e);
      arr[a].push(p[0], p[1], p[2], p[0] + w[0], p[1] + w[1], p[2] + w[2]);
    }
  }
  for (let a = 0; a < 3; a++) bufs[a].setSegments(new Float32Array(arr[a]), new Float32Array(arr[a].length / 3));
}

function rebuildAll() { for (const c of state.clips) rebuildClip(c); state.needRender = true; }

// The attitude the camera wrote, as [x,y,z,w] taking sensor axes into the
// world. The reader reports which sense makes gravity constant; the other
// sense is its conjugate.
function attitudeAt(clip, t) {
  const a = clip.att;
  if (!a || !a.n) return null;
  const i = indexAt(t, a.t, a.n);
  const q = [a.x[i], a.y[i], a.z[i], a.w[i]];
  const s2w = clip.meta.check && clip.meta.check.attitudeSensorToWorld;
  return s2w ? q : [-q[0], -q[1], -q[2], q[3]];
}

function overlayScale() { return opts.triadSize * Math.max(1e-3, camera.dist()); }

function playPos(clip) {
  if (!clip || !clip.pos || !clip.sel || !clip.sel.length) return [0, 0, 0];
  const k = nearestSel(clip, state.play);
  return [clip.pos[k * 3], clip.pos[k * 3 + 1], clip.pos[k * 3 + 2]];
}
function nearestSel(clip, t) {
  let lo = 0, hi = clip.sel.length - 1;
  while (lo + 1 < hi) {
    const mid = (lo + hi) >> 1;
    if (clip.gps.t[clip.sel[mid]] <= t) lo = mid; else hi = mid;
  }
  return lo;
}

// ---------------------------------------------------------------------------
// Rendering
// ---------------------------------------------------------------------------
const seg = { buf: null };
function drawSegments(arr, mvp, color, width, alpha) {
  if (!arr.length) return;
  if (!seg.buf) seg.buf = new LineBuf(scene.gl);
  seg.buf.setSegments(new Float32Array(arr), new Float32Array(arr.length / 3));
  scene.drawLine(seg.buf, mvp, color, width, 0, alpha === undefined ? 1 : alpha);
}

function render() {
  if (!scene) return;
  scene.resize();
  const m = scene.matrices(camera);
  scene.begin([0.04, 0.045, 0.055]);
  const gz = groundZ();

  if (state.frame && !opts.privacy && opts.basemap !== 'none') {
    tiles.update(state.frame, scene.groundBounds(camera, gz), scene.metresPerPixel(camera));
    scene.drawTiles(tiles, m.mvp, gz, opts.mapAlpha, opts.mapDesat);
  } else tiles.draws.length = 0;

  if (opts.grid) {
    scene.updateGrid(camera.target, m.dist, gz, 0);
    scene.drawLine(scene.grid, m.mvp, [0.55, 0.60, 0.72], 1, 0, tiles.draws.length ? 0.22 : 0.5);
  }
  if (opts.axes) {
    const L = Math.max(m.dist * 0.25, 1);
    drawSegments([0, 0, gz, L, 0, gz], m.mvp, AX_COLORS.x, 2);
    drawSegments([0, 0, gz, 0, L, gz], m.mvp, AX_COLORS.y, 2);
    drawSegments([0, 0, gz, 0, 0, gz + L], m.mvp, AX_COLORS.z, 2);
  }

  const mode = opts.colorBy === 'file' ? 0 : 1;
  for (const c of state.clips) {
    if (!c.visible || !c.gpu) continue;
    const col = c.color;
    if (c.gpu.drops.n) scene.drawLine(c.gpu.drops, m.mvp, col, 1, mode, 0.35);
    if (c.gpu.circles.n) scene.drawLine(c.gpu.circles, m.mvp, col, 1, mode, 0.4);
    if (opts.connect) scene.drawLine(c.gpu.track, m.mvp, col, opts.lw, mode, 1);
    if (opts.points) scene.drawPoints(c.gpu.track, m.mvp, col, opts.lw + 2.5, mode, 1);
    for (let a = 0; a < 3; a++)
      if (c.gpu.trail[a].n) scene.drawLine(c.gpu.trail[a], m.mvp, [AX_COLORS.x, AX_COLORS.y, AX_COLORS.z][a], 1.5, 0, 0.75);
  }

  const clip = state.clips[state.active];
  if (clip) drawOverlayVectors(clip, m.mvp, gz);
  updateScaleBar();
}

function drawOverlayVectors(clip, mvp, gz) {
  const p = clip.gps && clip.pos ? playPos(clip) : [0, 0, gz];
  const len = overlayScale();
  if (clip.gps && clip.pos && clip.sel && clip.sel.length) {
    drawSegments([p[0], p[1], p[2] - len * 0.25, p[0], p[1], p[2] + len * 0.25], mvp, [0.95, 0.35, 0.35], 2);
  }
  const q = attitudeAt(clip, state.play);
  if (opts.triad && q) {
    const axes = [AX_COLORS.x, AX_COLORS.y, AX_COLORS.z];
    for (let a = 0; a < 3; a++) {
      const e = [0, 0, 0]; e[a] = len;
      const w = quat.rotVec(q, e);
      drawSegments([p[0], p[1], p[2], p[0] + w[0], p[1] + w[1], p[2] + w[2]], mvp, axes[a], 3);
    }
  }
  const drawVec = (src, scale, color) => {
    if (!src || !src.n) return;
    const i = indexAt(state.play, src.t, src.n);
    let w = [src.x[i], src.y[i], src.z[i]];
    if (q) w = quat.rotVec(q, w);
    const s = len * scale;
    drawSegments([p[0], p[1], p[2], p[0] + w[0] * s, p[1] + w[1] * s, p[2] + w[2] * s], mvp, color, 3);
  };
  if (opts.accelVec) drawVec(clip.accel, 1 / 9.80665, [0.95, 0.75, 0.3]);
  if (opts.gravVec) drawVec(clip.gravity, 1, [0.6, 0.85, 0.95]);
}

function loop(now) {
  const dt = state.lastFrame ? Math.min(0.1, (now - state.lastFrame) / 1000) : 0;
  state.lastFrame = now;
  nav.tick(dt);
  nav.gamepadTick(dt);
  if (state.playing) {
    const clip = state.clips[state.active];
    const dur = clip ? clipSpan(clip) : 0;
    state.play += dt * state.rate;
    if (state.play > dur) state.play = 0;
    refreshPlay(true);
  }
  if (opts.follow) {
    const clip = state.clips[state.active];
    if (clip && clip.pos && clip.sel && clip.sel.length) {
      const p = playPos(clip);
      const d = v3.sub(p, camera.target);
      camera.target = p;
      camera.pos = v3.add(camera.pos, d);
    }
  }
  render();
  requestAnimationFrame(loop);
}

// ---------------------------------------------------------------------------
// Plots
// ---------------------------------------------------------------------------
function chan(clip, key, name, color, t, v, scale) {
  clip._ch = clip._ch || {};
  const k = `${key}|${scale}`;
  if (!clip._ch[k]) clip._ch[k] = new Channel(name, color, t, v, scale);
  return clip._ch[k];
}

const ROWS = [
  { key: 'gyro', label: 'Gyro' }, { key: 'gyroMag', label: 'Gyro magnitude' },
  { key: 'accel', label: 'Accel' }, { key: 'accelMag', label: 'Accel magnitude' },
  { key: 'gravity', label: 'Gravity' }, { key: 'att', label: 'Attitude' },
  { key: 'speed', label: 'GPS speed' }, { key: 'alt', label: 'GPS altitude' },
  { key: 'dop', label: 'GPS DOP / fix' },
  { key: 'expo', label: 'Exposure (EV)' }, { key: 'shutter', label: 'Shutter speed' },
  { key: 'iso', label: 'ISO' }, { key: 'fnum', label: 'Aperture' },
];

function clipSpan(clip) {
  let d = clip.duration || 0;
  for (const s of [clip.gyro, clip.accel, clip.att, clip.gps, clip.expo]) if (s && s.n) d = Math.max(d, s.t[s.n - 1]);
  return d || 1;
}

const fmtShutter = (s) => (s >= 0.3 ? `${s.toFixed(s < 10 ? 1 : 0)} s` : `1/${Math.round(1 / s)} s`);
const fmtEv = (v) => (Math.abs(v) < 0.05 ? '0.0 EV' : `${v > 0 ? '+' : ''}${v.toFixed(1)} EV`);
// The factor rows plot log2 of the value, so a stop is one height anywhere.
const FACTORS = [
  { key: 'shutter', name: 'shutter', label: 'Shutter speed', color: '#e05555', fmt: (v) => fmtShutter(2 ** v) },
  { key: 'iso', name: 'ISO', label: 'ISO', color: '#4ecb71', fmt: (v) => `ISO ${Math.round(2 ** v)}` },
  { key: 'fnum', name: 'aperture', label: 'Aperture', color: '#4f8ef7', fmt: (v) => `f/${(2 ** v).toFixed(1)}` },
];

function exposureRows(clip, rows) {
  const e = clip.expo;
  if (!e) return;
  if (opts.rows.expo && e.common) {
    const shared = [0, 1, 2].filter((k) => e.common >> k & 1);
    const ch = [];
    if (shared.length === 1) {
      const f = FACTORS[shared[0]];
      ch.push(chan(clip, 'ev', f.name, f.color, e.t, e.total, 1));
    } else {
      ch.push(chan(clip, 'ev', 'total', '#e8eaf2', e.t, e.total, 1));
      for (const k of shared)
        if (e.varies[k]) ch.push(chan(clip, `ev${k}`, FACTORS[k].name, FACTORS[k].color, e.t, e.stops[k], 1));
    }
    rows.push({ label: 'Exposure', unit: 'EV, 0 = mean', sym: true, fmt: fmtEv, channels: ch });
  }
  FACTORS.forEach((f, k) => {
    if (opts.rows[f.key] && e.any >> k & 1)
      rows.push({ label: f.label, unit: 'log scale', fmt: f.fmt,
                  channels: [chan(clip, `lg${k}`, f.name, f.color, e.t, e.log2[k], 1)] });
  });
}

function exposureText(clip, t) {
  const j = expoIndex(clip, t);
  if (j < 0) return '';
  const e = clip.expo, out = [];
  if (e.shutter[j] > 0) out.push(fmtShutter(e.shutter[j]));
  if (e.iso[j] > 0) out.push(`ISO ${Math.round(e.iso[j])}`);
  if (e.fnum[j] > 0) out.push(`f/${e.fnum[j].toFixed(1)}`);
  const ev = expoLine(clip);
  if (ev && ev[j] === ev[j]) out.push(fmtEv(ev[j]));
  return out.join(' · ');
}

function buildRows(clip) {
  const rows = [];
  if (!clip) return rows;
  const k = opts.gyroDeg ? 180 / Math.PI : 1;
  const R = '#e05555', G = '#4ecb71', B = '#4f8ef7', Y = '#e0b055';
  const g = clip.gyro, a = clip.accel, gr = clip.gravity, at = clip.att, gp = clip.gps;
  if (opts.rows.gyro && g) rows.push({ label: 'Gyro', unit: opts.gyroDeg ? 'deg/s' : 'rad/s', sym: true, channels: [
    chan(clip, 'gx', 'x', R, g.t, g.x, k), chan(clip, 'gy', 'y', G, g.t, g.y, k), chan(clip, 'gz', 'z', B, g.t, g.z, k)] });
  if (opts.rows.gyroMag && g) rows.push({ label: '|Gyro|', unit: opts.gyroDeg ? 'deg/s' : 'rad/s', channels: [
    chan(clip, 'gm', '|w|', Y, g.t, g.mag, k)] });
  if (opts.rows.accel && a) rows.push({ label: 'Accel', unit: 'm/s²', sym: true, channels: [
    chan(clip, 'ax', 'x', R, a.t, a.x, 1), chan(clip, 'ay', 'y', G, a.t, a.y, 1), chan(clip, 'az', 'z', B, a.t, a.z, 1)] });
  if (opts.rows.accelMag && a) rows.push({ label: '|Accel|', unit: 'm/s²', channels: [
    chan(clip, 'am', '|a|', Y, a.t, a.mag, 1)] });
  if (opts.rows.gravity && gr) rows.push({ label: 'Gravity', unit: 'unit', sym: true, channels: [
    chan(clip, 'vx', 'x', R, gr.t, gr.x, 1), chan(clip, 'vy', 'y', G, gr.t, gr.y, 1), chan(clip, 'vz', 'z', B, gr.t, gr.z, 1)] });
  if (opts.rows.att && at) rows.push({ label: 'Attitude', unit: 'deg', channels: [
    chan(clip, 'yaw', 'yaw', R, at.t, at.yawU, 1), chan(clip, 'pit', 'pitch', G, at.t, at.pitch, 1),
    chan(clip, 'rol', 'roll', B, at.t, at.rollU, 1)] });
  if (opts.rows.speed && gp && clip.kind !== 'photos') {
    const ch = [chan(clip, 'sd', 'from fixes', B, gp.t, gp.dspeed, 1)];
    let any = false;
    for (let i = 0; i < gp.n; i++) if (gp.speed[i] >= 0) { any = true; break; }
    if (any) ch.push(chan(clip, 'sr', 'reported', Y, gp.t, gp.speed, 1));
    rows.push({ label: 'Speed', unit: 'm/s', channels: ch });
  }
  if (opts.rows.alt && gp) rows.push({ label: opts.privacy ? 'Altitude (relative)' : 'Altitude', unit: 'm', channels: [
    chan(clip, opts.privacy ? 'alr' : 'al', 'alt', G, gp.t, opts.privacy ? relAlt(gp) : gp.alt, 1)] });
  if (opts.rows.dop && gp) rows.push({ label: 'DOP / fix', unit: '', channels: [
    chan(clip, 'dp', 'dop', Y, gp.t, gp.dop, 1), chan(clip, 'fx', 'fix', G, gp.t, Float32Array.from(gp.fix), 1)] });
  exposureRows(clip, rows);
  return rows;
}

function relAlt(gp) {
  if (!gp._rel) {
    const a = new Float32Array(gp.n), b = gp.alt[0];
    for (let i = 0; i < gp.n; i++) a[i] = gp.alt[i] - b;
    gp._rel = a;
  }
  return gp._rel;
}

function refreshCharts() {
  const clip = state.clips[state.active];
  charts.xMode = clip && clip.kind === 'photos' ? 'index' : 'time';
  charts.setRows(buildRows(clip), 0, clip ? clipSpan(clip) : 1);
  charts.setPlay(state.play);
  charts.draw();
}
// Scrubbing redraws the plots; playback only needs them at eye rate, and a
// full redraw walks every visible channel's pyramid.
let lastPlayDraw = 0;
function refreshPlay(throttle) {
  const now = performance.now();
  if (!throttle || now - lastPlayDraw > 40) {
    lastPlayDraw = now;
    charts.setPlay(state.play);
    charts.draw();
  }
  $('play-time').textContent = fmtClock(state.play);
  updateReadout();
}

// ---------------------------------------------------------------------------
// UI
// ---------------------------------------------------------------------------
function setStatus(text, cls) {
  dom.statusText.textContent = text;
  dom.statusDot.className = cls || '';
}
function updateCounts() {
  $('st-scanned').textContent = state.scanned.toLocaleString();
  $('st-found').textContent = String(state.found);
}
function clipLabel(c, i) { return opts.privacy ? `Clip ${i + 1}` : c.name; }

function renderList() {
  dom.list.innerHTML = '';
  state.clips.forEach((c, i) => {
    const el = document.createElement('div');
    el.className = 'file-item' + (i === state.active ? ' active' : '');
    const cb = document.createElement('input');
    cb.type = 'checkbox'; cb.checked = c.visible;
    cb.onclick = (e) => { e.stopPropagation(); c.visible = cb.checked; state.needRender = true; };
    const sw = document.createElement('div');
    sw.className = 'swatch';
    sw.style.background = `rgb(${c.color.map((x) => Math.round(x * 255)).join(',')})`;
    const nm = document.createElement('div');
    nm.className = 'fname'; nm.textContent = clipLabel(c, i); nm.title = opts.privacy ? '' : c.path;
    el.append(cb, sw, nm);
    const ck = c.meta.check || {};
    for (const [txt, ok] of [['IMU', ck.imuUsable], ['GPS', ck.gpsUsable]]) {
      if (txt === 'GPS' && !(c.gps && c.gps.n)) continue;
      if (txt === 'IMU' && !(c.gyro || c.accel || c.att)) continue;
      const t = document.createElement('span');
      t.className = 'tag ' + (ok ? 'ok' : 'bad');
      t.textContent = txt;
      t.title = ok ? 'readings look like a working sensor' : 'present, but the checks are not satisfied';
      el.append(t);
    }
    if (c.expo) {
      const t = document.createElement('span');
      t.className = 'tag';
      t.textContent = 'EXP';
      t.title = 'records its exposure: shutter, ISO or aperture';
      el.append(t);
    }
    el.onclick = () => selectClip(i);
    dom.list.append(el);
  });
}

function selectClip(i) {
  state.active = i;
  state.play = 0;
  const c = state.clips[i];
  fitClip(c);
  dom.report.textContent = c ? (opts.privacy ? scrubReport(c.report) : c.report) : '—';
  renderList();
  refreshCharts();
  updateLegend();
  updateReadout();
  state.needRender = true;
}

// Distances and rates say nothing about where; the fix line and any wall
// clock do, so only those go.
function scrubReport(text) {
  return text.split('\n').filter((l) => !/first fix|unix|utc/i.test(l)).join('\n');
}

function updateLegend() {
  const c = state.clips[state.active];
  if (!c) { dom.legend.innerHTML = ''; dom.report.textContent = '—'; return; }
  const ck = c.meta.check || {};
  const bits = [`<b>${clipLabel(c, state.active)}</b>`];
  bits.push(`${c.meta.carrier}${c.meta.camera ? ` · ${c.meta.camera}` : ''}`);
  const s = [];
  if (c.gyro) s.push(`gyro ${fmtHz(ck.gyro && ck.gyro.rate)}`);
  if (c.accel) s.push(`accel ${fmtHz(ck.accel && ck.accel.rate)}`);
  if (c.att) s.push(`attitude ${fmtHz(ck.orientation && ck.orientation.rate)}`);
  if (c.gps) s.push(`gps ${fmtHz(effectiveHz(ck.gps))}`);
  if (c.expo) s.push(c.kind === 'photos' ? `exposure ${c.expo.n} photos` : `exposure ${fmtHz(c.expo.rate)}`);
  if (s.length) bits.push(s.join(' · '));
  const w = (ck.warnings || []).slice(0, 4);
  for (const x of w) bits.push(`<span class="warn">⚠ ${escapeHtml(x)}</span>`);
  dom.legend.innerHTML = bits.join('<br>');
  dom.credit.textContent = (state.frame && !opts.privacy && opts.basemap !== 'none') ? tiles.credit() : '';
  updateColorbar();
}

function updateColorbar() {
  if (opts.colorBy === 'file') { dom.colorbar.style.display = 'none'; return; }
  dom.colorbar.style.display = 'block';
  const ctx = dom.cbCanvas.getContext('2d');
  const w = dom.cbCanvas.width;
  for (let x = 0; x < w; x++) {
    ctx.fillStyle = turboCss(x / (w - 1));
    ctx.fillRect(x, 0, 1, dom.cbCanvas.height);
  }
  const r = state.ranges, c = state.clips[state.active];
  const spec = {
    time: ['0 s', `${(c ? clipSpan(c) : 0).toFixed(0)} s`],
    speed: ['0', `${r.speed.toFixed(1)} m/s`],
    alt: opts.privacy ? ['low', 'high'] : [`${r.altMin.toFixed(0)} m`, `${r.altMax.toFixed(0)} m`],
    dop: ['0', r.dop.toFixed(1)],
    gyro: ['0', `${(r.gyro * (opts.gyroDeg ? 180 / Math.PI : 1)).toFixed(0)} ${opts.gyroDeg ? 'deg/s' : 'rad/s'}`],
    accel: ['0', `${r.accel.toFixed(1)} m/s²`],
    expo: [fmtEv(r.evMin), fmtEv(r.evMax)],
  }[opts.colorBy] || ['', ''];
  dom.cbLo.textContent = spec[0];
  dom.cbHi.textContent = spec[1];
}

function updateReadout() {
  const c = state.clips[state.active];
  if (!c) { dom.readout.textContent = ''; return; }
  const t = state.play;
  const out = [c.kind === 'photos' ? `photo ${Math.round(t) + 1} / ${c.photos.length}` : `t = ${fmtClock(t)}`];
  const gi = c.gps && c.gps.n ? indexAt(t, c.gps.t, c.gps.n) : -1;
  if (gi >= 0 && (c.kind !== 'photos' || c.gps.t[gi] === Math.round(t))) {
    const i = gi;
    if (opts.privacy) {
      if (c.local) out.push(`local ${(c.local[i * 3]).toFixed(1)} E, ${(c.local[i * 3 + 1]).toFixed(1)} N m`);
      out.push(`alt ${(c.gps.alt[i] - c.gps.alt[0]).toFixed(1)} m rel`);
    } else {
      out.push(`${c.gps.lat[i].toFixed(6)}, ${c.gps.lon[i].toFixed(6)}`);
      if (c.gps.hasAlt[i]) out.push(`alt ${c.gps.alt[i].toFixed(1)} m`);
      if (c.gps.unix[i] > 0) out.push(new Date(c.gps.unix[i] * 1000).toISOString().replace('T', ' ').slice(0, 19) + ' UTC');
    }
    if (c.kind !== 'photos') {
      const sp = c.gps.speed[i] >= 0 ? c.gps.speed[i] : c.gps.dspeed[i];
      out.push(`speed ${sp.toFixed(2)} m/s${c.gps.dop[i] > 0 ? ` · dop ${c.gps.dop[i].toFixed(1)}` : ''}`);
    }
  }
  if (c.gyro) { const i = indexAt(t, c.gyro.t, c.gyro.n);
    out.push(`|ω| ${(c.gyro.mag[i] * (opts.gyroDeg ? 180 / Math.PI : 1)).toFixed(1)} ${opts.gyroDeg ? 'deg/s' : 'rad/s'}`); }
  if (c.accel) { const i = indexAt(t, c.accel.t, c.accel.n); out.push(`|a| ${c.accel.mag[i].toFixed(2)} m/s²`); }
  if (c.att) { const i = indexAt(t, c.att.t, c.att.n);
    out.push(`yaw ${c.att.yaw[i].toFixed(0)}° pitch ${c.att.pitch[i].toFixed(0)}° roll ${c.att.roll[i].toFixed(0)}°`); }
  const ex = exposureText(c, t);
  if (ex) out.push(ex);
  if (c.kind === 'photos' && !opts.privacy && c.photos[Math.round(t)]) out.push(c.photos[Math.round(t)].name);
  dom.readout.innerHTML = out.map(escapeHtml).join('<br>');
}

function updateScaleBar() {
  const mpp = scene.metresPerPixel(camera) * (Math.min(window.devicePixelRatio || 1, 2));
  let len = mpp * 80;
  const e = Math.pow(10, Math.floor(Math.log10(Math.max(len, 1e-6))));
  const m = len / e;
  len = (m < 1.5 ? 1 : m < 3.5 ? 2 : m < 7.5 ? 5 : 10) * e;
  const px = len / mpp;
  dom.scale.querySelector('.bar').style.width = `${px.toFixed(0)}px`;
  dom.scaleLabel.textContent = len >= 1000 ? `${(len / 1000).toFixed(len >= 10000 ? 0 : 1)} km` : `${len} m`;
}

// A steeper default than Nav.fitSphere's 22 deg: a track seen edge-on reads
// as a line, and the map under it as a sliver.
function frameSphere(c, r) {
  r = Math.max(r, 1e-6);
  nav._sceneScale = r;
  const dist = r / Math.tan(camera.fov / 2) * 1.3;
  const el = 55 * Math.PI / 180, az = -35 * Math.PI / 180;
  const dir = [Math.cos(el) * Math.cos(az), Math.cos(el) * Math.sin(az), Math.sin(el)];
  camera.lookAt(v3.add(c, v3.scale(dir, dist)), c, [0, 0, 1]);
  state.needRender = true;
}

function boundsOf(clips) {
  const lo = [Infinity, Infinity, Infinity], hi = [-Infinity, -Infinity, -Infinity];
  for (const c of clips) {
    if (!c.local) continue;
    for (let i = 0; i < c.local.length; i += 3)
      for (let a = 0; a < 3; a++) {
        const v = c.local[i + a] * (a === 2 ? opts.altScale : 1);
        if (v < lo[a]) lo[a] = v;
        if (v > hi[a]) hi[a] = v;
      }
  }
  return isFinite(lo[0]) ? [lo, hi] : null;
}

function frameBounds(b) {
  if (!b) { frameSphere([0, 0, 0], 30); return; }
  const [lo, hi] = b;
  const ctr = [0, 1, 2].map((a) => (lo[a] + hi[a]) / 2);
  frameSphere(ctr, Math.max(30, 0.6 * Math.hypot(hi[0] - lo[0], hi[1] - lo[1], hi[2] - lo[2])));
}

function fitAll() { frameBounds(boundsOf(state.clips.filter((c) => c.visible))); }
function fitClip(clip) { if (clip && clip.local) frameBounds(boundsOf([clip])); }

function lookDown() {
  const d = Math.max(10, camera.dist());
  camera.lookAt([camera.target[0], camera.target[1] - 1e-3, camera.target[2] + d], camera.target, [0, 1, 0]);
  state.needRender = true;
}

// A receiver logged faster than it fixes repeats its timestamp, which makes
// the median interval zero; the average over the capture still means something.
function effectiveHz(s) {
  if (!s || !s.count) return 0;
  if (s.rate) return s.rate;
  const span = s.tLast - s.tFirst;
  return span > 0 ? s.count / span : 0;
}

function fmtHz(v) { return v ? (v >= 1 ? `${v.toFixed(v < 10 ? 1 : 0)} Hz` : `${v.toFixed(2)} Hz`) : '—'; }
function fmtClock(t) {
  const s = Math.max(0, t), m = Math.floor(s / 60);
  return `${m}:${(s % 60).toFixed(2).padStart(5, '0')}`;
}
function escapeHtml(s) { return String(s).replace(/[&<>]/g, (c) => ({ '&': '&amp;', '<': '&lt;', '>': '&gt;' }[c])); }
function turboCss(x) {
  const r = 0.13572138 + x * (4.6153926 + x * (-42.66032258 + x * (132.13108234 + x * (-152.94239396 + x * 59.28637943))));
  const g = 0.09140261 + x * (2.19418839 + x * (4.84296658 + x * (-14.18503333 + x * (4.27729857 + x * 2.82956604))));
  const b = 0.1066733 + x * (12.64194608 + x * (-60.58204836 + x * (110.36276771 + x * (-89.90310912 + x * 27.34824973))));
  const q = (v) => Math.round(255 * Math.min(1, Math.max(0, v)));
  return `rgb(${q(r)},${q(g)},${q(b)})`;
}

function toast(msg) {
  dom.toast.textContent = msg;
  dom.toast.style.display = 'block';
  clearTimeout(toast._t);
  toast._t = setTimeout(() => { dom.toast.style.display = 'none'; }, 5000);
}

// ---------------------------------------------------------------------------
// Wiring
// ---------------------------------------------------------------------------
function bindCheck(id, key, after) {
  const el = $(id);
  el.checked = opts[key];
  el.onchange = () => { opts[key] = el.checked; if (after) after(); state.needRender = true; };
}
function bindRange(id, key, fmt, after) {
  const el = $(id), lab = $(id.replace('opt-', 'v-'));
  const show = () => { if (lab) lab.textContent = fmt(opts[key]); };
  el.value = String(opts[key]);
  show();
  el.oninput = () => { opts[key] = parseFloat(el.value); show(); if (after) after(); state.needRender = true; };
}

function wire() {
  const vp = dom.viewport;
  vp.addEventListener('dragover', (e) => { e.preventDefault(); vp.classList.add('dragover'); });
  vp.addEventListener('dragleave', () => vp.classList.remove('dragover'));
  vp.addEventListener('drop', async (e) => {
    e.preventDefault();
    vp.classList.remove('dragover');
    try { await scanEntries(await gatherEntries(e.dataTransfer)); }
    catch (err) { toast(String(err.message || err)); }
  });
  document.body.addEventListener('dragover', (e) => e.preventDefault());
  document.body.addEventListener('drop', (e) => e.preventDefault());

  const pick = (input) => {
    input.value = '';
    input.click();
  };
  $('btn-open').onclick = () => pick($('file-input'));
  $('btn-open-dir').onclick = () => pick($('dir-input'));
  for (const id of ['file-input', 'dir-input']) {
    $(id).onchange = async (e) => {
      const files = Array.from(e.target.files || []);
      await scanEntries(files.map((f) => ({ path: f.webkitRelativePath || f.name, getFile: async () => f })));
    };
  }
  $('btn-clear').onclick = () => {
    scanAbort = true;
    for (const c of state.clips) if (c.gpu) { c.gpu.track.dispose(); c.gpu.drops.dispose(); c.gpu.circles.dispose(); c.gpu.trail.forEach((b) => b.dispose()); }
    state.clips = []; state.photoSets.clear(); state.active = -1; state.frame = null;
    state.scanned = 0; state.found = 0; state.userMoved = false;
    tiles.clear();
    renderList(); updateCounts(); refreshCharts(); updateLegend();
    dom.hint.style.display = '';
    setStatus('Drop files to begin');
  };

  $('color-by').onchange = (e) => { opts.colorBy = e.target.value; rebuildAll(); updateColorbar(); };
  bindCheck('opt-connect', 'connect');
  bindCheck('opt-points', 'points');
  bindCheck('opt-stale', 'stale', rebuildAll);
  bindCheck('opt-drops', 'drops', rebuildAll);
  bindCheck('opt-circles', 'circles', rebuildAll);
  bindRange('opt-sigma', 'sigma', (v) => `${v.toFixed(1)} m/DOP`, rebuildAll);
  bindRange('opt-lw', 'lw', (v) => `${v.toFixed(1)} px`);
  const altEl = $('opt-altscale');
  altEl.value = '0';
  altEl.oninput = () => {
    opts.altScale = Math.pow(10, parseFloat(altEl.value));
    $('v-alt').textContent = `${opts.altScale.toFixed(opts.altScale < 10 ? 1 : 0)}×`;
    rebuildAll();
  };

  const sel = $('basemap');
  for (const [k, s] of Object.entries(SOURCES)) {
    const o = document.createElement('option');
    o.value = k; o.textContent = s.label;
    sel.append(o);
  }
  sel.value = opts.basemap;
  sel.onchange = () => {
    opts.basemap = sel.value;
    $('row-custom').style.display = opts.basemap === 'custom' ? '' : 'none';
    tiles.setSource(opts.basemap, opts.customUrl);
    updateLegend();
    state.needRender = true;
  };
  $('custom-url').oninput = (e) => { opts.customUrl = e.target.value.trim(); tiles.setSource(opts.basemap, opts.customUrl); };
  bindRange('opt-mapa', 'mapAlpha', (v) => `${Math.round(v * 100)}%`);
  bindRange('opt-mapd', 'mapDesat', (v) => `${Math.round(v * 100)}%`);
  $('ground-mode').onchange = (e) => { opts.ground = e.target.value; rebuildAll(); };

  bindCheck('opt-triad', 'triad');
  bindCheck('opt-accelvec', 'accelVec');
  bindCheck('opt-gravvec', 'gravVec');
  bindRange('opt-trail', 'trail', (v) => (v ? `${v} s` : 'off'), rebuildAll);
  bindRange('opt-triadsize', 'triadSize', (v) => `${Math.round(v * 100)}%`, rebuildAll);

  for (const r of ROWS) {
    const id = `row-${r.key}`;
    const wrap = document.createElement('div');
    wrap.className = 'row check';
    wrap.innerHTML = `<label><input autocomplete="off" type="checkbox" id="${id}"> ${r.label}</label>`;
    dom.rowToggles.append(wrap);
    const el = wrap.querySelector('input');
    el.checked = opts.rows[r.key];
    el.onchange = () => { opts.rows[r.key] = el.checked; refreshCharts(); };
  }
  $('opt-gyrodeg').onchange = (e) => { opts.gyroDeg = e.target.checked; refreshCharts(); updateColorbar(); updateReadout(); };

  bindCheck('opt-grid', 'grid');
  bindCheck('opt-axes', 'axes');
  bindCheck('opt-follow', 'follow');
  $('nav-mode').onchange = (e) => { nav.mode = e.target.value; };
  $('btn-fit').onclick = fitAll;
  $('btn-top').onclick = lookDown;

  const setPrivacy = (on) => {
    opts.privacy = on;
    $('opt-privacy').checked = on;
    $('btn-privacy').classList.toggle('on', on);
    $('btn-privacy').textContent = `🔒 Screenshot-safe: ${on ? 'on' : 'off'}`;
    renderList(); updateLegend(); updateReadout(); refreshCharts();
    const c = state.clips[state.active];
    dom.report.textContent = c ? (on ? scrubReport(c.report) : c.report) : '—';
    state.needRender = true;
  };
  $('opt-privacy').onchange = (e) => setPrivacy(e.target.checked);
  $('btn-privacy').onclick = () => setPrivacy(!opts.privacy);

  $('btn-play').onclick = () => {
    state.playing = !state.playing;
    $('btn-play').textContent = state.playing ? '❚❚' : '▶';
  };
  $('play-rate').oninput = (e) => {
    state.rate = Math.pow(10, parseFloat(e.target.value));
    $('play-rate-label').textContent = `${state.rate.toFixed(state.rate < 10 ? 1 : 0)}×`;
  };
  $('btn-zoom-reset').onclick = () => charts.resetZoom();
  $('panel-toggle').onclick = () => $('panel').classList.toggle('collapsed');

  let splitting = false;
  dom.splitter.addEventListener('pointerdown', (e) => { splitting = true; dom.splitter.setPointerCapture(e.pointerId); });
  dom.splitter.addEventListener('pointermove', (e) => {
    if (!splitting) return;
    const r = $('left').getBoundingClientRect();
    const h = Math.max(60, Math.min(r.height - 120, r.bottom - e.clientY));
    dom.charts.style.height = `${h}px`;
    charts.draw();
  });
  dom.splitter.addEventListener('pointerup', () => { splitting = false; });

  wireNav();
  window.addEventListener('resize', () => { charts.draw(); state.needRender = true; });
}

function wireNav() {
  const vp = dom.viewport;
  const pointers = new Map();
  let last = null, pinch = 0;
  vp.addEventListener('pointerdown', (e) => {
    vp.setPointerCapture(e.pointerId);
    pointers.set(e.pointerId, [e.clientX, e.clientY]);
    last = [e.clientX, e.clientY];
  });
  vp.addEventListener('pointermove', (e) => {
    if (!pointers.has(e.pointerId)) return;
    pointers.set(e.pointerId, [e.clientX, e.clientY]);
    if (pointers.size === 2) {
      const [a, b] = Array.from(pointers.values());
      const d = Math.hypot(a[0] - b[0], a[1] - b[1]);
      if (pinch) nav.dolly((pinch - d) * 4);
      pinch = d;
      state.needRender = true;
      return;
    }
    if (!last) return;
    const dx = e.clientX - last[0], dy = e.clientY - last[1];
    last = [e.clientX, e.clientY];
    nav.drag(dx, dy, e.shiftKey || e.buttons === 4 || e.buttons === 2);
    state.userMoved = true;
    state.needRender = true;
  });
  const up = (e) => { pointers.delete(e.pointerId); if (!pointers.size) { last = null; pinch = 0; } };
  vp.addEventListener('pointerup', up);
  vp.addEventListener('pointercancel', up);
  vp.addEventListener('contextmenu', (e) => e.preventDefault());
  vp.addEventListener('wheel', (e) => {
    e.preventDefault();
    nav.dolly(e.deltaY);
    state.userMoved = true;
    state.needRender = true;
  }, { passive: false });
  window.addEventListener('keydown', (e) => {
    if (/input|select|textarea/i.test(e.target.tagName)) return;
    if (e.key === ' ') { e.preventDefault(); $('btn-play').click(); }
    nav.keyDown(e.key.toLowerCase());
  });
  window.addEventListener('keyup', (e) => nav.keyUp(e.key.toLowerCase()));
}

// Test hook, the same shape as the model viewer's window.__viewer.
window.__telemetry = { state, opts, fitAll, refreshCharts, selectClip };

if (scene) {
  wire();
  frameSphere([0, 0, 0], 30);
  updateCounts();
  refreshCharts();
  requestAnimationFrame(loop);
}
