#include "app/Tools.h"
#include "dense/ConfigFields.h"
#include "i18n/Locale.h"
#include "i18n/catalog/Dense.h"
#ifdef SS_HAVE_ROMA
#include "app/DenseProcessing.h"
#include "nn/Device.h"
#endif

#include <algorithm>
#include <atomic>
#include <csignal>
#include <cstdio>
#include <filesystem>
#include <fstream>

namespace {
using namespace spirula::dense;
using spirula::i18n::format;
namespace D = spirula::i18n::msg::dense;
std::atomic<bool> interrupted{false};
// The first Ctrl+C stops at the next check and keeps finished pairs; a second one ends the process.
void interrupt(int signal) {
    if (interrupted.exchange(true)) { std::signal(signal, SIG_DFL); std::raise(signal); }
}

template<class T> void command_value(DenseConfig& config, const char* key, const T&, const std::string& text) {
    JsonValue value;
    if constexpr (std::is_same_v<T, std::string>) value = json_parse(json_quote(text));
    else if constexpr (std::is_same_v<T, bool> || std::is_same_v<T, std::optional<bool>>) {
        if (text == "1" || text == "true") value = json_parse("true");
        else if (text == "0" || text == "false") value = json_parse("false");
        else if (text == "null" && std::is_same_v<T, std::optional<bool>>) value = json_parse("null");
        else throw std::runtime_error("invalid boolean for --" + std::string(key));
    } else value = json_parse(text);
    assign_config_field(config, key, value);
}

bool command_field(DenseConfig& config, std::string key, const std::string& value) {
    std::replace(key.begin(), key.end(), '-', '_');
    if (key == "preset") { config.apply_preset(value); return true; }
    if (key == "source_workflow") {
        if (value == "true" || value == "1") config.apply_source_workflow();
        else if (value != "false" && value != "0") throw std::runtime_error("source-workflow expects true or false");
        return true;
    }
    if (key == "precision") {
        if (value == "auto") config.match.precision = spirula::roma::InferencePrecision::Automatic;
        else if (value == "float32") config.match.precision = spirula::roma::InferencePrecision::Float32;
        else if (value == "mixed") config.match.precision = spirula::roma::InferencePrecision::Mixed;
        else throw std::runtime_error("dense precision must be auto, float32, or mixed");
        return true;
    }
    if (key == "pair_mode") {
        if (value == "automatic") config.pairs.mode = PairMode::Automatic;
        else if (value == "sequential") config.pairs.mode = PairMode::Sequential;
        else if (value == "exhaustive") config.pairs.mode = PairMode::Exhaustive;
        else if (value == "explicit") config.pairs.mode = PairMode::Explicit;
        else throw std::runtime_error("unknown dense pair mode: " + value);
        return true;
    }
#define SS_DENSE_COMMAND(name, member) if (key == #name) { command_value(config, #name, config.member, value); return true; }
    SS_DENSE_CONFIG_FIELDS(SS_DENSE_COMMAND)
#undef SS_DENSE_COMMAND
    return false;
}

void help() {
    std::printf("%s\n\n%s <dataset> [--preset fast|balanced|high|custom] [--config PATH]\n\n%s\n\n",
                format(D::title, {}).c_str(), app::program_name().c_str(), format(D::help, {}).c_str());
    const DenseConfig defaults;
#define SS_DENSE_HELP(key, member) { std::string name = #key; std::replace(name.begin(), name.end(), '_', '-'); \
    std::printf("  --%-30s %s\n", name.c_str(), name == "precision" ? "auto (auto|float32|mixed)" : json_field::emit(defaults.member).c_str()); }
    SS_DENSE_CONFIG_FIELDS(SS_DENSE_HELP)
#undef SS_DENSE_HELP
    std::printf("  --source-workflow true|false\n  --config PATH\n  --write-config PATH\n  --progress-dir DIR\n  --perf-dir DIR\n  --help\n");
}

}  // namespace

int spirula_dense_main(int argc, char** argv) {
    app::set_program_name(argc ? argv[0] : nullptr, "spirula dense");
    try {
        DenseConfig config;
        std::string dataset, write_config, progress_dir, perf_dir;
        for (int i = 1; i < argc; ++i) {
            const std::string option = argv[i];
            if (option == "--help" || option == "-h") { help(); return 0; }
            if (option.rfind("--", 0) != 0) {
                if (!dataset.empty()) throw std::runtime_error("dense expects one dataset directory");
                dataset = option; continue;
            }
            if (i + 1 == argc) throw std::runtime_error("missing value for " + option);
            const std::string value = argv[++i];
            if (option == "--config") read_config(config, json_parse_file(value));
            else if (option == "--write-config") write_config = value;
            else if (option == "--progress-dir") progress_dir = value;
            else if (option == "--perf-dir") perf_dir = value;
            else if (!command_field(config, option.substr(2), value)) throw std::runtime_error("unknown dense option: " + option);
        }
        config.validate_run();
        if (!write_config.empty()) {
            const auto parent = std::filesystem::path(write_config).parent_path();
            if (!parent.empty()) std::filesystem::create_directories(parent);
            std::ofstream file(write_config); file << config_json(config); file.flush();
            if (!file) throw std::runtime_error("cannot write dense settings: " + write_config);
            return 0;
        }
        if (dataset.empty()) { help(); return 1; }
#ifdef SS_HAVE_ROMA
        interrupted.store(false);
        std::signal(SIGINT, interrupt); std::signal(SIGTERM, interrupt);
        // Fusion reports permille of its steps, so it prints as a percentage.
        auto progress = [](const char* stage, uint64_t done, uint64_t total) {
            const std::string name = stage;
            const auto* message = &D::prepare;
            if (name == "load") message = &D::load_model;
            else if (name == "match") message = &D::match;
            else if (name == "refine") message = &D::refine;
            else if (name == "fuse") message = &D::fuse;
            else if (name == "outliers") message = &D::outliers;
            else if (name == "export") message = &D::export_cloud;
            else if (name == "complete") return;
            const auto label = format(*message, {});
            const auto text = name == "fuse" && total ? format(D::progress_percent, {label, (long long)(done * 100 / total)})
                : total ? format(D::progress, {label, (long long)done, (long long)total}) : format(D::count, {label, (long long)done});
            static std::string last;   // finer steps than the text shows would repeat it
            if (text == last) return;
            last = text;
            std::printf("%s\n", text.c_str()); std::fflush(stdout);
        };
        const auto result = app::run_dense(dataset, config, progress, &interrupted, progress_dir, perf_dir);
        nn::shutdown();
        std::printf("%s\n", format(D::completed, {result.cloud, (long long)result.statistics.exported, result.seconds}).c_str());
        return 0;
#else
        throw std::runtime_error("RoMa inference is unavailable in this build; rebuild with SS_BUILD_SAM=ON");
#endif
    } catch (const std::exception& e) {
        std::fprintf(stderr, "%s\n", format(D::error, {e.what()}).c_str());
#ifdef SS_HAVE_ROMA
        nn::shutdown();
#endif
        return 1;
    }
}
