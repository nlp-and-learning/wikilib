/**
 * @file extract_pages_example.cpp
 * @brief Example of extracting pages from Wikimedia dumps using index
 *
 * This example demonstrates:
 * - Finding wikimedia-dump-fetcher directory automatically
 * - Loading index for random access to dump
 * - Extracting specific pages by title
 * - Batch extraction of multiple pages (grouped by chunk for efficiency)
 * - Reading terms from terms_to_extract.*.txt files
 */

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <string>
#include <vector>
#include "wikilib/dump/dump_path.h"
#include "wikilib/dump/dump_reader.h"

namespace fs = std::filesystem;
using namespace wikilib::dump;

void print_usage(const char* program) {
    std::cerr << "Usage: " << program << " [options]\n"
              << "Options:\n"
              << "  -b, --base DIR     Base directory (wikimedia-dump-fetcher)\n"
              << "  -p, --project NAME Project: wiki, wiktionary, etc.\n"
              << "  -l, --lang CODE    Language code (e.g., pl, en)\n"
              << "  -d, --date DATE    Dump date (YYYYMMDD), or latest if omitted\n"
              << "  -t, --terms FILE   File with terms to extract (one per line)\n"
              << "  -w, --work DIR     Work directory with terms_to_extract.*.txt\n"
              << "  -o, --output DIR   Output directory for extracted pages\n"
              << "  --list             Just list available languages and dates\n"
              << "\nExamples:\n"
              << "  " << program << " -p wiktionary -l pl -t terms.txt\n"
              << "  " << program << " -w /path/to/work -p wiktionary\n";
}

std::optional<WikiProject> string_to_project(const std::string& name) {
    return dirname_to_project(name);
}

int main(int argc, char* argv[]) {
    std::string base_dir;
    std::string project_name = "wiktionary";
    std::string language = "pl";
    std::string date;
    std::string terms_file;
    std::string work_dir;
    std::string output_dir;
    bool list_only = false;

    // Parse arguments
    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        if ((arg == "-b" || arg == "--base") && i + 1 < argc) {
            base_dir = argv[++i];
        } else if ((arg == "-p" || arg == "--project") && i + 1 < argc) {
            project_name = argv[++i];
        } else if ((arg == "-l" || arg == "--lang") && i + 1 < argc) {
            language = argv[++i];
        } else if ((arg == "-d" || arg == "--date") && i + 1 < argc) {
            date = argv[++i];
        } else if ((arg == "-t" || arg == "--terms") && i + 1 < argc) {
            terms_file = argv[++i];
        } else if ((arg == "-w" || arg == "--work") && i + 1 < argc) {
            work_dir = argv[++i];
        } else if ((arg == "-o" || arg == "--output") && i + 1 < argc) {
            output_dir = argv[++i];
        } else if (arg == "--list") {
            list_only = true;
        } else if (arg == "-h" || arg == "--help") {
            print_usage(argv[0]);
            return 0;
        } else {
            std::cerr << "Unknown option: " << arg << "\n";
            print_usage(argv[0]);
            return 1;
        }
    }

    // Find or validate base directory
    std::unique_ptr<DumpPath> path;
    try {
        if (base_dir.empty()) {
            // Try to find automatically
            auto found = find_dump_fetcher_dir();
            if (!found) {
                std::cerr << "Error: Could not find wikimedia-dump-fetcher directory.\n"
                          << "Use -b to specify the base directory.\n";
                return 1;
            }
            std::cout << "Found dump fetcher at: " << found->string() << "\n";
            path = std::make_unique<DumpPath>(*found);
        } else {
            path = std::make_unique<DumpPath>(base_dir);
        }
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }

    // Set project
    auto project = string_to_project(project_name);
    if (!project) {
        std::cerr << "Unknown project: " << project_name << "\n";
        return 1;
    }
    path->set_project(*project);
    path->set_language(language);

    if (!date.empty()) {
        path->set_date(date);
    }

    // List mode - show available data
    if (list_only) {
        std::cout << "Project: " << project_to_dirname(*project) << "\n";
        std::cout << "Available dates:\n";
        for (const auto& d : path->available_dates()) {
            std::cout << "  " << d << "\n";
        }

        std::cout << "\nAvailable languages for " << path->date() << ":\n";
        for (const auto& l : path->available_languages()) {
            std::cout << "  " << l << "\n";
        }
        return 0;
    }

    // Check dump exists
    if (!path->dump_exists()) {
        std::cerr << "Dump file not found: " << path->dump_path() << "\n";
        return 1;
    }

    if (!path->index_exists()) {
        std::cerr << "Index file not found: " << path->index_path() << "\n";
        return 1;
    }

    std::cout << "Dump: " << path->symbolic_name() << "\n";
    std::cout << "Dump file: " << path->dump_path().filename().string() << "\n";
    std::cout << "Index file: " << path->index_path().filename().string() << "\n";
    std::cout << "Dump size: " << (path->dump_size() / 1024 / 1024) << " MB\n";

    // Read terms to extract
    std::vector<std::string> terms;
    if (!terms_file.empty()) {
        terms = read_terms_file(terms_file);
        std::cout << "Loaded " << terms.size() << " terms from " << terms_file << "\n";
    } else if (!work_dir.empty()) {
        terms = read_terms_for_language(work_dir, language);
        std::cout << "Loaded " << terms.size() << " terms for language " << language << "\n";
    } else {
        // Try default location: examples/data/ relative to executable or source
        std::vector<fs::path> search_paths = {
            fs::current_path() / "examples" / "data",
            fs::current_path().parent_path() / "examples" / "data",
            fs::path(__FILE__).parent_path() / "data"
        };

        for (const auto& search_path : search_paths) {
            if (fs::exists(search_path)) {
                terms = read_terms_for_language(search_path, language);
                if (!terms.empty()) {
                    std::cout << "Loaded " << terms.size() << " terms from "
                              << search_path.string() << "\n";
                    break;
                }
            }
        }
    }

    if (terms.empty()) {
        std::cerr << "No terms to extract.\n"
                  << "Use -t FILE or -w DIR to specify terms, or place files in examples/data/\n";
        return 1;
    }

    // Create dump reader
    DumpReader reader(*path);

    // Load index with progress
    std::cout << "Loading index...\n";
    auto start_time = std::chrono::steady_clock::now();
    size_t last_pages = 0;

    reader.load_index([&](size_t chunks) {
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - start_time).count();

        size_t pages = reader.page_count();
        double pages_per_sec = elapsed > 0 ? static_cast<double>(pages) / elapsed : 0;

        // Clear line and print progress
        std::cout << "\r\033[K  Chunks: " << std::setw(8) << chunks
                  << "  Pages: " << std::setw(10) << pages
                  << "  Time: " << std::setw(4) << elapsed << "s"
                  << "  (" << std::fixed << std::setprecision(0)
                  << pages_per_sec << " pages/s)" << std::flush;

        last_pages = pages;
    });

    auto end_time = std::chrono::steady_clock::now();
    auto total_time = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time).count();

    std::cout << "\n";

    if (!reader.index_loaded()) {
        std::cerr << "Failed to load index: " << reader.error() << "\n";
        return 1;
    }

    std::cout << "Index loaded: " << reader.page_count() << " pages in "
              << reader.chunk_count() << " chunks"
              << " (" << (total_time / 1000.0) << "s)\n";

    // Check which terms exist
    size_t found_count = 0;
    for (const auto& term : terms) {
        if (reader.has_page(term)) {
            found_count++;
        }
    }
    std::cout << "Found " << found_count << " of " << terms.size() << " terms in index\n";

    // Extract pages
    std::cout << "Extracting pages...\n";
    auto pages = reader.extract_pages(terms);

    // Create output directory if needed
    fs::path out_path;
    if (!output_dir.empty()) {
        out_path = output_dir;
        fs::create_directories(out_path);
    } else {
        out_path = fs::current_path() / "extracted";
        fs::create_directories(out_path);
    }

    // Save extracted pages
    size_t extracted_count = 0;
    for (const auto& page : pages) {
        if (!page.found) continue;

        // Create safe filename
        std::string filename = page.title;
        for (char& c : filename) {
            if (c == '/' || c == '\\' || c == ':' || c == '*' ||
                c == '?' || c == '"' || c == '<' || c == '>' || c == '|') {
                c = '_';
            }
        }
        filename += ".wiki";

        fs::path file_path = out_path / filename;
        std::ofstream out(file_path);
        if (out) {
            out << page.content;
            extracted_count++;
        }
    }

    std::cout << "Extracted " << extracted_count << " pages to " << out_path.string() << "\n";

    // Report missing
    size_t missing = 0;
    for (const auto& page : pages) {
        if (!page.found) {
            missing++;
            if (missing <= 10) {
                std::cout << "  Missing: " << page.title << "\n";
            }
        }
    }
    if (missing > 10) {
        std::cout << "  ... and " << (missing - 10) << " more\n";
    }

    return 0;
}
