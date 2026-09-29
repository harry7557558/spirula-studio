// dataset_color -- what `--image-color-log auto` makes of a dataset's record of
// its inputs' picture profiles (data/DatasetColor.h), that the record survives
// a failed write safely, and that a run's config.json keeps `auto`.

#include "app/TrainerCore.h"
#include "checkpoint/Resume.h"
#include "config/TrainConfigJson.h"
#include "data/Json.h"
#include "external/stb_image_write.h"

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include <random>

namespace fs = std::filesystem;
using namespace spirula;

namespace {

int g_failures = 0;

void check(bool ok, const std::string& what) {
    std::printf("%-78s %s\n", what.c_str(), ok ? "ok" : "FAILED");
    if (!ok) g_failures++;
}

DatasetColor record(std::initializer_list<ClipColor> modes, const char* proto = "dvtm_oq101.proto") {
    DatasetColor d;
    int i = 0;
    for (ClipColor m : modes)
        d.clips.push_back({m,
                           m == ClipColor::DlogM ? 19 : m == ClipColor::Normal ? 0
                               : m == ClipColor::OtherLog ? 22 : -1,
                           m == ClipColor::NotRecorded ? std::string() : std::string(proto),
                           "clip " + std::to_string(i++) + ".OSV"});
    return d;
}

colorspace::InputCurve curve_after(TrainConfig c, const DatasetColor& d, std::string* line = nullptr) {
    const std::string l = adopt_dataset_color(c, d);
    if (line) *line = l;
    return resolve_color(c).image_curve;
}

bool refused(TrainConfig c, const DatasetColor& d, std::string* why = nullptr) {
    try {
        adopt_dataset_color(c, d);
    } catch (const std::exception& e) {
        if (why) *why = e.what();
        return true;
    }
    return false;
}

bool has(const std::string& s, const std::string& part) { return s.find(part) != std::string::npos; }

std::string slurp(const fs::path& p) {
    std::ifstream f(p, std::ios::binary);
    std::stringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

void write_colmap(const fs::path& root) {
    constexpr int kW = 16, kH = 12;
    fs::create_directories(root / "images");
    fs::create_directories(root / "sparse" / "0");
    std::vector<uint8_t> px((size_t)kW * kH * 3, 102);
    std::ofstream im(root / "sparse" / "0" / "images.txt");
    for (int i = 0; i < 3; i++) {
        const std::string name = "v" + std::to_string(i) + ".png";
        stbi_write_png((root / "images" / name).string().c_str(), kW, kH, 3, px.data(), kW * 3);
        im << i + 1 << " 1 0 0 0 " << 0.2 * i << " 0 0 1 " << name << "\n\n";
    }
    std::ofstream(root / "sparse" / "0" / "cameras.txt") << "1 PINHOLE 16 12 15 15 8 6\n";
    std::ofstream pts(root / "sparse" / "0" / "points3D.txt");
    for (int k = 0; k < 16; k++)
        pts << k + 1 << " " << 0.1 * (k % 4) << " " << 0.1 * (k / 4) << " 4 102 102 102 0.5\n";
}

void prepare(TrainerSession& s, const fs::path& data) {
    s.cfg.data = data.string();
    s.log_fn = [](const std::string&) {};
}

void test_adopt() {
    using C = ClipColor;
    using IC = colorspace::InputCurve;
    const TrainConfig unset;
    check(unset.image_color_log == "auto", "config: unset is `auto`, distinct from an explicit none");

    std::string line;
    check(curve_after(unset, record({C::DlogM, C::DlogM}), &line) == IC::DlogMOsmo360,
          "default: a D-Log M dataset decodes as dlogm-osmo360");
    check(has(line, "dlogm-osmo360"), "default: the start line names the curve");
    {
        TrainConfig c = unset;
        adopt_dataset_color(c, record({C::DlogM}));
        check(resolve_color(c).point_curve == IC::DlogMOsmo360,
              "default: the SfM seed colours follow the images");
        check(c.image_color_log == "auto" && c.image_color_log_resolved == "dlogm-osmo360",
              "default: the flag stays `auto`, the answer goes to the resolved field");
    }

    TrainConfig cli_none = unset, gui_none = unset;
    cli_none.image_color_log = "";      // `--image-color-log none` as the CLI parses it
    gui_none.image_color_log = "none";  // as the GUI's combo writes it
    check(curve_after(cli_none, record({C::DlogM}), &line) == IC::None &&
              curve_after(gui_none, record({C::DlogM})) == IC::None,
          "explicit none: wins over a D-Log M dataset");
    check(has(line, "none"), "explicit none: the start line says it was set");

    TrainConfig explicit_log = unset;
    explicit_log.image_color_log = "dlogm-osmo360";
    check(curve_after(explicit_log, record({C::Normal})) == IC::DlogMOsmo360,
          "explicit dlogm-osmo360: wins over a Normal dataset");

    // Each camera has its own curve: the record's layout says which shot a clip.
    check(curve_after(unset, record({C::DlogM, C::DlogM}, "dvtm_AVATA360.proto"), &line) ==
              IC::DlogMAvata360,
          "avata: an Avata 360 D-Log M dataset decodes as dlogm-avata360");
    check(has(line, "dlogm-avata360") && !has(line, "dlogm-osmo360"),
          "avata: the start line names the Avata curve");
    std::string why;
    {
        DatasetColor both = record({C::DlogM});
        both.clips.push_back({C::DlogM, 19, "dvtm_AVATA360.proto", "clip 1.OSV"});
        check(refused(unset, both, &why), "cameras: Osmo and Avata D-Log M in one dataset is refused");
        check(has(why, "--image-color-log") && has(why, "1"),
              "cameras: the refusal names the flag and counts each camera's inputs");
        TrainConfig av = unset;
        av.image_color_log = "dlogm-avata360";
        check(!refused(av, both), "cameras: an explicit curve is not refused");
    }
    check(refused(unset, record({C::DlogM, C::Normal}), &why), "mixed: D-Log M beside Normal is refused");
    check(has(why, "--image-color-log") && !has(why, "clips"),
          "mixed: the refusal names the flag and counts inputs, not clips");
    check(refused(unset, record({C::DlogM, C::Unknown})), "mixed: D-Log M beside an unknown input is refused");
    check(refused(unset, record({C::DlogM, C::NotRecorded})),
          "mixed: D-Log M beside an input with no profile metadata is refused");
    check(!refused(cli_none, record({C::DlogM, C::Normal})) &&
              !refused(explicit_log, record({C::DlogM, C::Normal})),
          "mixed: an explicit value is not refused");

    // Review M3: another DJI log profile is log, and not one this build decodes.
    check(refused(unset, record({C::OtherLog}), &why), "other log: a D-Log2 (22) dataset is refused under auto");
    check(has(why, "22") && has(why, "clip 0.OSV"), "other log: the refusal names the input and its code");
    check(refused(unset, record({C::OtherLog, C::Normal})),
          "other log: beside Normal it is still refused, not read as not-log");
    check(!refused(cli_none, record({C::OtherLog})), "other log: an explicit none trains it undecoded");
    check(dataset_color_label(summarize_dataset_color(record({C::OtherLog}))) !=
              dataset_color_label(summarize_dataset_color(record({C::Normal}))),
          "other log: the GUI label is not the not-log one");

    {
        TrainConfig c = unset;
        adopt_dataset_color(c, record({C::Normal}));
        TrainConfig plain = unset;
        plain.image_color_log = "";
        const ColorResolution a = resolve_color(c), b = resolve_color(plain);
        check(a.image_curve == IC::None && a.point_curve == IC::None && a.image_linear == b.image_linear &&
                  a.image_gamut == b.image_gamut && c.image_color_log_resolved == "none",
              "normal: a Normal dataset is read exactly as with no flag");
    }

    check(curve_after(unset, record({C::Unknown}), &line) == IC::None, "unknown: no automatic default");
    check(has(line, "--image-color-log") && has(line, "clip 0.OSV"),
          "unknown: the start line names the input and asks for the flag");
    // Review M4: an unknown input outranks a Normal one.
    check(curve_after(unset, record({C::Normal, C::Unknown}), &line) == IC::None &&
              has(line, "clip 1.OSV") && has(line, "--image-color-log"),
          "unknown beside Normal: asks for the flag, not 'not log'");
    // The Avata 360's mode is read now, so an unknown one gets the same line as any other.
    curve_after(unset, record({C::Unknown}, "dvtm_AVATA360.proto"), &line);
    check(!has(line, "not readable yet") && has(line, "clip 0.OSV") && has(line, "--image-color-log"),
          "unknown (Avata 360): the start line names the input and asks for the flag");

    {
        TrainConfig c = unset;
        c.image_color_log_resolved = "dlogm-osmo360";  // a stale answer from an earlier dataset
        check(curve_after(c, DatasetColor{}, &line) == IC::None && line.empty(),
              "no record: nothing changes and nothing is said");
        adopt_dataset_color(c, DatasetColor{});
        check(c.image_color_log_resolved == "none", "no record: the resolved field says none (review M5)");
    }
    {
        TrainConfig c = unset;
        c.resume = "some/run";
        c.image_color_log_resolved = "dlogm-osmo360";
        check(curve_after(c, record({C::Normal})) == IC::DlogMOsmo360,
              "resume: a resumed run keeps its resolved curve, never re-detects");
    }
}

void test_record(const fs::path& tmp) {
    using C = ClipColor;
    const fs::path d = tmp / "rec";
    fs::create_directories(d);
    const fs::path rec = d / kDatasetColorFile;

    ColorRecordResult r = write_dataset_color(d.string(), record({C::DlogM, C::Unknown}));
    DatasetColor back = read_dataset_color(d.string());
    check(r.status == ColorRecordWrite::Written && back.clips.size() == 2 &&
              back.clips[0].mode == C::DlogM && back.clips[0].code == 19 &&
              back.clips[0].proto == "dvtm_oq101.proto" && back.clips[1].mode == C::Unknown &&
              back.clips[1].source == "clip 1.OSV",
          "record: written and read back, spaces in the name kept");
    check(!fs::exists(d / (std::string(kDatasetColorFile) + ".tmp")), "record: no temp file left behind");

    r = write_dataset_color(d.string(), record({C::NotRecorded}));
    check(r.status == ColorRecordWrite::Removed && !fs::exists(rec),
          "record: nothing to say removes a stale one");

    // I1: a read-only record is replaced, not kept.
    write_dataset_color(d.string(), record({C::DlogM}));
    fs::permissions(rec, fs::perms::owner_read | fs::perms::group_read | fs::perms::others_read);
    r = write_dataset_color(d.string(), record({C::Normal}));
    back = read_dataset_color(d.string());
    check(r.status == ColorRecordWrite::Written && back.clips.size() == 1 && back.clips[0].mode == C::Normal,
          "record: a read-only D-Log M record is replaced by the new one");

    // I1: the temp file cannot be written, so the old record must go.
    write_dataset_color(d.string(), record({C::DlogM}));
    const fs::path tmpdir = d / (std::string(kDatasetColorFile) + ".tmp");
    fs::create_directories(tmpdir / "blocker");
    r = write_dataset_color(d.string(), record({C::Normal}));
    check(r.status == ColorRecordWrite::Failed && !r.error.empty() && !fs::exists(rec),
          "record: a failed write removes the old record and says why");
    fs::remove_all(tmpdir);

    // I1: a record that cannot be removed is reported as stale.
    std::error_code ec;
    fs::remove(rec, ec);
    fs::create_directories(rec / "blocker");
    r = write_dataset_color(d.string(), record({C::Normal}));
    check(r.status == ColorRecordWrite::FailedStale && !r.error.empty(),
          "record: an unremovable record is reported, not skipped (write)");
    r = write_dataset_color(d.string(), record({C::NotRecorded}));
    check(r.status == ColorRecordWrite::FailedStale, "record: an unremovable record is reported (remove)");
    back = read_dataset_color(d.string());
    check(back.clips.size() == 1 && back.clips[0].mode == C::Unknown,
          "record: a record that is there but unreadable reads as unknown");
    fs::remove_all(rec);

    // I1: a torn record keeps every line it holds.
    std::ofstream(rec, std::ios::binary) << "dlogm 19 dvtm_oq101.proto a.OSV\nnormal\n";
    back = read_dataset_color(d.string());
    check(back.clips.size() == 2 && back.clips[1].mode == C::Unknown &&
              summarize_dataset_color(back).verdict == DatasetColorVerdict::Mixed,
          "record: a torn line reads as unknown, so the verdict is mixed");
}

void test_session(const fs::path& tmp) {
    using C = ClipColor;
    const fs::path data = tmp / "data";
    write_colmap(data);
    write_dataset_color(data.string(), record({C::DlogM}));
    TrainerSession s;
    prepare(s, data);
    s.load_dataset();
    check(resolve_color(s.cfg).image_curve == colorspace::InputCurve::DlogMOsmo360,
          "session: load_dataset adopts the dataset's record");

    // I2: config.json keeps what was asked for; the answer has its own key.
    const fs::path run = tmp / "run";
    fs::create_directories(run);
    save_config_json(s.cfg, run, "3dgs");
    const JsonValue j = json_parse_file((run / "config.json").string());
    const JsonValue* asked = j.find("image_color_log");
    const JsonValue* got = j.find("image_color_log_resolved");
    check(asked && asked->as_string() == "auto",
          "config.json: image_color_log stays auto");
    check(got && got->as_string() == "dlogm-osmo360",
          "config.json: image_color_log_resolved holds the detected curve");
    const TrainConfig back = ckpt::config_from_json(run / "config.json");
    check(back.image_color_log_resolved == "dlogm-osmo360" &&
              resolve_color(back).image_curve == colorspace::InputCurve::DlogMOsmo360,
          "config.json: the resume reader takes the resolved curve");
    TrainConfig flags;
    train_config_from_json(j, flags);
    check(flags.image_color_log == "auto" && flags.image_color_log_resolved.empty(),
          "config.json: the flag reader (presets, batches) ignores the resolved key");

    // M5: a dataset without a record still settles, and says so in config.json.
    const fs::path bare = tmp / "bare";
    write_colmap(bare);
    TrainerSession b;
    prepare(b, bare);
    b.load_dataset();
    save_config_json(b.cfg, run, "3dgs");
    const JsonValue jb = json_parse_file((run / "config.json").string());
    const JsonValue* none = jb.find("image_color_log_resolved");
    check(none && none->as_string() == "none",
          "config.json: a dataset with no record resolves to none");
}

}  // namespace

int main() {
    test_adopt();
    const fs::path tmp = fs::temp_directory_path() / ("dataset_color_test_" + std::to_string(std::random_device{}()));
    fs::create_directories(tmp);
    test_record(tmp);
    test_session(tmp);
    std::error_code ec;
    fs::permissions(tmp, fs::perms::owner_all, fs::perm_options::add, ec);
    fs::remove_all(tmp, ec);
    std::printf("%s\n", g_failures ? "FAILED" : "all ok");
    return g_failures ? 1 : 0;
}
