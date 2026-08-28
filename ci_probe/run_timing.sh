#!/usr/bin/env bash
# Timing counterpart to the codegen probe. Codegen is deterministic and was
# measured in CI; this needs a quiet, pinned host, so it is kept separate.
set -euo pipefail
CPU="${CPU:-1}"
OUT="${OUT:-$PWD/timing}"
mkdir -p "$OUT"

sudo cpupower frequency-set -g performance >/dev/null 2>&1 || echo "governor: unavailable" >&2

for cc in g++ clang++; do
    command -v "$cc" >/dev/null || { echo "skip $cc (absent)" >&2; continue; }
    for opt in O0 O2 O3; do
        bin="bench_${cc%%+*}_$opt"
        $cc -std=c++20 "-$opt" -I include -I ci_probe -I "$BENCH_INC" \
            ci_probe/bench_rng.cpp -o "$bin" "$BENCH_LIB" -lpthread
        taskset -c "$CPU" "./$bin" \
            --benchmark_min_time=0.5s \
            --benchmark_repetitions=15 \
            --benchmark_report_aggregates_only=true \
            --benchmark_enable_random_interleaving=true \
            --benchmark_out="$OUT/${cc%%+*}-$opt.json" \
            --benchmark_out_format=json > "$OUT/${cc%%+*}-$opt.txt" 2>&1
        echo "done $cc -$opt"
    done
done

{
    date -Iseconds
    uname -a
    echo "git-sha: $(git rev-parse HEAD)"
    echo "pinned-cpu: $CPU"
    echo "governor: $(cat /sys/devices/system/cpu/cpu$CPU/cpufreq/scaling_governor 2>/dev/null || echo unavailable)"
    g++ --version | head -1
    clang++ --version 2>/dev/null | head -1 || true
    lscpu
} > "$OUT/environment.txt"
