#include "ports-of-call/portability.hpp"
#include "ports-of-call/static_vector.hpp"

#ifndef CATCH_CONFIG_FAST_COMPILE
#define CATCH_CONFIG_FAST_COMPILE
#include <catch2/catch_test_macros.hpp>
#endif

#include <iterator>
#include <type_traits>

namespace static_vector_test {

template <typename T>
constexpr static bool really_const = std::is_const<std::remove_reference_t<T>>::value;

// This doesn't do anything interesting.  It just needs to be non-trivial to
// test that static_vector works for non-trivial types.
struct NonTrivialType {
    int n;
    int * p;
    PORTABLE_FUNCTION NonTrivialType(int nn)
    : n{nn}
    , p{&n}
    {}
    PORTABLE_FUNCTION NonTrivialType(const NonTrivialType & other)
    : n{other.n}
    , p{&n}
    {}
    PORTABLE_FUNCTION NonTrivialType(NonTrivialType && other)
    : n{other.n}
    , p{&n}
    {}
    PORTABLE_FUNCTION NonTrivialType& operator=(const NonTrivialType & other) {
        n = other.n;
        p = &n;
        return *this;
    }
    PORTABLE_FUNCTION NonTrivialType& operator=(NonTrivialType && other) {
        n = other.n;
        p = &n;
        return *this;
    }
    PORTABLE_FUNCTION ~NonTrivialType() {}
};

}

static_assert(!std::is_trivial<static_vector_test::NonTrivialType>::value);

TEST_CASE("static_vector", "[util][static_vector]") {
  using std::begin;
  using std::cbegin;
  using std::cend;
  using std::end;

  using static_vector_test::really_const;
  using static_vector_test::NonTrivialType;

  SECTION("begin/end iteration") {
    SECTION("with non-zero size") {
      using test_t = PortsOfCall::static_vector<double, 5>;
      test_t data = {1, 2, 3};

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
      using test_t = PortsOfCall::static_vector<int, 5>;

      SECTION("non-const data") {
        test_t data = {1, 2, 3};

        for (int i = 0; i < 3; ++i) {
          CHECK(data[i] == i + 1);
        }
        CHECK(not really_const<decltype(data[0])>);
      }

      SECTION("with const data") {
        test_t const data = {1, 2, 3};

        for (int i = 0; i < 3; ++i) {
          CHECK(data[i] == i + 1);
        }
        CHECK(really_const<decltype(data[0])>);
      }
    }
  }

  SECTION("front/back") {
    using test_t = PortsOfCall::static_vector<int, 5>;

    SECTION("non-const data") {
      test_t data = {1, 2, 3};

      CHECK(data.front() == 1);
      CHECK(data.back() == 3);
      CHECK(not really_const<decltype(data.front())>);
      CHECK(not really_const<decltype(data.back())>);
    }

    SECTION("with const data") {
      test_t const data = {1, 2, 3};

      CHECK(data.front() == 1);
      CHECK(data.back() == 3);
      CHECK(really_const<decltype(data.front())>);
      CHECK(really_const<decltype(data.back())>);
    }
  }

  SECTION("data") {
    SECTION("with non-zero size") {
      using test_t = PortsOfCall::static_vector<double, 5>;

      SECTION("non-const data") {
        test_t data = {1, 2, 3};

        CHECK(data.data() == std::addressof(data[0]));
      }

      SECTION("const data") {
        test_t const data = {1, 2, 3};

        CHECK(data.data() == std::addressof(data[0]));
      }
    }
  }

  SECTION("empty/size/max_size/capacity") {
    SECTION("with non-zero size") {
      using test_t = PortsOfCall::static_vector<double, 5>;

      test_t const data1 = {};

      CHECK(data1.empty());
      CHECK(data1.size() == 0);
      CHECK(data1.max_size() == 5);
      CHECK(data1.capacity() == 5);

      test_t const data2 = {1, 2, 3};

      CHECK(not data2.empty());
      CHECK(data2.size() == 3);
      CHECK(data2.max_size() == 5);
      CHECK(data2.capacity() == 5);
    }
  }

  SECTION("clear") {
    SECTION("with trivial type") {
      using test_t = PortsOfCall::static_vector<double, 5>;

      test_t data = {1, 2, 3};
      CHECK(data.size() == 3);
      data.clear();
      CHECK(data.size() == 0);
    }

    SECTION("with non-trivial type") {
      using test_t = PortsOfCall::static_vector<NonTrivialType, 5>;

      test_t data = {NonTrivialType(2), NonTrivialType(4), NonTrivialType(6)};
      CHECK(data.size() == 3);
      data.clear();
      CHECK(data.size() == 0);
    }
  }

  SECTION("push_back/emplace_back") {
    using test_t = PortsOfCall::static_vector<NonTrivialType, 5>;

    test_t data;

    auto insert = NonTrivialType(1);
    data.push_back(insert);

    // Note: Some compilers struggle to parse the Catch2 macros, and will generate errors
    // if you declare a lambda inside a CHECK macro.  Therefore we move the line with the
    // lambda outside of the CHECK.
    auto expected1 = {NonTrivialType(1)};
    auto size_check1 =
        std::equal(begin(data), end(data), begin(expected1), end(expected1),
                   [](auto const &l, auto const &r) { return l.n == r.n; });
    CHECK(size_check1);

    data.push_back(NonTrivialType(2));
    auto expected2 = {NonTrivialType(1), NonTrivialType(2)};
    auto size_check2 =
        std::equal(begin(data), end(data), begin(expected2), end(expected2),
                   [](auto const &l, auto const &r) { return l.n == r.n; });
    CHECK(size_check2);

    data.emplace_back(3);
    auto expected3 = {NonTrivialType(1), NonTrivialType(2), NonTrivialType(3)};
    auto size_check3 =
        std::equal(begin(data), end(data), begin(expected3), end(expected3),
                   [](auto const &l, auto const &r) { return l.n == r.n; });
    CHECK(size_check3);
  }

  SECTION("pop_back") {
    SECTION("with trivial type") {
      using test_t = PortsOfCall::static_vector<double, 5>;
      test_t data = {1, 2, 3, 4, 5};

      data.pop_back();
      test_t const expected = {1, 2, 3, 4};
      CHECK(std::equal(begin(data), end(data), begin(expected)));
    }

    SECTION("with non-trivial type") {
      using test_t = PortsOfCall::static_vector<NonTrivialType, 5>;
      test_t data = {NonTrivialType(1), NonTrivialType(2),
                     NonTrivialType(3), NonTrivialType(4),
                     NonTrivialType(5)};

      data.pop_back();
      test_t const expected = {NonTrivialType(1), NonTrivialType(2),
                               NonTrivialType(3), NonTrivialType(4)};

      auto size_check =
          std::equal(begin(data), end(data), begin(expected), end(expected),
                     [](auto const &l, auto const &r) { return l.n == r.n; });
      CHECK(size_check);
    }
  }
}
