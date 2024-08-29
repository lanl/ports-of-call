#include "ports-of-call/span.hpp"

#ifndef CATCH_CONFIG_FAST_COMPILE
#define CATCH_CONFIG_FAST_COMPILE
#include <catch2/catch_test_macros.hpp>
#endif

#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <algorithm>
#include <array>
#include <iterator>
#include <numeric>
#include <type_traits>
#include <vector>

namespace span_test {

template <typename T>
constexpr static bool really_const = std::is_const<std::remove_reference_t<T>>::value;
}

TEST_CASE("span", "[util][span]") {
  using std::begin;
  using std::cbegin;
  using std::cend;
  using std::end;

  using span_test::really_const;

  SECTION("begin/end iteration") {
    SECTION("with non-zero size") {
      std::vector<double> data0 = {1, 2, 3};
      PortsOfCall::span<double> data(data0.data(), data0.size());

      auto b = begin(data);
      auto e = end(data);

      CHECK(std::distance(b, e) == 3);
      CHECK(b + 3 == e);
      CHECK(not really_const<decltype(*b)>);
      CHECK(not really_const<decltype(*e)>);

      auto cb = cbegin(data);
      auto ce = cend(data);

      CHECK(really_const<decltype(*cb)>);
      CHECK(really_const<decltype(*ce)>);
    }
  }

  SECTION("operator[]") {
    SECTION("with non-zero size") {
      SECTION("non-const data") {
        std::vector<int> data0{1, 2, 3};
        PortsOfCall::span<int> data(data0.data(), data0.size());

        for (int i = 0; i < 3; ++i) {
          CHECK(data[i] == i + 1);
        }
        CHECK(not really_const<decltype(data[0])>);
      }

      SECTION("with const data") {
        std::vector<int> const data0{1, 2, 3};
        PortsOfCall::span<int const> data(data0.data(), data0.size());

        for (int i = 0; i < 3; ++i) {
          CHECK(data[i] == i + 1);
        }
        CHECK(really_const<decltype(data[0])>);
      }
    }
  }

  SECTION("range-based for") {
    constexpr int N{10};
    std::vector<float> vec(N);
    float *ptr = vec.data();
    PortsOfCall::span<float> span(ptr, N);
    float const denom = static_cast<float>(1) / static_cast<float>(N);
    int n{0};
    for (auto &x : span) {
      x = static_cast<float>(n++) * denom;
    }
    for (int i{0}; i < N; ++i) {
      REQUIRE_THAT(span[i], Catch::Matchers::WithinRel(static_cast<float>(i) * denom));
    }
  }

  SECTION("STL algorithms") {
    constexpr int N{10};
    std::vector<int> vec(N);
    PortsOfCall::span<int> span(vec.data(), N);
    std::fill(span.begin(), span.end(), 42);
    bool all42 =
        std::all_of(span.begin(), span.end(), [](int const x) { return x == 42; });
    CHECK(all42);
    std::iota(span.begin(), span.end(), 1);
    int sum = std::accumulate(span.begin(), span.end(), 5);
    CHECK(sum == 60);
  }
}
