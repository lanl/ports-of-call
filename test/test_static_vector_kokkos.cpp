#ifdef PORTABILITY_STRATEGY_KOKKOS

#include "ports-of-call/portability.hpp"
#include "ports-of-call/static_vector.hpp"

#ifndef CATCH_CONFIG_FAST_COMPILE
#define CATCH_CONFIG_FAST_COMPILE
#include <catch2/catch_test_macros.hpp>
#endif

#include <Kokkos_Core.hpp>

#include <iostream>

// ================================================================================================

template <typename Space, std::size_t MaxSize>
PORTABLE_FUNCTION void print(PortsOfCall::static_vector<int, MaxSize> const &sv) {
  switch (sv.size()) {
  case 0:
    printf("[%s] {}\n", Space::name());
    break;
  case 1:
    printf("[%s] {%d}\n", Space::name(), sv[0]);
    break;
  case 2:
    printf("[%s] {%d,%d}\n", Space::name(), sv[0], sv[1]);
    break;
  case 3:
    printf("[%s] {%d,%d,%d}\n", Space::name(), sv[0], sv[1], sv[2]);
    break;
  case 4:
    printf("[%s] {%d,%d,%d,%d}\n", Space::name(), sv[0], sv[1], sv[2], sv[3]);
    break;
  case 5:
    printf("[%s] {%d,%d,%d,%d,%d}\n", Space::name(), sv[0], sv[1], sv[2], sv[3], sv[4]);
    break;
  case 6:
    printf("[%s] {%d,%d,%d,%d,%d,%d}\n", Space::name(), sv[0], sv[1], sv[2], sv[3], sv[4],
           sv[5]);
    break;
  default:
    printf("[%s] {%d,%d,...,%d,%d} (size=%d)\n", Space::name(), sv[0], sv[1],
           sv[sv.size() - 2], sv[sv.size() - 1], static_cast<int>(sv.size()));
    break;
  }
}

// ================================================================================================

template <typename Space, typename DataType, std::size_t MaxSize>
PORTABLE_FUNCTION void do_stuff(int const n,
                                PortsOfCall::static_vector<DataType, MaxSize> &sv) {
  printf("[%s] start\n", Space::name());
  print<Space>(sv);
  sv.push_back(n + 0);
  print<Space>(sv);
  sv.push_back(n + 1);
  print<Space>(sv);
  sv.push_back(n + 2);
  print<Space>(sv);
}

// ================================================================================================

TEST_CASE("static vector GPU", "[reaction_network]") {
  std::cout << "start" << std::endl;

  using HostSpace = Kokkos::HostSpace;
  using ExecSpace = Kokkos::DefaultExecutionSpace::memory_space;

  constexpr int N{10};

  std::cout << "declare sv_list_c" << std::endl;
  Kokkos::View<PortsOfCall::static_vector<int, N + 2> *, HostSpace> sv_list_c("sv_list",
                                                                              N);

  std::cout << "print sv_list_c" << std::endl;
  for (int n{0}; n < N; ++n) {
    for (int i{0}; i < n; ++i) {
      sv_list_c(n).push_back(i);
    }
    print<HostSpace>(sv_list_c(n));
  }

  std::cout << "build GPU mirror (sv_list_g)" << std::endl;
  auto sv_list_g = Kokkos::create_mirror_view_and_copy(ExecSpace(), sv_list_c);

  std::cout << "run on the GPU" << std::endl;
  Kokkos::fence(); // ensure the data has arrived on the GPU
  Kokkos::parallel_for(
      N, KOKKOS_LAMBDA(int const n) { do_stuff<ExecSpace>(n, sv_list_g(n)); });

  std::cout << "copy back to the CPU" << std::endl;
  Kokkos::fence(); // ensure the calculation is complete
  Kokkos::deep_copy(sv_list_c, sv_list_g);

  std::cout << "print again" << std::endl;
  Kokkos::fence(); // ensure the data has arrived back on the CPU
  for (int n{0}; n < N; ++n) {
    print<HostSpace>(sv_list_c(n));
  }

  std::cout << "check results" << std::endl;
  for (int n{0}; n < N; ++n) {
    REQUIRE(sv_list_c(n).size() == static_cast<std::size_t>(n + 3));
    for (std::size_t i{0}; i < sv_list_c(n).size(); ++i) {
      CHECK(sv_list_c(n)[i] == static_cast<int>(i));
    }
  }

  std::cout << "done" << std::endl;
}

// ================================================================================================

#endif // PORTABILITY_STRATEGY_KOKKOS
