// Application entry: wires the WASM loader, WebGL renderer, camera controller,
// and the UI panel together.

import { initWasm, loadModel, sortSplats, freeSplatSh, reduceSh, splatHistogram, meshHistogram, meshEdgeCount, fitSphere, raycastMesh, dsFree, dsPickPoint } from './wasm.js';
import { loadDatasetFiles, parseDatasetComponent } from './dataset.js';
import { Renderer } from './renderer.js';
import { Camera, Nav } from './camera.js';
import { v3, mat3, quat } from './linalg.js';
import { GAMUTS, toColMajor, hexToRgb } from './colors.js';
import { drawHistogram } from './histogram.js';

const $ = (id) => document.getElementById(id);
const canvas = $('glcanvas');

let renderer, camera, nav;
let dirty = true;
let model = null;          // { type:'splat'|'mesh', ... meta }
let lastSortDir = [0,0,0];
const opts = {
  primitive: 0, gamut: toColMajor(GAMUTS['Rec.709']), transfer: 0, isLinear: false,
  shDegree: 0, exposure: 1.0, opacityScale: 1.0,
  cameraModel: 'perspective', upAxis: 'z', showGrid: true, gridRadius: 1,
  background: hexToRgb('0a0b0e'), shade: true, flatShade: false, meshColor: true,
  // dataset (point cloud + camera frustums)
  pointSize: 2.0, frustumScale: 1.0, showPoints: true, showFrustums: true, hoverCam: -1,
};

// dataset session state (cameras metadata for hover/pick/view-from-camera)
let dataset = null;   // { cameras, components, token, fit, frustumBase, frustumMult, pickR }

// Valid FOV range (degrees) per display camera model: tan blows up toward
// 180° for the linear models; the fisheye projections are defined to 360°.
// Equirectangular has no entry — it is a fixed full-sphere view (no FOV).
const FOV_RANGE = {
  perspective: [10, 150], orthographic: [10, 150],
  fisheye: [10, 360], equisolid: [10, 360],
};
// Last FOV per display model (radians), restored when switching back to that
// model — a 210° fisheye view must not drag a 100° perspective FOV to 150°.
const fovMemory = {};

function setStatus(text, cls='') { $('status-text').textContent = text; $('status-dot').className = cls; }

// Transient popup over the viewport, so load problems are visible without
// opening the console. Auto-hides; a new message replaces the previous one.
let toastTimer = null;
function showToast(msg, isError = true) {
  const el = $('toast');
  el.textContent = msg;
  el.className = isError ? 'err' : 'warn';
  el.style.display = 'block';
  clearTimeout(toastTimer);
  toastTimer = setTimeout(() => { el.style.display = 'none'; }, 8000);
}

// ---------------------------------------------------------------------------
// Boot
// ---------------------------------------------------------------------------
async function boot() {
  setStatus('Loading engine…');
  try {
    await initWasm();
    renderer = new Renderer(canvas);
  } catch (e) {
    setStatus('Init failed: ' + e.message, 'err');
    console.error(e);
    return;
  }
  camera = new Camera();
  nav = new Nav(camera);
  nav.onChange = () => { dirty = true; };
  initSortWorker();
  wireInput();
  wireControls();
  syncControls();       // apply any browser-restored control values on refresh
  resize();
  new ResizeObserver(resize).observe($('viewport'));
  setStatus('Ready — drop a model', 'ok');
  requestAnimationFrame(loop);

  // Optional auto-load: index.html?model=<url> (handy for hosting a default
  // model or for automated testing).
  const url = new URLSearchParams(location.search).get('model');
  if (url) {
    try { await load(await fetchModelFiles(url)); }
    catch (e) { setStatus('Auto-load failed: ' + e.message, 'err'); }
  }
}

// Fetch a model URL plus any sibling files it references (so hosting a
// textured .gltf/.obj with external .bin/.mtl/image works from a link).
// PLY urls are NOT buffered here: a marker entry routes them through the
// streaming loader (loadModelFromUrl), so a multi-GB hosted splat never
// materializes as a Blob.
async function fetchModelFiles(url) {
  const dir = url.slice(0, url.lastIndexOf('/')+1);
  const name = url.split('/').pop().split('?')[0];
  if (name.toLowerCase().endsWith('.ply')) {
    const marker = { path: name, name, urlStream: url };
    marker.getFile = async () => marker;
    return [marker];
  }
  const fetchFile = async (u, n) => new File([await (await fetch(u)).blob()], n);
  const files = [await fetchFile(url, name)];
  const lname = name.toLowerCase();
  const add = async (n) => { try { files.push(await fetchFile(dir+n, n)); } catch {} };
  if (lname.endsWith('.gltf')) {
    const g = JSON.parse(await files[0].text());
    for (const b of g.buffers||[]) if (b.uri && !b.uri.startsWith('data:')) await add(b.uri.split('/').pop());
    for (const im of g.images||[]) if (im.uri && !im.uri.startsWith('data:')) await add(im.uri.split('/').pop());
  } else if (lname.endsWith('.obj')) {
    const t = await files[0].slice(0, 1 << 18).text();   // mtllib is in the header
    const mtl = t.match(/^mtllib\s+(.+)$/m);
    if (mtl) { const mn = mtl[1].trim(); await add(mn);
      const mf = files.find(f=>f.name===mn);
      if (mf) { const mm = (await mf.text()).match(/^\s*map_Kd\s+(.+)$/m); if (mm) await add(mm[1].trim().split(/[\\/]/).pop()); } }
  }
  return files;
}

// expose for automated testing / embedding
window.__viewer = {
  load: (files) => load(files),
  get model() { return model; },
  get opts() { return opts; },
  setOpt: (k, v) => { opts[k] = v; dirty = true; },
  get nav() { return nav; },
  get camera() { return camera; },
  snapshot: () => { if (model && model.type==='splat') maybeSort(true); renderer.render(camera, opts); return renderer.snapshot(); },
  get sortStats() { return sortStats; },
  get dataset() { return dataset; },
  viewFromCamera: (i) => viewFromCamera(i),
  pickCamera: (px, py) => pickCamera(px, py),
};

// ---------------------------------------------------------------------------
// Render loop
// ---------------------------------------------------------------------------
let lastT = performance.now(), frames = 0, fpsT = performance.now();
function loop(now) {
  const dt = Math.min((now - lastT)/1000, 0.1); lastT = now;
  if (nav.tick(dt)) dirty = true;
  if (nav.gamepadTick(dt)) dirty = true;

  if (dirty) {
    if (model && model.type === 'splat') maybeSort(false);
    renderer.render(camera, opts);
    dirty = false;
  }
  // fps
  frames++;
  if (now - fpsT > 500) { $('fps').textContent = (frames*1000/(now-fpsT)|0) + ' fps'; frames=0; fpsT=now; }
  requestAnimationFrame(loop);
}

function upTransform() {
  return opts.upAxis === 'y' ? [1,0,0, 0,0,1, 0,-1,0] : [1,0,0, 0,1,0, 0,0,1];
}
// Fit the camera to the current model using a robust (median-based) center
// and radius — trained splats and generated meshes often contain far-away
// outliers that make a bounding-box fit useless.
function fitModel() {
  if (!model) return;
  if (model.type === 'dataset') { if (dataset) fitDataset(dataset.fit); return; }
  const fs = fitSphere();                       // [cx,cy,cz, medianDist] (native frame)
  const c = mat3.mulVec(upTransform(), [fs[0], fs[1], fs[2]]);
  const r = 2.0 * fs[3] || 1;
  opts.gridRadius = r;
  nav.fitSphere(c, r, [0,0,1]);
}
// Switch the up axis: the model (and its native-frame grid) visibly rotates
// into the new canonical orientation; the camera stays put, so there is no
// viewport reset and the toggle takes effect immediately.
function setUpAxis(axis) {
  if (axis === opts.upAxis) return;
  opts.upAxis = axis;
  lastSortDir = [0,0,0];
  dirty = true;
}

function forwardNative() {
  // camera forward in the model's native frame: U^T * forward_canonical
  return v3.norm(mat3.mulVec(mat3.transpose(upTransform()), camera.forward()));
}
function camPosNative() {
  return mat3.mulVec(mat3.transpose(upTransform()), camera.pos);
}
// Sorting-depth mode (must match ssv_sort): planar depth for perspective /
// orthographic; camera distance for equirectangular; a smooth |z|/radial
// blend for fisheye so >180° views with splats behind the camera sort right.
function sortMode() {
  return camera.model === 'equirectangular' ? 1
       : (camera.model === 'fisheye' || camera.model === 'equisolid') ? 2 : 0;
}
let lastSortPos = [0,0,0], lastSortMode = -1;

// ---- asynchronous depth sorting ----
// The counting sort is O(N) but at 10-20M splats it takes 100s of ms; run it
// in a worker so look-around never blocks the render loop. The GPU keeps
// drawing with the previous order until a fresh one arrives (latest request
// wins; at most one sort in flight). A synchronous WASM path remains for the
// initial sort after load and for snapshot() (deterministic tests).
let sortWorker = null, sortWorkerOk = false;
let sortBusy = false, sortPending = null, sortGen = 0;
const sortStats = { async: 0, sync: 0 };
function initSortWorker() {
  try {
    sortWorker = new Worker(new URL('./sortworker.js', import.meta.url), { type: 'module' });
    sortWorker.onmessage = (e) => {
      const d = e.data;
      if (d.type !== 'sorted') return;
      sortBusy = false;
      if (d.gen === sortGen && d.order.byteLength > 0 && model && model.type === 'splat') {
        renderer.updateSplatOrder(new Uint32Array(d.order));
        sortStats.async++;
        sortStats.native = !!d.native;
        dirty = true;
        sortWorker.postMessage({ type: 'recycle', buffer: d.order }, [d.order]);
      }
      if (sortPending) { const r = sortPending; sortPending = null; sortBusy = true; sortWorker.postMessage(r); }
    };
    sortWorker.onerror = () => { sortWorkerOk = false; };  // fall back to sync
    sortWorkerOk = true;
  } catch { sortWorkerOk = false; }
}
function sortWorkerInitModel(data) {
  sortGen++; sortPending = null;
  if (!sortWorkerOk || !sortWorker) return;
  // the worker owns its own copy — xyz only (the sort never reads opacity),
  // 12 instead of 16 bytes per splat
  const N = data.count, src = data.posop;
  const xyz = new Float32Array(N * 3);
  for (let i = 0, j = 0, k = 0; i < N; i++, j += 3, k += 4) {
    xyz[j] = src[k]; xyz[j+1] = src[k+1]; xyz[j+2] = src[k+2];
  }
  sortWorker.postMessage({ type: 'init', gen: sortGen, count: N, positions: xyz.buffer }, [xyz.buffer]);
}

function maybeSort(force) {
  if (!model || model.type !== 'splat') return;
  const d = forwardNative();
  const mode = sortMode();
  const c = camPosNative();
  if (!force && mode === lastSortMode &&
      v3.dot(d, lastSortDir) > Math.cos(0.06) &&
      // nonplanar depths depend on the camera position too
      !(mode > 0 && v3.len(v3.sub(c, lastSortPos)) > 0.01 * nav._sceneScale)) return;
  lastSortDir = d; lastSortPos = c; lastSortMode = mode;
  if (sortWorkerOk && !force) {
    const req = { type: 'sort', gen: sortGen, mode, c, d };
    if (sortBusy) sortPending = req;                 // latest wins
    else { sortBusy = true; sortWorker.postMessage(req); }
    return;
  }
  const order = sortSplats(mode, c[0], c[1], c[2], d[0], d[1], d[2]);
  if (order) renderer.updateSplatOrder(order);
  sortStats.sync++;
}

// ---------------------------------------------------------------------------
// Resize (device-pixel aware, capped for performance)
// ---------------------------------------------------------------------------
function resize() {
  const vp = $('viewport');
  const dpr = window.devicePixelRatio || 1;
  let w = Math.round(vp.clientWidth * dpr), h = Math.round(vp.clientHeight * dpr);
  const maxPix = 2500000;
  const scale = Math.min(1, Math.sqrt(maxPix / Math.max(w*h, 1)));
  w = Math.max(1, Math.round(w*scale)); h = Math.max(1, Math.round(h*scale));
  if (canvas.width !== w || canvas.height !== h) { canvas.width = w; canvas.height = h; dirty = true; }
}

// ---------------------------------------------------------------------------
// Model loading
// ---------------------------------------------------------------------------
// Normalize a drop / picker input to entries [{path, getFile}] (path relative,
// for MEMFS mounting + sibling lookup). getFile() materializes the File
// LAZILY: a dropped dataset folder can hold thousands of high-resolution
// images, and creating a File per image (a filesystem stat each) up front
// froze the load for a long time even though only the small metadata files
// are ever read.
function toEntries(input) {
  return Array.from(input).map(e =>
    e && e.getFile ? e :                                            // already lazy
    e && e.file ? { path: e.path, getFile: async () => e.file } :   // {file, path}
    { path: e.webkitRelativePath || e.name, getFile: async () => e });  // File
}
// COLMAP / Nerfstudio / Metashape signal in the dropped set.
function looksLikeDataset(entries) {
  return entries.some(e => {
    const p = e.path.toLowerCase();
    return /(^|\/)(cameras|images|points3d)\.(bin|txt)$/.test(p)
        || /(^|\/)transforms[^/]*\.json$/.test(p)
        || /\.(xml|psx)$/.test(p);
  });
}

async function load(input) {
  const entries = toEntries(input);
  try {
    if (looksLikeDataset(entries)) { await loadDataset(entries); return; }
    try {
      await loadSingleModel(entries);
    } catch (e) {
      // A bare XYZ(RGB) point-cloud .ply is not a splat/mesh — try the dataset
      // path (the bridge shows it as a point cloud).
      if (entries.some(x => /\.ply$/i.test(x.path))) {
        try { await loadDataset(entries); return; } catch {}
      }
      throw e;
    }
  } catch (e) {
    console.error(e);
    setStatus('Load error: ' + e.message, 'err');
    showToast('Load error: ' + e.message);
  }
}

async function loadSingleModel(entries) {
  setStatus('Loading model…', '');
  // Materialize only files a model load can use: the model itself, plus
  // external resources (.bin/.mtl/images) when a .gltf/.obj needs siblings —
  // never thousands of unrelated dataset images.
  const needsSiblings = entries.some(e => /\.(gltf|obj)$/i.test(e.path));
  let wanted = entries.filter(e => /\.(ply|obj|gltf|glb)$/i.test(e.path) ||
    (needsSiblings && /\.(bin|mtl|png|jpg|jpeg|webp)$/i.test(e.path)));
  if (!wanted.length) wanted = entries;
  const files = await Promise.all(wanted.map(e => e.getFile()));
  const res = await loadModel(files);
  const firstModel = !model;
  if (dataset) { try { dsFree(); } catch {} }   // release the retained point cloud
  dataset = null;
  if (res.kind === 'splat') {
    // Upload; if the GPU runs out of memory on the (dominant) SH textures,
    // drop one SH degree at a time and retry — a huge model renders with
    // reduced view-dependence instead of failing outright.
    let data = res.data;
    while (renderer.setSplat(data) === 'oom-sh') {
      const deg = data.shDegree - 1;
      data = reduceSh(deg);
      showToast(`Out of GPU memory — SH degree reduced to ${deg}`, false);
    }
    freeSplatSh();   // SH lives on the GPU now; drop the WASM copy (+ quats)
    sortWorkerInitModel(data);
    model = { type:'splat', count: data.count, shDegree: data.shDegree };
    showSplatUI(data);
  } else {
    renderer.setMesh(res.data, res.image || null);
    sortGen++; sortPending = null;   // invalidate any in-flight splat sort
    model = { type:'mesh', nv: res.data.nv, nt: res.data.nt };
    showMeshUI(res.data);
  }
  // Only fit the camera for the first model of the session: replacing the
  // model (e.g. dropping the mesh of the same object after a splat) keeps
  // the current view. The scene scale (move/zoom speed) is still refreshed.
  if (firstModel) fitModel();
  else { const fs = fitSphere(); nav._sceneScale = 2.0 * fs[3] || 1; opts.gridRadius = nav._sceneScale; }
  lastSortDir = [0,0,0]; lastSortMode = -1;
  if (model.type === 'splat') maybeSort(true);   // initial order, synchronous
  histCache.clear();
  updateHistParams();
  $('drop-hint').style.display = 'none';
  const name = files.find(f=>/\.(ply|obj|gltf|glb)$/i.test(f.name))?.name || '';
  $('st-file').textContent = name.length>22 ? '…'+name.slice(-21) : name;
  setStatus(model.type==='splat' ? 'Splat model loaded' : 'Mesh loaded', 'ok');
  dirty = true;
}

// Load a COLMAP / Nerfstudio / Metashape dataset (or bare point cloud).
// `token` selects a component of an already-mounted dataset (picker switch);
// omit it to mount `entries` and auto-select the first component.
async function loadDataset(entries, token) {
  setStatus(token ? 'Loading component…' : 'Loading dataset…', '');
  const res = token
    ? { ...parseDatasetComponent(token), components: dataset.components, selectedToken: token }
    : await loadDatasetFiles(entries);

  const firstDataset = !model || model.type !== 'dataset';
  renderer.setDataset({ points: res.points, frustum: res.frustum });
  // Keep the wasm-side dataset resident so double-click point picking
  // (dsPickPoint) still works; it is freed when a non-dataset model loads.
  sortGen++; sortPending = null;       // invalidate any in-flight splat sort

  const fit = res.fit;
  const frustumBase = 0.02 * (fit[3] || 1);
  const frustumMult = dataset ? dataset.frustumMult : 1.0;
  opts.frustumScale = frustumBase * frustumMult;
  dataset = {
    cameras: res.cameras,
    components: res.components || (dataset && dataset.components) || [],
    token: res.selectedToken || token,
    fit, frustumBase, frustumMult,
    pickR: res.frustum.pickR,   // size-1 per-camera frustum radii (ray picking)
  };
  model = { type:'dataset', numCameras: res.cameras.length, numPoints: res.points.count };
  opts.hoverCam = -1;
  showDatasetUI(res.summary);
  if (firstDataset || token) fitDataset(fit); else nav._sceneScale = 2.0*(fit[3]||1);
  histCache.clear(); updateHistParams();
  $('drop-hint').style.display = 'none';
  const label = (dataset.components.find(c=>c.token===dataset.token) || {}).label || 'dataset';
  $('st-file').textContent = label.length>24 ? label.slice(0,23)+'…' : label;
  if (res.error) {
    // parser threw partway but something usable was salvaged — tell the user
    setStatus('Dataset partially loaded', 'err');
    showToast('Dataset partially loaded: ' + res.error);
  } else {
    setStatus('Dataset loaded', 'ok');
  }
  dirty = true;
}

function fitDataset(fit) {
  const c = mat3.mulVec(upTransform(), [fit[0], fit[1], fit[2]]);
  opts.gridRadius = 2.0*(fit[3] || 1);
  nav.fitSphere(c, 2.0*(fit[3] || 1), [0,0,1]);
}

function showSplatUI(data) {
  $('sec-splat').classList.remove('hidden');
  $('sec-mesh').classList.add('hidden');
  $('sec-dataset').classList.add('hidden');
  $('sec-hist').style.display = '';
  $('st-type').textContent = '3D Gaussian Splats';
  $('st-splat-stats').style.display = '';
  $('st-mesh-stats').style.display = 'none';
  $('st-dataset-stats').style.display = 'none';
  $('ds-component-row').style.display = 'none';
  $('st-count').textContent = data.count.toLocaleString();
  $('st-sh').textContent = data.shDegree;
  // slider range 0..model degree, defaulting to the model's actual degree
  const sh = $('shdeg'); sh.max = data.shDegree; sh.value = data.shDegree;
  opts.shDegree = data.shDegree; $('v-shdeg').textContent = sh.value;
}
function showMeshUI(data) {
  $('sec-mesh').classList.remove('hidden');
  $('sec-splat').classList.add('hidden');
  $('sec-dataset').classList.add('hidden');
  $('sec-hist').style.display = '';
  $('st-type').textContent = data.uv ? 'Textured mesh' : (data.color ? 'Vertex-color mesh' : 'Mesh');
  $('st-splat-stats').style.display = 'none';
  $('st-mesh-stats').style.display = '';
  $('st-dataset-stats').style.display = 'none';
  $('ds-component-row').style.display = 'none';
  $('st-verts').textContent = data.nv.toLocaleString();
  $('st-faces').textContent = data.nt.toLocaleString();
  $('st-edges').textContent = '…';
  // edge count can be a touch slow for huge meshes; compute async-ish
  setTimeout(() => { try { $('st-edges').textContent = meshEdgeCount().toLocaleString(); } catch {} }, 0);
}

function showDatasetUI(summary) {
  $('sec-splat').classList.add('hidden');
  $('sec-mesh').classList.add('hidden');
  $('sec-dataset').classList.remove('hidden');
  $('sec-hist').style.display = 'none';    // no per-splat distributions for datasets
  $('st-splat-stats').style.display = 'none';
  $('st-mesh-stats').style.display = 'none';
  $('st-dataset-stats').style.display = '';
  $('st-type').textContent = summary.format || 'Dataset';
  $('st-ds-images').textContent = (summary.num_images ?? 0).toLocaleString();
  $('st-ds-points').textContent = (summary.num_points ?? 0).toLocaleString();
  $('st-ds-groups').textContent = (summary.num_groups ?? 0).toLocaleString();
  // per-model breakdown
  const models = summary.models || {};
  const parts = Object.entries(models).map(([k, v]) => `${k.toLowerCase()}: ${v}`);
  $('st-ds-models').textContent = parts.length ? parts.join(', ') : '—';

  // component picker (only meaningful with >1 component; still lists skipped)
  const sel = $('ds-component');
  const comps = (dataset && dataset.components) || [];
  sel.innerHTML = '';
  for (const c of comps) {
    const o = document.createElement('option');
    o.value = c.token;
    let lbl = c.label;
    if (c.cameras >= 0) lbl += ` (${c.cameras} cam${c.cameras===1?'':'s'})`;
    o.textContent = lbl;
    sel.appendChild(o);
  }
  sel.value = (dataset && dataset.token) || (comps[0] && comps[0].token) || '';
  $('ds-component-row').style.display = comps.length > 1 ? '' : 'none';
}

// ---------------------------------------------------------------------------
// Histograms
// ---------------------------------------------------------------------------
const SPLAT_PARAMS = [
  ['Opacity',0],['Scale (log₁₀)',1],['Effective rank',2],
  ['Color R',3],['Color G',4],['Color B',5],
  ['Scale X (log₁₀)',6],['Scale Y (log₁₀)',7],['Scale Z (log₁₀)',8],['Anisotropy (log₁₀)',9],
];
const MESH_PARAMS = [
  ['Edge length (log₁₀)',0],['Triangle area (log₁₀)',1],['X',2],['Y',3],['Z',4],
];
const histCache = new Map();
function updateHistParams() {
  const sel = $('hist-param'); sel.innerHTML = '';
  const params = model && model.type==='mesh' ? MESH_PARAMS : SPLAT_PARAMS;
  for (const [name, id] of params) { const o=document.createElement('option'); o.value=id; o.textContent=name; sel.appendChild(o); }
  const ctx = $('hist-canvas').getContext('2d'); ctx.clearRect(0,0,9999,9999);
}
function computeHistogram() {
  if (!model || model.type === 'dataset') return;
  const param = +$('hist-param').value;
  const key = model.type + ':' + param;
  let hist = histCache.get(key);
  if (!hist) {
    hist = model.type==='mesh' ? meshHistogram(param, 64) : splatHistogram(param, 64);
    histCache.set(key, hist);
  }
  const label = ($('hist-param').selectedOptions[0]||{}).textContent || '';
  drawHistogram($('hist-canvas'), hist, label);
}

// ---------------------------------------------------------------------------
// Input handling (pointer / wheel / touch / keyboard)
// ---------------------------------------------------------------------------
function wireInput() {
  const vp = $('viewport');
  const pointers = new Map();
  let prevPinch = null;

  vp.addEventListener('pointerdown', (e) => {
    vp.setPointerCapture(e.pointerId);
    pointers.set(e.pointerId, { x:e.clientX, y:e.clientY, button:e.button });
    if (pointers.size === 2) prevPinch = pinchState(pointers);
  });
  vp.addEventListener('pointermove', (e) => {
    const p = pointers.get(e.pointerId); if (!p) return;
    const dx = e.clientX - p.x, dy = e.clientY - p.y;
    p.x = e.clientX; p.y = e.clientY;
    if (pointers.size === 1) {
      const panning = e.shiftKey || p.button === 1 || p.button === 2;
      nav.drag(dx, dy, panning);
      dirty = true;
    } else if (pointers.size === 2) {
      const ps = pinchState(pointers);
      if (prevPinch) {
        nav.dolly((prevPinch.dist - ps.dist) * 4);
        nav.pan(ps.cx - prevPinch.cx, ps.cy - prevPinch.cy);
        let dang = ps.ang - prevPinch.ang;
        dang = (dang + Math.PI) % (2*Math.PI) - Math.PI;
        nav.roll(dang);
        dirty = true;
      }
      prevPinch = ps;
    }
  });
  const up = (e) => { pointers.delete(e.pointerId); if (pointers.size < 2) prevPinch = null; };
  vp.addEventListener('pointerup', up);
  vp.addEventListener('pointercancel', up);
  vp.addEventListener('wheel', (e) => { e.preventDefault(); nav.dolly(e.deltaY); dirty = true; }, { passive:false });
  vp.addEventListener('dblclick', (e) => { e.preventDefault(); pickRecenter(e); });
  vp.addEventListener('contextmenu', (e) => e.preventDefault());

  window.addEventListener('keydown', (e) => {
    if (e.target.tagName === 'SELECT' || e.target.tagName === 'INPUT') return;
    const k = e.key.toLowerCase();
    if (['w','a','s','d','q','e','arrowup','arrowdown','arrowleft','arrowright'].includes(k)) { e.preventDefault(); nav.keyDown(k); }
  });
  window.addEventListener('keyup', (e) => nav.keyUp(e.key.toLowerCase()));
  window.addEventListener('blur', () => nav._keys.clear());

  // hover a camera frustum -> intrinsics tooltip (dataset only, no button held)
  vp.addEventListener('pointermove', (e) => {
    if (!model || model.type !== 'dataset' || !dataset || e.buttons) { hideTooltip(); return; }
    const [px, py] = canvasPixel(e);
    const hit = pickCamera(px, py);
    if (opts.hoverCam !== hit) { opts.hoverCam = hit; dirty = true; }
    vp.style.cursor = hit >= 0 ? 'pointer' : '';
    if (hit >= 0) showTooltip(e, hit); else hideTooltip();
  });
  vp.addEventListener('pointerleave', () => { if (opts.hoverCam !== -1) { opts.hoverCam = -1; dirty = true; } vp.style.cursor = ''; hideTooltip(); });

  // drag & drop / file picker (folder-aware)
  vp.addEventListener('dragover', (e) => { e.preventDefault(); vp.classList.add('dragover'); });
  vp.addEventListener('dragleave', () => vp.classList.remove('dragover'));
  vp.addEventListener('drop', async (e) => {
    e.preventDefault(); vp.classList.remove('dragover');
    const entries = await gatherEntries(e.dataTransfer);
    if (entries.length) load(entries);
  });
  $('file-input').addEventListener('change', (e) => { if (e.target.files.length) load(e.target.files); });
  $('drop-hint').addEventListener('pointerup', () => $('file-input').click());
}

// Recursively collect dropped files (folders via webkitGetAsEntry) into
// [{path, getFile}] keeping relative paths for MEMFS mounting + sibling
// lookup. Only directory LISTINGS happen here — entry.file() (a filesystem
// stat + File creation per file, the expensive part for folders with
// thousands of images) is deferred into getFile() and only ever runs for the
// files a load path actually reads.
async function gatherEntries(dt) {
  const items = dt.items ? Array.from(dt.items) : [];
  const roots = [];
  for (const it of items) {
    if (it.kind === 'file' && it.webkitGetAsEntry) { const en = it.webkitGetAsEntry(); if (en) roots.push(en); }
  }
  if (!roots.length)
    return Array.from(dt.files || []).map(f => ({ path: f.webkitRelativePath || f.name, getFile: async () => f }));
  const out = [];
  const progress = () => { if (out.length % 500 === 0) setStatus(`Scanning folder… (${out.length.toLocaleString()} files)`); };
  const walk = (entry, prefix) => new Promise((resolve) => {
    if (entry.isFile) {
      out.push({
        path: prefix + entry.name,
        getFile: () => new Promise((res, rej) => entry.file(res, rej)),
      });
      progress();
      resolve();
    } else if (entry.isDirectory) {
      const reader = entry.createReader();
      const readBatch = () => reader.readEntries(async (ents) => {
        if (!ents.length) { resolve(); return; }
        await Promise.all(ents.map(en => walk(en, prefix + entry.name + '/')));
        readBatch();   // directory entries arrive in batches until empty
      }, () => resolve());
      readBatch();
    } else resolve();
  });
  setStatus('Scanning folder…');
  await Promise.all(roots.map(r => walk(r, '')));
  return out;
}

// ---------------------------------------------------------------------------
// Dataset picking helpers (frustum hover / view-from-camera / point recenter).
// ---------------------------------------------------------------------------
// Project a canonical-frame world point to canvas pixels (device, y-up), or
// null if it is behind / outside the current camera model's domain. Mirrors
// projectCam in shaders.js.
function projectWorld(p) {
  const pc = mat3.mulVec(camera.viewR(), v3.sub(p, camera.pos));   // camera space (-Z fwd)
  const intr = renderer._intrinsics(camera, canvas.width, canvas.height);
  const { fx, fy, cx, cy } = intr;
  const m = camera.model;
  if (m === 'orthographic') return [fx*pc[0]+cx, fy*pc[1]+cy, -pc[2]];
  if (m === 'perspective') { const t = -pc[2]; if (t <= 1e-6) return null; return [fx*(pc[0]/t)+cx, fy*(pc[1]/t)+cy, t]; }
  if (m === 'equirectangular') {
    const lam = Math.atan2(pc[0], -pc[2]), phi = Math.atan2(pc[1], Math.hypot(pc[0], pc[2]));
    return [cx + lam*fx, cy + phi*fy, v3.len(pc)];
  }
  // fisheye / equisolid
  const lxy = Math.hypot(pc[0], pc[1]);
  const theta = Math.atan2(lxy, -pc[2]);
  const dx = lxy > 1e-9 ? pc[0]/lxy : 0, dy = lxy > 1e-9 ? pc[1]/lxy : 0;
  const r = m === 'equisolid' ? 2*Math.sin(0.5*theta) : theta;
  return [fx*r*dx + cx, fy*r*dy + cy, v3.len(pc)];
}

// World-space (canonical frame) view ray through canvas pixel (px,py) under
// the current display camera model. rd is unit length.
function cursorRay(px, py) {
  const intr = renderer._intrinsics(camera, canvas.width, canvas.height);
  const uv = [(px - intr.cx)/intr.fx, (py - intr.cy)/intr.fy];
  const R = camera.rotMat3();
  if (camera.model === 'orthographic')
    return { ro: v3.add(camera.pos, mat3.mulVec(R, [uv[0], uv[1], 0])), rd: mat3.mulVec(R, [0, 0, -1]) };
  return { ro: camera.pos, rd: mat3.mulVec(R, unprojectDir(uv)) };
}

// Camera frustum under the cursor: { i, t } (index + depth along the cursor
// ray) or null. Casts the view ray against each camera's frustum bounding
// sphere (center = camera position, radius = frustum extent x size slider) so
// pointing anywhere on/inside a frustum counts — not just near its apex.
// Nearest surface hit wins; a sphere the eye is inside is skipped (when
// viewing *from* a camera you interact with the scene, not with that camera's
// own shell). Falls back to nearest projected center within a pixel threshold
// so tiny/faraway frustums stay pickable. Hidden cameras are not pickable
// (no hover tooltip, no double-click).
function pickCameraHit(px, py) {
  if (!dataset || !opts.showFrustums) return null;
  const U = upTransform();
  const cams = dataset.cameras;
  const pickR = dataset.pickR;
  const scale = opts.frustumScale || 1.0;
  const { ro, rd } = cursorRay(px, py);
  let best = -1, bestT = Infinity;
  for (let i = 0; i < cams.length; i++) {
    const m = cams[i].c2w;
    const c = mat3.mulVec(U, [m[3], m[7], m[11]]);
    const r = (pickR ? pickR[i] : 0) * scale;
    const oc = v3.sub(c, ro);
    const oc2 = v3.dot(oc, oc);
    if (oc2 <= r*r) continue;                    // eye inside this frustum
    const b = v3.dot(oc, rd);
    if (b <= 0) continue;                        // behind the ray origin
    const d2 = oc2 - b*b;
    if (d2 > r*r) continue;
    const t = b - Math.sqrt(r*r - d2);
    if (t < bestT) { bestT = t; best = i; }
  }
  if (best >= 0) return { i: best, t: bestT };
  // fallback: nearest projected camera center within a pixel threshold
  const thr = 18 * (window.devicePixelRatio || 1);
  let bestD = thr*thr, bestB = 0;
  for (let i = 0; i < cams.length; i++) {
    const m = cams[i].c2w;
    const c = mat3.mulVec(U, [m[3], m[7], m[11]]);
    const sp = projectWorld(c);
    if (!sp || sp[2] <= 0) continue;
    const dx = sp[0]-px, dy = sp[1]-py, d = dx*dx + dy*dy;
    if (d < bestD) { bestD = d; best = i; bestB = v3.dot(v3.sub(c, ro), rd); }
  }
  return best >= 0 ? { i: best, t: bestB } : null;
}

// Point-cloud point under the cursor: { t, p } (depth + canonical-frame
// position) or null. Nearest point along the ray within a 3% angular cone.
function datasetPointHit(px, py) {
  const { ro, rd } = cursorRay(px, py);
  const Ut = mat3.transpose(upTransform());
  const on = mat3.mulVec(Ut, ro), dn = mat3.mulVec(Ut, rd);
  const pick = dsPickPoint(on[0],on[1],on[2], dn[0],dn[1],dn[2]);
  if (pick.index >= 0 && pick.t > 0 && pick.perp < 0.03 * pick.t)
    return { t: pick.t, p: v3.add(ro, v3.scale(rd, pick.t)) };
  return null;
}

// Camera under the cursor as the user *sees* it: a frustum occluded by the
// rendered point cloud (a point-cloud hit nearer along the ray) is not
// picked. Keeps hover and double-click consistent with the visible scene.
function pickCamera(px, py) {
  const hit = pickCameraHit(px, py);
  if (!hit) return -1;
  if (opts.showPoints && dataset) {
    const pt = datasetPointHit(px, py);
    if (pt && pt.t < hit.t) return -1;
  }
  return hit.i;
}

// Set the interactive camera to look through dataset camera i: same pose and
// the matching display camera model (average focal for the FOV; cx/cy and
// distortion omitted — the display projection is the ideal model).
function viewFromCamera(i) {
  const cam = dataset.cameras[i];
  const m = cam.c2w, U = upTransform();
  const Rnat = [m[0],m[4],m[8],  m[1],m[5],m[9],  m[2],m[6],m[10]];   // cam->world (native, col-major)
  camera.pos = mat3.mulVec(U, [m[3], m[7], m[11]]);
  camera.rot = quat.fromMat3(mat3.mul(U, Rnat));
  camera.target = v3.add(camera.pos, v3.scale(camera.forward(), nav._sceneScale));

  // dataset CameraModelType -> display model (renderer CAM_MODEL names)
  const displayModel = ['perspective','fisheye','equisolid','equirectangular'][cam.model] || 'perspective';
  const f = (cam.fx + cam.fy) / 2;
  const h = cam.h || 2*cam.cy || 0;
  const s = Math.min(cam.w || h, h);    // display fisheye FOV spans min(w,h)
  let fov = camera.fov;
  if (f > 0 && h > 0) {
    if (cam.model === 1)      fov = s / f;                                    // equidistant: r = f*theta
    else if (cam.model === 2) fov = 4*Math.asin(Math.min(s/(4*f), 1));        // r = 2f sin(theta/2)
    else if (cam.model !== 3) fov = 2*Math.atan(h/(2*f));                     // pinhole (equirect: fov unused)
  }
  const sel = $('camera-model');
  sel.value = displayModel;
  sel.dispatchEvent(new Event('change'));   // sets opts.cameraModel, camera.model, FOV range
  const range = FOV_RANGE[displayModel];
  if (range) {
    camera.fov = Math.min(Math.max(fov, range[0]*Math.PI/180), range[1]*Math.PI/180);
    fovMemory[displayModel] = camera.fov;   // this model's FOV is now the dataset camera's
    const deg = Math.round(camera.fov*180/Math.PI);
    $('fov').value = deg;
    $('v-fov').textContent = deg + '°';
  }
  dirty = true;
}

function showTooltip(e, i) {
  const cam = dataset.cameras[i];
  const el = $('cam-tooltip');
  const MODEL = ['Pinhole','Fisheye','Equisolid','Equirectangular'];
  const d = cam.dist || [];
  const nz = d.some(x => x);
  const dlabels = ['k1','k2','k3','k4','p1','p2','s1','s2','b1','b2'];
  const dstr = nz ? d.map((x,j)=> x ? `${dlabels[j]} ${x.toFixed(4)}` : null).filter(Boolean).join(', ') : 'none';
  el.innerHTML =
    `<b>${cam.name || 'camera ' + i}</b><br>` +
    `${MODEL[cam.model] || 'model ' + cam.model} · ${cam.w}×${cam.h}<br>` +
    `fx ${cam.fx.toFixed(1)}  fy ${cam.fy.toFixed(1)}<br>` +
    `cx ${cam.cx.toFixed(1)}  cy ${cam.cy.toFixed(1)}<br>` +
    `dist: ${dstr}`;
  const rect = canvas.getBoundingClientRect();
  el.style.left = (e.clientX - rect.left + 14) + 'px';
  el.style.top  = (e.clientY - rect.top + 14) + 'px';
  el.style.display = 'block';
}
function hideTooltip() { const el = $('cam-tooltip'); if (el) el.style.display = 'none'; }
// ---------------------------------------------------------------------------
// Double-click view centering (MeshLab-style): find the 3D point under the
// cursor — GPU depth accumulation for splats, WASM brute-force raycast for
// meshes — then pan so it sits on the optical axis and make it the orbit
// pivot. Works with every camera model.
// ---------------------------------------------------------------------------
function canvasPixel(e) {
  const rect = canvas.getBoundingClientRect();
  return [ (e.clientX - rect.left) * canvas.width / rect.width,
           canvas.height - (e.clientY - rect.top) * canvas.height / rect.height ];
}
// camera-space ray direction for pixel-plane uv (matches unprojectCam in GLSL)
function unprojectDir(uv) {
  const m = camera.model;
  if (m === 'perspective' || m === 'orthographic') return v3.norm([uv[0], uv[1], -1]);
  if (m === 'equirectangular') {
    const cp = Math.cos(uv[1]);
    return [cp*Math.sin(uv[0]), Math.sin(uv[1]), -cp*Math.cos(uv[0])];
  }
  const r = Math.hypot(uv[0], uv[1]);
  const d = r > 1e-9 ? [uv[0]/r, uv[1]/r] : [0, 0];
  const th = m === 'equisolid' ? 2*Math.asin(Math.max(-1, Math.min(1, 0.5*r))) : r;
  return [d[0]*Math.sin(th), d[1]*Math.sin(th), -Math.cos(th)];
}
function pickRecenter(e) {
  if (!model) return;
  const [px, py] = canvasPixel(e);
  if (model.type === 'dataset') {
    // act on whatever is visually in front at the cursor: a camera frustum or
    // the point cloud (each only if shown), compared by depth along the ray
    const camHit = pickCameraHit(px, py);
    const ptHit = opts.showPoints ? datasetPointHit(px, py) : null;
    if (camHit && (!ptHit || camHit.t <= ptHit.t)) viewFromCamera(camHit.i);
    else if (ptHit) recenterAt(ptHit.p);
    return;
  }
  const intr = renderer._intrinsics(camera, canvas.width, canvas.height);
  const uv = [(px - intr.cx)/intr.fx, (py - intr.cy)/intr.fy];
  const R = camera.rotMat3();            // cam -> world (canonical)
  let p = null;
  if (model.type === 'splat') {
    const depth = renderer.pickSplatDepth(camera, opts, px, py);
    if (depth != null) {
      let pc;
      if (camera.model === 'orthographic') pc = [uv[0], uv[1], -depth];
      else if (camera.model === 'perspective') pc = [uv[0]*depth, uv[1]*depth, -depth]; // axial depth
      else pc = v3.scale(unprojectDir(uv), depth);                                     // ray length
      p = v3.add(camera.pos, mat3.mulVec(R, pc));
    }
  } else {
    let ro, rd;
    if (camera.model === 'orthographic') {
      ro = v3.add(camera.pos, mat3.mulVec(R, [uv[0], uv[1], 0]));
      rd = mat3.mulVec(R, [0, 0, -1]);
    } else {
      ro = camera.pos;
      rd = mat3.mulVec(R, unprojectDir(uv));
    }
    const Ut = mat3.transpose(upTransform());
    const on = mat3.mulVec(Ut, ro), dn = mat3.mulVec(Ut, rd);
    const t = raycastMesh(on[0], on[1], on[2], dn[0], dn[1], dn[2]);
    if (t > 0) p = v3.add(ro, v3.scale(rd, t));
  }
  if (p) recenterAt(p);
}
// Pan laterally so p lies on the optical axis (view orientation preserved) and
// set it as the orbit pivot; if p is behind the camera plane (possible with
// >180° cameras) rotate toward it instead.
function recenterAt(p) {
  const fwd = camera.forward();
  const rel = v3.sub(p, camera.pos);
  const d = v3.dot(rel, fwd);
  if (d > 0) camera.pos = v3.add(camera.pos, v3.sub(rel, v3.scale(fwd, d)));
  else camera.lookAt(camera.pos, p, camera.up());
  camera.target = p.slice();
  dirty = true;
}

function pinchState(pointers) {
  const [a,b] = [...pointers.values()];
  // angle convention matches the training viewer (atan2(dx, dy)) so two-finger
  // rotation rolls in the same direction
  return { dist: Math.hypot(a.x-b.x, a.y-b.y), cx:(a.x+b.x)/2, cy:(a.y+b.y)/2, ang: Math.atan2(a.x-b.x, a.y-b.y) };
}
// ---------------------------------------------------------------------------
// UI control wiring
// ---------------------------------------------------------------------------
function wireControls() {
  const bind = (id, ev, fn) => $(id).addEventListener(ev, fn);
  bind('nav-mode','change', e => nav.mode = e.target.value);
  bind('move-speed','input', e => { nav.speedExp = +e.target.value; $('v-speed').textContent = Math.pow(10,+e.target.value).toFixed(2)+'×'; });
  bind('btn-reset','click', () => { if (model) fitModel(); else nav.reset(); });

  bind('primitive','change', e => { opts.primitive = +e.target.value; dirty=true; });
  bind('gamut','change', e => { opts.gamut = toColMajor(GAMUTS[e.target.value]); dirty=true; });
  bind('linear','change', e => { opts.isLinear = e.target.checked; dirty=true; });
  bind('transfer','change', e => { opts.transfer = +e.target.value; dirty=true; });
  bind('shdeg','input', e => { opts.shDegree = +e.target.value; $('v-shdeg').textContent = e.target.value; dirty=true; });
  bind('exposure','input', e => { opts.exposure = Math.pow(10,+e.target.value); $('v-exposure').textContent = opts.exposure.toFixed(2); dirty=true; });
  bind('shade','change', e => { opts.shade = e.target.checked; dirty=true; });

  bind('camera-model','change', e => {
    const prev = opts.cameraModel;
    if (FOV_RANGE[prev]) fovMemory[prev] = camera.fov;   // remember the outgoing model's FOV
    opts.cameraModel = e.target.value; camera.model = e.target.value;
    // FOV is meaningless for equirectangular (full 360×180)
    $('fov').disabled = (e.target.value === 'equirectangular');
    $('fov').parentElement.style.opacity = $('fov').disabled ? 0.4 : 1;
    // Per-model FOV range (fisheye reaches past 180°; pinhole must not).
    // Switching to a model restores its remembered FOV; a model used for the
    // first time inherits the current FOV clamped into its range — e.g. a
    // 213° fisheye view switched back to a never-adjusted perspective would
    // otherwise keep an impossible FOV.
    // Equirectangular has no range (slider disabled, camera.fov untouched).
    const range = FOV_RANGE[e.target.value];
    if (range) {
      const [lo, hi] = range;
      $('fov').min = lo; $('fov').max = hi;
      const mem = fovMemory[e.target.value];
      camera.fov = Math.min(Math.max(mem != null ? mem : camera.fov, lo*Math.PI/180), hi*Math.PI/180);
      const deg = Math.round(camera.fov*180/Math.PI);   // slider is integer-stepped
      $('fov').value = deg; $('v-fov').textContent = deg + '°';
    }
    dirty=true;
  });
  bind('fov','input', e => { camera.fov = +e.target.value*Math.PI/180; fovMemory[camera.model] = camera.fov; $('v-fov').textContent = e.target.value+'°'; dirty=true; });
  bind('up-axis','change', e => setUpAxis(e.target.checked ? 'y' : 'z'));
  bind('flat-shade','change', e => { opts.flatShade = e.target.checked; dirty=true; });
  bind('mesh-color','change', e => { opts.meshColor = e.target.checked; dirty=true; });
  bind('grid','change', e => { opts.showGrid = e.target.checked; dirty=true; });
  bind('bg-color','input', e => { opts.background = hexToRgb(e.target.value); dirty=true; });

  // dataset controls
  bind('ds-component','change', async e => {
    if (!dataset) return;
    try { await loadDataset(null, e.target.value); }
    catch (err) { console.error(err); setStatus('Load error: ' + err.message, 'err'); showToast('Load error: ' + err.message); }
  });
  bind('point-size','input', e => { opts.pointSize = +e.target.value; $('v-point-size').textContent = (+e.target.value).toFixed(1); dirty=true; });
  bind('frustum-size','input', e => {
    const mult = Math.pow(10, +e.target.value);
    if (dataset) { dataset.frustumMult = mult; opts.frustumScale = dataset.frustumBase * mult; }
    $('v-frustum-size').textContent = mult.toFixed(2)+'×'; dirty=true;
  });
  bind('show-points','change', e => { opts.showPoints = e.target.checked; dirty=true; });
  bind('show-frustums','change', e => { opts.showFrustums = e.target.checked; dirty=true; });

  bind('hist-param','change', () => computeHistogram());
  bind('btn-hist','click', () => computeHistogram());
  bind('panel-toggle','click', () => $('panel').classList.toggle('collapsed'));

  updateNavLegend();
  $('nav-mode').addEventListener('change', updateNavLegend);
}

// Read every control's current DOM value and apply it. Browsers (Firefox in
// particular) restore form values on refresh without firing events, which
// would leave opts/camera out of sync; dispatching the events fixes that.
function syncControls() {
  const panel = $('panel');
  panel.querySelectorAll('select, input[type=color]').forEach(el => el.dispatchEvent(new Event(el.tagName==='SELECT'?'change':'input')));
  panel.querySelectorAll('input[type=range]').forEach(el => el.dispatchEvent(new Event('input')));
  panel.querySelectorAll('input[type=checkbox]').forEach(el => el.dispatchEvent(new Event('change')));
}
function updateNavLegend() {
  const m = $('nav-mode').value;
  const L = {
    turntable: 'LMB Orbit · MMB/Shift Pan · Wheel Zoom · DblClick Center',
    trackball: 'LMB Rotate · MMB/Shift Pan · Wheel Zoom · DblClick Center',
    fps: 'WASD Move · E/Q Up/Down · LMB Look · DblClick Center',
    fly: 'WASD Move · E/Q Roll · LMB Look · DblClick Center',
  };
  $('nav-legend').textContent = L[m];
}

boot();
