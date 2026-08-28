// One non-inline function per TU, so every multiply in the assembly listing
// belongs to the Philox block function. Answers: does the compiler already
// fold UMul64's four partial products to one native multiply when w == 32?
#include "philox_specialised.h"
#include <cstdint>

std::uint32_t ProbeDraw(xgboost_spec::Philox4x32& e) { return e(); }
