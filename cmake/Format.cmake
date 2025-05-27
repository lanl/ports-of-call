#=========================================================================================
# (C) (or copyright) 2025. Triad National Security, LLC. All rights reserved.
#
# This program was produced under U.S. Government contract 89233218CNA000001 for Los
# Alamos National Laboratory (LANL), which is operated by Triad National Security, LLC
# for the U.S. Department of Energy/National Nuclear Security Administration. All rights
# in the program are reserved by Triad National Security, LLC, and the U.S. Department
# of Energy/National Nuclear Security Administration. The Government is granted for
# itself and others acting on its behalf a nonexclusive, paid-up, irrevocable worldwide
# license in this material to reproduce, prepare derivative works, distribute copies to
# the public, perform publicly and display publicly, and to permit others to do so.
#=========================================================================================

set(PORTS_CLANG_FORMAT_VERSION "19" CACHE STRING "preferred clang format version")
math(EXPR PORTS_CLANG_FORMAT_VERSION_MAX "${PORTS_CLANG_FORMAT_VERSION} + 1")

find_program(
    CLANG_FORMAT
    NAMES
        # Debian package manager, among others, provide this name
        clang-format-${PORTS_CLANG_FORMAT_VERSION}
        # MacPorts
        clang-format-mp-${PORTS_CLANG_FORMAT_VERSION}.0
        clang-format # Default name
    )

if (CLANG_FORMAT AND NOT CLANG_FORMAT_VERSION)
    # Get clang-format --version
    execute_process(
        COMMAND ${CLANG_FORMAT} --version
        OUTPUT_VARIABLE CLANG_FORMAT_VERSION_OUTPUT)

    if (CLANG_FORMAT_VERSION_OUTPUT MATCHES "clang-format version ([0-9]+\.[0-9]+\.[0-9]+)")
        set(CLANG_FORMAT_VERSION ${CMAKE_MATCH_1})
        message(STATUS "clang-format --version: " ${CLANG_FORMAT_VERSION})

        set(CLANG_FORMAT_VERSION ${CLANG_FORMAT_VERSION} CACHE STRING "clang-format version")
    endif()
endif()

if (NOT CLANG_FORMAT_VERSION)
    message(
        WARNING
        "Couldn't determine clang-format version. clang-format ${PORTS_CLANG_FORMAT_VERSION} is \
        expected - results on other versions may not be stable")

    set(CLANG_FORMAT_VERSION "0.0.0" CACHE STRING "clang-format version not found")
elseif (NOT (CLANG_FORMAT_VERSION VERSION_GREATER_EQUAL "${PORTS_CLANG_FORMAT_VERSION}.0" AND
         CLANG_FORMAT_VERSION VERSION_LESS "${PORTS_CLANG_FORMAT_VERSION_MAX}.0"))
    message(
        WARNING
        "clang-format version ${PORTS_CLANG_FORMAT_VERSION} is required - results on other \
        versions may not be stable. Your version is ${CLANG_FORMAT_VERSION}.")
endif()

# Specifically trying to exclude external here - I'm not sure if there's a better way
set(
    GLOBS
    ${PROJECT_SOURCE_DIR}/spiner/[^\.]*.cpp          ${PROJECT_SOURCE_DIR}/spiner/[^\.]*.hpp
    ${PROJECT_SOURCE_DIR}/ports-of-call/[^\.]*.cpp   ${PROJECT_SOURCE_DIR}/ports-of-call/[^\.]*.hpp
    ${PROJECT_SOURCE_DIR}/test/[^\.]*.cpp            ${PROJECT_SOURCE_DIR}/test/[^\.]*.hpp
    ${PROJECT_SOURCE_DIR}/installtest/[^\.]*.cpp     ${PROJECT_SOURCE_DIR}/installtest/[^\.]*.hpp
)

file(GLOB_RECURSE FORMAT_SOURCES CONFIGURE_DEPENDS ${GLOBS})

if (CLANG_FORMAT)
  add_custom_target(format_ports
    COMMAND ${CLANG_FORMAT} -i ${FORMAT_SOURCES}
    VERBATIM)
endif()
