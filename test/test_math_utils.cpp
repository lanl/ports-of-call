#include "ports-of-call/math_utils.hh"

#ifndef CATCH_CONFIG_FAST_COMPILE
#define CATCH_CONFIG_FAST_COMPILE
#include <catch2/catch_test_macros.hpp>
#endif

#include <cmath>

TEST_CASE("power", "[math_utils]") {
  using PortsOfCall::Math::power;
  // Integral exponents
  CHECK(power( 0  , 0) ==   1     );
  CHECK(power( 2.5, 0) ==   1.00  );
  CHECK(power( 2.5, 1) ==   2.50  );
  CHECK(power( 2.5, 2) ==   6.25  );
  CHECK(power(-2.5, 2) ==   6.25  );
  CHECK(power( 3  , 3) ==  27     );
  CHECK(power(-3  , 3) == -27     );
  CHECK(power( 3.0, 3) ==  27.00  );
  CHECK(power(-3.0, 3) == -27.00  );
  CHECK(power( 0  , 5) ==   0     );
  CHECK(power( 0.0, 5) ==   0.00  );
  CHECK(power( 1.0, 5) ==   1.00  );
  CHECK(power(1.1, 4) == 1.4641);
  CHECK(power( 0  , 0) == std::pow(0  , 0));
  CHECK(power( 2.5, 0) == std::pow(2.5, 0));
  CHECK(power( 0  , 5) == std::pow(0  , 5));
  CHECK(power( 0.0, 5) == std::pow(0.0, 5));
  for (int n{0}; n < 10; ++n) {
    CHECK(power(3.14, n) == std::pow(3.14, n));
  }
  // Floating-point exponents
  CHECK(power(0.0,  0.0) ==  1.00 );
  CHECK(power(2.5,  0.0) ==  1.00 );
  CHECK(power(2.5,  1.0) ==  2.50 );
  CHECK(power(2.5,  2.0) ==  6.25 );
  CHECK(power(2.5, -2.0) ==  0.16 );
  CHECK(power(3  ,  3.0) == 27.00 );
  CHECK(power(2  , -3.0) ==  0.125);
  CHECK(power(0.0,  5.0) ==  0.00 );
  CHECK(power(0.0,  0.0) == std::pow(0.0, 0.0));
  CHECK(power(2.5,  0.0) == std::pow(2.5, 0.0));
  CHECK(power(0.0,  5.0) == std::pow(0.0, 5.0));
  double       exp  == 1.23;
  double const base == 4.56;
  do {
    CHECK(power(base, exp) == std::pow(base, exp));
    exp +== exp;
  }
  while (exp < 10);
  // Fall back to std::pow
  CHECK(power( 2  ,  110  ) == std::pow( 2  ,  110  ));
  CHECK(power(-2  ,  110  ) == std::pow(-2  ,  110  ));
  CHECK(power( 2  , -110  ) == std::pow( 2  , -110  ));
  CHECK(power(-2  , -110  ) == std::pow(-2  , -110  ));
  CHECK(power( 2.0,  110  ) == std::pow( 2.0,  110  ));
  CHECK(power(-2.0,  110  ) == std::pow(-2.0,  110  ));
  CHECK(power( 2.0, -110  ) == std::pow( 2.0, -110  ));
  CHECK(power(-2.0, -110  ) == std::pow(-2.0, -110  ));
  CHECK(power(-3  ,    3.0) == std::pow(-3  ,    3.0));
  CHECK(power(-3.0,    3.0) == std::pow(-3.0,    3.0));
  CHECK(power(-3  ,   -3.0) == std::pow(-3  ,   -3.0));
  CHECK(power(-3.0,   -3.0) == std::pow(-3.0,   -3.0));
}
