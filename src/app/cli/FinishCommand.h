#pragma once

#include "app/gui/Subprocess.h"
#include "i18n/catalog/Log.h"

#include <cstdio>

namespace spirula::cli {

inline void run_finish_command(const std::string& command, const std::string& message,
                               const std::atomic<bool>& cancel = std::atomic<bool>{false}) {
    using i18n::format;
    namespace log = i18n::msg::log;
    if (command.empty() || cancel.load()) return;
    const auto argv = gui::command_argv(command, gui::kMessageToken, message);
    if (argv.empty()) return;
    std::printf("%s\n", format(log::finish_cmd_running, {argv.front()}).c_str());
    std::fflush(stdout);
    const int code = gui::run_process(argv, "", [](const std::string& line) {
        std::printf("%s\n", line.c_str());
        std::fflush(stdout);
    }, cancel);
    if (code == gui::kCancelled) return;
    if (code == gui::kSpawnFailed)
        std::fprintf(stderr, "%s\n", format(log::finish_cmd_missing, {argv.front()}).c_str());
    else if (code != 0)
        std::fprintf(stderr, "%s\n", format(log::finish_cmd_exit, {(long long)code}).c_str());
    else
        std::printf("%s\n", log::finish_cmd_ok.get());
}

}  // namespace spirula::cli
