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

# PROJECT
# ----------------------------------------

# define project and version
set(PORTS_OF_CALL_VERSION 1.0.0)
project(ports-of-call VERSION ${PORTS_OF_CALL_VERSION})

# CMAKE WARMUP
# ----------------------------------------

# update the module path
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

# some global properties
# TODO: may be more appropriate down the road to attach the standard
#       directly to the target usage requirements
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_EXPORT_COMPILE_COMMANDS On)

# bring in some helpful CMake scripts 
# make cache variables for install destinations
include(GNUInstallDirs)
# package config file
include(CMakePackageConfigHelpers)

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

# CONFIGURATION LOGIC
# ----------------------------------------

# TODO: I ended up not liking this
# set the portability strategy. we only allow 3 options: None, Kokkos, Cuda
set(PORTABILITY_STRATEGY_OPTIONS "None;Kokkos;Cuda" CACHE STRING "" FORCE)
set_property(CACHE PORTABILITY_STRATEGY_OPTIONS
  PROPERTY STRINGS ${PORTABILITY_STRATEGY_OPTIONS})
if(NOT PORTABILITY_STRATEGY)
  list(GET PORTABILITY_STRATEGY_OPTIONS 0 PORTABILITY_STRATEGY)
endif()
# if the cache variable is not valid, display a warning and default to none
if (PORTABILITY_STRATEGY IN_LIST PORTABILITY_STRATEGY_OPTIONS)
  message(WARNING "The portability strategy [\"${PORTABILITY_STRATEGY}\"] is 
    not valid, defaulting to \"None\", valid options are 
    ${PORTABILITY_STRATEGY_OPTIONS}")
  list(GET PORTABILITY_STRATEGY_OPTIONS 0 PORTABILITY_STRATEGY)
endif()
# string-manip for the define
string(TOUPPER ${PORTABILITY_STRATEGY} PORTABILITY_STRATEGY)
set(PORTABILITY_STRATEGY "PORTABILITY_STRATEGY_${PORTABILITY_STRATEGY}" CACHE STRING "" FORCE) 
message(STATUS "Will define ${PORTABILITY_STRATEGY}")

# TARGET CONFIGURATION
# ----------------------------------------

# Define library
# use a string variable to ease derived variable naming
# NOTE: this gets used for directory names
set(POCLIB "ports-of-call")
# Pure-header library, so just use interface
add_library(${POCLIB} INTERFACE)
# Make an alias (useful for in-tree builds)
add_library(${POCLIB}::${POCLIB} ALIAS ${POCLIB})

target_compile_definitions(${POCLIB}
INTERFACE
  ${PORTABILITY_STRATEGY}
)

# Enables
# #include <ports-of-call/portability.hpp>
target_include_directories(${POCLIB}
  INTERFACE
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
  $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)


# INSTALL & EXPORT
# ----------------------------------------

# Coordinate external CMake interface (EXPORT) with targets
install(TARGETS ${POCLIB}
  EXPORT ${POCLIB}Targets
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
  INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

configure_package_config_file(${POCLIB}Config.cmake.in
  ${CMAKE_CURRENT_BINARY_DIR}/${POCLIB}Config.cmake
  INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/${POCLIB}
)

# ...and the version file
write_basic_package_version_file(
  ${CMAKE_CURRENT_BINARY_DIR}/${POCLIB}ConfigVersion.cmake
  VERSION ${PORTS_OF_CALL_VERSION}
  COMPATIBILITY SameMajorVersion )

# Install the cmake configuration files
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/${POCLIB}Config.cmake
              ${CMAKE_CURRENT_BINARY_DIR}/${POCLIB}ConfigVersion.cmake
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/${POCLIB} )

# Install the source header files
install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/ports-of-call/portability.hpp"
              "${CMAKE_CURRENT_SOURCE_DIR}/ports-of-call/portable_arrays.hpp"
  DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/${POCLIB}
  )

# Install the export target. This will define the CMake target
# for external projects when used with `find_package`
install(EXPORT ${POCLIB}Targets
  NAMESPACE ${POCLIB}::
  FILE "${POCLIB}Targets.cmake"
  DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/${POCLIB}"
  COMPONENT dev)

# Export configuration for external projects that reference
# just our build-tree; e.g. for submodules. To use, ensure 
# `CMAKE_PREFIX_PATH` points to this source directory.
# NOTE: This config will not be relocatable!
export(TARGETS ${POCLIB} NAMESPACE ${POCLIB}::
  FILE "${CMAKE_CURRENT_BINARY_DIR}/${POCLIB}Targets.cmake")
export(PACKAGE ${POCLIB})


