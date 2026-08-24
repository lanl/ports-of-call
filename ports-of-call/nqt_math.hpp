//------------------------------------------------------------------------------
// © 2021-2026. Triad National Security, LLC. All rights reserved.  This
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

#ifndef PORTS_OF_CALL_NQT_MATH_HPP_
#define PORTS_OF_CALL_NQT_MATH_HPP_

#include <bit>
#include <cstdint>
#include <limits>

#include <ports-of-call/math_utils.hpp>
#include <ports-of-call/portability.hpp>
#include <ports-of-call/portable_errors.hpp>

/*
 * Implmentations of the not-quite-transcendental logarithm and
 * exponent methods described in the series of papers:
 *
 * Not-Quite Transcendental Functions and their Applications,
 * Miller, Dolence, holladay, 2022, ArXiv:2206.08957
 *
 * Not-Quite Transcendental Functions for Logarithmic Interpolation of
 * Tabulated Data
 * Hammond, Fields, Miller, and Barker, 2025, ApJS 277 65
 *
 * The core idea here is that for many applications that require
 * logarithms, an logarithm is not required. Rather, an invertible
 * function with approximately logarithmic spacing is the desirable
 * feature. The above papers propose to take advantage of the
 * structure of floating point numbers to construct functions that are
 * very fast to evaluate that meet these constraints.
 *
 * We split a real number into its mantissa
 * + exponent in base 2. The mantissa is a real number on the interval
 * [0.5, 1) and the exponent is an integer such that
 *
 * mantissa * 2^exponent = number
 *
 * The log in base 2 is then
 *
 * lg(number) = lg(mantissa) + exponent
 *
 * Our goal is to approximate lg(mantissa) satisfying the above
 * criteria. If we do, then we will achieve the desired goals. We have
 * two approaches. The first is a linear approximation of
 * lg(mantissa). The second is a quadratic one. The latter allows for
 * continuity of the derivative.
 */

// TODO(JMM): All the math here is all assuming inputs are
// doubles. Should add single-precision overloads.

namespace PortsOfCall {

// Magic numbers constexpr because C++ doesn't constexpr reinterpret casts
// These numbers will be different for different precisions and endianness.
namespace FP64LE { // 64 bit, little endian
// TODO(JMM): Add templating and concepts to enforce bit width
// equivalence in input/output types.
PORTABLE_FORCEINLINE_FUNCTION
constexpr auto as_int(double f) { return std::bit_cast<std::int64_t>(f); }
PORTABLE_FORCEINLINE_FUNCTION
constexpr auto as_double(std::int64_t i) { return std::bit_cast<double>(i); }

constexpr std::int64_t one = 1;
// as_int(1.0) == 2^62 - 2^52
constexpr std::int64_t one_as_int = (one << 62) - (one << 52);
// 1./static_cast<double>(as_int(2.0) - as_int(1.0)) == 2^-52
constexpr double scale_down = 0x1p-52;
// as_int(2.0) - as_int(1.0) = 2^52, but note the type
constexpr double scale_up = (one << 52);
// 2^52 - 1
constexpr std::int64_t mantissa_mask = (one << 52) - one;
// 2^26 - 1
constexpr std::int64_t low_mask = (one << 26) - 1;
} // namespace FP64LE

namespace NQT {
namespace O1 {
namespace Portable {
// First order interpolation based NQTs
// ----------------------------------------------------------------------
// Reference implementations, however the integer cast implementation
// below is probably faster.
PORTABLE_FORCEINLINE_FUNCTION
double lg(const double x) {
  int e;
  PORTABLE_REQUIRE(x > 0, "log divergent for x <= 0");
  PORTABLE_REQUIRE(std::isfinite(x), "log divergent for non-finite x");
  const double m = frexp(x, &e);
  return 2 * (m - 1) + e;
}

PORTABLE_FORCEINLINE_FUNCTION
double pow2(const double x) {
  PORTABLE_REQUIRE(std::isfinite(x) && x >= -1022 && x <= 1024,
                   "x must be finite and sufficiently small");
  const int flr = std::floor(x);
  const double remainder = x - flr;
  const double mantissa = 0.5 * (remainder + 1);
  const double exponent = flr + 1;
  return ldexp(mantissa, exponent);
}

} // namespace Portable

namespace Aliased {
// Integer aliased versions
PORTABLE_FORCEINLINE_FUNCTION
double lg(const double x) {
  using namespace FP64LE;
  PORTABLE_REQUIRE(x >= std::numeric_limits<double>::min() && std::isfinite(x),
                   "Aliased log unsafe for negatives and subnormals");
  return static_cast<double>(as_int(x) - one_as_int) * scale_down;
}

PORTABLE_FORCEINLINE_FUNCTION
double pow2(const double x) {
  using namespace FP64LE;
  PORTABLE_REQUIRE(std::isfinite(x) && x >= -1022 && x <= 1024,
                   "x must be finite and sufficiently small");
  return as_double(static_cast<std::int64_t>(x * scale_up) + one_as_int);
}
} // namespace Aliased
} // namespace O1
// ----------------------------------------------------------------------

namespace O2 {
namespace Portable {
// Second-order interpolation based NQTs
// These implementations are due to Peter Hammond
// ----------------------------------------------------------------------
// Portable versions that use frexp/ldexp rather than integer aliasing
PORTABLE_FORCEINLINE_FUNCTION
double lg(const double x) {
  PORTABLE_REQUIRE(x > 0, "log divergent for x <= 0");
  PORTABLE_REQUIRE(std::isfinite(x), "log divergent for non-finite x");
  constexpr double four_thirds = 4. / 3.;
  int e;
  const double m = frexp(x, &e);
  return e - four_thirds * (m - 2) * (m - 1);
}

// This version uses the exact formula
PORTABLE_FORCEINLINE_FUNCTION
double pow2(const double x) {
  PORTABLE_REQUIRE(std::isfinite(x) && x >= -1022 && x <= 1024,
                   "x must be finite and sufficiently small");
  // log2(mantissa). should go between -1 and 0
  const int flr = std::floor(x);
  const double lm = x - flr - 1;
  const double mantissa = 0.5 * (3 - std::sqrt(1 - 3 * lm));
  const double exponent = flr + 1;
  return ldexp(mantissa, exponent);
}

} // namespace Portable

namespace Aliased {
// Integer aliased/bithacked versions
PORTABLE_FORCEINLINE_FUNCTION
double lg(const double x) {
  using namespace FP64LE;
  PORTABLE_REQUIRE(x >= std::numeric_limits<double>::min() && std::isfinite(x),
                   "Aliased log unsafe for negatives and subnormals");
  const std::int64_t x_as_int = as_int(x) - one_as_int;
  const std::int64_t frac_as_int = x_as_int & mantissa_mask;
  const std::int64_t frac_high = frac_as_int >> 26;
  const std::int64_t frac_low = frac_as_int & low_mask;
  const std::int64_t frac_squared =
      frac_high * frac_high + ((frac_high * frac_low) >> 25);

  return static_cast<double>(x_as_int + ((frac_as_int - frac_squared) / 3)) * scale_down;
}

PORTABLE_FORCEINLINE_FUNCTION
double pow2(const double x) {
  using namespace FP64LE;
  PORTABLE_REQUIRE(std::isfinite(x) && x >= -1022 && x <= 1024,
                   "x must be finite and sufficiently small");
  constexpr std::int64_t a = 9007199254740992;  // 2 * 2^52
  constexpr double b = 67108864;                // 2^26
  constexpr std::int64_t c = 18014398509481984; // 4 * 2^52
  const std::int64_t x_as_int = static_cast<std::int64_t>(x * scale_up);
  const std::int64_t frac_as_int = x_as_int & mantissa_mask;
  const std::int64_t frac_sqrt =
      static_cast<std::int64_t>(b * std::sqrt(static_cast<double>(c - 3 * frac_as_int)));

  return as_double(x_as_int + a - frac_sqrt - frac_as_int + one_as_int);
}
} // namespace Aliased
} // namespace O2

namespace impl {
template <auto Lg>
PORTABLE_FORCEINLINE_FUNCTION double asinh(const double x) {
  constexpr double e_half = 1.3591409142295226177;
  constexpr double inv_e = 0.3678794411714423216;
  constexpr double ln2 = 0.6931471805599453094;

  const double ax = std::abs(x);
  const double large = static_cast<double>(ax >= e_half);
  // Both sides of the mask are evaluated, so give Lg a valid argument
  // for small inputs as well.
  const double safe_ax = large * ax + (1.0 - large) * e_half;
  const double small_result = 2.0 * x * inv_e;
  const double large_result = Math::sgn(x) * ln2 * (Lg(safe_ax) + 1.0);

  return (1.0 - large) * small_result + large * large_result;
}

template <auto Pow2>
PORTABLE_FORCEINLINE_FUNCTION double sinh(const double x) {
  constexpr double e_half = 1.3591409142295226177;
  constexpr double log2e = 1.4426950408889634074;

  const double ax = std::abs(x);
  const double large = static_cast<double>(ax >= 1.0);
  const double small_result = e_half * x;
  const double large_result = 0.5 * Math::sgn(x) * Pow2(log2e * ax);

  return (1.0 - large) * small_result + large * large_result;
}
} // namespace impl

namespace O1 {
namespace Aliased {
PORTABLE_FORCEINLINE_FUNCTION
double asinh(const double x) {
  return impl::asinh<lg>(x);
}

PORTABLE_FORCEINLINE_FUNCTION
double sinh(const double x) {
  return impl::sinh<pow2>(x);
}
} // namespace Aliased

namespace Portable {
PORTABLE_FORCEINLINE_FUNCTION
double asinh(const double x) {
  return impl::asinh<lg>(x);
}

PORTABLE_FORCEINLINE_FUNCTION
double sinh(const double x) {
  return impl::sinh<pow2>(x);
}
} // namespace Portable
} // namespace O1
// ----------------------------------------------------------------------

} // namespace NQT

} // namespace PortsOfCall

#endif // PORTS_OF_CALL_NQT_MATH_HPP_
