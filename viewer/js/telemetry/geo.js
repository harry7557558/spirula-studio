// Geodesy and the per-clip data model for the IMU/GPS tool.
//
// Everything is drawn in ONE local frame: Web Mercator, shifted to the scene
// origin and scaled by cos(lat0) so a unit is a metre at the origin. Map tiles
// are Mercator squares, so this is the only frame in which the imagery and the
// track are consistent without warping either; over a few kilometres the
// residual scale error is under 0.1%.

export const EARTH_CIRCUM = 40075016.686;

export function mercX(lonDeg) { return (lonDeg + 180) / 360; }
export function mercY(latDeg) {
  const s = Math.sin(Math.max(-85.05112878, Math.min(85.05112878, latDeg)) * Math.PI / 180);
  return 0.5 - Math.log((1 + s) / (1 - s)) / (4 * Math.PI);
}
export function invMercX(x) { return x * 360 - 180; }
export function invMercY(y) {
  return (2 * Math.atan(Math.exp((0.5 - y) * 2 * Math.PI)) - Math.PI / 2) * 180 / Math.PI;
}

// The mapping between the local metric frame and Mercator, fixed once per
// session by the first track that appears so later drops stay registered.
export class Frame {
  constructor(latDeg, lonDeg, alt) {
    this.lat0 = latDeg; this.lon0 = lonDeg; this.alt0 = alt || 0;
    this.mx0 = mercX(lonDeg); this.my0 = mercY(latDeg);
    this.scale = EARTH_CIRCUM * Math.cos(latDeg * Math.PI / 180);
  }
  toLocal(latDeg, lonDeg, alt) {
    return [(mercX(lonDeg) - this.mx0) * this.scale,
            -(mercY(latDeg) - this.my0) * this.scale,
            (alt || 0) - this.alt0];
  }
  // Mercator unit square -> local metres, for placing a map tile.
  mercToLocal(mx, my) { return [(mx - this.mx0) * this.scale, -(my - this.my0) * this.scale]; }
  localToLatLon(x, y) {
    return [invMercY(this.my0 - y / this.scale), invMercX(this.mx0 + x / this.scale)];
  }
}

export function haversine(lat1, lon1, lat2, lon2) {
  const R = 6371000, d = Math.PI / 180;
  const dlat = (lat2 - lat1) * d, dlon = (lon2 - lon1) * d;
  const a = Math.sin(dlat / 2) ** 2 + Math.cos(lat1 * d) * Math.cos(lat2 * d) * Math.sin(dlon / 2) ** 2;
  return 2 * R * Math.asin(Math.min(1, Math.sqrt(a)));
}

// --- clip construction -----------------------------------------------------

const RAD2DEG = 180 / Math.PI;

function splitVec(a) {
  if (!a) return null;
  const n = a.length / 4;
  const t = new Float32Array(n), x = new Float32Array(n), y = new Float32Array(n), z = new Float32Array(n);
  const m = new Float32Array(n);
  for (let i = 0; i < n; i++) {
    t[i] = a[i * 4]; x[i] = a[i * 4 + 1]; y[i] = a[i * 4 + 2]; z[i] = a[i * 4 + 3];
    m[i] = Math.hypot(x[i], y[i], z[i]);
  }
  return { n, t, x, y, z, mag: m };
}

// Yaw/pitch/roll of the sensor in the world, from the attitude the camera
// wrote. Reported in the quaternion's own sense; which sense that is comes
// from the reader's check (attitudeSensorToWorld).
function splitQuat(a) {
  if (!a) return null;
  const n = a.length / 5;
  const t = new Float32Array(n), w = new Float32Array(n), x = new Float32Array(n);
  const y = new Float32Array(n), z = new Float32Array(n);
  const yaw = new Float32Array(n), pitch = new Float32Array(n), roll = new Float32Array(n);
  for (let i = 0; i < n; i++) {
    t[i] = a[i * 5]; w[i] = a[i * 5 + 1]; x[i] = a[i * 5 + 2]; y[i] = a[i * 5 + 3]; z[i] = a[i * 5 + 4];
    const qw = w[i], qx = x[i], qy = y[i], qz = z[i];
    const sinp = 2 * (qw * qy - qz * qx);
    yaw[i]   = Math.atan2(2 * (qw * qz + qx * qy), 1 - 2 * (qy * qy + qz * qz)) * RAD2DEG;
    pitch[i] = Math.asin(Math.max(-1, Math.min(1, sinp))) * RAD2DEG;
    roll[i]  = Math.atan2(2 * (qw * qx + qy * qz), 1 - 2 * (qx * qx + qy * qy)) * RAD2DEG;
  }
  return { n, t, w, x, y, z, yaw, pitch, roll };
}

function unwrapDeg(a) {
  const out = Float32Array.from(a);
  for (let i = 1; i < out.length; i++) {
    let d = out[i] - out[i - 1];
    while (d > 180) { out[i] -= 360; d -= 360; }
    while (d < -180) { out[i] += 360; d += 360; }
  }
  return out;
}

// A receiver logged faster than it fixes repeats each position; the derived
// speed only means anything between positions that actually changed.
export function buildGps(a) {
  const n = a.length / 9;
  const g = {
    n,
    t: new Float32Array(n), unix: new Float64Array(n),
    lat: new Float64Array(n), lon: new Float64Array(n), alt: new Float32Array(n),
    fix: new Uint8Array(n), hasAlt: new Uint8Array(n),
    speed: new Float32Array(n), track: new Float32Array(n), dop: new Float32Array(n),
    dspeed: new Float32Array(n), dist: new Float32Array(n), fresh: new Uint8Array(n),
  };
  let last = -1, total = 0;
  for (let i = 0; i < n; i++) {
    g.t[i] = a[i * 9]; g.unix[i] = a[i * 9 + 1];
    g.lat[i] = a[i * 9 + 2]; g.lon[i] = a[i * 9 + 3]; g.alt[i] = a[i * 9 + 4];
    const flags = a[i * 9 + 5];
    g.hasAlt[i] = (flags & 1) ? 1 : 0;
    g.fix[i] = (flags & 2) ? 1 : 0;
    g.speed[i] = a[i * 9 + 6]; g.track[i] = a[i * 9 + 7]; g.dop[i] = a[i * 9 + 8];
    const moved = last < 0 || g.lat[i] !== g.lat[last] || g.lon[i] !== g.lon[last];
    g.fresh[i] = moved ? 1 : 0;
    if (moved && last >= 0) {
      const d = haversine(g.lat[last], g.lon[last], g.lat[i], g.lon[i]);
      const dt = g.t[i] - g.t[last];
      total += d;
      g.dspeed[i] = dt > 0 ? d / dt : 0;
    } else if (last >= 0) {
      g.dspeed[i] = g.dspeed[last];
    }
    g.dist[i] = total;
    if (moved) last = i;
  }
  return g;
}

// Shutter, ISO and aperture as the stops of light each lets in (log2 t,
// log2 ISO, -2 log2 N), each about its own mean, and their sum over the factors
// every record has: for one source, what --ppisp-exposure-from-exif compares.
export function buildExposure(t, shutter, iso, fnum) {
  const n = t.length, val = [shutter, iso, fnum];
  const log2 = [0, 1, 2].map(() => new Float32Array(n));
  const stops = [0, 1, 2].map(() => new Float32Array(n));
  let common = 7, any = 0;
  for (let i = 0; i < n; i++) {
    let has = 0;
    for (let k = 0; k < 3; k++) {
      const l = val[k][i] > 0 ? Math.log2(val[k][i]) : NaN;
      log2[k][i] = l;
      stops[k][i] = k === 2 ? -2 * l : l;
      if (l === l) has |= 1 << k;
    }
    common &= has;
    any |= has;
  }
  const varies = [false, false, false];
  for (let k = 0; k < 3; k++) {
    let sum = 0, m = 0, lo = Infinity, hi = -Infinity;
    for (const v of stops[k]) if (v === v) { sum += v; m++; lo = Math.min(lo, v); hi = Math.max(hi, v); }
    for (let i = 0; i < n; i++) stops[k][i] -= m ? sum / m : 0;
    varies[k] = hi - lo > 0.05;
  }
  let total = null;
  if (common) {
    total = new Float32Array(n);
    for (let i = 0; i < n; i++)
      for (let k = 0; k < 3; k++) if (common >> k & 1) total[i] += stops[k][i];
  }
  const rate = n > 1 && t[n - 1] > t[0] ? (n - 1) / (t[n - 1] - t[0]) : 0;
  return { n, t, shutter, iso, fnum, log2, stops, total, common, any, varies, rate };
}

function splitExposure(a) {
  if (!a) return null;
  const n = a.length / 4;
  const order = Array.from({ length: n }, (_, i) => i).sort((x, y) => a[x * 4] - a[y * 4]);
  const t = new Float32Array(n), sh = new Float32Array(n), iso = new Float32Array(n), fn = new Float32Array(n);
  order.forEach((j, i) => { t[i] = a[j * 4]; sh[i] = a[j * 4 + 1]; iso[i] = a[j * 4 + 2]; fn[i] = a[j * 4 + 3]; });
  return n ? buildExposure(t, sh, iso, fn) : null;
}

const CLIP_COLORS = [
  [0.31, 0.62, 0.97], [0.98, 0.66, 0.25], [0.36, 0.83, 0.53], [0.92, 0.42, 0.52],
  [0.63, 0.51, 0.94], [0.30, 0.82, 0.83], [0.90, 0.83, 0.35], [0.85, 0.55, 0.85],
];

// Stills with a position or an exposure are collected per folder rather than
// one clip each: a shoot is one set of camera positions, and 500 list rows
// would be unusable. The time axis is the photo index, in name order.
export function makePhotoClip(folder, index) {
  return {
    id: `photo:${folder}`, path: folder, name: folder.replace(/\/$/, '').split('/').pop() || 'photos',
    size: 0, kind: 'photos', color: CLIP_COLORS[index % CLIP_COLORS.length],
    visible: true, report: '', photos: [],
    meta: { carrier: 'EXIF', camera: '', duration: 0, fps: 0, unixStart: 0, notes: [], check: {} },
    gps: null, gyro: null, accel: null, gravity: null, magnet: null, att: null, expo: null,
    duration: 0, local: null,
  };
}

export function addPhoto(clip, msg) {
  const e = msg.exif;
  clip.photos.push({
    name: msg.path.split('/').pop(), hasGps: !!e.hasGps,
    lat: e.lat, lon: e.lon, alt: e.hasAlt ? e.alt : 0, hasAlt: !!e.hasAlt,
    unix: (msg.lastModified || 0) / 1000,
    camera: [e.make, e.model].filter(Boolean).join(' '),
    px: e.width && e.height ? `${e.width}x${e.height}` : '',
    focal: e.focalMm || 0,
    shutter: e.exposureTime || 0, iso: e.iso || 0, fnum: e.fNumber || 0,
  });
  clip.photos.sort((a, b) => (a.name < b.name ? -1 : a.name > b.name ? 1 : 0));
  const n = clip.photos.length;
  const fixes = [], exposed = [];
  clip.photos.forEach((p, i) => {
    if (p.hasGps) fixes.push(i);
    if (p.shutter > 0 || p.iso > 0 || p.fnum > 0) exposed.push(i);
  });
  const a = new Float64Array(fixes.length * 9);
  fixes.forEach((i, k) => {
    const p = clip.photos[i];
    a[k * 9] = i; a[k * 9 + 1] = p.unix; a[k * 9 + 2] = p.lat; a[k * 9 + 3] = p.lon;
    a[k * 9 + 4] = p.alt; a[k * 9 + 5] = (p.hasAlt ? 1 : 0) + 2;
    a[k * 9 + 6] = -1; a[k * 9 + 7] = -1; a[k * 9 + 8] = 0;
  });
  clip.gps = fixes.length ? buildGps(a) : null;
  const pick = (key) => Float32Array.from(exposed, (i) => clip.photos[i][key] || -1);
  clip.expo = exposed.length ? buildExposure(Float32Array.from(exposed), pick('shutter'), pick('iso'), pick('fnum')) : null;
  clip.duration = Math.max(1, n - 1);
  const cams = Array.from(new Set(clip.photos.map((p) => p.camera).filter(Boolean)));
  clip.meta.camera = cams.slice(0, 2).join(', ') + (cams.length > 2 ? `, +${cams.length - 2}` : '');
  clip.meta.check = { gpsUsable: fixes.length > 0, gps: { count: fixes.length, tFirst: 0, tLast: n - 1, rate: 0 } };
  const withAlt = clip.photos.filter((p) => p.hasGps && p.hasAlt).length;
  clip.report = [`${n} photo${n === 1 ? '' : 's'} in ${clip.path || '.'}`,
                 `${fixes.length} geotagged, ${withAlt} with an altitude`,
                 `${exposed.length} with an exposure`,
                 cams.length ? `cameras: ${cams.join(', ')}` : 'no camera identity in EXIF'].join('\n');
}

// One dropped file's telemetry, in the shape the viewport and the charts read.
export function makeClip(msg, index) {
  const name = msg.path.split('/').pop();
  const clip = {
    id: msg.id, path: msg.path, name, size: msg.size, kind: msg.kind,
    color: CLIP_COLORS[index % CLIP_COLORS.length],
    visible: true, report: msg.report || '',
    meta: msg.meta || {}, gps: null, gyro: null, accel: null, gravity: null,
    magnet: null, att: null, expo: null, duration: 0, local: null,
  };
  const s = msg.streams || {};
  clip.gyro = splitVec(s.gyro);
  clip.accel = splitVec(s.accel);
  clip.gravity = splitVec(s.gravity);
  clip.magnet = splitVec(s.magnet);
  clip.att = splitQuat(s.orientation);
  if (clip.att) {
    clip.att.yawU = unwrapDeg(clip.att.yaw);
    clip.att.rollU = unwrapDeg(clip.att.roll);
  }
  if (s.gps) clip.gps = buildGps(s.gps);
  clip.expo = splitExposure(s.exposure);
  clip.duration = msg.meta.duration || 0;
  for (const st of [clip.gyro, clip.accel, clip.gravity, clip.att, clip.gps, clip.expo])
    if (st && st.n) clip.duration = Math.max(clip.duration, st.t[st.n - 1]);
  return clip;
}

// Local metres for every fix, once the scene frame is known. Kept separate
// from makeClip so a later drop can share the first clip's origin.
export function projectClip(clip, frame) {
  const g = clip.gps;
  if (!g || !g.n) return;
  const p = new Float32Array(g.n * 3);
  for (let i = 0; i < g.n; i++) {
    const l = frame.toLocal(g.lat[i], g.lon[i], g.hasAlt[i] ? g.alt[i] : frame.alt0);
    p[i * 3] = l[0]; p[i * 3 + 1] = l[1]; p[i * 3 + 2] = l[2];
  }
  clip.local = p;
}

// Nearest sample at or before `t`, by binary search on a monotone time array.
export function indexAt(t, time, n) {
  let lo = 0, hi = n - 1;
  if (n === 0) return -1;
  if (t <= time[0]) return 0;
  if (t >= time[n - 1]) return n - 1;
  while (lo + 1 < hi) {
    const mid = (lo + hi) >> 1;
    if (time[mid] <= t) lo = mid; else hi = mid;
  }
  return lo;
}
