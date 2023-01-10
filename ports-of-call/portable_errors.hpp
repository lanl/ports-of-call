#ifndef _PORTS_OF_CALL_PORTABLE_ERRORS_HPP_
#define _PORTS_OF_CALL_PORTABLE_ERRORS_HPP_

// ========================================================================================
// © (or copyright) 2023. Triad National Security, LLC. All rights
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
#include <stdexcept>

// C headers
#include <cstdio>
#include <cstdlib>

// TODO(JMM): Is relative path right here?
#include "portability.hpp"

#define PORTABLE_ALWAYS_REQUIRE(condition, message)                            \
  if (!(condition)) {                                                          \
    PortsOfCall::ErrorChecking::require(#condition, message, __FILE__,         \
                                        __LINE__);                             \
  }

#define PORTABLE_ALWAYS_ABORT(message)                                         \
  PortsOfCall::ErrorChecking::abort(message, __FILE__, __LINE__);

#define PORTABLE_ALWAYS_WARN(message)                                          \
  PortsOfCall::ErrorChecking::warn(message, __FILE__, __LINE__);

#define PORTABLE_ERROR_MESSAGE(message, output)                                \
  PortsOfCall::ErrorChecking::error_msg(message, __FILE__, __LINE__, output);

#ifdef PORTABILITY_STRATEGY_NONE
#define PORTABLE_ALWAYS_THROW_OR_ABORT(message)                                \
  PortsOfCall::ErrorChecking::abort_throws(message, __FILE__, __LINE__);
#else
#define PORTABLE_ALWAYS_THROW_OR_ABORT(message)                                \
  PortsOfCall::ErrorChecking::abort(message, __FILE__, __LINE__);
#endif // PORTABILITY_STRATEGY_NONE

#ifdef NDEBUG
#define PORTABLE_REQUIRE(condition, message) ((void)0)
#define PORTABLE_ABORT(message) ((void)0)
#define PORTABLE_WARN(message) ((void)0)
#define PORTABLE_THROW_OR_ABORT(message) ((void)0)
#else
#define PORTABLE_REQUIRE(condition, message)                                   \
  PORTABLE_ALWAYS_REQUIRE(condition, message)
#define PORTABLE_ABORT(message) PORTABLE_ALWAYS_ABORT(message)
#define PORTABLE_WARN(message) PORTABLE_ALWAYS_WARN(message)
#define PORTABLE_THROW_OR_ABORT(message) PORTABLE_ALWAYS_THROW_OR_ABORT(message)
#endif // NDEBUG

namespace PortsOfCall {
namespace ErrorChecking {
namespace impl {
[[noreturn]] PORTABLE_INLINE_FUNCTION void abort(const char *const message) {
#ifdef PORTABILITY_STRATEGY_KOKKOS
  Kokkos::abort(message);
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

[[noreturn]] PORTABLE_INLINE_FUNCTION void require(const char *const condition,
                                                   const char *const message,
                                                   const char *const filename,
                                                   int const linenumber) {
  std::printf("### ERROR\n  Condition:   %s\n  Message:     %s\n  File:        "
              "%s\n  Line number: %i\n",
              condition, message, filename, linenumber);
  impl::abort(message);
}

inline void require(const char *const condition, std::string const &message,
                    const char *const filename, int const linenumber) {
  require(condition, message.c_str(), filename, linenumber);
}

inline void require(const char *const condition,
                    std::stringstream const &message,
                    const char *const filename, int const linenumber) {
  require(condition, message.str().c_str(), filename, linenumber);
}

[[noreturn]] PORTABLE_INLINE_FUNCTION void abort(const char *const message,
                                                 const char *const filename,
                                                 int const linenumber) {
  std::printf("### ERROR\n  Message:     %s\n  File:        %s\n  Line number: %i\n",
              message, filename, linenumber);
  impl::abort(message);
}

[[noreturn]] inline void abort(std::stringstream const &message,
                               const char *const filename,
                               int const linenumber) {
  abort(message.str().c_str(), filename, linenumber);
}

[[noreturn]] inline void abort(std::string const &message,
                               const char *const filename,
                               int const linenumber) {
  abort(message.str().c_str(), filename, linenumber);
}

[[noreturn]] inline void abort_throws(const char *const message,
                                      const char *const filename,
                                      int const linenumber) {
  std::stringstream msg;
  msg << "### ERROR\n  Message:     " << message
      << "\n  File:        " << filename << "\n  Line number: " << linenumber
      << std::endl;
  throw std::runtime_error(msg.str().c_str());
}

[[noreturn]] inline void abort_throws(std::string const &message,
                                      const char *const filename,
                                      int const linenumber) {
  abort_throws(message.c_str(), filename, linenumber);
}

[[noreturn]] inline void abort_throws(std::stringstream const &message,
                                     const char *const filename,
                                     int const linenumber) {
  abort_throws(message.str().c_str(), filename, linenumber);
}

PORTABLE_INLINE_FUNCTION
void warn(const char *const message, const char *const filename,
          int const linenumber) {
  std::printf("### WARNING\n  Message:     %s\n  File:        %s\n  Line number: "
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

PORTABLE_INLINE_FUNCTION
void error_msg(const char *const input_message, const char *const filename,
               int const linenumber, char *output_message) {
  std::sprintf(output_message,
               "### ERROR\n  Message:     %s\n  File:        %s\n  Line number: %i\n",
               input_message, filename, linenumber);
}

inline void error_msg(std::stringstream const &input_message, const char *const filename,
                      int const linenumber, char *output_message) {
  error_msg(input_message.str().c_str(), filename, linenumber, output_message);
}

inline void error_msg(std::string const &message, const char *const filename,
                        int const linenumber) {
  error_msg(input_message.c_str(), filename, linenumber, output_message);
}

} // namespace ErrorChecking
} // namespace PortsOfCall

#endif // _PORTS_OF_CALL_PORTABLE_ERRORS_HPP_