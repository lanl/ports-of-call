#ifndef _PORTABLE_ARRAYS_HPP_
#define _PORTABLE_ARRAYS_HPP_
//========================================================================================
// Portable MDAarray, based on AthenaArray in Athena++
// Copyright(C) 2014 James M. Stone <jmstone@princeton.edu> and other code
// contributors Licensed under the 3-clause BSD License, see LICENSE file for
// details
//
// © (or copyright) 2019-2024. Triad National Security, LLC. All rights
// reserved.  This program was produced under U.S. Government contract
// 89233218CNA000001 for Los Alamos National Laboratory (LANL), which
// is operated by Triad National Security, LLC for the U.S.
// Department of Energy/National Nuclear Security Administration. All
// rights in the program are reserved by Triad National Security, LLC,
// and the U.S. Department of Energy/National Nuclear Security
// Administration. The Government is granted for itself and others
// acting on its behalf a nonexclusive, paid-up, irrevocable worldwide
// license in this material to reproduce, prepare derivative works,
// distribute copies to the public, perform publicly and display
// publicly, and to permit others to do so.
// ========================================================================================

//  The operator() is overloaded, e.g. elements of a 4D array of size
//  [N4xN3xN2xN1] are accessed as:  A(n,k,j,i) = A[i + N1*(j + N2*(k + N3*n))]
//  NOTE THE TRAILING INDEX INSIDE THE PARENTHESES IS INDEXED FASTEST

#include "array.hpp"
#include "portability.hpp"
#include "span.hpp"
#include "utility/array_algo.hpp"
#include "utility/index_algo.hpp"
#include <algorithm>
#include <assert.h>
#include <cstddef> // size_t
#include <cstring> // memset()
#include <functional>
#include <type_traits>
#include <utility> // swap()

namespace PortsOfCall {
// maximum number of dimensions
constexpr std::size_t MAXDIM = 6;

namespace detail {
// convert `index_sequence` values to constant
template <std::size_t I, std::size_t V>
constexpr std::size_t to_const = V;

} // namespace detail

// if C++20, we can use std::array without worry on device
// else use a hand-rolled array
#if __cplusplus == 202002L
template <class T, auto N>
using Array = std::array<T, N>;
#else
template <class T, auto N>
using Array = PortsOfCall::array<T, N>;
#endif // __cplusplus

template <auto N>
using IArray = Array<std::size_t, N>;

template <typename T, std::size_t D = MAXDIM>
class PortableMDArray {
 public:
  using this_type = PortableMDArray<T, D>;
  using element_type = T;
  using value_type = typename std::remove_cv<T>::type;
  using index_type = std::ptrdiff_t;
  using size_type = std::size_t;
  using pointer = T *;
  using const_pointer = const T *;
  using reference = T &;
  using const_reference = const T &;

  // explicit initialization of objects
  PORTABLE_FUNCTION PortableMDArray(T *data, IArray<D> extents, IArray<D> strides,
                                    size_type rank) noexcept
      : pdata_(data), nxs_(extents), strides_(strides), rank_(rank) {}

  // variadic ctor, dispatch to explicit constructor
  template <class... NXs, size_type N = sizeof...(NXs)>
  PORTABLE_FUNCTION PortableMDArray(T *p, NXs... nxs) noexcept {
    NewPortableMDArray(p, nxs...);
  } //: PortableMDArray(p, make_nxs_array(nxs...), make_strides_array<N>(), N) {
  //}

  // ctors
  // default ctor: simply set null PortableMDArray
  PORTABLE_FUNCTION
  PortableMDArray() noexcept : pdata_(nullptr), nxs_{{0}}, rank_{0} {}
  // define copy constructor and overload assignment operator so both do deep
  // copies.
  //  PortableMDArray(const PortableMDArray<T> &t) noexcept;
  //  PortableMDArray<T> &operator=(const PortableMDArray<T> &t) noexcept;
  PortableMDArray(const this_type &src) noexcept
      : nxs_(src.nxs_), rank_(src.rank_), strides_(src.strides_),
        pdata_(src.pdata_ ? src.pdata_ : nullptr) {}

  PortableMDArray<T, D> &operator=(const this_type &src) noexcept {
    if (this != &src) {
      nxs_ = src.nxs_;
      rank_ = src.rank_;
      pdata_ = src.pdata_;
    }
    return *this;
  }

  template <typename... NXs>
  PORTABLE_FUNCTION void NewPortableMDArray(T *data, NXs... nxs) noexcept {
    pdata_ = data;
    update_layout(nxs...);
  }
  //----------------------------------------------------------------------------------------
  //! \fn PortableMDArray::SwapPortableMDArray()
  //  \brief  swap pdata_ pointers of two equally sized PortableMDArrays
  //  (shallow swap)
  // Does not allocate memory for either array

  void SwapPortableMDArray(this_type &array2) {
    std::swap(pdata_, array2.pdata_);
    return;
  }

  // functions to get array dimensions
  template <std::size_t I>
  PORTABLE_FUNCTION constexpr auto GetDim() const {
    return nxs_[rank_ - I];
  }

  // legacy API, TODO: deprecate
  [[deprecated("Use GetDim<N> instead.")]] PORTABLE_FORCEINLINE_FUNCTION int
  GetDim1() const {
    return GetDim<1>();
  }
  [[deprecated("Use GetDim<N> instead.")]] PORTABLE_FORCEINLINE_FUNCTION int
  GetDim2() const {
    return GetDim<2>();
  }
  [[deprecated("Use GetDim<N> instead.")]] PORTABLE_FORCEINLINE_FUNCTION int
  GetDim3() const {
    return GetDim<3>();
  }
  [[deprecated("Use GetDim<N> instead.")]] PORTABLE_FORCEINLINE_FUNCTION int
  GetDim4() const {
    return GetDim<4>();
  }
  [[deprecated("Use GetDim<N> instead.")]] PORTABLE_FORCEINLINE_FUNCTION int
  GetDim5() const {
    return GetDim<5>();
  }
  [[deprecated("Use GetDim<N> instead.")]] PORTABLE_FORCEINLINE_FUNCTION int
  GetDim6() const {
    return GetDim<6>();
  }
  PORTABLE_INLINE_FUNCTION int GetDim(size_t i) const {
    // TODO: remove if performance cirtical
    assert(0 < i && i <= 6 && "PortableMDArrays are max 6D");
    switch (i) {
    case 1:
      return GetDim<1>();
    case 2:
      return GetDim<2>();
    case 3:
      return GetDim<3>();
    case 4:
      return GetDim<4>();
    case 5:
      return GetDim<5>();
    case 6:
      return GetDim<6>();
    }
    return -1;
  }

  PORTABLE_FORCEINLINE_FUNCTION int GetSize() const noexcept {
    return util::array_reduce(nxs_, 1, std::multiplies<size_type>{});
  }
  PORTABLE_FORCEINLINE_FUNCTION size_type GetSizeInBytes() const noexcept {
    return GetSize() * sizeof(T);
  }

  PORTABLE_INLINE_FUNCTION size_type GetRank() const noexcept { return rank_; }
  template <typename... NXs>
  PORTABLE_INLINE_FUNCTION void Reshape(NXs... nxs) {
    assert(util::array_reduce(IArray<D>{nxs...}, 1, std::multiplies<size_type>{}) ==
               GetSize() &&
           "New shape gives different size than existing size");
    update_layout(nxs...);
  }

  PORTABLE_FORCEINLINE_FUNCTION bool IsShallowSlice() const noexcept { return true; }
  PORTABLE_FORCEINLINE_FUNCTION bool IsEmpty() const noexcept { return GetSize() < 1; }
  // "getter" function to access private data member
  // TODO(felker): Replace this unrestricted "getter" with a limited, safer
  // alternative.
  // TODO(felker): Rename function. Conflicts with "PortableMDArray<> data"
  // OutputData member.
  PORTABLE_FORCEINLINE_FUNCTION pointer data() noexcept { return pdata_; }
  PORTABLE_FORCEINLINE_FUNCTION const_pointer data() const noexcept { return pdata_; }
  PORTABLE_FORCEINLINE_FUNCTION pointer begin() noexcept { return pdata_; }
  PORTABLE_FORCEINLINE_FUNCTION pointer end() noexcept { return pdata_ + GetSize(); }

  // overload "function call" operator() to access 1d-5d data
  // provides Fortran-like syntax for multidimensional arrays vs. "subscript"
  // operator[]

  // "non-const variants" called for "PortableMDArray<T>()" provide read/write
  // access via returning by reference, enabling assignment on returned
  // l-value, e.g.: a(3) = 3.0;
  template <typename... Is> //, class = std::enable_if_t<(sizeof...(Is) > 3)>>
  PORTABLE_FORCEINLINE_FUNCTION constexpr reference operator()(Is... idxs) noexcept {
    return pdata_[util::fast_findex({static_cast<size_type>(idxs)...}, nxs_, strides_)];
  }

  template <typename... Is> //, class = std::enable_if_t<(sizeof...(Is) > 3)>>
  PORTABLE_FORCEINLINE_FUNCTION constexpr T &operator()(Is... idxs) const noexcept {
    return pdata_[util::fast_findex({static_cast<size_type>(idxs)...}, nxs_, strides_)];
  }

  this_type &operator*=(T scale) {
    std::transform(pdata_, pdata_ + GetSize(), pdata_,
                   [scale](T val) { return scale * val; });
    return *this;
  }

  this_type &operator+=(const this_type &other) {
    assert(GetSize() == other.GetSize());
    std::transform(pdata_, pdata_ + GetSize(), other.pdata_, pdata_, std::plus<T>());
    return *this;
  }

  this_type &operator-=(const this_type &other) {
    assert(GetSize() == other.GetSize());
    std::transform(pdata_, pdata_ + GetSize(), other.pdata_, pdata_, std::minus<T>());
    return *this;
  }

  // Checks that arrays point to same data with same shape
  // note this POINTER equivalence, not data equivalence
  bool operator==(const this_type &rhs) const {
    return (pdata_ == rhs.pdata_ && nxs_ == rhs.nxs_); // NB rank is implied
  }

  bool operator!=(const this_type &other) const { return !(*this == other); }

  //----------------------------------------------------------------------------------------
  //! \fn PortableMDArray::InitWithShallowSlice()
  //  \brief shallow copy of nvar elements in dimension dim of an array,
  //  starting at index=indx. Copies pointer to data, but not data itself.

  //  Shallow slice is only able to address the "nvar" range in "dim", and all
  //  entries of the src array for d<dim (cannot access any nx4=2, etc. entries
  //  if dim=3 for example)

  PORTABLE_FUNCTION void InitWithShallowSlice(const this_type &src, const int dim,
                                              const int indx, const int nvar) {
    pdata_ = src.pdata_;
    std::size_t offs = indx;
    nxs_[dim - 1] = nvar;

    for (std::size_t i = 0; i < dim - 1; ++i) {
      nxs_[i] = src.nxs_[i];
      offs *= nxs_[i];
    }
    for (std::size_t i = dim; i < MAXDIM; ++i) {
      nxs_[i] = 1;
    }
    pdata_ += offs;

    return;
  }

 private:
  // driver of nx array creation.
  // Note we iterate from the RIGHT, to match
  // what was done explicilt prior.
  template <typename... NXs, auto N = sizeof...(NXs)>
  PORTABLE_INLINE_FUNCTION constexpr auto update_layout(NXs... nxs) noexcept {

    rank_ = N;
    // NB: the functions here have trouble with template argument deduction
    // on some compilers, so they are hand-written.
    auto szs = util::wrap_vars<IArray<N>>(nxs...);
    nxs_ = util::make_underfilled_array<D, 1, Array, size_type, N>(szs);
    auto sds = util::get_strides(nxs_);
    strides_ = util::make_underfilled_array<D, 0, Array, size_type, D>(sds);
  }

  T *pdata_;
  IArray<D> nxs_;
  IArray<D> strides_;
  int rank_;
};

} // namespace PortsOfCall
#endif // _PORTABLE_ARRAYS_HPP_
