#pragma once

// Which face the UI draws with, and how the full CJK faces get onto disk.
//
// Five faces are embedded and always loaded: one Latin/Greek/Cyrillic and one
// CJK per region, 3.3 MB together. Between them they cover the interface in
// all thirteen languages AND an ordinary file name in any of them with
// nothing to download, which is the whole design; the reasoning, the sizes
// and what is still not covered are in assets/fonts/README.md. The full
// 4-8 MB faces are still fetched on demand, for the tail a national
// common-use standard leaves out.

#include "i18n/Message.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace gui {

// One regional CJK face, downloadable in full.
struct CjkFace {
    const char* id;       // "sc" -- also the SS_FONT_CJK value that bundles it
    const char* file;     // basename on disk
    const char* url;
    const char* sha256;   // verified after download and on every load
    uint64_t    bytes;
    const char* label;    // "Simplified Chinese", for the download prompt
};

// The embedded counterpart: the same region, cut down to this program's own
// vocabulary. Generated into app_generated/cjk_subsets.h at configure time.
struct CjkSubset {
    const char* id;
    const unsigned char* data;
    size_t size;
};

// The face a language needs, or null for the Latin-script languages.
const CjkFace* cjk_face_for(spirula::i18n::Lang l);
const CjkFace* find_cjk_face(const std::string& id);

// Where that face would live, and whether a verified copy is there. Searched
// in order: $SS_FONT_DIR, <exe dir>/fonts (a regional build ships it there),
// then <cache dir>/fonts (what a download writes). Empty when not found.
std::string cjk_face_path(const CjkFace& f);
std::string cjk_face_download_path(const CjkFace& f);

// The atlas.
//
// Call ensure() once per frame, before ImGui::NewFrame(). It returns
// immediately unless the language changed or invalidate() was called, because
// deciding what to load means hashing a multi-megabyte file and that is not a
// per-frame job.
class FontSet {
public:
    void ensure();

    // Re-examine the disk on the next ensure(). Call when a font download
    // finishes; nothing else changes the answer.
    void invalidate() { _dirty = true; }

    // The full face for the current language, when it is not installed. NOT
    // an error state: the UI and ordinary file names both render without it.
    // It is what the language menu offers, for the rare characters it adds.
    const CjkFace* optional_face() const { return _optional; }

    // A build with SS_FONT_CJK=none has no fetch path, and should say so
    // rather than offer a download that will not happen.
    static bool fetch_enabled();

private:
    void rebuild();

    std::string _loaded_cjk;              // full face id currently in the atlas
    std::string _lead_cjk;                // region whose subset is merged first
    const CjkFace* _optional = nullptr;
    std::vector<char> _cjk_data;          // must outlive the atlas
    spirula::i18n::Lang _lang = spirula::i18n::Lang::en;
    bool _built = false;
    bool _dirty = true;
};

}  // namespace gui
