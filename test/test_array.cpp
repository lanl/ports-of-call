#include "ports-of-call/array.hpp"
#include "ports-of-call/portability.hpp"

#ifndef CATCH_CONFIG_FAST_COMPILE
#define CATCH_CONFIG_FAST_COMPILE
#include <catch2/catch_test_macros.hpp>
#endif

#include <iostream>

using namespace PortsOfCall;

TEST_CASE("array nominal element access (GPU)", "[array][GPU]") {
  // Declare the array
  constexpr int N = 16;
  array<double, N> arr;

  // Fill the array
  for (int i = 0; i < N; ++i) {
    arr[i] = i + 1;
  }

  // Can we read from it on the GPU?
  int count = 0;
  auto func = PORTABLE_LAMBDA(const int i, int &count) mutable {
    if (arr[i] == i + 1) {
      ++count;
    }
  };
  portableReduce("assign_and_check", 0, N, func, count);
  CHECK(count == N);
}

TEST_CASE("array nominal element access", "[array]") {
  // Declare the array
  constexpr int N = 16;
  array<double, N> arr;

  // Fill the array
  for (int i = 0; i < N; ++i) {
    arr[i] = i + 1;
  }

  // Can we read from it?
  for (int i = 0; i < N; ++i) {
    CHECK(arr[i] == i + 1);
  }
}

TEST_CASE("array zero-sized element access", "[array]") {
  array<double, 0> arr;

  // accessing the zeroth element of a zero-size array is
  // allowed but undefined behavior.  We will only verify that
  // the address returned is well defined.
  CHECK(std::addressof(arr[0]));
}

TEST_CASE("const array element access", "[array]") {
  // Declare the array
  constexpr int N = 5;
  array<int, N> const arr{{0, 1, 2, 3, 4}};

  // Can we read from it?
  for (int i = 0; i < N; ++i) {
    CHECK(arr[i] == i);
  }
}

TEST_CASE("const array zero-sized element access", "[array]") {
  array<double, 0> const arr{};

  // accessing the zeroth element of a zero-size array is
  // allowed but undefined behavior.  We will only verify that
  // the address returned is well defined.
  CHECK(std::addressof(arr[0]));
}

TEST_CASE("array range-based for loop", "[array]") {
  SECTION("with non-const array") {
    // Declare the array
    constexpr int N = 15;
    array<int, N> arr;

    // Fill the loop
    int i = 0;
    for (auto &x : arr) {
      x = ++i;
    }

    // Check the values
    i = 0;
    for (auto &x : arr) {
      ++i;
      CHECK(x == i);
    }
  }

  SECTION("with const array") {
    // Declare the array
    constexpr int N = 5;
    array<int, N> const arr{{1, 2, 3, 4, 5}};

    // Check the values
    int i = 0;
    for (auto const &x : arr) {
      ++i;
      CHECK(x == i);
    }
  }
}

TEST_CASE("array begins and ends", "[array]") {
  using std::begin;
  using std::cbegin;
  using std::cend;
  using std::end;

  SECTION("with non-const array") {
    array<int, 3> arr{{1, 2, 3}};

    CHECK(std::is_same<int *, decltype(begin(arr))>::value);
    CHECK(std::is_same<int const *, decltype(cbegin(arr))>::value);

    CHECK(std::is_same<int *, decltype(end(arr))>::value);
    CHECK(std::is_same<int const *, decltype(cend(arr))>::value);
  }

  SECTION("with const array") {
    array<int, 3> const arr{{1, 2, 3}};

    CHECK(std::is_same<int const *, decltype(begin(arr))>::value);
    CHECK(std::is_same<int const *, decltype(cbegin(arr))>::value);

    CHECK(std::is_same<int const *, decltype(end(arr))>::value);
    CHECK(std::is_same<int const *, decltype(cend(arr))>::value);
  }

  SECTION("with non-const zero-sized array") {
    array<int, 0> arr;

    CHECK(begin(arr) == end(arr));
  }

  SECTION("with const zero-sized array") {
    array<int, 0> const arr{};

    CHECK(begin(arr) == end(arr));
  }
}

TEST_CASE("array front and back", "[array]") {
  array<int, 3> arr1{{3, 2, 1}};

  CHECK(std::is_same<int &, decltype(arr1.front())>::value);
  CHECK(arr1.front() == 3);

  CHECK(std::is_same<int &, decltype(arr1.back())>::value);
  CHECK(arr1.back() == 1);

  array<double, 3> const arr2{{3, 2, 1}};

  CHECK(std::is_same<double const &, decltype(arr2.front())>::value);
  CHECK(arr2.front() == 3.0);

  CHECK(std::is_same<double const &, decltype(arr2.back())>::value);
  CHECK(arr2.back() == 1.0);
}

TEST_CASE("array data", "[array]") {
  SECTION("with non-const array") {
    array<int, 3> arr{{3, 2, 1}};

    CHECK(std::is_same<int *, decltype(arr.data())>::value);
    CHECK(arr.data() == std::addressof(arr[0]));
  }

  SECTION("with non-const array") {
    array<int, 3> const arr{{3, 2, 1}};

    CHECK(std::is_same<int const *, decltype(arr.data())>::value);
    CHECK(arr.data() == std::addressof(arr[0]));
  }
}

TEST_CASE("array empty", "[array]") {
  SECTION("with non-empty array") {
    array<int, 10> arr;

    CHECK(not arr.empty());
  }

  SECTION("with empty array") {
    array<int, 0> arr;

    CHECK(arr.empty());
  }
}

TEST_CASE("array sizes", "[array]") {
  array<int, 42> arr;

  CHECK(arr.size() == arr.max_size());
}

TEST_CASE("array fill (GPU)", "[array][GPU]") {
  constexpr std::size_t N = 42;
  std::size_t count = 0;
  auto func = PORTABLE_LAMBDA(const int i, std::size_t &count) {
    constexpr double value = 3.14;
    array<double, N> arr;
    arr.fill(value);
    for (const double x : arr) {
      count += (x == value) ? 1 : 0;
    }
  };
  constexpr std::size_t M = 5;
  portableReduce("check", 0, M, func, count);
  CHECK(count == N * M);
}

TEST_CASE("array fill", "[array]") {
  array<double, 42> arr;
  arr.fill(3.14);

  for (auto const &x : arr) {
    CHECK(x == 3.14);
  }
}

TEST_CASE("array swap", "[array]") {
  array<int, 10> zeros;
  zeros.fill(0);
  array<int, 10> ones;
  ones.fill(1);

  zeros.swap(ones);

  for (auto const &x : zeros) {
    CHECK(x == 1);
  }

  for (auto const &x : ones) {
    CHECK(x == 0);
  }

  using std::swap;

  swap(zeros, ones);

  for (auto const &x : zeros) {
    CHECK(x == 0);
  }

  for (auto const &x : ones) {
    CHECK(x == 1);
  }
}

TEST_CASE("array tuple_size", "[array]") {
  array<double, 5> arr;

  CHECK(std::tuple_size<decltype(arr)>::value == 5);
}

TEST_CASE("array tuple_element", "[array]") {
  struct Foo {};

  array<Foo, 3> foos;

  CHECK(std::is_same<Foo, std::tuple_element<1, decltype(foos)>::type>::value);
}

TEST_CASE("make_array function", "[array]") {
  auto arr = make_array(1.0, 2.0, 3.0);

  CHECK(std::is_same<decltype(arr), array<double, 3>>::value);
}
