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

- [ ] Preserve the first `<page>` encountered while parsing the dump header.
- [ ] Emit an `EndElement` event for self-closing XML elements.
- [ ] Preserve whitespace inside `<text>` content.
- [ ] Propagate decompression and XML errors to `PageHandler`.
- [ ] Apply `PageFilter::only_latest_revision` rather than merely exposing the
  option.
- [ ] Add tests for empty text, redirects, multiple revisions, and XML elements
  split across buffer boundaries.

**Acceptance criteria:** Pages, metadata, and text match the input, including the
first and last pages. Empty and self-closing elements do not disrupt parsing.

## 3. Stabilize index reading and page extraction

- [ ] Fix ownership of the `ifstream` used by `IndexChunker::from_file()` for
  uncompressed index files.
- [ ] Validate offset ordering and ranges against the dump size.
- [ ] Replace recursive skipping of malformed index lines with a loop, and make
  the malformed-line policy explicit.
- [ ] Make repeated `DumpReader::load_index()` calls safe and leave a clear state
  after loading fails.
- [ ] Distinguish a missing page from an existing page with empty content.
- [ ] Return correct results for duplicate titles in `extract_pages()`.
- [ ] Add tests for TXT and TXT.BZ2 indexes, invalid ranges, missing files,
  repeated loading, empty pages, and duplicate requests.

**Acceptance criteria:** Plain and compressed indexes behave consistently. An
invalid index is not reported as successfully loaded, and extraction results
preserve the requested order and duplicates.

## 4. Add streaming decompression of indexed chunks

Depends on stages 1–3.

Target pipeline:

```text
index chunk -> bounded BZ2 range -> decompression buffers -> XML parser
            -> page -> callback
```

- [ ] Add a reader for a compressed range `[start_offset, end_offset)` that
  decompresses incrementally using fixed-size buffers.
- [ ] Connect the range reader to `XmlReader` and `PageHandler`.
- [ ] Add callback-based APIs, provisionally named `process_chunk(chunk,
  callback)` and `process_indexed(callback)`; settle signatures during
  implementation.
- [ ] Iterate over index chunks without constructing a full in-memory page map.
- [ ] Support callback cancellation, progress reporting, and explicit errors.
- [ ] Keep `decompress_chunk()` as a convenience API returning complete content,
  and document its memory usage accurately.
- [ ] Add integration tests for the first and last chunks, range boundaries,
  cancellation, and corrupted chunks.

**Acceptance criteria:** Pages reach the callback without accumulating the
entire decompressed chunk. Working memory depends primarily on decompression
buffers and the current page, rather than the whole chunk, index, or dump.

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
