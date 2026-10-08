#pragma once

#include "dense/DenseConfig.h"
#include "data/JsonField.h"

#define SS_DENSE_CONFIG_FIELDS(X) \
    X(preset, preset) X(checkpoint, checkpoint) \
    X(low_width, match.low_width) X(low_height, match.low_height) \
    X(high_width, match.high_width) X(high_height, match.high_height) \
    X(bidirectional, match.bidirectional) X(overlap_saturation, match.overlap_saturation) \
    X(memory_budget_bytes, match.memory_budget_bytes) X(precision, match.precision) \
    X(pair_mode, pairs.mode) X(neighbors, pairs.neighbors) X(reference_coverage, pairs.reference_coverage) \
    X(sequence_window, pairs.sequence_window) X(max_pairs, pairs.max_pairs) X(pair_list, pair_list) \
    X(min_angle_degrees, geometry.min_angle_degrees) \
    X(max_reprojection_error, geometry.max_reprojection_error) \
    X(max_relative_depth_error, geometry.max_relative_depth_error) \
    X(min_source_images, geometry.min_source_images) \
    X(image_dir, image_dir) X(recon_dir, recon_dir) \
    X(mask_dir, mask_dir) X(feature_mask_dir, feature_mask_dir) \
    X(use_masks, use_masks) X(training_masks, training_masks) X(alpha_masks, alpha_masks) X(feature_masks, feature_masks) \
    X(invert_masks, invert_masks) X(mask_threshold, mask_threshold) X(mask_boundary, mask_boundary) \
    X(exif_orientation, exif_orientation) X(metashape_xml, metashape_xml) \
    X(metashape_psx, metashape_psx) X(metashape_component, metashape_component) \
    X(split_views, split_views) X(sparse_face_pairs, sparse_face_pairs) X(max_face_size, max_face_size) \
    X(matching_space, matching_space) X(reference_fraction, reference_fraction) \
    X(samples_per_reference, samples_per_reference) X(sampling_seed, sampling_seed) \
    X(source_reprojection_error, source_reprojection_error) \
    X(stride, stride) X(point_limit, point_limit) X(min_overlap, min_overlap) \
    X(max_cycle_error, max_cycle_error) X(cycle_check, cycle_check) X(voxel_size, voxel_size) \
    X(remove_outliers, remove_outliers) X(outlier_neighbors, outlier_neighbors) \
    X(outlier_stddev, outlier_stddev) X(image_cache_bytes, image_cache_bytes) \
    X(cpu_workers, cpu_workers) X(resume, resume) X(rebuild, rebuild) X(keep_cache, keep_cache) \
    X(device, device) X(image_gamut, image_gamut) X(image_exposure, image_exposure) \
    X(image_is_linear, image_is_linear)

namespace spirula::dense {

template<class T> inline void assign_dense_value(T& out, const JsonValue& value) {
    bool valid = false;
    if constexpr (std::is_same_v<T, std::string>) valid = value.type == JsonValue::Type::String || value.is_null();
    else if constexpr (std::is_same_v<T, bool> || std::is_same_v<T, std::optional<bool>>)
        valid = value.type == JsonValue::Type::Bool || (std::is_same_v<T, std::optional<bool>> && value.is_null());
    else valid = value.type == JsonValue::Type::Number && std::isfinite(value.num);
    if (!valid) throw std::runtime_error("dense setting has the wrong JSON type");
    if constexpr (std::is_same_v<T, int> || std::is_enum_v<T>)
        if (std::floor(value.num) != value.num || value.num < INT32_MIN || value.num > INT32_MAX)
            throw std::runtime_error("dense setting requires an integer in range");
    json_field::assign(out, value);
}

inline std::string config_json(const DenseConfig& config) {
    JsonWriter w;
    w.object();
#define SS_DENSE_EMIT(key, member) w.field_raw(#key, json_field::emit(config.member));
    SS_DENSE_CONFIG_FIELDS(SS_DENSE_EMIT)
#undef SS_DENSE_EMIT
    w.end();
    return w.str();
}

inline bool assign_config_field(DenseConfig& config, const std::string& key, const JsonValue& value) {
#define SS_DENSE_ASSIGN(name, member) if (key == #name) { assign_dense_value(config.member, value); return true; }
    SS_DENSE_CONFIG_FIELDS(SS_DENSE_ASSIGN)
#undef SS_DENSE_ASSIGN
    return false;
}

inline void read_config(DenseConfig& config, const JsonValue& object) {
    if (!object.is_object()) throw std::runtime_error("dense settings must be a JSON object");
    if (const auto* preset = object.find("preset")) config.apply_preset(preset->as_string());
    for (const auto& field : object.obj)
        if (!assign_config_field(config, field.first, field.second))
            throw std::runtime_error("unknown dense setting: " + field.first);
    // Written before reference coverage existed: every image was a reference, and every
    // prediction was kept because that was the default then, not a choice anyone made.
    if (!object.find("reference_coverage")) {
        if (!is_dense_preset(config.preset)) config.pairs.reference_coverage = 0;
        config.keep_cache = false;
    }
    config.validate();
}

}  // namespace spirula::dense

namespace json_field {
inline std::string emit(const spirula::dense::DenseConfig& config) { return spirula::dense::config_json(config); }
inline void assign(spirula::dense::DenseConfig& config, const JsonValue& value) {
    if (!value.is_null()) spirula::dense::read_config(config, value);
}
}
