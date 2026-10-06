"""Check real-dump benchmark agreement without reading the dump again."""
import json
import sys

with open(sys.argv[1], encoding="utf-8") as source:
    rows = [json.loads(line) for line in source]
assert len(rows) == 54, f"Expected 54 measurements, got {len(rows)}"
for repeat in (1, 2, 3):
    results = [row["result"] for row in rows if row["repeat"] == repeat]
    def get(mode, count=None, spread=None):
        matches = [r for r in results if r["mode"] == mode
                   and (count is None or r["request_count"] == count)
                   and (spread is None or r["spread"] == spread)]
        assert len(matches) == 1
        return matches[0]

    for field in ("pages", "content_bytes", "checksum"):
        assert get("stream")[field] == get("buffered")[field], field
    for field in ("index_pages", "index_chunks"):
        assert get("index-stream")[field] == get("index-load")[field], field
    assert get("cancel")["cancelled"] and get("cancel")["pages"] == 10
    assert get("stream")["completed_chunks"] == 30
    assert get("parse")["pages"] == 30
    for spread in (False, True):
        for count in (1, 10, 100):
            batch, single = get("batch", count, spread), get("single", count, spread)
            assert batch["pages"] == single["pages"] == count
            for field in ("content_bytes", "checksum"):
                assert batch[field] == single[field], (repeat, count, spread, field)
            assert single["planned_compressed_bytes"] <= 128 * 1024 * 1024
print("All 54 measurements agree; cancellation and compressed-read budgets verified.")
