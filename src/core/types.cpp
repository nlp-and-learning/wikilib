#include "wikilib/core/types.h"

namespace wikilib {

std::string PageInfo::full_title() const {
    // Dump titles already include their localized namespace prefix.
    return title;
}

} // namespace wikilib
