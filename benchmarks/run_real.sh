#!/usr/bin/env bash
set -euo pipefail
benchmark=${1:?benchmark executable required}
plan=${2:?prepared plan required}
output=${3:?results JSONL required}
"$benchmark" run "$plan" samples > "$output.samples.json"
: > "$output"
for repeat in 1 2 3; do
    for mode in index-stream index-load stream buffered cancel parse; do
        echo "repeat $repeat: $mode" >&2
        result=$(timeout 60 "$benchmark" run "$plan" "$mode" 10 2> "$output.stderr")
        printf '{"repeat":%s,"result":%s}\n' "$repeat" "$result" >> "$output"
    done
    for distribution in same spread; do
        for count in 1 10 100; do
            for mode in batch single; do
                echo "repeat $repeat: $mode $count $distribution" >&2
                result=$(timeout 60 "$benchmark" run "$plan" "$mode" "$count" "$distribution" 2> "$output.stderr")
                printf '{"repeat":%s,"result":%s}\n' "$repeat" "$result" >> "$output"
            done
        done
    done
done
python3 "$(dirname "$0")/verify_real.py" "$output"
