#include "ports-of-call/span.hh"
#include <type_traits>

#ifndef CATCH_CONFIG_FAST_COMPILE
#define CATCH_CONFIG_FAST_COMPILE
#include <catch2/catch_test_macros.hpp>
#endif

#include <iostream>

#define FIXED 10

using namespace PortsOfCall::span;

template <class S, class I, auto N = 3>
auto span_require(S &&s, I &&i) {
  REQUIRE(s.size() == N);
  REQUIRE(s.data() == i);
  REQUIRE(s.begin() == std::begin(i));
  REQUIRE(s.end() == std::end(i));
}

TEST_CASE("default construction", "[PortsOfCall::span]") {
  static_assert(std::is_nothrow_default_constructible<span<int>>::value, "");
  static_assert(std::is_nothrow_constructible<span<int, 0>>::value, "");

  static_assert(!std::is_nothrow_constructible<span<int, FIXED>>::value, "");

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

TEST_CASE("iter, extent construction", "[PortsOfCall::span]") {
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

TEST_CASE("Carr construction", "[PortsOfCall::span]") {
  using int_array_t = int[3];
  using real_array_t = double[3];

  static_assert(std::is_nothrow_constructible<span<int>, int_array_t &>::value, "");
  static_assert(!std::is_constructible<span<int>, int_array_t const &>::value, "");
  static_assert(!std::is_constructible<span<int>, real_array_t>::value, "");

  static_assert(std::is_nothrow_constructible<span<const int>, int_array_t &>::value, "");
  static_assert(
      std::is_nothrow_constructible<span<const int>, int_array_t const &>::value, "");
  static_assert(!std::is_constructible<span<const int>, real_array_t>::value, "");

  static_assert(std::is_nothrow_constructible<span<int, 3>, int_array_t &>::value, "");
  static_assert(!std::is_constructible<span<int, 3>, int_array_t const &>::value, "");
  static_assert(!std::is_constructible<span<int, 3>, real_array_t &>::value, "");

  static_assert(std::is_nothrow_constructible<span<const int, 3>, int_array_t &>::value,
                "");
  static_assert(
      std::is_nothrow_constructible<span<const int, 3>, int_array_t const &>::value, "");
  static_assert(!std::is_constructible<span<const int, 3>, real_array_t>::value, "");

  static_assert(!std::is_constructible<span<int, 42>, int_array_t &>::value, "");
  static_assert(!std::is_constructible<span<int, 42>, int_array_t const &>::value, "");
  static_assert(!std::is_constructible<span<int, 42>, real_array_t &>::value, "");

  static_assert(!std::is_constructible<span<const int, 42>, int_array_t &>::value, "");
  static_assert(!std::is_constructible<span<const int, 42>, int_array_t const &>::value,
                "");
  static_assert(!std::is_constructible<span<const int, 42>, real_array_t &>::value, "");

  int arr[] = {0, 1, 2};

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
