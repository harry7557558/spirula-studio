#pragma once

// Cross-platform owned subprocesses for external tools (colmap, ffmpeg).
// These APIs terminate owned descendants on cancel/kill, owner death, or
// direct-child exit. Windows uses non-inherited kill-on-close jobs assigned at
// creation; POSIX uses an owner-death pipe guardian and isolated process group.
// run_process/finish join managed cleanup; kill only signals cancellation.
// POSIX tools must retain private lease FDs/metadata and stay in their group.
// open_url is detached. The CLI decoder links this GUI-free implementation too.

#include <cstddef>
#include <atomic>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace gui {

// Exit codes returned by run_process in addition to the child's own.
constexpr int kSpawnFailed = -1;   // executable not found / spawn error
constexpr int kCancelled   = -2;   // cancel flag was set; process killed

// Blocking; call from a worker. Empty cwd inherits; stderr is merged into stdout.
// on_line receives complete lines without newlines. Cancellation is polled ~10 Hz.
// on_poll runs on the same worker even while the child produces no output.
int run_process(const std::vector<std::string>& argv,
                const std::string& cwd,
                const std::function<void(const std::string&)>& on_line,
                const std::atomic<bool>& cancel,
                const std::function<void()>& on_poll = {});

// A child whose STDIN this process writes -- raw frames into an encoder.
// Its merged stdout and stderr still arrive line by line, on a thread of the
// pipe's own. write() and finish() belong to one thread.
class ProcessPipe {
public:
    ProcessPipe();
    ~ProcessPipe();
    bool start(const std::vector<std::string>& argv,
               std::function<void(const std::string&)> on_line);
    // False once the child has stopped reading, which is how a crashed
    // encoder is noticed.
    bool write(const void* data, size_t bytes);
    // Close stdin and join owned cleanup: exit code, or kSpawnFailed.
    int finish();
    void kill();

private:
    struct Impl;
    std::unique_ptr<Impl> _impl;
};

// A child whose STDOUT this process reads as bytes -- decoded frames out of
// ffmpeg. Its stderr arrives line by line on a thread of the pipe's own.
class ProcessReader {
public:
    ProcessReader();
    ~ProcessReader();
    bool start(const std::vector<std::string>& argv,
               std::function<void(const std::string&)> on_line);
    // Up to `bytes`; 0 at end of stream or on error. Blocks.
    size_t read(void* data, size_t bytes);
    // Join owned cleanup: exit code, or kSpawnFailed.
    int finish();
    void kill();

private:
    struct Impl;
    std::unique_ptr<Impl> _impl;
};

// One caller per read/write; join both before finish() drains stdout.
// kill() unblocks I/O; the destructor kills and joins. stderr feeds on_stderr.
class ProcessDuplex {
public:
    ProcessDuplex();
    ~ProcessDuplex();
    bool start(const std::vector<std::string>& argv,
               std::function<void(const std::string&)> on_stderr);
    // Full write or false; read may be short and returns zero at EOF/error.
    bool write(const void* data, size_t bytes);
    size_t read(void* data, size_t bytes);
    int finish();
    void kill();

private:
    struct Impl;
    std::unique_ptr<Impl> _impl;
};

// True when `exe` resolves to an executable (PATH search like the shell).
bool command_exists(const std::string& exe);

// Splits a flag or command string the way a shell would for the simple cases:
// whitespace separates (a line break included), "quoted runs" stay together,
// and a backslash before a break continues the line instead of being an arg.
std::vector<std::string> split_args(const std::string& s);

// A MESSAGE made safe to hand to another program: no backslash, no straight
// quote, no control character, so it needs no escaping in a JSON payload or a
// command line. Quotes curl (” ’), a backslash becomes '/', spaces collapse.
std::string safe_arg(const std::string& text);

// What stands for the summary inside a finish command (a batch queue's, or
// --finish-command's). Braces because the alternatives are punctuation a shell
// would eat: `<>` is redirection, `%..%` and `$..` are variables.
inline constexpr char kMessageToken[] = "{message}";

// split_args(command) with every `token` inside an argument replaced by
// safe_arg(value). The value lands in ONE argument whether or not the token
// was quoted, so nothing in it can be read as syntax.
std::vector<std::string> command_argv(const std::string& command,
                                      const std::string& token,
                                      const std::string& value);

// Hands a URL to the desktop's default browser and returns immediately. False
// when nothing could be launched (a headless session, no xdg-open), which is
// the caller's cue to fall back to showing the address. Never blocks and never
// waits for the browser to exit.
bool open_url(const std::string& url);

}  // namespace gui
