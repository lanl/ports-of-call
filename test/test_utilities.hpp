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

#ifndef _TEST_TEST_UTILITIES_HPP_
#define _TEST_TEST_UTILITIES_HPP_

#include <ports-of-call/array.hpp>
#include <ports-of-call/portability.hpp>
#include <ports-of-call/portable_arrays.hpp>
#include <type_traits>
#include <utility>

namespace testing {

template <typename Array, std::size_t... I>
constexpr auto array_fix_impl(const Array &a, std::index_sequence<I...> is) {
  auto mm = std::array<int, is.size() * 2>();
  for (int i = 0; i < is.size(); ++i) {
    mm[(i * 2)] = 0;
    mm[(i * 2) + 1] = a[i];
  }
  return mm;
}

// we want to call
//    portableFor(..., <0, NX>, <0, NY>, ...)
// for an arbitrary set of ranges
// this function takes an array of N sizes, and converts
// it into an array 2N {0, NX} contiguous pairs.
// ex:
//    array{10,12,13} -> array{0,10,0,12,0,13}
template <typename T, std::size_t N, typename Indx = std::make_index_sequence<N>>
PORTABLE_FORCEINLINE_FUNCTION constexpr auto array_fix(const std::array<T, N> &a) {
  return array_fix_impl(a, Indx{});
}

// constructs the portableFor call
// tparams:
//    - View : class of data view
//    - Array: class of container holding sizes
//    - J... : index sequence
// params:
//    - View v: view of data
//    - array ext_par: array of sizes
// this was written ad-hoc, and could be more general
// (e.g. pass label, functor in lambda)
template <class View, class Array, std::size_t... J>
PORTABLE_INLINE_FUNCTION constexpr auto pf_invoke(View v, const Array &ext_par,
                                                  std::index_sequence<J...>) {

  auto stud_arr = array_fix(ext_par);
  portableFor(
      "set unique val", stud_arr[J]..., PORTABLE_LAMBDA(auto... is) { v(is...) = 1.0; });
}

template <class Ptr, class Array, std::size_t... I>
PORTABLE_FORCEINLINE_FUNCTION constexpr decltype(auto) mdview(Ptr *d, const Array &arr,
                                                              std::index_sequence<I...>) {
  return PortsOfCall::PortableMDArray<Real>(d, arr[I]...);
}

// stand up a benchmark
template <class... Ns, std::size_t N = sizeof...(Ns),
          typename I1 = std::make_index_sequence<N>,
          typename I2 = std::make_index_sequence<2 * N>,
          class = std::enable_if_t<std::conjunction_v<std::is_integral<Ns>...>>>
PORTABLE_FUNCTION auto alloc_tape(Ns... ns) {
  const auto nc = (ns * ... * std::size_t{1});

  Real *tape_d = (Real *)PORTABLE_MALLOC(nc * sizeof(Real));

  auto view_d = mdview(tape_d, std::array{ns...}, I1{});

  return std::tuple(tape_d, view_d);
}

} // namespace testing
#endif