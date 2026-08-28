# Measurements answering the review comment on dmlc/xgboost#12485

The review that closed #12485 gave "the performance here is 1/10 of mt ...
due to no specialisation for wide multiplies" as the reason. These files are
the attempt to reproduce that, and to measure what the figure means for
XGBoost specifically.

## Host

Sapphire Rapids (Xeon Platinum 8481C @ 2.70GHz), GCP c3-standard-4, dedicated
vCPU, pinned to CPU 1, 15 repetitions per case. Median CV 0.07-0.29%, which is
the bar these numbers are quoted at. The governor is unavailable because a GCP
guest does not expose cpufreq; see `environment.txt` for the full capture.

Cloud rather than bare metal, so treat these as ratios measured within one
host, which is how they are used. Absolute cycles do not transfer.

## Files

`{g,clang}-{O0,O2,O3}.json` are Google Benchmark output for
`ci_probe/bench_rng.cpp`; the `.txt` beside each is the console form.

## What they show

1. "1/10 of mt" reproduces at -O0 and only there: Philox lands at 0.04-0.10x
   mt19937 unoptimised, against 0.59-0.77x at -O2/-O3. CMake emits no
   optimisation flags when CMAKE_BUILD_TYPE is unset, which is the likeliest
   honest route to that measurement.

2. Optimised, Philox really is slower than mt19937 in steady state, by roughly
   a quarter to a third. That is the honest number and it is not 10x.

3. The single-product MulHiLo form is worth 1.22x on clang and nothing on GCC,
   which is exactly what the codegen probe predicted from multiply counts
   (clang 40 -> 20, GCC 20 -> 20, MSVC 24 -> 13).

4. Steady state is not how XGBoost uses the engine. ColumnSampler::ColSample
   constructs a fresh RandomEngine per call, so mt19937 pays a ~1.6us state
   init to serve as few as 16 draws. Philox is 27x ahead at 16 features, 7.4x
   at 64, 2.6x at 256, and does not fall behind until past ~2000.
