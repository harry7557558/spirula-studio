// Scan worker for the IMU/GPS tool. Hosts its own instance of the telemetry
// WASM module (ssv_telemetry, built from src/sfm/core/Telemetry.cpp) and
// answers the parser's reads from a File slice with FileReaderSync — so a
// capture far larger than any ArrayBuffer the browser will hand out is still
// parsed in full, and nothing is uploaded anywhere.
//
// Protocol:
//   {type:'scan', id, path, file}
//     -> {type:'progress', id, stage} while working
//     -> {type:'result', id, ...} | {type:'empty', id, reason}

let Module = null;
const fr = new FileReaderSync();

// Small block cache: the sample tables and the telemetry payloads are read in
// many short, mostly forward runs, and one FileReaderSync call per read made
// a 12-minute GoPro take tens of thousands of slices.
const BLOCK = 1 << 20;
const MAX_BLOCKS = 64;
let file = null;
const blocks = new Map();

function blockAt(i) {
  let b = blocks.get(i);
  if (b !== undefined) { blocks.delete(i); blocks.set(i, b); return b; }
  const start = i * BLOCK;
  b = new Uint8Array(fr.readAsArrayBuffer(file.slice(start, Math.min(file.size, start + BLOCK))));
  blocks.set(i, b);
  if (blocks.size > MAX_BLOCKS) blocks.delete(blocks.keys().next().value);
  return b;
}

function hostRead(off, ptr, n) {
  try {
    if (!file || off < 0 || off + n > file.size) return false;
    const heap = Module.HEAPU8;
    if (n >= BLOCK) {
      heap.set(new Uint8Array(fr.readAsArrayBuffer(file.slice(off, off + n))), ptr);
      return true;
    }
    let done = 0;
    while (done < n) {
      const p = off + done, i = Math.floor(p / BLOCK), o = p - i * BLOCK;
      const b = blockAt(i);
      const take = Math.min(n - done, b.length - o);
      if (take <= 0) return false;
      heap.set(b.subarray(o, o + take), ptr + done);
      done += take;
    }
    return true;
  } catch (e) {
    return false;
  }
}

const boot = (async () => {
  const factory = (await import('../ssv_telemetry.js')).default;
  Module = await factory({ locateFile: (p) => new URL('../' + p, import.meta.url).href });
  Module.sstRead = hostRead;
})().catch((e) => { self.postMessage({ type: 'fatal', reason: String(e && e.stack || e) }); throw e; });

const C = {
  scan:  (size) => Module.ccall('sst_scan', 'number', ['number'], [size]),
  json:  () => Module.ccall('sst_json', 'string', [], []),
  report:() => Module.ccall('sst_report', 'string', [], []),
  error: () => Module.ccall('sst_error', 'string', [], []),
  ptr:   (w) => Module.ccall('sst_stream', 'number', ['number'], [w]) >>> 0,
  count: (w) => Module.ccall('sst_stream_count', 'number', ['number'], [w]),
  alloc: (n) => Module.ccall('sst_alloc', 'number', ['number'], [n]) >>> 0,
  dealloc: (p) => Module.ccall('sst_free', null, ['number'], [p]),
  exif:  (p, n) => Module.ccall('sst_exif', 'string', ['number', 'number'], [p, n]),
};

const STREAMS = ['gyro', 'accel', 'gravity', 'magnet', 'orientation', 'gps', 'exposure'];
const STRIDE  = [4, 4, 4, 4, 5, 9, 4];

function readStreams() {
  const out = {};
  const transfer = [];
  for (let w = 0; w < STREAMS.length; w++) {
    const n = C.count(w);
    if (!n) continue;
    const ptr = C.ptr(w);
    const len = n * STRIDE[w];
    const src = w === 5 ? new Float64Array(Module.HEAPF64.buffer, ptr, len)
                        : new Float32Array(Module.HEAPF32.buffer, ptr, len);
    const copy = src.slice();
    out[STREAMS[w]] = copy;
    transfer.push(copy.buffer);
  }
  return { streams: out, transfer };
}

function scanImage(f) {
  const n = Math.min(f.size, 1 << 18);
  const bytes = new Uint8Array(fr.readAsArrayBuffer(f.slice(0, n)));
  const ptr = C.alloc(n);
  if (!ptr) return null;
  Module.HEAPU8.set(bytes, ptr);
  let exif = null;
  try { exif = JSON.parse(C.exif(ptr, n)); } catch (e) { exif = null; }
  C.dealloc(ptr);
  return exif;
}

// One file at a time: the parser reads through module-global state, and two
// handlers interleaving at their first await would swap files under each other.
let queue = Promise.resolve();
self.onmessage = (ev) => {
  if (ev.data && ev.data.type === 'scan') queue = queue.then(() => handle(ev.data));
};

async function handle(m) {
  await boot;
  const { id, path } = m;
  file = m.file;
  blocks.clear();
  try {
    const head = new Uint8Array(fr.readAsArrayBuffer(file.slice(0, Math.min(file.size, 12))));
    const isJpeg = head.length > 3 && head[0] === 0xFF && head[1] === 0xD8 && head[2] === 0xFF;
    if (isJpeg) {
      const exif = scanImage(file);
      const exposed = exif && (exif.exposureTime > 0 || exif.fNumber > 0 || exif.iso > 0);
      if (!exif || !(exif.hasGps || exposed)) { self.postMessage({ type: 'empty', id, reason: 'no EXIF GPS or exposure' }); }
      else self.postMessage({ type: 'result', id, path, kind: 'image', exif,
                              size: file.size, lastModified: file.lastModified });
    } else {
      const rc = C.scan(file.size);
      if (rc < 0) { self.postMessage({ type: 'empty', id, reason: C.error() }); }
      else if (rc === 0) { self.postMessage({ type: 'empty', id, reason: 'no telemetry in this container' }); }
      else {
        const meta = JSON.parse(C.json());
        const report = C.report();
        const { streams, transfer } = readStreams();
        self.postMessage({ type: 'result', id, path, kind: 'video', meta, report, streams,
                           size: file.size, lastModified: file.lastModified }, transfer);
      }
    }
  } catch (e) {
    self.postMessage({ type: 'empty', id, reason: String(e && e.message || e) });
  }
  file = null;
  blocks.clear();
}
