#ifndef PORTS_OF_CALL_SPAN_HH_
#define PORTS_OF_CALL_SPAN_HH_

#include <cstdio>
#include <stdexcept>

#include <array>
#include <cstddef>
#include <cstdint>
#include <type_traits>

namespace PortsOfCall::span {

using std::byte;
using std::void_t;

// set dynamic_extent to maximum integer size
constexpr std::size_t dynamic_extent = SIZE_MAX;

// forward declare
template <typename ElementType, std::size_t Extent = dynamic_extent>
class span;

namespace detail {

using std::data;
using std::size;

// object to handle storage of span
template <typename E, std::size_t S>
struct span_storage {
  constexpr span_storage() noexcept = default;

  constexpr span_storage(E *p_ptr, std::size_t /*unused*/) noexcept : ptr(p_ptr) {}

  E *ptr = nullptr;
  static constexpr std::size_t size = S;
};

// specialization for dynamic_extent
template <typename E>
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

template <class>
struct is_span : std::false_type {};

template <class T, std::size_t S>
struct is_span<span<T, S>> : std::true_type {};

template <class>
struct is_std_array : std::false_type {};

template <class T, std::size_t N>
struct is_std_array<std::array<T, N>> : std::true_type {};

template <class, typename = void>
struct has_size_and_data : std::false_type {};

template <class T>
struct has_size_and_data<T, void_t<decltype(detail::size(std::declval<T>())),
                                   decltype(detail::data(std::declval<T>()))>>
    : std::true_type {};

template <class C, typename U = uncvref_t<C>>
struct is_container {
  static constexpr bool value = !is_span<U>::value && !is_std_array<U>::value &&
                                !std::is_array<U>::value && has_size_and_data<C>::value;
};

template <class T>
using remove_pointer_t = class std::remove_pointer<T>::type;

template <class, class, class = void>
struct is_container_element_type_compatible : std::false_type {};

// checks to insure that container T and element E are compatible
template <class T, class E>
struct is_container_element_type_compatible<
    T, E,
    typename std::enable_if<!std::is_same<typename std::remove_cv<decltype(detail::data(
                                              std::declval<T>()))>::type,
                                          void>::value &&
                            std::is_convertible<remove_pointer_t<decltype(detail::data(
                                                    std::declval<T>()))> (*)[],
                                                E (*)[]>::value>::type> : std::true_type {
};

template <class, class = size_t>
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
  template <std::size_t E = Extent,
            typename std::enable_if<(E == dynamic_extent || E <= 0), int>::type = 0>
  constexpr span() noexcept {}

  constexpr span(pointer ptr, size_type count) : storage_(ptr, count) {}

  constexpr span(pointer first_elem, pointer last_elem)
      : storage_(first_elem, last_elem - first_elem) {
    TCB_SPAN_EXPECT(extent == dynamic_extent ||
                    last_elem - first_elem == static_cast<std::ptrdiff_t>(extent));
  }

  template <std::size_t N, std::size_t E = Extent,
            typename std::enable_if<(E == dynamic_extent || N == E) &&
                                        detail::is_container_element_type_compatible<
                                            element_type (&)[N], ElementType>::value,
                                    int>::type = 0>
  constexpr span(element_type (&arr)[N]) noexcept : storage_(arr, N) {}

  template <class T, std::size_t N, std::size_t E = Extent,
            typename std::enable_if<(E == dynamic_extent || N == E) &&
                                        detail::is_container_element_type_compatible<
                                            std::array<T, N> &, ElementType>::value,
                                    int>::type = 0>
  constexpr span(std::array<T, N> &arr) noexcept : storage_(arr.data(), N) {}

  template <class T, std::size_t N, std::size_t E = Extent,
            typename std::enable_if<(E == dynamic_extent || N == E) &&
                                        detail::is_container_element_type_compatible<
                                            const std::array<T, N> &, ElementType>::value,
                                    int>::type = 0>
  constexpr span(const std::array<T, N> &arr) noexcept : storage_(arr.data(), N) {}

  template <class Container, std::size_t E = Extent,
            typename std::enable_if<E == dynamic_extent &&
                                        detail::is_container<Container>::value &&
                                        detail::is_container_element_type_compatible<
                                            Container &, ElementType>::value,
                                    int>::type = 0>
  constexpr span(Container &cont) : storage_(detail::data(cont), detail::size(cont)) {}

  template <class Container, std::size_t E = Extent,
            typename std::enable_if<E == dynamic_extent &&
                                        detail::is_container<Container>::value &&
                                        detail::is_container_element_type_compatible<
                                            const Container &, ElementType>::value,
                                    int>::type = 0>
  constexpr span(const Container &cont)
      : storage_(detail::data(cont), detail::size(cont)) {}

  constexpr span(const span &other) noexcept = default;

  template <class OtherElementType, std::size_t OtherExtent,
            typename std::enable_if<
                (Extent == dynamic_extent || OtherExtent == dynamic_extent ||
                 Extent == OtherExtent) &&
                    std::is_convertible<OtherElementType (*)[], ElementType (*)[]>::value,
                int>::type = 0>
  constexpr span(const span<OtherElementType, OtherExtent> &other) noexcept
      : storage_(other.data(), other.size()) {}

  ~span() noexcept = default;

  constexpr span &operator=(const span &other) noexcept = default;

  // [span.sub], span subviews
  template <std::size_t Count>
  constexpr span<element_type, Count> first() const {
    TCB_SPAN_EXPECT(Count <= size());
    return {data(), Count};
  }

  template <std::size_t Count>
  constexpr span<element_type, Count> last() const {
    TCB_SPAN_EXPECT(Count <= size());
    return {data() + (size() - Count), Count};
  }

  template <std::size_t Offset, std::size_t Count = dynamic_extent>
  using subspan_return_t =
      span<ElementType,
           Count != dynamic_extent
               ? Count
               : (Extent != dynamic_extent ? Extent - Offset : dynamic_extent)>;

  template <std::size_t Offset, std::size_t Count = dynamic_extent>
  constexpr subspan_return_t<Offset, Count> subspan() const {
    TCB_SPAN_EXPECT(Offset <= size() &&
                    (Count == dynamic_extent || Offset + Count <= size()));
    return {data() + Offset, Count != dynamic_extent ? Count : size() - Offset};
  }

  constexpr span<element_type, dynamic_extent> first(size_type count) const {
    TCB_SPAN_EXPECT(count <= size());
    return {data(), count};
  }

  constexpr span<element_type, dynamic_extent> last(size_type count) const {
    TCB_SPAN_EXPECT(count <= size());
    return {data() + (size() - count), count};
  }

  constexpr span<element_type, dynamic_extent>
  subspan(size_type offset, size_type count = dynamic_extent) const {
    TCB_SPAN_EXPECT(offset <= size() &&
                    (count == dynamic_extent || offset + count <= size()));
    return {data() + offset, count == dynamic_extent ? size() - offset : count};
  }

  // [span.obs], span observers
  constexpr size_type size() const noexcept { return storage_.size; }

  constexpr size_type size_bytes() const noexcept {
    return size() * sizeof(element_type);
  }

  [[nodiscard]] constexpr bool empty() const noexcept { return size() == 0; }

  // [span.elem], span element access
  constexpr reference operator[](size_type idx) const {
    TCB_SPAN_EXPECT(idx < size());
    return *(data() + idx);
  }

  constexpr reference front() const {
    TCB_SPAN_EXPECT(!empty());
    return *data();
  }

  constexpr reference back() const {
    TCB_SPAN_EXPECT(!empty());
    return *(data() + (size() - 1));
  }

  constexpr pointer data() const noexcept { return storage_.ptr; }

  // [span.iterators], span iterator support
  constexpr iterator begin() const noexcept { return data(); }

  constexpr iterator end() const noexcept { return data() + size(); }

  constexpr reverse_iterator rbegin() const noexcept { return reverse_iterator(end()); }

  constexpr reverse_iterator rend() const noexcept { return reverse_iterator(begin()); }

 private:
  storage_type storage_{};
};

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

template <class ElementType, std::size_t Extent>
span<const byte,
     ((Extent == dynamic_extent) ? dynamic_extent : sizeof(ElementType) * Extent)>
as_bytes(span<ElementType, Extent> s) noexcept {
  return {reinterpret_cast<const byte *>(s.data()), s.size_bytes()};
}

template <class ElementType, size_t Extent,
          typename std::enable_if<!std::is_const<ElementType>::value, int>::type = 0>
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

#endif
