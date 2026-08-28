// End-to-end training time, mt19937 against Philox, through the C API.
//
// The engine is chosen at build time by the RandomEngine alias in context.h,
// so the two binaries differ in exactly one typedef and nothing else. Data is
// synthetic but fixed: the same bytes feed both builds.
//
// colsample_bynode is set below 1 on purpose. It is the parameter that makes
// ColumnSampler::ColSample run, which is the only place the host RNG is hot.

#include <xgboost/c_api.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <random>
#include <string>
#include <vector>

#define CHECK_CALL(x)                                                       \
    do {                                                                    \
        if ((x) != 0) {                                                     \
            std::fprintf(stderr, "xgboost error: %s\n", XGBGetLastError()); \
            return 1;                                                       \
        }                                                                   \
    } while (0)

int main(int argc, char** argv) {
    const int n_rows     = argc > 1 ? std::atoi(argv[1]) : 20000;
    const int n_features = argc > 2 ? std::atoi(argv[2]) : 128;
    const int n_rounds   = argc > 3 ? std::atoi(argv[3]) : 100;
    const int n_reps     = argc > 4 ? std::atoi(argv[4]) : 7;
    const int n_thread   = argc > 5 ? std::atoi(argv[5]) : 4;

    // Fixed generator, independent of anything under test.
    std::mt19937 gen{20260828};
    std::normal_distribution<float> nd{0.f, 1.f};
    std::vector<float> x(static_cast<size_t>(n_rows) * n_features);
    for (auto& v : x) v = nd(gen);
    std::vector<float> y(n_rows);
    for (int i = 0; i < n_rows; ++i) {
        double s = 0;
        for (int j = 0; j < 8; ++j) s += x[static_cast<size_t>(i) * n_features + j];
        y[i] = s > 0 ? 1.f : 0.f;
    }

    std::vector<double> times;
    for (int rep = 0; rep < n_reps; ++rep) {
        DMatrixHandle dtrain;
        CHECK_CALL(XGDMatrixCreateFromMat(x.data(), n_rows, n_features, NAN, &dtrain));
        CHECK_CALL(XGDMatrixSetFloatInfo(dtrain, "label", y.data(), n_rows));

        BoosterHandle booster;
        CHECK_CALL(XGBoosterCreate(&dtrain, 1, &booster));
        auto set = [&](char const* k, std::string const& v) {
            return XGBoosterSetParam(booster, k, v.c_str());
        };
        CHECK_CALL(set("objective", "binary:logistic"));
        CHECK_CALL(set("tree_method", "hist"));
        CHECK_CALL(set("max_depth", "8"));
        CHECK_CALL(set("eta", "0.1"));
        CHECK_CALL(set("colsample_bynode", "0.5"));
        CHECK_CALL(set("nthread", std::to_string(n_thread)));
        CHECK_CALL(set("verbosity", "0"));
        CHECK_CALL(set("seed", "42"));

        auto t0 = std::chrono::steady_clock::now();
        for (int i = 0; i < n_rounds; ++i) CHECK_CALL(XGBoosterUpdateOneIter(booster, i, dtrain));
        auto t1 = std::chrono::steady_clock::now();
        times.push_back(std::chrono::duration<double>(t1 - t0).count());

        XGBoosterFree(booster);
        XGDMatrixFree(dtrain);
    }

    std::sort(times.begin(), times.end());
    double med  = times[times.size() / 2];
    double mean = 0;
    for (double t : times) mean += t;
    mean /= static_cast<double>(times.size());
    double var = 0;
    for (double t : times) var += (t - mean) * (t - mean);
    double cv = std::sqrt(var / static_cast<double>(times.size())) / mean * 100.0;

    std::printf("{\"rows\":%d,\"features\":%d,\"rounds\":%d,\"reps\":%d,\"threads\":%d,",
                n_rows, n_features, n_rounds, n_reps, n_thread);
    std::printf("\"median_s\":%.4f,\"mean_s\":%.4f,\"cv_pct\":%.2f,\"all\":[", med, mean, cv);
    for (size_t i = 0; i < times.size(); ++i)
        std::printf("%.4f%s", times[i], i + 1 < times.size() ? "," : "");
    std::printf("]}\n");
    return 0;
}
