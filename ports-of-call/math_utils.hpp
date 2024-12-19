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
#include <ports-of-call/robust_utils.hpp>

namespace PortsOfCall {
namespace Math {

// Faster implementation of std::pow for arithmetic bases and non-negative integer
// exponents.  For sufficiently large integer powers std::pow may be faster, but testing
// indicates that the following implementation is significantly faster (roughly a factor
// of two or better) up to powers of at least 100.
template <typename BaseT, typename ExponentT,
          typename std::enable_if<std::is_arithmetic_v<std::decay_t<BaseT>> &&
                                  std::is_integral_v<std::decay_t<ExponentT>>>::type * =
              nullptr>
PORTABLE_FORCEINLINE_FUNCTION constexpr auto power(BaseT base,
                                                                     ExponentT exponent) {
  using std::pow;
  using PowT = decltype(pow(base, exponent));
  if (!Robust::check_nonnegative(exponent) || exponent > ExponentT{100}) {
    return pow(base, exponent);
  }
  PowT result = PowT{1};
  while (true) {
    if (exponent & 1) result *= base; // Multiply if the remaining exponent is odd
    exponent >>= 1;                   // Right-shift the exponent (divide by 2)
    if (!exponent) break;             // If the remaining exponent is zero, we are done
    base *= base;                     // We halved the exponent, so square the base
  }
  return result;
}
// Faster implementation of std::pow() for non-negative arithmetic bases and
// floating-point exponents
template <typename BaseT, typename ExponentT,
          typename std::enable_if<std::is_arithmetic_v<std::decay_t<BaseT>> &&
                                  std::is_floating_point_v<std::decay_t<ExponentT>>>::type
              * = nullptr>
PORTABLE_FORCEINLINE_FUNCTION constexpr auto
power(BaseT const &base, ExponentT const &exponent) {
  using std::exp;
  using std::log;
  using std::pow;
  if (!Robust::check_nonnegative(base)) {
    return pow(base, exponent);
  }
  return exponent == ExponentT{0} ? BaseT{1} // Enforcing base^0=1 (including 0^0=1)
         : base == BaseT{0}       ? BaseT{0}
                                  : exp(exponent * log(base));
}
// Overload for non-arithmetic bases or exponents
template <typename BaseT, typename ExponentT,
          typename std::enable_if<not std::is_arithmetic_v<std::decay_t<BaseT>> ||
                                  not std::is_arithmetic_v<std::decay_t<ExponentT>>>::type
              * = nullptr>
PORTABLE_FORCEINLINE_FUNCTION constexpr auto
power(BaseT const &base, ExponentT const &exponent) {
  using std::pow;
  return pow(base, exponent);
}

template <typename Value>
struct plus {
  PORTABLE_FUNCTION constexpr Value operator()(Value const accum, Value const current) {
    return accum + current;
  }
};

template <typename IterB, typename IterE, typename Value,
          typename Op = PortsOfCall::Math::plus<Value>>
PORTABLE_FUNCTION constexpr Value accumulate(IterB begin, IterE end, Value accum,
                                             Op &&op = PortsOfCall::Math::plus<Value>{}) {
  for (auto iter = begin; iter != end; ++iter) {
    accum = op(accum, *iter);
  }
  return accum;
}

} // namespace Math
} // namespace PortsOfCall

#endif // PORTS_OF_CALL_MATH_UTILS_HPP_
