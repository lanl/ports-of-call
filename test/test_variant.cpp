// Variant Copyright Michael Park, 2015-2017
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

// GPU compatibility corrections are © 2025. Triad National Security,
// LLC under U.S. Government contract 89233218CNA000001 for Los Alamos
// National Laboratory (LANL), which is operated by Triad National
// Security, LLC for the U.S. Department of Energy/National Nuclear
// Security Administration.

#include "ports-of-call/variant/variant.hpp"

#ifndef CATCH_CONFIG_FAST_COMPILE
#define CATCH_CONFIG_FAST_COMPILE
#include <catch2/catch_test_macros.hpp>
#endif

#include <iostream>
#include <map>
#include <string>
#include <utility>
#include <vector>

enum Qual { Ptr, ConstPtr, LRef, ConstLRef, RRef, ConstRRef };

struct get_qual_t {
  constexpr Qual operator()(int *) const { return Ptr; }
  constexpr Qual operator()(const int *) const { return ConstPtr; }
  constexpr Qual operator()(int &) const { return LRef; }
  constexpr Qual operator()(const int &) const { return ConstLRef; }
  constexpr Qual operator()(int &&) const { return RRef; }
  constexpr Qual operator()(const int &&) const { return ConstRRef; }
};

constexpr get_qual_t get_qual{};

#ifdef MPARK_EXCEPTIONS
struct CopyConstruction : std::exception {};
struct CopyAssignment : std::exception {};
struct MoveConstruction : std::exception {};
struct MoveAssignment : std::exception {};

struct copy_thrower_t {
  constexpr copy_thrower_t() {}
  [[noreturn]] copy_thrower_t(const copy_thrower_t &) { throw CopyConstruction{}; }
  copy_thrower_t(copy_thrower_t &&) = default;
  copy_thrower_t &operator=(const copy_thrower_t &) { throw CopyAssignment{}; }
  copy_thrower_t &operator=(copy_thrower_t &&) = default;
};

inline bool operator<(const copy_thrower_t &, const copy_thrower_t &) noexcept {
  return false;
}

inline bool operator>(const copy_thrower_t &, const copy_thrower_t &) noexcept {
  return false;
}

inline bool operator<=(const copy_thrower_t &, const copy_thrower_t &) noexcept {
  return true;
}

inline bool operator>=(const copy_thrower_t &, const copy_thrower_t &) noexcept {
  return true;
}

inline bool operator==(const copy_thrower_t &, const copy_thrower_t &) noexcept {
  return true;
}

inline bool operator!=(const copy_thrower_t &, const copy_thrower_t &) noexcept {
  return false;
}

struct move_thrower_t {
  constexpr move_thrower_t() {}
  move_thrower_t(const move_thrower_t &) = default;
  [[noreturn]] move_thrower_t(move_thrower_t &&) { throw MoveConstruction{}; }
  move_thrower_t &operator=(const move_thrower_t &) = default;
  move_thrower_t &operator=(move_thrower_t &&) { throw MoveAssignment{}; }
};

inline bool operator<(const move_thrower_t &, const move_thrower_t &) noexcept {
  return false;
}

inline bool operator>(const move_thrower_t &, const move_thrower_t &) noexcept {
  return false;
}

inline bool operator<=(const move_thrower_t &, const move_thrower_t &) noexcept {
  return true;
}

inline bool operator>=(const move_thrower_t &, const move_thrower_t &) noexcept {
  return true;
}

inline bool operator==(const move_thrower_t &, const move_thrower_t &) noexcept {
  return true;
}

inline bool operator!=(const move_thrower_t &, const move_thrower_t &) noexcept {
  return false;
}

#endif

struct unary {
  int operator()(int) const noexcept { return 0; }
  int operator()(const std::string &) const noexcept { return 1; }
}; // unary

struct binary {
  int operator()(int, int) const noexcept { return 0; }
  int operator()(int, const std::string &) const noexcept { return 1; }
  int operator()(const std::string &, int) const noexcept { return 2; }
  int operator()(const std::string &, const std::string &) const noexcept { return 3; }
}; // binary

TEST_CASE("Variant hello world", "[Variant]") {
  // direct initialization.
  PortsOfCall::variant<int, std::string> v("hello world!");

  // direct access via reference.
  REQUIRE("hello world!" == PortsOfCall::get<std::string>(v));

  // bad access.
#ifdef MPARK_EXCEPTIONS
  REQUIRE_THROWS_AS(PortsOfCall::get<int>(v), PortsOfCall::bad_variant_access);
#endif

  // copy construction.
  PortsOfCall::variant<int, std::string> w(v);

  // direct access via pointer.
  REQUIRE_FALSE(PortsOfCall::get_if<int>(&w));
  REQUIRE(PortsOfCall::get_if<std::string>(&w));

  // diff-type assignment.
  v = 42;

  // single visitation.
  REQUIRE(0 == PortsOfCall::visit(unary{}, v));

  // same-type assignment.
  w = "hello";

  REQUIRE(v != w);

  // make `w` equal to `v`.
  w = 42;

  REQUIRE(v == w);

  // binary visitation.
  REQUIRE(0 == PortsOfCall::visit(binary{}, v, w));
}

struct JsonIsh {
  JsonIsh(bool b) : data(b) {}
  JsonIsh(int i) : data(i) {}
  JsonIsh(std::string s) : data(std::move(s)) {}
  JsonIsh(std::vector<JsonIsh> v) : data(std::move(v)) {}

  PortsOfCall::variant<bool, int, std::string, std::vector<JsonIsh>> data;
};
SCENARIO("Variant can be used for type erasure in something like Json", "[Variant]") {
  WHEN("We want to hold a bool") {
    JsonIsh json_ish = true;
    THEN("We can!") {
      REQUIRE(PortsOfCall::get<bool>(json_ish.data));
      json_ish = false;
      REQUIRE_FALSE(PortsOfCall::get<bool>(json_ish.data));
    }
  }
  WHEN("We want to hold an int") {
    JsonIsh json_ish = 42;
    THEN("We can!") { REQUIRE(PortsOfCall::get<int>(json_ish.data)); }
  }
  WHEN("We want to hold a string") {
    JsonIsh json_ish = std::string("hello");
    THEN("We can!") { REQUIRE("hello" == PortsOfCall::get<std::string>(json_ish.data)); }
  }
  WHEN("We want an array of type erased objects") {
    JsonIsh json_ish = std::vector<JsonIsh>{true, 42, std::string("world")};
    const auto &array = PortsOfCall::get<std::vector<JsonIsh>>(json_ish.data);
    THEN("We can!") {
      REQUIRE(PortsOfCall::get<bool>(array[0].data));
      REQUIRE(42 == PortsOfCall::get<int>(array[1].data));
      REQUIRE("world" == PortsOfCall::get<std::string>(array[2].data));
    }
  }
}

template <bool same>
struct Obj {
  constexpr Obj() {}
  Obj(const Obj &) noexcept { REQUIRE(!same); }
  Obj(Obj &&) = default;
  Obj &operator=(const Obj &) noexcept {
    REQUIRE(true);
    return *this;
  }
  Obj &operator=(Obj &&) = delete;
};

TEST_CASE("Variant copy assignment, same type", "[Variant]") {
  PortsOfCall::variant<Obj<true>, int> v, w;
  v = w;
}

TEST_CASE("Variant copy assignment, different type", "[Variant]") {
  PortsOfCall::variant<Obj<false>, int> v(42), w;
  v = w;
}

#ifdef MPARK_EXCEPTIONS
TEST_CASE("Variant copy assignment, Valueless by exception", "[Variant]") {
  PortsOfCall::variant<int, move_thrower_t> v(42);
  REQUIRE_THROWS_AS(v = move_thrower_t{}, MoveConstruction);
  REQUIRE(v.valueless_by_exception());
  PortsOfCall::variant<int, move_thrower_t> w(42);
  w = v;
  REQUIRE(w.valueless_by_exception());
}
#endif

TEST_CASE("Variant forward assignment, same type", "[Variant]") {
  PortsOfCall::variant<int, std::string> v(101);
  REQUIRE(101 == PortsOfCall::get<int>(v));
  v = 202;
  REQUIRE(202 == PortsOfCall::get<int>(v));
}

TEST_CASE("Variant forward assignment, same type, with exact match", "[Variant]") {
  PortsOfCall::variant<int, std::string> v(42);
  REQUIRE(42 == PortsOfCall::get<int>(v));
  v = "42";
  REQUIRE("42" == PortsOfCall::get<std::string>(v));
}

TEST_CASE("Variant forward assignment, different type, with exact match", "[Variant]") {
  PortsOfCall::variant<const char *, std::string> v;
  v = std::string("hello");
  REQUIRE("hello" == PortsOfCall::get<std::string>(v));
}

TEST_CASE("Variant forward assignment, better match", "[Variant]") {
  PortsOfCall::variant<int, double> v;
  // `char` -> `int` is better than `char` -> `double`
  v = 'x';
  REQUIRE(static_cast<int>('x') == PortsOfCall::get<int>(v));
}

struct x {};
TEST_CASE("Variant forward assignment, no match", "[Variant]") {
  static_assert(!std::is_assignable<PortsOfCall::variant<int, std::string>, x>{},
                "variant<int, std::string> v; v = x;");
}

TEST_CASE("Variant forward assignment, WideningOrAmbiguous", "[Variant]") {
#if defined(__clang__) || !defined(__GNUC__) || __GNUC__ >= 5
  static_assert(std::is_assignable<PortsOfCall::variant<short, long>, int>{},
                "variant<short, long> v; v = 42;");
#else
  static_assert(!std::is_assignable<PortsOfCall::variant<short, long>, int>{},
                "variant<short, long> v; v = 42;");
#endif
}

TEST_CASE("Variant forward assignment, optimizations for same type", "[Variant]") {
  PortsOfCall::variant<int, std::string> v("hello world!");
  // Check `v`.
  const std::string &x = PortsOfCall::get<std::string>(v);
  REQUIRE("hello world!" == x);
  // Save the "hello world!"'s capacity.
  auto capacity = x.capacity();
  // Use `std::string::operator=(const char *)` to assign into `v`.
  v = "hello";
  // Check `v`.
  const std::string &y = PortsOfCall::get<std::string>(v);
  REQUIRE("hello" == y);
  // Since "hello" is shorter than "hello world!", we should have preserved the
  // existing capacity of the string!.
  REQUIRE(capacity == y.capacity());
}

#ifdef MPARK_EXCEPTIONS
TEST_CASE("Variant forward assignment, ThrowOnAssignment", "[Variant]") {
  PortsOfCall::variant<int, move_thrower_t> v(
      PortsOfCall::in_place_type_t<move_thrower_t>{});
  // Since `variant` is already in `move_thrower_t`, assignment optimization
  // kicks and we simply invoke
  // `move_thrower_t &operator=(move_thrower_t &&);` which throws.
  REQUIRE_THROWS_AS(v = move_thrower_t{}, MoveAssignment);
  REQUIRE_FALSE(v.valueless_by_exception());
  REQUIRE(1u == v.index());
  // We can still assign into a variant in an invalid state.
  v = 42;
  // Check `v`.
  REQUIRE_FALSE(v.valueless_by_exception());
  REQUIRE(42 == PortsOfCall::get<int>(v));
}
#endif

SCENARIO("Variant move assignment", "[Variant]") {
  WHEN("We try to assign variants of the same type") {
    PortsOfCall::variant<Obj<true>, int> v, w;
    THEN("we are succesful") { v = std::move(w); }
  }
  WHEN("We try to assign variants of different types") {
    PortsOfCall::variant<Obj<false>, int> v(42), w;
    THEN("We are succesful") { v = std::move(w); }
  }
}

#ifdef MPARK_EXCEPTIONS
TEST_CASE("Variant move assignment, ValuelessByException", "[Variant]") {
  PortsOfCall::variant<int, move_thrower_t> v(42);
  REQUIRE_THROWS_AS(v = move_thrower_t{}, MoveConstruction);
  REQUIRE(v.valueless_by_exception());
  PortsOfCall::variant<int, move_thrower_t> w(42);
  w = std::move(v);
  REQUIRE(w.valueless_by_exception());
}
#endif

TEST_CASE("Variant copy constructor", "[Variant]") {
  // `v`
  PortsOfCall::variant<int, std::string> v("hello");
  REQUIRE("hello" == PortsOfCall::get<std::string>(v));
  // `w`
  PortsOfCall::variant<int, std::string> w(v);
  REQUIRE("hello" == PortsOfCall::get<std::string>(w));
  // Check `v`
  REQUIRE("hello" == PortsOfCall::get<std::string>(v));

  /* constexpr */ {
    // `cv`
    constexpr PortsOfCall::variant<int, const char *> cv(42);
    static_assert(42 == PortsOfCall::get<int>(cv), "");
    // `cw`
    constexpr PortsOfCall::variant<int, const char *> cw(cv);
    static_assert(42 == PortsOfCall::get<int>(cw), "");
  }
}

#ifdef MPARK_EXCEPTIONS
TEST_CASE("Variant copy constructor, ValuelessByException", "[Variant]") {
  PortsOfCall::variant<int, move_thrower_t> v(42);
  REQUIRE_THROWS_AS(v = move_thrower_t{}, MoveConstruction);
  REQUIRE(v.valueless_by_exception());
  PortsOfCall::variant<int, move_thrower_t> w(v);
  REQUIRE(w.valueless_by_exception());
}
#endif

TEST_CASE("Variant default constructor", "[Variant]") {
  PortsOfCall::variant<int, std::string> v;
  REQUIRE(0 == PortsOfCall::get<0>(v));

  /* constexpr */ {
    constexpr PortsOfCall::variant<int> cv{};
    static_assert(0 == PortsOfCall::get<0>(cv), "");
  }
}

SCENARIO("Variant forward constructor", "[Variant]") {
  WHEN("direct construction") {
    PortsOfCall::variant<int, std::string> v(42);
    REQUIRE(42 == PortsOfCall::get<int>(v));

    /* constexpr */ {
      constexpr PortsOfCall::variant<int, const char *> cv(42);
      static_assert(42 == PortsOfCall::get<int>(cv), "");
    }
  }
  WHEN("direct conversion") {
    PortsOfCall::variant<int, std::string> v("42");
    REQUIRE("42" == PortsOfCall::get<std::string>(v));

    /* constexpr */ {
      constexpr PortsOfCall::variant<int, const char *> cv('A');
      static_assert(65 == PortsOfCall::get<int>(cv), "");
    }
  }
  WHEN("copy initialization") {
    PortsOfCall::variant<int, std::string> v = 42;
    REQUIRE(42 == PortsOfCall::get<int>(v));

    /* constexpr */ {
      constexpr PortsOfCall::variant<int, const char *> cv = 42;
      static_assert(42 == PortsOfCall::get<int>(cv), "");
    }
  }
  WHEN("copy initialization conversion") {
    PortsOfCall::variant<int, std::string> v = "42";
    REQUIRE("42" == PortsOfCall::get<std::string>(v));

    /* constexpr */ {
      constexpr PortsOfCall::variant<int, const char *> cv = 'A';
      static_assert(65 == PortsOfCall::get<int>(cv), "");
    }
  }
}

SCENARIO("Variant in-place construction", "[Variant]") {

  WHEN("Ctor_InPlace, IndexDirect") {
    PortsOfCall::variant<int, std::string> v(PortsOfCall::in_place_index_t<0>{}, 42);
    REQUIRE(42 == PortsOfCall::get<0>(v));

    /* constexpr */ {
      constexpr PortsOfCall::variant<int, const char *> cv(
          PortsOfCall::in_place_index_t<0>{}, 42);
      static_assert(42 == PortsOfCall::get<0>(cv), "");
    }
  }

  WHEN("Ctor_InPlace, IndexDirectDuplicate") {
    PortsOfCall::variant<int, int> v(PortsOfCall::in_place_index_t<0>{}, 42);
    REQUIRE(42 == PortsOfCall::get<0>(v));

    /* constexpr */ {
      constexpr PortsOfCall::variant<int, int> cv(PortsOfCall::in_place_index_t<0>{}, 42);
      static_assert(42 == PortsOfCall::get<0>(cv), "");
    }
  }

  WHEN("Ctor_InPlace, IndexConversion") {
    PortsOfCall::variant<int, std::string> v(PortsOfCall::in_place_index_t<1>{}, "42");
    REQUIRE("42" == PortsOfCall::get<1>(v));

    /* constexpr */ {
      constexpr PortsOfCall::variant<int, const char *> cv(
          PortsOfCall::in_place_index_t<0>{}, 1.1);
      static_assert(1 == PortsOfCall::get<0>(cv), "");
    }
  }

  WHEN("Ctor_InPlace, IndexInitializerList") {
    PortsOfCall::variant<int, std::string> v(PortsOfCall::in_place_index_t<1>{},
                                             {'4', '2'});
    REQUIRE("42" == PortsOfCall::get<1>(v));
  }

  WHEN("Ctor_InPlace, TypeDirect") {
    PortsOfCall::variant<int, std::string> v(PortsOfCall::in_place_type_t<std::string>{},
                                             "42");
    REQUIRE("42" == PortsOfCall::get<std::string>(v));

    /* constexpr */ {
      constexpr PortsOfCall::variant<int, const char *> cv(
          PortsOfCall::in_place_type_t<int>{}, 42);
      static_assert(42 == PortsOfCall::get<int>(cv), "");
    }
  }

  WHEN("Ctor_InPlace, TypeConversion") {
    PortsOfCall::variant<int, std::string> v(PortsOfCall::in_place_type_t<int>{}, 42.5);
    REQUIRE(42 == PortsOfCall::get<int>(v));

    /* constexpr */ {
      constexpr PortsOfCall::variant<int, const char *> cv(
          PortsOfCall::in_place_type_t<int>{}, 42.5);
      static_assert(42 == PortsOfCall::get<int>(cv), "");
    }
  }

  WHEN("Ctor_InPlace, TypeInitializerList") {
    PortsOfCall::variant<int, std::string> v(PortsOfCall::in_place_type_t<std::string>{},
                                             {'4', '2'});
    REQUIRE("42" == PortsOfCall::get<std::string>(v));
  }
}

SCENARIO("Variant move constructor", "[Variant]") {
  WHEN("We try to move a value") {
    // `v`
    PortsOfCall::variant<int, std::string> v("hello");
    REQUIRE("hello" == PortsOfCall::get<std::string>(v));
    // `w`
    PortsOfCall::variant<int, std::string> w(std::move(v));
    REQUIRE("hello" == PortsOfCall::get<std::string>(w));

    /* constexpr */ {
      // `cv`
      constexpr PortsOfCall::variant<int, const char *> cv(42);
      static_assert(42 == PortsOfCall::get<int>(cv), "");
      // `cw`
      constexpr PortsOfCall::variant<int, const char *> cw(std::move(cv));
      static_assert(42 == PortsOfCall::get<int>(cw), "");
    }
  }

#ifdef MPARK_EXCEPTIONS
  WHEN("We check ValuelessByException") {
    PortsOfCall::variant<int, move_thrower_t> v(42);
    REQUIRE_THROWS_AS(v = move_thrower_t{}, MoveConstruction);
    REQUIRE(v.valueless_by_exception());
    PortsOfCall::variant<int, move_thrower_t> w(std::move(v));
    REQUIRE(w.valueless_by_exception());
  }
#endif
}

struct DestructibleObj {
  DestructibleObj(bool &dtor_called) : dtor_called_(dtor_called) {}
  ~DestructibleObj() { dtor_called_ = true; }
  bool &dtor_called_;
}; // DestructibleObj

TEST_CASE("Variant destructor", "[Variant]") {
  bool dtor_called = false;
  // Construct/Destruct `DestructibleObj`.
  {
    PortsOfCall::variant<DestructibleObj> v(
        PortsOfCall::in_place_type_t<DestructibleObj>{}, dtor_called);
  }
  // Check that the destructor was called.
  REQUIRE(dtor_called);
}

SCENARIO("Variant Get", "[Variant]") {
  WHEN("We have an int variant") {
    PortsOfCall::variant<int, std::string> v(42);
    THEN("Holds alternative returns the correct values") {
      REQUIRE(PortsOfCall::holds_alternative<0>(v));
      REQUIRE_FALSE(PortsOfCall::holds_alternative<1>(v));
      REQUIRE(PortsOfCall::holds_alternative<int>(v));
      REQUIRE_FALSE(PortsOfCall::holds_alternative<std::string>(v));
    }

    /* constexpr */ {
      constexpr PortsOfCall::variant<int, const char *> cv(42);
      static_assert(PortsOfCall::holds_alternative<0>(cv), "");
      static_assert(!PortsOfCall::holds_alternative<1>(cv), "");
      static_assert(PortsOfCall::holds_alternative<int>(cv), "");
      static_assert(!PortsOfCall::holds_alternative<const char *>(cv), "");
    }
  }

  WHEN("We have an int variant") {
    PortsOfCall::variant<int> v(42);
    REQUIRE(PortsOfCall::get<int>(v) == 42);
    THEN("It has the right lvalue and rvalue types") {
      REQUIRE(LRef == get_qual(PortsOfCall::get<int>(v)));
      REQUIRE(RRef == get_qual(PortsOfCall::get<int>(std::move(v))));
    }
  }

  WHEN("We have a const int variant") {
    PortsOfCall::variant<const int> v(42);
    REQUIRE(PortsOfCall::get<const int>(v) == 42);
    THEN("It has the right lvalue and rvalue types") {
      REQUIRE(ConstLRef == get_qual(PortsOfCall::get<const int>(v)));
      REQUIRE(ConstRRef == get_qual(PortsOfCall::get<const int>(std::move(v))));
    }
  }

  WHEN("We have a const mutable type") {
    const PortsOfCall::variant<int> v(42);
    REQUIRE(PortsOfCall::get<int>(v) == 42);
    THEN("It has the right lvalue and rvalue types") {
      REQUIRE(ConstLRef == get_qual(PortsOfCall::get<int>(v)));
      REQUIRE(ConstRRef == get_qual(PortsOfCall::get<int>(std::move(v))));
      /* constexpr */ {
        constexpr PortsOfCall::variant<int> cv(42);
        static_assert(42 == PortsOfCall::get<int>(cv), "");
        // Check qualifier.
        static_assert(ConstLRef == get_qual(PortsOfCall::get<int>(cv)), "");
        static_assert(ConstRRef == get_qual(PortsOfCall::get<int>(std::move(cv))), "");
      }
    }
  }

  WHEN("We have a const var const type") {
    const PortsOfCall::variant<const int> v(42);
    REQUIRE(PortsOfCall::get<const int>(v) == 42);
    THEN("It has the right lvalue and rvalue types") {
      REQUIRE(ConstLRef == get_qual(PortsOfCall::get<const int>(v)));
      REQUIRE(ConstRRef == get_qual(PortsOfCall::get<const int>(std::move(v))));
      /* constexpr */ {
        constexpr PortsOfCall::variant<const int> cv(42);
        static_assert(42 == PortsOfCall::get<const int>(cv), "");
        // Check qualifier.
        static_assert(ConstLRef == get_qual(PortsOfCall::get<const int>(cv)), "");
        static_assert(ConstRRef == get_qual(PortsOfCall::get<const int>(std::move(cv))),
                      "");
      }
    }
  }

#ifdef MPARK_EXCEPTIONS
  WHEN("We call get on a valueless variant") {
    THEN("We get an exception") {
      PortsOfCall::variant<int, move_thrower_t> v(42);
      REQUIRE_THROWS_AS(v = move_thrower_t{}, MoveConstruction);
      REQUIRE(v.valueless_by_exception());
      REQUIRE_THROWS_AS(PortsOfCall::get<int>(v), PortsOfCall::bad_variant_access);
      REQUIRE_THROWS_AS(PortsOfCall::get<move_thrower_t>(v),
                        PortsOfCall::bad_variant_access);
    }
  }
#endif

  WHEN("We conditionally get on a mutable type") {
    THEN("The value is extracted as expected") {
      PortsOfCall::variant<int> v(42);
      REQUIRE(42 == *PortsOfCall::get_if<int>(&v));
      // Check qualifier.
      REQUIRE(Ptr == get_qual(PortsOfCall::get_if<int>(&v)));
    }
  }

  WHEN("We conditionally call get on a mutable const var type") {
    THEN("The value is extracted as expected") {
      PortsOfCall::variant<const int> v(42);
      REQUIRE(42 == *PortsOfCall::get_if<const int>(&v));
      // Check qualifier.
      REQUIRE(ConstPtr == get_qual(PortsOfCall::get_if<const int>(&v)));
    }
  }

  WHEN("We conditionally call get on a const variant of a mutable type") {
    THEN("The value is extracted as expected") {
      const PortsOfCall::variant<int> v(42);
      REQUIRE(42 == *PortsOfCall::get_if<int>(&v));
      // Check qualifier.
      REQUIRE(ConstPtr == get_qual(PortsOfCall::get_if<int>(&v)));

      /* constexpr */ {
        static constexpr PortsOfCall::variant<int> cv(42);
        static_assert(42 == *PortsOfCall::get_if<int>(&cv), "");
        // Check qualifier.
        static_assert(ConstPtr == get_qual(PortsOfCall::get_if<int>(&cv)), "");
      }
    }
  }

  WHEN("We conditionally call get on a const variant of a const type") {
    THEN("The value is extracted as expected") {
      const PortsOfCall::variant<const int> v(42);
      REQUIRE(42 == *PortsOfCall::get_if<const int>(&v));
      // Check qualifier.
      REQUIRE(ConstPtr == get_qual(PortsOfCall::get_if<const int>(&v)));

      /* constexpr */ {
        static constexpr PortsOfCall::variant<const int> cv(42);
        static_assert(42 == *PortsOfCall::get_if<const int>(&cv), "");
        // Check qualifier.
        static_assert(ConstPtr == get_qual(PortsOfCall::get_if<const int>(&cv)), "");
      }
    }
  }

#ifdef MPARK_EXCEPTONS
  WHEN("We call get if on a variant with an invalid value") {
    PortsOfCall::variant<int, move_thrower_t> v(42);
    THEN("Exceptions are raised appropriately") {
      REQUIRE_THROWS_AS(v = move_thrower_t{}, MoveConstruction);
      REQUIRE(v.valueless_by_exception());
      REQUIRE(nullptr == PortsOfCall::get_if<int>(&v));
      REQUIRE(nullptr == PortsOfCall::get_if<move_thrower_t>(&v));
    }
  }
#endif
}

SCENARIO("Test variant hash", "[Variant]") {
  WHEN("The variant contains a monostate") {
    PortsOfCall::variant<int, PortsOfCall::monostate, std::string> v(
        PortsOfCall::monostate{});
    THEN("The hash functions for the variant and the monostate are distinct") {
      // Construct hash function objects.
      std::hash<PortsOfCall::monostate> monostate_hash;
      std::hash<PortsOfCall::variant<int, PortsOfCall::monostate, std::string>>
          variant_hash;
      // Check the hash.
      REQUIRE(monostate_hash(PortsOfCall::monostate{}) != variant_hash(v));
    }
  }
  WHEN("The variant contains a string") {
    PortsOfCall::variant<int, std::string> v("hello");
    REQUIRE("hello" == PortsOfCall::get<std::string>(v));
    THEN("The hash functions for the variant and the string are distinct") {
      // Construct hash function objects.
      std::hash<std::string> string_hash;
      std::hash<PortsOfCall::variant<int, std::string>> variant_hash;
      // Check the hash.
      REQUIRE(string_hash("hello") != variant_hash(v));
    }
  }
}

#ifdef MPARK_INCOMPLETE_TYPE_TRAITS
// https://github.com/mpark/variant/issues/34
struct S {
  S(const S &) = default;
  S(S &&) = default;
  S &operator=(const S &) = default;
  S &operator=(S &&) = default;

  PortsOfCall::variant<std::map<std::string, S>> value;
};
#endif

// https://github.com/mpark/variant/pull/57
TEST_CASE("Variant Issue 57", "[Variant]") {
  std::vector<PortsOfCall::variant<int, std::unique_ptr<int>>> vec;
  vec.emplace_back(0);
  REQUIRE(true);
}

SCENARIO("Modify variant in place", "[Variant]") {
  WHEN("We have a variant that takes int and string") {
    PortsOfCall::variant<int, std::string> v;
    THEN("We can set it to 42") {
      v.emplace<1>("42");
      REQUIRE("42" == PortsOfCall::get<1>(v));
      REQUIRE("42" == PortsOfCall::get<std::string>(v));
    }
    THEN("We can assign it via an initializer list") {
      v.emplace<1>({'4', '2'});
      REQUIRE("42" == PortsOfCall::get<1>(v));
      REQUIRE("42" == PortsOfCall::get<std::string>(v));
    }
  }
  WHEN("We have a variant that takes int and int") {
    PortsOfCall::variant<int, int> v;
    THEN("We can set it to 42") {
      v.emplace<1>(42);
      REQUIRE(42 == PortsOfCall::get<1>(v));
    }
    THEN("We can convert from a float implicitly") {
      v.emplace<1>(1.1);
      REQUIRE(1 == PortsOfCall::get<1>(v));
    }
  }
}

SCENARIO("Variant comparisons", "[Variant]") {
  GIVEN("Two variants of the same type and value") {
    PortsOfCall::variant<int, std::string> v(0), w(0);
    THEN("Comparisons match") {
      // `v` op `w`
      REQUIRE(v == w);
      REQUIRE_FALSE(v != w);
      REQUIRE_FALSE(v < w);
      REQUIRE_FALSE(v > w);
      REQUIRE(v <= w);
      REQUIRE(v >= w);
      // `w` op `v`
      REQUIRE(w == v);
      REQUIRE_FALSE(w != v);
      REQUIRE_FALSE(w < v);
      REQUIRE_FALSE(w > v);
      REQUIRE(w <= v);
      REQUIRE(w >= v);

#ifdef MPARK_CPP11_CONSTEXPR
      /* constexpr */ {
        constexpr PortsOfCall::variant<int, const char *> cv(0), cw(0);
        // `cv` op `cw`
        static_assert(cv == cw, "");
        static_assert(!(cv != cw), "");
        static_assert(!(cv < cw), "");
        static_assert(!(cv > cw), "");
        static_assert(cv <= cw, "");
        static_assert(cv >= cw, "");
        // `cw` op `cv`
        static_assert(cw == cv, "");
        static_assert(!(cw != cv), "");
        static_assert(!(cw < cv), "");
        static_assert(!(cw > cv), "");
        static_assert(cw <= cv, "");
        static_assert(cw >= cv, "");
      }
#endif
    }
  }

  GIVEN("Two variants of the same type and different values") {
    PortsOfCall::variant<int, std::string> v(0), w(1);
    THEN("Comparisons match") {
      // `v` op `w`
      REQUIRE_FALSE(v == w);
      REQUIRE(v != w);
      REQUIRE(v < w);
      REQUIRE_FALSE(v > w);
      REQUIRE(v <= w);
      REQUIRE_FALSE(v >= w);
      // `w` op `v`
      REQUIRE_FALSE(w == v);
      REQUIRE(w != v);
      REQUIRE_FALSE(w < v);
      REQUIRE(w > v);
      REQUIRE_FALSE(w <= v);
      REQUIRE(w >= v);

#ifdef MPARK_CPP11_CONSTEXPR
      /* constexpr */ {
        constexpr PortsOfCall::variant<int, const char *> cv(0), cw(1);
        // `cv` op `cw`
        static_assert(!(cv == cw), "");
        static_assert(cv != cw, "");
        static_assert(cv < cw, "");
        static_assert(!(cv > cw), "");
        static_assert(cv <= cw, "");
        static_assert(!(cv >= cw), "");
        // `cw` op `cv`
        static_assert(!(cw == cv), "");
        static_assert(cw != cv, "");
        static_assert(!(cw < cv), "");
        static_assert(cw > cv, "");
        static_assert(!(cw <= cv), "");
        static_assert(cw >= cv, "");
      }
#endif
    }
  }

  GIVEN("Two variants of the different types, but the same value") {
    PortsOfCall::variant<int, unsigned int> v(0), w(0u);
    THEN("Comparisons match") {
      // `v` op `w`
      REQUIRE_FALSE(v == w);
      REQUIRE(v != w);
      REQUIRE(v < w);
      REQUIRE_FALSE(v > w);
      REQUIRE(v <= w);
      REQUIRE_FALSE(v >= w);
      // `w` op `v`
      REQUIRE_FALSE(w == v);
      REQUIRE(w != v);
      REQUIRE_FALSE(w < v);
      REQUIRE(w > v);
      REQUIRE_FALSE(w <= v);
      REQUIRE(w >= v);

#ifdef MPARK_CPP11_CONSTEXPR
      /* constexpr */ {
        constexpr PortsOfCall::variant<int, unsigned int> cv(0), cw(0u);
        // `cv` op `cw`
        static_assert(!(cv == cw), "");
        static_assert(cv != cw, "");
        static_assert(cv < cw, "");
        static_assert(!(cv > cw), "");
        static_assert(cv <= cw, "");
        static_assert(!(cv >= cw), "");
        // `cw` op `cv`
        static_assert(!(cw == cv), "");
        static_assert(cw != cv, "");
        static_assert(!(cw < cv), "");
        static_assert(cw > cv, "");
        static_assert(!(cw <= cv), "");
        static_assert(cw >= cv, "");
      }
#endif
    }
  }

  GIVEN("Two variants of different types and different values") {
    PortsOfCall::variant<int, unsigned int> v(0), w(1u);
    THEN("Comparisons match") {
      // `v` op `w`
      REQUIRE_FALSE(v == w);
      REQUIRE(v != w);
      REQUIRE(v < w);
      REQUIRE_FALSE(v > w);
      REQUIRE(v <= w);
      REQUIRE_FALSE(v >= w);
      // `w` op `v`
      REQUIRE_FALSE(w == v);
      REQUIRE(w != v);
      REQUIRE_FALSE(w < v);
      REQUIRE(w > v);
      REQUIRE_FALSE(w <= v);
      REQUIRE(w >= v);

#ifdef MPARK_CPP11_CONSTEXPR
      /* constexpr */ {
        constexpr PortsOfCall::variant<int, unsigned int> cv(0), cw(1u);
        // `cv` op `cw`
        static_assert(!(cv == cw), "");
        static_assert(cv != cw, "");
        static_assert(cv < cw, "");
        static_assert(!(cv > cw), "");
        static_assert(cv <= cw, "");
        static_assert(!(cv >= cw), "");
        // `cw` op `cv`
        static_assert(!(cw == cv), "");
        static_assert(cw != cv, "");
        static_assert(!(cw < cv), "");
        static_assert(cw > cv, "");
        static_assert(!(cw <= cv), "");
        static_assert(cw >= cv, "");
      }
#endif
    }
  }
}

namespace swp_detail {

struct Obj {
  Obj(size_t *dtor_count) : dtor_count_(dtor_count) {}
  Obj(const Obj &) = default;
  Obj(Obj &&) = default;
  ~Obj() { ++(*dtor_count_); }
  Obj &operator=(const Obj &) = default;
  Obj &operator=(Obj &&) = default;
  size_t *dtor_count_;
}; // Obj
using V = Obj;
struct W {
  W(size_t *dtor_count) : dtor_count_(dtor_count) {}
  W(const W &) = default;
  W(W &&) = default;
  ~W() { ++(*dtor_count_); }
  W &operator=(const W &) = default;
  W &operator=(W &&) = default;
  size_t *dtor_count_;
}; // W

static void swap(Obj &lhs, Obj &rhs) noexcept {
  std::swap(lhs.dtor_count_, rhs.dtor_count_);
}

} // namespace swp_detail

SCENARIO("Swapping two variants", "[Variant]") {
  GIVEN("two variants holding the same type") {
    PortsOfCall::variant<int, std::string> v("hello");
    PortsOfCall::variant<int, std::string> w("world");
    REQUIRE("hello" == PortsOfCall::get<std::string>(v));
    REQUIRE("world" == PortsOfCall::get<std::string>(w));

    THEN("They can be swapped") {
      std::swap(v, w);
      REQUIRE("world" == PortsOfCall::get<std::string>(v));
      REQUIRE("hello" == PortsOfCall::get<std::string>(w));
    }
  }
  GIVEN("Two variants holding different types") {
    PortsOfCall::variant<int, std::string> v(42);
    PortsOfCall::variant<int, std::string> w("hello");
    REQUIRE(42 == PortsOfCall::get<int>(v));
    REQUIRE("hello" == PortsOfCall::get<std::string>(w));

    THEN("They can be swapped") {
      std::swap(v, w);
      REQUIRE("hello" == PortsOfCall::get<std::string>(v));
      REQUIRE(42 == PortsOfCall::get<int>(w));
    }
  }
#ifdef MPARK_EXCEPTIONS
  GIVEN("Two variants, one holding a corrupted value") {
    PortsOfCall::variant<int, move_thrower_t> v(42), w(42);
    REQUIRE_THROWS_AS(w = move_thrower_t{}, MoveConstruction);
    REQUIRE(42 == PortsOfCall::get<int>(v));
    REQUIRE(w.valueless_by_exception());
    THEN("They can be swapped") {
      std::swap(v, w);
      REQUIRE(v.valueless_by_exception());
      REQUIRE(42 == PortsOfCall::get<int>(w));
    }
  }
  GIVEN("Two variants that are both corrupted") {
    PortsOfCall::variant<int, move_thrower_t> v(42);
    REQUIRE_THROWS_AS(v = move_thrower_t{}, MoveConstruction);
    PortsOfCall::variant<int, move_thrower_t> w(v);
    REQUIRE(v.valueless_by_exception());
    REQUIRE(w.valueless_by_exception());
    THEN("They can be swapped") {
      std::swap(v, w);
      REQUIRE(v.valueless_by_exception());
      REQUIRE(w.valueless_by_exception());
    }
  }
#endif

/*
  GIVEN("Variants of destructibles") {
    size_t v_count = 0;
    size_t w_count = 0;
    WHEN("We 
    { // scope to trigger destructors
      PortsOfCallVariant<swp_detail::Obj> v{&v_count}, w { &w_count }
    }
  }
*/
}
