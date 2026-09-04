#pragma once
// Getting a checkpoint onto disk: the shared cache directory, the download,
// and the SHA-256 that has to match before a parser sees the bytes.
//
// The URL tables stay with the models (aliked/model/Fetch.h,
// metric3d/model/Fetch.h) because which artifact to fetch is a model decision.
// Everything below that -- where it lands, how it is verified, what happens to
// a half-finished download -- is not, and there is one copy of it here.
//
// Nothing is bundled and nothing of ours is hosted anywhere, so the bytes we
// run are the bytes the reference implementation runs and a parity check
// compares two implementations rather than two checkpoints.

#include <cstdint>
#include <string>

namespace nn {

// One artifact. `sha256` is lowercase hex, and is checked after download and
// on every subsequent load of the cached file -- a truncated or tampered
// artifact must not reach the parser.
struct FetchFile {
    const char* file = nullptr;    // basename in the cache directory
    const char* url = nullptr;
    const char* sha256 = nullptr;
    uint64_t    bytes = 0;         // approximate, for the "downloading N MB" line
};

// <cache>/spirula-studio/models. Mirrors src/app/AppPaths.cpp's
// cache_dir(); duplicated rather than shared because src/nn/ sits below
// src/app/ in the layering and may not include it.
std::string model_cache_dir();
std::string cached_path(const FetchFile& f);

// A verified local copy, fetched with the system `curl` if missing; `tag`
// prefixes its progress lines. Throws nn::Error naming the URL to fetch by
// hand -- and, with SS_NO_AUTO_FETCH set, instead of downloading at all.
std::string ensure_file(const FetchFile& f, const char* tag);

// Lowercase hex SHA-256 of a file's contents. Empty when it cannot be read.
std::string sha256_file(const std::string& path);

}  // namespace nn
