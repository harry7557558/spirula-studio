// WASM bridge + model loading. Wraps the native parser (ssv_wasm) and adds the
// JS-side GLTF/GLB path. Large PLY/OBJ files are streamed directly into the
// WASM heap (chunked) so we never allocate a >2GB JS ArrayBuffer.

let Module = null;

export async function initWasm() {
  const factory = (await import('./ssv_wasm.js')).default;
  Module = await factory({
    locateFile: (p) => new URL(p, import.meta.url).href,
  });
  return Module;
}

// WASM pointers come back through ccall as SIGNED i32; once the heap grows
// past 2GB they read as negative numbers — always normalize with >>> 0.
const C = {
  parse:   (ptr,len,hint)=>Module.ccall('ssv_parse','number',['number','number','number'],[ptr,len,hint]),
  alloc:   (n)=>Module.ccall('ssv_alloc','number',['number'],[n]) >>> 0,
  free:    (p)=>Module.ccall('ssv_free',null,['number'],[p]),
};
function call(name, ret='number', args=[], vals=[]) { return Module.ccall(name, ret, args, vals); }
function f32(ptr, n) { return new Float32Array(Module.HEAPF32.buffer, ptr >>> 0, n); }
function u32(ptr, n) { return new Uint32Array(Module.HEAPU32.buffer, ptr >>> 0, n); }
function u16(ptr, n) { return new Uint16Array(Module.HEAPU16.buffer, ptr >>> 0, n); }
function u8(ptr, n)  { return new Uint8Array(Module.HEAPU8.buffer, ptr >>> 0, n); }
function i8(ptr, n)  { return new Int8Array(Module.HEAPU8.buffer, ptr >>> 0, n); }

// ---- stream a File into the WASM heap; returns {ptr, len} ----
async function fileToHeap(file) {
  const len = file.size;
  const ptr = C.alloc(len);
  if (!ptr) throw new Error(`out of memory allocating ${len} bytes`);
  const reader = file.stream().getReader();
  let off = 0;
  while (true) {
    const { done, value } = await reader.read();
    if (done) break;
    // HEAPU8 view may change if the heap grows elsewhere; re-fetch each chunk
    new Uint8Array(Module.HEAPU8.buffer).set(value, ptr + off);
    off += value.length;
  }
  return { ptr, len };
}

// ---- extract splat views (into heap) after a successful parse ----
// posop is f32; quat/scale/fdc are IEEE half floats (Uint16Array views,
// uploaded to RGBA16F textures as HALF_FLOAT with zero conversion). scale is
// 4-strided: [3] holds the per-splat SH quantization scale. sh is snorm8
// (Int8Array, uploaded to RGB8_SNORM): coeff = (sh/127) * scale[i*4+3].
// sb is the spherical-beta lobes, two RGBA16F texels each: (axis, sharpness)
// and (amplitude, 0), already activated.
function readSplat() {
  const count = call('ssv_splat_count');
  const shDegree = call('ssv_splat_sh_degree');
  const shRest = call('ssv_splat_sh_rest');
  const sbLobes = call('ssv_splat_sb_lobes');
  const data = {
    count, shDegree, shRest, sbLobes,
    posop: f32(call('ssv_splat_posop'), count*4),
    quat:  u16(call('ssv_splat_quat'), count*4),
    scale: u16(call('ssv_splat_scale'), count*4),
    fdc:   u16(call('ssv_splat_fdc'), count*3),
    sh:    shRest>0 ? i8(call('ssv_splat_sh'), count*shRest*3) : null,
    sb:    sbLobes>0 ? u16(call('ssv_splat_sb'), count*sbLobes*8) : null,
  };
  return data;
}
function readMesh() {
  const nv = call('ssv_mesh_nv'), nt = call('ssv_mesh_nt');
  const nrm = call('ssv_mesh_normal'), col = call('ssv_mesh_color'), uv = call('ssv_mesh_uv');
  return {
    nv, nt,
    pos: f32(call('ssv_mesh_pos'), nv*3),
    normal: nrm ? f32(nrm, nv*3) : null,
    color: col ? u8(col, nv*3) : null,
    uv: uv ? f32(uv, nv*2) : null,
    idx: u32(call('ssv_mesh_idx'), nt*3),
  };
}

// ---- streaming parse for binary-LE PLY: the file is fed through a small
// chunk buffer, so multi-GB models never need file + parsed data resident in
// the heap at once. Returns null if the input should use the whole-buffer
// fallback (not a PLY, ascii PLY, ...) — in that case the rest reader has
// not been touched. getRestReader() supplies the bytes AFTER headBuf (or
// null if headBuf already holds the whole file). ----
async function tryStreamPlyParts(headBuf, getRestReader) {
  if (headBuf.length < 16 || headBuf[0]!==0x70 || headBuf[1]!==0x6c || headBuf[2]!==0x79) return null; // 'ply'
  const text = new TextDecoder('latin1').decode(headBuf);
  const m = text.indexOf('end_header');
  if (m < 0) return null;
  let hdrEnd = m + 'end_header'.length;
  if (text[hdrEnd] === '\r') hdrEnd++;
  if (text[hdrEnd] === '\n') hdrEnd++;
  const hp = C.alloc(hdrEnd);
  new Uint8Array(Module.HEAPU8.buffer).set(headBuf.subarray(0, hdrEnd), hp);
  const mode = call('ssv_stream_begin','number',['number','number'],[hp, hdrEnd]);
  C.free(hp);
  if (mode !== 1 && mode !== 2) return null;   // unsupported for streaming -> fallback

  let buf = 0, cap = 0, err = 0;
  const feed = (bytes) => {
    if (!bytes.length) return 0;
    if (bytes.length > cap) { if (buf) C.free(buf); cap = bytes.length; buf = C.alloc(cap); }
    new Uint8Array(Module.HEAPU8.buffer).set(bytes, buf);
    return call('ssv_stream_chunk','number',['number','number'],[buf, bytes.length]);
  };
  err = feed(headBuf.subarray(hdrEnd));
  const reader = err ? null : await getRestReader();
  if (reader) {
    while (!err) {
      const { done, value } = await reader.read();
      if (done) break;
      err = feed(value);
    }
  }
  if (buf) C.free(buf);
  const kind = call('ssv_stream_end');
  if (err) throw new Error('failed to parse PLY (bad face data)');
  if (kind === 1) return { kind:'splat', data: readSplat() };
  if (kind === 2) return { kind:'mesh', data: readMesh() };
  throw new Error('failed to parse PLY (truncated file?)');
}

async function tryStreamPly(file) {
  const headBuf = new Uint8Array(await file.slice(0, 1<<20).arrayBuffer());
  return tryStreamPlyParts(headBuf, async () =>
    file.size > headBuf.length ? file.slice(headBuf.length).stream().getReader() : null);
}

// ---- streaming load from a URL (deep-linked models): the response body is
// fed straight into the streaming parser, so a multi-GB hosted PLY is never
// buffered as a Blob/File (Chrome's fetch().blob() fails outright on files
// this large). Non-streamable payloads fall back to a (small) buffered File.
export async function loadModelFromUrl(url, name) {
  const resp = await fetch(url);
  if (!resp.ok || !resp.body) throw new Error(`fetch failed (${resp.status})`);
  const reader = resp.body.getReader();
  let head = new Uint8Array(0), exhausted = false;
  while (head.length < (1<<20)) {
    const { done, value } = await reader.read();
    if (done) { exhausted = true; break; }
    const nh = new Uint8Array(head.length + value.length);
    nh.set(head); nh.set(value, head.length); head = nh;
  }
  const res = await tryStreamPlyParts(head, async () => exhausted ? null : reader);
  if (res) return res;
  try { reader.cancel(); } catch {}
  const file = new File([await (await fetch(url)).blob()], name);
  return await loadNative(file, 0);
}

// ---- public: parse a PLY / OBJ file via WASM ----
async function loadNative(file, hint) {
  if (hint === 0) {
    const res = await tryStreamPly(file);
    if (res) return res;
  }
  const { ptr, len } = await fileToHeap(file);
  const kind = C.parse(ptr, len, hint);
  C.free(ptr);
  if (kind === 1) return { kind:'splat', data: readSplat() };
  if (kind === 2) return { kind:'mesh', data: readMesh() };
  throw new Error('failed to parse file (unrecognized format)');
}

// register a JS-built mesh (from GLTF) with WASM for stats/histograms/bbox
function registerMesh(m) {
  const nv = m.nv, nt = m.nt;
  const pp = C.alloc(nv*3*4); new Float32Array(Module.HEAPF32.buffer, pp, nv*3).set(m.pos);
  let np=0, cp=0, up=0;
  if (m.normal) { np = C.alloc(nv*3*4); new Float32Array(Module.HEAPF32.buffer, np, nv*3).set(m.normal); }
  if (m.color)  { cp = C.alloc(nv*3);   new Uint8Array(Module.HEAPU8.buffer, cp, nv*3).set(m.color); }
  if (m.uv)     { up = C.alloc(nv*2*4); new Float32Array(Module.HEAPF32.buffer, up, nv*2).set(m.uv); }
  const ip = C.alloc(nt*3*4); new Uint32Array(Module.HEAPU32.buffer, ip, nt*3).set(m.idx);
  Module.ccall('ssv_set_mesh', null,
    ['number','number','number','number','number','number','number'],
    [nv, nt, pp, np, cp, up, ip]);
  C.free(pp); if(np)C.free(np); if(cp)C.free(cp); if(up)C.free(up); C.free(ip);
}

// ---------------------------------------------------------------------------
// GLTF / GLB (pure JS). Only the attributes the mesh exporter emits are needed but the
// reader is written generally for robustness.
// ---------------------------------------------------------------------------
const COMP = { 5120:Int8Array, 5121:Uint8Array, 5122:Int16Array, 5123:Uint16Array, 5125:Uint32Array, 5126:Float32Array };
const NCOMP = { SCALAR:1, VEC2:2, VEC3:3, VEC4:4, MAT4:16 };

function accessorArray(gltf, buffers, idx) {
  const acc = gltf.accessors[idx];
  const view = gltf.bufferViews[acc.bufferView];
  const buf = buffers[view.buffer];
  const compCtor = COMP[acc.componentType];
  const nc = NCOMP[acc.type];
  const byteOffset = (view.byteOffset||0) + (acc.byteOffset||0);
  const count = acc.count;
  const elemBytes = compCtor.BYTES_PER_ELEMENT * nc;
  const stride = view.byteStride || elemBytes;
  const out = new Float32Array(count*nc);
  const dv = new DataView(buf);
  const getters = {
    5120:(o)=>Math.max(dv.getInt8(o)/127,-1), 5121:(o)=>dv.getUint8(o)/255,
    5122:(o)=>Math.max(dv.getInt16(o,true)/32767,-1), 5123:(o)=>dv.getUint16(o,true)/65535,
    5125:(o)=>dv.getUint32(o,true), 5126:(o)=>dv.getFloat32(o,true),
  };
  const raw = {
    5120:(o)=>dv.getInt8(o), 5121:(o)=>dv.getUint8(o), 5122:(o)=>dv.getInt16(o,true),
    5123:(o)=>dv.getUint16(o,true), 5125:(o)=>dv.getUint32(o,true), 5126:(o)=>dv.getFloat32(o,true),
  };
  const get = acc.normalized ? getters[acc.componentType] : raw[acc.componentType];
  for (let i=0;i<count;i++)
    for (let c=0;c<nc;c++)
      out[i*nc+c] = get(byteOffset + i*stride + c*compCtor.BYTES_PER_ELEMENT);
  return { array: out, nc, count };
}

function indexArray(gltf, buffers, idx) {
  const acc = gltf.accessors[idx];
  const view = gltf.bufferViews[acc.bufferView];
  const buf = buffers[view.buffer];
  const ctor = COMP[acc.componentType];
  const byteOffset = (view.byteOffset||0) + (acc.byteOffset||0);
  return new ctor(buf, byteOffset, acc.count);
}

function srgbEncode(x){ return x<0.0031308 ? 12.92*x : 1.055*Math.pow(x,1/2.4)-0.055; }

async function parseGLTF(gltf, buffers, resolveImage) {
  // gather primitives across all meshes/nodes (identity transforms in practice)
  const positions=[], normals=[], uvs=[], colors=[], indices=[];
  let hasN=false, hasUV=false, hasC=false;
  let vbase=0;
  let texSource = -1;
  const nodes = gltf.nodes||[];
  const meshUsed = new Set();
  const meshList = [];
  const walk = (ni) => {
    const node = nodes[ni]; if(!node) return;
    if (node.mesh != null) meshList.push(node.mesh);
    (node.children||[]).forEach(walk);
  };
  if (gltf.scenes && gltf.scenes[gltf.scene??0]) (gltf.scenes[gltf.scene??0].nodes||[]).forEach(walk);
  if (meshList.length===0) gltf.meshes?.forEach((_,i)=>meshList.push(i));

  for (const mi of meshList) {
    const mesh = gltf.meshes[mi];
    for (const prim of mesh.primitives) {
      if (prim.mode != null && prim.mode !== 4) continue; // triangles only
      const attr = prim.attributes;
      const pos = accessorArray(gltf, buffers, attr.POSITION);
      const nV = pos.count;
      for (let i=0;i<nV*3;i++) positions.push(pos.array[i]);
      if (attr.NORMAL!=null){ hasN=true; const a=accessorArray(gltf,buffers,attr.NORMAL); for(let i=0;i<nV*3;i++) normals.push(a.array[i]); }
      else for(let i=0;i<nV*3;i++) normals.push(0);
      if (attr.TEXCOORD_0!=null){ hasUV=true; const a=accessorArray(gltf,buffers,attr.TEXCOORD_0); for(let i=0;i<nV*2;i++) uvs.push(a.array[i]); }
      else for(let i=0;i<nV*2;i++) uvs.push(0);
      if (attr.COLOR_0!=null){ hasC=true; const a=accessorArray(gltf,buffers,attr.COLOR_0); const s=a.nc;
        for(let i=0;i<nV;i++){ // COLOR_0 is linear; encode to sRGB u8
          colors.push(Math.round(255*Math.min(1,Math.max(0,srgbEncode(a.array[i*s]||0)))));
          colors.push(Math.round(255*Math.min(1,Math.max(0,srgbEncode(a.array[i*s+1]||0)))));
          colors.push(Math.round(255*Math.min(1,Math.max(0,srgbEncode(a.array[i*s+2]||0)))));
        } }
      else for(let i=0;i<nV*3;i++) colors.push(200);
      // indices
      if (prim.indices!=null){ const ia=indexArray(gltf,buffers,prim.indices); for(let i=0;i<ia.length;i++) indices.push(vbase+ia[i]); }
      else for(let i=0;i<nV;i++) indices.push(vbase+i);
      vbase += nV;
      // texture
      if (texSource<0 && prim.material!=null){
        const mat=gltf.materials?.[prim.material];
        const ti=mat?.pbrMetallicRoughness?.baseColorTexture?.index ?? mat?.emissiveTexture?.index;
        if (ti!=null){ const tex=gltf.textures[ti]; if(tex && tex.source!=null) texSource=tex.source; }
      }
    }
  }
  const m = {
    nv: positions.length/3, nt: indices.length/3,
    pos: new Float32Array(positions),
    normal: hasN ? new Float32Array(normals) : null,
    uv: hasUV ? new Float32Array(uvs) : null,
    color: hasC ? new Uint8Array(colors) : null,
    idx: new Uint32Array(indices),
  };
  let image = null;
  if (texSource >= 0 && gltf.images && gltf.images[texSource]) {
    image = await resolveImage(gltf.images[texSource]);
  }
  return { mesh: m, image };
}

function parseGLB(buffer) {
  const dv = new DataView(buffer);
  if (dv.getUint32(0,true) !== 0x46546C67) throw new Error('bad GLB magic');
  let off = 12, json=null, bin=null;
  while (off < dv.byteLength) {
    const len = dv.getUint32(off,true), type = dv.getUint32(off+4,true); off+=8;
    if (type===0x4E4F534A) json = JSON.parse(new TextDecoder().decode(new Uint8Array(buffer, off, len)));
    else if (type===0x004E4942) bin = buffer.slice(off, off+len);
    off += len;
    if (len % 4) off += 4 - (len%4);
  }
  return { json, bin };
}

async function loadGLTFFromFiles(mainFile, siblings) {
  const name = mainFile.name.toLowerCase();
  const findSibling = (uri) => {
    const base = decodeURIComponent(uri.split('/').pop());
    return siblings.find(f => f.name === base || f.name.endsWith('/'+base));
  };
  let gltf, buffers = [], glbBin = null;
  if (name.endsWith('.glb')) {
    const { json, bin } = parseGLB(await mainFile.arrayBuffer());
    gltf = json; glbBin = bin;
    for (const b of gltf.buffers||[]) {
      if (b.uri == null) buffers.push(glbBin);
      else buffers.push(await uriToArrayBuffer(b.uri, findSibling));
    }
  } else {
    gltf = JSON.parse(await mainFile.text());
    for (const b of gltf.buffers||[]) buffers.push(await uriToArrayBuffer(b.uri, findSibling));
  }
  const resolveImage = async (img) => {
    let blob;
    if (img.bufferView != null) {
      const view = gltf.bufferViews[img.bufferView];
      const buf = buffers[view.buffer];
      blob = new Blob([new Uint8Array(buf, view.byteOffset||0, view.byteLength)], { type: img.mimeType||'image/png' });
    } else if (img.uri) {
      if (img.uri.startsWith('data:')) blob = await (await fetch(img.uri)).blob();
      else { const f = findSibling(img.uri); if(!f) return null; blob = f; }
    } else return null;
    try { return await createImageBitmap(blob); }
    catch { return null; }
  };
  const { mesh, image } = await parseGLTF(gltf, buffers, resolveImage);
  registerMesh(mesh);
  return { kind:'mesh', data: mesh, image };
}

async function uriToArrayBuffer(uri, findSibling) {
  if (uri.startsWith('data:')) return await (await fetch(uri)).arrayBuffer();
  const f = findSibling(uri);
  if (!f) throw new Error(`missing external file referenced by GLTF: ${uri}`);
  return await f.arrayBuffer();
}

// ---------------------------------------------------------------------------
// OBJ external resources (mtl + texture image)
// ---------------------------------------------------------------------------
async function loadOBJTexture(objFile, text, siblings) {
  // find mtllib, then map_Kd within it
  const mtlMatch = text.match(/^mtllib\s+(.+)$/m);
  if (!mtlMatch) return null;
  const mtlName = mtlMatch[1].trim();
  const mtlFile = siblings.find(f => f.name === mtlName || f.name.endsWith('/'+mtlName));
  if (!mtlFile) return null;
  const mtl = await mtlFile.text();
  const map = mtl.match(/^\s*map_Kd\s+(.+)$/m);
  if (!map) return null;
  const texName = map[1].trim().split(/[\\/]/).pop();
  const texFile = siblings.find(f => f.name === texName || f.name.endsWith('/'+texName));
  if (!texFile) return null;
  try { return await createImageBitmap(texFile); } catch { return null; }
}

// ---------------------------------------------------------------------------
// Top-level: given the dropped/selected FileList, load a model.
// ---------------------------------------------------------------------------
export async function loadModel(files) {
  const list = Array.from(files);
  // pick the main model file
  const exts = ['.ply','.glb','.gltf','.obj','.stl'];
  const main = list.find(f => exts.some(e => f.name.toLowerCase().endsWith(e)));
  if (!main) throw new Error('no supported model file found (.ply/.obj/.gltf/.glb/.stl)');
  if (main.urlStream) return await loadModelFromUrl(main.urlStream, main.name);
  const lname = main.name.toLowerCase();
  if (lname.endsWith('.gltf') || lname.endsWith('.glb')) {
    return await loadGLTFFromFiles(main, list);
  }
  if (lname.endsWith('.obj')) {
    // mtllib sits in the file header — decoding the entire (possibly 100+ MB)
    // OBJ into a JS string just to find it took seconds and doubled memory.
    const head = await main.slice(0, 1 << 18).text();
    const image = await loadOBJTexture(main, head, list);
    // parse geometry via WASM (needs raw bytes)
    const res = await loadNative(main, 1);
    res.image = image;
    return res;
  }
  if (lname.endsWith('.stl')) return await loadNative(main, 2);
  // PLY (splat or mesh, auto-detected)
  return await loadNative(main, 0);
}

// ---------------------------------------------------------------------------
// Runtime queries backed by WASM (operate on the currently loaded model)
// ---------------------------------------------------------------------------
// mode: 0 planar (perspective/ortho), 1 distance (equirect), 2 fisheye-blend
export function sortSplats(mode, cx, cy, cz, dx, dy, dz) {
  const ptr = Module.ccall('ssv_sort','number',
    ['number','number','number','number','number','number','number'],
    [mode, cx, cy, cz, dx, dy, dz]) >>> 0;
  const n = call('ssv_splat_count');
  if (!ptr || !n) return null;
  return u32(ptr, n);
}
// free the big SH array (and quats) after their texture upload — sorting,
// histograms and picking never read them
export function freeSplatSh() { call('ssv_free_splat_sh', null); }
// drop the rest-SH degree in place (GPU-OOM fallback) and return fresh views
export function reduceSh(newDegree) {
  call('ssv_reduce_sh', 'number', ['number'], [newDegree]);
  return readSplat();
}
export function splatHistogram(param, nbins) {
  const ptr = call('ssv_splat_histogram','number',['number','number'],[param,nbins]);
  const mm = f32(call('ssv_histogram_minmax'), 2);
  return { bins: f32(ptr, nbins).slice(), min: mm[0], max: mm[1] };
}
export function meshHistogram(param, nbins) {
  const ptr = call('ssv_mesh_histogram','number',['number','number'],[param,nbins]);
  const mm = f32(call('ssv_histogram_minmax'), 2);
  return { bins: f32(ptr, nbins).slice(), min: mm[0], max: mm[1] };
}
export function meshEdgeCount() { return call('ssv_mesh_edge_count'); }
export function bbox() { return f32(call('ssv_bbox'), 6).slice(); }
// fit sphere about centering mode `mode` (index.html #center-mode):
// [cx, cy, cz, medianDistance]
export function fitSphere(mode = 2) { return f32(call('ssv_fit_sphere', 'number', ['number'], [mode|0]), 4).slice(); }
// nearest ray/mesh hit distance (model-native frame), or -1
export function raycastMesh(ox, oy, oz, dx, dy, dz) {
  return call('ssv_raycast_mesh','number',
    ['number','number','number','number','number','number'], [ox,oy,oz,dx,dy,dz]);
}
export function freeSplat() { call('ssv_free_splat', null); }
export function freeMesh() { call('ssv_free_mesh', null); }

// ---------------------------------------------------------------------------
// Dataset bridge (COLMAP / Nerfstudio / Metashape). Files are mounted into
// Emscripten's MEMFS under /data; the native parsers (dataset_bridge.cpp) run
// against that virtual filesystem. Only metadata files are mounted — image
// pixels are never copied (the parsers only need poses/points).
// ---------------------------------------------------------------------------
const DS_META_EXT = new Set(['bin','json','ply','xml','psx','txt','zip']);

function fsRmrf(path) {
  const FS = Module.FS;
  let st; try { st = FS.stat(path); } catch { return; }
  if (FS.isDir(st.mode)) {
    for (const e of FS.readdir(path)) {
      if (e === '.' || e === '..') continue;
      fsRmrf(path + '/' + e);
    }
    try { FS.rmdir(path); } catch {}
  } else { try { FS.unlink(path); } catch {} }
}
function fsMkdirp(path) {
  let cur = '';
  for (const p of path.split('/')) {
    if (!p) continue;
    cur += '/' + p;
    try { Module.FS.mkdir(cur); } catch {}
  }
}

// entries: [{path, getFile}] (path relative, forward slashes; getFile
// materializes the File lazily). Extension-filters BEFORE materializing, so a
// dataset folder full of high-resolution images never stats or reads them.
// Returns #mounted.
export async function dsMount(entries) {
  fsRmrf('/data');
  try { Module.FS.mkdir('/data'); } catch {}
  let n = 0;
  for (const entry of entries) {
    const rel = String(entry.path).replace(/^\/+/, '').replace(/\\/g, '/');
    const ext = (rel.split('.').pop() || '').toLowerCase();
    if (!DS_META_EXT.has(ext)) continue;
    const file = await entry.getFile();
    const dst = '/data/' + rel;
    const slash = dst.lastIndexOf('/');
    if (slash > 0) fsMkdirp(dst.slice(0, slash));
    const buf = new Uint8Array(await file.arrayBuffer());
    Module.FS.writeFile(dst, buf);
    n++;
  }
  return n;
}

export function dsEnumerate() {
  call('ssv_ds_enumerate');
  const json = Module.ccall('ssv_ds_json', 'string', [], []);
  try { return JSON.parse(json); } catch { return []; }
}
export function dsParse(token) {
  return Module.ccall('ssv_ds_parse', 'number', ['string'], [token]);
}
// kCameraDistortionParams (core/CameraModel.h)
const DIST_PARAMS = 8;
export function dsReadCameras() {
  const n = call('ssv_ds_num_cameras');
  const out = [];
  if (!n) return out;
  const names = Module.ccall('ssv_ds_image_names', 'string', [], []).split('\n');
  const heldOut = new Set(new Int32Array(Module.HEAP32.buffer, call('ssv_ds_validation') >>> 0, call('ssv_ds_num_validation')));
  const models  = new Int32Array(Module.HEAP32.buffer, call('ssv_ds_cam_models') >>> 0, n);
  const intr    = f32(call('ssv_ds_cam_intrins') >>> 0, n*4);
  const distPtr = call('ssv_ds_cam_dist') >>> 0;
  const dist    = distPtr ? f32(distPtr, n*DIST_PARAMS) : null;
  const dtPtr   = call('ssv_ds_cam_dist_types') >>> 0;
  const dtypes  = dtPtr ? new Int32Array(Module.HEAP32.buffer, dtPtr, n) : null;
  const c2w     = f32(call('ssv_ds_cam_c2w') >>> 0, n*12);
  const widths  = new Int32Array(Module.HEAP32.buffer, call('ssv_ds_cam_widths') >>> 0, n);
  const heights = new Int32Array(Module.HEAP32.buffer, call('ssv_ds_cam_heights') >>> 0, n);
  for (let i = 0; i < n; i++) {
    out.push({
      model: models[i],
      heldOut: heldOut.has(i),
      fx: intr[i*4], fy: intr[i*4+1], cx: intr[i*4+2], cy: intr[i*4+3],
      distType: dtypes ? dtypes[i] : 0,
      dist: dist ? Array.from(dist.subarray(i*DIST_PARAMS, (i+1)*DIST_PARAMS)) : new Array(DIST_PARAMS).fill(0),
      c2w: Array.from(c2w.subarray(i*12, i*12+12)),
      w: widths[i], h: heights[i],
      name: names[i] || '',
    });
  }
  return out;
}
// Live heap views — upload to the GPU before any further wasm allocation.
export function dsReadPoints() {
  const np = call('ssv_ds_num_points');
  if (!np) return { count: 0, xyz: null, rgb: null };
  const xyzP = call('ssv_ds_points_xyz') >>> 0;
  const rgbP = call('ssv_ds_points_rgb') >>> 0;
  return { count: np, xyz: f32(xyzP, np*3), rgb: rgbP ? u8(rgbP, np*3) : null };
}
export function dsSummary() {
  const json = Module.ccall('ssv_ds_summary_json', 'string', [], []);
  try { return JSON.parse(json); } catch { return {}; }
}
// last parse error ("" = clean load; non-empty with a loaded dataset = partial)
export function dsLastError() {
  return Module.ccall('ssv_ds_last_error', 'string', [], []);
}
export function dsFitSphere(mode = 2) { return f32(call('ssv_ds_fit_sphere', 'number', ['number'], [mode|0]) >>> 0, 4).slice(); }
export function dsFrustumSize() { return call('ssv_ds_frustum_size'); }
export function dsPickPoint(ox, oy, oz, dx, dy, dz) {
  const ptr = Module.ccall('ssv_ds_pick_point', 'number',
    ['number','number','number','number','number','number'], [ox,oy,oz,dx,dy,dz]) >>> 0;
  const p = f32(ptr, 3);
  return { index: p[0]|0, t: p[1], perp: p[2] };
}
export function dsFree() { call('ssv_ds_free'); }

// 0 fly, 1 orbit about the camera median, 2 orbit about the camera focus.
export function dsNavigation() { return call('ssv_ds_navigation'); }
