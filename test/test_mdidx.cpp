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
#include <random>
#include <vector>

#include "catch2/benchmark/catch_chronometer.hpp"
#include "test_utilities.hpp"

#ifndef CATCH_CONFIG_FAST_COMPILE
#define CATCH_CONFIG_FAST_COMPILE
#include <catch2/benchmark/catch_benchmark.hpp>
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#endif

using namespace PortsOfCall;
constexpr const std::size_t MAXDIM = DEFAULT_MAXDIM;

TEST_CASE("PortableMDArrays Sizes are sane", "[PortableMDArray]") {
  using tape_t = std::vector<int>;

  constexpr std::array<std::size_t, MAXDIM> nxs{2, 5, 10, 10, 5, 2};

  std::vector<int> subsz(MAXDIM);
  std::vector<tape_t> dats;

  std::partial_sum(nxs.cbegin(), nxs.cend(), subsz.begin(), std::multiplies<int>());
  for (std::size_t i = 0; i < MAXDIM; ++i) {
    dats.push_back(std::vector<int>(subsz[i], nxs[i]));
  }

  SECTION("Can construct correctly") {
    auto pmd1 = PortableMDArray<int>(dats[0].data(), nxs[0]);

    REQUIRE(pmd1.GetSize() == subsz[0]);
    REQUIRE(pmd1.GetRank() == 1);

    auto pmd3 = PortableMDArray<int>(dats[3].data(), nxs[0], nxs[1], nxs[2]);

    REQUIRE(pmd3.GetSize() == subsz[2]);
    REQUIRE(pmd3.GetRank() == 3);
    REQUIRE(pmd3.GetDim<1>() == nxs[2]);
    REQUIRE(pmd3.GetDim<2>() == nxs[1]);
    REQUIRE(pmd3.GetDim<3>() == nxs[0]);
  }
}

TEST_CASE("Correct portable indexing", "[PortableMDArray]") {
  // layout

  constexpr std::size_t NX = 4, NY = 12, NZ = 3;
  constexpr std::size_t NC = NX * NY * NZ;
  constexpr Real scale = 1.0;
  // size in bytes
  constexpr const std::size_t NCb = NC * sizeof(Real);

  // vector length N on host of Real
  std::vector<Real> tape_ref(NC), tape_buf(NC);

  for (std::size_t n = 0; n < NC; ++n) {
    tape_ref[n] = scale * static_cast<Real>(n);
  }

  Real *tape_d = (Real *)PORTABLE_MALLOC(NCb);
  auto view_d = PortableMDArray<Real>(tape_d, NZ, NY, NX);

  // set device values
  portableFor(
      "set unique val", 0, NZ, 0, NY, 0, NX,
      PORTABLE_LAMBDA(const int &k, const int &j, const int &i) {
        view_d(k, j, i) = scale * (i + NX * j + NX * NY * k);
      });

  portableCopyToHost(tape_buf.data(), tape_d, NCb);
  for (std::size_t n = 0; n < NC; ++n) {
    std::cout << "REF=" << tape_ref[n] << " BUF=" << tape_buf[n] << " n=" << n << "\n";
  }

  for (std::size_t n = 0; n < NC; ++n) {
    INFO("REF=" << tape_ref[n] << " BUF=" << tape_buf[n] << " n=" << n);
    REQUIRE_THAT(tape_buf[n], Catch::Matchers::WithinRel(tape_ref[n]));
  }
  PORTABLE_FREE(tape_d);
}
