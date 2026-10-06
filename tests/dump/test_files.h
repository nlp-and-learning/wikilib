#pragma once

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>
#include "wikilib/dump/bz2_stream.h"

namespace wikilib::test {

class TempDirectory {
public:
    TempDirectory() {
        auto pattern = (std::filesystem::temp_directory_path() / "wikilib-test-XXXXXX").string();
        const auto created = ::mkdtemp(pattern.data());
        if (!created)
            throw std::runtime_error("Cannot create test directory");
        path = created;
    }

    ~TempDirectory() {
        std::error_code ignored;
        std::filesystem::remove_all(path, ignored);
    }

    TempDirectory(const TempDirectory &) = delete;
    TempDirectory &operator=(const TempDirectory &) = delete;
    std::filesystem::path path;
};

inline void write_file(const std::filesystem::path &path, std::string_view data) {
    std::ofstream out(path, std::ios::binary);
    out.exceptions(std::ios::badbit | std::ios::failbit);
    out.write(data.data(), static_cast<std::streamsize>(data.size()));
    out.close();
}

inline std::string bz2(std::string_view text) {
    auto result = dump::compress_bz2(text);
    if (!result)
        throw std::runtime_error("Cannot prepare compressed test input");
    return *result;
}

} // namespace wikilib::test
