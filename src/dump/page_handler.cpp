/**
 * @file page_handler.cpp
 * @brief Implementation of page-level handling for MediaWiki dump processing
 */

#include "wikilib/dump/page_handler.h"
#include <algorithm>
#include <charconv>
#include <stdexcept>
#include "wikilib/dump/xml_reader.h"

namespace wikilib::dump {

namespace {

template<typename T>
T parse_number(std::string_view value) {
    const auto start = value.find_first_not_of(" \t\r\n");
    if (start == std::string_view::npos)
        throw std::runtime_error("Empty numeric XML metadata");
    value = value.substr(start, value.find_last_not_of(" \t\r\n") - start + 1);
    T number{};
    const auto parsed = std::from_chars(value.data(), value.data() + value.size(), number);
    if (parsed.ec != std::errc{} || parsed.ptr != value.data() + value.size())
        throw std::runtime_error("Invalid numeric XML metadata: " + std::string(value));
    return number;
}

} // namespace

// ============================================================================
// PageFilter implementation
// ============================================================================

bool PageFilter::matches(const Page &page) const {
    // Check namespace filter
    if (namespaces.has_value()) {
        bool found = false;
        for (NamespaceId ns: *namespaces) {
            if (page.info.namespace_id == ns) {
                found = true;
                break;
            }
        }
        if (!found)
            return false;
    }

    // Check redirect filter
    if (!include_redirects && page.info.is_redirect()) {
        return false;
    }

    // Check title prefix
    if (title_prefix.has_value()) {
        if (!page.info.title.starts_with(*title_prefix)) {
            return false;
        }
    }

    // Check title contains
    if (title_contains.has_value()) {
        if (page.info.title.find(*title_contains) == std::string::npos) {
            return false;
        }
    }

    return true;
}

// ============================================================================
// PageHandler implementation
// ============================================================================

struct PageHandler::Impl {
    std::unique_ptr<XmlReader> reader;
    SiteInfo site_info;
    Stats stats;
    std::string error_message;
    bool header_parsed = false;
    bool at_eof = false;
    bool page_started = false;

    void parse_header();
    std::optional<XmlEvent> next_event();
    bool sync_error();
    std::optional<Page> read_page(bool only_latest_revision = false);
    Revision parse_revision();
};

bool PageHandler::Impl::sync_error() {
    if (reader && !reader->error().empty()) {
        error_message = std::string(reader->error());
        at_eof = true;
    }
    return !error_message.empty();
}

std::optional<XmlEvent> PageHandler::Impl::next_event() {
    auto event = reader->next();
    if (sync_error() || !event || event->type == XmlEventType::EndDocument) {
        at_eof = true;
        return std::nullopt;
    }
    return event;
}

void PageHandler::Impl::parse_header() {
    if (header_parsed || !reader)
        return;
    header_parsed = true;

    while (true) {
        auto event = next_event();
        if (!event)
            break;

        if (event->type == XmlEventType::StartElement) {
            if (event->name == "sitename") {
                site_info.site_name = reader->read_text();
            } else if (event->name == "dbname") {
                site_info.db_name = reader->read_text();
            } else if (event->name == "base") {
                site_info.base_url = reader->read_text();
            } else if (event->name == "generator") {
                site_info.generator = reader->read_text();
            } else if (event->name == "namespace") {
                Namespace ns{};
                if (auto key = event->get_attribute("key")) {
                    ns.id = parse_number<NamespaceId>(*key);
                }
                ns.name = reader->read_text();
                ns.canonical_name = ns.name;
                site_info.namespaces.push_back(std::move(ns));
            } else if (event->name == "page") {
                page_started = true;
                break;
            }
        } else if (event->type == XmlEventType::EndElement) {
            if (event->name == "siteinfo" || event->name == "namespaces") {
                // Continue to find pages
            }
        }
    }
    sync_error();
}

Revision PageHandler::Impl::parse_revision() {
    Revision rev;
    int depth = 1;

    while (depth > 0) {
        auto event = next_event();
        if (!event)
            break;

        if (event->type == XmlEventType::StartElement) {
            depth++;
            if (event->name == "id" && depth == 2) {
                rev.id = parse_number<RevisionId>(reader->read_text());
                depth--;
            } else if (event->name == "parentid") {
                rev.parent_id = parse_number<RevisionId>(reader->read_text());
                depth--;
            } else if (event->name == "timestamp") {
                rev.timestamp = reader->read_text();
                depth--;
            } else if (event->name == "contributor") {
                // Parse contributor - can have username or ip
                int contrib_depth = 1;
                while (contrib_depth > 0) {
                    auto ce = next_event();
                    if (!ce)
                        break;
                    if (ce->type == XmlEventType::StartElement) {
                        contrib_depth++;
                        if (ce->name == "username" || ce->name == "ip") {
                            rev.contributor = reader->read_text();
                            contrib_depth--;
                        }
                    } else if (ce->type == XmlEventType::EndElement) {
                        contrib_depth--;
                    }
                }
                depth--;
            } else if (event->name == "comment") {
                rev.comment = reader->read_text();
                depth--;
            } else if (event->name == "model") {
                rev.model = reader->read_text();
                depth--;
            } else if (event->name == "format") {
                rev.format = reader->read_text();
                depth--;
            } else if (event->name == "text") {
                rev.content = reader->read_text();
                depth--;
            } else if (event->name == "sha1") {
                rev.sha1 = reader->read_text();
                depth--;
            }
        } else if (event->type == XmlEventType::EndElement) {
            depth--;
        }
    }

    return rev;
}

std::optional<Page> PageHandler::Impl::read_page(bool only_latest_revision) {
    if (!reader || at_eof) {
        return std::nullopt;
    }

    // Find start of <page>
    while (!page_started) {
        auto event = next_event();
        if (!event) {
            at_eof = true;
            return std::nullopt;
        }

        if (event->type == XmlEventType::StartElement && event->name == "page") {
            page_started = true;
            break;
        }

        if (event->type == XmlEventType::EndDocument) {
            at_eof = true;
            return std::nullopt;
        }
    }
    page_started = false;

    Page page;
    int depth = 1;

    while (depth > 0) {
        auto event = next_event();
        if (!event)
            break;

        if (event->type == XmlEventType::StartElement) {
            depth++;

            if (event->name == "title") {
                page.info.title = reader->read_text();
                depth--;
            } else if (event->name == "ns") {
                page.info.namespace_id = parse_number<NamespaceId>(reader->read_text());
                depth--;
            } else if (event->name == "id" && depth == 2) {
                page.info.id = parse_number<PageId>(reader->read_text());
                depth--;
            } else if (event->name == "redirect") {
                if (auto title = event->get_attribute("title")) {
                    page.info.redirect_target = std::string(*title);
                }
                stats.redirects_found++;
            } else if (event->name == "revision") {
                if (only_latest_revision)
                    page.revisions.clear();
                page.revisions.push_back(parse_revision());
                depth--;
            }
        } else if (event->type == XmlEventType::EndElement) {
            depth--;
        }
    }

    if (sync_error() || depth != 0) {
        if (error_message.empty())
            error_message = "Unexpected end of XML page";
        at_eof = true;
        return std::nullopt;
    }
    stats.pages_read++;
    stats.bytes_processed = reader->bytes_processed();
    if (const auto *revision = page.latest_revision()) {
        page.info.revision_id = revision->id;
        page.info.timestamp = revision->timestamp;
    }

    return page;
}

PageHandler::PageHandler(const std::string &dump_path) : impl_(std::make_unique<Impl>()) {
    impl_->reader = std::make_unique<XmlReader>(dump_path);
    if (!impl_->reader->error().empty()) {
        impl_->error_message = std::string(impl_->reader->error());
        impl_->at_eof = true;
    }
    try {
        impl_->parse_header();
    } catch (const std::exception &error) {
        if (!impl_->sync_error())
            impl_->error_message = error.what();
        impl_->at_eof = true;
    }
}

PageHandler::PageHandler(std::unique_ptr<XmlReader> reader) : impl_(std::make_unique<Impl>()) {
    impl_->reader = std::move(reader);
    if (!impl_->reader) {
        impl_->error_message = "Null XML reader";
        impl_->at_eof = true;
        return;
    }
    if (impl_->reader && !impl_->reader->error().empty()) {
        impl_->error_message = std::string(impl_->reader->error());
        impl_->at_eof = true;
    }
    try {
        impl_->parse_header();
    } catch (const std::exception &error) {
        if (!impl_->sync_error())
            impl_->error_message = error.what();
        impl_->at_eof = true;
    }
}

PageHandler::~PageHandler() = default;

PageHandler::PageHandler(PageHandler &&) noexcept = default;
PageHandler &PageHandler::operator=(PageHandler &&) noexcept = default;

void PageHandler::process(PageCallback callback) {
    process(callback, PageFilter{});
}

void PageHandler::process(PageCallback callback, const PageFilter &filter) {
    size_t count = 0;

    while (true) {
        auto page = next_page(filter);
        if (!page)
            break;

        impl_->stats.pages_processed++;

        if (!callback(*page)) {
            break;
        }

        ++count;
        if (filter.max_pages.has_value() && count >= *filter.max_pages) {
            break;
        }
    }
}

std::optional<Page> PageHandler::next_page() {
    if (!impl_)
        return std::nullopt;
    try {
        return impl_->read_page();
    } catch (const std::exception &error) {
        if (!impl_->sync_error())
            impl_->error_message = error.what();
        impl_->at_eof = true;
        return std::nullopt;
    }
}

std::optional<Page> PageHandler::next_page(const PageFilter &filter) {
    while (true) {
        if (!impl_)
            return std::nullopt;
        std::optional<Page> page;
        try {
            page = impl_->read_page(filter.only_latest_revision);
        } catch (const std::exception &error) {
            if (!impl_->sync_error())
                impl_->error_message = error.what();
            impl_->at_eof = true;
            return std::nullopt;
        }
        if (!page)
            return std::nullopt;

        if (filter.matches(*page)) {
            return page;
        }

        impl_->stats.pages_skipped++;
    }
}

const PageHandler::SiteInfo &PageHandler::site_info() const {
    static SiteInfo empty;
    return impl_ ? impl_->site_info : empty;
}

const PageHandler::Stats &PageHandler::stats() const noexcept {
    static Stats empty;
    return impl_ ? impl_->stats : empty;
}

bool PageHandler::eof() const noexcept {
    return !impl_ || impl_->at_eof;
}

std::string_view PageHandler::error() const noexcept {
    return impl_ ? std::string_view(impl_->error_message) : std::string_view{};
}

// ============================================================================
// Convenience functions
// ============================================================================

void process_dump(const std::string &path, PageCallback callback, const PageFilter &filter) {
    PageHandler handler(path);
    handler.process(callback, filter);
}

size_t count_pages(const std::string &path, const PageFilter &filter) {
    size_t count = 0;
    PageHandler handler(path);

    while (auto page = handler.next_page(filter)) {
        ++count;
        if (filter.max_pages.has_value() && count >= *filter.max_pages) {
            break;
        }
    }

    return count;
}

std::optional<Page> find_page(const std::string &path, std::string_view title) {
    PageHandler handler(path);

    while (auto page = handler.next_page()) {
        if (page->info.title == title) {
            return page;
        }
    }

    return std::nullopt;
}

std::vector<Page> extract_templates(const std::string &path) {
    std::vector<Page> templates;

    PageFilter filter;
    filter.namespaces = std::vector<NamespaceId>{10}; // Template namespace

    PageHandler handler(path);
    handler.process(
            [&templates](const Page &page) {
                templates.push_back(page);
                return true;
            },
            filter);

    return templates;
}

} // namespace wikilib::dump
