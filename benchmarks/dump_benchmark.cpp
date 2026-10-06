// Deterministic local fixture; generation and measurement run in separate processes.
#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <sys/resource.h>
#include "wikilib/dump/dump_reader.h"

using namespace wikilib::dump;

namespace {
void generate(const DumpPath &path, size_t pages, size_t per_chunk) {
    if (!pages || !per_chunk)
        throw std::invalid_argument("Page counts must be positive");
    std::filesystem::create_directories(path.date_dir());
    std::ofstream dump(path.dump_path(), std::ios::binary);
    auto index_path = path.index_path();
    index_path.replace_extension();
    if (std::filesystem::exists(path.index_path()))
        throw std::runtime_error("Remove the existing compressed index before generating this fixture");
    std::ofstream index(index_path);
    dump.exceptions(std::ios::badbit | std::ios::failbit);
    index.exceptions(std::ios::badbit | std::ios::failbit);
    uint64_t offset = 0, xml_bytes = 0;
    for (size_t start = 0; start < pages; start += per_chunk) {
        std::string xml = start == 0 ? "<mediawiki><siteinfo/>\n" : "";
        for (size_t i = start; i < std::min(pages, start + per_chunk); ++i) {
            const auto title = "Page" + std::to_string(i + 1);
            index << offset << ':' << i + 1 << ':' << title << '\n';
            std::string text = "== Heading ==\n";
            unsigned int state = static_cast<unsigned int>(i + 1);
            while (text.size() < 4096) {
                state = state * 1664525u + 1013904223u;
                text += "[[Link|word" + std::to_string(state % 1000) + "]] {{template|value}} ";
            }
            text.resize(4096);
            xml += "<page><title>" + title + "</title><ns>0</ns><id>" + std::to_string(i + 1) +
                   "</id><revision><text>" + text + "</text></revision></page>\n";
        }
        if (start + per_chunk >= pages)
            xml += "</mediawiki>";
        xml_bytes += xml.size();
        auto compressed = compress_bz2(xml);
        if (!compressed)
            throw std::runtime_error("Compression failed");
        dump.write(compressed->data(), static_cast<std::streamsize>(compressed->size()));
        offset += compressed->size();
    }
    std::cout << "pages=" << pages << " pages_per_chunk=" << per_chunk << " xml_bytes=" << xml_bytes
              << " compressed_bytes=" << offset << '\n';
}
} // namespace

int main(int argc, char **argv) {
    try {
        if (argc < 3)
            throw std::invalid_argument("Usage: benchmark_dump BASE generate PAGES PER_CHUNK | BASE "
                                        "sequential|indexed|buffered|batch [REQUESTS]");
        DumpPath path(argv[1]);
        path.set_project(WikiProject::Wikipedia).set_language("en").set_date("20260101");
        const std::string mode = argv[2];
        if (mode == "generate") {
            if (argc != 5)
                throw std::invalid_argument("generate requires PAGES and PER_CHUNK");
            generate(path, std::stoull(argv[3]), std::stoull(argv[4]));
            return 0;
        }
        DumpReader reader(path);
        size_t count = 0, content_bytes = 0;
        uint64_t checksum = 14695981039346656037ull;
        const auto consume = [&](std::string_view title, std::string_view content) {
            ++count;
            content_bytes += content.size();
            for (const auto value: {title, content}) {
                for (unsigned char byte: value)
                    checksum = (checksum ^ byte) * 1099511628211ull;
                checksum = (checksum ^ 0xffu) * 1099511628211ull;
            }
            return true;
        };
        const auto begin = std::chrono::steady_clock::now();
        if (mode == "sequential") {
            reader.process_all([&](const auto &title, const auto &content) { return consume(title, content); });
        } else if (mode == "indexed") {
            if (!reader.process_indexed([&](const Page &page) { return consume(page.info.title, page.content()); }))
                throw std::runtime_error(reader.error());
        } else if (mode == "buffered" || mode == "batch") {
            reader.load_index();
            if (!reader.index_loaded())
                throw std::runtime_error(reader.error());
            if (mode == "buffered") {
                for (size_t chunk = 0; chunk < reader.chunk_count(); ++chunk) {
                    const auto xml = reader.decompress_chunk(chunk);
                    if (!reader.error().empty())
                        throw std::runtime_error(reader.error());
                    for (const auto &[title, content]: extract_all_from_xml(xml))
                        consume(title, content);
                }
            } else {
                const size_t requested = argc > 3 ? std::stoull(argv[3]) : 128;
                std::vector<std::string> titles;
                for (size_t i = requested; i > 0; --i)
                    titles.push_back("Page" + std::to_string(i));
                for (const auto &page: reader.extract_pages(titles)) {
                    if (!page.found)
                        throw std::runtime_error("Requested page missing");
                    consume(page.title, page.content);
                }
            }
        } else
            throw std::invalid_argument("Unknown mode");
        if (!reader.error().empty())
            throw std::runtime_error(reader.error());
        const double seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - begin).count();
        rusage usage{};
        if (::getrusage(RUSAGE_SELF, &usage) != 0)
            throw std::runtime_error("getrusage failed");
        std::cout << "mode=" << mode << " pages=" << count << " seconds=" << seconds
                  << " content_MiB_per_second=" << content_bytes / (1024.0 * 1024 * seconds)
                  << " peak_RSS_KiB=" << usage.ru_maxrss << " checksum=" << checksum << '\n';
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
