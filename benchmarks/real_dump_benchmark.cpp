// Bounded real-file benchmark. Original dump/index are accessed through read-only file opens.
#include <algorithm>
#include <chrono>
#include <deque>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <nlohmann/json.hpp>
#include <set>
#include <stdexcept>
#include <sys/resource.h>
#include "wikilib.hpp"

namespace fs = std::filesystem;
using nlohmann::json;
using namespace wikilib;
using namespace wikilib::dump;

namespace {
constexpr uint64_t budget = 128 * 1024 * 1024;
using Clock = std::chrono::steady_clock;

json load_json(const fs::path &file) {
    std::ifstream input(file);
    input.exceptions(std::ios::badbit | std::ios::failbit);
    json value;
    input >> value;
    return value;
}

void save_json(const fs::path &file, const json &value) {
    std::ofstream output(file);
    output.exceptions(std::ios::badbit | std::ios::failbit);
    output << value.dump(2) << '\n';
}

DumpPath layout(const json &plan) {
    DumpPath path(plan.at("work_dir").get<std::string>());
    path.set_project(WikiProject::Wikipedia).set_language(plan.at("language")).set_date(plan.at("date"));
    return path;
}

json encode(const IndexChunk &chunk) {
    json entries = json::array();
    for (const auto &entry: chunk.entries)
        entries.push_back({{"offset", entry.offset}, {"id", entry.page_id}, {"title", entry.title}});
    return {{"start", chunk.start_offset}, {"end", chunk.end_offset}, {"entries", entries}};
}

std::vector<IndexChunk> decode(const json &plan) {
    std::vector<IndexChunk> chunks;
    for (const auto &value: plan.at("selected")) {
        IndexChunk chunk;
        chunk.start_offset = value.at("start");
        chunk.end_offset = value.at("end");
        for (const auto &entry: value.at("entries"))
            chunk.entries.push_back({entry.at("offset"), entry.at("id"), entry.at("title")});
        chunks.push_back(std::move(chunk));
    }
    return chunks;
}

struct Metrics {
    size_t pages = 0;
    uint64_t content_bytes = 0, hash = 14695981039346656037ull;

    void consume(std::string_view title, std::string_view content) {
        ++pages;
        content_bytes += content.size();
        for (auto value: {title, content}) {
            for (unsigned char byte: value)
                hash = (hash ^ byte) * 1099511628211ull;
            hash = (hash ^ 255u) * 1099511628211ull;
        }
    }
};

void finish(json output, const Metrics &metrics, Clock::time_point begin) {
    rusage usage{};
    if (::getrusage(RUSAGE_SELF, &usage) != 0)
        throw std::runtime_error("getrusage failed");
    const double seconds = std::chrono::duration<double>(Clock::now() - begin).count();
    output["seconds"] = seconds;
    output["peak_RSS_KiB"] = usage.ru_maxrss;
    output["pages"] = metrics.pages;
    output["content_bytes"] = metrics.content_bytes;
    output["checksum"] = metrics.hash;
    output["content_MiB_per_second"] = metrics.content_bytes / (1024.0 * 1024 * seconds);
    std::cout << output.dump() << '\n';
}

void check(const DumpReader &reader) {
    if (!reader.error().empty())
        throw std::runtime_error(reader.error());
}

void prepare(const fs::path &dump, const fs::path &index, const fs::path &work, const fs::path &manifest) {
    const auto begin = Clock::now();
    const auto name = dump.filename().string();
    const auto split = name.find("wiki-");
    if (split == name.npos || name.size() < split + 13)
        throw std::runtime_error("Expected Wikipedia multistream filename");
    fs::create_directories(work);
    json plan = {{"dump", fs::absolute(dump).string()},     {"index", fs::absolute(index).string()},
                 {"work_dir", fs::absolute(work).string()}, {"language", name.substr(0, split)},
                 {"date", name.substr(split + 5, 8)},       {"dump_bytes", fs::file_size(dump)},
                 {"index_bytes", fs::file_size(index)},     {"budget_bytes", budget}};
    const auto path = layout(plan);
    fs::create_directories(path.date_dir());
    for (const auto &[source, destination]: {std::pair{dump, path.dump_path()}, std::pair{index, path.index_path()}}) {
        if (!fs::exists(destination))
            fs::create_symlink(fs::absolute(source), destination);
        if (!fs::equivalent(source, destination))
            throw std::runtime_error("Work layout already points to another file");
    }
    auto chunker = IndexChunker::from_file(index.string(), fs::file_size(dump), IndexLinePolicy::RejectMalformed);
    std::vector<IndexChunk> first, middle;
    std::deque<IndexChunk> last;
    size_t chunk_count = 0, page_count = 0;
    IndexChunk chunk;
    while (chunker.next_chunk(chunk)) {
        ++chunk_count;
        page_count += chunk.entries.size();
        if (first.size() < 10)
            first.push_back(chunk);
        if (middle.size() < 10 && chunk.start_offset >= fs::file_size(dump) / 2)
            middle.push_back(chunk);
        last.push_back(chunk);
        if (last.size() > 10)
            last.pop_front();
    }
    if (!chunker.error().empty())
        throw std::runtime_error(std::string(chunker.error()));
    std::vector<IndexChunk> selected = first;
    selected.insert(selected.end(), middle.begin(), middle.end());
    selected.insert(selected.end(), last.begin(), last.end());
    std::sort(selected.begin(), selected.end(),
              [](const auto &a, const auto &b) { return a.start_offset < b.start_offset; });
    selected.erase(std::unique(selected.begin(), selected.end(),
                               [](const auto &a, const auto &b) { return a.start_offset == b.start_offset; }),
                   selected.end());
    uint64_t compressed = 0;
    size_t selected_pages = 0;
    plan["selected"] = json::array();
    for (const auto &value: selected) {
        compressed += value.end_offset - value.start_offset;
        if (compressed > budget)
            throw std::runtime_error("Selected ranges exceed 128 MiB compressed budget");
        selected_pages += value.entries.size();
        plan["selected"].push_back(encode(value));
    }
    plan["index_chunks"] = chunk_count;
    plan["index_pages"] = page_count;
    plan["selected_compressed_bytes"] = compressed;
    plan["selected_pages"] = selected_pages;
    save_json(manifest, plan);
    finish({{"mode", "prepare"},
            {"index_chunks", chunk_count},
            {"index_pages", page_count},
            {"selected_chunks", selected.size()},
            {"selected_pages", selected_pages},
            {"selected_compressed_bytes", compressed}},
           {}, begin);
}

std::vector<std::string> requests(const std::vector<IndexChunk> &chunks, size_t count, bool spread) {
    std::vector<std::string> titles;
    if (spread) {
        for (size_t position = 0; titles.size() < count; ++position) {
            bool added = false;
            for (const auto &chunk: chunks) {
                if (position < chunk.entries.size() && titles.size() < count) {
                    titles.push_back(chunk.entries[position].title);
                    added = true;
                }
            }
            if (!added)
                throw std::runtime_error("Insufficient sampled pages");
        }
    } else {
        const auto chunk =
                std::find_if(chunks.begin(), chunks.end(), [&](const auto &c) { return c.entries.size() >= count; });
        if (chunk == chunks.end())
            throw std::runtime_error("No sampled chunk contains enough pages");
        for (size_t i = 0; i < count; ++i)
            titles.push_back(chunk->entries[i].title);
    }
    std::reverse(titles.begin(), titles.end());
    return titles;
}

void run(const json &plan, std::string mode, size_t count, bool spread) {
    const auto path = layout(plan);
    const auto chunks = decode(plan);
    Metrics metrics;
    json output = {{"mode", mode}, {"request_count", count}, {"spread", spread}};
    DumpReader reader(path);
    const auto begin = Clock::now();
    if (mode == "index-stream") {
        auto chunker =
                IndexChunker::from_file(path.index_path().string(), path.dump_size(), IndexLinePolicy::RejectMalformed);
        size_t pages = 0, total = 0;
        IndexChunk chunk;
        while (chunker.next_chunk(chunk)) {
            pages += chunk.entries.size();
            ++total;
        }
        if (!chunker.error().empty())
            throw std::runtime_error(std::string(chunker.error()));
        output["index_pages"] = pages;
        output["index_chunks"] = total;
    } else if (mode == "index-load") {
        reader.load_index();
        check(reader);
        output["index_pages"] = reader.page_count();
        output["index_chunks"] = reader.chunk_count();
    } else if (mode == "stream" || mode == "buffered" || mode == "cancel" || mode == "samples") {
        struct Sample {
            std::string title, content;
            PageId id;
        };

        std::vector<Sample> small, medium, large;
        json reference = json::object();
        auto sample = [&](const Page &page) {
            const auto add = [&](auto &values, auto less) {
                values.push_back({page.info.title, std::string(page.content()), page.info.id});
                std::sort(values.begin(), values.end(), less);
                if (values.size() > 10)
                    values.pop_back();
            };
            if (!page.content().empty() && page.content().size() <= 1024 * 1024) {
                add(small, [](const auto &a, const auto &b) { return a.content.size() < b.content.size(); });
                add(large, [](const auto &a, const auto &b) { return a.content.size() > b.content.size(); });
                add(medium, [](const auto &a, const auto &b) {
                    const auto distance = [](size_t n) { return n > 4096 ? n - 4096 : 4096 - n; };
                    return distance(a.content.size()) < distance(b.content.size());
                });
            }
        };
        uint64_t bytes = 0;
        size_t completed = 0;
        for (const auto &chunk: chunks) {
            if (mode == "buffered") {
                auto xml = reader.decompress_chunk(chunk.start_offset, chunk.end_offset - chunk.start_offset);
                check(reader);
                const auto pages = extract_all_from_xml(xml);
                if (pages.size() != chunk.entries.size())
                    throw std::runtime_error("Buffered page count mismatch");
                for (size_t i = 0; i < pages.size(); ++i) {
                    if (pages[i].first != chunk.entries[i].title)
                        throw std::runtime_error("Buffered title mismatch");
                    metrics.consume(pages[i].first, pages[i].second);
                }
                bytes += chunk.end_offset - chunk.start_offset;
            } else {
                DumpReader::ProcessProgress progress;
                const bool complete = reader.process_chunk(
                        chunk,
                        [&](const Page &page) {
                            metrics.consume(page.info.title, page.content());
                            if (mode == "samples") {
                                sample(page);
                                Metrics one;
                                one.consume(page.info.title, page.content());
                                reference[page.info.title] = one.hash;
                            }
                            return mode != "cancel" || metrics.pages < count;
                        },
                        [&](const auto &p) { progress = p; });
                check(reader);
                bytes += progress.bytes_compressed;
                if (!complete) {
                    if (mode != "cancel" || metrics.pages != count)
                        throw std::runtime_error("Unexpected cancellation");
                    output["cancelled"] = true;
                    break;
                }
            }
            ++completed;
        }
        output["compressed_bytes_fetched"] = bytes;
        output["completed_chunks"] = completed;
        if (mode == "samples") {
            const fs::path dir = fs::path(plan.at("work_dir").get<std::string>()) / "samples";
            fs::create_directories(dir);
            json saved = json::array();
            std::set<PageId> seen;
            for (const auto *group: {&small, &medium, &large}) {
                for (const auto &page: *group)
                    if (seen.insert(page.id).second) {
                        const auto file = dir / (std::to_string(page.id) + ".txt");
                        std::ofstream data(file, std::ios::binary);
                        data.exceptions(std::ios::badbit | std::ios::failbit);
                        data << page.content;
                        saved.push_back({{"id", page.id},
                                         {"title", page.title},
                                         {"bytes", page.content.size()},
                                         {"file", file.string()}});
                    }
            }
            save_json(dir / "manifest.json", saved);
            save_json(fs::path(plan.at("work_dir").get<std::string>()) / "reference.json", reference);
            output["parser_samples"] = saved.size();
        }
    } else if (mode == "batch" || mode == "single") {
        const auto titles = requests(chunks, count, spread);
        reader.load_index();
        check(reader);
        output["index_load_seconds"] = std::chrono::duration<double>(Clock::now() - begin).count();
        uint64_t planned_bytes = 0;
        std::set<size_t> selected_chunks;
        for (const auto &title: titles) {
            const auto info = reader.get_page_info(title);
            if (!info)
                throw std::runtime_error("Requested title missing from loaded index");
            if (mode == "single" || selected_chunks.insert(info->chunk_index).second) {
                const auto found = std::find_if(chunks.begin(), chunks.end(), [&](const auto &c) {
                    return std::any_of(c.entries.begin(), c.entries.end(),
                                       [&](const auto &e) { return e.title == title; });
                });
                if (found == chunks.end())
                    throw std::runtime_error("Request outside sampled chunks");
                planned_bytes += found->end_offset - found->start_offset;
            }
        }
        if (planned_bytes > budget)
            throw std::runtime_error("Extraction would exceed 128 MiB compressed-read budget");
        const auto reference = load_json(fs::path(plan.at("work_dir").get<std::string>()) / "reference.json");
        const auto verify = [&](const ExtractedPage &page) {
            Metrics one;
            one.consume(page.title, page.content);
            if (!reference.contains(page.title) || reference.at(page.title).get<uint64_t>() != one.hash)
                throw std::runtime_error("Extracted content does not match sampled streaming reference");
        };
        const auto extraction_begin = Clock::now();
        if (mode == "batch") {
            for (const auto &page: reader.extract_pages(titles)) {
                if (!page.found)
                    throw std::runtime_error("Batch page missing: " + reader.error());
                verify(page);
                metrics.consume(page.title, page.content);
            }
        } else {
            for (const auto &title: titles) {
                const auto page = reader.extract_page(title);
                if (!page.found)
                    throw std::runtime_error("Single page missing: " + reader.error());
                verify(page);
                metrics.consume(page.title, page.content);
            }
        }
        check(reader);
        output["extraction_seconds"] = std::chrono::duration<double>(Clock::now() - extraction_begin).count();
        output["planned_compressed_bytes"] = planned_bytes;
    } else if (mode == "parse") {
        auto samples = load_json(fs::path(plan.at("work_dir").get<std::string>()) / "samples/manifest.json");
        size_t errors = 0, sections = 0;
        json pages = json::array();
        for (const auto &sample: samples) {
            std::ifstream data(sample.at("file").get<std::string>(), std::ios::binary);
            const std::string content{std::istreambuf_iterator<char>(data), {}};
            if (content.size() != sample.at("bytes").get<size_t>())
                throw std::runtime_error("Sample size mismatch");
            std::cerr << "parsing page_id=" << sample.at("id") << " bytes=" << content.size() << '\n';
            const auto started = Clock::now();
            auto parsed = markup::parse(content);
            if (!parsed.document)
                throw std::runtime_error("No parsed document");
            const auto tree = markup::build_section_tree(parsed.document.get());
            const auto page_sections = tree->total_section_count();
            errors += parsed.errors.size();
            sections += page_sections;
            json diagnostics = json::array();
            for (const auto &error: parsed.errors)
                diagnostics.push_back({{"message", error.message}, {"context", error.context}});
            metrics.consume(sample.at("title").get<std::string>(), content);
            pages.push_back({{"id", sample.at("id")},
                             {"bytes", content.size()},
                             {"errors", parsed.errors.size()},
                             {"diagnostics", diagnostics},
                             {"sections", page_sections},
                             {"seconds", std::chrono::duration<double>(Clock::now() - started).count()}});
        }
        output["parse_errors"] = errors;
        output["sections"] = sections;
        output["page_metrics"] = pages;
    } else
        throw std::runtime_error("Unknown mode");
    finish(output, metrics, begin);
}
} // namespace

int main(int argc, char **argv) {
    try {
        if (argc == 6 && std::string_view(argv[1]) == "prepare") {
            prepare(argv[2], argv[3], argv[4], argv[5]);
        } else if (argc >= 4 && std::string_view(argv[1]) == "run") {
            run(load_json(argv[2]), argv[3], argc > 4 ? std::stoull(argv[4]) : 100,
                argc > 5 && std::string_view(argv[5]) == "spread");
        } else
            throw std::runtime_error("Usage: benchmark_real_dump prepare DUMP INDEX WORK_DIR PLAN | run PLAN MODE "
                                     "[COUNT] [same|spread]");
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
