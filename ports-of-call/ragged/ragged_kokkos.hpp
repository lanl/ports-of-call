#pragma once
// ragged_kokkos.hpp
//
// Minimal Kokkos backend for ragged_offsets_core.hpp.
// Exposes only:
//   * ragged_kokkos::build_nd<T,DepthDims,Index,ExecSpace>(n_root, body [, exec])
//   * ragged_kokkos::only_fill(row, lambda)
//   * ragged_kokkos::span<T>
//   * ragged_kokkos::ContiguousRagged<T,DepthDims,Index,ExecSpace>

#include <Kokkos_Core.hpp>

#include "ragged_offsets_core.hpp"

#include <cstddef>
#include <limits>
#include <type_traits>
#include <utility>

namespace ragged_kokkos {

namespace detail {

template <class ExecSpace>
struct backend_kokkos {
  using exec_space   = ExecSpace;
  using memory_space = typename ExecSpace::memory_space;

  template <class U>
  struct view_vector {
    using value_type = U;
    using view_type  = Kokkos::View<U*, memory_space>;

    view_type v{};

    view_vector() = default;

    std::size_t size() const noexcept { return static_cast<std::size_t>(v.extent(0)); }

    void resize(const std::size_t n) {
      v = view_type(Kokkos::view_alloc(Kokkos::WithoutInitializing, "ragged_kokkos_vec"), n);
    }
  };

  template <class U>
  using vector = view_vector<U>;

  template <class Vec>
  static auto raw_ptr(Vec& vv) { return vv.v.data(); }

  template <class Vec>
  static auto raw_ptr(const Vec& vv) { return vv.v.data(); }

  template <class Vec, class V>
  static void fill(Vec& vv, const V& value, const ExecSpace& exec) {
    if (vv.size() == 0) return;
    Kokkos::deep_copy(exec, vv.v, value);
  }

  template <class Vec>
  static void inclusive_scan_inplace(Vec& vv, const std::size_t start, const ExecSpace& exec) {
    using value_type = typename Vec::value_type;

    const std::size_t n = vv.size();
    if (n == 0 || start >= n) return;

    auto v = vv.v;
    const std::size_t imax = static_cast<std::size_t>((std::numeric_limits<int>::max)());

    if (n <= imax && start <= imax) {
      const int b = static_cast<int>(start);
      const int e = static_cast<int>(n);
      Kokkos::parallel_scan(
        "ragged_kokkos_scan_inplace_int",
        Kokkos::RangePolicy<ExecSpace, Kokkos::IndexType<int>>(exec, b, e),
        PORTABLE_LAMBDA(const int i, value_type& upd, const bool final) {
          const value_type add  = v(i);
          const value_type next = static_cast<value_type>(upd + add);
          upd = next;
          if (final) v(i) = next;
        }
      );
    } else {
      Kokkos::parallel_scan(
        "ragged_kokkos_scan_inplace_size_t",
        Kokkos::RangePolicy<ExecSpace, Kokkos::IndexType<std::size_t>>(exec, start, n),
        PORTABLE_LAMBDA(const std::size_t i, value_type& upd, const bool final) {
          const value_type add  = v(i);
          const value_type next = static_cast<value_type>(upd + add);
          upd = next;
          if (final) v(i) = next;
        }
      );
    }
  }

  template <class Vec>
  static typename Vec::value_type read_back(const Vec& vv, const std::size_t idx, const ExecSpace& exec) {
    using value_type = typename Vec::value_type;
    value_type out{};
    if (vv.size() == 0) return out;
    exec.fence();
    Kokkos::deep_copy(out, Kokkos::subview(vv.v, idx));
    return out;
  }

  template <class F>
  static void for_each_index(const std::size_t n, F f, const ExecSpace& exec) {
    if (n == 0) return;

    const std::size_t imax = static_cast<std::size_t>((std::numeric_limits<int>::max)());

    if (n <= imax) {
      Kokkos::parallel_for(
        "ragged_kokkos_for_each_int",
        Kokkos::RangePolicy<ExecSpace, Kokkos::IndexType<int>>(exec, 0, static_cast<int>(n)),
        PORTABLE_LAMBDA(const int ii) { f(static_cast<std::size_t>(ii)); }
      );
    } else {
      Kokkos::parallel_for(
        "ragged_kokkos_for_each_size_t",
        Kokkos::RangePolicy<ExecSpace, Kokkos::IndexType<std::size_t>>(exec, 0, n),
        f
      );
    }
  }
};

} // namespace detail

template <class T>
using span = ::ragged_detail::span<T>;

using ::ragged_detail::only_fill;

template <class T, int DepthDims, class ExecSpace, class Index = std::size_t>
using ContiguousRagged =
  ::ragged_detail::ContiguousRaggedOffsets<detail::backend_kokkos<ExecSpace>, T, DepthDims, Index>;

template <class T,
          int DepthDims,
          class Index = std::size_t,
          class ExecSpace = Kokkos::DefaultExecutionSpace,
          class Body>
auto build_nd(const std::size_t n_root, Body body, ExecSpace exec = ExecSpace{})
  -> ContiguousRagged<T, DepthDims, ExecSpace, Index> {
  return ::ragged_detail::build_nd_impl<detail::backend_kokkos<ExecSpace>, T, DepthDims, Index>(
    n_root, std::move(body), exec
  );
}

} // namespace ragged_kokkos
