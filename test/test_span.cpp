#include "ports-of-call/span.hh"
#include <type_traits>

#ifndef CATCH_CONFIG_FAST_COMPILE
#define CATCH_CONFIG_FAST_COMPILE
#include <catch2/catch_test_macros.hpp>
#endif

#include <iostream>

namespace test {

using namespace PortsOfCall::span;

namespace detail {

// equal, search are the std::equal, std::search but made constexpr
// this is the C++20 implementation
template <class InputIt1, class InputIt2>
constexpr bool equal(InputIt1 first1, InputIt1 last1, InputIt2 first2) {
  for (; first1 != last1; ++first1, ++first2)
    if (!(*first1 == *first2)) return false;

  return true;
}

template <class ForwardIt1, class ForwardIt2>
constexpr ForwardIt1 search(ForwardIt1 first, ForwardIt1 last, ForwardIt2 s_first,
                            ForwardIt2 s_last) {
  while (true) {
    ForwardIt1 it = first;
    for (ForwardIt2 s_it = s_first;; ++it, ++s_it) {
      if (s_it == s_last) return first;
      if (it == last) return last;
      if (!(*it == *s_it)) break;
    }
    ++first;
  }
}

// same as prior, moved to constexpr
template <class ForwardIt, class Generator>
constexpr void generate(ForwardIt first, ForwardIt last, Generator g) {
  for (; first != last; ++first)
    *first = g();
}

template <class T, std::size_t N>
[[nodiscard]]
constexpr auto slide(span<T, N> s, std::size_t offset, std::size_t width) {
  return s.subspan(offset, offset + width <= s.size() ? width : 0U);
}

template <class T, std::size_t N, std::size_t M>
constexpr bool starts_with(span<T, N> data, span<T, M> prefix) {
  return data.size() >= prefix.size() &&
         equal(prefix.begin(), prefix.end(), data.begin());
}

template <class T, std::size_t N, std::size_t M>
constexpr bool ends_with(span<T, N> data, span<T, M> suffix) {
  return data.size() >= suffix.size() &&
         equal(data.end() - suffix.size(), data.end(), suffix.end() - suffix.size());
}

template <class T, std::size_t N, std::size_t M>
constexpr bool contains(span<T, N> s, span<T, M> sub) {
  return search(s.begin(), s.end(), sub.begin(), sub.end()) != s.end();
}

} // namespace detail

TEST_CASE("span default construction", "[PortsOfCall::span]") {

  static_assert(std::is_nothrow_default_constructible<span<int>>::value,
                "span<int> is not nothrow default constructible");
  static_assert(std::is_nothrow_constructible<span<int, 0>>::value,
                "span<int, 0> is not nothrow constructible");

  static_assert(!std::is_nothrow_constructible<span<int, 10>>::value,
                "span<int, 10> is nothrow construcible");

  SECTION("dynamic size") {
    constexpr span<int, 0> static_span;
    static_assert(nullptr == static_span.data(), "");
    static_assert(0u == static_span.size(), "");

    static_assert(static_span.begin() == static_span.end(),
                  "default constructed dynamic size span has begin != end");
  }

  SECTION("fixed size") {
    constexpr span<int, 0> s{};
    static_assert(s.size() == 0, "default constructed fixed size span has size != 0");
    static_assert(s.data() == 0,
                  "default constructed fixed size span has data != nullptr");
    static_assert(s.begin() == s.end(),
                  "default constructed fixed size span has begin != end");
  }
}

TEST_CASE("span iter, extent construction", "[PortsOfCall::span]") {
  static_assert(std::is_constructible<span<int>, int *, int>::value,
                "span<int>(int*, int) is not constructible");
  static_assert(std::is_constructible<span<const int>, int *, int>::value,
                "span<const int>(int*, int) is not constructible");
  static_assert(std::is_constructible<span<const int>, const int *, int>::value,
                "span<const int>(const int*, int) is not constructible");

  static_assert(std::is_constructible<span<int, 10>, int *, int>::value,
                "span<int, 10>(int*, int) is not constructible");
  static_assert(std::is_constructible<span<const int, 10>, int *, int>::value,
                "span<const int, 10>(int*, int) is not constructible");
  static_assert(std::is_constructible<span<const int, 10>, const int *, int>::value,
                "span<const int, 10>(const int*, int) is not constructible");

  int arr[] = {0, 1, 2};

  SECTION("dynamic") {
    span<int> s{arr, arr + 3};
    REQUIRE(s.size() == 3);
    REQUIRE(s.data() == arr);
    REQUIRE(s.begin() == arr);
    REQUIRE(s.end() == arr + 3);
  }

  SECTION("fixed") {
    span<int, 3> s(arr, 3);
    REQUIRE(s.size() == 3);
    REQUIRE(s.data() == arr);
    REQUIRE(s.begin() == arr);
    REQUIRE(s.end() == arr + 3);
  }
}

TEST_CASE("span Carr construction", "[PortsOfCall::span]") {
  using int_carr_t = int[3];
  static_assert(std::is_nothrow_constructible<span<int>, int_carr_t &>::value, "");
  static_assert(!std::is_constructible<span<int>, const int_carr_t &>::value, "");
  static_assert(std::is_nothrow_constructible<span<const int>, int_carr_t &>::value, "");
  static_assert(std::is_nothrow_constructible<span<const int>, const int_carr_t &>::value,
                "");

  static_assert(std::is_nothrow_constructible<span<int, 3>, int_carr_t &>::value, "");
  static_assert(!std::is_constructible<span<int, 3>, const int_carr_t &>::value, "");
  static_assert(std::is_nothrow_constructible<span<const int, 3>, int_carr_t &>::value,
                "");
  static_assert(
      std::is_nothrow_constructible<span<const int, 3>, const int_carr_t &>::value, "");

  static_assert(!std::is_constructible<span<int, 10>, int_carr_t &>::value, "");
  static_assert(!std::is_constructible<span<int, 10>, const int_carr_t &>::value, "");

  static_assert(!std::is_constructible<span<const int, 10>, int_carr_t &>::value, "");
  static_assert(!std::is_constructible<span<const int, 10>, const int_carr_t &>::value,
                "");

  SECTION("regular c-array") {
    int arr[] = {0, 1, 2};
    span<const int> const_span(arr);
    REQUIRE(arr == const_span.data());
    REQUIRE(std::size(arr) == const_span.size());
    for (size_t i = 0; i < const_span.size(); ++i)
      REQUIRE(arr[i] == const_span[i]);
    span<int> dynamic_span(arr);
    REQUIRE(arr == dynamic_span.data());
    REQUIRE(std::size(arr) == dynamic_span.size());
    for (size_t i = 0; i < dynamic_span.size(); ++i)
      REQUIRE(arr[i] == dynamic_span[i]);
    span<int, std::size(arr)> static_span(arr);
    REQUIRE(arr == static_span.data());
    REQUIRE(std::size(arr) == static_span.size());
    for (size_t i = 0; i < static_span.size(); ++i)
      REQUIRE(arr[i] == static_span[i]);
  }

  SECTION("constexpr c-array") {
    static constexpr int kArray[] = {5, 4, 3, 2, 1};
    constexpr span<const int> dynamic_span(kArray);
    static_assert(kArray == dynamic_span.data(), "");
    static_assert(std::size(kArray) == dynamic_span.size(), "");
    static_assert(kArray[0] == dynamic_span[0], "");
    static_assert(kArray[1] == dynamic_span[1], "");
    static_assert(kArray[2] == dynamic_span[2], "");
    static_assert(kArray[3] == dynamic_span[3], "");
    static_assert(kArray[4] == dynamic_span[4], "");
    constexpr span<const int, std::size(kArray)> static_span(kArray);
    static_assert(kArray == static_span.data(), "");
    static_assert(std::size(kArray) == static_span.size(), "");
    static_assert(kArray[0] == static_span[0], "");
    static_assert(kArray[1] == static_span[1], "");
    static_assert(kArray[2] == static_span[2], "");
    static_assert(kArray[3] == static_span[3], "");
    static_assert(kArray[4] == static_span[4], "");
  }
}

TEST_CASE("span std::array construction", "[PortsOfCall::span]") {
  // In the following assertions we use std::is_convertible_v<From, To>, which
  // for non-void types is equivalent to checking whether the following
  // expression is well-formed:
  //
  // T obj = std::declval<From>();
  //
  // In particular we are checking whether From is implicitly convertible to To,
  // which also implies that To is explicitly constructible from From.

  static_assert(std::is_convertible<std::array<int, 3> &, span<int>>::value,
                "Error: l-value reference to std::array<int> should be convertible to "
                "span<int> with dynamic extent.");
  static_assert(std::is_convertible<std::array<int, 3> &, span<int, 3>>::value,
                "Error: l-value reference to std::array<int> should be convertible to "
                "span<int> with the same static extent.");
  static_assert(std::is_convertible<std::array<int, 3> &, span<const int>>::value,
                "Error: l-value reference to std::array<int> should be convertible to "
                "span<const int> with dynamic extent.");
  static_assert(std::is_convertible<std::array<int, 3> &, span<const int, 3>>::value,
                "Error: l-value reference to std::array<int> should be convertible to "
                "span<const int> with the same static extent.");
  static_assert(std::is_convertible<const std::array<int, 3> &, span<const int>>::value,
                "Error: const l-value reference to std::array<int> should be "
                "convertible to span<const int> with dynamic extent.");
  static_assert(
      std::is_convertible<const std::array<int, 3> &, span<const int, 3>>::value,
      "Error: const l-value reference to std::array<int> should be convertible "
      "to span<const int> with the same static extent.");
  static_assert(std::is_convertible<std::array<const int, 3> &, span<const int>>::value,
                "Error: l-value reference to std::array<const int> should be "
                "convertible to span<const int> with dynamic extent.");
  static_assert(
      std::is_convertible<std::array<const int, 3> &, span<const int, 3>>::value,
      "Error: l-value reference to std::array<const int> should be convertible "
      "to span<const int> with the same static extent.");
  static_assert(
      std::is_convertible<const std::array<const int, 3> &, span<const int>>::value,
      "Error: const l-value reference to std::array<const int> should be "
      "convertible to span<const int> with dynamic extent.");
  static_assert(
      std::is_convertible<const std::array<const int, 3> &, span<const int, 3>>::value,
      "Error: const l-value reference to std::array<const int> should be "
      "convertible to span<const int> with the same static extent.");

  // In the following assertions we use !std::is_constructible_v<T, Args>, which
  // is equivalent to checking whether the following expression is malformed:
  //
  // T obj(std::declval<Args>()...);
  //
  // In particular we are checking that T is not explicitly constructible from
  // Args, which also implies that T is not implicitly constructible from Args
  // as well.
  static_assert(!std::is_constructible<span<int>, const std::array<int, 3> &>::value,
                "Error: span<int> with dynamic extent should not be "
                "constructible from const l-value reference to std::array<int>");
  static_assert(!std::is_constructible<span<int>, std::array<const int, 3> &>::value,
                "Error: span<int> with dynamic extent should not be constructible "
                "from l-value reference to std::array<const int>");
  static_assert(
      !std::is_constructible<span<int>, const std::array<const int, 3> &>::value,
      "Error: span<int> with dynamic extent should not be constructible "
      "const from l-value reference to std::array<const int>");
  static_assert(!std::is_constructible<span<int, 2>, std::array<int, 3> &>::value,
                "Error: span<int> with static extent should not be constructible "
                "from l-value reference to std::array<int> with different extent");
  static_assert(!std::is_constructible<span<int, 4>, std::array<int, 3> &>::value,
                "Error: span<int> with dynamic extent should not be constructible "
                "from l-value reference to std::array<int> with different extent");
  static_assert(!std::is_constructible<span<int>, std::array<bool, 3> &>::value,
                "Error: span<int> with dynamic extent should not be constructible "
                "from l-value reference to std::array<bool>");

  // Note: Constructing a constexpr span from a constexpr std::array does not
  // work prior to C++17 due to non-constexpr std::array::data.
  std::array<int, 5> array = {{5, 4, 3, 2, 1}};
  span<const int> const_span(array);
  REQUIRE(array.data() == const_span.data());
  REQUIRE(array.size() == const_span.size());
  for (size_t i = 0; i < const_span.size(); ++i)
    REQUIRE(array[i] == const_span[i]);
  span<int> dynamic_span(array);
  REQUIRE(array.data() == dynamic_span.data());
  REQUIRE(array.size() == dynamic_span.size());
  for (size_t i = 0; i < dynamic_span.size(); ++i)
    REQUIRE(array[i] == dynamic_span[i]);
  span<int, std::size(array)> static_span(array);
  REQUIRE(array.data() == static_span.data());
  REQUIRE(array.size() == static_span.size());
  for (size_t i = 0; i < static_span.size(); ++i)
    REQUIRE(array[i] == static_span[i]);
}

TEST_CASE("span container construction", "[PortsOfCall::span]") {
  using vec_t = std::vector<int>;

  static_assert(std::is_nothrow_constructible<span<int>, vec_t &>::value, "");
  static_assert(!std::is_constructible<span<int>, const vec_t &>::value, "");
  static_assert(std::is_nothrow_constructible<span<const int>, vec_t &>::value, "");
  static_assert(std::is_nothrow_constructible<span<const int>, const vec_t &>::value, "");

  static_assert(!std::is_constructible<span<int, 3>, vec_t &>::value, "");
  static_assert(!std::is_constructible<span<int, 3>, const vec_t &>::value, "");
  static_assert(!std::is_constructible<span<const int, 3>, vec_t &>::value, "");
  static_assert(!std::is_constructible<span<const int, 3>, const vec_t &>::value, "");

  vec_t varr = {1, 2, 3};

  SECTION("dynamic") {
    span<int> s{varr};
    REQUIRE(s.size() == 3);
    REQUIRE(s.data() == varr.data());
    REQUIRE(s.begin() == varr.data());
    REQUIRE(s.end() == varr.data() + 3);
  }

  SECTION("dynamic const") {
    span<const int> s{varr};
    REQUIRE(s.size() == 3);
    REQUIRE(s.data() == varr.data());
    REQUIRE(s.begin() == varr.data());
    REQUIRE(s.end() == varr.data() + 3);
  }
  // NB: fixed/fixed const is equivilant to std::array construction
}

TEST_CASE("span different size span construction", "PortsOfCall::span") {
  using zero_span = span<int, 0>;
  using zero_const_span = span<const int, 0>;

  using big_span = span<int, 1000000>;
  using big_const_span = span<const int, 1000000>;

  using dynamic_span = span<int>;
  using dynamic_const_span = span<const int>;

  //
  static_assert(std::is_trivially_copyable<zero_span>::value, "");
  static_assert(std::is_trivially_move_constructible<zero_span>::value, "");

  static_assert(!std::is_constructible<zero_span, big_span>::value, "");

  static_assert(!std::is_constructible<zero_span, zero_const_span>::value, "");
  static_assert(!std::is_constructible<zero_span, big_const_span>::value, "");
  static_assert(!std::is_constructible<zero_span, dynamic_const_span>::value, "");

  static_assert(std::is_nothrow_constructible<zero_span, dynamic_span>::value, "");
  //
  static_assert(std::is_trivially_copyable<big_span>::value, "");
  static_assert(std::is_trivially_move_constructible<big_span>::value, "");

  static_assert(!std::is_constructible<big_span, zero_span>::value, "");
  static_assert(!std::is_constructible<big_span, zero_const_span>::value, "");
  static_assert(!std::is_constructible<big_span, big_const_span>::value, "");
  static_assert(!std::is_constructible<big_span, dynamic_const_span>::value, "");

  static_assert(std::is_nothrow_constructible<big_span, dynamic_span>::value, "");
  //
  //
  static_assert(std::is_trivially_copyable<zero_const_span>::value, "");
  static_assert(std::is_trivially_move_constructible<zero_const_span>::value, "");
  static_assert(!std::is_constructible<zero_const_span, big_span>::value, "");
  static_assert(!std::is_constructible<zero_const_span, big_const_span>::value, "");
  static_assert(std::is_nothrow_constructible<zero_const_span, zero_span>::value, "");

  static_assert(std::is_nothrow_constructible<zero_const_span, dynamic_span>::value, "");
  static_assert(std::is_nothrow_constructible<zero_const_span, dynamic_const_span>::value,
                "");
  //
  static_assert(std::is_trivially_copyable<big_const_span>::value, "");
  static_assert(std::is_trivially_move_constructible<big_const_span>::value, "");

  static_assert(!std::is_constructible<big_const_span, zero_span>::value, "");
  static_assert(!std::is_constructible<big_const_span, zero_const_span>::value, "");
  static_assert(std::is_nothrow_constructible<big_const_span, big_span>::value, "");
  static_assert(std::is_nothrow_constructible<big_const_span, dynamic_span>::value, "");
  static_assert(std::is_nothrow_constructible<big_const_span, dynamic_const_span>::value,
                "");
  //
  //
  static_assert(std::is_trivially_copyable<dynamic_span>::value, "");
  static_assert(std::is_trivially_move_constructible<dynamic_span>::value, "");

  static_assert(!std::is_constructible<dynamic_span, big_const_span>::value, "");
  static_assert(!std::is_constructible<dynamic_span, zero_const_span>::value, "");
  static_assert(!std::is_constructible<dynamic_span, dynamic_const_span>::value, "");

  static_assert(std::is_nothrow_constructible<dynamic_span, big_span>::value, "");
  static_assert(std::is_nothrow_constructible<dynamic_span, zero_span>::value, "");

  //
  static_assert(std::is_trivially_copyable<dynamic_const_span>::value, "");
  static_assert(std::is_trivially_move_constructible<dynamic_const_span>::value, "");

  static_assert(std::is_nothrow_constructible<dynamic_const_span, zero_span>::value, "");
  static_assert(std::is_nothrow_constructible<dynamic_const_span, zero_const_span>::value,
                "");
  static_assert(std::is_nothrow_constructible<dynamic_const_span, big_span>::value, "");
  static_assert(std::is_nothrow_constructible<dynamic_const_span, big_const_span>::value,
                "");
  static_assert(std::is_nothrow_constructible<dynamic_const_span, dynamic_span>::value,
                "");
  constexpr zero_const_span s0{};
  constexpr dynamic_const_span d{s0};

  static_assert(d.size() == 0, "");
  static_assert(d.data() == nullptr, "");
  static_assert(d.begin() == d.end(), "");
}

TEST_CASE("span member subview operations", "[PortsOfCall::span]") {

  int arr[] = {1, 2, 3, 4, 5};
  span<int, 5> s{arr};

  SECTION("first<N>") {
    auto f = s.first<3>();

    static_assert(std::is_same<decltype(f), span<int, 3>>::value, "");
    REQUIRE(f.size() == 3);
    REQUIRE(f.data() == arr);
    REQUIRE(f.begin() == arr);
    REQUIRE(f.end() == arr + 3);
  }

  SECTION("last<N>") {
    auto l = s.last<3>();

    static_assert(std::is_same<decltype(l), span<int, 3>>::value, "");
    REQUIRE(l.size() == 3);
    REQUIRE(l.data() == arr + 2);
    REQUIRE(l.begin() == arr + 2);
    REQUIRE(l.end() == std::end(arr));
  }

  SECTION("subspan<N>") {
    auto ss = s.subspan<1, 2>();

    static_assert(std::is_same<decltype(ss), span<int, 2>>::value, "");
    REQUIRE(ss.size() == 2);
    REQUIRE(ss.data() == arr + 1);
    REQUIRE(ss.begin() == arr + 1);
    REQUIRE(ss.end() == arr + 3);
  }

  SECTION("first(n)") {
    auto f = s.first(3);

    static_assert(std::is_same<decltype(f), span<int>>::value, "");
    REQUIRE(f.size() == 3);
    REQUIRE(f.data() == arr);
    REQUIRE(f.begin() == arr);
    REQUIRE(f.end() == arr + 3);
  }

  SECTION("last(n)") {
    auto l = s.last(3);

    static_assert(std::is_same<decltype(l), span<int>>::value, "");
    REQUIRE(l.size() == 3);
    REQUIRE(l.data() == arr + 2);
    REQUIRE(l.begin() == arr + 2);
    REQUIRE(l.end() == std::end(arr));
  }

  SECTION("subspan(n)") {
    auto ss = s.subspan(1, 2);

    static_assert(std::is_same<decltype(ss), span<int>>::value, "");
    REQUIRE(ss.size() == 2);
    REQUIRE(ss.data() == arr + 1);
    REQUIRE(ss.begin() == arr + 1);
    REQUIRE(ss.end() == arr + 3);
  }
}

TEST_CASE("span observers", "[PortsOfCall::span]") {
  constexpr span<int, 0> empty{};
  static_assert(empty.size() == 0, "");
  static_assert(empty.empty(), "");

  constexpr int arr[] = {1, 2, 3};
  static_assert(span<const int>{arr}.size() == 3, "");
  static_assert(!span<const int>{arr}.empty(), "");
}

TEST_CASE("span element access", "[PortsOfCall::span]") {
  constexpr int arr[] = {1, 2, 3};
  span<const int> s{arr};

  REQUIRE(s[0] == arr[0]);
  REQUIRE(s[1] == arr[1]);
  REQUIRE(s[2] == arr[2]);
}

// adopted from https://en.cppreference.com/w/cpp/container/span
TEST_CASE("span typical", "[PortsOfCall::span]") {
  constexpr int a[]{0, 1, 2, 3, 4, 5, 6, 7, 8};
  constexpr int b[]{8, 7, 6};
  constexpr static std::size_t width{6};

  constexpr int res[][width] = {
      {0, 1, 2, 3, 4, 5}, {1, 2, 3, 4, 5, 6}, {2, 3, 4, 5, 6, 7}, {3, 4, 5, 6, 7, 8}};

  for (std::size_t offset{};; ++offset)
    if (auto s = detail::slide(span{a}, offset, width); !s.empty())
      REQUIRE(detail::equal(s.begin(), s.end(), std::begin(res[offset])));
    else
      break;

  static_assert("" && detail::starts_with(span{a}, span{a, 4}) &&
                detail::starts_with(span{a + 1, 4}, span{a + 1, 3}) &&
                !detail::starts_with(span{a}, span{b}) &&
                !detail::starts_with(span{a, 8}, span{a + 1, 3}) &&
                detail::ends_with(span{a}, span{a + 6, 3}) &&
                !detail::ends_with(span{a}, span{a + 6, 2}) &&
                detail::contains(span{a}, span{a + 1, 4}) &&
                !detail::contains(span{a, 8}, span{a, 9}));
}

} // namespace test
