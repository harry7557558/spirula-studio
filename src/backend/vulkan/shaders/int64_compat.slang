// Adaptive 64-bit integer layer for the Vulkan compute entries. By default
// every type below is the native 64-bit integer (int64-capable devices
// compile identical code to before this header existed). Under
// -DSS_EMULATE_INT64 (the ".noint64" blob variants, picked by
// VulkanPipelines.cpp on devices without shaderInt64) the SPIR-V must not
// declare the Int64 capability, so:
//
//  - gindex_t (index/size arithmetic) narrows to int32. Devices without
//    shaderInt64 are old/small iGPUs; buffers with >= 2^31 elements cannot
//    exist there, which the host guards anyway (sort/scan reject > INT32_MAX
//    items, and allocations that large fail long before dispatch).
//  - i64mem_t (the in-memory image of an int64 tensor element or push
//    field) becomes uint2 (little-endian lo, hi) so buffer and push-constant
//    layouts stay byte-identical to the host's int64/uint64 fields.
//  - u64key_t (genuinely 64-bit values: radix-sort keys, morton codes)
//    becomes uint2 with helpers for the few operations kernels perform on
//    them; values stay bit-identical to the native path.
//  - is_null(p) avoids the ConvertPtrToU-to-u64 comparison slang emits for
//    `p == nullptr` by bitcasting the pointer to uint2 (explicitly legal
//    under SPV_KHR_physical_storage_buffer without Int64).
//
// spirv_tool.cpp verifies each .noint64 blob really dropped OpCapability
// Int64, so an unguarded int64 use fails the build instead of the device.

#ifndef SS_INT64_COMPAT
#define SS_INT64_COMPAT

#ifndef SS_EMULATE_INT64

typedef int64_t gindex_t;
typedef int64_t i64mem_t;
typedef uint64_t u64key_t;

[ForceInline] gindex_t i64mem_get(i64mem_t v) { return v; }
[ForceInline] i64mem_t i64mem_make(gindex_t v) { return v; }

[ForceInline] u64key_t u64key_make(gindex_t hi, uint lo) {
    return (uint64_t(hi) << 32) | uint64_t(lo);
}
[ForceInline] gindex_t u64key_hi(u64key_t k) { return gindex_t(k >> 32); }
[ForceInline] bool u64key_eq(u64key_t a, u64key_t b) { return a == b; }
[ForceInline] u64key_t u64key_xor(u64key_t a, u64key_t b) { return a ^ b; }
[ForceInline] int u64key_clz(u64key_t k) {
    uint hi = uint(k >> 32);
    if (hi != 0) return 31 - int(firstbithigh(hi));
    uint lo = uint(k);
    return lo != 0 ? 63 - int(firstbithigh(lo)) : 64;
}
// Digit extraction for the radix sort (shift need not be word-aligned).
[ForceInline] uint u64key_digit(u64key_t k, uint shift, uint mask) {
    return uint(k >> shift) & mask;
}

[ForceInline] bool is_null<T>(T* p) { return p == nullptr; }

#else  // SS_EMULATE_INT64

typedef int32_t gindex_t;
typedef uint2 i64mem_t;
typedef uint2 u64key_t;

[ForceInline] gindex_t i64mem_get(i64mem_t v) { return int(v.x); }
[ForceInline] i64mem_t i64mem_make(gindex_t v) {
    return uint2(uint(v), v < 0 ? 0xffffffffu : 0u);
}

[ForceInline] u64key_t u64key_make(gindex_t hi, uint lo) {
    return uint2(lo, uint(hi));
}
[ForceInline] gindex_t u64key_hi(u64key_t k) { return int(k.y); }
[ForceInline] bool u64key_eq(u64key_t a, u64key_t b) {
    return all(a == b);
}
[ForceInline] u64key_t u64key_xor(u64key_t a, u64key_t b) { return a ^ b; }
[ForceInline] int u64key_clz(u64key_t k) {
    if (k.y != 0) return 31 - int(firstbithigh(k.y));
    return k.x != 0 ? 63 - int(firstbithigh(k.x)) : 64;
}
[ForceInline] uint u64key_digit(u64key_t k, uint shift, uint mask) {
    uint word = shift >= 32u ? (k.y >> (shift - 32u))
              : shift == 0u ? k.x
              : ((k.x >> shift) | (k.y << (32u - shift)));
    return word & mask;
}

[ForceInline] bool is_null<T>(T* p) {
    uint2 w = spirv_asm { result:$$uint2 = OpBitcast $p };
    return (w.x | w.y) == 0u;
}

#endif  // SS_EMULATE_INT64

// 32-bit overload so the radix sort macros work for both key widths.
[ForceInline] uint u64key_digit(uint k, uint shift, uint mask) {
    return (k >> shift) & mask;
}

// elem_at(base, i) -- base[i], but with the byte offset built explicitly.
// Drivers fold an OpPtrAccessChain element index into 32 bits, so a plain
// base[i] past 4 GiB wraps onto the start of the buffer and corrupts it.
#ifndef SS_EMULATE_INT64
[ForceInline] int* elem_at(int* b, gindex_t i) {
    return bit_cast<int*>(bit_cast<uint64_t>(b) + uint64_t(i) * 4);
}
[ForceInline] uint* elem_at(uint* b, gindex_t i) {
    return bit_cast<uint*>(bit_cast<uint64_t>(b) + uint64_t(i) * 4);
}
[ForceInline] float* elem_at(float* b, gindex_t i) {
    return bit_cast<float*>(bit_cast<uint64_t>(b) + uint64_t(i) * 4);
}
[ForceInline] int64_t* elem_at(int64_t* b, gindex_t i) {
    return bit_cast<int64_t*>(bit_cast<uint64_t>(b) + uint64_t(i) * 8);
}
[ForceInline] uint64_t* elem_at(uint64_t* b, gindex_t i) {
    return bit_cast<uint64_t*>(bit_cast<uint64_t>(b) + uint64_t(i) * 8);
}
#else
// gindex_t is 32-bit here, and the host rejects buffers these devices
// could not address anyway.
[ForceInline] int* elem_at(int* b, gindex_t i) { return b + i; }
[ForceInline] uint* elem_at(uint* b, gindex_t i) { return b + i; }
[ForceInline] float* elem_at(float* b, gindex_t i) { return b + i; }
[ForceInline] uint2* elem_at(uint2* b, gindex_t i) { return b + i; }
#endif

#endif  // SS_INT64_COMPAT
