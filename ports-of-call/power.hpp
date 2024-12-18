#ifndef UTIL_POWER_HH
#define UTIL_POWER_HH

#include <cmath>
#include <ports-of-call/portability.hpp>
#include <type_traits>

// C++20 requires-like macro that enables SFINAE without typing std::enable_if each time
#define REQUIRES(...) typename std::enable_if_t<__VA_ARGS__>* = nullptr

namespace util {

namespace detail {

/* Function: power(base, exponent) */
struct power_fn {

  /* Faster implementation of std::pow for arithmetic bases and non-negative integer exponents.
     For sufficiently large integer powers std::pow may be faster, but testing indicates that the
     following implementation is significantly faster (roughly a factor of two or better) up to
     powers of at least 100. */
  template<
    typename BaseT
  , typename ExponentT
  , REQUIRES(std::is_arithmetic_v<std::decay_t<BaseT>>
          && std::is_integral_v<std::decay_t<ExponentT>>)
  >
  PORTABLE_INLINE_FUNCTION constexpr auto operator()(
    BaseT base
  , ExponentT exponent
  ) const
  {
    using std::pow;
    using PowT = decltype(pow(base, exponent));
    if (exponent < ExponentT{0} or exponent > ExponentT{100}) return pow(base, exponent);
    PowT result = PowT{1};
    for (;;) {
      if (exponent & 1) result *= base; // Multiply either if the exponent starts odd or we have bit shifted it to 1
      exponent >>= 1; // Shift the exponent to the next position (i.e. divide it by 2)
      if (!exponent) break; // If the exponent has been bit shifted to zero, we are done
      base *= base; // We halved the exponent (see the shift operation), so square the base
    }
    return result;
  }

  /* Faster implementation of std::pow() for non-negative arithmetic bases and floating-point
     exponents */
  template<
    typename BaseT
  , typename ExponentT
  , REQUIRES(std::is_arithmetic_v<std::decay_t<BaseT>>
          && std::is_floating_point_v<std::decay_t<ExponentT>>)
  >
  PORTABLE_INLINE_FUNCTION constexpr auto operator()(
    BaseT const& base
  , ExponentT const& exponent
  ) const
  {
    using std::pow;
    using std::exp;
    using std::log;
    if (base < BaseT{0}) return pow(base, exponent);
    return exponent == ExponentT{0} ? BaseT{1} // Enforcing base^0=1 (including 0^0=1)
         : base == BaseT{0} ? BaseT{0}
         : exp(exponent*log(base));
  }

  /* Overload for non-arithmetic bases or exponents */
  template<
    typename BaseT
  , typename ExponentT
  , REQUIRES(not std::is_arithmetic_v<std::decay_t<BaseT>>
          || not std::is_arithmetic_v<std::decay_t<ExponentT>>)
  >
  PORTABLE_INLINE_FUNCTION constexpr auto operator()(
    BaseT const& base
  , ExponentT const& exponent
  ) const
  {
    using std::pow;
    return pow(base, exponent);
  }

}; // struct power_fn

} // namespace detail

constexpr static auto power = detail::power_fn{};

} // namespace util

#endif // #ifndef UTIL_POWER_HH
