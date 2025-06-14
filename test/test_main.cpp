//------------------------------------------------------------------------------
// © 2024. Triad National Security, LLC. All rights reserved.  This
// program was produced under U.S. Government contract 89233218CNA000001
// for Los Alamos National Laboratory (LANL), which is operated by Triad
// National Security, LLC for the U.S.  Department of Energy/National
// Nuclear Security Administration. All rights in the program are
// reserved by Triad National Security, LLC, and the U.S. Department of
// Energy/National Nuclear Security Administration. The Government is
// granted for itself and others acting on its behalf a nonexclusive,
// paid-up, irrevocable worldwide license in this material to reproduce,
// prepare derivative works, distribute copies to the public, perform
// publicly and display publicly, and to permit others to do so.
//------------------------------------------------------------------------------

#include <catch2/catch_session.hpp>
#include <ports-of-call/portability.hpp>

#ifdef PORTABILITY_STRATEGY_KOKKOS
#include <Kokkos_Core.hpp>
#endif

int main(int argc, char *argv[]) {
#ifdef PORTABILITY_STRATEGY_KOKKOS
  Kokkos::initialize();
#endif
  int result;
  {
    result = Catch::Session().run(argc, argv);
  }
#ifdef PORTABILITY_STRATEGY_KOKKOS
  Kokkos::finalize();
#endif
  return result;
}
