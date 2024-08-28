#define CATCH_CONFIG_MAIN
#define CATCH_CONFIG_ENABLE_BENCHMARKING
#include "catch2/catch.hpp"

#ifdef PORTABILITY_STRATEGY_KOKKOS
#include <Kokkos_Core.hpp>
#endif

class testRunListener : public Catch::TestEventListenerBase
{
public:
    using Catch::TestEventListenerBase::TestEventListenerBase;

    void testRunStarting(Catch::TestRunInfo const &) override
    {
#ifdef PORTABILITY_STRATEGY_KOKKOS
        Kokkos::initialize();
#endif
    }

    void testRunEnded(Catch::TestRunStats const &) override
    {
#ifdef PORTABILITY_STRATEGY_KOKKOS
        Kokkos::finalize();
#endif
    }
};

CATCH_REGISTER_LISTENER(testRunListener)
