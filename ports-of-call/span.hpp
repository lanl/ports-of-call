#ifndef _PORTS_OF_CALL_SPAN_HPP_
#define _PORTS_OF_CALL_SPAN_HPP_

#include "ports-of-call/portability.hpp"

#include <cassert>
#include <cstddef>

namespace PortsOfCall {

// ================================================================================================
// Heavily simplified relative to std::span, but provides a similar concept in a way
// that's portable to GPUs.

template <typename T>
class span {
 private:
  T *ptr_{nullptr};
  std::size_t size_{0};

 public:
  // Construct an empty span
  PORTABLE_FUNCTION constexpr span() = default;

  // Construct a span from a pointer, along with offsets.
  // -- ptr   : Pointer to the beginning of the array
  // -- count : Number of entries in the span
  // For example:
  //      double* my_array = new double[10]
  //      span full_range(my_array, 10); // elements 0-9 (all 10 elements in the array)
  //      span sub_range(my_array+2, 6); // elements 2-7 (total of six elements)
  // The full_range object provides access to the full range of my_array.  The sub_range
  // object provides access to indices 2 <= i < 8 of the array my_array, but the indices
  // are shifted so that sub_range[i] is my_array[i+2].
  template <typename SizeType>
  PORTABLE_FUNCTION constexpr span(T *ptr, SizeType const &count)
      : ptr_{ptr}, size_(count) {
    assert(count >= 0);
  }

  // Query the size of the range
  PORTABLE_FUNCTION constexpr auto size() const { return size_; }

  // Iterator (really a pointer) to the beginning of the range, providing mutable access.
  PORTABLE_FUNCTION constexpr T *begin() { return ptr_; }

  // Iterator (really a pointer) to the beginning of the range, providing constant access.
  PORTABLE_FUNCTION constexpr T const *begin() const { return ptr_; }

  // Iterator (really a pointer) to the beginning of the range, providing constant access.
  PORTABLE_FUNCTION constexpr T const *cbegin() const { return ptr_; }

  // Iterator (really a pointer) to the end of the range, providing mutable access.
  PORTABLE_FUNCTION constexpr T *end() { return ptr_ + size_; }

  // Iterator (really a pointer) to the beginning of the range, providing constant access.
  PORTABLE_FUNCTION constexpr T const *end() const { return ptr_ + size_; }

  // Iterator (really a pointer) to the beginning of the range, providing constant access.
  PORTABLE_FUNCTION constexpr T const *cend() const { return ptr_ + size_; }

  // Index operator to obtain mutable access to an element of the range.
  template <typename Index>
  PORTABLE_FUNCTION constexpr T &operator[](Index const &index) {
    assert(index >= 0);
    assert(static_cast<std::size_t>(index) < size_);
    return *(ptr_ + index);
  }

  // Index operator to obtain constant access to an element of the range.
  template <typename Index>
  PORTABLE_FUNCTION constexpr T const &operator[](Index const &index) const {
    assert(index >= static_cast<Index>(0));
    assert(index < static_cast<Index>(size_));
    return *(ptr_ + index);
  }
};

// ================================================================================================

template <typename T, typename SizeType>
PORTABLE_FUNCTION constexpr auto make_span(T *const pointer,
                                           SizeType const count) {
  return span<T>(pointer, count);
}

// ================================================================================================

} // end namespace PortsOfCall

#endif // #ifndef _PORTS_OF_CALL_SPAN_HPP_
