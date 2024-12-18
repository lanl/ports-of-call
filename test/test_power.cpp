#include "power.hh"

#include <cmath>
#include <gtest/gtest.h>

TEST(Util, Power)
{
  using util::power;
  using std::pow;
  // Integral exponents
  EXPECT_EQ(power( 0  , 0),   1     );
  EXPECT_EQ(power( 2.5, 0),   1.00  );
  EXPECT_EQ(power( 2.5, 1),   2.50  );
  EXPECT_EQ(power( 2.5, 2),   6.25  );
  EXPECT_EQ(power(-2.5, 2),   6.25  );
  EXPECT_EQ(power( 3  , 3),  27     );
  EXPECT_EQ(power(-3  , 3), -27     );
  EXPECT_EQ(power( 3.0, 3),  27.00  );
  EXPECT_EQ(power(-3.0, 3), -27.00  );
  EXPECT_EQ(power( 0  , 5),   0     );
  EXPECT_EQ(power( 0.0, 5),   0.00  );
  EXPECT_EQ(power( 1.0, 5),   1.00  );
  EXPECT_DOUBLE_EQ(power(1.1, 4), 1.4641);
  EXPECT_EQ(power( 0  , 0), pow(0  , 0));
  EXPECT_EQ(power( 2.5, 0), pow(2.5, 0));
  EXPECT_EQ(power( 0  , 5), pow(0  , 5));
  EXPECT_EQ(power( 0.0, 5), pow(0.0, 5));
  for (int n{0}; n < 10; ++n) {
    EXPECT_DOUBLE_EQ(power(3.14, n), pow(3.14, n));
  }
  // Floating-point exponents
  EXPECT_DOUBLE_EQ(power(0.0,  0.0),  1.00 );
  EXPECT_DOUBLE_EQ(power(2.5,  0.0),  1.00 );
  EXPECT_DOUBLE_EQ(power(2.5,  1.0),  2.50 );
  EXPECT_DOUBLE_EQ(power(2.5,  2.0),  6.25 );
  EXPECT_DOUBLE_EQ(power(2.5, -2.0),  0.16 );
  EXPECT_DOUBLE_EQ(power(3  ,  3.0), 27.00 );
  EXPECT_DOUBLE_EQ(power(2  , -3.0),  0.125);
  EXPECT_DOUBLE_EQ(power(0.0,  5.0),  0.00 );
  EXPECT_DOUBLE_EQ(power(0.0,  0.0), pow(0.0, 0.0));
  EXPECT_DOUBLE_EQ(power(2.5,  0.0), pow(2.5, 0.0));
  EXPECT_DOUBLE_EQ(power(0.0,  5.0), pow(0.0, 5.0));
  double       exp  = 1.23;
  double const base = 4.56;
  do {
    EXPECT_DOUBLE_EQ(power(base, exp), pow(base, exp));
    exp += exp;
  }
  while (exp < 10);
  // Fall back to std::pow
  EXPECT_EQ(power( 2  ,  110  ), pow( 2  ,  110  ));
  EXPECT_EQ(power(-2  ,  110  ), pow(-2  ,  110  ));
  EXPECT_EQ(power( 2  , -110  ), pow( 2  , -110  ));
  EXPECT_EQ(power(-2  , -110  ), pow(-2  , -110  ));
  EXPECT_EQ(power( 2.0,  110  ), pow( 2.0,  110  ));
  EXPECT_EQ(power(-2.0,  110  ), pow(-2.0,  110  ));
  EXPECT_EQ(power( 2.0, -110  ), pow( 2.0, -110  ));
  EXPECT_EQ(power(-2.0, -110  ), pow(-2.0, -110  ));
  EXPECT_EQ(power(-3  ,    3.0), pow(-3  ,    3.0));
  EXPECT_EQ(power(-3.0,    3.0), pow(-3.0,    3.0));
  EXPECT_EQ(power(-3  ,   -3.0), pow(-3  ,   -3.0));
  EXPECT_EQ(power(-3.0,   -3.0), pow(-3.0,   -3.0));
}
