#pragma once

#include "core/AtomicFile.h"

#include <vector>

namespace ckpt {

class StagedFiles {
    struct File {
        std::filesystem::path final, temporary;
        bool closed = false;
    };
    struct Paths {
        std::vector<File> files;
        ~Paths() {
            std::error_code ec;
            for (const auto& f : files)
                if (!f.temporary.empty()) std::filesystem::remove(f.temporary, ec);
        }
    } paths_;
    spirula::BufferedOutputFile stream_;

public:
    template<class Write>
    void write(std::filesystem::path final, Write&& write_bytes) {
        namespace fs = std::filesystem;
        auto temporary = fs::path(final).concat(".tmp");
        std::error_code ec;
        const auto status = fs::symlink_status(temporary, ec);
        if ((ec && ec != std::errc::no_such_file_or_directory) ||
            (fs::exists(status) && !fs::is_regular_file(status)))
            throw std::runtime_error("cannot open checkpoint staging: " + temporary.string());
        const auto& file = paths_.files.emplace_back(File{std::move(final), std::move(temporary)});
        stream_.open(file.temporary, std::ios::binary | std::ios::trunc);
        if (!stream_) throw std::runtime_error("cannot open checkpoint staging: " + file.temporary.string());
        write_bytes(stream_);
        spirula::close_checked(stream_, file.temporary);
        paths_.files.back().closed = true;
    }

    void publish() {
        for (const auto& f : paths_.files)
            if (!f.closed) throw std::logic_error("cannot publish an unchecked checkpoint");
        for (auto& f : paths_.files) {
            spirula::replace_file(f.temporary, f.final);
            f.temporary.clear();
        }
        paths_.files.clear();
    }
};

}  // namespace ckpt
