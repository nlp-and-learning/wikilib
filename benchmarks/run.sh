#!/usr/bin/env bash
set -eu
# Fixtures must already have been generated as described in README.md.
if [ "$#" -lt 2 ]; then
    printf 'Usage: bash run.sh BENCHMARK FIXTURE_PREFIX [STAGE4_BENCHMARK]\n' >&2
    exit 1
fi
benchmark=$1
fixtures=$2
run_case() {
    local dataset=$1 binary=$2 implementation=$3 mode=$4 result run
    for run in 1 2 3; do
        result=$("$binary" "$fixtures-$dataset" "$mode")
        printf 'dataset=%s implementation=%s run=%s %s\n' "$dataset" "$implementation" "$run" "$result"
    done
}
run_case multi "$benchmark" stage5 sequential
run_case multi "$benchmark" stage5 indexed
run_case multi "$benchmark" stage5 batch
if [ "$#" -gt 2 ]; then run_case multi "$3" stage4 batch; fi
run_case small "$benchmark" stage5 indexed
run_case small "$benchmark" stage5 buffered
run_case large "$benchmark" stage5 indexed
run_case large "$benchmark" stage5 buffered
