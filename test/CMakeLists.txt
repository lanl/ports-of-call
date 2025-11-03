# © 2024. Triad National Security, LLC. All rights reserved.  This
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

if(NOT TARGET Catch2::Catch2)
  find_package(Catch2 QUIET)
  set(Catch2_VERSION ${Catch2_VERSION} PARENT_SCOPE)
  if(NOT Catch2_FOUND)
    message(STATUS "Fetching Catch2 as needed")
    include(FetchContent)
    FetchContent_Declare(
      Catch2
      GIT_REPOSITORY https://github.com/catchorg/Catch2.git
      # JMM: update to 3.7.1 12/18/24 for Ubuntu 24.04
      GIT_TAG v3.7.1)
    FetchContent_MakeAvailable(Catch2)
    list(APPEND CMAKE_MODULE_PATH ${Catch2_SOURCE_DIR}/contrib)
  endif()
  set(Catch2_VERSION "${Catch2_VERSION}" PARENT_SCOPE)
endif()

# this interface target is to collect
# compile/link options for the test
add_library(portsofcall_iface INTERFACE)
if (PORTS_OF_CALL_TEST_PORTABILITY_STRATEGY STREQUAL "Kokkos")
  if(NOT TARGET Kokkos::kokkos)
    find_package(Kokkos QUIET)
    set(Kokkos_VERSION ${Kokkos_VERSION} PARENT_SCOPE)
    if(NOT Kokkos_FOUND)
      message(STATUS "Fetching Kokkos as needed")
      include(FetchContent)
      FetchContent_Declare(
        Kokkos
        GIT_REPOSITORY https://github.com/kokkos/kokkos.git
        # This is most recent but any version that supports C++17 is
        # fine
        GIT_TAG 4.3.01)
      FetchContent_MakeAvailable(Kokkos)
    endif()
    set(Kokkos_VERSION "${Kokkos_VERSION}" PARENT_SCOPE)
  endif()

  target_link_libraries(portsofcall_iface INTERFACE Kokkos::kokkos)
  # this comes with ports-of-call target
  target_compile_definitions(portsofcall_iface INTERFACE PORTABILITY_STRATEGY_KOKKOS)
elseif (PORTS_OF_CALL_TEST_PORTABILITY_STRATEGY STREQUAL "Cuda")
  message(FATAL_ERROR "Cuda tests not yet fully supported")
  target_compile_definitions(portsofcall_iface INTERFACE PORTABILITY_STRATEGY_CUDA)
endif()

if(NOT PORTS_OF_CALL_SILENCE_WARNINGS)
    # turn on strict warnings and error
    # NOTE: The ROCm 6.2.1 installation on some machines (RZAdams and RZVernal at least) will
    #       inject a `--gcc-toolchain` command-line argument into Clang, which is a deprecated
    #       option.  Clang will warn about unused command-line arguments, so you get multiple
    #       instances of that warning.  Then `-Werror` promotes those to errors.  We can turn these
    #       back to non-fatal warnings with `-Wno-error=unused-command-line-argument` or we can
    #       turn off that particular warning with `-Wno-unused-command-line-argument`.
    # TODO: find comiler flags for non-GNU compatible compilers
    target_compile_options(portsofcall_iface INTERFACE
        $<$<IN_LIST:${CMAKE_CXX_COMPILER_ID},GNU>:-Werror;-Wall;-pedantic-errors;-Wunused-parameter>
        $<$<IN_LIST:${CMAKE_CXX_COMPILER_ID},AppleClang>:-Werror;-Wall;-Wunused-parameter>
        $<$<NOT:$<BOOL:${WIN32}>>:$<$<IN_LIST:${CMAKE_CXX_COMPILER_ID},Clang>:-Werror;-Wall;-Wno-unused-command-line-argument>>
        $<$<BOOL:${WIN32}>:$<$<IN_LIST:${CMAKE_CXX_COMPILER_ID},Clang>:-Wall;-Wno-c++98-compat;-Wno-c++98-compat-pedantic>>
        $<$<IN_LIST:${CMAKE_CXX_COMPILER_ID},Intel>:-Werror;-w2;-Wunused-variable;-Wunused-parameter;-diag-disable=remark>
        $<$<IN_LIST:${CMAKE_CXX_COMPILER_ID},PGI>:>
        $<$<IN_LIST:${CMAKE_CXX_COMPILER_ID},MSVC>:/permissive->
        $<$<IN_LIST:${CMAKE_CXX_COMPILER_ID},XL>:>
    )
else()
    # turn off warnings
    # TODO: find comiler flags for non-GNU compatible compilers
    target_compile_options(portsofcall_iface INTERFACE
        $<$<IN_LIST:${CMAKE_CXX_COMPILER_ID},GNU>:-w>
        $<$<IN_LIST:${CMAKE_CXX_COMPILER_ID},AppleClang>:>
        $<$<NOT:$<BOOL:${WIN32}>>:$<$<IN_LIST:${CMAKE_CXX_COMPILER_ID},Clang>:>>
        $<$<BOOL:${WIN32}>:$<$<IN_LIST:${CMAKE_CXX_COMPILER_ID},Clang>:>>
        $<$<IN_LIST:${CMAKE_CXX_COMPILER_ID},Intel>:>
        $<$<IN_LIST:${CMAKE_CXX_COMPILER_ID},PGI>:>
        $<$<IN_LIST:${CMAKE_CXX_COMPILER_ID},MSVC>:>
        $<$<IN_LIST:${CMAKE_CXX_COMPILER_ID},XL>:>
    )
endif()


target_link_libraries(portsofcall_iface INTERFACE Catch2::Catch2)

add_executable(test_portsofcall test_main.cpp)
target_link_libraries(test_portsofcall
  PRIVATE
    ports-of-call::ports-of-call
    portsofcall_iface
)

include(Catch)
catch_discover_tests(test_portsofcall)

target_sources(test_portsofcall
  PRIVATE
    test_portability.cpp
    test_array.cpp
    test_math_utils.cpp
    test_span.cpp
    test_static_vector.cpp
    test_static_vector_iterator.cpp
    test_variant.cpp
    "$<$<BOOL:${PORTABILITY_STRATEGY_KOKKOS}>:test_static_vector_kokkos.cpp>"
)
