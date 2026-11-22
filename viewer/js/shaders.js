// GLSL (WebGL2 / GLSL ES 3.00) shader sources for the viewer.
//
// Splats are drawn as instanced quads. 3dgs/mip project with an analytic EWA
// Jacobian; 3dgut and 2dgs fragments evaluate the splat along per-pixel rays
// ("eval3d") inside a quad bounding their exact footprint
// (docs/notes/footprint.md). Data lives in 2D-array textures so arbitrarily
// large models fit even under a small MAX_TEXTURE_SIZE.
//
// Shaders are specialized at compile time: the renderer injects
//   #define CAM_MODEL n   (0 perspective, 1 orthographic, 2 fisheye
//                          equidistant, 3 fisheye equisolid, 4 equirect)
//   #define PRIM n        (0 3dgs, 1 mip, 2 3dgut, 3 2dgs; splat shaders only)
// and caches one program per combination, so no shader carries dead branches.

// ---------------------------------------------------------------------------
// Shared: fetch a per-splat texel from a layered (W,H,L) 2D array texture.
// ---------------------------------------------------------------------------
const TEX_FETCH = /* glsl */`
ivec3 texCoord(int ti, ivec2 dim) {
  int x = ti % dim.x;
  int t = ti / dim.x;
  int y = t % dim.y;
  int l = t / dim.y;
  return ivec3(x, y, l);
}
`;

// Shared camera projection, specialized by CAM_MODEL. Camera looks down -Z.
// projectCam returns (pixelX, pixelY, depth) where depth is a positive
// front-distance used for culling / sorting. projJac is the analytic Jacobian
// d(pixel)/d(cameraPos) — derived in a z-forward frame (matching the training
// code's projection_utils.slang) with the z column negated at the end.
export const PROJ_GLSL = /* glsl */`
const float PI = 3.14159265358979;
vec3 projectCam(vec3 pc, vec2 focal, vec2 center) {
#if CAM_MODEL == 1
  return vec3(focal * pc.xy + center, -pc.z);                          // ortho
#elif CAM_MODEL == 0
  float t = -pc.z;
  return vec3(focal*(pc.xy/t) + center, t);                            // pinhole
#elif CAM_MODEL == 4
  float lam = atan(pc.x, -pc.z);                                       // [-pi,pi]
  float phi = atan(pc.y, length(pc.xz));                               // [-pi/2,pi/2]
  return vec3(center.x + lam*focal.x, center.y + phi*focal.y, length(pc));
#else
  float lxy = length(pc.xy);
  vec2 dir = lxy > 1e-9 ? pc.xy / lxy : vec2(0.0);
  float theta = atan(lxy, -pc.z);                                      // angle from -Z, [0,pi]
  #if CAM_MODEL == 2
  return vec3(focal*theta*dir + center, length(pc));                   // equidistant
  #else
  return vec3(focal*(2.0*sin(0.5*theta))*dir + center, length(pc));    // equisolid
  #endif
#endif
}
// inverse: pixel-plane uv (= (frag-center)/focal) -> camera-space ray direction
vec3 unprojectCam(vec2 uv) {
#if CAM_MODEL == 0 || CAM_MODEL == 1
  return normalize(vec3(uv, -1.0));
#elif CAM_MODEL == 4
  float lam = uv.x, phi = uv.y;
  return vec3(cos(phi)*sin(lam), sin(phi), -cos(phi)*cos(lam));
#else
  float r = length(uv);
  vec2 dir = r > 1e-9 ? uv / r : vec2(0.0);
  #if CAM_MODEL == 3
  float theta = 2.0*asin(clamp(0.5*r, -1.0, 1.0));
  #else
  float theta = r;
  #endif
  return vec3(dir*sin(theta), -cos(theta));
#endif
}
// analytic Jacobian: mat2x3 whose columns are the gradients of (px,py) wrt
// the camera-space position (matches cov2d = transpose(J)*Sigma*J below)
mat2x3 projJac(vec3 pc, vec2 focal) {
  float x = pc.x, y = pc.y, z = -pc.z;   // z-forward frame
  vec3 gu, gv;
#if CAM_MODEL == 1
  gu = vec3(focal.x, 0.0, 0.0);
  gv = vec3(0.0, focal.y, 0.0);
  return mat2x3(gu, gv);                 // no z dependence, nothing to flip
#elif CAM_MODEL == 0
  float iz = 1.0/z;
  gu = vec3(focal.x*iz, 0.0, -focal.x*x*iz*iz);
  gv = vec3(0.0, focal.y*iz, -focal.y*y*iz*iz);
#elif CAM_MODEL == 4
  // u = fx*atan2(x,z), v = fy*atan2(y, rho), rho = sqrt(x^2+z^2)
  float rho2 = max(x*x + z*z, 1e-12);
  float rho = sqrt(rho2);
  float R2 = rho2 + y*y;
  gu = vec3(focal.x*z/rho2, 0.0, -focal.x*x/rho2);
  gv = vec3(-focal.y*y*x/(rho*R2), focal.y*rho/R2, -focal.y*y*z/(rho*R2));
#else
  // fisheye: uv = f * m * k(r,z), m = (x,y), r = |m|
  vec2 m = vec2(x, y);
  float r2 = dot(m, m), r = sqrt(r2);
  float R2 = r2 + z*z;
  if (r < 1e-6*z) {
    // on-axis limit == pinhole (both models); behind-axis is culled elsewhere
    float iz = 1.0/z;
    gu = vec3(focal.x*iz, 0.0, -focal.x*x*iz*iz);
    gv = vec3(0.0, focal.y*iz, -focal.y*y*iz*iz);
  } else {
    float theta = atan(r, z);
  #if CAM_MODEL == 2
    float k = theta/r;                             // equidistant
    float dk_dr = (z*r/R2 - theta)/r2;
    float dz_coef = -1.0/R2;                       // d(m*k)/dz = m*dz_coef
  #else
    float s = 2.0*sin(0.5*theta);                  // equisolid
    float k = s/r;
    float dk_dr = (cos(0.5*theta)*z*r/R2 - s)/r2;
    float dz_coef = -cos(0.5*theta)/R2;
  #endif
    float q = dk_dr/r;                             // d(m_i k)/dm_j = k dij + m_i m_j q
    gu = vec3(focal.x*(k + x*x*q), focal.x*(x*y*q), focal.x*(x*dz_coef));
    gv = vec3(focal.y*(x*y*q), focal.y*(k + y*y*q), focal.y*(y*dz_coef));
  }
#endif
  gu.z = -gu.z; gv.z = -gv.z;            // back to the -Z-forward frame
  return mat2x3(gu, gv);
}
`;

// ---------------------------------------------------------------------------
// SPLAT vertex shader (specialized by PRIM and CAM_MODEL)
// ---------------------------------------------------------------------------
// Shared: a splat's view-dependent colour (SH and spherical-beta lobes).
const SPLAT_COLOR_GLSL = /* glsl */`
uniform sampler2DArray uCol;        // (r,g,b,_)     raw f_dc
// Rest SH coeffs, uShRest texels/splat, snorm8 (RGB8_SNORM): the true coeff
// is texel * uScale.w (Gaussian-wise 8-bit quantization). Split by layer
// ranges across up to 4 textures: a single multi-GB texture exceeds
// per-resource limits of some GL/Vulkan/D3D implementations. Unused chunks
// alias chunk 0.
uniform sampler2DArray uSh0;
uniform sampler2DArray uSh1;
uniform sampler2DArray uSh2;
uniform sampler2DArray uSh3;
uniform int   uShChunkLayers;       // layers per chunk
uniform ivec2 uDim;                 // layout for pos/rot/scale/col (T=1)
uniform ivec2 uShDim;               // layout for sh texture
uniform int   uShDegree;
uniform int   uShRest;
// Spherical-beta lobes, 2 RGBA16F texels each -- (axis, sharpness),
// (amplitude, 0) -- in up to 2 layer-range chunks like SH.
uniform sampler2DArray uSb0;
uniform sampler2DArray uSb1;
uniform int   uSbChunkLayers;
uniform ivec2 uSbDim;
uniform int   uSbLobes;
uniform vec3  uCamPos;

vec4 fetch1(sampler2DArray tex, int i) { return texelFetch(tex, texCoord(i, uDim), 0); }
vec3 fetchSh(int i, int k) {
  ivec3 c = texCoord(i*uShRest + k, uShDim);
  int ch = c.z / uShChunkLayers;
  c.z -= ch * uShChunkLayers;
  return (ch == 0) ? texelFetch(uSh0, c, 0).rgb
       : (ch == 1) ? texelFetch(uSh1, c, 0).rgb
       : (ch == 2) ? texelFetch(uSh2, c, 0).rgb
       :             texelFetch(uSh3, c, 0).rgb;
}

vec4 fetchSb(int i, int k) {
  ivec3 c = texCoord(i*2*uSbLobes + k, uSbDim);
  int ch = c.z / uSbChunkLayers;
  c.z -= ch * uSbChunkLayers;
  return (ch == 0) ? texelFetch(uSb0, c, 0) : texelFetch(uSb1, c, 0);
}

// ---- spherical harmonics (real, up to degree 4; matches harmonics.slang).
// The rest coeffs are snorm8; the sum is de-quantized once by shScale
// (uScale.w), which distributes over the per-splat-constant scale. ----
vec3 evalColor(int idx, vec3 pw, float shScale) {
#ifdef SPLAT_ZERO_DC
  vec3 dc = vec3(0.0);                 // the view-dependent part alone
#else
  vec3 dc = fetch1(uCol, idx).rgb;
#endif
  vec3 c = 0.2820947917738781 * dc;
  if (uShDegree >= 1) {
    vec3 d = normalize(pw - uCamPos);
    float x=d.x, y=d.y, z=d.z;
    vec3 r = 0.4886025119029199 * (-y*fetchSh(idx,0) + z*fetchSh(idx,1) - x*fetchSh(idx,2));
    if (uShDegree >= 2) {
      float xx=x*x, yy=y*y, zz=z*z, xy=x*y, yz=y*z, xz=x*z;
      r += 1.0925484305920792*xy*fetchSh(idx,3);
      r += -1.0925484305920792*yz*fetchSh(idx,4);
      r += (0.9461746957575601*zz-0.31539156525252005)*fetchSh(idx,5);
      r += -1.0925484305920792*xz*fetchSh(idx,6);
      r += 0.5462742152960396*(xx-yy)*fetchSh(idx,7);
      if (uShDegree >= 3) {
        r += -0.5900435899266435*y*(3.0*xx-yy)*fetchSh(idx,8);
        r += 2.890611442640554*xy*z*fetchSh(idx,9);
        r += -0.4570457994644658*y*(4.0*zz-xx-yy)*fetchSh(idx,10);
        r += 0.3731763325901154*z*(2.0*zz-3.0*xx-3.0*yy)*fetchSh(idx,11);
        r += -0.4570457994644658*x*(4.0*zz-xx-yy)*fetchSh(idx,12);
        r += 1.445305721320277*z*(xx-yy)*fetchSh(idx,13);
        r += -0.5900435899266435*x*(xx-3.0*yy)*fetchSh(idx,14);
        if (uShDegree >= 4) {
          float fC1=xx-yy, fS1=2.0*xy;
          float fC2=x*fC1-y*fS1, fS2=x*fS1+y*fC1;
          float fC3=x*fC2-y*fS2, fS3=x*fS2+y*fC2;
          float pSH6 = 0.9461746957575601*zz - 0.3153915652525201;
          float pSH12 = z*(1.865881662950577*zz - 1.119528997770346);
          float fTmp0D = z*(-4.683325804901025*zz + 2.007139630671868);
          float fTmp1C = 3.31161143515146*zz - 0.47308734787878;
          float fTmp2B = -1.770130769779931*z;
          r += (0.6258357354491763*fS3)*fetchSh(idx,15);
          r += (fTmp2B*fS2)*fetchSh(idx,16);
          r += (fTmp1C*fS1)*fetchSh(idx,17);
          r += (fTmp0D*y)*fetchSh(idx,18);
          r += (1.984313483298443*z*pSH12 - 1.006230589874905*pSH6)*fetchSh(idx,19);
          r += (fTmp0D*x)*fetchSh(idx,20);
          r += (fTmp1C*fC1)*fetchSh(idx,21);
          r += (fTmp2B*fC2)*fetchSh(idx,22);
          r += (0.6258357354491763*fC3)*fetchSh(idx,23);
        }
      }
    }
    c += shScale * r;
  }
  vec3 col = max(c + 0.5, 0.0);
  // Lobes add onto the clamped colour, as the trainers do; renormalized,
  // since an f16 axis can run past unit length and overshoot a sharp peak.
  if (uSbLobes > 0) {
    vec3 d = normalize(pw - uCamPos);
    for (int l = 0; l < 8; l++) {
      if (l >= uSbLobes) break;
      vec4 a = fetchSb(idx, 2*l);
      float w = dot(d, normalize(a.xyz));
      if (w > 0.0) col += pow(w, a.w) * fetchSb(idx, 2*l + 1).rgb;
    }
  }
  return col;
}

`;

export const SPLAT_VS = /* glsl */`#version 300 es
precision highp float;
precision highp int;
precision highp sampler2DArray;

layout(location=0) in uint aIndex;  // per-instance splat index (sorted)

uniform sampler2DArray uPos;        // (x,y,z,opacity)
uniform sampler2DArray uRot;        // (qx,qy,qz,qw)
uniform sampler2DArray uScale;      // (sx,sy,sz, shScale)  exp-scale (world);
                                    // w = per-splat SH quantization scale
#ifdef SPLAT_COLOR_TEXTURE
uniform sampler2DArray uColor;      // the colour pass's output, one texel a splat
#endif

uniform mat3  uViewR;               // world -> camera rotation
uniform vec2  uFocal;               // fx, fy (pixels)
uniform vec2  uCenter;              // cx, cy (pixels)
uniform vec2  uViewport;            // width, height (pixels)
uniform float uNear;
uniform float uOpacityScale;
uniform bool uLogScale;            // native textures retain thin splats in log space

flat out vec4 vColor;
flat out vec2 vCenterPx;
flat out float vDepth;              // projCam front-depth (used by the pick pass)
#if PRIM >= 2
flat out vec3 vGro;                 // G * (ray_origin - mean_cam), ray_origin = 0
flat out mat3 vG;                   // maps cam-space offsets into the unit gaussian frame
#else
flat out vec3 vConic;
#endif

${TEX_FETCH}
${PROJ_GLSL}
${SPLAT_COLOR_GLSL}

mat3 quatToMat3(vec4 q) {
  float x=q.x,y=q.y,z=q.z,w=q.w;
  return mat3(
    1.0-2.0*(y*y+z*z), 2.0*(x*y+w*z),     2.0*(x*z-w*y),
    2.0*(x*y-w*z),     1.0-2.0*(x*x+z*z), 2.0*(y*z+w*x),
    2.0*(x*z+w*y),     2.0*(y*z-w*x),     1.0-2.0*(x*x+y*y));
}

vec3 projCam(vec3 pc) { return projectCam(pc, uFocal, uCenter); }

void cull() { gl_Position = vec4(0.0, 0.0, 2.0, 1.0); }

// A renderer defining SPLAT_COLOR_TEXTURE evaluates every splat's colour in a
// pass of its own, in storage order: 15 SH texels fetched in depth order cost
// ~6x the rest of this shader on a 10M-splat model.
vec4 splatColor(vec3 pw, float shScale, float opacity) {
#ifdef SPLAT_COLOR_TEXTURE
  return vec4(fetch1(uColor, int(aIndex)).rgb, opacity);
#else
  return vec4(evalColor(int(aIndex), pw, shScale), opacity);
#endif
}

// A renderer defining SPLAT_STOCHASTIC depth-tests unsorted splats: their z is
// the native CPU sort's key (HardwareSplatSort.h) mapped into [0, 1).
#ifdef SPLAT_STOCHASTIC
flat out uint vIndex;
float gSortZ = 0.0;
void setSortZ(vec3 pc) {
#if CAM_MODEL == 0 || CAM_MODEL == 1
  float k = max(-pc.z, 0.0);
#elif CAM_MODEL == 4
  float k = dot(pc, pc);
#else
  float z2 = pc.z*pc.z, r2 = dot(pc.xy, pc.xy);
  float k = z2*z2 + r2*r2/512.0;
#endif
  gSortZ = k/(k + 1.0);
}
#endif

// The quad ctr + x a + y b, x and y = +-1 from gl_VertexID.
void emitQuad(vec2 ctr, vec2 a, vec2 b) {
  vec2 corner = vec2((gl_VertexID & 1) == 0 ? -1.0 : 1.0,
                     (gl_VertexID & 2) == 0 ? -1.0 : 1.0);
  vec2 px = ctr + corner.x*a + corner.y*b;
#ifdef SPLAT_STOCHASTIC
  gl_Position = vec4(2.0*px/uViewport - 1.0, 2.0*gSortZ - 1.0, 1.0);
#else
  gl_Position = vec4(2.0*px/uViewport - 1.0, 0.0, 1.0);
#endif
}

#if PRIM >= 2
// footprint_disc (projection_utils.slang): every ray that reaches alpha 1/255
// passes through the planar ellipse c0 + a cos + b sin. m is the mean in a
// forward-z camera frame, ax the splat's axes there (columns). False: the
// camera is inside the gaussian, and every forward ray reaches it.
bool footprintDisc(vec3 m, mat3 ax, vec3 s, float r2, out vec3 c0, out vec3 a, out vec3 b) {
#if PRIM == 3
  float r = sqrt(r2);
  c0 = m; a = r*s.x*ax[0]; b = r*s.y*ax[1];
  return true;
#else
  s = max(s, 1e-3*max(s.x, max(s.y, s.z)));
  vec3 h = (transpose(ax)*m)/s;
  float hh = dot(h, h);
  c0 = m; a = vec3(0.0); b = vec3(0.0);
  if (!(hh > 1.001*r2)) return false;
  float k = r2/hh;
  float rho = sqrt(r2*(1.0 - k));
  h *= inversesqrt(hh);
  float sg = h.z >= 0.0 ? 1.0 : -1.0;
  float p = -1.0/(sg + h.z);
  float o = h.x*h.y*p;
  c0 = m*(1.0 - k);
  a = ax*(rho*s*vec3(1.0 + sg*h.x*h.x*p, sg*o, -sg*h.x));
  b = ax*(rho*s*vec3(o, sg + h.y*h.y*p, -h.y));
  return true;
#endif
}

// footprint_pinhole_ellipse: the disc's conic in pixels while it stays in
// front of the camera plane, as a centre and covariance (xx, xy, yy).
bool pinholeEllipse(vec3 c0, vec3 a, vec3 b, out vec2 ctr, out vec3 cov) {
  float den = c0.z*c0.z - a.z*a.z - b.z*b.z;
  ctr = vec2(0.0); cov = vec3(0.0);
  if (!(c0.z > 0.0 && den > 1e-6*c0.z*c0.z)) return false;
  float inv = 1.0/den;
  ctr = uFocal*vec2(c0.x*c0.z - a.x*a.z - b.x*b.z, c0.y*c0.z - a.y*a.z - b.y*b.z)*inv + uCenter;
  float ab_x = a.x*b.z - a.z*b.x, ab_y = a.y*b.z - a.z*b.y;
  float ca_x = c0.x*a.z - c0.z*a.x, cb_x = c0.x*b.z - c0.z*b.x;
  float ca_y = c0.y*a.z - c0.z*a.y, cb_y = c0.y*b.z - c0.z*b.y;
  cov = vec3(uFocal.x*uFocal.x*max(ca_x*ca_x + cb_x*cb_x - ab_x*ab_x, 0.0),
             uFocal.x*uFocal.y*(ca_x*ca_y + cb_x*cb_y - ab_x*ab_y),
             uFocal.y*uFocal.y*max(ca_y*ca_y + cb_y*cb_y - ab_y*ab_y, 0.0))*(inv*inv);
  return true;
}

#if CAM_MODEL == 2 || CAM_MODEL == 3
float lensRadius(float theta) {
#if CAM_MODEL == 2
  return theta;
#else
  return 2.0*sin(0.5*theta);
#endif
}
#endif
#endif

void main() {
  int idx = int(aIndex);
  vec4 po = fetch1(uPos, idx);
  vec3 pw = po.xyz;
  float opacity = po.w * uOpacityScale;

  vec3 pc = uViewR * (pw - uCamPos);
  vDepth = projCam(pc).z;
#ifdef SPLAT_STOCHASTIC
  vIndex = aIndex;
  setSortZ(pc);
#endif
  if (vDepth <= uNear) { cull(); return; }

  vec4 q = fetch1(uRot, idx);
  q = dot(q, q) > 0.0 ? normalize(q) : vec4(0.0, 0.0, 0.0, 1.0);
  vec4 scaleSh = fetch1(uScale, idx);   // xyz = world scale, w = SH quant scale
  vec3 scale = uLogScale ? exp(scaleSh.xyz) : scaleSh.xyz;
  mat3 Rq = quatToMat3(q);

#if PRIM >= 2
  // ---- eval3d (3dgut, 2dgs): each fragment casts its pixel's ray, so the
  // quad only has to hold the exact footprint, as compute bins it. ----
  if (opacity <= 1.0/255.0) { cull(); return; }
  float r2 = 2.0*log(opacity*255.0);
  mat3 ax = uViewR*Rq;                 // the splat's axes in camera space
  // G = S^-1 Rq^T Rv^T maps camera-space offsets into the splat's frame
  // (compute_3dgut_iscl_rot); a disc keeps its normal row unit length
  // (compute_2dgs_iscl_rot). An axis that flushes to 0 in f16 stays finite.
#if PRIM == 3
  vec3 iscl = vec3(1.0/max(scale.xy, vec2(uLogScale ? 1e-30 : 1e-7)), 1.0);
#else
  vec3 iscl = 1.0/max(scale, vec3(uLogScale ? 1e-30 : 1e-7));
#endif
  vG = mat3(iscl.x, 0.0, 0.0,  0.0, iscl.y, 0.0,  0.0, 0.0, iscl.z)*transpose(ax);
  vGro = -(vG*pc);
  // corners of the quad: ctr + x*semi1 + y*semi2, a pixel wider than the footprint
  vec2 ctr = 0.5*uViewport, semi1 = vec2(0.5*uViewport.x + 1.0, 0.0), semi2 = vec2(0.0, 0.5*uViewport.y + 1.0);
  vec3 cov = vec3(-1.0);
#if CAM_MODEL == 1
  // footprint_ortho_ellipse: a parallel projection is the splat's marginal
  vec3 sv = scale;
#if PRIM == 3
  sv.z = 0.0;
#endif
  mat3 M = ax*mat3(sv.x, 0.0, 0.0,  0.0, sv.y, 0.0,  0.0, 0.0, sv.z);
  mat3 Sigma = M*transpose(M);
  ctr = uFocal*pc.xy + uCenter;
  cov = r2*vec3(uFocal.x*uFocal.x*Sigma[0][0], uFocal.x*uFocal.y*Sigma[0][1], uFocal.y*uFocal.y*Sigma[1][1]);
#else
  vec3 c0, da, db;
  mat3 axf = mat3(ax[0]*vec3(1.0, 1.0, -1.0), ax[1]*vec3(1.0, 1.0, -1.0), ax[2]*vec3(1.0, 1.0, -1.0));
  if (footprintDisc(vec3(pc.xy, -pc.z), axf, scale, r2, c0, da, db)) {
#if CAM_MODEL == 0
    if (!pinholeEllipse(c0, da, db, ctr, cov)) { ctr = 0.5*uViewport; cov = vec3(-1.0); }
#else
    // Other lenses: the cone of directions the disc subtends, mapped to its
    // pixel box. A 16-gon pushed out by 1/cos(pi/16) holds the rim.
    vec3 cd = normalize(c0);
    float cmin = 1.0;
    for (int k = 0; k < 16; k++) {
      float t = float(k)*(PI/8.0);
      cmin = min(cmin, dot(cd, normalize(c0 + 1.0196*(da*cos(t) + db*sin(t)))));
    }
    if (cmin > 0.0) {
      float al = acos(cmin);
      vec2 lo, hi;
#if CAM_MODEL == 4
      float phc = asin(clamp(cd.y, -1.0, 1.0));
      lo = vec2(-PI, max(phc - al, -0.5*PI));
      hi = vec2(PI, min(phc + al, 0.5*PI));
      if (phc - al > -0.5*PI && phc + al < 0.5*PI) {
        float lamc = atan(cd.x, cd.z);
        float dl = asin(min(sin(al)/cos(phc), 1.0));
        // past the +-pi seam the footprint is at both edges: keep the full width
        if (lamc - dl > -PI && lamc + dl < PI) { lo.x = lamc - dl; hi.x = lamc + dl; }
      }
#else
      float thc = acos(clamp(cd.z, -1.0, 1.0));
      float r0 = lensRadius(max(thc - al, 0.0)), r1 = lensRadius(min(thc + al, PI));
      lo = vec2(-r1); hi = vec2(r1);
      if (thc > al && thc + al < PI) {
        // an annular sector: its corners, and the axes it sweeps across
        float phc = atan(cd.y, cd.x);
        float dph = asin(min(sin(al)/sin(thc), 1.0));
        vec2 u0 = vec2(cos(phc - dph), sin(phc - dph)), u1 = vec2(cos(phc + dph), sin(phc + dph));
        lo = min(min(r0*u0, r1*u0), min(r0*u1, r1*u1));
        hi = max(max(r0*u0, r1*u0), max(r0*u1, r1*u1));
        for (int q = 0; q < 4; q++) {
          float ang = float(q)*0.5*PI;
          float dq = ang - phc;
          dq -= 2.0*PI*floor((dq + PI)/(2.0*PI));
          if (abs(dq) <= dph) {
            vec2 e = r1*vec2(cos(ang), sin(ang));
            lo = min(lo, e); hi = max(hi, e);
          }
        }
      }
#endif
      ctr = uCenter + uFocal*0.5*(lo + hi);
      semi1 = vec2(0.5*uFocal.x*(hi.x - lo.x) + 1.0, 0.0);
      semi2 = vec2(0.0, 0.5*uFocal.y*(hi.y - lo.y) + 1.0);
    }
#endif
  }
#endif
  if (cov.x >= 0.0) {
    float a = cov.x, b = cov.y, c = cov.z;
    float mid = 0.5*(a + c);
    float disc = sqrt(max(0.0, mid*mid - (a*c - b*b)));
    float l1 = mid + disc, l2 = max(mid - disc, 0.0);
    vec2 e1 = abs(b) < 1e-12*max(l1, 1e-30) ? (a >= c ? vec2(1.0, 0.0) : vec2(0.0, 1.0))
                                           : normalize(vec2(l1 - c, b));
    semi1 = e1*(sqrt(l1) + 1.0);
    semi2 = vec2(-e1.y, e1.x)*(sqrt(l2) + 1.0);
  }
  vec2 be = abs(semi1) + abs(semi2);
  if (ctr.x + be.x < 0.0 || ctr.x - be.x > uViewport.x ||
      ctr.y + be.y < 0.0 || ctr.y - be.y > uViewport.y) { cull(); return; }
  vColor = splatColor(pw, scaleSh.w, opacity);
  emitQuad(ctr, semi1, semi2);
}
#else
  // ---- 3dgs / mip: EWA projection with the analytic Jacobian ----
  mat2 cov2d;
  vec2 mean2d;
  mat3 M = uViewR * Rq * mat3(scale.x,0.0,0.0, 0.0,scale.y,0.0, 0.0,0.0,scale.z);
  mat3 Sigma = M * transpose(M);       // camera-space 3D covariance
  mat2x3 J = projJac(pc, uFocal);
#if CAM_MODEL == 0
  // clip the z-gradients so a gaussian projected far outside the padded
  // viewport cannot produce an extreme Jacobian that occludes the view
  // (conventional 3dgs; see persp_proj_3dgs in primitive_3dgs.slang)
  {
    float rz = 1.0 / max(-pc.z, 1e-9);
    float lim_x_pos = 1.15*uViewport.x, lim_x_neg = 0.15*uViewport.x;
    float lim_y_pos = 1.15*uViewport.y, lim_y_neg = 0.15*uViewport.y;
    J[0].z = clamp(J[0].z, -(lim_x_neg + uCenter.x)*rz, (lim_x_pos - uCenter.x)*rz);
    J[1].z = clamp(J[1].z, -(lim_y_neg + uCenter.y)*rz, (lim_y_pos - uCenter.y)*rz);
  }
#endif
  cov2d = transpose(J) * Sigma * J;
  mean2d = projCam(pc).xy;
#if CAM_MODEL == 2 || CAM_MODEL == 3
  // sorting for >180deg fisheye is unreliable; cull behind-camera gaussians
  // that would contribute visibly at their projected center
  {
    vec2 dc = mean2d - uCenter;
    float det0 = cov2d[0][0]*cov2d[1][1] - cov2d[0][1]*cov2d[1][0];
    if (pc.z > 0.0 && abs(det0) > 1e-12) {
      mat2 covi = mat2(cov2d[1][1], -cov2d[0][1], -cov2d[1][0], cov2d[0][0]) * (1.0/det0);
      float opac = opacity * exp(-0.5 * dot(covi * dc, dc));
      if (opac > 1.0/255.0) { cull(); return; }
    }
  }
#endif

  // low-pass dilation + optional opacity compensation
#if PRIM == 1
  const float eps2d = 0.1;
#else
  const float eps2d = 0.3;
#endif
  float detOrig = cov2d[0][0]*cov2d[1][1] - cov2d[0][1]*cov2d[1][0];
  cov2d[0][0] += eps2d;
  cov2d[1][1] += eps2d;
  float det = cov2d[0][0]*cov2d[1][1] - cov2d[0][1]*cov2d[1][0];
  if (det <= 0.0) { cull(); return; }
#if PRIM == 1
  opacity *= sqrt(max(0.0, detOrig/det));
#endif

  float a=cov2d[0][0], b=cov2d[0][1], c=cov2d[1][1];
  vConic = vec3(c/det, -b/det, a/det);

  // eigen-decomposition for the oriented bounding quad
  float mid = 0.5*(a+c);
  float disc = sqrt(max(0.0, mid*mid - det));
  float l1 = mid+disc, l2 = max(mid-disc, 0.0);
  vec2 e1;
  if (abs(b) < 1e-9) e1 = (a>=c)?vec2(1.0,0.0):vec2(0.0,1.0);
  else e1 = normalize(vec2(l1-c, b));
  vec2 e2 = vec2(-e1.y, e1.x);
  const float kMinOpac = 1.0/255.0;
  float extend = sqrt(2.0*log(max(opacity/kMinOpac, 1.0+1e-6)));
  extend = min(extend, 3.33);
  vec2 semi1 = e1*extend*sqrt(l1);
  vec2 semi2 = e2*extend*sqrt(l2);

  // frustum cull by screen bounds
  vec2 be = abs(semi1) + abs(semi2);
  if (mean2d.x+be.x < 0.0 || mean2d.x-be.x > uViewport.x ||
      mean2d.y+be.y < 0.0 || mean2d.y-be.y > uViewport.y) { cull(); return; }

  vCenterPx = mean2d;
  vColor = splatColor(pw, scaleSh.w, opacity);
  emitQuad(mean2d, semi1, semi2);
}
#endif
`;

// fragment domain limit for fisheye models (image circle): everything outside
// is not part of the projection domain and must stay background
const DOMAIN_DISCARD = /* glsl */`
#if CAM_MODEL == 2
  if (length((gl_FragCoord.xy - uCenter) / uFocal) > 3.14159265358979) discard;
#elif CAM_MODEL == 3
  if (length((gl_FragCoord.xy - uCenter) / uFocal) > 2.0) discard;
#endif
`;

// 2D conic evaluation (3dgs / mip)
export const SPLAT_FS = /* glsl */`#version 300 es
precision highp float;
flat in vec3 vConic;
flat in vec4 vColor;
flat in vec2 vCenterPx;
uniform vec2 uFocal;
uniform vec2 uCenter;
out vec4 fragColor;
void main() {
  vec2 d = gl_FragCoord.xy - vCenterPx;
  float sigma = 0.5*(vConic.x*d.x*d.x + vConic.z*d.y*d.y) + vConic.y*d.x*d.y;
  if (sigma < 0.0) discard;
  float alpha = min(0.999, vColor.a * exp(-sigma));
  if (alpha <= 1.0/255.0) discard;
  fragColor = vec4(vColor.rgb, alpha);
}
`;

// The fragment the compute rasterizer evaluates along the pixel's ray
// (evaluate_alpha_3dgs, or a 2dgs disc's surfel_ray_hit), and its ray depth.
const EVAL3D_ALPHA_GLSL = /* glsl */`
float eval3dAlpha(vec2 frag, out vec3 rd, out float t) {
  vec2 uv = (frag - uCenter) / uFocal;
#if CAM_MODEL == 1
  rd = vec3(0.0, 0.0, -1.0);           // parallel rays: origin on the image plane
  vec3 gro = vG * vec3(uv, 0.0) + vGro;
#else
  rd = unprojectCam(uv);               // rays start at the camera origin
  vec3 gro = vGro;
#endif
  vec3 grd = vG * rd;
#if PRIM == 3
  t = 0.0;
  if (grd.z*grd.z <= 1e-6*dot(rd, rd)) return 0.0;   // SURFEL_GRAZING_COS2
  t = -gro.z/grd.z;
  vec2 h = gro.xy + t*grd.xy;
  return t > 0.0 ? vColor.a*exp(-0.5*dot(h, h)) : 0.0;
#else
  t = -dot(gro, grd)/dot(grd, grd);
  vec3 gcrod = cross(grd, gro);
  return t > 0.0 ? vColor.a*exp(-0.5*dot(gcrod, gcrod)/dot(grd, grd)) : 0.0;
#endif
}
`;

export const SPLAT_FS_3D = /* glsl */`#version 300 es
precision highp float;
flat in vec4 vColor;
flat in vec3 vGro;
flat in mat3 vG;
uniform vec2 uFocal;
uniform vec2 uCenter;
out vec4 fragColor;
${PROJ_GLSL}
${EVAL3D_ALPHA_GLSL}
void main() {
${DOMAIN_DISCARD}
  vec3 rd; float t;
  float alpha = eval3dAlpha(gl_FragCoord.xy, rd, t);
  if (alpha <= 1.0/255.0) discard;
  fragColor = vec4(vColor.rgb, alpha);
}
`;

// ---------------------------------------------------------------------------
// PICK fragment shaders — depth-under-cursor for double-click view centering.
// Same alpha as the render shaders, but output premultiplied expected depth
// (R = depth*alpha, A = alpha; blend ONE, ONE_MINUS_SRC_ALPHA back-to-front)
// so depth = R/A after the pass. Rendered into a 1x1 FBO with the viewport
// shifted so the clicked canvas pixel lands on it; uPixelOffset restores the
// canvas-space gl_FragCoord the conic/ray math expects.
// ---------------------------------------------------------------------------
export const PICK_FS = /* glsl */`#version 300 es
precision highp float;
flat in vec3 vConic;
flat in vec4 vColor;
flat in vec2 vCenterPx;
flat in float vDepth;
uniform vec2 uFocal;
uniform vec2 uCenter;
uniform vec2 uPixelOffset;
out vec4 fragColor;
void main() {
  vec2 frag = gl_FragCoord.xy + uPixelOffset;
  vec2 d = frag - vCenterPx;
  float sigma = 0.5*(vConic.x*d.x*d.x + vConic.z*d.y*d.y) + vConic.y*d.x*d.y;
  if (sigma < 0.0) discard;
  float alpha = min(0.999, vColor.a * exp(-sigma));
  if (alpha <= 1.0/255.0) discard;
  fragColor = vec4(vDepth*alpha, 0.0, 0.0, alpha);
}
`;

export const PICK_FS_3D = /* glsl */`#version 300 es
precision highp float;
flat in vec4 vColor;
flat in vec3 vGro;
flat in mat3 vG;
uniform vec2 uFocal;
uniform vec2 uCenter;
uniform vec2 uPixelOffset;
out vec4 fragColor;
${PROJ_GLSL}
${EVAL3D_ALPHA_GLSL}
void main() {
  vec2 frag = gl_FragCoord.xy + uPixelOffset;
#if CAM_MODEL == 2
  if (length((frag - uCenter) / uFocal) > PI) discard;
#elif CAM_MODEL == 3
  if (length((frag - uCenter) / uFocal) > 2.0) discard;
#endif
  vec3 rd; float t;
  float alpha = eval3dAlpha(frag, rd, t);
  if (alpha <= 1.0/255.0) discard;
  // ray parameter of the hit -> the projCam depth convention
#if CAM_MODEL == 0
  float depth = t * -rd.z;              // pinhole depth is axial distance
#else
  float depth = t;                      // ortho: t along -Z; others: ray length
#endif
  fragColor = vec4(depth*alpha, 0.0, 0.0, alpha);
}
`;

// ---------------------------------------------------------------------------
// Fullscreen quad VS (for grid + tonemap)
// ---------------------------------------------------------------------------
export const FULLSCREEN_VS = /* glsl */`#version 300 es
precision highp float;
out vec2 vUv;
void main() {
  vec2 p = vec2((gl_VertexID & 1) == 0 ? -1.0 : 3.0,
                (gl_VertexID & 2) == 0 ? -1.0 : 3.0);
  vUv = 0.5*p + 0.5;
  gl_Position = vec4(p, 0.0, 1.0);
}
`;

// ---------------------------------------------------------------------------
// TONEMAP / color-space fragment shader. Resolves the HDR splat buffer to the
// canvas: decodes per uIsLinear, applies the gamut matrix and the tone curve
// (uTransfer, the same order as colorspace::Transfer), composites on the bg.
// ---------------------------------------------------------------------------
export const TONEMAP_FS = /* glsl */`#version 300 es
precision highp float;
in vec2 vUv;
uniform sampler2D uHdr;
uniform mat3 uGamut;        // source gamut -> Rec.709
uniform int  uTransfer;     // 0 srgb, 1 srgb-clamped, 2 aces, 3 filmic, 4 uncharted2
uniform int  uIsLinear;     // 1 => the stored values are linear light
uniform vec3 uBackground;
uniform float uExposure;
out vec4 outColor;

float oetf(float x){ return x < 0.0031308 ? 12.92*x : 1.055*pow(x, 1.0/2.4) - 0.055; }
float eotf(float x){ return x < 0.04045 ? x/12.92 : pow((x+0.055)/1.055, 2.4); }
vec3 eotf3(vec3 c){ return vec3(eotf(c.r),eotf(c.g),eotf(c.b)); }

// Same curves as core/ColorSpace.h and shaders/pixel_wise.slang; a model must
// be shown with the transfer it was trained with or its whites move.
float aces(float x){ return (x*(2.51*x+0.03))/(x*(2.43*x+0.59)+0.14); }
float hable(float x){ return (x*(0.15*x+0.05)+0.004)/(x*(0.15*x+0.5)+0.06) - 0.02/0.3; }
float filmic(float x){ float t = max(x-0.004,0.0); return (t*(6.2*t+0.5))/(t*(6.2*t+1.7)+0.06); }

float encode(float x) {
  if (uTransfer == 3) return filmic(x);
  if (uTransfer == 2) return oetf(clamp(aces(max(x,0.0)), 0.0, 1.0));
  if (uTransfer == 4) return oetf(clamp(hable(max(x,0.0))/hable(11.2), 0.0, 1.0));
  if (uTransfer == 1) return oetf(clamp(x, 0.0, 1.0));
  return oetf(x);
}
vec3 encode3(vec3 c){ return vec3(encode(c.r),encode(c.g),encode(c.b)); }

void main() {
  vec4 hdr = texture(uHdr, vUv);
  // The compute renderer applies its display transfer to composited RGB.
  vec3 rgb = hdr.rgb * uExposure;
  if (uIsLinear == 0) rgb = eotf3(rgb);
  rgb = encode3(uGamut * rgb);
  vec3 outc = rgb + clamp(hdr.a, 0.0, 1.0) * uBackground;
  outColor = vec4(clamp(outc,0.0,1.0), 1.0);
}
`;

// ---------------------------------------------------------------------------
// MESH shaders (specialized by CAM_MODEL) — headlight shading with vertex
// color or a base texture, flat/smooth normals, discontinuity-safe for the
// nonlinear camera models.
// ---------------------------------------------------------------------------
export const MESH_VS = /* glsl */`#version 300 es
precision highp float;
layout(location=0) in vec3 aPos;
layout(location=1) in vec3 aNormal;
layout(location=2) in vec3 aColor;
layout(location=3) in vec2 aUv;
uniform mat4 uMVP;
uniform mat3 uNormalMat;
uniform mat3 uModelR;         // model (up-axis) rotation, native -> canonical
uniform mat3 uViewR;
uniform vec3 uCamPos;
uniform vec2 uFocal, uCenter, uViewport, uNearFar;
out vec3 vNormal;
out vec3 vColor;
out vec2 vUv;
out vec3 vPosW;               // canonical world position (shading, flat normals)
#if CAM_MODEL >= 2
out vec3 vPosCam;             // camera-space position (discontinuity check)
${PROJ_GLSL}
#endif
void main() {
  vNormal = uNormalMat * aNormal;
  vColor = aColor;
  vUv = aUv;
  vPosW = uModelR * aPos;
#if CAM_MODEL <= 1
  gl_Position = uMVP * vec4(aPos, 1.0);
#else
  vec3 pc = uViewR * (vPosW - uCamPos);
  vPosCam = pc;
  vec3 pr = projectCam(pc, uFocal, uCenter);
  vec2 ndc = 2.0*pr.xy/uViewport - 1.0;
  float ndcz = clamp((pr.z - uNearFar.x)/(uNearFar.y - uNearFar.x), 0.0, 1.0)*2.0 - 1.0;
  gl_Position = (pr.z < uNearFar.x) ? vec4(0.0,0.0,2.0,1.0) : vec4(ndc, ndcz, 1.0);
#endif
}
`;

// ---------------------------------------------------------------------------
// DATASET point-cloud + camera-frustum shaders (specialized by CAM_MODEL).
// Same projection contract as MESH_VS: a real MVP for the linear cameras, and
// the analytic projectCam path (writing NDC directly) for fisheye/equirect.
// Positions arrive in the model's NATIVE frame; uModelR = U applies the up-axis.
// ---------------------------------------------------------------------------
// Project a native-frame position `pos` to clip space (shared by points/lines).
const DATASET_PROJECT = /* glsl */`
#if CAM_MODEL <= 1
  gl_Position = uMVP * vec4(pos, 1.0);
#else
  vec3 pw = uModelR * pos;
  vec3 pc = uViewR * (pw - uCamPos);
  vec3 pr = projectCam(pc, uFocal, uCenter);
  vec2 ndc = 2.0*pr.xy/uViewport - 1.0;
  float ndcz = clamp((pr.z - uNearFar.x)/(uNearFar.y - uNearFar.x), 0.0, 1.0)*2.0 - 1.0;
  gl_Position = (pr.z < uNearFar.x) ? vec4(0.0,0.0,2.0,1.0) : vec4(ndc, ndcz, 1.0);
#endif
`;

export const POINT_VS = /* glsl */`#version 300 es
precision highp float;
layout(location=0) in vec3 aPos;
layout(location=1) in vec3 aColor;
uniform mat4 uMVP;
uniform mat3 uModelR, uViewR;
uniform vec3 uCamPos;
uniform vec2 uFocal, uCenter, uViewport, uNearFar;
uniform float uPointSize;
out vec3 vColor;
#if CAM_MODEL >= 2
${PROJ_GLSL}
#endif
void main() {
  vColor = aColor;
  vec3 pos = aPos;
${DATASET_PROJECT}
  gl_PointSize = uPointSize;
}
`;

export const POINT_FS = /* glsl */`#version 300 es
precision highp float;
in vec3 vColor;
uniform vec2 uFocal, uCenter;
out vec4 outColor;
void main() {
${DOMAIN_DISCARD}
  vec2 d = gl_PointCoord * 2.0 - 1.0;
  if (dot(d, d) > 1.0) discard;            // round sprite
  outColor = vec4(vColor, 1.0);
}
`;

// Frustum lines: each vertex is (apex + unit-offset * uFrustumScale) in native
// frame, so the frustum-size slider is a cheap GPU uniform (no CPU rebuild).
export const LINE_VS = /* glsl */`#version 300 es
precision highp float;
layout(location=0) in vec3 aApex;
layout(location=1) in vec3 aOffset;
layout(location=2) in vec3 aColor;
layout(location=3) in vec3 aDelta;   // this-endpoint minus other-endpoint,
                                     // pre-scale (frusta: offset units,
                                     // grid: position units)
uniform mat4 uMVP;
uniform mat3 uModelR, uViewR;
uniform vec3 uCamPos;
uniform vec2 uFocal, uCenter, uViewport, uNearFar;
uniform float uFrustumScale;
uniform float uDeltaScale;           // uFrustumScale for frusta, 1 for grid
uniform int uUseUniform;
uniform vec3 uColor;
out vec3 vColor;
#if CAM_MODEL >= 2
out vec3 vPosCam;                    // camera-space position (seam discard)
${PROJ_GLSL}
#endif
#if CAM_MODEL == 4
out float vKill;                     // whole-segment equirect seam kill
#endif
void main() {
  vColor = (uUseUniform == 1) ? uColor : aColor;
  vec3 pos = aApex + aOffset * uFrustumScale;
${DATASET_PROJECT}
#if CAM_MODEL >= 2
  vPosCam = pc;
#endif
#if CAM_MODEL == 4
  // A segment whose chord crosses the equirect ±180° azimuth seam (the
  // half-plane x = 0, z > 0 in camera space) rasterizes as the wrapped
  // chord: no fragment of it is valid, and the reprojection check in
  // LINE_FS alone keeps ~5%-of-viewport stubs at both ends (the error
  // shrinks to zero at the endpoints), which stack into ladder artifacts
  // at the image edges. Both vertices see both endpoints via aDelta and
  // compute the same flag, so it interpolates flat.
  vec3 pc2 = uViewR * (uModelR * (pos - aDelta * uDeltaScale) - uCamPos);
  vKill = 0.0;
  if (pc.x * pc2.x < 0.0) {
    float t = pc.x / (pc.x - pc2.x);
    if (mix(pc.z, pc2.z, t) > 0.0) vKill = 1.0;
  }
#endif
}
`;

export const LINE_FS = /* glsl */`#version 300 es
precision highp float;
in vec3 vColor;
uniform vec2 uFocal, uCenter;
#if CAM_MODEL >= 2
in vec3 vPosCam;
uniform vec2 uViewport;
${PROJ_GLSL}
#endif
#if CAM_MODEL == 4
in float vKill;
#endif
out vec4 outColor;
void main() {
#if CAM_MODEL == 4
  if (vKill > 0.5) discard;          // segment crosses the equirect seam
#endif
#if CAM_MODEL >= 2
${DOMAIN_DISCARD}
  // Same seam handling as MESH_FS: a segment crossing a projection
  // discontinuity (equirect ±180° seam, fisheye backward point) rasterizes as
  // the chord between the wrapped endpoints; re-projecting the interpolated
  // camera-space position exposes it as a huge reprojection error.
  vec2 err = projectCam(vPosCam, uFocal, uCenter).xy - gl_FragCoord.xy;
  if (length(err) > 0.05*min(uViewport.x, uViewport.y)) discard;
#endif
  outColor = vec4(vColor, 1.0);
}
`;

export const MESH_FS = /* glsl */`#version 300 es
precision highp float;
in vec3 vNormal;
in vec3 vColor;
in vec2 vUv;
in vec3 vPosW;
uniform sampler2D uTex;
uniform int uMode;          // 0 none, 1 vertex color, 2 texture
uniform int uColorOn;       // 1 show vertex/texture color, 0 plain
uniform int uShade;         // 1 apply shading
uniform int uFlat;          // 1 face normals, 0 interpolated vertex normals
uniform vec3 uCamPos;
#if CAM_MODEL >= 2
in vec3 vPosCam;
uniform vec2 uFocal, uCenter, uViewport;
${PROJ_GLSL}
#endif
out vec4 outColor;
void main() {
#if CAM_MODEL >= 2
${DOMAIN_DISCARD}
  // Discard triangles that cross a projection discontinuity (the equirect
  // ±180° seam, the fisheye backward point): re-project the interpolated
  // camera-space position and compare against the rasterized position. A
  // wrapped triangle is off by a large error — and, near its vertices where
  // the error vanishes, still has an O(1) error *gradient*, while a normal
  // triangle's curvature error and gradient are both small.
  vec2 err = projectCam(vPosCam, uFocal, uCenter).xy - gl_FragCoord.xy;
  vec2 gx = dFdx(err), gy = dFdy(err);
  if (length(err) > 0.05*min(uViewport.x, uViewport.y) ||
      max(length(gx), length(gy)) > 0.5) discard;
#endif
  vec3 base = vec3(0.78);
  if (uColorOn == 1) {
    if (uMode == 2) base = texture(uTex, vUv).rgb;
    else if (uMode == 1) base = vColor;
  }
  float shade = 1.0;
  if (uShade == 1) {
    vec3 n = vNormal;
    if (uFlat == 1 || dot(n, n) < 1e-12)
      n = cross(dFdx(vPosW), dFdy(vPosW));   // face normal
    n = normalize(n);
    // headlight: light follows the camera, so the mesh looks shaded from
    // every viewing angle; abs() also lights back faces
    vec3 v = normalize(uCamPos - vPosW);
    shade = 0.25 + 0.75*abs(dot(n, v));
  }
  outColor = vec4(base*shade, 1.0);
}
`;
