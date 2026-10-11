#pragma once

#include <cstddef>
#include <stdexcept>

#include "sfm/core/Model.h"

namespace sfm {

namespace model_memory_detail {

inline size_t addBytes(size_t a, size_t b) {
    if (b > SIZE_MAX - a) throw std::runtime_error("model memory estimate overflow");
    return a + b;
}

template <class T> inline size_t vectorBytes(const std::vector<T>& v) {
    if (v.capacity() > SIZE_MAX / sizeof(T))
        throw std::runtime_error("model vector size overflow");
    return v.capacity() * sizeof(T);
}

}  // namespace model_memory_detail

inline size_t modelResidentBytes(const Reconstruction& m) {
    using namespace model_memory_detail;
    size_t bytes = sizeof(Reconstruction);
    auto add = [&](size_t n) { bytes = addBytes(bytes, n); };
    for (const auto& kv : m.cameras) add(sizeof(kv) + 64);
    for (const auto& kv : m.images) {
        add(sizeof(kv) + 64); add(kv.second.name.capacity() + 1);
        add(vectorBytes(kv.second.points2D)); add(vectorBytes(kv.second.point3D_ids));
    }
    for (const auto& kv : m.points3D) {
        add(sizeof(kv) + 64); add(vectorBytes(kv.second.track));
    }
    add(vectorBytes(m.rigs));
    for (const RigCalib& r : m.rigs) {
        add(vectorBytes(r.cam_from_rig)); add(vectorBytes(r.established)); add(vectorBytes(r.fixed));
        add(vectorBytes(r.support)); add(vectorBytes(r.spread_deg)); add(vectorBytes(r.declined_at));
    }
    for (uint32_t id : m.rig_detached) { (void)id; add(sizeof(uint32_t) + 64); }
    return bytes;
}

}  // namespace sfm
