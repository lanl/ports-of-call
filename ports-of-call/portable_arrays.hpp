#ifndef _PORTABLE_ARRAYS_HPP_
#define _PORTABLE_ARRAYS_HPP_
//========================================================================================
// Portable MDAarray, based on AthenaArray in Athena++
// Copyright(C) 2014 James M. Stone <jmstone@princeton.edu> and other code
// contributors Licensed under the 3-clause BSD License, see LICENSE file for
// details
//
// © (or copyright) 2019-2021. Triad National Security, LLC. All rights
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

#include "portability.hpp"
#include <algorithm>
#include <array>
#include <assert.h>
#include <cstddef> // size_t
#include <cstring> // memset()
#include <functional>
#include <numeric>
#include <type_traits>
#include <utility> // swap()

constexpr std::size_t MAXDIM = 6;
using narr = std::array<std::size_t, MAXDIM>;
namespace detail {

template <std::size_t Ind, typename NX>
constexpr void set_value(narr &ndat, NX value) {
  ndat[Ind] = value;
}

template <std::size_t Ind, typename NX>
constexpr void set_values(narr &ndat, NX value) {
  set_value<Ind>(ndat, value);
}

template <std::size_t Ind, typename NX, typename... NXs>
constexpr void set_values(narr &ndat, NX value, NXs... nxs) {
  set_value<Ind>(ndat, value);
  set_values<Ind - 1>(ndat, nxs...);
}

template <std::size_t Ind>
size_t computeIndex(const narr &nd, const size_t index) {
  return index;
}
template <std::size_t Ind, typename... Tail>
size_t computeIndex(const narr &nd, const size_t index, const Tail... tail) {
  return index * nd[Ind] + computeIndex<Ind + 1>(nd, tail...);
}

template <typename NX>
PORTABLE_INLINE_FUNCTION auto varmul(NX v) {
  return v;
}

template <typename NX, typename... NXs>
PORTABLE_INLINE_FUNCTION auto varmul(NX v, NXs... vs) {
  return v * varmul(vs...);
}

} // namespace detail

template <typename... NXs>
PORTABLE_INLINE_FUNCTION auto nx_arr(NXs... nxs) {
  narr r;
  constexpr std::size_t NP = sizeof...(NXs);
  detail::set_values<NP - 1>(r, nxs...);
  for (auto i = NP; i < MAXDIM; ++i)
    r[i] = 1;
  return r;
}

template <typename... Indicies>
size_t compute_index(const narr &nd, const Indicies... idxs) {
  return 0 + detail::computeIndex<0>(nd, idxs...);
}

template <typename... NXs>
PORTABLE_INLINE_FUNCTION auto nx_mul(NXs... nxs) {
  return detail::varmul(nxs...);
}

template <typename T>
class PortableMDArray {
 public:
  PORTABLE_FUNCTION PortableMDArray(T *data, narr nxa, std::size_t rank)
      : pdata_(data), nxs_(nxa), rank_(rank) {}
  // rank_{std::distance(nxs_.cbegin(), std::find(nxs_.cbegin(), nxs_.cend(),
  // 1))} {}

  template <typename... NXs>
  PORTABLE_FUNCTION PortableMDArray(T *p, NXs... nxs) noexcept
      : PortableMDArray(p, nx_arr(nxs...), sizeof...(NXs)) {}

  // ctors
  // default ctor: simply set null PortableMDArray
  PORTABLE_FUNCTION
  PortableMDArray() noexcept : pdata_(nullptr), nxs_{{0}}, rank_{0} {}

  // define copy constructor and overload assignment operator so both do deep
  // copies.
  PortableMDArray(const PortableMDArray<T> &t) noexcept;
  PortableMDArray<T> &operator=(const PortableMDArray<T> &t) noexcept;

  template <typename... NXs>
  PORTABLE_FUNCTION void NewPortableMDArray(T *data, NXs... nxs) noexcept {
    pdata_ = data;
    nxs_ = nx_arr(nxs...);
    rank_ = sizeof...(NXs);
  }
  // public function to swap underlying data pointers of two equally-sized
  // arrays
  void SwapPortableMDArray(PortableMDArray<T> &array2);

  // functions to get array dimensions
  template <std::size_t I>
  PORTABLE_FUNCTION constexpr auto GetDim() const {
    return nxs_[I];
  }
  // PORTABLE_FUNCTION constexpr GetSize() const { return computeSize()}

  // legacy API, TODO: deprecate
  PORTABLE_FORCEINLINE_FUNCTION int GetDim1() const { return GetDim<0>(); }
  PORTABLE_FORCEINLINE_FUNCTION int GetDim2() const { return GetDim<1>(); }
  PORTABLE_FORCEINLINE_FUNCTION int GetDim3() const { return GetDim<2>(); }
  PORTABLE_FORCEINLINE_FUNCTION int GetDim4() const { return GetDim<3>(); }
  PORTABLE_FORCEINLINE_FUNCTION int GetDim5() const { return GetDim<4>(); }
  PORTABLE_FORCEINLINE_FUNCTION int GetDim6() const { return GetDim<5>(); }
  PORTABLE_INLINE_FUNCTION int GetDim(size_t i) const {
    // TODO: remove if performance cirtical
    assert(0 < i && i <= 6 && "PortableMDArrays are max 6D");
    switch (i) {
    case 1:
      return GetDim1();
    case 2:
      return GetDim2();
    case 3:
      return GetDim3();
    case 4:
      return GetDim4();
    case 5:
      return GetDim5();
    case 6:
      return GetDim6();
    }
    return -1;
  }

  PORTABLE_FORCEINLINE_FUNCTION int GetSize() const {
    return std::accumulate(nxs_.cbegin(), nxs_.cend(), 1,
                           std::multiplies<std::size_t>());
  }
  PORTABLE_FORCEINLINE_FUNCTION std::size_t GetSizeInBytes() const {
    return GetSize() * sizeof(T);
  }

  PORTABLE_INLINE_FUNCTION size_t GetRank() const { return rank_; }
  template <typename... NXs>
  PORTABLE_INLINE_FUNCTION void Reshape(NXs... nxs) {
    assert(nx_mul(nxs...) == GetSize());
    nxs_ = nx_arr(nxs...);
    rank_ = sizeof...(NXs);
  }
  PORTABLE_FORCEINLINE_FUNCTION bool IsShallowSlice() { return true; }
  PORTABLE_FORCEINLINE_FUNCTION bool IsEmpty() { return GetSize() < 1; }
  // "getter" function to access private data member
  // TODO(felker): Replace this unrestricted "getter" with a limited, safer
  // alternative.
  // TODO(felker): Rename function. Conflicts with "PortableMDArray<> data"
  // OutputData member.
  PORTABLE_FORCEINLINE_FUNCTION T *data() { return pdata_; }
  PORTABLE_FORCEINLINE_FUNCTION const T *data() const { return pdata_; }
  PORTABLE_FORCEINLINE_FUNCTION T *begin() { return pdata_; }
  PORTABLE_FORCEINLINE_FUNCTION T *end() { return pdata_ + GetSize(); }

  // overload "function call" operator() to access 1d-5d data
  // provides Fortran-like syntax for multidimensional arrays vs. "subscript"
  // operator[]

  // "non-const variants" called for "PortableMDArray<T>()" provide read/write
  // access via returning by reference, enabling assignment on returned
  // l-value, e.g.: a(3) = 3.0;
  template <typename... Is>
  PORTABLE_FORCEINLINE_FUNCTION T &operator()(const Is... idxs) {
    return pdata_[compute_index(nxs_, idxs...)];
  }

  template <typename... Is>
  PORTABLE_FORCEINLINE_FUNCTION T &operator()(const Is... idxs) const {
    return pdata_[compute_index(nxs_, idxs...)];
  }

  PortableMDArray<T> &operator*=(T scale) {
    std::transform(pdata_, pdata_ + GetSize(), pdata_,
                   [scale](T val) { return scale * val; });
    return *this;
  }

  PortableMDArray<T> &operator+=(const PortableMDArray<T> &other) {
    assert(GetSize() == other.GetSize());
    std::transform(pdata_, pdata_ + GetSize(), other.pdata_, pdata_,
                   std::plus<T>());
    return *this;
  }

  PortableMDArray<T> &operator-=(const PortableMDArray<T> &other) {
    assert(GetSize() == other.GetSize());
    std::transform(pdata_, pdata_ + GetSize(), other.pdata_, pdata_,
                   std::minus<T>());
    return *this;
  }

  // Checks that arrays point to same data with same shape
  // note this POINTER equivalence, not data equivalence
  bool operator==(const PortableMDArray<T> &other) const;
  bool operator!=(const PortableMDArray<T> &other) const {
    return !(*this == other);
  }

  // (deferred) initialize an array with slice from another array
  PORTABLE_FUNCTION
  void InitWithShallowSlice(const PortableMDArray<T> &src, const int dim,
                            const int indx, const int nvar);

 private:
  T *pdata_;
  narr nxs_;
  int rank_;
};

// copy constructor (does a shallow copy)

template <typename T>
PortableMDArray<T>::PortableMDArray(const PortableMDArray<T> &src) noexcept {
  nxs_ = src.nxs_;
  rank_ = src.rank_;
  if (src.pdata_) pdata_ = src.pdata_;
}

// shallow copy assignment operator

template <typename T>
PortableMDArray<T> &
PortableMDArray<T>::operator=(const PortableMDArray<T> &src) noexcept {
  if (this != &src) {
    nxs_ = src.nxs;
    rank_ = src.rank_;
    pdata_ = src.pdata_;
  }
  return *this;
}

// Checks that arrays point to same data with same shape
// note this POINTER equivalence, not data equivalence
template <typename T>
bool PortableMDArray<T>::operator==(const PortableMDArray<T> &rhs) const {
  return (pdata_ == rhs.pdata_ && nxs_ == rhs.nxs_); // NB rank is implied
}

//----------------------------------------------------------------------------------------
//! \fn PortableMDArray::InitWithShallowSlice()
//  \brief shallow copy of nvar elements in dimension dim of an array,
//  starting at index=indx. Copies pointer to data, but not data itself.

//  Shallow slice is only able to address the "nvar" range in "dim", and all
//  entries of the src array for d<dim (cannot access any nx4=2, etc. entries
//  if dim=3 for example)

template <typename T>
PORTABLE_FUNCTION void
PortableMDArray<T>::InitWithShallowSlice(const PortableMDArray<T> &src,
                                         const int dim, const int indx,
                                         const int nvar) {
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

//----------------------------------------------------------------------------------------
//! \fn PortableMDArray::SwapPortableMDArray()
//  \brief  swap pdata_ pointers of two equally sized PortableMDArrays
//  (shallow swap)
// Does not allocate memory for either array

template <typename T>
void PortableMDArray<T>::SwapPortableMDArray(PortableMDArray<T> &array2) {
  std::swap(pdata_, array2.pdata_);
  return;
}

#endif // _PORTABLE_ARRAYS_HPP_
