#ifndef _PORTS_OF_CALL_PORTABLE_ERRORS_HPP_
#define _PORTS_OF_CALL_PORTABLE_ERRORS_HPP_

// ========================================================================================
// © (or copyright) 2023-2024. Triad National Security, LLC. All rights
// reserved.  This program was produced under U.S. Government contract
// 89233218CNA000001 for Los Alamos National Laboratory (LANL), which is
// operated by Triad National Security, LLC for the U.S.  Department of
// Energy/National Nuclear Security Administration. All rights in the
// program are reserved by Triad National Security, LLC, and the
// U.S. Department of Energy/National Nuclear Security
// Administration. The Government is granted for itself and others acting
// on its behalf a nonexclusive, paid-up, irrevocable worldwide license
// in this material to reproduce, prepare derivative works, distribute
// copies to the public, perform publicly and display publicly, and to
// permit others to do so.
// ========================================================================================

// C++ headers
#include <iostream>
#include <sstream>
#include <stdexcept>

// C headers
#include <cstdio>
#include <cstdlib>

// TODO(JMM): Is relative path right here?
#include "portability.hpp"

// Use these macros for error handling. A macro is required, as
// opposed to a function, so that the file name and line number can be
// pulled from the call site.

// Messages passed in can be C strings, C++ strings, or stringstreams

// Use this as an assert with an error message. This assert is *not*
// disabled when compiling for production.
#define PORTABLE_ALWAYS_REQUIRE(condition, message)                                      \
  PortsOfCall::ErrorChecking::require(condition, #condition, message, __FILE__, __LINE__)

// Use this to abort the program with an error message with file and
// line number.
#define PORTABLE_ALWAYS_ABORT(message)                                                   \
  PortsOfCall::ErrorChecking::abort(message, __FILE__, __LINE__)

// Prints a warning with file and line number.
#define PORTABLE_ALWAYS_WARN(message)                                                    \
  PortsOfCall::ErrorChecking::warn(message, __FILE__, __LINE__)

// Fills a char* array output with an error message
// with file name and line number. Note there is no bounds checking
// so make sure you allocate enough memory.
#define PORTABLE_ERROR_MESSAGE(message, output)                                          \
  PortsOfCall::ErrorChecking::error_msg(message, __FILE__, __LINE__, output)

// Aborts the program with an error message with file and line number.
// if PORTABILITY_STRATEGY_NONE is enabled, then this throws a C++
// exception. Otherwise exceptions are not used.
#ifdef PORTABILITY_STRATEGY_NONE
#define PORTABLE_ALWAYS_THROW_OR_ABORT(message)                                          \
  PortsOfCall::ErrorChecking::abort_throws(message, __FILE__, __LINE__)
#else
#define PORTABLE_ALWAYS_THROW_OR_ABORT(message)                                          \
  PortsOfCall::ErrorChecking::abort(message, __FILE__, __LINE__)
#endif // PORTABILITY_STRATEGY_NONE

// Same as the above, but disabled when compiled in release mode.
#ifdef NDEBUG
#define PORTABLE_REQUIRE(condition, message) ((void)0)
#define PORTABLE_ABORT(message) ((void)0)
#define PORTABLE_WARN(message) ((void)0)
#define PORTABLE_THROW_OR_ABORT(message) ((void)0)
#else
#define PORTABLE_REQUIRE(condition, message) PORTABLE_ALWAYS_REQUIRE(condition, message)
#define PORTABLE_ABORT(message) PORTABLE_ALWAYS_ABORT(message)
#define PORTABLE_WARN(message) PORTABLE_ALWAYS_WARN(message)
#define PORTABLE_THROW_OR_ABORT(message) PORTABLE_ALWAYS_THROW_OR_ABORT(message)
#endif // NDEBUG

// Below are the underlying type-safe functions called by the
// macros. The functions are not to be called by downstream users, as
// they don't know file and line numbers. They simply enable type
// checking for the above macros,
namespace PortsOfCall {
namespace ErrorChecking {
namespace impl {
// Abort with an error message. The abort function depends on
// portability backend.
[[noreturn]] PORTABLE_INLINE_FUNCTION void abort() {
#ifdef PORTABILITY_STRATEGY_KOKKOS
  Kokkos::abort("");
  // For some versions of Kokkos, Kokkos::abort ends control flow, but
  // is not marked as `[[noreturn]]`, so we need this loop to supress
  // a warning that the function does not return.
  while (true) {
  }
#else
  std::abort();
#endif // PORTABILITY_STRATEGY_KOKKOS
}
} // namespace impl

// Prints an error message describing the failed condition in an
// assert (the assertion is applied above in the macro) with file name
// and line number. Then aborts.
PORTABLE_INLINE_FUNCTION void require(bool condition_bool, const char *const condition,
                                      const char *const message,
                                      const char *const filename, int const linenumber) {
  using PortsOfCall::printf;
  if (!condition_bool) {
    printf("### ERROR\n  Condition:   %s\n  Message:     %s\n  File:        "
           "%s\n  Line number: %i\n",
           condition, message, filename, linenumber);
    impl::abort();
  }
}
inline void require(bool condition_bool, const char *const condition,
                    std::string const &message, const char *const filename,
                    int const linenumber) {
  require(condition_bool, condition, message.c_str(), filename, linenumber);
}
inline void require(bool condition_bool, const char *const condition,
                    std::stringstream const &message, const char *const filename,
                    int const linenumber) {
  require(condition_bool, condition, message.str().c_str(), filename, linenumber);
}

// Prints an error message with file name and line number and then aborts.
[[noreturn]] PORTABLE_INLINE_FUNCTION void
abort(const char *const message, const char *const filename, int const linenumber) {
  using PortsOfCall::printf;
  printf("### ERROR\n  Message:     %s\n  File:        %s\n  Line number: %i\n", message,
         filename, linenumber);
  impl::abort();
}
[[noreturn]] inline void abort(std::stringstream const &message,
                               const char *const filename, int const linenumber) {
  abort(message.str().c_str(), filename, linenumber);
}
[[noreturn]] inline void abort(std::string const &message, const char *const filename,
                               int const linenumber) {
  abort(message.c_str(), filename, linenumber);
}

// Prints an error message with file name and line number and then
// throws an exception.
[[noreturn]] inline void abort_throws(const char *const message,
                                      const char *const filename, int const linenumber) {
  std::stringstream msg;
  msg << "### ERROR\n  Message:     " << message << "\n  File:        " << filename
      << "\n  Line number: " << linenumber << std::endl;
  throw std::runtime_error(msg.str().c_str());
}
[[noreturn]] inline void abort_throws(std::string const &message,
                                      const char *const filename, int const linenumber) {
  abort_throws(message.c_str(), filename, linenumber);
}
[[noreturn]] inline void abort_throws(std::stringstream const &message,
                                      const char *const filename, int const linenumber) {
  abort_throws(message.str().c_str(), filename, linenumber);
}

// Prints an warning message with file name and line number and then
// program continues.
PORTABLE_INLINE_FUNCTION
void warn(const char *const message, const char *const filename, int const linenumber) {
  using PortsOfCall::printf;
  printf("### WARNING\n  Message:     %s\n  File:        %s\n  Line number: "
         "%i\n",
         message, filename, linenumber);
}
inline void warn(std::stringstream const &message, const char *const filename,
                 int const linenumber) {
  warn(message.str().c_str(), filename, linenumber);
}
inline void warn(std::string const &message, const char *const filename,
                 int const linenumber) {
  warn(message.c_str(), filename, linenumber);
}

// Fills the output_message char* array with an error message with
// filename and line number.
PORTABLE_INLINE_FUNCTION
void error_msg(const char *const input_message, const char *const filename,
               int const linenumber, char *output_message) {
  PortsOfCall::sprintf(
      output_message,
      "### ERROR\n  Message:     %s\n  File:        %s\n  Line number: %i\n",
      input_message, filename, linenumber);
}
inline void error_msg(std::stringstream const &input_message, const char *const filename,
                      int const linenumber, char *output_message) {
  error_msg(input_message.str().c_str(), filename, linenumber, output_message);
}
inline void error_msg(std::string const &input_message, const char *const filename,
                      int const linenumber, char *output_message) {
  error_msg(input_message.c_str(), filename, linenumber, output_message);
}

} // namespace ErrorChecking
} // namespace PortsOfCall

#endif // _PORTS_OF_CALL_PORTABLE_ERRORS_HPP_
