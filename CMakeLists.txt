# ------------------------------------------------------------------------------#
# © 2021-2022. Triad National Security, LLC. All rights reserved.  This program
# was produced under U.S. Government contract 89233218CNA000001 for Los Alamos
# National Laboratory (LANL), which is operated by Triad National Security, LLC
# for the U.S.  Department of Energy/National Nuclear Security Administration.
# All rights in the program are reserved by Triad National Security, LLC, and
# the U.S. Department of Energy/National Nuclear Security Administration. The
# Government is granted for itself and others acting on its behalf a
# nonexclusive, paid-up, irrevocable worldwide license in this material to
# reproduce, prepare derivative works, distribute copies to the public, perform
# publicly and display publicly, and to permit others to do so.
# ------------------------------------------------------------------------------#

cmake_minimum_required(VERSION 3.14)

# PROJECT
# ----------------------------------------

# define project and version
set(PORTS_OF_CALL_VERSION 1.7.1)
project(ports-of-call VERSION ${PORTS_OF_CALL_VERSION} LANGUAGES CXX)

# CMAKE WARMUP
# ----------------------------------------

# Don't allow in-source builds
if(${CMAKE_SOURCE_DIR} STREQUAL ${CMAKE_BINARY_DIR})
  message(
    FATAL_ERROR
      "You cannot build in a source directory (or any directory with a CMakeLists.txt file). "
      "Please make a build subdirectory. Feel free to remove CMakeCache.txt and CMakeFiles."
  )
endif()

option(PORTS_OF_CALL_BUILD_TESTING "Test the current installation" OFF)
# off by default but possible to turn on
if(PORTS_OF_CALL_BUILD_TESTING)
  set(PORTS_OF_CALL_TEST_PORTABILITY_STRATEGY
      "None"
      CACHE STRING "Portability strategy used by tests")
  set_property(CACHE PORTS_OF_CALL_TEST_PORTABILITY_STRATEGY
               PROPERTY STRINGS None Cuda Kokkos)
endif()

# CONFIGURATION LOGIC
# ----------------------------------------
set(CMAKE_MODULE_PATH ${CMAKE_CURRENT_LIST_DIR}/cmake)

# TARGET CONFIGURATION
# ----------------------------------------

# Define library use a string variable to ease derived variable naming NOTE:
# this gets used for directory names
set(POCLIB "ports-of-call")
# Pure-header library, so just use interface
add_library(${POCLIB} INTERFACE)
# Make an alias (useful for in-tree builds)
add_library(${POCLIB}::${POCLIB} ALIAS ${POCLIB})

# make cache variables for install destinations
include(GNUInstallDirs)

# Enables #include <ports-of-call/portability.hpp>
target_include_directories(
  ${POCLIB} INTERFACE $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
                      $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)

target_compile_features(${POCLIB} INTERFACE cxx_std_17)

# TESTING
# ----------------------------------------
if(PORTS_OF_CALL_BUILD_TESTING)
  message(STATUS "Configuring tests")
  include(CTest)
  add_subdirectory(test)
endif()

# FORMATTING
# ----------------------------------------
include(Format)

# INSTALL & EXPORT
# ----------------------------------------

# package config file
include(CMakePackageConfigHelpers)

# Coordinate external CMake interface (EXPORT) with targets
install(
  TARGETS ${POCLIB}
  EXPORT ${POCLIB}Targets
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
  INCLUDES
  DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

configure_package_config_file(
  ${CMAKE_CURRENT_SOURCE_DIR}/cmake/${POCLIB}Config.cmake.in
  ${CMAKE_CURRENT_BINARY_DIR}/${POCLIB}Config.cmake
  INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/${POCLIB}
  NO_SET_AND_CHECK_MACRO # poc does not provide legacy style define
)

# ...and the version file
write_basic_package_version_file(
  ${CMAKE_CURRENT_BINARY_DIR}/${POCLIB}ConfigVersion.cmake
  VERSION ${PORTS_OF_CALL_VERSION}
  COMPATIBILITY SameMajorVersion)

# Install the cmake configuration files
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/${POCLIB}Config.cmake
              ${CMAKE_CURRENT_BINARY_DIR}/${POCLIB}ConfigVersion.cmake
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/${POCLIB})

# Install the source header files
install(DIRECTORY "${PROJECT_SOURCE_DIR}/${POCLIB}"
        DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}")

# Install the export target. This will define the CMake target for external
# projects when used with `find_package`
install(
  EXPORT ${POCLIB}Targets
  NAMESPACE ${POCLIB}::
  FILE ${POCLIB}Targets.cmake
  DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/${POCLIB})

# NOTE: This config will not be relocatable!
export(
  TARGETS ${POCLIB}
  NAMESPACE ${POCLIB}::
  FILE "${CMAKE_CURRENT_BINARY_DIR}/${POCLIB}Targets.cmake")

# Configuration summary
# ---------------------------------------------------------------------------

include(config_summary)

config_summary_header("Ports-of-Call" "ports-of-call" 48)

config_summary_block("CMake Options")
config_summary_variable("CMAKE_BUILD_TYPE")
config_summary_variable("CMAKE_C_COMPILER") # Does PoC use C or only C++?
config_summary_variable("CMAKE_C_COMPILER_VERSION")
config_summary_variable("CMAKE_C_FLAGS")
config_summary_variable("CMAKE_CXX_COMPILER")
config_summary_variable("CMAKE_CXX_COMPILER_VERSION")
config_summary_variable("CMAKE_CXX_FLAGS")

if(PORTS_OF_CALL_BUILD_TESTING)
  config_summary_block("Dependencies")
  config_summary_dependency("Kokkos" "Kokkos")
  config_summary_dependency("Catch2" "Catch2")
endif()

config_summary_block("User Options") # Are these the right user options?
config_summary_option("PORTS_OF_CALL_BUILD_TESTING")
config_summary_variable("PORTS_OF_CALL_TEST_PORTABILITY_STRATEGY")

config_summary_print()
