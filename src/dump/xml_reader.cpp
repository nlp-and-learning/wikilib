/**
 * @file xml_reader.cpp
 * @brief Implementation of streaming XML reader for MediaWiki dumps
 */

#include "wikilib/dump/xml_reader.h"
#include <algorithm>
#include <cstring>
#include <fstream>
#include <sstream>
#include <stack>
#include "wikilib/dump/bz2_stream.h"

namespace wikilib::dump {

// ============================================================================
// XmlEvent implementation
// ============================================================================

std::optional<std::string_view> XmlEvent::get_attribute(std::string_view attr_name) const {
    for (const auto &attr: attributes) {
        if (attr.name == attr_name) {
            return attr.value;
        }
    }
    return std::nullopt;
}

// ============================================================================
// XmlReader implementation
// ============================================================================

struct XmlReader::Impl {
    std::unique_ptr<Bz2Stream> bz2_stream;
    std::unique_ptr<std::istream> plain_stream;
    std::string buffer;
    size_t buffer_pos = 0;
    std::stack<std::string> element_stack;
    bool at_eof = false;
    bool document_started = false;
    bool document_ended = false;
    std::string error_message;
    uint64_t bytes_processed = 0;

    // Stored strings for lifetime management
    std::string current_element_name;
    std::string current_text;
    std::vector<std::pair<std::string, std::string>> current_attributes;

    static constexpr size_t BUFFER_SIZE = 64 * 1024;
    char read_buffer[BUFFER_SIZE];

    bool refill_buffer();
    char peek_char();
    char get_char();
    bool skip_whitespace();
    std::string read_name();
    std::string read_quoted_string();
    std::string decode_entities(std::string_view text);
    void skip_until(char c);
    void skip_until(std::string_view s);
    bool match(std::string_view s);
};

bool XmlReader::Impl::refill_buffer() {
    if (buffer_pos < buffer.size()) {
        return true;
    }

    buffer.clear();
    buffer_pos = 0;

    if (bz2_stream) {
        size_t n = bz2_stream->read(read_buffer, BUFFER_SIZE);
        if (n > 0) {
            buffer.assign(read_buffer, n);
            bytes_processed += n;
            return true;
        }
        at_eof = bz2_stream->eof();
    } else if (plain_stream) {
        plain_stream->read(read_buffer, BUFFER_SIZE);
        size_t n = static_cast<size_t>(plain_stream->gcount());
        if (n > 0) {
            buffer.assign(read_buffer, n);
            bytes_processed += n;
            return true;
        }
        at_eof = plain_stream->eof();
    } else {
        at_eof = true;
    }

    return false;
}

char XmlReader::Impl::peek_char() {
    if (buffer_pos >= buffer.size()) {
        if (!refill_buffer()) {
            return '\0';
        }
    }
    if (buffer_pos >= buffer.size()) {
        return '\0';
    }
    return buffer[buffer_pos];
}

char XmlReader::Impl::get_char() {
    if (buffer_pos >= buffer.size()) {
        if (!refill_buffer()) {
            return '\0';
        }
    }
    if (buffer_pos >= buffer.size()) {
        return '\0';
    }
    return buffer[buffer_pos++];
}

bool XmlReader::Impl::skip_whitespace() {
    while (true) {
        char c = peek_char();
        if (c == '\0')
            return false;
        if (c == ' ' || c == '\t' || c == '\n' || c == '\r') {
            get_char();
        } else {
            break;
        }
    }
    return true;
}

std::string XmlReader::Impl::read_name() {
    std::string name;
    while (true) {
        char c = peek_char();
        if (c == '\0')
            break;
        if (std::isalnum(static_cast<unsigned char>(c)) || c == '_' || c == '-' || c == ':' || c == '.') {
            name += get_char();
        } else {
            break;
        }
    }
    return name;
}

std::string XmlReader::Impl::read_quoted_string() {
    char quote = get_char();
    if (quote != '"' && quote != '\'') {
        return "";
    }

    std::string value;
    while (true) {
        char c = get_char();
        if (c == '\0' || c == quote)
            break;
        value += c;
    }

    return decode_entities(value);
}

std::string XmlReader::Impl::decode_entities(std::string_view text) {
    std::string result;
    result.reserve(text.size());

    size_t i = 0;
    while (i < text.size()) {
        if (text[i] == '&') {
            size_t end = text.find(';', i);
            if (end != std::string_view::npos) {
                std::string_view entity = text.substr(i + 1, end - i - 1);

                if (entity == "lt") {
                    result += '<';
                } else if (entity == "gt") {
                    result += '>';
                } else if (entity == "amp") {
                    result += '&';
                } else if (entity == "quot") {
                    result += '"';
                } else if (entity == "apos") {
                    result += '\'';
                } else if (!entity.empty() && entity[0] == '#') {
                    // Numeric entity
                    int codepoint = 0;
                    if (entity.size() > 1 && (entity[1] == 'x' || entity[1] == 'X')) {
                        // Hex
                        for (size_t j = 2; j < entity.size(); ++j) {
                            char c = entity[j];
                            codepoint *= 16;
                            if (c >= '0' && c <= '9')
                                codepoint += c - '0';
                            else if (c >= 'a' && c <= 'f')
                                codepoint += c - 'a' + 10;
                            else if (c >= 'A' && c <= 'F')
                                codepoint += c - 'A' + 10;
                        }
                    } else {
                        // Decimal
                        for (size_t j = 1; j < entity.size(); ++j) {
                            codepoint = codepoint * 10 + (entity[j] - '0');
                        }
                    }

                    // Encode as UTF-8
                    if (codepoint < 0x80) {
                        result += static_cast<char>(codepoint);
                    } else if (codepoint < 0x800) {
                        result += static_cast<char>(0xC0 | (codepoint >> 6));
                        result += static_cast<char>(0x80 | (codepoint & 0x3F));
                    } else if (codepoint < 0x10000) {
                        result += static_cast<char>(0xE0 | (codepoint >> 12));
                        result += static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F));
                        result += static_cast<char>(0x80 | (codepoint & 0x3F));
                    } else {
                        result += static_cast<char>(0xF0 | (codepoint >> 18));
                        result += static_cast<char>(0x80 | ((codepoint >> 12) & 0x3F));
                        result += static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F));
                        result += static_cast<char>(0x80 | (codepoint & 0x3F));
                    }
                } else {
                    // Unknown entity, keep as-is
                    result += '&';
                    result += entity;
                    result += ';';
                }

                i = end + 1;
                continue;
            }
        }

        result += text[i];
        ++i;
    }

    return result;
}

void XmlReader::Impl::skip_until(char c) {
    while (true) {
        char ch = get_char();
        if (ch == '\0' || ch == c)
            break;
    }
}

void XmlReader::Impl::skip_until(std::string_view s) {
    if (s.empty())
        return;

    std::string match_buffer;
    while (true) {
        char c = get_char();
        if (c == '\0')
            break;

        match_buffer += c;
        if (match_buffer.size() > s.size()) {
            match_buffer.erase(0, 1);
        }

        if (match_buffer == s)
            break;
    }
}

bool XmlReader::Impl::match(std::string_view s) {
    for (char expected: s) {
        if (get_char() != expected)
            return false;
    }
    return true;
}

XmlReader::XmlReader(const std::string &path) : impl_(std::make_unique<Impl>()) {
    // Check if file is BZ2
    std::ifstream test_file(path, std::ios::binary);
    if (!test_file) {
        impl_->error_message = "Failed to open file: " + path;
        impl_->at_eof = true;
        return;
    }

    char header[2];
    test_file.read(header, 2);
    test_file.close();

    if (header[0] == 'B' && header[1] == 'Z') {
        impl_->bz2_stream = std::make_unique<Bz2Stream>(path);
        if (!impl_->bz2_stream->is_open()) {
            impl_->error_message = "Failed to open BZ2 file";
            impl_->at_eof = true;
        }
    } else {
        impl_->plain_stream = std::make_unique<std::ifstream>(path, std::ios::binary);
        if (!*impl_->plain_stream) {
            impl_->error_message = "Failed to open file";
            impl_->at_eof = true;
        }
    }
}

XmlReader::XmlReader(std::unique_ptr<Bz2Stream> stream) : impl_(std::make_unique<Impl>()) {
    impl_->bz2_stream = std::move(stream);
    if (!impl_->bz2_stream || !impl_->bz2_stream->is_open()) {
        impl_->error_message = "Invalid BZ2 stream";
        impl_->at_eof = true;
    }
}

XmlReader XmlReader::from_string(std::string_view xml) {
    // Create with dummy path, then replace impl
    XmlReader reader("");
    reader.impl_ = std::make_unique<Impl>();
    reader.impl_->buffer = std::string(xml);
    reader.impl_->buffer_pos = 0;
    reader.impl_->at_eof = false;
    reader.impl_->error_message.clear();
    return reader;
}

XmlReader::~XmlReader() = default;

XmlReader::XmlReader(XmlReader &&) noexcept = default;
XmlReader &XmlReader::operator=(XmlReader &&) noexcept = default;

std::optional<XmlEvent> XmlReader::next() {
    if (!impl_ || impl_->at_eof || impl_->document_ended) {
        return std::nullopt;
    }

    // Start document event
    if (!impl_->document_started) {
        impl_->document_started = true;

        // Skip BOM if present
        if (impl_->peek_char() == '\xEF') {
            impl_->get_char();
            if (impl_->peek_char() == '\xBB') {
                impl_->get_char();
                if (impl_->peek_char() == '\xBF') {
                    impl_->get_char();
                }
            }
        }

        // Skip XML declaration if present
        impl_->skip_whitespace();
        if (impl_->peek_char() == '<') {
            // Check for <?xml
            size_t saved_pos = impl_->buffer_pos;
            impl_->get_char(); // <
            if (impl_->peek_char() == '?') {
                impl_->skip_until("?>");
            } else {
                impl_->buffer_pos = saved_pos;
            }
        }

        return XmlEvent{XmlEventType::StartDocument, {}, {}, {}};
    }

    impl_->skip_whitespace();

    if (impl_->peek_char() == '\0') {
        if (!impl_->document_ended) {
            impl_->document_ended = true;
            return XmlEvent{XmlEventType::EndDocument, {}, {}, {}};
        }
        return std::nullopt;
    }

    if (impl_->peek_char() == '<') {
        impl_->get_char(); // consume <

        // Comment
        if (impl_->peek_char() == '!') {
            impl_->get_char();
            if (impl_->peek_char() == '-') {
                impl_->get_char();
                if (impl_->peek_char() == '-') {
                    impl_->get_char();
                    // Read comment content
                    impl_->current_text.clear();
                    while (true) {
                        char c = impl_->get_char();
                        if (c == '\0')
                            break;
                        if (c == '-' && impl_->peek_char() == '-') {
                            impl_->get_char();
                            if (impl_->peek_char() == '>') {
                                impl_->get_char();
                                break;
                            }
                            impl_->current_text += '-';
                            impl_->current_text += '-';
                        } else {
                            impl_->current_text += c;
                        }
                    }
                    return XmlEvent{XmlEventType::Comment, {}, impl_->current_text, {}};
                }
            }
            // Skip DOCTYPE or CDATA
            if (impl_->peek_char() == '[') {
                // CDATA
                impl_->skip_until("]]>");
                return next();
            }
            impl_->skip_until('>');
            return next();
        }

        // End element
        if (impl_->peek_char() == '/') {
            impl_->get_char();
            impl_->current_element_name = impl_->read_name();
            impl_->skip_until('>');

            if (!impl_->element_stack.empty()) {
                impl_->element_stack.pop();
            }

            return XmlEvent{XmlEventType::EndElement, impl_->current_element_name, {}, {}};
        }

        // Processing instruction
        if (impl_->peek_char() == '?') {
            impl_->skip_until("?>");
            return next();
        }

        // Start element
        impl_->current_element_name = impl_->read_name();
        impl_->current_attributes.clear();

        // Read attributes
        while (true) {
            impl_->skip_whitespace();
            char c = impl_->peek_char();

            if (c == '/' || c == '>' || c == '\0')
                break;

            std::string attr_name = impl_->read_name();
            if (attr_name.empty())
                break;

            impl_->skip_whitespace();
            if (impl_->peek_char() == '=') {
                impl_->get_char();
                impl_->skip_whitespace();
                std::string attr_value = impl_->read_quoted_string();
                impl_->current_attributes.emplace_back(std::move(attr_name), std::move(attr_value));
            } else {
                impl_->current_attributes.emplace_back(std::move(attr_name), "");
            }
        }

        bool self_closing = false;
        if (impl_->peek_char() == '/') {
            impl_->get_char();
            self_closing = true;
        }

        if (impl_->peek_char() == '>') {
            impl_->get_char();
        }

        // Build event
        XmlEvent event;
        event.type = XmlEventType::StartElement;
        event.name = impl_->current_element_name;
        event.attributes.reserve(impl_->current_attributes.size());
        for (const auto &[name, value]: impl_->current_attributes) {
            event.attributes.push_back({name, value});
        }

        if (!self_closing) {
            impl_->element_stack.push(impl_->current_element_name);
        } else {
            // For self-closing, we'll need to return EndElement next
            // For simplicity, just push and will pop on next call
        }

        return event;
    }

    // Text content
    impl_->current_text.clear();
    while (true) {
        char c = impl_->peek_char();
        if (c == '\0' || c == '<')
            break;
        impl_->current_text += impl_->get_char();
    }

    if (!impl_->current_text.empty()) {
        std::string decoded = impl_->decode_entities(impl_->current_text);
        impl_->current_text = std::move(decoded);
        return XmlEvent{XmlEventType::Text, {}, impl_->current_text, {}};
    }

    return next();
}

void XmlReader::skip_element() {
    if (!impl_)
        return;

    int start_depth = static_cast<int>(impl_->element_stack.size());
    if (start_depth == 0)
        return;

    while (true) {
        auto event = next();
        if (!event)
            break;

        if (event->type == XmlEventType::EndElement) {
            if (static_cast<int>(impl_->element_stack.size()) < start_depth) {
                break;
            }
        }
    }
}

std::string XmlReader::read_text() {
    if (!impl_)
        return "";

    std::string text;
    int start_depth = static_cast<int>(impl_->element_stack.size());

    while (true) {
        auto event = next();
        if (!event)
            break;

        if (event->type == XmlEventType::Text) {
            text += event->text;
        } else if (event->type == XmlEventType::EndElement) {
            if (static_cast<int>(impl_->element_stack.size()) < start_depth) {
                break;
            }
        }
    }

    return text;
}

bool XmlReader::eof() const noexcept {
    return !impl_ || impl_->at_eof || impl_->document_ended;
}

int XmlReader::depth() const noexcept {
    return impl_ ? static_cast<int>(impl_->element_stack.size()) : 0;
}

std::string XmlReader::current_path() const {
    if (!impl_)
        return "";

    std::string path;
    std::stack<std::string> temp = impl_->element_stack;
    std::vector<std::string> elements;

    while (!temp.empty()) {
        elements.push_back(temp.top());
        temp.pop();
    }

    for (auto it = elements.rbegin(); it != elements.rend(); ++it) {
        if (!path.empty())
            path += '/';
        path += *it;
    }

    return path;
}

uint64_t XmlReader::bytes_processed() const noexcept {
    return impl_ ? impl_->bytes_processed : 0;
}

std::string_view XmlReader::error() const noexcept {
    return impl_ ? impl_->error_message : "";
}

// ============================================================================
// XmlElementIterator implementation
// ============================================================================

std::optional<std::string_view> XmlElementIterator::Element::attribute(std::string_view attr_name) const {
    for (const auto &attr: attributes) {
        if (attr.name == attr_name) {
            return attr.value;
        }
    }
    return std::nullopt;
}

XmlElementIterator::XmlElementIterator(XmlReader &reader, std::string path) : reader_(reader), path_(std::move(path)) {
    // Split path into parts
    size_t start = 0;
    while (start < path_.size()) {
        size_t end = path_.find('/', start);
        if (end == std::string::npos) {
            path_parts_.push_back(path_.substr(start));
            break;
        }
        if (end > start) {
            path_parts_.push_back(path_.substr(start, end - start));
        }
        start = end + 1;
    }
}

std::optional<XmlElementIterator::Element> XmlElementIterator::next() {
    while (true) {
        auto event = reader_.next();
        if (!event) {
            return std::nullopt;
        }

        if (event->type == XmlEventType::StartElement) {
            // Check if current path matches
            std::string current = reader_.current_path();

            bool matches = false;
            if (path_parts_.empty()) {
                matches = true;
            } else if (path_parts_.size() == 1) {
                matches = (event->name == path_parts_[0]);
            } else {
                // Check full path match
                matches = (current == path_);
            }

            if (matches) {
                Element elem;
                elem.name = event->name;
                elem.attributes = event->attributes;
                elem.text_content = reader_.read_text();
                return elem;
            }
        }
    }
}

// Iterator implementation
XmlElementIterator::Iterator::Iterator(XmlElementIterator *iter) : iter_(iter), at_end_(false) {
    ++(*this); // Load first element
}

XmlElementIterator::Iterator &XmlElementIterator::Iterator::operator++() {
    if (iter_) {
        auto elem = iter_->next();
        if (elem) {
            current_ = std::move(*elem);
        } else {
            at_end_ = true;
            iter_ = nullptr;
        }
    }
    return *this;
}

bool XmlElementIterator::Iterator::operator==(const Iterator &other) const {
    if (at_end_ && other.at_end_)
        return true;
    if (at_end_ != other.at_end_)
        return false;
    return iter_ == other.iter_;
}

} // namespace wikilib::dump
