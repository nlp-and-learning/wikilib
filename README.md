# wikilib
C++ library for parsing MediaWiki markup (Wikipedia, Wiktionary, etc.)
                                                                                                                                                                
Components:                                                                                                                                                     
- markup: Tokenizer, parser, and AST for wikitext syntax                                                                                                        
- dump: XML reader with bzip2 decompression for Wikipedia dumps                                                                                                 
- output: JSON and plain text serialization                                                                                                                     
- templates: Basic template parsing and expansion                                                                                                               
                                                                                                                                                                  
Features:                                                                                                                                                       
- Full wikitext tokenization (links, templates, formatting, tables, lists)                                                                                      
- AST with visitor pattern for traversal and transformation                                                                                                     
- Streaming XML dump processing with page handlers                                                                                                              
- Index file support for random access to compressed dumps                                                                                                      
- JSON/JSONL output formats                                                                                                                                     
- Plain text extraction with configurable options    

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
