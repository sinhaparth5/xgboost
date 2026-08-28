// Both halves of the product taken from one multiply, rather than MulHi and
// MulLo each computing it. GCC already does this by CSE; clang and MSVC do not.
#include "philox_hilo.h"
#include <cstdint>

std::uint32_t ProbeDraw(xgboost_hilo::Philox4x32& e) { return e(); }
