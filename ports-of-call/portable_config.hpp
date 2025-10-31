// portable_config
// A set of macros to detect C++ features
//
// Copyright Michael Park, 2015-2017
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

// GPU compatibility corrections are © 2025. Triad National Security,
// LLC under U.S. Government contract 89233218CNA000001 for Los Alamos
// National Laboratory (LANL), which is operated by Triad National
// Security, LLC for the U.S. Department of Energy/National Nuclear
// Security Administration.

// TODO(JMM): I'm not sure how many of these features are ever not
// available now that we're in 2025. I left this mostly complete with
// the names changed for safety, but it's possible a lot of this could
// be cleaned up/removed.

#ifndef _PORTS_OF_CALL_PORTABLE_CONFIG_HPP_
#define _PORTS_OF_CALL_PORTABLE_CONFIG_HPP_

// MSVC 2015 Update 3.
#if __cplusplus < 201103L && (!defined(_MSC_VER) || _MSC_FULL_VER < 190024210)
#error "Ports of Call requires C++11 support."
#endif

#ifndef __has_attribute
#define __has_attribute(x) 0
#endif

#ifndef __has_builtin
#define __has_builtin(x) 0
#endif

#ifndef __has_include
#define __has_include(x) 0
#endif

#ifndef __has_feature
#define __has_feature(x) 0
#endif

#if __has_attribute(always_inline) || defined(__GNUC__)
#define POC_ALWAYS_INLINE __attribute__((__always_inline__)) inline
#elif defined(_MSC_VER)
#define POC_ALWAYS_INLINE __forceinline
#else
#define POC_ALWAYS_INLINE inline
#endif

#if __has_builtin(__builtin_addressof) || (defined(__GNUC__) && __GNUC__ >= 7) ||        \
    defined(_MSC_VER)
#define PORTABLE_HAS_BUILTIN_ADDRESSOF
#endif

#if __has_builtin(__builtin_unreachable) || defined(__GNUC__)
#define PORTABLE_BUILTIN_UNREACHABLE __builtin_unreachable()
#elif defined(_MSC_VER)
#define PORTABLE_BUILTIN_UNREACHABLE __assume(false)
#else
#define PORTABLE_BUILTIN_UNREACHABLE
#endif

#if __has_builtin(__type_pack_element) && !(defined(__ICC))
#define PORTABLE_HAS_TYPE_PACK_ELEMENT
#endif

#if defined(__cpp_constexpr) && __cpp_constexpr >= 201304
#define PORTABLE_HAS_CPP14_CONSTEXPR
#endif

#if __has_feature(cxx_exceptions) || defined(__cpp_exceptions) ||                        \
    (defined(_MSC_VER) && defined(_CPPUNWIND)) || defined(__EXCEPTIONS)
#define PORTABLE_HAS_EXCEPTIONS
#endif

#if defined(__cpp_generic_lambdas) || defined(_MSC_VER)
#define PORTABLE_HAS_GENERIC_LAMBDAS
#endif

#if defined(__cpp_lib_integer_sequence)
#define PORTABLE_HAS_INTEGER_SEQUENCE
#endif

#if (defined(__cpp_decltype_auto) && defined(__cpp_return_type_deduction)) ||            \
    defined(_MSC_VER)
#define PORTABLE_HAS_RETURN_TYPE_DEDUCTION
#endif

#if defined(__cpp_lib_transparent_operators) || defined(_MSC_VER)
#define PORTABLE_HAS_TRANSPARENT_OPERATORS
#endif

#if defined(__cpp_variable_templates) || defined(_MSC_VER)
#define PORTABLE_HAS_VARIABLE_TEMPLATES
#endif

#if !defined(__GLIBCXX__) || __has_include(<codecvt>) // >= libstdc++-5
#define PORTABLE_HAS_TRIVIALITY_TYPE_TRAITS
#define PORTABLE_HAS_INCOMPLETE_TYPE_TRAITS
#endif

#endif // _PORTS_OF_CALL_PORTABLE_CONFIG_HPP_
