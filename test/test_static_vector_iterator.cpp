#include "ports-of-call/portability.hpp"
#include "ports-of-call/static_vector.hpp"

#ifndef CATCH_CONFIG_FAST_COMPILE
#define CATCH_CONFIG_FAST_COMPILE
#include <catch2/catch_test_macros.hpp>
#endif

#include <array>
#include <iterator>
#include <type_traits>

namespace static_vector_iterator_test {

template <typename T>
constexpr static bool really_const = std::is_const<std::remove_reference_t<T>>::value;
}

TEST_CASE("static_vector iterator", "[util][static_vector]") {
  using std::begin;
  using std::cbegin;
  using std::cend;
  using std::end;

  using PortsOfCall::static_vector;

  SECTION("non-const iterator") {
    static_vector<int, 5> sv{1, 2, 3};
    auto it = begin(sv);
    auto end_it = end(sv);

    SECTION("construct const iterator") {
      using static_vector_iterator_test::really_const;
      decltype(cbegin(sv)) const_it{it};
      REQUIRE(really_const<decltype(*const_it)>);
    }

    SECTION("operator++ and mutate") {
      while (it != end_it) {
        *it *= 2;
        ++it;
      }
      std::array<int, 3> expected{{2, 4, 6}};
      REQUIRE(std::equal(begin(sv), end(sv), begin(expected)));
    }

    SECTION("operator++(int)") {
      auto next_it = it++;
      REQUIRE(*next_it == 2);
    }

    SECTION("operator--") {
      auto next_it = it++;
      --next_it;
      REQUIRE(*next_it == 1);
    }

    SECTION("operator--(int)") {
      auto next_it = it++;
      auto prev_it = next_it--;
      REQUIRE(*prev_it == 1);
    }

    SECTION("operator+=") {
      it += 2;
      REQUIRE(*it == 3);
    }

    SECTION("operator+") {
      auto next_it = it + 2;
      REQUIRE(*next_it == 3);
    }

    SECTION("operator-=") {
      end_it -= 2;
      REQUIRE(*end_it == 2);
    }

    SECTION("operator-") {
      auto prev_it = end_it - 2;
      REQUIRE(*prev_it == 2);
    }

    SECTION("difference operator-") { REQUIRE(end_it - it == 3); }

    SECTION("operator==") { REQUIRE(it + 3 == end_it); }

    SECTION("operator!=") { REQUIRE(it != end_it); }

    SECTION("operator<") {
      REQUIRE(it < end_it);
      REQUIRE(not(it + 3 < end_it));
    }

    SECTION("operator<=") {
      REQUIRE(it <= end_it);
      REQUIRE(it + 3 <= end_it);
    }

    SECTION("operator>") {
      REQUIRE(end_it > it);
      REQUIRE(not(end_it > it + 3));
    }

    SECTION("operator>=") {
      REQUIRE(end_it >= it);
      REQUIRE(end_it >= it + 3);
    }

    SECTION("operator->") {
      REQUIRE(std::is_same<decltype(it.operator->()), int *>::value);
    }

    SECTION("operator[]") { REQUIRE(it[1] == 2); }

    SECTION("advance") {
      advance(it, 2);
      REQUIRE(*it == 3);
    }

    SECTION("next") {
      auto next_it = next(it, 2);
      REQUIRE(*next_it == 3);
    }

    SECTION("prev") {
      auto prev_it = prev(end_it, 2);
      REQUIRE(*prev_it == 2);
    }

    SECTION("distance") { REQUIRE(distance(it, end_it) == 3); }
  }

  SECTION("const iterator") {
    static_vector<int, 5> sv{1, 2, 3};
    auto it = cbegin(sv);
    auto end_it = cend(sv);

    SECTION("operator++ and mutate") {
      ++it;
      REQUIRE(*it == 2);
    }

    SECTION("operator++(int)") {
      auto next_it = it++;
      REQUIRE(*next_it == 2);
    }

    SECTION("operator--") {
      auto next_it = it++;
      --next_it;
      REQUIRE(*next_it == 1);
    }

    SECTION("operator--(int)") {
      auto next_it = it++;
      auto prev_it = next_it--;
      REQUIRE(*prev_it == 1);
    }

    SECTION("operator+=") {
      it += 2;
      REQUIRE(*it == 3);
    }

    SECTION("operator+") {
      auto next_it = it + 2;
      REQUIRE(*next_it == 3);
    }

    SECTION("operator-=") {
      end_it -= 2;
      REQUIRE(*end_it == 2);
    }

    SECTION("operator-") {
      auto prev_it = end_it - 2;
      REQUIRE(*prev_it == 2);
    }

    SECTION("difference operator-") { REQUIRE(end_it - it == 3); }

    SECTION("operator==") { REQUIRE(it + 3 == end_it); }

    SECTION("operator!=") { REQUIRE(it != end_it); }

    SECTION("operator<") {
      REQUIRE(it < end_it);
      REQUIRE(not(it + 3 < end_it));
    }

    SECTION("operator<=") {
      REQUIRE(it <= end_it);
      REQUIRE(it + 3 <= end_it);
    }

    SECTION("operator>") {
      REQUIRE(end_it > it);
      REQUIRE(not(end_it > it + 3));
    }

    SECTION("operator>=") {
      REQUIRE(end_it >= it);
      REQUIRE(end_it >= it + 3);
    }

    SECTION("operator->") {
      REQUIRE(std::is_same<decltype(it.operator->()), int const *>::value);
    }

    SECTION("operator[]") { REQUIRE(it[1] == 2); }

    SECTION("advance") {
      advance(it, 2);
      REQUIRE(*it == 3);
    }

    SECTION("next") {
      auto next_it = next(it, 2);
      REQUIRE(*next_it == 3);
    }

    SECTION("prev") {
      auto prev_it = prev(end_it, 2);
      REQUIRE(*prev_it == 2);
    }

    SECTION("distance") { REQUIRE(distance(it, end_it) == 3); }
  }
}
