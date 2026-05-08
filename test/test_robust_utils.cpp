#include <ports-of-call/robust_utils.hpp>

#ifndef CATCH_CONFIG_FAST_COMPILE
#define CATCH_CONFIG_FAST_COMPILE
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#endif

#include <cmath>
#include <limits>

// This file made with the assistance of generative AI

namespace {

template <typename T>
PORTABLE_FORCEINLINE_FUNCTION bool within_rel(const T lhs, const T rhs,
                                              const T rel_tol = T{1.0e-12}) {
  const T lhs_abs = lhs < T{0} ? -lhs : lhs;
  const T rhs_abs = rhs < T{0} ? -rhs : rhs;
  const T scale = lhs_abs > rhs_abs ? lhs_abs : rhs_abs;
  const T diff = lhs > rhs ? lhs - rhs : rhs - lhs;
  return diff <= rel_tol * (scale > T{1} ? scale : T{1});
}

} // namespace

TEST_CASE("robust_utils predicates", "[robust_utils]") {
  namespace Robust = PortsOfCall::Robust;

  CHECK(Robust::is_normal(7));
  CHECK(Robust::is_normal(1.0));
  CHECK_FALSE(Robust::is_normal(0.0));
  CHECK_FALSE(Robust::is_normal(std::numeric_limits<double>::min() / 2.0));
  CHECK_FALSE(Robust::is_normal(std::numeric_limits<double>::max(), 2.0));

  CHECK(Robust::is_normal_or_zero(0.0));
  CHECK(Robust::is_normal_or_zero(1.0));
  CHECK_FALSE(Robust::is_normal_or_zero(std::numeric_limits<double>::min() / 2.0));

  CHECK_FALSE(Robust::check_nonnegative(-1));
  CHECK(Robust::check_nonnegative(0));
  CHECK(Robust::check_nonnegative(5));
  CHECK(Robust::check_nonnegative(5u));
}

TEST_CASE("robust_utils numeric helpers", "[robust_utils]") {
  using Catch::Matchers::WithinRel;
  namespace Robust = PortsOfCall::Robust;

  CHECK(Robust::SMALL<double>() == 10 * std::numeric_limits<double>::min());
  CHECK(Robust::EPS<double>() == 10 * std::numeric_limits<double>::epsilon());
  CHECK_THAT(Robust::min_exp_arg<double>(),
             WithinRel((std::numeric_limits<double>::min_exponent - 1) * std::log(2.0)));
  CHECK_THAT(Robust::max_exp_arg<double>(),
             WithinRel(std::numeric_limits<double>::max_exponent * std::log(2.0)));

  CHECK_THAT(Robust::make_positive(-1.0), WithinRel(Robust::EPS<double>()));
  CHECK_THAT(Robust::make_positive(2.5), WithinRel(2.5));

  CHECK_THAT(Robust::make_bounded(-1.0, 0.0, 2.0), WithinRel(Robust::EPS<double>()));
  CHECK_THAT(Robust::make_bounded(1.5, 0.0, 2.0), WithinRel(1.5));
  CHECK_THAT(Robust::make_bounded(3.0, 0.0, 2.0),
             WithinRel(2.0 * (1.0 - Robust::EPS<double>())));

  CHECK(Robust::sgn(-3) == -1);
  CHECK(Robust::sgn(0) == 1);
  CHECK(Robust::sgn(3) == 1);

  CHECK_THAT(Robust::ratio(6.0, 3.0), WithinRel(2.0));
  CHECK(Robust::ratio(0.0, 0.0) == 0.0);
  CHECK(Robust::ratio(1.0, 0.0) > 1.0e300);

  CHECK(Robust::safe_arg_exp(Robust::min_exp_arg<double>() - 1.0) == 0.0);
  CHECK(Robust::safe_arg_exp(Robust::max_exp_arg<double>() + 1.0) ==
        std::numeric_limits<double>::infinity());
  CHECK_THAT(Robust::safe_arg_exp(1.0), WithinRel(std::exp(1.0)));
}

TEST_CASE("robust_utils (GPU)", "[robust_utils][GPU]") {
  int failures = 0;
  portableReduce(
      "robust utils device", 0, 1,
      PORTABLE_LAMBDA(const int /*i*/, int &local_failures) {
        local_failures += !PortsOfCall::Robust::is_normal(7);
        local_failures += !PortsOfCall::Robust::is_normal(1.0);
        local_failures += PortsOfCall::Robust::is_normal(0.0);
        local_failures +=
            PortsOfCall::Robust::is_normal(std::numeric_limits<double>::min() / 2.0);
        local_failures +=
            PortsOfCall::Robust::is_normal(std::numeric_limits<double>::max(), 2.0);
        local_failures += !PortsOfCall::Robust::is_normal_or_zero(0.0);
        local_failures += !PortsOfCall::Robust::check_nonnegative(5u);
        local_failures += PortsOfCall::Robust::check_nonnegative(-1);
        local_failures += (PortsOfCall::Robust::SMALL<double>() !=
                           10 * std::numeric_limits<double>::min());
        local_failures += (PortsOfCall::Robust::EPS<double>() !=
                           10 * std::numeric_limits<double>::epsilon());
        local_failures +=
            !within_rel(PortsOfCall::Robust::min_exp_arg<double>(),
                        (std::numeric_limits<double>::min_exponent - 1) * M_LN2);
        local_failures += !within_rel(PortsOfCall::Robust::max_exp_arg<double>(),
                                      std::numeric_limits<double>::max_exponent * M_LN2);
        local_failures += !within_rel(PortsOfCall::Robust::make_positive(-1.0),
                                      PortsOfCall::Robust::EPS<double>());
        local_failures += !within_rel(PortsOfCall::Robust::make_positive(2.5), 2.5);
        local_failures += !within_rel(PortsOfCall::Robust::make_bounded(-1.0, 0.0, 2.0),
                                      PortsOfCall::Robust::EPS<double>());
        local_failures +=
            !within_rel(PortsOfCall::Robust::make_bounded(1.5, 0.0, 2.0), 1.5);
        local_failures += !within_rel(PortsOfCall::Robust::make_bounded(3.0, 0.0, 2.0),
                                      2.0 * (1.0 - PortsOfCall::Robust::EPS<double>()));
        local_failures += (PortsOfCall::Robust::sgn(-3) != -1);
        local_failures += (PortsOfCall::Robust::sgn(0) != 1);
        local_failures += (PortsOfCall::Robust::sgn(3) != 1);
        local_failures += !within_rel(PortsOfCall::Robust::ratio(6.0, 3.0), 2.0);
        local_failures += (PortsOfCall::Robust::ratio(0.0, 0.0) != 0.0);
        local_failures += !(PortsOfCall::Robust::ratio(1.0, 0.0) > 1.0e300);
        local_failures += (PortsOfCall::Robust::safe_arg_exp(
                               PortsOfCall::Robust::min_exp_arg<double>() - 1.0) != 0.0);
        local_failures += (PortsOfCall::Robust::safe_arg_exp(
                               PortsOfCall::Robust::max_exp_arg<double>() + 1.0) !=
                           std::numeric_limits<double>::infinity());
        local_failures +=
            !within_rel(PortsOfCall::Robust::safe_arg_exp(1.0), std::exp(1.0));
      },
      failures);

  REQUIRE(failures == 0);
}
