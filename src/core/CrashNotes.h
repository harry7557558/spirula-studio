#pragma once

// Lines the crash report (app/CrashLog.cpp) quotes about the machine: the GPU
// and driver each runtime opened, the Vulkan loader and its layers. Any
// library may set one; the handler only reads, so a racing update costs a
// garbled line, not a crash. Header-only: the runtimes do not link app/.

#include <algorithm>
#include <cstring>
#include <mutex>
#include <string>

namespace spirula {

struct CrashNote {
    char key[24];
    char text[1024];   // the last byte is never written: always terminated
};

constexpr int kCrashNotes = 8;

inline CrashNote* crash_notes() {
    static CrashNote notes[kCrashNotes] = {};
    return notes;
}

// Replaces the line `key`, or adds it while a slot is free. Slots are never
// emptied, so a key already set always comes before the first free one.
inline void set_crash_note_line(const char* key, const std::string& text) {
    static std::mutex mu;
    std::lock_guard<std::mutex> lock(mu);
    CrashNote* slot = nullptr;
    for (int i = 0; i < kCrashNotes && !slot; i++) {
        CrashNote& n = crash_notes()[i];
        if (!n.key[0] || std::strncmp(n.key, key, sizeof n.key - 1) == 0) slot = &n;
    }
    if (!slot) return;
    const size_t len = std::min(text.size(), sizeof slot->text - 1);
    std::memcpy(slot->text, text.data(), len);
    std::memset(slot->text + len, 0, sizeof slot->text - len);
    if (!slot->key[0])
        std::memcpy(slot->key, key, std::min(std::strlen(key), sizeof slot->key - 1));
}

}  // namespace spirula
