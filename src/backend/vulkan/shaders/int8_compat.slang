// Adaptive byte-granular buffer access for the Vulkan compute entries.
//
// The baseline reads/writes uint8 buffer elements through u32 WORDS (shift +
// mask) because shaderInt8 is not in the device baseline; word reads past the
// last byte stay in bounds because device allocations are 16-byte rounded
// (VulkanRuntime). Byte writes must not clobber the neighboring bytes of the
// word, so the baseline store is an InterlockedAnd+InterlockedOr pair.
//
// Under -DSS_NATIVE_INT8 (the ".int8" blob variants, picked by
// VulkanPipelines.cpp on devices with shaderInt8 + storageBuffer8BitAccess)
// these lower to native byte loads/stores — notably turning the two-atomic
// store into a plain store.
//
// Buffers are passed as uint* (the word view) on BOTH paths so kernel
// signatures and push layouts never change between variants.

#ifndef SS_INT8_COMPAT
#define SS_INT8_COMPAT

#include "int64_compat.slang"

// A uint overload beside each gindex_t one keeps 32-bit call sites off
// int64, which would drag OpCapability Int64 into base blobs. The gindex_t
// ones use elem_at: their callers run past 4 GiB, where buf[i] wraps.

#ifdef SS_NATIVE_INT8

// Unsigned byte load (0..255).
[ForceInline] uint u8_load(uint* buf, uint idx) {
    return uint(((uint8_t*)buf)[idx]);
}
[ForceInline] uint u8_load(uint* buf, gindex_t idx) {
    return uint(((uint8_t*)elem_at(buf, idx >> 2))[uint(idx) & 3u]);
}
// Sign-extended int8 load (-128..127).
[ForceInline] int s8_load(uint* buf, uint idx) {
    return int(((int8_t*)buf)[idx]);
}
[ForceInline] int s8_load(uint* buf, gindex_t idx) {
    return int(((int8_t*)elem_at(buf, idx >> 2))[uint(idx) & 3u]);
}
[ForceInline] void u8_store(uint* buf, uint idx, uint v) {
    ((uint8_t*)buf)[idx] = uint8_t(v);
}
[ForceInline] void u8_store(uint* buf, gindex_t idx, uint v) {
    ((uint8_t*)elem_at(buf, idx >> 2))[uint(idx) & 3u] = uint8_t(v);
}

#else  // !SS_NATIVE_INT8

[ForceInline] uint u8_load(uint* buf, uint idx) {
    uint w = buf[idx >> 2];
    return (w >> ((idx & 3u) * 8u)) & 0xffu;
}
[ForceInline] uint u8_load(uint* buf, gindex_t idx) {
    uint w = elem_at(buf, idx >> 2)[0];
    return (w >> ((uint(idx) & 3u) * 8u)) & 0xffu;
}
[ForceInline] int s8_load(uint* buf, uint idx) {
    return (int(u8_load(buf, idx)) << 24) >> 24;
}
[ForceInline] int s8_load(uint* buf, gindex_t idx) {
    return (int(u8_load(buf, idx)) << 24) >> 24;
}
[ForceInline] void u8_store(uint* buf, uint idx, uint v) {
    uint shift = 8u * (idx & 3u);
    InterlockedAnd(buf[idx >> 2], ~(0xffu << shift));
    InterlockedOr(buf[idx >> 2], (v & 0xffu) << shift);
}
[ForceInline] void u8_store(uint* buf, gindex_t idx, uint v) {
    uint shift = 8u * (uint(idx) & 3u);
    uint* w = elem_at(buf, idx >> 2);
    InterlockedAnd(w[0], ~(0xffu << shift));
    InterlockedOr(w[0], (v & 0xffu) << shift);
}

#endif  // SS_NATIVE_INT8

#endif  // SS_INT8_COMPAT
