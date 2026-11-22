#pragma once

// <dataset>/logs/<kind>_<stamp>.log: a settings snapshot, a separator, then
// the run's log lines stamped with local time. The GUI and `spirula train`
// both write it through these.

#include "config/TrainConfig.h"

#include <array>
#include <filesystem>
#include <fstream>
#include <optional>
#include <string>

namespace spirula::runlog {

// "[yyyy-MM-dd HH:mm:ss.fff] ", local time.
std::string line_stamp();
// yyyyMMddHHmmss, local time: the file name's stamp and `run_started`.
std::string file_stamp();

// The folder a run's logs/ goes in: `data`, or its parent when `data` is the
// images folder itself.
std::string dataset_dir(const std::string& data);

// Opens <dataset_dir>/logs/<kind>_<stamp>.log for append. Empty path, and `f`
// left closed, when the folder cannot be made.
std::filesystem::path open(std::ofstream& f, const std::string& dataset_dir,
                           const char* kind, const std::string& stamp);

// One stamped line per line of `s`, flushed.
void write_line(std::ofstream& f, const std::string& s);

std::string cfg_str(const std::string& v);
std::string cfg_str(bool v);
std::string cfg_str(int v);
std::string cfg_str(float v);
template <typename T>
std::string cfg_str(const std::optional<T>& v) {
    return v ? cfg_str(*v) : std::string("none");
}
template <typename T, size_t N>
std::string cfg_str(const std::array<T, N>& v) {
    std::string s;
    for (size_t i = 0; i < N; i++) {
        if (i) s += ',';
        s += cfg_str(v[i]);
    }
    return s;
}

// "[<heading>]\n" and "  key = value\n", the snapshot's two line shapes.
std::string section(const std::string& name);
std::string entry(const std::string& key, const std::string& value);

// The snapshot's first heading, above run_started.
std::string run_heading();
// The training config, grouped the way the options editor groups it.
std::string train_sections(const TrainConfig& c);
// The rule that ends the snapshot.
std::string settings_end();

}  // namespace spirula::runlog
