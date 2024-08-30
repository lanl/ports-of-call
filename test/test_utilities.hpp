#ifndef _TEST_PORTOFCALL_HPP_
#define _TEST_PORTOFCALL_HPP_

#include <functional>
#include <ports-of-call/array.hpp>
#include <ports-of-call/portability.hpp>
#include <ports-of-call/portable_arrays.hpp>
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
  return PortableMDArray<Real>(d, arr[I]...);
}

// stand up and strip down a benchmark
// input is a set of sizes
template <class T, std::size_t N, typename I1 = std::make_index_sequence<N>,
          typename I2 = std::make_index_sequence<2 * N>>
PORTABLE_FUNCTION auto idx_contiguous_bm(const std::array<T, N> &nxa) {
  auto nc = std::accumulate(std::begin(nxa), std::end(nxa), 1, std::multiplies<T>{});

  Real *tape_d = (Real *)PORTABLE_MALLOC(nc * sizeof(Real));

  auto view_d = mdview(tape_d, nxa, I1{});

  pf_invoke(view_d, nxa, I2{});

  PORTABLE_FREE(tape_d);

  // return here is arbitrary, Catch2 recommends this to avoid
  // optimizing out a call.
  return 1;
}

} // namespace testing
#endif
