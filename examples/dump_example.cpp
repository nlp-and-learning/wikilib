/**
 * @file dump_example.cpp
 * @brief Example: Processing Wikipedia/Wiktionary dump files
 */

#include <chrono>
#include <iostream>
#include <wikilib.hpp>

using namespace wikilib;
using namespace wikilib::dump;

void print_progress(const PageHandler::Stats &stats, uint64_t total_size, uint64_t current) {
    double progress = total_size > 0 ? (100.0 * current / total_size) : 0;
    std::cout << "\rProcessed: " << stats.pages_processed << " pages (" << std::fixed << std::setprecision(1)
              << progress << "%)" << std::flush;
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <dump-file.xml.bz2>" << std::endl;
        std::cerr << "Example: " << argv[0] << " enwiktionary-latest-pages-articles.xml.bz2" << std::endl;
        return 1;
    }

    const std::string dump_path = argv[1];
    std::cout << "Processing dump: " << dump_path << std::endl;

    // Get file size for progress tracking
    uint64_t file_size = get_file_size(dump_path);

    // Create page handler
    PageHandler handler(dump_path);

    // Print site info
    const auto &info = handler.site_info();
    std::cout << "Site: " << info.site_name << std::endl;
    std::cout << "Database: " << info.db_name << std::endl;
    std::cout << "Namespaces: " << info.namespaces.size() << std::endl;
    std::cout << std::endl;

    // Configure filter
    PageFilter filter;
    filter.namespaces = {{0}}; // Main namespace only
    filter.include_redirects = false;
    filter.only_latest_revision = true;

    // Statistics
    size_t total_pages = 0;
    size_t total_links = 0;
    size_t total_templates = 0;

    auto start_time = std::chrono::steady_clock::now();

    // Process pages
    handler.process(
            [&](const Page &page) {
                total_pages++;

                // Parse the page content
                auto result = markup::parse(page.content());
                if (!result.success()) {
                    return true; // Skip failed pages
                }

                // Count links
                markup::LinkExtractor links;
                links.visit(*result.document);
                total_links += links.links().size();

                // Count templates
                markup::TemplateExtractor templates;
                templates.visit(*result.document);
                total_templates += templates.templates().size();

                // Progress update every 1000 pages
                if (total_pages % 1000 == 0) {
                    print_progress(handler.stats(), file_size, handler.stats().bytes_processed);
                }

                // Process more pages
                return true;
            },
            filter);

    auto end_time = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::seconds>(end_time - start_time);

    std::cout << std::endl << std::endl;
    std::cout << "Processing complete!" << std::endl;
    std::cout << "-------------------" << std::endl;
    std::cout << "Total pages: " << total_pages << std::endl;
    std::cout << "Total links: " << total_links << std::endl;
    std::cout << "Total templates: " << total_templates << std::endl;
    std::cout << "Time: " << duration.count() << " seconds" << std::endl;

    if (duration.count() > 0) {
        std::cout << "Speed: " << (total_pages / duration.count()) << " pages/second" << std::endl;
    }

    return 0;
}
