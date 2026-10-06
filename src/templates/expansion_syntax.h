#pragma once
#include <cctype>
#include <string_view>
#include <vector>

namespace wikilib::templates::detail {
inline std::string_view trim(std::string_view text) {
    const auto start = text.find_first_not_of(" \t\r\n");
    return start == text.npos ? std::string_view{} : text.substr(start, text.find_last_not_of(" \t\r\n") - start + 1);
}

// Comments and lowercase nowiki sections are opaque to expansion and splitting.
inline size_t opaque_end(std::string_view text, size_t pos) {
    if (text.substr(pos).starts_with("<!--")) {
        const auto end = text.find("-->", pos + 4);
        return end == text.npos ? text.size() : end + 3;
    }
    if (text.substr(pos).starts_with("<nowiki") && pos + 7 < text.size() &&
        (text[pos + 7] == '>' || text[pos + 7] == '/' || std::isspace(static_cast<unsigned char>(text[pos + 7])))) {
        const auto opening = text.find('>', pos + 7);
        if (opening == text.npos)
            return text.size();
        if (opening > pos && text[opening - 1] == '/')
            return opening + 1;
        const auto end = text.find("</nowiki>", opening + 1);
        return end == text.npos ? text.size() : end + 9;
    }
    return pos;
}

inline size_t brace_end(std::string_view text, size_t pos) {
    std::vector<size_t> widths;
    for (size_t i = pos; i < text.size();) {
        if (auto end = opaque_end(text, i); end != i) {
            i = end;
            continue;
        }
        if (text.substr(i).starts_with("{{")) {
            const size_t width = text.substr(i).starts_with("{{{") ? 3 : 2;
            widths.push_back(width);
            i += width;
        } else if (!widths.empty() && text.substr(i).starts_with(std::string_view("}}}", widths.back()))) {
            i += widths.back();
            widths.pop_back();
            if (widths.empty())
                return i;
        } else
            ++i;
    }
    return text.npos;
}

inline std::vector<std::string_view> split(std::string_view text, char separator) {
    std::vector<std::string_view> parts;
    size_t start = 0;
    int links = 0;
    for (size_t i = 0; i < text.size();) {
        if (auto end = opaque_end(text, i); end != i) {
            i = end;
            continue;
        }
        if (text.substr(i).starts_with("{{")) {
            const auto end = brace_end(text, i);
            if (end == text.npos)
                break;
            i = end;
        } else if (text.substr(i).starts_with("[[")) {
            ++links;
            i += 2;
        } else if (links && text.substr(i).starts_with("]]")) {
            --links;
            i += 2;
        } else if (text[i] == separator && links == 0) {
            parts.push_back(text.substr(start, i - start));
            start = ++i;
        } else
            ++i;
    }
    parts.push_back(text.substr(start));
    return parts;
}
} // namespace wikilib::templates::detail
