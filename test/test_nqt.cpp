//------------------------------------------------------------------------------
// © 2026. Triad National Security, LLC. All rights reserved.  This
// program was produced under U.S. Government contract 89233218CNA000001
// for Los Alamos National Laboratory (LANL), which is operated by Triad
// National Security, LLC for the U.S.  Department of Energy/National
// Nuclear Security Administration. All rights in the program are
// reserved by Triad National Security, LLC, and the U.S. Department of
// Energy/National Nuclear Security Administration. The Government is
// granted for itself and others acting on its behalf a nonexclusive,
// paid-up, irrevocable worldwide license in this material to reproduce,
// prepare derivative works, distribute copies to the public, perform
// publicly and display publicly, and to permit others to do so.
//------------------------------------------------------------------------------

#include <ports-of-call/nqt_math.hpp>
#include <ports-of-call/portability.hpp>

#ifndef CATCH_CONFIG_FAST_COMPILE
#define CATCH_CONFIG_FAST_COMPILE
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#endif

#include <cmath>
#include <limits>

namespace {

template <typename Log, typename Pow2>
void check_o1_values(Log log, Pow2 pow2) {
  using Catch::Matchers::WithinAbs;
  constexpr double tolerance = 4.0 * std::numeric_limits<double>::epsilon();

  CHECK(log(0.5) == -1.0);
  CHECK(log(1.0) == 0.0);
  CHECK(log(2.0) == 1.0);
  CHECK_THAT(log(0.75), WithinAbs(-0.5, tolerance));
  CHECK_THAT(log(1.5), WithinAbs(0.5, tolerance));

  CHECK(pow2(-1.0) == 0.5);
  CHECK(pow2(0.0) == 1.0);
  CHECK(pow2(1.0) == 2.0);
  CHECK_THAT(pow2(-0.5), WithinAbs(0.75, tolerance));
  CHECK_THAT(pow2(0.5), WithinAbs(1.5, tolerance));
}

template <typename Log, typename Pow2>
void check_o2_values(Log log, Pow2 pow2) {
  using Catch::Matchers::WithinAbs;
  constexpr double tolerance = 4.0 * std::numeric_limits<double>::epsilon();

  CHECK(log(0.5) == -1.0);
  CHECK(log(1.0) == 0.0);
  CHECK(log(2.0) == 1.0);
  CHECK_THAT(log(0.75), WithinAbs(-5.0 / 12.0, tolerance));
  CHECK_THAT(log(1.5), WithinAbs(7.0 / 12.0, tolerance));

  CHECK(pow2(-1.0) == 0.5);
  CHECK(pow2(0.0) == 1.0);
  CHECK(pow2(1.0) == 2.0);
  CHECK_THAT(pow2(-0.5), WithinAbs(0.5 * (3.0 - std::sqrt(2.5)), tolerance));
  CHECK_THAT(pow2(0.5), WithinAbs(3.0 - std::sqrt(2.5), tolerance));
}

template <typename Asinh, typename Sinh>
void check_o1_hyperbolic_values(Asinh asinh, Sinh sinh) {
  using Catch::Matchers::WithinAbs;
  constexpr double e_half = 1.3591409142295226177;
  constexpr double inv_e = 0.3678794411714423216;
  constexpr double tolerance = 16.0 * std::numeric_limits<double>::epsilon();

  CHECK(asinh(0.0) == 0.0);
  CHECK(sinh(0.0) == 0.0);
  CHECK_THAT(asinh(0.5), WithinAbs(inv_e, tolerance));
  CHECK_THAT(asinh(-0.5), WithinAbs(-inv_e, tolerance));
  CHECK_THAT(sinh(0.5), WithinAbs(0.5 * e_half, tolerance));
  CHECK_THAT(sinh(-0.5), WithinAbs(-0.5 * e_half, tolerance));

  constexpr double values[] = {-4.0, -1.0, -0.5, 0.5, 1.0, 4.0};
  for (const double x : values) {
    CHECK_THAT(asinh(sinh(x)), WithinAbs(x, tolerance));
  }
}

} // namespace

SCENARIO("Test that the NQT math magic numbers are all correct", "[NQT]") {
  using namespace PortsOfCall::FP64LE;

  REQUIRE(one_as_int == 4607182418800017408);
  REQUIRE(scale_down == 0x1p-52);
  REQUIRE(scale_up == 4503599627370496.0);
  REQUIRE(mantissa_mask == 4503599627370495);
  REQUIRE(low_mask == 67108863);
  REQUIRE(as_int(1.0) == one_as_int);
  REQUIRE(as_double(one_as_int) == 1.0);
}

TEST_CASE("NQT implementations reproduce analytic values", "[NQT]") {
  SECTION("portable first order") {
    check_o1_values(PortsOfCall::NQT::O1::Portable::lg,
                    PortsOfCall::NQT::O1::Portable::pow2);
  }
  SECTION("aliased first order") {
    check_o1_values(PortsOfCall::NQT::O1::Aliased::lg,
                    PortsOfCall::NQT::O1::Aliased::pow2);
  }
  SECTION("portable second order") {
    check_o2_values(PortsOfCall::NQT::O2::Portable::lg,
                    PortsOfCall::NQT::O2::Portable::pow2);
  }
  SECTION("aliased second order") {
    check_o2_values(PortsOfCall::NQT::O2::Aliased::lg,
                    PortsOfCall::NQT::O2::Aliased::pow2);
  }
}

TEST_CASE("First-order NQT hyperbolic functions are odd and invertible", "[NQT]") {
  SECTION("portable") {
    check_o1_hyperbolic_values(PortsOfCall::NQT::O1::Portable::asinh,
                               PortsOfCall::NQT::O1::Portable::sinh);
  }
  SECTION("aliased") {
    check_o1_hyperbolic_values(PortsOfCall::NQT::O1::Aliased::asinh,
                               PortsOfCall::NQT::O1::Aliased::sinh);
  }
}

SCENARIO("Test that all NQT implementations are invertible and run on device",
         "[NQT][GPU]") {
  GIVEN("normal values spanning a large dynamic range") {
    constexpr double min_log2 = -1022.0;
    constexpr double max_log2 = 1023.0;
    constexpr int num_points = 2048;
    constexpr double spacing = (max_log2 - min_log2) / (num_points - 1);

    THEN("each matching log and power implementation round-trips the values") {
      int failures = 0;
      portableReduce(
          "NQT round trips", 0, num_points,
          PORTABLE_LAMBDA(const int i, int &local_failures) {
            constexpr double tolerance = 1024.0 * std::numeric_limits<double>::epsilon();
            const double expected_log2 = min_log2 + i * spacing;
            const double x = std::pow(2.0, expected_log2);
            const double round_trips[] = {
                PortsOfCall::NQT::O1::Portable::pow2(PortsOfCall::NQT::O1::Portable::lg(x)),
                PortsOfCall::NQT::O1::Aliased::pow2(PortsOfCall::NQT::O1::Aliased::lg(x)),
                PortsOfCall::NQT::O2::Portable::pow2(PortsOfCall::NQT::O2::Portable::lg(x)),
                PortsOfCall::NQT::O2::Aliased::pow2(PortsOfCall::NQT::O2::Aliased::lg(x))};

            constexpr double min_hyperbolic_arg = -4.0;
            constexpr double max_hyperbolic_arg = 4.0;
            const double hyperbolic_arg =
                min_hyperbolic_arg + i * (max_hyperbolic_arg - min_hyperbolic_arg) /
                                         (num_points - 1);
            const double hyperbolic_round_trips[] = {
                PortsOfCall::NQT::O1::Portable::asinh(
                    PortsOfCall::NQT::O1::Portable::sinh(hyperbolic_arg)),
                PortsOfCall::NQT::O1::Aliased::asinh(
                    PortsOfCall::NQT::O1::Aliased::sinh(hyperbolic_arg))};

            for (const double round_trip : round_trips) {
              const double relative_error =
                  2.0 * std::abs(x - round_trip) / (std::abs(x) + std::abs(round_trip));
              local_failures += !std::isfinite(round_trip) || relative_error > tolerance;
            }
            for (const double round_trip : hyperbolic_round_trips) {
              const double relative_error =
                  2.0 * std::abs(hyperbolic_arg - round_trip) /
                  (std::abs(hyperbolic_arg) + std::abs(round_trip));
              local_failures += !std::isfinite(round_trip) || relative_error > tolerance;
            }
          },
          failures);

      REQUIRE(failures == 0);
    }
  }
}
