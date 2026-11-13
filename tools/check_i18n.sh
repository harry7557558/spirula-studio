#!/bin/bash
# Fail if the GUI renders text ImGui was handed directly, bypassing i18n.
#
# `Msg` (src/i18n/Message.h) makes an INCOMPLETE translation a compile error --
# but it cannot see ImGui::Button("Start"), which compiles fine and is simply
# never translated. This lint is the other half: inside src/app/gui/, the
# text-bearing ImGui calls belong to src/app/gui/Ui.h and nowhere else. Call
# ui::Button(msg) for UI copy, or ui::ButtonRaw(...) for a path, a number or an
# engine log line -- the `Raw` in the name is the point, it makes "this is
# deliberately not translated" visible in the diff.
#
# Layout, cursor and ID calls (Begin, SameLine, PushID, Spacing, ...) carry no
# text and are not restricted.
#
# Also reports how many messages are still untranslated stubs, which is the
# Phase 4 TODO list rather than an error.
#
# Usage:  bash tools/check_i18n.sh

cd "$(dirname "$0")/.." || exit 1

# The ImGui entry points that put author-written text on screen.
banned='Text|TextV|TextUnformatted|TextColored|TextColoredV|TextDisabled'
banned="$banned"'|TextDisabledV|TextWrapped|TextWrappedV|LabelText|BulletText'
banned="$banned"'|Button|SmallButton|InvisibleButton|ArrowButton|Checkbox'
banned="$banned"'|CheckboxFlags|RadioButton|Selectable|Combo|BeginCombo'
banned="$banned"'|MenuItem|BeginMenu|CollapsingHeader|SeparatorText|TreeNode'
banned="$banned"'|TreeNodeEx|SetTooltip|SetTooltipV|SetItemTooltip'
banned="$banned"'|SliderInt|SliderFloat|SliderInt2|SliderInt3|SliderInt4'
banned="$banned"'|SliderFloat2|SliderFloat3|SliderFloat4|DragInt|DragFloat'
banned="$banned"'|InputText|InputTextMultiline|InputTextWithHint|InputInt'
banned="$banned"'|InputInt2|InputInt3|InputInt4|InputFloat|InputFloat2'
banned="$banned"'|InputFloat3|InputFloat4|InputDouble|InputScalar'
banned="$banned"'|ColorEdit3|ColorEdit4|ColorPicker3|ColorPicker4'
banned="$banned"'|ProgressBar|PlotLines|PlotHistogram|BeginPopupModal'
banned="$banned"'|BeginTabItem|TableSetupColumn|Value|TextLink'

hits=$(git grep --untracked -nIE "ImGui::($banned)[[:space:]]*\(" \
    -- 'src/app/gui/*.cpp' 'src/app/gui/*.h' ':!src/app/gui/Ui.h')

if [ -n "$hits" ]; then
    n=$(echo "$hits" | wc -l)
    echo "Text-bearing ImGui calls outside src/app/gui/Ui.h ($n):"
    echo ""
    echo "$hits" | sed 's/^/  /'
    echo ""
    echo "Use the ui:: wrapper in src/app/gui/Ui.h. UI copy takes a Msg from"
    echo "src/i18n/catalog/; paths, numbers and engine log lines take the"
    echo "matching ui::*Raw overload."
    exit 1
fi

# The tag macros are #undef'd by EndCatalog.h; a catalog that forgets to
# include it leaks EN/JA/TR (two letters!) into whatever includes it next.
for f in src/i18n/catalog/*.h; do
    [ -e "$f" ] || continue
    grep -q 'i18n/BeginCatalog.h' "$f" || continue
    grep -q 'i18n/EndCatalog.h' "$f" || {
        echo "$f includes BeginCatalog.h but never EndCatalog.h --"
        echo "the EN/JA/TR tag macros would leak out of it."
        exit 1
    }
done

total=$(git grep --untracked -hcE '^[[:space:]]*SS_MSG(_EN)?\(' -- 'src/i18n/catalog/*.h' \
        | awk '{s+=$1} END {print s+0}')
stubs=$(git grep --untracked -hcE '^[[:space:]]*SS_MSG_EN\(' -- 'src/i18n/catalog/*.h' \
        | awk '{s+=$1} END {print s+0}')

echo "OK: no unwrapped ImGui text calls in src/app/gui/."
if [ "$stubs" -gt 0 ]; then
    echo "     $stubs of $total messages are still SS_MSG_EN (English only):"
    git grep --untracked -cE '^[[:space:]]*SS_MSG_EN\(' -- 'src/i18n/catalog/*.h' \
        | sed 's/^/       /'
else
    echo "     all $total messages are translated into every language."
fi
