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

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_EXPORT_COMPILE_COMMANDS On)

project(ports-of-call-cmake-test)

# Don't allow in-source builds
file(TO_CMAKE_PATH "${PROJECT_BINARY_DIR}/CMakeLists.txt" LOC_PATH)
if(EXISTS "${LOC_PATH}")
  message(FATAL_ERROR
    "You cannot build in a source directory (or any directory with a CMakeLists.txt file). "
    "Please make a build subdirectory. Feel free to remove CMakeCache.txt and CMakeFiles.")
endif()

find_package(ports-of-call REQUIRED)
if (NOT ports-of-call_FOUND)
  message(FATAL_ERROR "Ports of Call not found.")
endif()

if (TARGET ports-of-call::ports-of-call)
  message(STATUS "The ports of call library was succesfully provided")
  get_target_property(_poc_incdir ports-of-call::ports-of-call INTERFACE_INCLUDE_DIRECTORIES)
  message(STATUS "ports of call include directory: ${_poc_incdir}")
else()
  message(FATAL_ERROR "The ports-of-call::ports-of-call library is missing!")
endif()

add_library(mytest libtest.cc)
target_link_libraries(mytest
  PRIVATE
  ports-of-call::ports-of-call
  )
