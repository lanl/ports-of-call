// © (or copyright) 2019-2024. Triad National Security, LLC. All rights
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
#include <vector>

#ifndef CATCH_CONFIG_FAST_COMPILE
#define CATCH_CONFIG_FAST_COMPILE
#include <catch2/catch_test_macros.hpp>
#endif

TEST_CASE("EXECUTION_IS_HOST is set correctly", "[PortsOfCall]") {
  // testing this is maybe nontrivial?
  auto isHost = PortsOfCall::EXECUTION_IS_HOST;

#if defined(PORTABILITY_STRATEGY_KOKKOS)
  auto checkHost = std::is_same<Kokkos::DefaultExecutionSpace,
                                Kokkos::HostSpace::execution_space>::value;
  REQUIRE(isHost == checkHost);
#elif defined(PORTABILITY_STRATEGY_CUDA)
  REQUIRE(isHost == false);
#else
  REQUIRE(isHost == true);
#endif
}

// this test is lifted directly from `spiner`;
// and there appears to be a significant amount of
// ports-of-call testing done there.
TEST_CASE("PortableMDArrays can be allocated from a pointer", "[PortableMDArray]") {
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
    tot = 0;
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

PORTABLE_FORCEINLINE_FUNCTION
Real index_func(size_t i) { return i * i + 2.0 * i + 3.0; }

TEST_CASE("portableCopy works with all portability strategies", "[portableCopy]") {
  // number of elements
  constexpr const size_t N = 32;
  // size in bytes
  constexpr const size_t Nb = N * sizeof(Real);
  // vector length N on host of Real
  std::vector<Real> b(N);
  // device pointer
  Real *a = (Real *)PORTABLE_MALLOC(Nb);

  // set device values to 0
  portableFor("set to 0", 0, N, PORTABLE_LAMBDA(const int &i) { a[i] = 0.0; });

  // set host values to reference
  for (size_t i = 0; i < N; ++i) {
    b[i] = index_func(i);
  }

  // copy data to device pointer
  portableCopyToDevice(a, b.data(), Nb);

  // check if device values match reference
  int sum{0};
  portableReduce(
      "check portableCopy", 0, N, 0, 0, 0, 0,
      PORTABLE_LAMBDA(const int &i, const int & /*j*/, const int & /*k*/, int &isum) {
        if (a[i] != index_func(i)) {
          isum += 1;
        }
      },
      sum);

  REQUIRE(sum == 0);

  // set b to 0
  for (auto &v : b) {
    v = 0.0;
  }

  // copy reference device a into b
  portableCopyToHost(b.data(), a, Nb);

  // count elements that don't match reference
  sum = 0;
  for (size_t i = 0; i < N; ++i) {
    if (b[i] != index_func(i)) {
      sum += 1;
    }
  }
  // make sure all elements match
  REQUIRE(sum == 0);

  // free device memory
  PORTABLE_FREE(a);
}

PORTABLE_FORCEINLINE_FUNCTION void expensive_operation(int iterations) {
  double sum = 0.0;
  for (int i = 0; i < iterations; i++) {
    sum += 1.0 / (i + 1.0); // Expensive floating-point operation
  }
  // Prevent compiler optimization
  if (sum < 0) {
    printf("Artificial delay: %f\n", sum);
  }
}

TEST_CASE("PORTABLE_FENCE properly synchronizes execution after a portableFor",
          "[portableFor][PORTABLE_FENCE]") {
  // Create host array
  constexpr int N = 10000;
  constexpr Real init_val = 2.0;
  constexpr Real mult_factor = 50.;
  std::vector<Real> h_data(N); // Zero initialized
  for (size_t i = 0; i < N; i++) {
    h_data[i] = init_val;
  }

  // Copy to device
  constexpr const size_t bytes = N * sizeof(Real);
  Real *d_data = (Real *)PORTABLE_MALLOC(bytes);
  portableCopyToDevice(d_data, h_data.data(), bytes);

  // Use portableFor since it doesn't have an implicit fence
  portableFor(
      "Expensive loop to test PORTABLE_FENCE", 0, N, PORTABLE_LAMBDA(const int i) {
        expensive_operation(1e6);
        d_data[i] *= mult_factor;
      });

  // Fence before reduction
  PORTABLE_FENCE("Fence after expensive operation");

  // JHP: it's not completely clear to us whether this test would fail
  // without the fence. Are device loops inherently sequential?
  int n_wrong = 0;
  portableReduce(
      "Check PORTABLE_FENCE", 0, N,
      PORTABLE_LAMBDA(const int &i, int &n_wrong) {
        if (d_data[i] != init_val * mult_factor) {
          n_wrong += 1;
        }
      },
      n_wrong);

  REQUIRE(n_wrong == 0);

  // free device memory
  PORTABLE_FREE(d_data);
}
