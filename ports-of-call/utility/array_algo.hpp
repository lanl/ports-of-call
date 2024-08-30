// © (or copyright) 2019-2024. Triad National Security, LLC. All rights
// reserved.  This program was produced under U.S. Government contract
// 89233218CNA000001 for Los Alamos National Laboratory (LANL), which is
// operated by Triad National Security, LLC for the U.S.  Department of
// Energy/National Nuclear Security Administration. All rights in the
// program are reserved by Triad National Security, LLC, and the
// U.S. Department of Energy/National Nuclear Security
// Administration. The Government is granted for itself and others acting
// on its behalf a nonexclusive, paid-up, irrevocable worldwide license
// in this material to reproduce, prepare derivative works, distribute
// copies to the public, perform publicly and display publicly, and to
// permit others to do so.

#ifndef _PORTSOFCALL_UTILITY_ARRAY_ALGO_HPP_
#define _PORTSOFCALL_UTILITY_ARRAY_ALGO_HPP_

#include "../portability.hpp"
#include <array>
#include <type_traits>

namespace util {

// shorter make_index_sequence
template <auto N>
constexpr auto is(std::integral_constant<decltype(N), N>) {
  return std::make_index_sequence<N>{};
}

// shorter value_type
template <class A>
using value_t = typename A::value_type;

// determines the return type of Op(a,b)
template <class A, class Op>
using reduction_value_t =
    decltype(std::declval<Op>()(std::declval<value_t<A>>(), std::declval<value_t<A>>()));

namespace detail {
template <class A, class F, std::size_t... Is>
PORTABLE_FORCEINLINE_FUNCTION constexpr auto array_map_impl(A const &x, F f,
                                                            std::index_sequence<Is...>) {
  return std::array{f(x[Is])...};
}

template <class A, class B, std::size_t N, class F, std::size_t... Is>
PORTABLE_FORCEINLINE_FUNCTION constexpr auto array_map_impl(A const &x, B const &y, F f,
                                                            std::index_sequence<Is...>) {
  return A{f(x[Is], y[Is])...};
}

template <std::size_t f, std::size_t l, class A, class Op>
PORTABLE_INLINE_FUNCTION constexpr auto array_reduce_impl(A const &x, Op op) {
  if constexpr ((l - f) == 1)
    return x[f];
  else {
    constexpr std::size_t n = l - f;
    auto left_sum = array_reduce_impl<f, f + n / 2>(x, op);
    auto right_sum = array_reduce_impl<f + n / 2, l>(x, op);
    return op(left_sum, right_sum);
  }
}

} // namespace detail

// maps an unary function f(x) to each array value, returning an array of results
// x = {f(a[0]), f(a[1]),..., f(a[N-1])}
template <class A, class F>
PORTABLE_FORCEINLINE_FUNCTION constexpr auto array_map(A const &x, F f) {
  return detail::array_map_impl(x, f, is(x.size()));
}

// maps a binary function to each array value, returning an array of results
// x = {f(a[0], b[0]), f(a[1], b[1]),..., f(a[N-1], b[N-1])}

template <class A, class B, class F>
PORTABLE_FORCEINLINE_FUNCTION constexpr auto array_map(A const &x, B const &y, F f) {
  return detail::array_map_impl(x, y, f, is(x.size()));
}

template <std::size_t I, class A, class Op, class T = reduction_value_t<A, Op>>
PORTABLE_FORCEINLINE_FUNCTION constexpr T array_partial_reduce(A x, T initial_value,
                                                               Op op) {
  static_assert(I <= x.size());
  if constexpr (I == 0)
    return initial_value;
  else
    return detail::array_reduce_impl<0, I>(x, op);
}

template <class A, class Op, class T = reduction_value_t<A, Op>>

// performs a reduction on an array of values
// e.g. x = sum_i a[i]
PORTABLE_FORCEINLINE_FUNCTION constexpr T array_reduce(A x, T initial_value, Op op) {
  return array_partial_reduce<x.size()>(x, initial_value, op);
}

} // namespace util

#endif // _PORTSOFCALL_UTILITY_ARRAY_HPP_
