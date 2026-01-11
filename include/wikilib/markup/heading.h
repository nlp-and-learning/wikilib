#pragma once

/**
 * @file heading.h
 * @brief Heading parsing utilities for MediaWiki wikitext
 */

#include <string>
#include <string_view>

namespace wikilib::markup {

// ============================================================================
// Heading info
// ============================================================================

/**
 * @brief Information about a parsed heading
 */
struct HeadingInfo {
    int level = 0;           // Heading level (1-6)
    std::string title;       // Title text (without = markers)
    bool found = false;      // Whether a valid heading was found
};

// ============================================================================
// Heading parsing functions
// ============================================================================

/**
 * @brief Parse a heading from wikitext
 *
 * Parses the input as wikitext and extracts the first heading found.
 * Returns HeadingInfo with found=false if no heading is present.
 *
 * @param input The wikitext to parse
 * @return HeadingInfo with level, title, and found status
 */
[[nodiscard]] HeadingInfo parse_heading(std::string_view input);

} // namespace wikilib::markup