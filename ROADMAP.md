# wikilib improvement roadmap

The order of work is: reliable reading, indexed streaming, performance, then
broader wikitext support. Each stage should be a separate, reviewable change or
small series of changes. Update the checkboxes as work is completed.

## 1. Fix BZ2 reading — critical

- [x] Preserve libbz2's unused input when moving to the next concatenated stream.
- [x] Drain buffered data in `Bz2Stream::read_line()` after the underlying stream
  reaches EOF.
- [x] Distinguish clean EOF, truncated streams, and decompression errors.
- [x] Add tests for single and concatenated streams, a final line without a
  newline, buffer boundaries, and corrupted input.

**Acceptance criteria:** Every byte and line is returned exactly once. Corrupted
or truncated input produces an explicit error.

**Completed:** Added 19 BZ2 regression tests, including seeking after EOF or an
error. Also fixed the dangling `string_view` returned by `Bz2Stream::error()`.
The library and examples build; the full suite has 382 passing tests and 8
skipped tests. The BZ2 tests pass AddressSanitizer, UndefinedBehaviorSanitizer,
and LeakSanitizer checks.

## 2. Fix XML parsing and page reading

Depends on stage 1 for compressed-input integration tests.

- [x] Preserve the first `<page>` encountered while parsing the dump header.
- [x] Emit an `EndElement` event for self-closing XML elements.
- [x] Preserve whitespace inside `<text>` content.
- [x] Propagate decompression and XML errors to `PageHandler`.
- [x] Apply `PageFilter::only_latest_revision` rather than merely exposing the
  option.
- [x] Add tests for empty text, redirects, multiple revisions, and XML elements
  split across buffer boundaries.

**Acceptance criteria:** Pages, metadata, and text match the input, including the
first and last pages. Empty and self-closing elements do not disrupt parsing.

**Completed:** Added 23 XML and page-reading tests. Malformed or truncated XML
produces an Error event and incomplete pages are not returned. Numeric metadata
is validated, numeric entities are decoded without integer overflow, and CDATA
and text whitespace are preserved. The latest-revision filter retains only the
last revision in dump order; unfiltered `next_page()` retains all revisions.
Returned XML iterator elements now own their name and attribute storage, and
error views remain valid while the owning reader exists and the error is unchanged.
The library and examples build; the full suite has 405 passing tests and 8
skipped tests. All 23 new tests pass ASan, UBSan, and LSan checks.

## 3. Stabilize index reading and page extraction

- [x] Fix ownership of the `ifstream` used by `IndexChunker::from_file()` for
  uncompressed index files.
- [x] Validate offset ordering and ranges against the dump size.
- [x] Replace recursive skipping of malformed index lines with a loop, and make
  the malformed-line policy explicit.
- [x] Make repeated `DumpReader::load_index()` calls safe and leave a clear state
  after loading fails.
- [x] Distinguish a missing page from an existing page with empty content.
- [x] Return correct results for duplicate titles in `extract_pages()`.
- [x] Add tests for TXT and TXT.BZ2 indexes, invalid ranges, missing files,
  repeated loading, empty pages, and duplicate requests.

**Acceptance criteria:** Plain and compressed indexes behave consistently. An
invalid index is not reported as successfully loaded, and extraction results
preserve the requested order and duplicates.

**Completed:** Added 28 index and extraction tests. `IndexChunker` now owns its
plain-file stream, exposes read failures and skipped-line counts, and supports
explicit permissive or strict malformed-line handling. `IndexParser` and
`DumpReader::load_index()` use strict validation; failed reloads leave an empty,
unloaded state. DumpReader supports a plain TXT index when the BZ2 counterpart
is absent. Extraction handles empty pages, duplicate requests, and the partial
mediawiki root in the first or last indexed XML chunk. Invalid compressed ranges
are rejected before allocation. The library and examples build; the full suite
has 433 passing tests and 8 skipped tests. All 28 new tests pass ASan, UBSan,
and LSan checks.

## 4. Add streaming decompression of indexed chunks

Depends on stages 1–3.

Target pipeline:

```text
index chunk -> bounded BZ2 range -> decompression buffers -> XML parser
            -> page -> callback
```

- [x] Add a reader for a compressed range `[start_offset, end_offset)` that
  decompresses incrementally using fixed-size buffers.
- [x] Connect the range reader to `XmlReader` and `PageHandler`.
- [x] Add callback-based APIs, provisionally named `process_chunk(chunk,
  callback)` and `process_indexed(callback)`; settle signatures during
  implementation.
- [x] Iterate over index chunks without constructing a full in-memory page map.
- [x] Support callback cancellation, progress reporting, and explicit errors.
- [x] Keep `decompress_chunk()` as a convenience API returning complete content,
  and document its memory usage accurately.
- [x] Add integration tests for the first and last chunks, range boundaries,
  cancellation, and corrupted chunks.

**Acceptance criteria:** Pages reach the callback without accumulating the
entire decompressed chunk. Working memory depends primarily on decompression
buffers and the current page, rather than the whole chunk, index, or dump.

**Completed:** Added `Bz2RangeReader`, `XmlReader::from_chunk()`, and
`DumpReader::process_chunk()` / `process_indexed()` with page callbacks,
cancellation, progress, and explicit errors. The range reader uses fixed 64 KiB
input buffers, supports concatenated streams, and never reads beyond the selected
range. Indexed processing retains the last revision and validates each chunk's
page titles, IDs, and order against its index entries. It streams TXT or TXT.BZ2
indexes without building or changing the full page map. Working memory includes
the current page, XML metadata, buffers, and the current chunk's index entries;
it does not accumulate full chunk XML. `decompress_chunk()` now uses the same
range reader while retaining its whole-string result. The README includes usage
and memory/error semantics. Added 23 tests, including delivery before the whole
chunk is decompressed, first/middle/last boundaries, cancellation, corruption,
large page text, and periodic progress. The library and examples build; the full
suite has 456 passing tests and 8 skipped tests. All 23 new tests pass ASan,
UBSan, and LSan checks.

## 5. Unify processing paths and improve performance

Depends on stage 4.

- [ ] Implement `process_all()` through the XML parser instead of detecting
  `<page>` boundaries by line matching.
- [ ] Parse each chunk only once in `extract_pages()`.
- [ ] Process requested chunks in offset order to reduce file seeking while
  preserving the requested result order.
- [ ] Measure elapsed time, throughput, and peak memory on representative inputs.
- [ ] Compare sequential and indexed processing results in integration tests.

**Acceptance criteria:** Sequential and indexed processing return equivalent
pages. Measurements confirm bounded memory for streaming processing and avoid
repeated parsing of a chunk during batch extraction.

## 6. Complete template expansion and prepare a release

Schedule after reliable reading and indexed streaming are available.

- [ ] Implement `TemplateExpander::expand_ast()`.
- [ ] Enforce expansion limits consistently and make expansion caching account
  for page context.
- [ ] Implement expression parsing for `#expr` and `#ifexpr`.
- [ ] Define and document the supported subset of MediaWiki behavior.
- [ ] Treat Lua execution and `#invoke` support as a separate follow-up stage.
- [ ] Update the README with actual capabilities, limitations, and indexed
  streaming examples.
- [ ] Verify installation and consumption from an external CMake project.

**Acceptance criteria:** Implemented behavior is covered by tests, unsupported
behavior is documented, and an external project can build against the installed
library.

## Milestones

1. **Reliable reading:** stages 1–3 complete; no silent data loss in the covered
   BZ2, XML, and index scenarios.
2. **Indexed streaming:** stage 4 complete; indexed chunks can be processed with
   bounded working memory.
3. **Efficient processing:** stage 5 complete; processing paths agree and have
   recorded performance measurements.
4. **Broader functionality and release readiness:** stage 6 complete.

## Review baseline

The initial review built the library and examples and ran the existing suite:
363 tests passed and 8 were skipped. Additional small probes reproduced lost
lines at EOF, loss of a second concatenated BZ2 stream, omission of the first
page by `PageHandler`, and missing end events for self-closing XML elements.
These cases should become regression tests before their fixes are considered
complete.
