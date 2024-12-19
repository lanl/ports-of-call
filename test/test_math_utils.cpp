#include <ports-of-call/math_utils.hpp>

#ifndef CATCH_CONFIG_FAST_COMPILE
#define CATCH_CONFIG_FAST_COMPILE
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#endif

#include <cmath>

TEST_CASE("power", "[math_utils]") {
  using Catch::Matchers::WithinRel;
  using PortsOfCall::Math::power;
  // Integral exponents
  CHECK(power(0, 0) == 1);
  CHECK_THAT(power(2.5, 0), WithinRel(1.00));
  CHECK_THAT(power(2.5, 1), WithinRel(2.50));
  CHECK_THAT(power(2.5, 2), WithinRel(6.25));
  CHECK_THAT(power(-2.5, 2), WithinRel(6.25));
  CHECK(power(3, 3) == 27);
  CHECK(power(-3, 3) == -27);
  CHECK_THAT(power(3.0, 3), WithinRel(27.00));
  CHECK_THAT(power(-3.0, 3), WithinRel(-27.00));
  CHECK(power(0, 5) == 0);
  CHECK_THAT(power(0.0, 5), WithinRel(0.00));
  CHECK_THAT(power(1.0, 5), WithinRel(1.00));
  CHECK_THAT(power(1.1, 4), WithinRel(1.4641));
  CHECK(power(0, 0) == std::pow(0, 0));
  CHECK_THAT(power(2.5, 0), WithinRel(std::pow(2.5, 0)));
  CHECK(power(0, 5) == std::pow(0, 5));
  CHECK_THAT(power(0.0, 5), WithinRel(std::pow(0.0, 5)));
  for (int n{0}; n < 10; ++n) {
    CHECK_THAT(power(3.14, n), WithinRel(std::pow(3.14, n)));
  }
  // Floating-point exponents
  CHECK_THAT(power(0.0, 0.0), WithinRel(1.00));
  CHECK_THAT(power(2.5, 0.0), WithinRel(1.00));
  CHECK_THAT(power(2.5, 1.0), WithinRel(2.50));
  CHECK_THAT(power(2.5, 2.0), WithinRel(6.25));
  CHECK_THAT(power(2.5, -2.0), WithinRel(0.16));
  CHECK_THAT(power(3, 3.0), WithinRel(27.00));
  CHECK_THAT(power(2, -3.0), WithinRel(0.125));
  CHECK_THAT(power(0.0, 5.0), WithinRel(0.00));
  CHECK_THAT(power(0.0, 0.0), WithinRel(std::pow(0.0, 0.0)));
  CHECK_THAT(power(2.5, 0.0), WithinRel(std::pow(2.5, 0.0)));
  CHECK_THAT(power(0.0, 5.0), WithinRel(std::pow(0.0, 5.0)));
  double exp = 1.23;
  double const base = 4.56;
  do {
    CHECK_THAT(power(base, exp), WithinRel(std::pow(base, exp)));
    exp += exp;
  } while (exp < 10);
  // Fall back to std::pow
  CHECK_THAT(power(2, 110), WithinRel(std::pow(2, 110)));
  CHECK_THAT(power(-2, 110), WithinRel(std::pow(-2, 110)));
  CHECK_THAT(power(2, -110), WithinRel(std::pow(2, -110)));
  CHECK_THAT(power(-2, -110), WithinRel(std::pow(-2, -110)));
  CHECK_THAT(power(2.0, 110), WithinRel(std::pow(2.0, 110)));
  CHECK_THAT(power(-2.0, 110), WithinRel(std::pow(-2.0, 110)));
  CHECK_THAT(power(2.0, -110), WithinRel(std::pow(2.0, -110)));
  CHECK_THAT(power(-2.0, -110), WithinRel(std::pow(-2.0, -110)));
  CHECK_THAT(power(-3, 3.0), WithinRel(std::pow(-3, 3.0)));
  CHECK_THAT(power(-3.0, 3.0), WithinRel(std::pow(-3.0, 3.0)));
  CHECK_THAT(power(-3, -3.0), WithinRel(std::pow(-3, -3.0)));
  CHECK_THAT(power(-3.0, -3.0), WithinRel(std::pow(-3.0, -3.0)));
}
