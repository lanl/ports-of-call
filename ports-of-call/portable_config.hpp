// ========================================================================================
// Modifications © (or copyright) 2025. Triad National Security,
// LLC. All rights reserved.  This program was produced under
// U.S. Government contract 89233218CNA000001 for Los Alamos National
// Laboratory (LANL), which is operated by Triad National Security,
// LLC for the U.S.  Department of Energy/National Nuclear Security
// Administration. All rights in the program are reserved by Triad
// National Security, LLC, and the U.S. Department of Energy/National
// Nuclear Security Administration. The Government is granted for
// itself and others acting on its behalf a nonexclusive, paid-up,
// irrevocable worldwide license in this material to reproduce,
// prepare derivative works, distribute copies to the public, perform
// publicly and display publicly, and to permit others to do so.
// ========================================================================================

// portable compiler configuration checks via preprocessor macros.
// Largely taken from mpark::variant,
// Copyright Michael Park, 2015-2017
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#ifndef _PORTS_OF_CALL_PORTABLE_CONFIG_HPP_
#define _PORTS_OF_CALL_PORTABLE_CONFIG_HPP_

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
#define PORTABLE_FORCEINLINE __attribute__((__always_inline__)) inline
#elif defined(_MSC_VER)
#define PORTABLE_FORCEINLINE __forceinline
#else
#define PORTABLE_FORCEINLINE inline
#endif

#if __has_builtin(__builtin_addressof) || \
    (defined(__GNUC__) && __GNUC__ >= 7) || defined(_MSC_VER)
#define PORTABLE_HAS_BUILTIN_ADDRESSOF
#endif

#if __has_builtin(__builtin_unreachable) || defined(__GNUC__)
#define PORTABLE_HAS_BUILTIN_UNREACHABLE __builtin_unreachable()
#elif defined(_MSC_VER)
#define PORTABLE_HAS_BUILTIN_UNREACHABLE __assume(false)
#else
#define PORTABLE_HAS_BUILTIN_UNREACHABLE
#endif

#if __has_builtin(__type_pack_element) && !(defined(__ICC))
#define PORTABLE_HAS_TYPE_PACK_ELEMENT
#endif

#if __has_feature(cxx_exceptions) || defined(__cpp_exceptions) || \
    (defined(_MSC_VER) && defined(_CPPUNWIND)) || \
    defined(__EXCEPTIONS)
#define PORTABLE_HAS_EXCEPTIONS
#endif

#endif  // _PORTS_OF_CALL_PORTABLE_CONFIG_HPP_
