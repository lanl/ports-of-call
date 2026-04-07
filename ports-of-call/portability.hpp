#ifndef _PORTABILITY_HPP_
#define _PORTABILITY_HPP_

// ========================================================================================
// © (or copyright) 2019-2026. Triad National Security, LLC. All rights
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
// ========================================================================================

// This file was generated in part with generative AI

#include <cstdlib>

#include <string>
#include <string_view>
#include <type_traits>

#include <ports-of-call/portable_config.hpp>

#ifdef PORTABILITY_STRATEGY_KOKKOS
#ifdef PORTABILITY_STRATEGY_CUDA
#error "Two or more portability strategies defined."
#endif // PORTABILITY_STRATEGY_CUDA
#ifdef PORTABILITY_STRATEGY_NONE
#error "Two or more portability strategies defined."
#endif // PORTABILITY_STRATEGY_NONE
#endif // PORTABILITY_STRATEGY_KOKKOS

#ifdef PORTABILITY_STRATEGY_CUDA
#ifdef PORTABILITY_STRATEGY_NONE
#error "Two or more portability strategies defined."
#endif // PORTABILITY_STRATEGY_NONE
#endif // PORTABILITY_STRATEGY_CUDA

// if no portability strategy defined, define none
#if !(defined PORTABILITY_STRATEGY_CUDA || defined PORTABILITY_STRATEGY_KOKKOS)
#ifndef PORTABILITY_STRATEGY_NONE
#define PORTABILITY_STRATEGY_NONE
#endif // none not defined
#endif

#ifdef PORTABILITY_STRATEGY_KOKKOS
#include "Kokkos_Core.hpp"
#define PORTABLE_FUNCTION KOKKOS_FUNCTION
#define PORTABLE_INLINE_FUNCTION KOKKOS_INLINE_FUNCTION
#define PORTABLE_FORCEINLINE_FUNCTION KOKKOS_FORCEINLINE_FUNCTION
#define PORTABLE_LAMBDA KOKKOS_LAMBDA
#define _WITH_KOKKOS_
// Do we want to include additional terms here (for memory spaces, etc.)?
#define PORTABLE_FENCE(...) Kokkos::fence(__VA_ARGS__)
#else
#ifdef PORTABILITY_STRATEGY_CUDA
// currently error out on cuda since its not implemented
#error "CUDA portability strategy not yet implemented"
#include "cuda.h"
#define PORTABLE_FUNCTION __host__ __device__
#define PORTABLE_INLINE_FUNCTION __host__ __device__ inline
#define PORTABLE_FORCEINLINE_FUNCTION __host__ __device__ POC_ALWAYS_INLINE
#define PORTABLE_LAMBDA [=] __host__ __device__
#define PORTABLE_FENCE(...) cudaDeviceSynchronize()
#define _WITH_CUDA_
// It is worth noting here that we will not define
// _WITH_CUDA_ when we are doing KOKKOS (even with the
// CUDA backend)  Rely on KOKKOS_HAVE_CUDA in that case
#else
#define PORTABLE_FUNCTION
#define PORTABLE_INLINE_FUNCTION inline
#define PORTABLE_FORCEINLINE_FUNCTION POC_ALWAYS_INLINE
#define PORTABLE_LAMBDA [=]
#define PORTABLE_FENCE(...)
#endif
#endif
#define PORTABLE_MALLOC(...) PortsOfCall::portableMalloc<>(__VA_ARGS__)
#define PORTABLE_FREE(...) PortsOfCall::portableFree(__VA_ARGS__)

#ifndef SINGLE_PRECISION_ENABLED
#define SINGLE_PRECISION_ENABLED 0
#endif

#if SINGLE_PRECISION_ENABLED
typedef float Real;
#else
typedef double Real;
#endif

namespace PortsOfCall {
// compile-time constant to check if execution of memory space
// will be done on the host or is offloaded
#if defined(PORTABILITY_STRATEGY_KOKKOS)
constexpr bool EXECUTION_IS_HOST{
    Kokkos::SpaceAccessibility<Kokkos::DefaultExecutionSpace::memory_space,
                               Kokkos::HostSpace>::accessible};
#elif defined(PORTABILITY_STRATEGY_CUDA)
constexpr bool EXECUTION_IS_HOST{false};
#else
constexpr bool EXECUTION_IS_HOST{true};
#endif

namespace Exec {
#ifdef PORTABILITY_STRATEGY_KOKKOS
using Device = Kokkos::DefaultExecutionSpace;
using Host = Kokkos::DefaultHostExecutionSpace;
#else  // otherwise
struct Device {};
struct Host {};
#endif // PORTABILITY_STRATEGY_KOKKOS
} // namespace Exec

// portable printf
#define PORTABLE_MAX_NUM_CHAR (2048)
template <typename... Ts>
PORTABLE_INLINE_FUNCTION void printf(char const *const format, Ts... ts) {
  // disable for hip
#ifndef __HIPCC__
  if constexpr (sizeof...(Ts) > 0) {
    std::printf(format, ts...);
  } else {
    std::printf("%s", format);
  }
#endif // __HIPCC__
  return;
}
template <typename... Ts>
PORTABLE_INLINE_FUNCTION void snprintf(char *target, std::size_t size,
                                       char const *const format, Ts... ts) {
#ifndef __HIPCC__
  std::snprintf(target, size, format, ts...);
#endif // __HIPCC__
  return;
}

template <typename E>
inline auto portableMalloc([[maybe_unused]] E e, std::size_t size_bytes) {
  void *ret;
#ifdef PORTABILITY_STRATEGY_KOKKOS
  ret = Kokkos::kokkos_malloc<typename E::memory_space>(size_bytes);
#elif defined(PORTABILITY_STRATEGY_CUDA)
  if constexpr (std::is_same_v < E, Exec::Device) {
    cudaError_t e = cudaMalloc(&ret, size_bytes);
  } else if constexpr (std::is_same_v < E, Exec::Host) {
    ret = malloc(size_bytes);
  } else {
    throw
  }
#else  // PORTABILITY_STRATEGY_NONE
  ret = std::malloc(size_bytes);
#endif // PORTABILITY STRATEGY
  return ret;
}
template <typename E = Exec::Device>
inline auto portableMalloc(std::size_t size_bytes) {
  return portableMalloc(E(), size_bytes);
}

template <typename E, typename T>
void portableFree([[maybe_unused]] E e, T *p) {
#ifdef PORTABILITY_STRATEGY_KOKKOS
  Kokkos::kokkos_free<typename E::memory_space>(p);
#elif defined(PORTABILITY_STRATEGY_CUDA)
  if constexpr (std::is_same_v<E, Device>) {
    cudaError_t e = cudaFree(p);
  } else {
    std::free(p);
  }
#else  // PORTABILITY_STRATEGY_NONE
  std::free(p);
#endif // PORTABILITY STRATEGY
}
template <typename T>
void portableFree(T *p) {
  portableFree(Exec::Device(), p);
}

} // namespace PortsOfCall

// TODO(JMM): Pass in exec space here later
template <typename T>
void portableCopyToDevice(T *const to, T const *const from, size_t const size_bytes) {
  auto const length = size_bytes / sizeof(T);
#ifdef PORTABILITY_STRATEGY_KOKKOS
  using UM = Kokkos::MemoryUnmanaged;
  using HS = Kokkos::HostSpace;
  Kokkos::View<const T *, HS, UM> from_v(from, length);
  Kokkos::View<T *, UM> to_v(to, length);
  Kokkos::deep_copy(to_v, from_v);
#elif defined(PORTABILITY_STRATEGY_CUDA)
  cudaMemcpy(to, from, size_bytes, cudaMemcpyHostToDevice);
#else
  if (to != from) {
    std::copy(from, from + length, to);
  }
#endif
  return;
}

template <typename T>
void portableCopyToHost(T *const to, T const *const from, size_t const size_bytes) {
  auto const length = size_bytes / sizeof(T);
#ifdef PORTABILITY_STRATEGY_KOKKOS
  using UM = Kokkos::MemoryUnmanaged;
  using HS = Kokkos::HostSpace;
  Kokkos::View<const T *, UM> from_v(from, length);
  Kokkos::View<T *, HS, UM> to_v(to, length);
  Kokkos::deep_copy(to_v, from_v);
#elif defined(PORTABILITY_STRATEGY_CUDA)
  cudaMemcpy(to, from, size_bytes, cudaMemcpyDeviceToHost);
#else
  if (to != from) {
    std::copy(from, from + length, to);
  }
#endif
  return;
}

template <typename E, typename Function,
          typename = std::enable_if_t<!std::is_arithmetic_v<E>>>
void portableFor([[maybe_unused]] const char *name, [[maybe_unused]] const E &e,
                 int start, int stop, const Function &function) {
#ifdef PORTABILITY_STRATEGY_KOKKOS
  using policy = Kokkos::RangePolicy<E>;
  Kokkos::parallel_for(name, policy(e, start, stop), function);
#else
  for (int i = start; i < stop; i++) {
    function(i);
  }
#endif
}

template <typename E, typename Function,
          typename = std::enable_if_t<!std::is_arithmetic_v<E>>>
void portableFor([[maybe_unused]] const char *name, [[maybe_unused]] const E &e,
                 int starty, int stopy, int startx, int stopx, const Function &function) {
#ifdef PORTABILITY_STRATEGY_KOKKOS
  using Policy2D = Kokkos::MDRangePolicy<E, Kokkos::Rank<2>>;
  Kokkos::parallel_for(name, Policy2D(e, {starty, startx}, {stopy, stopx}), function);
#else
  for (int iy = starty; iy < stopy; iy++) {
    for (int ix = startx; ix < stopx; ix++) {
      function(iy, ix);
    }
  }
#endif
}

template <typename E, typename Function,
          typename = std::enable_if_t<!std::is_arithmetic_v<E>>>
void portableFor([[maybe_unused]] const char *name, [[maybe_unused]] const E &e,
                 int startz, int stopz, int starty, int stopy, int startx, int stopx,
                 const Function &function) {
#ifdef PORTABILITY_STRATEGY_KOKKOS
  using Policy3D = Kokkos::MDRangePolicy<E, Kokkos::Rank<3>>;
  Kokkos::parallel_for(name, Policy3D(e, {startz, starty, startx}, {stopz, stopy, stopx}),
                       function);
#else
  for (int iz = startz; iz < stopz; iz++) {
    for (int iy = starty; iy < stopy; iy++) {
      for (int ix = startx; ix < stopx; ix++) {
        function(iz, iy, ix);
      }
    }
  }
#endif
}

template <typename E = PortsOfCall::Exec::Device, typename Function,
          typename = std::enable_if_t<!std::is_arithmetic_v<E>>>
void portableFor([[maybe_unused]] const char *name, [[maybe_unused]] const E &e,
                 int starta, int stopa, int startz, int stopz, int starty, int stopy,
                 int startx, int stopx, const Function &function) {
#ifdef PORTABILITY_STRATEGY_KOKKOS
  using Policy4D = Kokkos::MDRangePolicy<E, Kokkos::Rank<4>>;
  Kokkos::parallel_for(
      name, Policy4D(e, {starta, startz, starty, startx}, {stopa, stopz, stopy, stopx}),
      function);
#else
  for (int ia = starta; ia < stopa; ia++) {
    for (int iz = startz; iz < stopz; iz++) {
      for (int iy = starty; iy < stopy; iy++) {
        for (int ix = startx; ix < stopx; ix++) {
          function(ia, iz, iy, ix);
        }
      }
    }
  }
#endif
}

template <typename E, typename Function,
          typename = std::enable_if_t<!std::is_arithmetic_v<E>>>
void portableFor([[maybe_unused]] const char *name, [[maybe_unused]] const E &e,
                 int startb, int stopb, int starta, int stopa, int startz, int stopz,
                 int starty, int stopy, int startx, int stopx, const Function &function) {
#ifdef PORTABILITY_STRATEGY_KOKKOS
  using Policy5D = Kokkos::MDRangePolicy<E, Kokkos::Rank<5>>;
  Kokkos::parallel_for(name,
                       Policy5D(e, {startb, starta, startz, starty, startx},
                                {stopb, stopa, stopz, stopy, stopx}),
                       function);
#else
  for (int ib = startb; ib < stopb; ib++) {
    for (int ia = starta; ia < stopa; ia++) {
      for (int iz = startz; iz < stopz; iz++) {
        for (int iy = starty; iy < stopy; iy++) {
          for (int ix = startx; ix < stopx; ix++) {
            function(ib, ia, iz, iy, ix);
          }
        }
      }
    }
  }
#endif
}

template <typename Head, typename... Tail,
          typename = std::enable_if_t<std::is_arithmetic_v<Head>>>
void portableFor([[maybe_unused]] const char *name, Head &&h, Tail &&...tail) {
  portableFor(name, PortsOfCall::Exec::Device(), h, std::forward<Tail>(tail)...);
}

template <typename E, typename Function, typename T,
          typename = std::enable_if_t<!std::is_arithmetic_v<E>>>
void portableReduce([[maybe_unused]] const char *name, [[maybe_unused]] const E &e,
                    int start, int stop, const Function &function, T &reduced) {
#ifdef PORTABILITY_STRATEGY_KOKKOS
  using Policy = Kokkos::RangePolicy<E>;
  Kokkos::parallel_reduce(name, Policy(e, start, stop), function, reduced);
#else
  for (int i = start; i < stop; i++) {
    function(i, reduced);
  }
#endif
}

template <typename E, typename Function, typename T,
          typename = std::enable_if_t<!std::is_arithmetic_v<E>>>
void portableReduce([[maybe_unused]] const char *name, [[maybe_unused]] const E &e,
                    int starty, int stopy, int startx, int stopx,
                    const Function &function, T &reduced) {
#ifdef PORTABILITY_STRATEGY_KOKKOS
  using Policy2D = Kokkos::MDRangePolicy<E, Kokkos::Rank<2>>;
  Kokkos::parallel_reduce(name, Policy2D(e, {starty, startx}, {stopy, stopx}), function,
                          reduced);
#else
  for (int iy = starty; iy < stopy; iy++) {
    for (int ix = startx; ix < stopx; ix++) {
      function(iy, ix, reduced);
    }
  }
#endif
}

template <typename E, typename Function, typename T,
          typename = std::enable_if_t<!std::is_arithmetic_v<E>>>
void portableReduce([[maybe_unused]] const char *name, [[maybe_unused]] const E &e,
                    int startz, int stopz, int starty, int stopy, int startx, int stopx,
                    const Function &function, T &reduced) {
#ifdef PORTABILITY_STRATEGY_KOKKOS
  using Policy3D = Kokkos::MDRangePolicy<E, Kokkos::Rank<3>>;
  Kokkos::parallel_reduce(name,
                          Policy3D(e, {startz, starty, startx}, {stopz, stopy, stopx}),
                          function, reduced);
#else
  for (int iz = startz; iz < stopz; iz++) {
    for (int iy = starty; iy < stopy; iy++) {
      for (int ix = startx; ix < stopx; ix++) {
        function(iz, iy, ix, reduced);
      }
    }
  }
#endif
}

template <typename E, typename Function, typename T,
          typename = std::enable_if_t<!std::is_arithmetic_v<E>>>
void portableReduce([[maybe_unused]] const char *name, [[maybe_unused]] const E &e,
                    int starta, int stopa, int startz, int stopz, int starty, int stopy,
                    int startx, int stopx, const Function &function, T &reduced) {
#ifdef PORTABILITY_STRATEGY_KOKKOS
  using Policy4D = Kokkos::MDRangePolicy<E, Kokkos::Rank<4>>;
  Kokkos::parallel_reduce(
      name, Policy4D(e, {starta, startz, starty, startx}, {stopa, stopz, stopy, stopx}),
      function, reduced);
#else
  for (int ia = starta; ia < stopa; ia++) {
    for (int iz = startz; iz < stopz; iz++) {
      for (int iy = starty; iy < stopy; iy++) {
        for (int ix = startx; ix < stopx; ix++) {
          function(ia, iz, iy, ix, reduced);
        }
      }
    }
  }
#endif
}

template <typename E, typename Function, typename T,
          typename = std::enable_if_t<!std::is_arithmetic_v<E>>>
void portableReduce([[maybe_unused]] const char *name, [[maybe_unused]] const E &e,
                    int startb, int stopb, int starta, int stopa, int startz, int stopz,
                    int starty, int stopy, int startx, int stopx,
                    const Function &function, T &reduced) {
#ifdef PORTABILITY_STRATEGY_KOKKOS
  using Policy5D = Kokkos::MDRangePolicy<E, Kokkos::Rank<5>>;
  Kokkos::parallel_reduce(name,
                          Policy5D(e, {startb, starta, startz, starty, startx},
                                   {stopb, stopa, stopz, stopy, stopx}),
                          function, reduced);
#else
  for (int ib = startb; ib < stopb; ib++) {
    for (int ia = starta; ia < stopa; ia++) {
      for (int iz = startz; iz < stopz; iz++) {
        for (int iy = starty; iy < stopy; iy++) {
          for (int ix = startx; ix < stopx; ix++) {
            function(ib, ia, iz, iy, ix, reduced);
          }
        }
      }
    }
  }
#endif
}

template <typename Head, typename... Tail,
          typename = std::enable_if_t<std::is_arithmetic_v<Head>>>
void portableReduce([[maybe_unused]] const char *name, Head &&h, Tail &&...tail) {
  portableReduce(name, PortsOfCall::Exec::Device(), h, std::forward<Tail>(tail)...);
}

#endif // PORTABILITY_HPP
