// ffmpeg_probe_test -- an ffmpeg that is on disk but cannot start (built for
// another processor, a dead interpreter) is reported as that, not as a video
// with no tracks.

#include "app/FfmpegVideo.h"
#include "app/gui/Subprocess.h"

#include <atomic>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>

namespace fs = std::filesystem;

namespace {

int g_failed = 0;

void check(bool ok, const std::string& what) {
    std::printf("%-4s %s\n", ok ? "ok" : "FAIL", what.c_str());
    if (!ok) g_failed++;
}

}  // namespace

int main() {
    static const std::atomic<bool> never{false};
    app::VideoFacts facts;

    const bool missing =
        app::ffmpeg_probe_video("/nonexistent/dir/ffmpeg", "/dev/null", facts, never);
    check(!missing && facts.tracks.empty(), "a missing ffmpeg probes nothing");
    check(facts.error.empty(), "...and is not an ffmpeg that failed to start");

#ifndef _WIN32
    const fs::path dir = fs::temp_directory_path() / "ss_ffmpeg_probe_test";
    fs::create_directories(dir);
    const fs::path fake = dir / "ffmpeg";
    {
        std::ofstream f(fake);
        f << "#!/nonexistent/interpreter\n";
    }
    fs::permissions(fake, fs::perms::owner_all);
    check(gui::command_exists(fake.string()), "the fake ffmpeg passes command_exists");

    const bool ran = app::ffmpeg_probe_video(fake.string(), "/dev/null", facts, never);
    check(!ran && facts.tracks.empty(), "an ffmpeg that cannot start probes nothing");
    std::printf("      error: <%s>\n", facts.error.c_str());
    check(facts.error.rfind("cannot run ", 0) == 0 &&
              facts.error.find(fake.string()) != std::string::npos,
          "...and says so, naming the executable");
    fs::remove_all(dir);
#endif

    std::printf("%s\n", g_failed ? "FAILED" : "all passed");
    return g_failed ? 1 : 0;
}
