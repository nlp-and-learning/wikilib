#pragma once

/**
 * @file wikilib.hpp
 * @brief Main header for wikilib - MediaWiki markup parsing library
 *
 * Include this header to get access to all wikilib functionality.
 * For selective includes, use individual headers from wikilib/ directory.
 */

#include "wikilib/core/string_pool.h"
#include "wikilib/core/text_utils.hpp" // Has templates
#include "wikilib/core/types.h"
#include "wikilib/core/unicode_utils.h"

#include "wikilib/markup/ast.h"
#include "wikilib/markup/parser.h"
#include "wikilib/markup/tokenizer.h"
#include "wikilib/markup/wikitext_visitor.h"

#include "wikilib/templates/template_expander.h"
#include "wikilib/templates/template_parser.h"

#include "wikilib/dump/bz2_stream.h"
#include "wikilib/dump/page_handler.h"
#include "wikilib/dump/xml_reader.h"

#include "wikilib/output/json_writer.h"
#include "wikilib/output/plain_text.h"

namespace wikilib {

/**
 * @brief Library version information
 */
struct Version {
    static constexpr int major = 0;
    static constexpr int minor = 1;
    static constexpr int patch = 0;

    static constexpr const char *string = "0.1.0";
};

} // namespace wikilib
