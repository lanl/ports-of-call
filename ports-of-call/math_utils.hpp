//------------------------------------------------------------------------------
// © 2021-2024. Triad National Security, LLC. All rights reserved.  This
// program was produced under U.S. Government contract 89233218CNA000001
// for Los Alamos National Laboratory (LANL), which is operated by Triad
// National Security, LLC for the U.S.  Department of Energy/National
// Nuclear Security Administration. All rights in the program are
// reserved by Triad National Security, LLC, and the U.S. Department of
// Energy/National Nuclear Security Administration. The Government is
// granted for itself and others acting on its behalf a nonexclusive,
// paid-up, irrevocable worldwide license in this material to reproduce,
// prepare derivative works, distribute copies to the public, perform
// publicly and display publicly, and to permit others to do so.
//------------------------------------------------------------------------------

#ifndef PORTS_OF_CALL_MATH_UTILS_HPP_
#define PORTS_OF_CALL_MATH_UTILS_HPP_

#include <cmath>
#include <limits>
#include <ports-of-call/portability.hpp>

namespace PortsOfCall {
namespace Math {

// For small integer powers, int_power is faster than std::pow.  For sufficiently large
// integer powers std::pow may be faster, but testing indicates int_power is significantly
// faster (roughly a factor of two or better) up to powers of at least 100.
template <typename base_t, typename exp_t>
[[gnu::always_inline]] PORTABLE_FUNCTION constexpr inline base_t int_power(base_t base,
                                                                           exp_t exp) {
  static_assert(std::is_arithmetic<base_t>::value,
                "base value must be an arithmetic type");
  static_assert(std::is_integral<exp_t>::value, "exponent must be an integer type");
  assert_nonnegative(exp);
  base_t result{1};
  for (;;) {
    // Multiply if the current position in the exponent is true
    if (exp & 1) {
      result *= base;
    }
    // Shift the exponent to the next position
    exp >>= 1;
    // If the exponent is now empty, we're done
    if (!exp) {
      break;
    }
    // We halved the exponent (see the shift operation), so square the base
    base *= base;
  }
  return result;
}

template <typename Value>
struct plus {
  PORTABLE_FUNCTION constexpr Value operator()(Value const accum, Value const current) {
    return accum + current;
  }
};

template <typename IterB, typename IterE, typename Value,
          typename Op = singe::util::plus<Value>>
PORTABLE_FUNCTION constexpr Value accumulate(IterB begin, IterE end, Value accum,
                                             Op &&op = singe::util::plus<Value>{}) {
  for (auto iter = begin; iter != end; ++iter) {
    accum = op(accum, *iter);
  }
  return accum;
}

} // namespace Math
} // namespace PortsOfCall

#endif // PORTS_OF_CALL_MATH_UTILS_HPP_
