#ifndef _PORTSOFCALL_UTILITY_ARRAY_ALGO_HPP_
#define _PORTSOFCALL_UTILITY_ARRAY_ALGO_HPP_

#include "../portability.hpp"
#include <array>
#include <type_traits>

namespace util {

namespace detail {
template <class T, std::size_t N, class F, std::size_t... Is>
PORTABLE_FORCEINLINE_FUNCTION constexpr auto
array_map_impl(std::array<T, N> const &x, F f, std::index_sequence<Is...>) {
  return std::array{f(x[Is])...};
}

template <class T, class U, std::size_t N, class F, std::size_t... Is>
PORTABLE_FORCEINLINE_FUNCTION constexpr auto
array_map_impl(std::array<T, N> const &x, std::array<U, N> const &y, F f,
               std::index_sequence<Is...>) {
  return std::array{f(x[Is], y[Is])...};
}

template <std::size_t f, std::size_t l, class T, std::size_t N, class Op>
PORTABLE_INLINE_FUNCTION constexpr T array_reduce_impl(std::array<T, N> const &x, Op op) {
  if constexpr ((l - f) == 1)
    return x[f];
  else {
    constexpr std::size_t n = l - f;
    T left_sum = array_reduce_impl<f, f + n / 2>(x, op);
    T right_sum = array_reduce_impl<f + n / 2, l>(x, op);
    return op(left_sum, right_sum);
  }
}

} // namespace detail

template <class T, std::size_t N, class F>
PORTABLE_FORCEINLINE_FUNCTION constexpr auto array_map(std::array<T, N> const &x, F f) {
  return detail::array_map_impl(x, f, std::make_index_sequence<N>{});
}

template <class T, class U, std::size_t N, class F>
PORTABLE_FORCEINLINE_FUNCTION constexpr auto array_map(std::array<T, N> const &x,
                                                       std::array<U, N> const &y, F f) {
  return detail::array_map_impl(x, y, f, std::make_index_sequence<N>{});
}

template <std::size_t I, class T, std::size_t N, class Op>
PORTABLE_FORCEINLINE_FUNCTION constexpr T array_partial_reduce(std::array<T, N> x,
                                                               T initial_value, Op op) {
  static_assert(I <= N);
  if constexpr (I == 0)
    return initial_value;
  else
    return detail::array_reduce_impl<0, I>(x, op);
}

template <class T, std::size_t N, class Op>
PORTABLE_FORCEINLINE_FUNCTION constexpr T array_reduce(std::array<T, N> x,
                                                       T initial_value, Op op) {
  return array_partial_reduce<N>(x, initial_value, op);
}
template <std::size_t... Is>
PORTABLE_FORCEINLINE_FUNCTION constexpr auto
as_array(std::index_sequence<sizeof...(Is)>) {
  return std::array{Is...};
}

} // namespace util

#endif // _PORTSOFCALL_UTILITY_ARRAY_HPP_
