# wikilib

A C++23 library for reading Wikimedia dumps and parsing a subset of MediaWiki
wikitext. It is a library for data processing, with partial MediaWiki behavior.

- Wikitext tokenizer and AST for formatting, links, templates, headings, lists,
  tables, and HTML tags; visitors and a separate builder for heading section trees.
- Sequential XML/page reading and incremental BZ2 multistream decompression.
- TXT/TXT.BZ2 index reading, indexed streaming, and single/batch page extraction.
- Template expansion with parameters/defaults, selected parser functions,
  page context, numeric expressions, and explicit limits.
- JSON/JSONL serialization, plain-text output, and ICU Unicode utilities.

The wikitext and XML parsers support the library's documented subset rather than
complete MediaWiki rendering or general XML validation. See
[template expansion support](docs/TEMPLATE_EXPANSION.md) for exact behavior,
limits, AST replacement semantics, and unsupported features. Lua and `#invoke`
remain a separate follow-up.

## Build and install

Requires a C++23 compiler, CMake 3.20 or newer, BZip2, pugixml, ICU (`uc`, `i18n`),
and nlohmann_json installed with CMake package metadata. GoogleTest is needed
only when tests are enabled. Dependencies are currently found locally with
`find_package()`. The CMake minimum matches the version that introduced
[C++23 mode and export support](https://cmake.org/cmake/help/v3.20/release/3.20.html).

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j4
ctest --test-dir build --output-on-failure
cmake --install build --prefix /path/to/install
```

Tests and examples default to enabled; use `-DWIKILIB_BUILD_TESTS=OFF` and
`-DWIKILIB_BUILD_EXAMPLES=OFF` for a library-only build. Optional local benchmarks
use `-DWIKILIB_BUILD_BENCHMARKS=ON`. Include `<wikilib.hpp>` or individual headers.

An external CMake project can consume the installed package:

```cmake
find_package(wikilib CONFIG REQUIRED)
target_link_libraries(my_program PRIVATE wikilib::wikilib)
```

Configure the consumer with `-DCMAKE_PREFIX_PATH=/path/to/install`. The exported
target supplies C++23 and dependency requirements. `InstalledPackageConsumer`
tests installation, compiling and running a separate project against installed
headers and library.

## Template expansion

```cpp
#include <wikilib.hpp>

using namespace wikilib;
auto provider = std::make_shared<templates::MemoryTemplateProvider>();
provider->add_template("Greeting", "Hello {{{1|world}}}, {{PAGENAME}}!");
templates::TemplateExpander expander(provider);
PageInfo page;
page.title = "Example";
auto result = expander.expand("{{Greeting|reader}} {{#expr:2+3*4}}", page);
// On success: "Hello reader, Example! 14". Inspect result.error() on failure.
```

`expand_ast(document, page)` reparses expanded wikitext into an AST and replaces
its children on success. Expansion and reported parse failures leave the
original document unchanged. Parsing alone does not expand templates.

## Indexed streaming

`DumpReader::process_indexed()` reads a TXT.BZ2 index (or TXT when the compressed
index is absent) one chunk at a time. Each compressed range is decompressed into
fixed-size buffers and parsed into pages; it does not require `load_index()`.
The callback receives page metadata and the last revision in dump order.

```cpp
#include <iostream>
#include <wikilib/dump/dump_reader.h>

using namespace wikilib::dump;

DumpPath path("/path/to/dumps");
path.set_project(WikiProject::Wikipedia)
    .set_language("pl")
    .set_date("20260101");
DumpReader reader(path);
bool completed = reader.process_indexed([](const Page& page) {
    std::cout << page.info.title << '\t' << page.content().size() << '\n';
    return true; // Return false to stop processing.
});
if (!completed && !reader.error().empty()) {
    std::cerr << reader.error() << '\n';
}
```

Use `process_chunk(chunk, callback)` to process one `IndexChunk` without loading
the index. Nonempty `chunk.entries` are checked against XML page titles, IDs,
and order. Both APIs return `true` on completion and `false` on cancellation or
failure; cancellation leaves `error()` empty. Pages already delivered before a
later failure remain delivered. Callback page references are valid only during
the call; copy them if needed afterward.

An optional third argument reports `ProcessProgress`: pages, completed chunks,
and compressed bytes fetched, including read-ahead. Byte totals cover selected
ranges and exclude any unindexed header before the first range. Progress is
reported at the start, periodically, and at completion, cancellation, or input
failure after processing starts. Callback exceptions derived from
`std::exception` are reported through `error()`.

Working memory includes decompression/parser buffers, the current page and XML
metadata, and the current chunk's index entries. A large page still needs memory
for its text. `decompress_chunk()` returns the entire XML string.
`extract_page()`/`extract_pages()` now parse selected chunks incrementally, once
per chunk in offset order, and retain requested results. Results preserve request
order and duplicates; a failing chunk publishes no extracted pages. All page
processing and extraction APIs select the last revision in dump order.

`process_all()` uses the same XML/page parser to read a complete dump without an
index. Its existing title/content callback and void return type are preserved;
inspect `error()` afterward. Cancellation leaves the error empty. Sequential
progress leaves `chunks_processed` at zero because it does not consult the
index.

See [the benchmark report](benchmarks/README.md) for reproducible local timing,
throughput, and memory measurements.
