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

// This file created with the assistance of generative AI

#ifndef PORTS_OF_CALL_ROBUST_UTILS_HPP_
#define PORTS_OF_CALL_ROBUST_UTILS_HPP_

#include <algorithm>
#include <cmath>
#include <concepts>
#include <limits>
#include <ports-of-call/portability.hpp>

namespace PortsOfCall {
namespace Robust {

template <typename T>
concept nonnegative_testable = requires(const T value) {
  { value >= 0 } -> std::convertible_to<bool>;
};

template <typename T>
concept arithmetic_like =
    std::integral<std::decay_t<T>> || std::floating_point<std::decay_t<T>>;

template <typename T>
concept integral_like = std::integral<std::decay_t<T>>;

template <typename T>
concept floating_point_like = std::floating_point<std::decay_t<T>>;

template <typename T = Real>
  requires(std::integral<T> || std::floating_point<T>)
PORTABLE_FORCEINLINE_FUNCTION constexpr bool is_normal(const T val,
                                                       const T factor = T{1}) {
  if constexpr (std::is_integral_v<T>) {
    return true;
  } else {
    using limits = std::numeric_limits<T>;
    const T abs_val = (val < T{0}) ? -val : val;
    return ((abs_val >= limits::min()) && (abs_val * factor <= limits::max()));
  }
}

template <typename T = Real>
  requires(std::integral<T> || std::floating_point<T>)
PORTABLE_FORCEINLINE_FUNCTION constexpr bool is_normal_or_zero(const T val,
                                                               const T factor = T{1}) {
  return is_normal(val, factor) || (val == T{0});
}

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
  const B mask = static_cast<B>(std::abs(b) < SMALL<B>());
  const B denom = mask * sgn(b) * SMALL<B>() + (1 - mask) * b;
  return a / denom;
}

template <typename T>
PORTABLE_FORCEINLINE_FUNCTION T safe_arg_exp(const T &x) {
  return x < min_exp_arg<T>()   ? 0.0
         : x > max_exp_arg<T>() ? std::numeric_limits<T>::infinity()
                                : std::exp(x);
}

// type-safe check against 0
template <nonnegative_testable T>
PORTABLE_FUNCTION constexpr bool check_nonnegative(const T t) {
  if constexpr (std::unsigned_integral<T>) {
    return true;
  } else {
    return t >= 0;
  }
}

} // namespace Robust
} // namespace PortsOfCall

#endif // PORTS_OF_CALL_ROBUST_UTILS_HPP_
