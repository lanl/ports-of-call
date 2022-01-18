#------------------------------------------------------------------------------#
# © 2021-2022. Triad National Security, LLC. All rights reserved.  This
# program was produced under U.S. Government contract 89233218CNA000001
# for Los Alamos National Laboratory (LANL), which is operated by Triad
# National Security, LLC for the U.S.  Department of Energy/National
# Nuclear Security Administration. All rights in the program are
# reserved by Triad National Security, LLC, and the U.S. Department of
# Energy/National Nuclear Security Administration. The Government is
# granted for itself and others acting on its behalf a nonexclusive,
# paid-up, irrevocable worldwide license in this material to reproduce,
# prepare derivative works, distribute copies to the public, perform
# publicly and display publicly, and to permit others to do so.
#------------------------------------------------------------------------------#

cmake_minimum_required(VERSION 3.14)
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_EXPORT_COMPILE_COMMANDS On)

set(PORTS_OF_CALL_VERSION 1.0.0)
project(ports-of-call VERSION ${PORTS_OF_CALL_VERSION})

# Don't allow in-source builds
file(TO_CMAKE_PATH "${PROJECT_BINARY_DIR}/CMakeLists.txt" LOC_PATH)
if(EXISTS "${LOC_PATH}")
  message(FATAL_ERROR
    "You cannot build in a source directory (or any directory with a CMakeLists.txt file). "
    "Please make a build subdirectory. Feel free to remove CMakeCache.txt and CMakeFiles.")
endif()

# If the user doesn't specify a build type, prefer RelWithDebInfo
set(default_build_type "RelWithDebInfo")
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  message(STATUS "Setting build type to '${default_build_type}' as none was specified.")
  set(CMAKE_BUILD_TYPE "${default_build_type}" CACHE
    STRING "Choose the type of build." FORCE)
  # Set the possible values of build type for cmake-gui
  set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS
    "Debug" "Release" "MinSizeRel" "RelWithDebInfo")
endif()

# Add a library
# mostly includes
add_library(ports-of-call::ports-of-call INTERFACE IMPORTED GLOBAL)

# Portability strategy
if(PORTABILITY_STRATEGY)
  if (${PORTABILITY_STRATEGY} STREQUAL "Kokkos")
    target_compile_definitions(ports-of-call::ports-of-call
      INTERFACE
      PORTABILITY_STRATEGY_KOKKOS)
    message(STATUS "Setting portability strategy to Kokkos")
  else(${PORTABILITY_STRATEGY} STREQUAL "Cuda")
    target_compile_definitions(ports-of-call::ports-of-call
      INTERFACE
      PORTABILITY_STRATEGY_CUDA)
    message(STATUS "Setting portability strategy to Cuda")
  endif()
else()
  target_compile_definitions(ports-of-call::ports-of-call
    INTERFACE
    PORTABILITY_STRATEGY_NONE)
  message(STATUS "No portability strategy set. Setting to None")
endif()

# Export for in-tree builds
export(PACKAGE ports-of-call)
export(TARGETS ports-of-call::ports-of-call
  FILE "{PROJECT_BINARY_DIR}/ports-of-callTargets.cmake")

# INSTALL
# ----------------------------------------
set(CMAKE_INSTALL_LIBDIR lib/ CACHE FILEPATH "")
set(CMAKE_INSTALL_INCLUDEDIR include/ CACHE_FILEPATH "")
set(PORTS_OF_CALL_SYSCONFIG_INSTALL_DIR etc/ports-of-call FILEPATH "")

install(TARGETS ports-of-call::ports-of-call
  EXPORT ports-of-callTargets
  DESTINATION "${CMAKE_INSTALL_LIBDIR}/ports-of-call")

# package config file
include(CMakePackageConfigHelpers)
configure_package_config_file(ports-of-callConfig.cmake.in
  ${CMAKE_CURRENT_BINARY_DIR}/ports-of-callConfig.cmake
  INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/ports-of-call/cmake
  PATH_VARS CMAKE_INSTALL_INCLUDEDIR PORTS_OF_CALL_SYSCONFIG_INSTALL_DIR)
write_basic_package_version_file(
  ${CMAKE_CURRENT_BINARY_DIR}/ports-of-callConfigVersion.cmake
  VERSION ${PORTS_OF_CALL_VERSION}
  COMPATIBILITY SameMajorVersion )
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/ports-of-callConfig.cmake
              ${CMAKE_CURRENT_BINARY_DIR}/ports-of-callConfigVersion.cmake
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/ports-of-call/cmake )

# Enables
# #include <ports-of-call/portability.hpp>
target_include_directories(ports-of-call::ports-of-call
  INTERFACE
  $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}>)

# Enables includes
install(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/ports-of-call"
  DESTINATION "include"
  FILES_MATCHING PATTERN "*.hpp"
  )

# Output and installtargets
install(EXPORT ports-of-callTargets
  FILE "ports-of-callTargets.cmake"
  DESTINATION "${CMAKE_INSTALL_LIBDIR}/ports-of-call/cmake"
  COMPONENT dev)
