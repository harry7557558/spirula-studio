#!/bin/bash
# Structural tripwire over the GUI's licence consent. The dialog has no unit seam
# (it needs ImGui), so what fetch_consent_test proves about the shared record is
# pinned here at the GUI end: the Accept button records through the same store the
# CLI reads, and save_settings cannot drop an acceptance the CLI wrote. Text only,
# never behaviour; the exit status is the number of lines that missed.

cd "$(dirname "$0")/../.." || exit 1
FAILS=0
has() {   # min-count file(s) text
    local n
    n=$(cat $2 | command grep -cF -- "$3")
    if [ "$n" -ge "$1" ]; then echo "ok   $n >= $1: $3"; else echo "FAIL $n >= $1 in $2: $3"; FAILS=$((FAILS + 1)); fi
}
none() {  # file(s) text
    local n
    n=$(cat $1 | command grep -cF -- "$2")
    if [ "$n" -eq 0 ]; then echo "ok   0 == 0: $2"; else echo "FAIL $n == 0 in $1: $2"; FAILS=$((FAILS + 1)); fi
}
# GuiApp is split by screen: settings in GuiApp.cpp, the dialog in
# LicensePrompt.cpp; a "none" covers every file of it.
G=src/app/gui/GuiApp.cpp
L=src/app/gui/LicensePrompt.cpp
GA='src/app/gui/*.cpp'
has 1 $L 'if (!accept_license(_license_prompt))'
has 1 src/app/gui/ModelCache.cpp 'return spirula::license::record(family);'
has 1 $L '_accepted_cache = spirula::license::accepted_all();'
none "$GA" '_accepted_licenses'
none src/app/gui/GuiApp.h '_accepted_licenses'
has 1 $G 'const std::vector<std::string> accepted = spirula::license::accepted_all();'
has 1 $G 'for (const auto& l : accepted)'
# A callback that re-queues licences must not be wiped after it runs.
has 1 $L 'std::function<void()> then = std::move(_license_then);'
none "$GA" 'if (auto then = std::move(_license_then)) then();'
# save_settings writes beside and renames over.
has 1 $G 'const std::string tmp = spirula::license::scratch_path_for(settings_path());'
has 1 $G 'if (ok && closed) fs::rename(tmp, settings_path(), ec);'
none "$GA" 'std::fopen(settings_path().c_str(), "w")'
# The download queue refuses a licence-gated file that is not accepted.
has 1 src/app/gui/ModelCache.cpp 'spirula::license::missing(d.license_family)'
has 1 src/app/gui/ModelCache.cpp '_dl.start(d);'
none src/app/gui/ModelCache.cpp 'model_mirror_url'
none src/app/gui/SfmRunner.cpp 'model_mirror_url'
none src/app/gui/GeometryRunner.cpp 'model_mirror_url'
# Main consumes --accept-license for every command.
has 1 src/app/Main.cpp 'app::consume_accept_license_args(argc, argv);'
# accepted_all() is read BEFORE the "w" open truncates gui.conf.
read_at=$(command grep -nF 'spirula::license::accepted_all();' $G | head -1 | cut -d: -f1)
open_at=$(command grep -nF 'std::fopen(tmp.c_str(), "w")' $G | head -1 | cut -d: -f1)
if [ -n "$read_at" ] && [ -n "$open_at" ] && [ "$read_at" -lt "$open_at" ]; then
    echo "ok   accepted_all() ($read_at) precedes the truncating open ($open_at)"
else
    echo "FAIL accepted_all() must be read before save_settings truncates gui.conf"; FAILS=$((FAILS + 1))
fi
has 1 $L 'ImGui::BeginChild("##license_text"'
has 1 $L 'ui::TextWrappedRaw(std::string(li.full_text));'
has 1 src/app/gui/ModelCache.cpp 'out.push_back({e.fam, e.title, e.summary, t->url, t->text});'
# Accept is gated on the tick for every family, through the one policy function.
has 1 $L 'ImGui::BeginDisabled(!spirula::license::accept_enabled(_license_prompt, _license_tick));'
none "$GA" 'needs_tick'
none src/app/gui/ModelCache.h 'needs_tick'
# A download that names a licence goes through the consent door, not a bare start().
has 1 src/app/gui/DatasetAdvanced.cpp '_feat_download, sfm_feature_downloads(_sfm_job.features, _sfm_job.matcher));'
has 1 src/app/gui/GeometryOptions.cpp 'start_downloads_with_consent(_geom_download, geometry_model_downloads(_geometry.model));'
none "$GA" '_geom_download.start(geometry_model_downloads(_geometry.model));'
# The CLI's SAM entry points ask before they open a checkpoint.
has 2 src/app/cli/sam_main.cpp 'if (!app::require_model_license(o.model)) return 2;'
has 1 src/app/cli/sam_extract.cpp 'app::require_model_license(o.model)'
# A batch asks up front, in one place, before its first row.
has 2 "$GA" 'begin_batch('
none "$GA" 'case Pending::StartBatch: start_batch('
exit $FAILS
