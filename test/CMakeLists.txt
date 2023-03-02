# © 2021. Triad National Security, LLC. All rights reserved.  This
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


find_package(Catch2 REQUIRED)

# this interface target is to collect
# compile/link options for the test
add_library(portsofcall_iface INTERFACE)
# instructions for local content
# https://cmake.org/cmake/help/latest/module/FetchContent.html#variable:FETCHCONTENT_SOURCE_DIR_%3CuppercaseName%3E
# TLDR: set FETCHCONTENT_SOURCE_DIR_KOKKOS=path/to/local/checkout
if (PORTS_OF_CALL_TEST_USE_KOKKOS)
  find_package(Kokkos REQUIRED)
  target_link_libraries(portsofcall_iface INTERFACE Kokkos::kokkos)
  target_compile_definitions(portsofcall_iface INTERFACE PORTABILITY_STRATEGY_KOKKOS)
endif()

# add unit tests
add_executable(test_portsofcall test_portsofcall.cpp)

target_link_libraries(test_portsofcall
  PRIVATE
    ports-of-call::ports-of-call
    portsofcall_iface
)

include(Catch)
catch_discover_tests(test_portsofcall)

