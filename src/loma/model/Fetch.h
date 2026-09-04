#pragma once
// Getting a LoMa checkpoint onto disk.
//
// The artifacts are the author's own ONNX exports, fetched from the release
// COLMAP fetches them from (src/colmap/feature/resources.h) and verified
// against their SHA-256. We host nothing and convert nothing, so the bytes we
// run are the bytes the reference implementation runs and a parity check
// compares implementations rather than checkpoints.
//
// LoMa is MIT except the matcher, which inherits LightGlue's Apache-2.0, so
// this needs no consent gate. It still never downloads behind the user's back.

#include "nn/io/Fetch.h"

#include <string>

namespace loma {

// A checkpoint we know how to fetch. The download, the cache directory and the
// SHA-256 check are nn::ensure_file's; what is model-specific is which
// artifact and what to call it.
struct ModelSource {
    const char*   id;    // "loma-b128" -- what --matcher spells
    nn::FetchFile onnx;
};

// Null when `id` is not one of ours.
const ModelSource* find_model_source(const std::string& id);

// Where a cached checkpoint lives: <cache>/spirula-studio/models/<file>.
std::string model_cache_path(const ModelSource& src);

// A path to a verified local copy, downloading through the system `curl` if
// needed. Throws nn::Error with the URL to fetch by hand when that fails.
std::string ensure_model(const ModelSource& src);

// An explicit path is used as-is; an id is fetched. `what` names the role in
// the error message ("detector", "descriptor", "matcher").
std::string resolve_model(const std::string& id_or_path, const char* what);

}  // namespace loma
