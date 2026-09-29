#pragma once

// The mask prompt is written in English, always. This is the UI that makes
// that reasonable for someone who does not write English.
//
// SAM 3's text encoder is trained on English. A prompt in another language
// still returns *something* -- which is the dangerous part, because it looks
// like it worked -- but it finds noticeably less of what it names, and the
// user only discovers that after a masking run over the whole capture. So the
// prompt box does not follow the interface language:
//
//   - the box and its placeholder example stay English (ui::InputTextEnglish)
//   - a line under it says so, in the interface language
//   - and this palette gives the twenty or so subjects that account for most
//     real prompts, LABELLED in the interface language, INSERTING English
//
// A user who reads only Japanese can therefore build "person; car; shadow of a
// person" without writing a word of English, and can still type into the box
// if they want something the palette does not have.
//
// The chips toggle: clicking one that is already in the prompt takes it out,
// and a chip whose term is present is drawn highlighted, so the palette also
// reads as a summary of what the prompt currently says.

#include "app/gui/MaskSettings.h"
#include "i18n/Message.h"

#include <functional>
#include <string>
#include <vector>

namespace gui {

// A chip: what it says, and what it writes.
struct MaskSubject {
    const spirula::i18n::Msg* label;   // translated
    const char* term;                  // English, goes into the prompt verbatim
};

// The palette, in two groups: the ones that name what to act on, and the ones
// that are only ever exceptions to it (a statue is not a person; a face in a
// painting is not someone walking through the shot).
const std::vector<MaskSubject>& mask_subjects();
const std::vector<MaskSubject>& mask_exception_subjects();

// Semicolon-separated term list handling. Case- and space-insensitive, so a
// term the user typed themselves still lights its chip up.
bool prompt_has_term(const std::string& prompt, const char* term);
void prompt_toggle_term(std::string& prompt, const char* term);

// Draws the whole palette: a collapsible section holding both groups, the
// first feeding `prompt` and the second `negative`. The two group headings
// are the labels of the fields they write to, which is what tells the user
// where a chip's word will land. Returns true if either string changed.
//
// `keep_subject` picks those headings, exactly as it picks the field labels.
bool draw_subject_palette(std::string& prompt, std::string& negative,
                          bool keep_subject);

// The margin a match's outline moves by, 0-50% of its size: removing grows it
// (`dilate_ratio`), keeping trims it (`shrink_ratio`). `inline_label` puts the
// label beside a `width` slider, else above it. True when it changed.
bool draw_margin_slider(float& dilate_ratio, float& shrink_ratio, bool keep, float width,
                        bool inline_label);

// One colour per object, IM_COL32-packed; red is left for "not this" clicks.
unsigned int mask_object_color(int object);

// The clicked-object list: a row per object with its click counts on `frame` /
// `camera` and elsewhere, "Another object" and the clears. Only clicks from
// `source` count and are cleared. Writes clicks, object_count, current_object.
void draw_mask_objects(MaskSettings& settings, long long frame, const std::string& camera,
                       const std::string& source, bool& edited);

class FileDownload;   // app/gui/ModelCache.h

// The row under the checkpoint combo, in the dataset screen's own order: a
// download in flight wins over "ready". Inline so mask_doc_test can pin it.
enum class PickerRow { None, GetModel, Downloading, Ready };
inline PickerRow mask_picker_row(bool has_entry, bool cached, bool downloading) {
    if (has_entry && !cached && !downloading) return PickerRow::GetModel;
    if (downloading) return PickerRow::Downloading;
    return has_entry ? PickerRow::Ready : PickerRow::None;
}

// The checkpoint picker of the dataset screen and the mask editor, over the SAME
// download; `request_download` asks consent first. A null `detector_id` (the
// editor) drops the TextDetector combo and the non-SAM entries.
void draw_mask_model_picker(std::string& model_id, std::string* detector_id,
                            FileDownload& download,
                            const std::function<void()>& request_download);

}  // namespace gui
