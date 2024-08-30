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

#ifndef _PORTS_OF_CALL_ARRAY_HPP_
#define _PORTS_OF_CALL_ARRAY_HPP_

#include "portability.hpp"
#include "portable_errors.hpp"
#include "ports-of-call/utility/array_algo.hpp"

#include <cstddef>
#include <iterator>
#include <tuple>

namespace PortsOfCall {

//! The array class is a constexpr (mostly) replacement for std::array
/*!
std::array is not fully constexpr until C++20.  Currently we are relying on
"constexpr"-ness to offload to GPUs.  PortsOfCall::array provides a
constexpr replacement for std::array with the following notable exceptions:
    - the reverse iterators are not provided because std::reverse_iterator is
not constexpr until C++17 (could be provided with modest effort)

    - the `at` function is not provided because it bounds checks and throws an
exception which we cannot do on a GPU (it could be added and made
non-constexpr)

    - no std::get (cannot be provided without undefined behavior)
*/

namespace array_detail {

template <typename T, std::size_t N>
struct bits {
  using type = T[N];

  template <typename IntT>
  PORTABLE_FORCEINLINE_FUNCTION static constexpr T &ref_access(type &array, IntT index) {
    return array[index];
  }

  template <typename IntT>
  PORTABLE_FORCEINLINE_FUNCTION static constexpr T const &ref_access(type const &array,
                                                                     IntT index) {
    return array[index];
  }

  PORTABLE_FORCEINLINE_FUNCTION static constexpr T *ptr_access(type &array) {
    return static_cast<T *>(array);
  }

  PORTABLE_FORCEINLINE_FUNCTION static constexpr T const *ptr_access(type const &array) {
    return static_cast<T const *>(array);
  }
};

template <typename T>
struct bits<T, 0> {
  using type = T[1];

  template <typename IntT>
  PORTABLE_FORCEINLINE_FUNCTION static constexpr T &ref_access(type &arr, IntT) {
    return arr[0];
  }

  template <typename IntT>
  PORTABLE_FORCEINLINE_FUNCTION static constexpr T const &ref_access(type const &arr,
                                                                     IntT) {
    return arr[0];
  }

  PORTABLE_FORCEINLINE_FUNCTION static constexpr T *ptr_access(type &arr) {
    return (T *)&arr;
  }

  PORTABLE_FORCEINLINE_FUNCTION static constexpr T const *ptr_access(type const &arr) {
    return (T const *)&arr;
  }
};
} // namespace array_detail

template <typename T, std::size_t N>
struct array {
  // member types
  using value_type = T;
  using size_type = std::size_t;
  using difference_type = std::ptrdiff_t;
  using reference = T &;
  using const_reference = T const &;
  using pointer = value_type *;
  using const_pointer = value_type const *;
  using iterator = pointer;
  using const_iterator = const_pointer;

  //! The  array being wrapped by this class
  typename array_detail::bits<T, N>::type arr{};

  //! Index operator that returns a reference to the desired element.
  PORTABLE_FUNCTION constexpr reference operator[](size_type index) {
    return array_detail::bits<T, N>::ref_access(arr, index);
  }

  //! bounds-checked equivalent to index operator
  PORTABLE_FUNCTION constexpr reference at(size_type index) {
    if (index >= size()) {
      PORTABLE_ALWAYS_THROW_OR_ABORT("invalid index.");
    }
    return array_detail::bits<T, N>::ref_access(arr, index);
  }

  //! Index operator that returns a const reference to the desired element.
  PORTABLE_FUNCTION constexpr const_reference operator[](size_type index) const {
    return array_detail::bits<T, N>::ref_access(arr, index);
  }

  //! bounds-checked equivalent to index operator
  PORTABLE_FUNCTION constexpr const_reference at(size_type index) const {
    if (index >= size()) {
      PORTABLE_ALWAYS_THROW_OR_ABORT("invalid index.");
    }
    return array_detail::bits<T, N>::ref_access(arr, index);
  }

  //! access first element of array
  PORTABLE_FUNCTION constexpr reference front() {
    return array_detail::bits<T, N>::ref_access(arr, 0);
  }

  //! access first element of const array
  PORTABLE_FUNCTION constexpr const_reference front() const {
    return array_detail::bits<T, N>::ref_access(arr, 0);
  }

  //! access last element of array
  PORTABLE_FUNCTION constexpr reference back() {
    return N ? array_detail::bits<T, N>::ref_access(arr, N - 1)
             : array_detail::bits<T, N>::ref_access(arr, 0);
  }

  //! access first element of const array
  PORTABLE_FUNCTION constexpr const_reference back() const {
    return N ? array_detail::bits<T, N>::ref_access(arr, N - 1)
             : array_detail::bits<T, N>::ref_access(arr, 0);
  }

  //! get pointer to underlying data of array
  PORTABLE_FUNCTION constexpr pointer data() {
    return array_detail::bits<T, N>::ptr_access(arr);
  }

  //! get pointer to underlying data of const array
  PORTABLE_FUNCTION constexpr const_pointer data() const {
    return array_detail::bits<T, N>::ptr_access(arr);
  }

  //! Provides an iterator pointing to the beginning of the array.
  PORTABLE_FUNCTION constexpr iterator begin() { return iterator{data()}; }

  //! Provides a const iterator pointing to the beginning of the const array.
  PORTABLE_FUNCTION constexpr const_iterator begin() const {
    return const_iterator{data()};
  }

  //! Provides a const iterator pointing to the beginning of the const array.
  PORTABLE_FUNCTION constexpr const_iterator cbegin() const {
    return const_iterator{data()};
  }

  //! Provides an iterator pointing to the end of the array.
  PORTABLE_FUNCTION constexpr iterator end() { return iterator{data() + N}; }

  //! Provides a const iterator pointing to the end of the const array.
  PORTABLE_FUNCTION constexpr const_iterator end() const {
    return const_iterator{data() + N};
  }

  //! Provides a const iterator pointing to the end of the const array.
  PORTABLE_FUNCTION constexpr const_iterator cend() const {
    return const_iterator{data() + N};
  }

  //! test if array is empty, i.e., N==0
  PORTABLE_FUNCTION constexpr bool empty() const { return N == 0; }

  //! return the size of an array
  PORTABLE_FUNCTION constexpr size_type size() const { return N; }

  //! return the maximum size an array can hold
  PORTABLE_FUNCTION constexpr size_type max_size() const { return N; }

  //! fill the array with a single value
  PORTABLE_FUNCTION constexpr void fill(const_reference value) {
    for (auto &element : *this) {
      element = value;
    }
  }

  PORTABLE_FUNCTION constexpr bool operator==(const array &rhs) const {
    for (size_type i = 0; i < N; ++i)
      if (arr[i] != rhs.arr[i]) return false;
    return true;
  }

  //! swap the array contents with another
  PORTABLE_FUNCTION constexpr void swap(array &other) {
    using std::swap;
    for (size_type i = 0; i < N; ++i) {
      swap(arr[i], other.arr[i]);
    }
  }
};

//! specialization of swap for array
template <typename T, std::size_t N>
PORTABLE_FUNCTION constexpr void swap(array<T, N> &left, array<T, N> &right) {
  left.swap(right);
}

//! make_array function to return a deduced type array
// This function was proposed for standardization but superceded by
// C++17 automatic template parameter deduction.  It can still be
// useful at C++14 and earlier.
// * This implementation does not support the special handling of
//   of reference wrapper

namespace array_detail {

// determine the element type of the array
// * if explicitly specified D
// * if not explicitly specified std::common_type_t<ArgTs...>

template <typename D, typename... ArgTs>
struct array_element {
  using type = D;
};

template <typename... ArgTs>
struct array_element<void, ArgTs...> : std::common_type<ArgTs...> {};

} // namespace array_detail

template <typename D = void, typename... ArgTs>
PORTABLE_FUNCTION constexpr array<typename array_detail::array_element<D, ArgTs...>::type,
                                  sizeof...(ArgTs)>
make_array(ArgTs &&...args) {
  return {{std::forward<ArgTs>(args)...}};
}

} // end namespace PortsOfCall

// provide std specializations
namespace std {
/* libc++ and libstdc++ define tuple_size/tuple_element as a class and struct,
 * respectively.  To prevent clang from issuing a warning that these
 * customization points are mismatched, we are temporarily disabling that
 * diagnostic. */
#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wmismatched-tags"
#endif

template <typename T, std::size_t N>
struct tuple_size<PortsOfCall::array<T, N>>
    : public std::integral_constant<std::size_t, N> {};

template <std::size_t I, typename T, std::size_t N>
struct tuple_element<I, PortsOfCall::array<T, N>> {
 public:
  static_assert(I < N, "index must be less than number of elements");

  using type = T;
};

#if defined(__clang__)
#pragma clang diagnostic pop
#endif
} // namespace std

#endif // #ifndef _PORTS_OF_CALL_ARRAY_HPP_
