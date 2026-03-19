#pragma once
// ragged_std.hpp
//
// Minimal STL backend for ragged_offsets_core.hpp.
// Exposes only:
//   * ragged_std::build_nd<T,DepthDims,Index>(n_root, body [, ignored_exec])
//   * ragged_std::only_fill(row, lambda)
//   * ragged_std::span<T>
//   * ragged_std::ContiguousRagged<T,DepthDims,Index>

#include "ragged_offsets_core.hpp"

#include <algorithm>
#include <cstddef>
#include <type_traits>
#include <utility>
#include <vector>

namespace ragged_std {

namespace detail {

struct dummy_exec {};

struct backend_std {
  template <class U>
  using vector = std::vector<U>;

  template <class Vec>
  static auto raw_ptr(Vec& v) { return v.data(); }

  template <class Vec>
  static auto raw_ptr(const Vec& v) { return v.data(); }

  template <class Vec, class V>
  static void fill(Vec& v, const V& value, dummy_exec) {
    std::fill(v.begin(), v.end(), value);
  }

  template <class Vec>
  static void inclusive_scan_inplace(Vec& v, const std::size_t start, dummy_exec) {
    if (v.empty() || start >= v.size()) return;
    for (std::size_t i = start; i < v.size(); ++i) {
      v[i] = static_cast<typename Vec::value_type>(v[i] + v[i - 1]);
    }
  }

  template <class Vec>
  static typename Vec::value_type read_back(const Vec& v, const std::size_t idx, dummy_exec) {
    return v[idx];
  }

  template <class F>
  static void for_each_index(const std::size_t n, F f, dummy_exec) {
    for (std::size_t i = 0; i < n; ++i) f(i);
  }
};

} // namespace detail

template <class T>
using span = ::ragged_detail::span<T>;

using ::ragged_detail::only_fill;

template <class T, int DepthDims, class Index = std::size_t>
using ContiguousRagged =
  ::ragged_detail::ContiguousRaggedOffsets<detail::backend_std, T, DepthDims, Index>;

template <class T, int DepthDims, class Index = std::size_t, class Body>
auto build_nd(const std::size_t n_root, Body body)
  -> ContiguousRagged<T, DepthDims, Index> {
  return ::ragged_detail::build_nd_impl<detail::backend_std, T, DepthDims, Index>(n_root, std::move(body), detail::dummy_exec{});
}

// Optional trailing argument for call-site uniformity across backends.
template <class T, int DepthDims, class Index = std::size_t, class Body, class Ignored>
auto build_nd(const std::size_t n_root, Body body, const Ignored&)
  -> ContiguousRagged<T, DepthDims, Index> {
  return build_nd<T, DepthDims, Index>(n_root, std::move(body));
}

} // namespace ragged_std
