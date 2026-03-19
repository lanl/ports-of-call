#pragma once
// ragged.hpp
//
// Minimal compile-time backend selection.
// Exposes a single namespace:
//   namespace ragged = <selected backend>;
//
// Selection macros:
//   * -DRAGGED_USE_KOKKOS  -> ragged_kokkos_min
//   * -DRAGGED_USE_THRUST  -> ragged_thrust_min
//   * (default)            -> ragged_std_min
//
// All backends provide:
//   * ragged::build_nd<T,DepthDims,Index>(n_root, body [, exec_or_stream_or_ignored])
//   * ragged::only_fill(row, lambda)
//   * ragged::span<T>

#include <ports-of-call/portability.hpp>

#if defined(PORTABILITY_STRATEGY_KOKKOS)
  #include <ports-of-call/ragged/ragged_kokkos.hpp>
  namespace PortsOfCall {
  namespace ragged = ragged_kokkos;
  }
#elif defined(PORTABILITY_STRATEGY_CUDA)
  #include <ports-of-call/ragged/ragged_thrust.hpp>
  namespace PortsOfCall {
  namespace ragged = ragged_thrust;
  }
#elif defined(PORTABILITY_STRATEGY_NONE)
  #include <ports-of-call/ragged/ragged_std.hpp>
  namespace PortsOfCall {
  namespace ragged = ragged_std;
  }
#endif
