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

# make cache variables for install destinations
include(GNUInstallDirs)
# Add a library
# mostly includes
# Note the stupid hoops we jump through to expose
# ports-of-call::ports-of-call
set(POCLIB "ports-of-call")
add_library(${POCLIB} INTERFACE)
add_library(${POCLIB}::${POCLIB} ALIAS ${POCLIB})

# Portability strategy
if(PORTABILITY_STRATEGY)
  if (${PORTABILITY_STRATEGY} STREQUAL "Kokkos")
    target_compile_definitions(${POCLIB}
      INTERFACE
      PORTABILITY_STRATEGY_KOKKOS)
    message(STATUS "Setting portability strategy to Kokkos")
  else(${PORTABILITY_STRATEGY} STREQUAL "Cuda")
    target_compile_definitions(${POCLIB}
      INTERFACE
      PORTABILITY_STRATEGY_CUDA)
    message(STATUS "Setting portability strategy to Cuda")
  endif()
else()
  target_compile_definitions(${POCLIB}
    INTERFACE
    PORTABILITY_STRATEGY_NONE)
  message(STATUS "No portability strategy set. Setting to None")
endif()

# Enables
# #include <ports-of-call/portability.hpp>
target_include_directories(${POCLIB}
  INTERFACE
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
  $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
  )


# INSTALL
# ----------------------------------------
#set(CMAKE_INSTALL_LIBDIR lib/ CACHE FILEPATH "")
#set(CMAKE_INSTALL_INCLUDEDIR include/ CACHE_FILEPATH "")
#set(PORTS_OF_CALL_SYSCONFIG_INSTALL_DIR etc/ports-of-call FILEPATH "")

install(TARGETS ${POCLIB}
  EXPORT ports-of-callTargets
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
  INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)
# package config file
include(CMakePackageConfigHelpers)
configure_package_config_file(ports-of-callConfig.cmake.in
  ${CMAKE_CURRENT_BINARY_DIR}/ports-of-callConfig.cmake
  INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/ports-of-call
)
write_basic_package_version_file(
  ${CMAKE_CURRENT_BINARY_DIR}/ports-of-callConfigVersion.cmake
  VERSION ${PORTS_OF_CALL_VERSION}
  COMPATIBILITY SameMajorVersion )

install(FILES ${CMAKE_CURRENT_BINARY_DIR}/ports-of-callConfig.cmake
              ${CMAKE_CURRENT_BINARY_DIR}/ports-of-callConfigVersion.cmake
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/ports-of-call )

# Enables includes
install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/ports-of-call/portability.hpp"
              "${CMAKE_CURRENT_SOURCE_DIR}/ports-of-call/portable_arrays.hpp"
  DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/ports-of-call
  )

# Output and installtargets
install(EXPORT ports-of-callTargets
  NAMESPACE ${POCLIB}::
  FILE "ports-of-callTargets.cmake"
  DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/ports-of-call"
  COMPONENT dev)

# Export for in-tree builds
export(TARGETS ${POCLIB} NAMESPACE ${POCLIB}::
  FILE "${PROJECT_BINARY_DIR}/ports-of-callTargets.cmake")
export(PACKAGE ${POCLIB})


