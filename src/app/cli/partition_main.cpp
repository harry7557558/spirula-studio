// `spirula partition` -- split a dataset into parts that train separately
// (data/ScenePartition.h) and merge the trained parts back into one model
// (checkpoint/SplatMerge.h). Host-only; every build has it.

#include "app/Tools.h"

#include "checkpoint/SplatMerge.h"
#include "checkpoint/SplatPly.h"
#include "data/DatasetParser.h"
#include "data/ScenePartition.h"
#include "data/SparseEdit.h"
#include "i18n/Locale.h"
#include "i18n/catalog/Partition.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <string>
#include <vector>

namespace fs = std::filesystem;
namespace P = spirula::i18n::msg::partition;

using spirula::i18n::format;

namespace {

void help_row(const char* flags, const spirula::i18n::Msg& m, int col = 30) {
    std::string left = std::string("    ") + flags;
    if (spirula::i18n::display_width(left) >= col + 4) {
        std::fprintf(stderr, "%s\n", left.c_str());
        left.clear();
    }
    left = spirula::i18n::pad_to(left, col + 4);
    for (const std::string& line : spirula::i18n::wrap(m.get(), 86 - col - 4)) {
        std::fprintf(stderr, "%s%s\n", left.c_str(), line.c_str());
        left.assign((size_t)col + 4, ' ');
    }
}

void usage() {
    const std::string prog = app::program_name();
    std::fprintf(stderr, "%s -- %s\n\n", prog.c_str(), P::tagline.get());
    std::fprintf(stderr, "    %s split <dataset> [options]\n", prog.c_str());
    std::fprintf(stderr, "    %s merge <partition.json> [options]\n\n", prog.c_str());
    for (const std::string& l : spirula::i18n::wrap(P::usage_intro.get(), 80))
        std::fprintf(stderr, "    %s\n", l.c_str());
    std::fprintf(stderr, "\n%s\n", P::head_split.get());
    help_row("--parts <n>", P::opt_parts);
    help_row("--max-images <n>", P::opt_max_images);
    help_row("--method graph|viewgraph", P::opt_method);
    help_row("--ring <fraction>", P::opt_ring);
    help_row("--ring-min-points <n>", P::opt_ring_min);
    help_row("--max-seeds <n>", P::opt_max_seeds);
    help_row("--source auto|tracks|projection|proximity", P::opt_source);
    help_row("-o, --output <file>", P::opt_output);
    std::fprintf(stderr, "\n%s\n", P::head_merge.get());
    help_row("--runs <dir>", P::opt_runs);
    help_row("--part <k> <run|ply>", P::opt_merge_part);
    help_row("-o, --output <file>", P::opt_merge_output);
    std::fprintf(stderr, "\n%s --lang <code>\n", P::label_common.get());
}

int bad_value(const char* value, const char* flag) {
    std::fprintf(stderr, "%s\n", format(P::err_bad_value, {value, flag}).c_str());
    return 2;
}

int run_split(int argc, char** argv) {
    spirula::PartitionOptions opt;
    std::string dataset, output;
    for (int i = 1; i < argc; ++i) {
        const std::string a = argv[i];
        auto next = [&]() -> const char* {
            if (i + 1 >= argc) {
                std::fprintf(stderr, "%s needs a value\n", a.c_str());
                std::exit(2);
            }
            return argv[++i];
        };
        if (a == "--help" || a == "-h") { usage(); return 0; }
        else if (a == "--parts") opt.parts = std::atoi(next());
        else if (a == "--max-images") opt.max_images = std::atoi(next());
        else if (a == "--ring") opt.ring_fraction = (float)std::atof(next());
        else if (a == "--ring-min-points") opt.ring_min_points = std::atoi(next());
        else if (a == "--max-seeds") opt.max_seeds = std::atoi(next());
        else if (a == "--method") {
            const char* v = next();
            if (!spirula::partition_method_from_name(v, opt.method)) return bad_value(v, "--method");
        }
        else if (a == "--source") {
            const char* v = next();
            if (!spirula::covisibility_source_from_name(v, opt.source)) return bad_value(v, "--source");
        } else if (a == "-o" || a == "--output") output = next();
        else if (!a.empty() && a[0] == '-') {
            std::fprintf(stderr, "unknown option '%s'\n\n", a.c_str());
            usage();
            return 2;
        } else if (dataset.empty()) dataset = a;
        else { usage(); return 2; }
    }
    if (dataset.empty()) {
        std::fprintf(stderr, "%s\n", format(P::err_usage, {app::program_name()}).c_str());
        return 2;
    }
    if (opt.parts < 0 || opt.max_images < 1 || opt.max_seeds < 1000 || opt.ring_fraction < 0)
        return bad_value("", opt.parts < 0 ? "--parts" : opt.max_images < 1 ? "--max-images"
                                          : opt.max_seeds < 1000 ? "--max-seeds" : "--ring");

    std::printf("%s\n", format(P::log_reading, {dataset}).c_str());
    DatasetParserConfig dcfg;
    dcfg.require_image_files = false;
    const ParsedDataset ds = parse_dataset(dataset, dcfg, "");
    const spirula::SparseStats tracks = spirula::read_sparse_stats(dataset);
    std::printf("%s\n", format(P::log_dataset,
                               {(long long)ds.num_cameras, (long long)ds.points.num(),
                                tracks.empty() ? P::word_no.get() : P::word_yes.get()}).c_str());

    auto log = [](const std::string& s) { std::printf("%s\n", s.c_str()); };
    const spirula::Covisibility cov = spirula::build_covisibility(ds, &tracks, opt, log);
    std::printf("%s\n", format(P::log_covisibility,
                               {spirula::covisibility_source_name(cov.source),
                                (long long)(cov.cameras.adj.size() / 2)}).c_str());
    const spirula::ScenePartition part = spirula::partition_scene(ds, cov, opt);
    char cut[32];
    std::snprintf(cut, sizeof cut, "%.1f", 100.0 * part.cut_fraction);
    std::printf("%s\n", format(P::log_summary, {part.num_parts, cut,
                                                 (long long)(part.field ? part.field->num_seeds() : 0)}).c_str());
    for (int k = 0; k < part.num_parts; k++) {
        std::printf("%s\n", format(P::log_part, {k, (long long)part.core_count(k),
                                                 (long long)part.ring[(size_t)k].size(),
                                                 (long long)part.part_points[(size_t)k].size(),
                                                 (long long)std::lround(100.0 * part.view_share[(size_t)k])})
                                .c_str());
        if (part.core_pieces[(size_t)k] > 1)
            std::printf("%s\n", format(P::log_part_pieces, {k, part.core_pieces[(size_t)k]}).c_str());
    }

    if (output.empty()) output = (fs::path(dataset) / "partition.json").string();
    std::error_code ec;
    const std::string dataset_abs = fs::absolute(dataset, ec).lexically_normal().string();
    spirula::write_partition(part, output, ec ? dataset : dataset_abs);
    std::printf("%s\n", format(P::log_written, {output}).c_str());
    std::string prog = app::program_name();
    const size_t sp = prog.find(' ');
    if (sp != std::string::npos) prog = prog.substr(0, sp);
    std::printf("%s\n", format(P::log_next, {prog, dataset, output}).c_str());
    return 0;
}

int run_merge(int argc, char** argv) {
    std::string json, runs, output;
    std::vector<std::pair<int, std::string>> given;
    for (int i = 1; i < argc; ++i) {
        const std::string a = argv[i];
        auto next = [&]() -> const char* {
            if (i + 1 >= argc) {
                std::fprintf(stderr, "%s needs a value\n", a.c_str());
                std::exit(2);
            }
            return argv[++i];
        };
        if (a == "--help" || a == "-h") { usage(); return 0; }
        else if (a == "--runs") runs = next();
        else if (a == "--part") {
            const char* k = next();
            const int part = std::atoi(k);
            if (part < 0 || (part == 0 && std::strcmp(k, "0") != 0)) return bad_value(k, "--part");
            given.emplace_back(part, next());
        } else if (a == "-o" || a == "--output") output = next();
        else if (!a.empty() && a[0] == '-') {
            std::fprintf(stderr, "unknown option '%s'\n\n", a.c_str());
            usage();
            return 2;
        } else if (json.empty()) json = a;
        else { usage(); return 2; }
    }
    if (json.empty()) {
        std::fprintf(stderr, "%s\n", format(P::err_usage, {app::program_name()}).c_str());
        return 2;
    }
    std::printf("%s\n", format(P::log_reading, {json}).c_str());
    std::string dataset;
    const spirula::ScenePartition part = spirula::read_partition(json, &dataset);
    if (runs.empty()) runs = (fs::path(dataset.empty() ? fs::path(json).parent_path().string() : dataset) / "outputs").string();
    std::vector<std::string> paths = spirula::find_partition_runs(runs, json, part.num_parts);
    for (const auto& [k, path] : given) {
        if (k >= part.num_parts) return bad_value(std::to_string(k).c_str(), "--part");
        paths[(size_t)k] = path;
    }
    int found = 0;
    for (int k = 0; k < part.num_parts; k++) {
        if (paths[(size_t)k].empty())
            std::printf("%s\n", format(P::merge_part_missing, {k}).c_str());
        else
            found++;
    }
    if (found == 0) {
        std::fprintf(stderr, "%s\n", P::err_nothing_to_merge.get());
        return 1;
    }
    spirula::MergeStats stats;
    const spirula::SplatCloud merged = spirula::merge_partition_splats(part, paths, stats);
    for (int k = 0; k < part.num_parts; k++) {
        const spirula::MergePartStats& s = stats.parts[(size_t)k];
        if (s.ply.empty()) continue;
        std::printf("%s\n", format(P::merge_part, {k, (long long)s.read, (long long)s.kept, s.ply}).c_str());
    }
    if (output.empty()) output = (fs::path(json).parent_path() / "merged.ply").string();
    spirula::write_splat_ply(merged, output);
    std::printf("%s\n", format(P::merge_written, {output, (long long)merged.num}).c_str());
    return 0;
}

}  // namespace

int spirula_partition_main(int argc, char** argv) {
    app::set_program_name(argc > 0 ? argv[0] : nullptr, "spirula partition");
    if (argc < 2) {
        usage();
        return 2;
    }
    const std::string verb = argv[1];
    if (verb == "--help" || verb == "-h") { usage(); return 0; }
    // argv[1] becomes the verb's argv[0], so the parsers above start at 1.
    if (verb == "split") return run_split(argc - 1, argv + 1);
    if (verb == "merge") return run_merge(argc - 1, argv + 1);
    // A bare dataset path means split.
    std::error_code ec;
    if (fs::is_directory(verb, ec)) return run_split(argc, argv);
    std::fprintf(stderr, "%s\n", format(P::err_usage, {app::program_name()}).c_str());
    return 2;
}
