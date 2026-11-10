#pragma once

// The right-click menu on a saved set of drawn areas, in any list of them, and
// the dialogs its Rename and Delete open.

#include "app/gui/StencilPreset.h"

#include <optional>
#include <string>

namespace gui {

class StencilSetMenu {
public:
    // Right after the set's item in the list. True when Edit is picked, which
    // is the caller's to carry out; it closes the list too.
    bool item(const StencilPreset& p, bool can_edit = true);

    // What a dialog did to a set: `was` is its file before, `now` the set
    // after, empty once deleted.
    struct Change {
        std::string was;
        std::optional<StencilPreset> now;
    };
    // Every frame, outside the list and always from the same place: the modals
    // are keyed on the id stack here. The change, on the frame it is made;
    // `message` is written only when a dialog ends with something to report.
    std::optional<Change> draw_dialogs(std::string& message, bool& failed);

private:
    std::optional<Change> draw_delete(std::string& message, bool& failed);
    std::optional<Change> draw_rename(std::string& message, bool& failed);

    StencilPreset _target;
    std::string _name, _name_error;
    bool _ask_delete = false;
    bool _ask_rename = false;
};

}  // namespace gui
