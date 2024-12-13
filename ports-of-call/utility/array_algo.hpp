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
#include <utility>

namespace util {

// NB:
// the array type is explicitly left unspecified, as it may be
// desirable to use a type other than std::array.
// however, it is required that the underlying type must
// conform to a minimum std::array interface:
//
//  1.) constexpr operator[]
//  2.) constexpr size_type size()
//  3.) array_type::value_type
//
// A casual choice early on was that the array parameter is
// assumed to be "complete" at instanciation. That is,
// we use
//
//  template<class A>
//
// rather than (the more specific)
//
//  template<template<class, auto> A, class T, auto N>
//
// this imposes some structure that is a little loose, and may
// be refactored at some point.

// shorter make_index_sequence
template <auto N>
constexpr auto is(std::integral_constant<decltype(N), N>) {
  return std::make_index_sequence<N>{};
}

// shorter value_type
template <class A>
using value_t = typename A::value_type;

// constexpr helper to get compile-time size of array A
template <class A>
PORTABLE_FORCEINLINE_FUNCTION constexpr auto get_size(const A &a) {
  return a.size();
}

// wraps a variadic set of variables into an array,
// making an attempt to cast them.
template <class A, class... B>
PORTABLE_FORCEINLINE_FUNCTION constexpr auto wrap_vars(B... vv) {
  return A{static_cast<value_t<A>>(vv)...};
}

// determines the return type of Op(a,b)
template <class A, class Op>
using reduction_value_t =
    decltype(std::declval<Op>()(std::declval<value_t<A>>(), std::declval<value_t<A>>()));

namespace detail {
template <class A, class F, auto... Is>
PORTABLE_FORCEINLINE_FUNCTION constexpr auto array_map_impl(A const &x, F f,
                                                            std::index_sequence<Is...>) {
  return std::array{f(x[Is])...};
}

template <class A, class B, class F, auto... Is>
PORTABLE_FORCEINLINE_FUNCTION constexpr auto array_map_impl(A const &x, B const &y, F f,
                                                            std::index_sequence<Is...>) {
  return A{f(x[Is], y[Is])...};
}

template <auto f, auto l, class A, class Op>
PORTABLE_INLINE_FUNCTION constexpr auto array_reduce_impl(A const &x, Op op) {
  if constexpr ((l - f) == 1)
    return x[f];
  else {
    constexpr auto n = l - f;
    auto left_sum = array_reduce_impl<f, f + n / 2>(x, op);
    auto right_sum = array_reduce_impl<f + n / 2, l>(x, op);
    return op(left_sum, right_sum);
  }
}

} // namespace detail

///////////////////////////////////////////
/// CMM: This verbose code is probably too
/// messy to keep, but I liked it so I wanted
/// to keep a record on a commit somehwere if
/// i ever wanted to lift it.
/*
namespace detail{
template <auto, auto V>
constexpr auto NREP = V;

template <class A, auto Fill, std::size_t... Is, std::size_t... Rs, class... B>
PORTABLE_FORCEINLINE_FUNCTION constexpr auto
make_underfilled_reverse_array_impl(std::index_sequence<Is...>,
                                    std::index_sequence<Rs...>, B &&...vv) {
    return wrap_vars<A>((std::get<sizeof...(Is) - 1 - Is>(std::tie(vv...)))...,
                      NREP<Rs, Fill>...);
}
template <class A, auto Fill, std::size_t... Is, std::size_t... Rs, class... B>
PORTABLE_FORCEINLINE_FUNCTION constexpr auto
make_underfilled_array_impl(std::index_sequence<Is...>, std::index_sequence<Rs...>,
                            B &&...vv) {
    return wrap_vars<A>((std::get<Is>(std::tie(vv...)))..., NREP<Rs, Fill>...);
}
} //

template <class A, auto Fill = 1, class... B>
PORTABLE_FORCEINLINE_FUNCTION constexpr auto make_underfilled_reverse_array(B... vv) {
  constexpr auto D = get_size(A{});
  return detail::make_underfilled_reverse_array_impl<A, Fill>(
      std::index_sequence_for<B...>{}, std::make_index_sequence<D - sizeof...(B)>{},
      vv...);
}

template <class A, auto Fill = 1, class... B>
PORTABLE_FORCEINLINE_FUNCTION constexpr auto make_underfilled_array(B... vv) {
  constexpr auto D = get_size(A{});
  return detail::make_underfilled_array_impl<A, Fill>(
      std::index_sequence_for<B...>{}, std::make_index_sequence<D - sizeof...(B)>{},
      vv...);
}
*/
///////////////////////////////////////////

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
  return detail::array_map_impl(x, y, f, std::make_index_sequence<get_size(A{})>{});
}

// does a reduction on the sub-array A[I..F)
// also used to construct full reduction A[..N)
template <auto I, auto F, class A, class Op, class T = reduction_value_t<A, Op>>
PORTABLE_FORCEINLINE_FUNCTION constexpr T array_partial_reduce(A x, T initial_value,
                                                               Op op) {
  static_assert(F <= get_size(A{}));
  if constexpr ((F - I) == 0)
    return initial_value;
  else
    return detail::array_reduce_impl<I, F>(x, op);
}

// performs a reduction on an array of values
// e.g. x = sum_i a[i]
// NB Op is evaluated as a magma, i.e. Op(a,b,c,d) = Op(a,b) + Op(c,d)
// it executes O(ln n) operations. It's possible this obfuscates
// potential compiler optimizations for small (2-3) sized arrays
template <class A, class Op, class T = reduction_value_t<A, Op>>
PORTABLE_FORCEINLINE_FUNCTION constexpr T array_reduce(A x, T initial_value, Op op) {
  return array_partial_reduce<0, get_size(A{})>(x, initial_value, op);
}

// if input values are less than output array length,
// this function returns an array with "filled" values.
// e.g.
//  in: {x, y}
//  out: {x, y, 1, 1, 1}
template <auto P, auto Fill = 1, template <class, auto> class A, class T, auto O>
PORTABLE_FORCEINLINE_FUNCTION constexpr decltype(auto)
make_underfilled_array(const A<T, O> &in) {
  using i_t = typename A<T, 0>::size_type;
  A<T, P> out;
  for (i_t i = 0; i < in.size(); ++i)
    out[i] = in[i];
  for (i_t i = in.size(); i < out.size(); ++i)
    out[i] = Fill;
  return out;
}

// same as above, but reverses input values
// e.g.
//  in: {x, y, z}
//  out: {z, y, x, 1, 1}
template <auto P, auto Fill = 1, template <class, auto> class A, class T, auto O>
PORTABLE_FORCEINLINE_FUNCTION constexpr decltype(auto)
make_underfilled_reversed_array(const A<T, O> &in) {
  using i_t = typename A<T, 0>::size_type;
  A<T, P> out;
  for (i_t i = in.size() - 1; i >= 0; i--)
    out[i] = in[i];
  for (i_t i = in.size(); i < out.size(); ++i)
    out[i] = Fill;
  return out;
}

} // namespace util

#endif // _PORTSOFCALL_UTILITY_ARRAY_HPP_
