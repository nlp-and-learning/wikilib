# Dump processing benchmark

These measurements cover stage 5's shared streaming parser and batch extraction.
The fixtures are deterministic, synthetic Wikimedia-shaped multistream dumps,
with 4 KiB of wikitext per page and a plain index. They contain headings, links,
and templates; they do not represent all languages, revision histories, or
compression ratios in real Wikimedia dumps. See the subsequent
[bounded Polish Wikipedia benchmark](PLWIKI-20260101.md) for real-file results.

## Reproduce

Run from the repository root on Linux. Benchmark generation writes dump/index
files in the chosen fixture directories, so use dedicated temporary directories.

```sh
cmake -S . -B /tmp/wikilib-benchmark-build \
  -DCMAKE_BUILD_TYPE=Release -DWIKILIB_USE_SYSTEM_DEPS=ON \
  -DWIKILIB_BUILD_TESTS=OFF -DWIKILIB_BUILD_EXAMPLES=OFF \
  -DWIKILIB_BUILD_BENCHMARKS=ON
cmake --build /tmp/wikilib-benchmark-build -j4
mkdir -p /tmp/wikilib-bench-multi /tmp/wikilib-bench-small /tmp/wikilib-bench-large
/tmp/wikilib-benchmark-build/bin/benchmark_dump /tmp/wikilib-bench-multi generate 4096 512
/tmp/wikilib-benchmark-build/bin/benchmark_dump /tmp/wikilib-bench-small generate 4096 4096
/tmp/wikilib-benchmark-build/bin/benchmark_dump /tmp/wikilib-bench-large generate 32768 32768
bash benchmarks/run.sh /tmp/wikilib-benchmark-build/bin/benchmark_dump /tmp/wikilib-bench
```

For the stage 4 batch baseline, compile the previous DumpReader implementation
against the shared, unchanged lower-level readers and the same benchmark:

```sh
git show 0384f1c:src/dump/dump_reader.cpp > /tmp/wikilib-stage4-dump-reader.cpp
c++ -O3 -DNDEBUG -std=c++23 -I include benchmarks/dump_benchmark.cpp \
  /tmp/wikilib-stage4-dump-reader.cpp \
  /tmp/wikilib-benchmark-build/lib/libwikilib.a \
  -lbz2 -lpugixml -licuuc -licui18n -o /tmp/wikilib-stage4-benchmark
bash benchmarks/run.sh /tmp/wikilib-benchmark-build/bin/benchmark_dump \
  /tmp/wikilib-bench /tmp/wikilib-stage4-benchmark
```

`sequential` uses `process_all()`, `indexed` uses `process_indexed()`, and
`buffered` loads the index, decompresses whole XML chunks, then parses them with
`extract_all_from_xml()`. `batch` loads the index and requests the first 128
pages in reverse order, all in the first chunk. The baseline decompresses that
chunk once but reparses it separately for each requested title.

Every mode hashes titles and decoded content to verify equivalent results.
Generation happens in separate processes. Each measurement runs in a fresh
process; elapsed time includes index reading/loading where applicable, parsing,
result handling, and hashing. Throughput counts decoded page content bytes,
excluding XML markup. Peak RSS is Linux `getrusage(RUSAGE_SELF).ru_maxrss` and
includes process/runtime, libbz2 state, indexes, and result storage. Runs use the
local filesystem cache; these are not cold-storage measurements.

## Results: 2026-10-06

Environment: Linux x86-64, AMD Ryzen 9 9900X, GCC 15.2, CMake Release
(`-O3 -DNDEBUG`), libbz2 1.0.8. Each row gives the median time/throughput and
maximum peak RSS across three runs. Raw measurements are in
[results-2026-10-06.txt](results-2026-10-06.txt).

| Fixture | Operation | Time (s) | Content MiB/s | Peak RSS (MiB) |
| --- | --- | ---: | ---: | ---: |
| 4,096 pages, 8 chunks | Sequential, stage 5 | 0.244 | 65.60 | 8.04 |
| 4,096 pages, 8 chunks | Indexed, stage 5 | 0.226 | 70.70 | 8.24 |
| 4,096 pages, 8 chunks | Batch of 128, stage 5 | 0.0312 | 16.03 | 8.98 |
| 4,096 pages, 8 chunks | Batch of 128, stage 4 | 0.1955 | 2.56 | 16.16 |
| 4,096 pages, 1 chunk | Indexed, stage 5 | 0.233 | 68.74 | 8.42 |
| 4,096 pages, 1 chunk | Buffered, stage 5 | 0.224 | 71.32 | 56.71 |
| 32,768 pages, 1 chunk | Indexed, stage 5 | 1.834 | 69.81 | 9.77 |
| 32,768 pages, 1 chunk | Buffered, stage 5 | 1.782 | 71.83 | 422.66 |

The smaller dump contains 17,160,061 XML bytes (16.37 MiB); the larger contains
137,341,279 XML bytes (130.98 MiB). The multi-chunk dump compresses to 628,064
bytes, the smaller single-chunk dump to 622,724 bytes, and the larger to 4,971,973
bytes.

All full-dump modes have matching page counts and checksums per fixture; stage 4
and stage 5 batch results also match. Batch extraction was about 6.3 times faster
in this fixture. Streaming RSS stayed below 10 MiB as a single chunk grew eight
fold, while buffered processing grew from about 57 to 423 MiB. Streaming still
retains the current chunk's index entries, so RSS can grow with their count and
with individual page sizes. Buffered parsing was slightly faster here; the
streaming benefit is bounded XML memory and consistent page handling. Real-dump
measurements are recorded separately; neither sample establishes universal throughput.
