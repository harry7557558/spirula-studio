// Portable float atomic add (Vulkan backend). VK_EXT_shader_atomic_float is
// not baseline (and absent under MoltenVK), so the default atomic_add_f32 is
// a compare-exchange loop over the value's u32 bit image — same
// nondeterministic accumulation order as CUDA atomicAdd, so parity stays
// tolerance-based wherever this is used. The loop's exit is a successful
// CAS (well-defined; not a UB-terminated loop — see README loop rules).
//
// Under -DSS_NATIVE_F32_ATOMIC_ADD the emulation is replaced by the
// native OpAtomicFAddEXT (SPV_EXT_shader_atomic_float_add): spirv_tool.cpp
// compiles every entry whose include closure reaches this file a second
// time with the define, and the pipeline layer picks the native blob when
// the device enabled shaderBufferFloat32AtomicAdd — so the choice is made
// at pipeline creation, never as a runtime branch in the shader.

// Float atomic max over a buffer declared as bits. Non-negative values only:
// across those, the unsigned order of the bit patterns is the float order, so
// one unsigned atomic does it. Common.cuh's atomicMax(float*, float) covers
// the negative half too, with an unsigned min over an int* view of the same
// address -- which is exactly what cannot be spelled here. MoltenVK's
// SPIR-V -> MSL translation drops the bitcast between two physical-storage
// pointer types and emits `reinterpret_cast<device int*>` of the *loaded
// float*, which the Metal compiler rejects, taking the whole shader library
// with it. Hence bits in, bits out, and one pointer type per buffer.
[ForceInline]
void atomic_max_f32_nonneg(uint* addr, float v) {
    InterlockedMax(addr[0], asuint(v));
}

#ifdef SS_NATIVE_F32_ATOMIC_ADD

[ForceInline]
void atomic_add_f32(float* addr, float v) {
    if (v == 0.0f) return;
    InterlockedAdd(addr[0], v);
}

#else

#ifdef SS_XCHG_FLOAT_ADD  // Windows builds (SpirvShaders.cmake)

// Set on AMD's Windows driver (amd_windows_emulated_fadd). The ID must match
// kXchgFloatAddSpecId in VulkanPipelines.cpp and stay clear of the 0..n-1
// that each kernel's own constants take in declaration order.
[vk::constant_id(1000)]
const bool kXchgFloatAdd = false;

// Needs shaderBufferFloat32Atomics. AMD's Windows compiler loses the index of a
// float pointer reinterpreted as uint*, so its CAS loop hit the array base
// (issue #23). Not inlined: slangc 2026.12 aborts at 290 call sites.
[noinline]
void atomic_add_f32_xchg(float* addr, float v) {
    float carry = v;
    for (;;) {
        float cur;
        InterlockedExchange(addr[0], 0.0f, cur);
        float back;  // whatever another thread deposited meanwhile
        InterlockedExchange(addr[0], cur + carry, back);
        if (back == 0.0f) break;
        carry = back;
    }
}

#endif

[ForceInline]
void atomic_add_f32(float* addr, float v) {
    if (v == 0.0f) return;
#ifdef SS_XCHG_FLOAT_ADD
    if (kXchgFloatAdd) {
        atomic_add_f32_xchg(addr, v);
        return;
    }
#endif
    uint* w = (uint*)addr;
    uint expected = w[0];
    for (;;) {
        uint desired = asuint(asfloat(expected) + v);
        uint prev;
        InterlockedCompareExchange(w[0], expected, desired, prev);
        if (prev == expected) break;
        expected = prev;
    }
}

#endif
