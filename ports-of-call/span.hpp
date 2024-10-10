#ifndef PORTS_OF_CALL_SPAN_HH_
#define PORTS_OF_CALL_SPAN_HH_

// ========================================================================================
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
// ========================================================================================

#include <array>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <type_traits>

#if defined(__cpp_lib_span) // do we already have std::span?

#include <span>
namespace PortsOfCall::span {

using std::span;

} // namespace PortsOfCall::span

#else

#define span_REQUIRES(...) typename std::enable_if<((__VA_ARGS__)), int>::type = 0

#define span_EXPECTS(...) assert((__VA_ARGS__))

namespace PortsOfCall::span {

using std::byte;
using std::void_t;

// set dynamic_extent to maximum integer size
constexpr std::size_t dynamic_extent = SIZE_MAX;

// forward declare
template <class ElementType, std::size_t Extent = dynamic_extent>
class span;

namespace detail {

using std::data;
using std::size;

// object to handle storage of span
template <class E, std::size_t S>
struct span_storage {
  constexpr span_storage() noexcept = default;

  constexpr span_storage(E *p_ptr, std::size_t /*unused*/) noexcept : ptr(p_ptr) {}

  E *ptr = nullptr;
  static constexpr std::size_t size = S;
};

// specialization for dynamic_extent
template <class E>
struct span_storage<E, dynamic_extent> {
  constexpr span_storage() noexcept = default;

  constexpr span_storage(E *p_ptr, std::size_t p_size) noexcept
      : ptr(p_ptr), size(p_size) {}

  E *ptr = nullptr;
  std::size_t size = 0;
};

// some metaprogramming
template <class T>
using uncvref_t = typename std::remove_cv<typename std::remove_reference<T>::type>::type;

// detector of span
template <class Q>
struct is_span_oracle : std::false_type {};

template <class T, std::size_t Extent>
struct is_span_oracle<span<T, Extent>> : std::true_type {};

template <class Q>
struct is_span : is_span_oracle<typename std::remove_cv<Q>::type> {};

template <class Q>
struct is_std_array_oracle : std::false_type {};

template <class T, std::size_t Extent>
struct is_std_array_oracle<std::array<T, Extent>> : std::true_type {};

template <class Q>
struct is_std_array : is_std_array_oracle<typename std::remove_cv<Q>::type> {};

template <class Q>
struct is_array : std::false_type {};

template <class T>
struct is_array<T[]> : std::true_type {};

template <class T, std::size_t N>
struct is_array<T[N]> : std::true_type {};

// detect C.size() and C.data()
template <class, class = void>
struct has_size_and_data : std::false_type {};

template <class C>
struct has_size_and_data<C, std::void_t<decltype(std::size(std::declval<C>())),
                                        decltype(std::data(std::declval<C>()))>>
    : std::true_type {};

// detect if *(C.data()) is compatible with E
template <class, class, class = void>
struct is_compatible_element : std::false_type {};

template <class C, class E>
struct is_compatible_element<C, E, std::void_t<decltype(std::data(std::declval<C>()))>>
    : std::is_convertible<typename std::remove_pointer<decltype(std::data(
                              std::declval<C &>()))>::type (*)[],
                          E (*)[]> {};

// A container C:
// - is not any of [span, c-array, std::array]
// - has C.size() and C.data() interface
template <class C>
struct is_container
    : std::bool_constant<!is_span<C>::value && !is_array<C>::value &&
                         !is_std_array<C>::value && has_size_and_data<C>::value> {};

// detect if C is a container and is compatible with element E
template <class C, class E>
struct is_compatible_container
    : std::bool_constant<is_container<C>::value && is_compatible_element<C, E>::value> {};

// detect if T is a complete type
// NB: sizeof(T) cannot be used with incomplete types
template <class, class = std::size_t>
struct is_complete : std::false_type {};

template <class T>
struct is_complete<T, decltype(sizeof(T))> : std::true_type {};

} // namespace detail

template <class ElementType, std::size_t Extent>
class span {
  static_assert(std::is_object<ElementType>::value,
                "A span's ElementType must be an object type (not a "
                "reference type or void)");
  static_assert(detail::is_complete<ElementType>::value,
                "A span's ElementType must be a complete type (not a forward "
                "declaration)");
  static_assert(!std::is_abstract<ElementType>::value,
                "A span's ElementType cannot be an abstract class type");

  using storage_type = detail::span_storage<ElementType, Extent>;

 public:
  // constants and types
  using element_type = ElementType;
  using value_type = typename std::remove_cv<ElementType>::type;
  using size_type = std::size_t;
  using difference_type = std::ptrdiff_t;
  using pointer = element_type *;
  using const_pointer = const element_type *;
  using reference = element_type &;
  using const_reference = const element_type &;
  using iterator = pointer;
  using reverse_iterator = std::reverse_iterator<iterator>;

  static constexpr size_type extent = Extent;

  // [span.cons], span constructors, copy, assignment, and destructor
  //

  // constructs an empty span
  // - data() == nullptr
  // - size() == 0
  template <std::size_t E = Extent,
            // span_REQUIRES(E == dynamic_extent || E <= 0 ) >
            typename std::enable_if<(E == dynamic_extent || E <= 0), int>::type = 0>
  constexpr span() noexcept {}

  // constructs a span that is a view over the range [first, first+count)
  // - data() == std::to_address(first)
  // - size() == count
  // explicit: extent != dynamic_extent
  // NB: iterator concepts to further restrict overload resolution
  constexpr span(pointer ptr, size_type count) : storage_(ptr, count) {
    span_EXPECTS((ptr == nullptr && count == 0) || (ptr != nullptr && count >= 0));
  }

  // constructs a span that is a view over the range [first, last)
  // - data() == std::to_address(first)
  // - size() == last - first
  // explicit: extent != dynamic_extent
  // NB: iterator concepts to further restrict overload resolution
  constexpr span(pointer first_elem, pointer last_elem)
      : storage_(first_elem, last_elem - first_elem) {
    span_EXPECTS(last_elem - first_elem >= 0);
  }

  // constructs a span that is a view over arr
  // - data() == std::to_data(arr)
  // - size() == N
  template <std::size_t N, std::size_t E = Extent,
            span_REQUIRES(
                (E == dynamic_extent || N == E) &&
                detail::is_compatible_container<element_type (&)[N], ElementType>::value)>
  // typename std::enable_if<(E == dynamic_extent || N == E) &&
  //                             detail::is_compatible_container<
  //                                 element_type (&)[N], ElementType>::value,
  //                         int>::type = 0>
  constexpr span(element_type (&arr)[N]) noexcept : storage_(arr, N) {}

  template <class T, std::size_t N, std::size_t E = Extent,
            span_REQUIRES(
                (E == dynamic_extent || N == E) &&
                detail::is_compatible_container<std::array<T, N> &, ElementType>::value)>

  // typename std::enable_if<(E == dynamic_extent || N == E) &&
  //                             detail::is_compatible_container<
  //                                 std::array<T, N> &, ElementType>::value,
  //                         int>::type = 0>
  constexpr span(std::array<T, N> &arr) noexcept : storage_(arr.data(), N) {}

  template <class T, std::size_t N, std::size_t E = Extent,
            span_REQUIRES((E == dynamic_extent || N == E) &&
                          detail::is_compatible_container<const std::array<T, N> &,
                                                          ElementType>::value)>
  // typename std::enable_if<(E == dynamic_extent || N == E) &&
  //                             detail::is_compatible_container<
  //                                 const std::array<T, N> &, ElementType>::value,
  //                         int>::type = 0>
  constexpr span(const std::array<T, N> &arr) noexcept : storage_(arr.data(), N) {}

  // constructs a span that is a view of cont
  // - data() == std::data(cont)
  // - size() == std::size(cont)
  // NB: C++20 replaces Container concept with Range concept.
  // NB: CompatibleContainer(or future Range) concept is exclusive with C-array,
  // std::array, and span
  template <
      class Container, std::size_t E = Extent,
      span_REQUIRES(E == dynamic_extent && detail::is_container<Container>::value &&
                    detail::is_compatible_container<Container &, ElementType>::value)>
  // typename std::enable_if<
  //     E == dynamic_extent && detail::is_container<Container>::value &&
  //         detail::is_compatible_container<Container &, ElementType>::value,
  //     int>::type = 0>
  constexpr span(Container &cont) noexcept
      : storage_(detail::data(cont), detail::size(cont)) {}

  template <class Container, std::size_t E = Extent,
            span_REQUIRES(
                E == dynamic_extent && detail::is_container<Container>::value &&
                detail::is_compatible_container<const Container &, ElementType>::value)>
  // typename std::enable_if<
  //     E == dynamic_extent && detail::is_container<Container>::value &&
  //         detail::is_compatible_container<const Container &, ElementType>::value,
  //     int>::type = 0>
  constexpr span(const Container &cont) noexcept
      : storage_(detail::data(cont), detail::size(cont)) {}

  constexpr span(const span &other) noexcept = default;

  template <class OtherElementType, std::size_t OtherExtent,
            span_REQUIRES(
                (Extent == dynamic_extent || OtherExtent == dynamic_extent ||
                 Extent == OtherExtent) &&
                std::is_convertible<OtherElementType (*)[], ElementType (*)[]>::value)>
  // typename std::enable_if<
  //     (Extent == dynamic_extent || OtherExtent == dynamic_extent ||
  //      Extent == OtherExtent) &&
  //         std::is_convertible<OtherElementType (*)[], ElementType (*)[]>::value,
  //     int>::type = 0>
  constexpr span(const span<OtherElementType, OtherExtent> &other) noexcept
      : storage_(other.data(), other.size()) {
    span_EXPECTS(OtherExtent == dynamic_extent || other.size() == OtherExtent);
  }

  ~span() noexcept = default;

  constexpr span &operator=(const span &other) noexcept = default;

  // [span.subviews]--
  // obtains a span that is a view over the first Count elements of this span
  // https://en.cppreference.com/w/cpp/container/span/first
  template <std::size_t Count>
  constexpr span<element_type, Count> first() const {
    span_EXPECTS(Count >= 0 && Count <= size());
    return {data(), Count};
  }

  constexpr span<element_type, dynamic_extent> first(size_type count) const {
    span_EXPECTS(count >= 0 && count <= size());
    return {data(), count};
  }

  // obtains a span that is a view over the last Count elements of this span
  // https://en.cppreference.com/w/cpp/container/span/last
  template <std::size_t Count>
  constexpr span<element_type, Count> last() const {
    span_EXPECTS(Count >= 0 && Count <= size());
    return {data() + (size() - Count), Count};
  }

  constexpr span<element_type, dynamic_extent> last(size_type count) const {
    span_EXPECTS(count >= 0 && count <= size());
    return {data() + (size() - count), count};
  }

  // obtains a span that is a view over Count elements of this span starting at Offset
  // https://en.cppreference.com/w/cpp/container/span/subspan
  template <std::size_t Offset, std::size_t Count = dynamic_extent>
  using subspan_return_t =
      span<ElementType,
           Count != dynamic_extent
               ? Count
               : (Extent != dynamic_extent ? Extent - Offset : dynamic_extent)>;

  template <std::size_t Offset, std::size_t Count = dynamic_extent>
  constexpr subspan_return_t<Offset, Count> subspan() const {
    span_EXPECTS((Offset >= 0 && Offset <= size()) &&
                 (Count == dynamic_extent || (Count >= 0 && Count + Offset <= size())));
    return {data() + Offset, Count != dynamic_extent ? Count : size() - Offset};
  }

  constexpr span<element_type, dynamic_extent>
  subspan(size_type offset, size_type count = dynamic_extent) const {
    span_EXPECTS((offset >= 0 && offset <= size()) &&
                 (count == static_cast<size_type>(dynamic_extent) ||
                  (count >= 0 && count + offset <= size())));

    return {data() + offset, count == dynamic_extent ? size() - offset : count};
  }
  // --[span.subviews]

  // [span.observers]--

  // returns the number of elements in the span
  constexpr size_type size() const noexcept { return storage_.size; }

  // returns the size of sequence in bytes
  constexpr size_type size_bytes() const noexcept {
    return size() * sizeof(element_type);
  }

  // checks if sequence is empty()
  [[nodiscard]] constexpr bool empty() const noexcept { return size() == 0; }

  // --[span.observers]

  // [span.element_access]--
  constexpr reference operator[](size_type idx) const {
    span_EXPECTS(idx >= 0 && idx < size());
    return *(data() + idx);
  }

  constexpr reference front() const {
    span_EXPECTS(!empty());
    return *data();
  }

  constexpr reference back() const {
    span_EXPECTS(!empty());
    return *(data() + (size() - 1));
  }

  constexpr pointer data() const noexcept { return storage_.ptr; }
  // --[span.element_access]

  // [span.iterators]--
  constexpr iterator begin() const noexcept { return data(); }

  constexpr iterator end() const noexcept { return data() + size(); }

  constexpr reverse_iterator rbegin() const noexcept { return reverse_iterator(end()); }

  constexpr reverse_iterator rend() const noexcept { return reverse_iterator(begin()); }
  // --[span.iterators]

 private:
  storage_type storage_{};
};

// deduction guides
// https://en.cppreference.com/w/cpp/container/span/deduction_guides
template <class T, size_t N>
span(T (&)[N]) -> span<T, N>;

template <class T, size_t N>
span(std::array<T, N> &) -> span<T, N>;

template <class T, size_t N>
span(const std::array<T, N> &) -> span<const T, N>;

template <class Container>
span(Container &) -> span<typename std::remove_reference<
                      decltype(*detail::data(std::declval<Container &>()))>::type>;

template <class Container>
span(const Container &) -> span<const typename Container::value_type>;

template <class ElementType, std::size_t Extent>
constexpr span<ElementType, Extent> make_span(span<ElementType, Extent> s) noexcept {
  return s;
}

template <class T, std::size_t N>
constexpr span<T, N> make_span(T (&arr)[N]) noexcept {
  return {arr};
}

template <class T, std::size_t N>
constexpr span<T, N> make_span(std::array<T, N> &arr) noexcept {
  return {arr};
}

template <class T, std::size_t N>
constexpr span<const T, N> make_span(const std::array<T, N> &arr) noexcept {
  return {arr};
}

template <class Container>
constexpr span<typename std::remove_reference<
    decltype(*detail::data(std::declval<Container &>()))>::type>
make_span(Container &cont) {
  return {cont};
}

template <class Container>
constexpr span<const typename Container::value_type> make_span(const Container &cont) {
  return {cont};
}

// as_bytes, as_writable_bytes
// obtains a view to the elements of span s
// https://en.cppreference.com/w/cpp/container/span/as_bytes
template <class ElementType, std::size_t Extent>
span<const byte,
     ((Extent == dynamic_extent) ? dynamic_extent : sizeof(ElementType) * Extent)>
as_bytes(span<ElementType, Extent> s) noexcept {
  return {reinterpret_cast<const byte *>(s.data()), s.size_bytes()};
}

template <class ElementType, size_t Extent,
          span_REQUIRES(!std::is_const<ElementType>::value)>
//          typename std::enable_if<!std::is_const<ElementType>::value, int>::type = 0>
span<byte, ((Extent == dynamic_extent) ? dynamic_extent : sizeof(ElementType) * Extent)>
as_writable_bytes(span<ElementType, Extent> s) noexcept {
  return {reinterpret_cast<byte *>(s.data()), s.size_bytes()};
}

template <std::size_t N, class E, std::size_t S>
constexpr auto get(span<E, S> s) -> decltype(s[N]) {
  return s[N];
}

} // namespace PortsOfCall::span

namespace std {

template <class ElementType, size_t Extent>
class tuple_size<PortsOfCall::span::span<ElementType, Extent>>
    : public integral_constant<size_t, Extent> {};

template <class ElementType>
class tuple_size<
    PortsOfCall::span::span<ElementType, PortsOfCall::span::dynamic_extent>>; // not
                                                                              // defined

template <size_t I, class ElementType, size_t Extent>
class tuple_element<I, PortsOfCall::span::span<ElementType, Extent>> {
 public:
  static_assert(Extent != PortsOfCall::span::dynamic_extent && I < Extent, "");
  using type = ElementType;
};

} // end namespace std

#endif // __cpp_lib_span
#endif // PORTS_OF_CALL_SPAN_HH_
