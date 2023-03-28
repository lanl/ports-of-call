// © (or copyright) 2019-2021. Triad National Security, LLC. All rights
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

#include <ports-of-call/portability.hpp>
#include <ports-of-call/portable_arrays.hpp>

#define CATCH_CONFIG_RUNNER
#include "catch2/catch.hpp"

// this test is lifted directly from `spiner`;
// and there appears to be a significant amount of
// ports-of-call testing done there.
TEST_CASE("PortableMDArrays can be allocated from a pointer",
          "[PortableMDArray]") {
  constexpr int N = 2;
  constexpr int M = 3;
  std::vector<int> data(N * M);
  PortableMDArray<int> a;
  int tot = 0;
  for (int i = 0; i < N * M; i++) {
    data[i] = tot;
    tot++;
  }
  a.NewPortableMDArray(data.data(), M, N);

  SECTION("Shape should be NxM") {
    REQUIRE(a.GetDim1() == N);
    REQUIRE(a.GetDim2() == M);
  }

  SECTION("Stride is as set by initialized pointer") {
    int tot = 0;
    for (int j = 0; j < M; j++) {
      for (int i = 0; i < N; i++) {
        REQUIRE(a(j, i) == tot);
        tot++;
      }
    }
  }

  SECTION("Identical slices of the same data should compare equal") {
    PortableMDArray<int> aslc1, aslc2;
    aslc1.InitWithShallowSlice(a, 1, 0, 2);
    aslc2.InitWithShallowSlice(a, 1, 0, 2);
    REQUIRE(aslc1 == aslc2);
  }

}

#ifdef PORTABILITY_STRATEGY_KOKKOS

SCENARIO("Kokkos functionality","sometest") {

}
#endif

int main(int argc, char *argv[]) {

#ifdef PORTABILITY_STRATEGY_KOKKOS
  Kokkos::initialize();
#endif
  int result;
  { result = Catch::Session().run(argc, argv); }
#ifdef PORTABILITY_STRATEGY_KOKKOS
  Kokkos::finalize();
#endif
  return result;
}
