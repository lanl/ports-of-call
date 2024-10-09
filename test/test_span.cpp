#include "ports-of-call/span.hh"
#include <type_traits>

#ifndef CATCH_CONFIG_FAST_COMPILE
#define CATCH_CONFIG_FAST_COMPILE
#include <catch2/catch_test_macros.hpp>
#endif

#include <iostream>

#define FIXED 10

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

template <class S, class I, auto N = 3>
auto span_require(S &&s, I &&i) {
  REQUIRE(s.size() == N);
  REQUIRE(s.data() == i);
  REQUIRE(s.begin() == i);
  REQUIRE(s.end() == i + N);
}

TEST_CASE("span default construction", "[PortsOfCall::span]") {
  static_assert(std::is_nothrow_default_constructible<span<int>>::value,
                "span<int> is not nothrow default constructible");
  static_assert(std::is_nothrow_constructible<span<int, 0>>::value,
                "span<int, 0> is not nothrow constructible");

  static_assert(!std::is_nothrow_constructible<span<int, FIXED>>::value,
                "span<int, FIXED> is nothrow construcible");

  SECTION("dynamic size") {
    constexpr span<int> s{};
    static_assert(s.size() == 0, "default constructed dynamic size span has size != 0");
    static_assert(s.data() == 0,
                  "default constructed dynamic size span has data != nullptr");
    static_assert(s.begin() == s.end(),
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

  static_assert(std::is_constructible<span<int, FIXED>, int *, int>::value,
                "span<int, FIXED>(int*, int) is not constructible");
  static_assert(std::is_constructible<span<const int, FIXED>, int *, int>::value,
                "span<const int, FIXED>(int*, int) is not constructible");
  static_assert(std::is_constructible<span<const int, FIXED>, const int *, int>::value,
                "span<const int, FIXED>(const int*, int) is not constructible");

  int arr[] = {0, 1, 2};

  SECTION("dynamic") {
    span<int> s{arr, arr + 3};
    span_require(s, arr);
  }

  SECTION("fixed") {
    span<int, 3> s(arr, 3);
    span_require(s, arr);
  }
}

template <class C, class E, std::size_t N = 3>
constexpr auto array_construcible_correct() {
  static_assert(std::is_nothrow_constructible<span<E>, C &>::value, "");
  static_assert(!std::is_constructible<span<E>, const C &>::value, "");
  static_assert(std::is_nothrow_constructible<span<const E>, C &>::value, "");
  static_assert(std::is_nothrow_constructible<span<const E>, const C &>::value, "");

  static_assert(std::is_nothrow_constructible<span<E, N>, C &>::value, "");
  static_assert(!std::is_constructible<span<E, N>, const C &>::value, "");
  static_assert(std::is_nothrow_constructible<span<const E, N>, C &>::value, "");
  static_assert(std::is_nothrow_constructible<span<const E, N>, const C &>::value, "");
}

template <class E, class C, std::size_t N = 3>
constexpr auto array_construcible_correct_oversized() {
  constexpr auto M = N + 1;
  static_assert(!std::is_constructible<span<int, M>, C &>::value, "");
  static_assert(!std::is_constructible<span<int, M>, const C &>::value, "");

  static_assert(!std::is_constructible<span<const int, M>, C &>::value, "");
  static_assert(!std::is_constructible<span<const int, M>, const C &>::value, "");
}

TEST_CASE("span Carr construction", "[PortsOfCall::span]") {
  using int_array_t = int[3];
  using real_array_t = double[3];

  array_construcible_correct<int_array_t, int>();
  array_construcible_correct<real_array_t, double>();

  array_construcible_correct_oversized<int_array_t, int>();

  int_array_t arr = {0, 1, 2};

  SECTION("dynamic") {
    span<int> s{arr};
    span_require(s, arr);
  }

  SECTION("dynamic const") {
    span<const int> s{arr};
    span_require(s, arr);
  }

  SECTION("fixed") {
    span<int, 3> s{arr};
    span_require(s, arr);
  }

  SECTION("fixed const") {
    span<const int, 3> s{arr};
    span_require(s, arr);
  }
}

TEST_CASE("span std::array construction", "[PortsOfCall::span]") {
  using int_array_t = std::array<int, 3>;
  using real_array_t = std::array<double, 3>;
  using zero_array_t = std::array<int, 0>;

  array_construcible_correct<int_array_t, int>();
  array_construcible_correct<real_array_t, double>();
  array_construcible_correct<zero_array_t, int, 0>();

  array_construcible_correct_oversized<int_array_t, int>();
  array_construcible_correct_oversized<real_array_t, int>();

  int_array_t arr = {0, 1, 2};

  SECTION("dynamic") {
    span<int> s{arr};
    span_require(s, arr.data());
  }

  SECTION("dynamic const") {
    span<const int> s{arr};
    span_require(s, arr.data());
  }

  SECTION("fixed") {
    span<int, 3> s{arr};
    span_require(s, arr.data());
  }

  SECTION("fixed const") {
    span<const int, 3> s{arr};
    span_require(s, arr.data());
  }
}

template <class C, std::size_t N = 3>
constexpr auto container_construcible_correct() {
  using E = typename C::value_type;
  static_assert(std::is_nothrow_constructible<span<E>, C &>::value, "");
  static_assert(!std::is_constructible<span<E>, const C &>::value, "");
  static_assert(std::is_nothrow_constructible<span<const E>, C &>::value, "");
  static_assert(std::is_nothrow_constructible<span<const E>, const C &>::value, "");

  static_assert(!std::is_constructible<span<E, N>, C &>::value, "");
  static_assert(!std::is_constructible<span<E, N>, const C &>::value, "");
  static_assert(!std::is_constructible<span<const E, N>, C &>::value, "");
  static_assert(!std::is_constructible<span<const E, N>, const C &>::value, "");
}

TEST_CASE("span container construction", "[PortsOfCall::span]") {
  using vec_t = std::vector<int>;

  container_construcible_correct<vec_t>();
  vec_t varr = {1, 2, 3};

  SECTION("dynamic") {
    span<int> s{varr};
    span_require(s, varr.data());
  }

  SECTION("dynamic const") {
    span<const int> s{varr};
    span_require(s, varr.data());
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
    span_require(f, arr);
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
    span_require(f, arr);
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
