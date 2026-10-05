#pragma once
// What `spirula train` and the GUI's training screen say about a GPU the
// backend knows training fails on (backend::DeviceIssue).

#include "backend/api/BackendRuntime.h"
#include "i18n/catalog/Train.h"

namespace app {

struct DeviceIssueText {
    const spirula::i18n::Msg* title = nullptr;  // null: nothing to say
    const spirula::i18n::Msg* body = nullptr;   // {0} is the device name
    const char* url = nullptr;
};

inline DeviceIssueText device_issue_text(backend::DeviceIssue issue) {
    namespace tmsg = spirula::i18n::msg::train;
    switch (issue) {
        case backend::DeviceIssue::AmdWindowsFloatAtomics:
            return {&tmsg::device_issue_amd_windows_title,
                    &tmsg::device_issue_amd_windows,
                    "https://github.com/harry7557558/spirula-studio/issues/23"};
        case backend::DeviceIssue::NoneKnown:
            break;
    }
    return {};
}

}  // namespace app
