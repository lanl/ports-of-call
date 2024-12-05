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

#ifndef PORTS_OF_CALL_ROBUST_UTILS_HPP_
#define PORTS_OF_CALL_ROBUST_UTILS_HPP_

#include <cmath>
#include <limits>
#include <ports-of-call/portability.hpp>

namespace PortsOfCall {
namespace Robust {

template <typename T = Real>
PORTABLE_FORCEINLINE_FUNCTION constexpr auto SMALL() {
  return 10 * std::numeric_limits<T>::min();
}

template <typename T = Real>
PORTABLE_FORCEINLINE_FUNCTION constexpr auto EPS() {
  return 10 * std::numeric_limits<T>::epsilon();
}

template <typename T = Real>
PORTABLE_FORCEINLINE_FUNCTION constexpr T min_exp_arg() {
  return (std::numeric_limits<T>::min_exponent - 1) * M_LN2;
}

template <typename T = Real>
PORTABLE_FORCEINLINE_FUNCTION constexpr T max_exp_arg() {
  return std::numeric_limits<T>::max_exponent * M_LN2;
}

template <typename T>
PORTABLE_FORCEINLINE_FUNCTION auto make_positive(const T val) {
  return std::max(val, EPS<T>());
}

template <typename T>
PORTABLE_FORCEINLINE_FUNCTION Real make_bounded(const T val, const T vmin, const T vmax) {
  return std::min(std::max(val, vmin + EPS<T>()), vmax * (1.0 - EPS<T>()));
}

template <typename T>
PORTABLE_FORCEINLINE_FUNCTION int sgn(const T &val) {
  return (T(0) <= val) - (val < T(0));
}

template <typename A, typename B>
PORTABLE_FORCEINLINE_FUNCTION auto ratio(const A &a, const B &b) {
  return a / (b + sgn(b) * SMALL<B>());
}

template <typename T>
PORTABLE_FORCEINLINE_FUNCTION T safe_arg_exp(const T &x) {
  return x < min_exp_arg<T>()   ? 0.0
         : x > max_exp_arg<T>() ? std::numeric_limits<T>::infinity()
                                : std::exp(x);
}

} // namespace Robust
} // namespace PortsOfCall

#endif // PORTS_OF_CALL_ROBUST_UTILS_HPP_
