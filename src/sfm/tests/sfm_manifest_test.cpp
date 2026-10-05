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
#include "sfm/Pipeline.h"
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

    // --no-masks against a manifest that names a mask_dir. The flag has to
    // CLAIM the field, not merely clear it: the file fills in afterwards, and
    // a run told to keep features out of the masks got them back this way.
    {
        write_file(dir + "/masked.yaml",
                   "image_dir: images\nmask_dir: masks\n");
        AutoRequest req;
        const std::string err = parse_auto_args(
            {dir + "/images", "-o", dir + "/out", "--manifest",
             dir + "/masked.yaml", "--no-masks"},
            req);
        check(err.empty(), "--no-masks with a manifest parses");
        check(req.cfg.mask_dir.empty(), "--no-masks beats a manifest's mask_dir");
        check(req.in.mask_dir_explicit,
              "and no sibling masks/ is looked for either");

        AutoRequest req2;
        check(parse_auto_args({dir + "/images", "-o", dir + "/out", "--manifest",
                               dir + "/masked.yaml"},
                              req2)
                  .empty(),
              "the same manifest without the flag parses");
        check(!req2.cfg.mask_dir.empty(),
              "and then the manifest's mask_dir is what is used");
    }

    // A rig kind fills in the extrinsics it implies and survives a round trip;
    // an explicit member refine does too.
    {
        write_file(dir + "/kind.yaml",
                   "rigs:\n"
                   "  - kind: dual-fisheye\n"
                   "    members: [cam0, cam1]\n"
                   "  - members:\n"
                   "      - prefix: v0\n"
                   "        rotation: [1, 0, 0, 0]\n"
                   "        translation: [0, 0, 0]\n"
                   "      - prefix: v1\n"
                   "        rotation: [0, 0, 1, 0]\n"
                   "        translation: [0, 0, 0]\n"
                   "        fixed: false\n"
                   "        refine: translation\n");
        Manifest k = manifest_read(dir + "/kind.yaml");
        check(k.rigs.size() == 2 && k.rigs[0].kind == "dual-fisheye" &&
                  k.rigs[0].members[1].has_ext && k.rigs[0].members[1].dof == kRigDofAll &&
                  !k.rigs[0].members[1].ext_fixed &&
                  std::fabs(k.rigs[0].members[1].ext.R[0] + 1.0) < 1e-12 &&
                  std::fabs(k.rigs[0].members[1].ext.R[8] + 1.0) < 1e-12,
              "dual-fisheye: back to back, refined freely");
        check(k.rigs[1].members[1].dof == kRigDofTranslation, "refine: translation");
        write_file(dir + "/kind2.yaml", manifest_write(k));
        check(manifest_write(manifest_read(dir + "/kind2.yaml")) == manifest_write(k),
              "kind and refine read back");
        RigDef d;
        check(parseRigArg("dual-fisheye=a/cam0,a/cam1", d).empty() && d.kind == "dual-fisheye" &&
                  d.members.size() == 2 && d.members[1].prefix == "a/cam1" &&
                  d.members[1].dof == kRigDofAll,
              "--rig dual-fisheye=...");
        write_file(dir + "/badkind.yaml", "rigs:\n  - kind: trifocal\n    members: [a, b]\n");
        bool bad_kind = false;
        try {
            manifest_read(dir + "/badkind.yaml");
        } catch (const std::exception& e) {
            bad_kind = std::string(e.what()).find("trifocal") != std::string::npos;
        }
        check(bad_kind, "an unknown rig kind names itself");
    }

    // An unknown lens is caught where it is written, not 40 minutes in.
    write_file(dir + "/bad.yaml", "cameras:\n  - prefix: cam0\n    model: banana\n");
    threw = false;
    try {
        manifest_read(dir + "/bad.yaml");
    } catch (const std::exception& e) {
        threw = std::string(e.what()).find("banana") != std::string::npos;
    }
    check(threw, "an unknown camera model names itself in the error");

    {
        const std::string calibrated = dir + "/calibrated.yaml";
        write_file(calibrated,
                   "camera_mode: folder\ncameras:\n"
                   "  - prefix: cam0\n    model: opencv-fisheye\n"
                   "    params: [900, 910, 1200, 1190, 0.1, -0.02, 0.003, -0.0004]\n"
                   "  - prefix: cam1\n    model: opencv-fisheye\n"
                   "    params: [920, 930, 1240, 1210, 0.2, -0.03, 0.004, -0.0005]\n");
        Manifest cm = manifest_read(calibrated);
        for (bool json : {false, true}) {
            write_file(dir + "/roundtrip.yaml", manifest_write(cm, json));
            check(manifest_read(dir + "/roundtrip.yaml").cameras[0].params ==
                      cm.cameras[0].params,
                  "full calibration round trips through YAML and JSON");
        }
        SfmConfig cc;
        std::string ci;
        check(manifest_apply(cm, cc, {}, ci).empty() && cc.finalize(CMD_AUTO).empty(),
              "full calibration reaches the camera setup");
        std::vector<ImageEntry> images = {{"cam0/a.jpg", 0}, {"cam1/a.jpg", 0}};
        std::vector<FeatureSet> feats(2);
        for (FeatureSet& f : feats) {
            f.width = f.height = 2464;
            f.extract_width = f.extract_height = 1232;
            f.exif_focal = 2000;
        }
        CameraSetup setup = buildCameras(images, feats, cc.camera);
        auto matches = [&](const CameraSetup& s, size_t i, const std::vector<double>& expected) {
            double p[12]{};
            const Camera& cam = s.cameras.at(s.ids[i]);
            packColmap(cam, p);
            return std::vector<double>(p, p + expected.size()) == expected &&
                   cam.width == 2464 && cam.height == 2464;
        };
        check(setup.count() == 2 && matches(setup, 0, cm.cameras[0].params) &&
                  matches(setup, 1, cm.cameras[1].params),
              "anisotropic focal and off-centre principal point stay in source pixels");
        check(setup.focal_known.count(setup.ids[0]) && setup.focal_given.count(setup.ids[1]),
              "complete calibrations suppress focal search");
        MatchesDatabase db;
        db.images = images;
        storeCameraSetup(db, setup);
        writeMatches(dir + "/calibrated.bin", db);
        MatchesDatabase reread = readMatches(dir + "/calibrated.bin");
        CameraSetup restored;
        check(loadCameraSetup(reread, restored) &&
                  matches(restored, 0, cm.cameras[0].params) &&
                  matches(restored, 1, cm.cameras[1].params),
              "calibration survives the match-to-map file boundary");
        const auto old_signature = stageSignature(cc, CMD_MATCH);
        cc.camera.overrides[0].params[2] += 1e-9;
        check(stageSignature(cc, CMD_MATCH) != old_signature,
              "changing only the principal point invalidates cached matches");

        SfmConfig prefixed;
        parseCameraOverride("cam0=800", OverrideKind::Focal, prefixed.camera.overrides);
        check(manifest_apply(cm, prefixed, {}, ci).empty() &&
                  prefixed.finalize(CMD_AUTO).empty(),
              "prefixed CLI override still applies");
        const auto pc = buildCameras(images, feats, prefixed.camera);
        check(pc.cameras.at(pc.ids[0]).fx == 800 && matches(pc, 1, cm.cameras[1].params),
              "a CLI camera entry wins over the same manifest prefix");

        cm.cameras.resize(1);
        cm.cameras[0].prefix.clear();
        SfmConfig global;
        global.focal = 850;
        check(manifest_apply(cm, global, {"focal"}, ci).empty() &&
                  global.finalize(CMD_AUTO).empty(),
              "dataset-wide calibration accepts the CLI focal override");
        const auto gc = buildCameras(images, feats, global.camera);
        const Camera& gcam = gc.cameras.at(gc.ids[0]);
        check(gcam.fx == 850 && gcam.fy == 850 && gcam.cx == 1200 && gcam.cy == 1190,
              "a dataset-wide CLI focal wins while preserving the principal point");

        for (const std::string& body : {
                 "params: [1, 2, 3, 4]",
                 "model: opencv-fisheye\n    params: [1, 2, 3, 4]",
                 "model: opencv-fisheye\n    params: [1, 0, 3, 4, 0, 0, 0, 0]",
                 "model: opencv-fisheye\n    params: [-1, 2, 3, 4, 0, 0, 0, 0]",
                 "model: opencv-fisheye\n    params: [1, 2, NaN, 4, 0, 0, 0, 0]",
                 "model: opencv-fisheye\n    params: [1, 2, 3, 4, 0, 0, 0, 0]\n    focal: 1",
                 "model: opencv-fisheye\n    params: [1, 2, 3, 4, 0, 0, 0, 0]\n    distortion: []"}) {
            write_file(dir + "/bad.yaml", "cameras:\n  - prefix: cam0\n    " + body + "\n");
            bool rejected = false;
            try { manifest_read(dir + "/bad.yaml"); }
            catch (const std::exception&) { rejected = true; }
            check(rejected, "invalid or ambiguous full calibration is rejected");
        }
    }

    std::filesystem::remove_all(dir, ec);
    std::printf("manifest: 2 camera groups, YAML and JSON agree\n");
    std::printf("%s\n", fails == 0 ? "PASS" : "FAIL");
    return fails == 0 ? 0 : 1;
}

int main() { return sfmTestMain(0, nullptr, cmdManifestTest); }
