// Reproduce and decompose the "1/10 of mt" measurement on dmlc/xgboost#12485.
//
// Three variants of the PR's PhiloxEngine:
//   px_asis  - as the PR left it: MulHi routes 32-bit through UMul64's 4-way
//              partial-product path, and MulLo recomputes the same product.
//   px_mulhi - MulHi specialised to one native multiply when 2*w <= 64.
//   px_hilo  - that, plus both halves taken from a single product.
//
// Two regimes, because they are not the same question:
//   Seq      - steady-state sequential draws, where "1/10" would be measured.
//   ColSample- construct a fresh engine from a seed then shuffle n features,
//              which is what src/common/random.cc actually does per call.

#include <benchmark/benchmark.h>

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <numeric>
#include <random>
#include <vector>

#include "xgboost/philox_engine.h"
#include "philox_hilo.h"
#include "philox_specialised.h"

namespace {

// [rand.predef]: the 10000th consecutive invocation of a default-constructed
// std::philox4x32 produces this. Any variant that misses it is a wrong change.
constexpr std::uint32_t kStdPhiloxNth = 1955073260;

template <typename Engine>
std::uint32_t NthDraw(int n) {
    Engine e;
    std::uint32_t v = 0;
    for (int i = 0; i < n; ++i) v = e();
    return v;
}

// All three variants must produce byte-identical streams. The fix is a speed
// change; if it moves the stream it is a different generator, not a fix.
bool CheckParity() {
    bool ok = true;
    if (NthDraw<xgboost::Philox4x32>(10000) != kStdPhiloxNth) {
        std::fprintf(stderr, "FAIL: px_asis misses [rand.predef]\n");
        ok = false;
    }
    xgboost::Philox4x32 a{12345};
    xgboost_spec::Philox4x32 b{12345};
    xgboost_hilo::Philox4x32 c{12345};
    for (int i = 0; i < 100000; ++i) {
        std::uint32_t va = a(), vb = b(), vc = c();
        if (va != vb || va != vc) {
            std::fprintf(stderr, "FAIL: stream diverges at draw %d\n", i);
            ok = false;
            break;
        }
    }
    return ok;
}

template <typename Engine>
void BM_Seq(benchmark::State& state) {
    Engine e{42};
    for (auto _ : state) {
        benchmark::DoNotOptimize(e());
    }
    state.SetItemsProcessed(state.iterations());
}

// Mirrors ColumnSampler::ColSample: one draw from the long-lived engine for a
// seed, a fresh engine built from it, then a shuffle over the feature set.
// The std::sort that follows is engine-independent, so it is left out.
template <typename Engine>
void BM_ColSample(benchmark::State& state) {
    auto const n = static_cast<std::size_t>(state.range(0));
    Engine global{7};
    std::vector<std::uint32_t> features(n);
    std::iota(features.begin(), features.end(), 0u);
    for (auto _ : state) {
        auto seed = global();
        Engine rng(seed);
        std::shuffle(features.begin(), features.end(), rng);
        benchmark::DoNotOptimize(features.data());
        benchmark::ClobberMemory();
    }
    state.SetItemsProcessed(state.iterations());
}

}  // namespace

BENCHMARK_TEMPLATE(BM_Seq, std::mt19937);
BENCHMARK_TEMPLATE(BM_Seq, xgboost::Philox4x32);
BENCHMARK_TEMPLATE(BM_Seq, xgboost_spec::Philox4x32);
BENCHMARK_TEMPLATE(BM_Seq, xgboost_hilo::Philox4x32);

#define COLSAMPLE_RANGE Arg(16)->Arg(64)->Arg(256)->Arg(1024)->Arg(4096)
BENCHMARK_TEMPLATE(BM_ColSample, std::mt19937)->COLSAMPLE_RANGE;
BENCHMARK_TEMPLATE(BM_ColSample, xgboost::Philox4x32)->COLSAMPLE_RANGE;
BENCHMARK_TEMPLATE(BM_ColSample, xgboost_spec::Philox4x32)->COLSAMPLE_RANGE;
BENCHMARK_TEMPLATE(BM_ColSample, xgboost_hilo::Philox4x32)->COLSAMPLE_RANGE;

int main(int argc, char** argv) {
    if (!CheckParity()) return 1;
    std::fprintf(stderr, "parity ok: all variants match [rand.predef] and each other\n");
    benchmark::Initialize(&argc, argv);
    benchmark::RunSpecifiedBenchmarks();
    benchmark::Shutdown();
    return 0;
}
