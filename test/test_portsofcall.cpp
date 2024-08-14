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

#include <functional>
#include <iostream>
#include <numeric>
#include <ports-of-call/portability.hpp>
#include <ports-of-call/portable_arrays.hpp>
// #include <sys/_types/_ucontext.h>
#include <utility>
#include <vector>

#define CATCH_CONFIG_RUNNER
#include "catch2/catch_all.hpp"
#include "test_utilities.hpp"

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

TEST_CASE("PortableMDArrays Sizes are sane", "[PortableMDArray]") {
  using tape_t = std::vector<int>;

  constexpr std::array<std::size_t, MAXDIM> nxs{2, 5, 10, 10, 5, 2};

  std::vector<int> subsz(MAXDIM);
  std::vector<tape_t> dats;

  std::partial_sum(nxs.cbegin(), nxs.cend(), subsz.begin(),
                   std::multiplies<int>());
  for (auto i = 0; i < MAXDIM; ++i) {
    dats.push_back(std::vector<int>(subsz[i], nxs[i]));
  }

  SECTION("Can construct correctly") {
    auto pmd1 = PortableMDArray<int>(dats[0].data(), nxs[0]);

    REQUIRE(pmd1.GetSize() == subsz[0]);
    REQUIRE(pmd1.GetRank() == 1);

    auto pmd3 = PortableMDArray<int>(dats[3].data(), nxs[0], nxs[1], nxs[2]);

    REQUIRE(pmd3.GetSize() == subsz[2]);
    REQUIRE(pmd3.GetRank() == 3);
    REQUIRE(pmd3.GetDim1() == nxs[2]);
    REQUIRE(pmd3.GetDim2() == nxs[1]);
    REQUIRE(pmd3.GetDim3() == nxs[0]);
  }
}

TEST_CASE("Correct portable indexing", "[PortableMDArray]") {
  // layout
  auto iflat = [](auto nx, auto nxny) {
    return [=](auto k, auto j, auto i) { return i + nx * j + nxny * k; };
  };

  constexpr std::size_t NX = 32, NY = 64, NZ = 4;
  constexpr std::size_t NC = NX * NY * NZ;
  constexpr Real scale = 0.1;
  // size in bytes
  constexpr const std::size_t NCb = NC * sizeof(Real);

  // vector length N on host of Real
  std::vector<Real> tape_ref(NC), tape_buf(NC);

  for (auto n = 0; n < NC; ++n) {
    tape_ref[n] = scale * static_cast<Real>(n);
  }

  // device pointer
  Real *tape_d = (Real *)PORTABLE_MALLOC(NCb);

  auto view_d = PortableMDArray<Real>(tape_d, NZ, NY, NX);

  // set device values
  portableFor(
      "set unique val", 0, NZ, 0, NY, 0, NX,
      PORTABLE_LAMBDA(const int &k, const int &j, const int &i) {
        view_d(k, j, i) = scale * iflat(NX, NX * NY)(k, j, i);
      });

  portableCopyToHost(tape_buf.data(), tape_d, NCb);

  for (auto n = 0; n < NC; ++n) {
    INFO("REF=" << tape_ref[n] << " BUF=" << tape_buf[n] << " n=" << n);
    REQUIRE_THAT(tape_buf[n], Catch::Matchers::WithinRel(tape_ref[n]));
  }

  PORTABLE_FREE(tape_d);
}

TEST_CASE("portableCopy works with all portability strategies",
          "[PortableMDArray]") {
  auto index_func = [](auto i) { return i * i + 2.0 * i + 3.0; };
  // number of elements
  constexpr const size_t N = 32;
  // size in bytes
  constexpr const size_t Nb = N * sizeof(Real);
  // vector length N on host of Real
  std::vector<Real> b(N);
  // device pointer
  Real *a = (Real *)PORTABLE_MALLOC(Nb);

  // set device values to 0
  portableFor(
      "set to 0", 0, N, PORTABLE_LAMBDA(const int &i) { a[i] = 0.0; });

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
      PORTABLE_LAMBDA(const int &i, const int &j, const int &k, int &isum) {
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
  int nbad{0};
  for (int i = 0; i < N; ++i) {
    if (b[i] != index_func(i)) {
      nbad += 1;
    }
  }
  // make sure all elements match
  INFO("nbad=" << nbad);
  REQUIRE(nbad == 0);

  // free device memory
  PORTABLE_FREE(a);
}

// runs benchmarks for indexing.
// note: to run, execute
//    ./tests/test_portsofcall "[!benchmark]"
// (you may need to escape the `!` char depending
// on your shell)
TEST_CASE("Benchmark Indexing", "[!benchmark]") {
  constexpr std::size_t NX = 32, NY = 64, NZ = 16;
  constexpr std::size_t NU = 256, NV = 256, NW = 128;

  // construct a series of PortableMDArrays
  // of different rank and sizes, and
  // iterates through all contiguous indexing.

  // clang-format off
  SECTION("small") {
    BENCHMARK("index calc 1D") 
    { 
      return testing::idx_contiguous_bm(std::array{NX}); 
    };
    BENCHMARK("index calc 2D") 
    { 
      return testing::idx_contiguous_bm(std::array{NX, NY}); 
    };
    BENCHMARK("index calc 3D") 
    { 
      return testing::idx_contiguous_bm(std::array{NX, NY, NZ}); 
    };
  }

  SECTION("big") {
    BENCHMARK("index calc 1D") 
    { 
      return testing::idx_contiguous_bm(std::array{NU}); 
    };
    BENCHMARK("index calc 2D") 
    { 
      return testing::idx_contiguous_bm(std::array{NU, NV}); 
    };
    BENCHMARK("index calc 3D") 
    { 
      return testing::idx_contiguous_bm(std::array{NU, NV, NW}); 
    };

  }
  // clang-format on
}

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
