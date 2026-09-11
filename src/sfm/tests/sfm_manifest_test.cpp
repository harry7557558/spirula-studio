// The manifest and the YAML subset under it.
//
// Two things have to hold for a file people edit by hand: what YAML says is
// what the config gets, and the same capture written as JSON says the same
// thing -- so a Python script and a text editor are interchangeable.
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <set>
#include <string>

#include "data/Yaml.h"
#include "sfm/core/Manifest.h"
#include "sfm/core/Rig.h"
#include "sfm/tests/TestMain.h"

using namespace sfm;

static int fails = 0;

static void check(bool ok, const std::string& what) {
    if (!ok) {
        std::fprintf(stderr, "FAIL: %s\n", what.c_str());
        fails++;
    }
}

static void write_file(const std::string& path, const std::string& text) {
    std::ofstream f(path, std::ios::binary | std::ios::trunc);
    f << text;
}

static int cmdManifestTest(int, char**) {
    const std::string dir = "sfm_manifest_test.tmp";
    std::error_code ec;
    std::filesystem::remove_all(dir, ec);
    std::filesystem::create_directories(dir, ec);

    // ---- the YAML subset ----
    const JsonValue doc = yaml_parse(
        "# comment\n"
        "a: 1\n"
        "b: text with spaces\n"
        "c: \"quoted: colon\"\n"
        "d: 'it''s'\n"
        "e: [1, 2, {k: v}]\n"
        "f:\n"
        "  g: true\n"
        "  h: ~\n"
        "list:\n"
        "  - x: 1\n"
        "    y: 2\n"
        "  - x: 3\n"
        "after: done\n");
    check(doc.find("a")->as_int() == 1, "number");
    check(doc.find("b")->str == "text with spaces", "plain scalar keeps spaces");
    check(doc.find("c")->str == "quoted: colon", "a colon survives quoting");
    check(doc.find("d")->str == "it's", "'' is an escaped quote");
    check(doc.find("e")->arr.size() == 3 && doc.find("e")->arr[2].find("k")->str == "v",
          "flow sequence with a nested flow mapping");
    check(doc.find("f")->find("g")->b, "nested mapping");
    check(doc.find("f")->find("h")->is_null(), "~ is null");
    check(doc.find("list")->arr.size() == 2, "sequence of mappings");
    check(doc.find("list")->arr[0].find("y")->as_int() == 2,
          "a mapping's later keys line up under the first");
    check(doc.find("after")->str == "done", "the mapping resumes after a sequence");

    // A sequence at its key's own indent is what yaml_write emits, so the
    // reader has to take it back.
    const JsonValue flat = yaml_parse("k:\n- 1\n- 2\nnext: 3\n");
    check(flat.find("k")->arr.size() == 2, "sequence at the key's own indent");
    check(flat.find("next")->as_int() == 3, "and the next key after it");

    // Writers are fixed points, and JSON reads back as the same value.
    const std::string ytext = yaml_write(doc);
    const std::string jtext = json_write(doc);
    check(yaml_write(yaml_parse(ytext)) == ytext, "yaml_write round trips");
    check(yaml_write(yaml_parse(jtext)) == ytext, "json_write round trips to the same");

    bool threw = false;
    try {
        yaml_parse("a: 1\n\tb: 2\n");
    } catch (const std::exception&) {
        threw = true;
    }
    check(threw, "a tab in the indentation is an error");

    // ---- the manifest ----
    const std::string yml = dir + "/m.yaml";
    write_file(yml,
               "image_dir: pics\n"
               "mask_dir: cutouts\n"
               "camera_mode: single\n"
               "cameras:\n"
               "  - prefix: \"\"\n"
               "    model: opencv\n"
               "    focal: 800\n"
               "  - prefix: cam0\n"
               "    model: opencv-fisheye\n"
               "    focal: 350.5\n"
               "    distortion: [0.1, -0.02]\n"
               "captures:\n"
               "  - prefix: cam0\n"
               "    telemetry: clip.insv\n"
               "    fps: 24\n"
               "rigs:\n"
               "  - name: dual\n"
               "    captures: [clip1, clip2]\n"
               "    members:\n"
               "      - prefix: cam0\n"
               "        rotation: [1, 0, 0, 0]\n"
               "        translation: [0, 0, 0]\n"
               "      - prefix: cam1\n"
               "        rotation: [0, 0, 1, 0]\n"
               "        translation: [0.03, 0, 0]\n"
               "        fixed: false\n"
               "  - members: [left, right]\n");
    Manifest m = manifest_read(yml);
    check(m.rigs.size() == 2 && m.rigs[0].name == "dual" && m.rigs[0].captures.size() == 2,
          "a rig with its captures");
    check(m.rigs[0].members.size() == 2 && m.rigs[0].members[0].has_ext &&
              m.rigs[0].members[1].has_ext && !m.rigs[0].members[1].ext_fixed &&
              std::fabs(m.rigs[0].members[1].ext.t.x - 0.03) < 1e-12 &&
              std::fabs(m.rigs[0].members[1].ext.R[0] + 1.0) < 1e-12,
          "a member's known extrinsic");
    check(m.rigs[1].members.size() == 2 && m.rigs[1].members[1].prefix == "right",
          "a rig of bare prefixes");
    check(m.captures.size() == 1 && m.captures[0].prefix == "cam0" && m.captures[0].fps == 24,
          "a capture's telemetry and frame rate");
    check(m.image_dir == "pics", "image_dir is kept as the file spells it");
    check(m.base_dir == dir, "and the manifest's own directory with it");
    check(m.camera_mode == "single", "camera_mode");
    check(m.cameras.size() == 2, "two camera groups");
    check(m.cameras[1].prefix == "cam0" && m.cameras[1].distortion.size() == 2,
          "the group's lens and distortion");

    // The same capture as JSON is the same manifest.
    const std::string jsn = dir + "/m.json";
    write_file(jsn, manifest_write(m, /*json=*/true));
    Manifest mj = manifest_read(jsn);
    check(manifest_write(mj) == manifest_write(m), "YAML and JSON describe the same capture");
    write_file(dir + "/m2.yaml", manifest_write(m));
    check(manifest_write(manifest_read(dir + "/m2.yaml")) == manifest_write(m),
          "a written manifest reads back");

    // ---- precedence ----
    SfmConfig cfg;
    std::string image_dir;
    check(manifest_apply(m, cfg, {}, image_dir).empty(), "apply succeeds");
    check(image_dir == (std::filesystem::path(dir) / "pics").string(),
          "a relative image_dir resolves against the manifest");
    check(cfg.camera_model == "opencv", "the dataset-wide entry sets --camera-model");
    check(cfg.focal == 800, "and the dataset-wide focal");
    check(cfg.camera.overrides.size() == 1 && cfg.camera.overrides[0].prefix == "cam0",
          "a prefixed entry becomes an override");
    check(cfg.camera.overrides[0].has_focal && cfg.camera.overrides[0].focal == 350.5,
          "with its focal");
    check(cfg.telemetry_inputs.size() == 1 &&
              cfg.telemetry_inputs[0].path == (std::filesystem::path(dir) / "clip.insv").string(),
          "a capture's telemetry resolves against the manifest");
    check(cfg.rigs.size() == 2 && cfg.rigs[0].captures[1] == "clip2", "the rigs reach the config");
    {
        // The definitions against a tree: captures key frames apart, the bare
        // rig pairs by name, and a conflict is refused.
        std::vector<std::string> names = {"clip1/cam0/00001.jpg", "clip1/cam1/00001.jpg",
                                          "clip2/cam0/00001.jpg", "clip2/cam1/00001.jpg",
                                          "clip2/cam1/00002.jpg", "left/a.jpg", "right/a.jpg"};
        RigTable t = buildRigTable(names, cfg.rigs);
        check(t.rigs.size() == 2 && t.rigs[0].frames.size() == 3 && t.rigs[1].frames.size() == 1,
              "frames keyed by capture and name");
        check(t.slot(0).valid() && t.slot(1).valid() && t.slot(0).frame == t.slot(1).frame &&
                  t.slot(2).frame != t.slot(0).frame,
              "one stem in two captures is two frames");
        check(t.slot(4).valid() && t.rigs[0].frames[t.slot(4).frame][0] == kNoImage,
              "a frame missing a lens keeps the slot empty");
        std::vector<RigDef> clash = cfg.rigs;
        RigDef again;
        again.members = {RigMemberDef{"left"}, RigMemberDef{"clip1/cam0"}};
        clash.push_back(again);
        bool refused = false;
        try {
            buildRigTable(names, clash);
        } catch (const std::exception&) {
            refused = true;
        }
        check(refused, "an image claimed by two rigs is refused");
    }

    SfmConfig cfg2;
    cfg2.camera_model = "radial";
    cfg2.mask_dir = "given";
    std::string image_dir2 = "given/images";
    const std::set<std::string> seen = {"camera-model", "masks", "camera-mode"};
    check(manifest_apply(m, cfg2, seen, image_dir2).empty(), "apply with flags set");
    check(cfg2.camera_model == "radial", "a --camera-model flag beats the file");
    check(cfg2.mask_dir == "given", "a --masks flag beats the file");
    check(image_dir2 == "given/images", "a positional image directory beats the file");
    check(cfg2.camera_mode != "single", "a --camera-mode flag beats the file");

    // An unknown lens is caught where it is written, not 40 minutes in.
    write_file(dir + "/bad.yaml", "cameras:\n  - prefix: cam0\n    model: banana\n");
    threw = false;
    try {
        manifest_read(dir + "/bad.yaml");
    } catch (const std::exception& e) {
        threw = std::string(e.what()).find("banana") != std::string::npos;
    }
    check(threw, "an unknown camera model names itself in the error");

    std::filesystem::remove_all(dir, ec);
    std::printf("manifest: 2 camera groups, YAML and JSON agree\n");
    std::printf("%s\n", fails == 0 ? "PASS" : "FAIL");
    return fails == 0 ? 0 : 1;
}

int main() { return sfmTestMain(0, nullptr, cmdManifestTest); }
