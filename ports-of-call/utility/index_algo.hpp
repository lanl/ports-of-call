#ifndef _PORTSOFCALL_UTILITY_INDEX_ALGO_HPP_
#define _PORTSOFCALL_UTILITY_INDEX_ALGO_HPP_

#include "../portability.hpp"
#include "array_algo.hpp"
#include <array>
#include <numeric>
#include <type_traits>

namespace util {

/*
 template <auto I, class T, std::size_t N>
PORTABLE_FORCEINLINE_FUNCTION static constexpr auto
get_stride(std::array<T, N> const &dim) {
  static_assert(I < dim.size(), "Dim index is out of bounds");

  // column major
  return std::accumulate(std::begin(dim), std::begin(dim) + I, std::size_t{1},
                         std::multiplies<std::size_t>{});
}

namespace detail {
template <class T, std::size_t N, std::size_t... Is>
PORTABLE_FORCEINLINE_FUNCTION static constexpr auto
get_strides_impl(std::array<T, N> const &dim, std::index_sequence<Is...>) {
  return std::array{get_stride<Is>(dim)...};
}

} // namespace detail

template <class T, std::size_t N>
PORTABLE_FORCEINLINE_FUNCTION static constexpr auto
get_strides(std::array<T, N> const &dim) {
  return detail::get_strides_impl(dim, std::make_index_sequence<dim.size()>{});
}

template <class T, std::size_t N>
PORTABLE_FORCEINLINE_FUNCTION static constexpr auto
fast_findex(std::array<T, N> const &ijk, std::array<T, N> const &dim,
            std::array<T, N> const &stride) {
  // TODO: assert ijk in bounds
  return std::inner_product(std::begin(ijk), std::end(ijk), std::begin(stride),
                            std::size_t{0});
}

template <class T, std::size_t N>
PORTABLE_FORCEINLINE_FUNCTION static constexpr auto
findex(std::array<T, N> const &ijk, std::array<T, N> const &dim) {
  return fast_findex(ijk, dim, get_strides(dim));
}
*/
template <auto I, class A>
PORTABLE_FORCEINLINE_FUNCTION static constexpr auto get_stride(A const &dim) {

  // column major
  return array_partial_reduce<I>(dim, value_t<A>{1}, std::multiplies<std::size_t>{});
}

namespace detail {
template <class A, std::size_t... Is>
PORTABLE_FORCEINLINE_FUNCTION static constexpr auto
get_strides_impl(A const &dim, std::index_sequence<Is...>) {
  return A{get_stride<Is>(dim)...};
}
template <class A>
PORTABLE_FORCEINLINE_FUNCTION static constexpr auto get_strides(A const &dim) {
  return detail::get_strides_impl(dim, is(dim.size()));
}
} // namespace detail
template <class A>
PORTABLE_FORCEINLINE_FUNCTION static constexpr auto
fast_findex(A const &ijk, A const &dim, A const &stride) {
  // TODO: assert ijk in bounds
  return array_reduce(array_map(ijk, stride, [](auto a, auto b) { return a * b; }),
                      value_t<A>{1}, std::plus<std::size_t>{});
}
template <class A>
PORTABLE_FORCEINLINE_FUNCTION static constexpr auto findex(A const &ijk, A const &dim) {
  return fast_findex(ijk, dim, get_strides(dim));
}

template <class A>
PORTABLE_FORCEINLINE_FUNCTION static constexpr A
fast_mindices(std::size_t idx, A const &dim, A const &stride) {
  A mdidx;
  for (std::int64_t i = dim.size() - 1; i >= 0; --i) {
    mdidx[i] = idx / std::size_t(stride[i]);
    idx -= mdidx[i] * std::size_t(stride[i]);
  }
  return mdidx;
}

template <class A>
PORTABLE_FORCEINLINE_FUNCTION static constexpr auto mindices(std::size_t idx, A dim) {
  return fast_mindices(idx, dim, get_strides(dim));
}

} // namespace util

#endif // _PORTSOFCALL_UTILITY_INDEX_ALGO_HPP_
