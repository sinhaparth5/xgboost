#!/usr/bin/env bash
# End-to-end training time with RandomEngine set two ways.
#
# The only difference between the two builds is the typedef in context.h, so
# anything that shows up in the timing is the engine and not the surroundings.
set -euo pipefail
OUT="${OUT:-$HOME/train}"; mkdir -p "$OUT"
JOBS="${JOBS:-$(nproc)}"
cd "$HOME/xgboost"

for variant in philox mt19937; do
    if [ "$variant" = mt19937 ]; then
        sed -i 's|^using RandomEngine = Philox4x32;|using RandomEngine = std::mt19937;|' include/xgboost/context.h
        grep -q '#include <random>' include/xgboost/context.h || \
            sed -i 's|#include <cstdint>      // for int16_t, int32_t, int64_t|#include <cstdint>      // for int16_t, int32_t, int64_t\n#include <random>       // for mt19937|' include/xgboost/context.h
    else
        sed -i 's|^using RandomEngine = std::mt19937;|using RandomEngine = Philox4x32;|' include/xgboost/context.h
    fi
    echo "== $variant: $(grep -n '^using RandomEngine' include/xgboost/context.h)"

    rm -rf build && mkdir -p build
    cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DBUILD_STATIC_LIB=OFF \
          -DUSE_OPENMP=ON -DBUILD_DEPRECATED_CLI=OFF > "$OUT/$variant-cmake.log" 2>&1
    cmake --build build -j"$JOBS" --target xgboost > "$OUT/$variant-build.log" 2>&1

    g++ -std=c++20 -O2 -I include -I dmlc-core/include \
        ci_probe/bench_train.cpp -o "bench_train_$variant" \
        -L build -lxgboost -Wl,-rpath,"$PWD/build"

    # rows features rounds reps threads
    "./bench_train_$variant" 20000 128 100 9 4 > "$OUT/$variant.json"
    echo "$variant: $(cat "$OUT/$variant.json")"
done

# leave the tree on the engine it is proposed with
sed -i 's|^using RandomEngine = std::mt19937;|using RandomEngine = Philox4x32;|' include/xgboost/context.h

{
    date -Iseconds; uname -a
    echo "git-sha: $(git rev-parse HEAD)"
    g++ --version | head -1
    cmake --version | head -1
    lscpu
} > "$OUT/environment.txt"
echo "TRAIN RUNS COMPLETE"
