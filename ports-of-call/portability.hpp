#ifndef _PORTABILITY_HPP_
#define _PORTABILITY_HPP_

// ========================================================================================
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
// ========================================================================================

#include <string>
#include <string_view>
#include <type_traits>

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
// The following is a malloc on default memory space
#define PORTABLE_MALLOC(size) Kokkos::kokkos_malloc<>(size)
#define PORTABLE_FREE(ptr) Kokkos::kokkos_free<>(ptr)
// Do we want to include additional terms here (for memory spaces, etc.)?
#else
#ifdef PORTABILITY_STRATEGY_CUDA
// currently error out on cuda since its not implemented
#error "CUDA portability strategy not yet implemented"
#include "cuda.h"
#define PORTABLE_FUNCTION __host__ __device__
#define PORTABLE_INLINE_FUNCTION __host__ __device__ inline
#define PORTABLE_FORCEINLINE_FUNCTION                                                    \
  __host__ __device__ inline __attribute__((always_inline))
#define PORTABLE_LAMBDA [=] __host__ __device__
void *PORTABLE_MALLOC(size_t size) {
  void *devPtr = nullptr;
  cudaError_t e = cudaMalloc(&devPtr, size);
  return devPtr;
}
void PORTABLE_FREE(void *ptr) { cudaError_t e = cudaFree(ptr); }
#define _WITH_CUDA_
// It is worth noting here that we will not define
// _WITH_CUDA_ when we are doing KOKKOS (even with the
// CUDA backend)  Rely on KOKKOS_HAVE_CUDA in that case
#else
#define PORTABLE_FUNCTION
#define PORTABLE_INLINE_FUNCTION inline
#define PORTABLE_FORCEINLINE_FUNCTION inline __attribute__((always_inline))
#define PORTABLE_LAMBDA [=]
#define PORTABLE_MALLOC(size) malloc(size)
#define PORTABLE_FREE(ptr) free(ptr)
#endif
#endif

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
// portable printf
#define PORTABLE_MAX_NUM_CHAR (2048)
template <typename... Ts>
PORTABLE_INLINE_FUNCTION void printf(char const *const format, Ts... ts) {
  // disable for hip
#ifndef __HIPCC__
  std::printf(format, ts...);
#endif // __HIPCC__
  return;
}
// Variadic strlen
template <typename Head, typename... Tail>
PORTABLE_INLINE_FUNCTION std::size_t strlen(Head h, Tail... tail) {
  constexpr std::size_t MAX_I = 4096;
  std::size_t i = 0;
  if constexpr (std::is_convertible_v<Head, std::string_view>) {
    // don't want a non-terminating loop if there's now null
    // character.
    for (i = 0; i < MAX_I; ++i) {
      if (h[i] == '\0') {
        break;
      }
    }
  } else {
    i = 100; // some big number to account for things like %.14e
  }
  if constexpr (sizeof...(Tail) > 0) {
    i += strlen(tail...);
  }
  return i;
}
template <typename... Ts>
PORTABLE_INLINE_FUNCTION void snprintf(char *target, std::size_t size,
                                       char const *const format, Ts... ts) {
#ifndef __HIPCC__
  std::snprintf(target, size, format, ts...);
#endif // __HIPCC__
  return;
}
template <typename... Ts>
PORTABLE_INLINE_FUNCTION void sprintf(char *target, char const *const format, Ts... ts) {
#ifndef __HIPCC__
  std::size_t size = PortsOfCall::strlen(format, ts...);
  std::snprintf(target, size, format, ts...);
#endif // __HIPCC__
  return;
}
} // namespace PortsOfCall

template <typename T>
void portableCopyToDevice(T *const to, T const *const from, size_t const size_bytes) {
  auto const length = size_bytes / sizeof(T);
#ifdef PORTABILITY_STRATEGY_KOKKOS
  using UM = Kokkos::MemoryUnmanaged;
  using HS = Kokkos::HostSpace;
  Kokkos::View<const T *, HS, UM> from_v(from, length);
  Kokkos::View<T *, UM> to_v(to, length);
  deep_copy(to_v, from_v);
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
  deep_copy(to_v, from_v);
#elif defined(PORTABILITY_STRATEGY_CUDA)
  cudaMemcpy(to, from, size_bytes, cudaMemcpyDeviceToHost);
#else
  if (to != from) {
    std::copy(from, from + length, to);
  }
#endif
  return;
}

template <typename Function>
void portableFor([[maybe_unused]] const char *name, int start, int stop,
                 Function function) {
#ifdef PORTABILITY_STRATEGY_KOKKOS
  using policy = Kokkos::RangePolicy<>;
  Kokkos::parallel_for(name, policy(start, stop), function);
#else
  for (int i = start; i < stop; i++) {
    function(i);
  }
#endif
}

template <typename Function>
void portableFor([[maybe_unused]] const char *name, int starty, int stopy, int startx,
                 int stopx, Function function) {
#ifdef PORTABILITY_STRATEGY_KOKKOS
  using Policy2D = Kokkos::MDRangePolicy<Kokkos::Rank<2>>;
  Kokkos::parallel_for(name, Policy2D({starty, startx}, {stopy, stopx}), function);
#else
  for (int iy = starty; iy < stopy; iy++) {
    for (int ix = startx; ix < stopx; ix++) {
      function(iy, ix);
    }
  }
#endif
}

template <typename Function>
void portableFor([[maybe_unused]] const char *name, int startz, int stopz, int starty,
                 int stopy, int startx, int stopx, Function function) {
#ifdef PORTABILITY_STRATEGY_KOKKOS
  using Policy3D = Kokkos::MDRangePolicy<Kokkos::Rank<3>>;
  Kokkos::parallel_for(name, Policy3D({startz, starty, startx}, {stopz, stopy, stopx}),
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

template <typename Function>
void portableFor([[maybe_unused]] const char *name, int starta, int stopa, int startz,
                 int stopz, int starty, int stopy, int startx, int stopx,
                 Function function) {
#ifdef PORTABILITY_STRATEGY_KOKKOS
  using Policy4D = Kokkos::MDRangePolicy<Kokkos::Rank<4>>;
  Kokkos::parallel_for(
      name, Policy4D({starta, startz, starty, startx}, {stopa, stopz, stopy, stopx}),
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

template <typename Function>
void portableFor([[maybe_unused]] const char *name, int startb, int stopb, int starta,
                 int stopa, int startz, int stopz, int starty, int stopy, int startx,
                 int stopx, Function function) {
#ifdef PORTABILITY_STRATEGY_KOKKOS
  using Policy5D = Kokkos::MDRangePolicy<Kokkos::Rank<5>>;
  Kokkos::parallel_for(name,
                       Policy5D({startb, starta, startz, starty, startx},
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

template <typename Function, typename T>
void portableReduce([[maybe_unused]] const char *name, int start, int stop,
                    Function function, T &reduced) {
#ifdef PORTABILITY_STRATEGY_KOKKOS
  using Policy = Kokkos::RangePolicy<>;
  Kokkos::parallel_reduce(name, Policy(start, stop), function, reduced);
#else
  for (int i = start; i < stop; i++) {
    function(i, reduced);
  }
#endif
}

template <typename Function, typename T>
void portableReduce([[maybe_unused]] const char *name, int starty, int stopy, int startx,
                    int stopx, Function function, T &reduced) {
#ifdef PORTABILITY_STRATEGY_KOKKOS
  using Policy2D = Kokkos::MDRangePolicy<Kokkos::Rank<2>>;
  Kokkos::parallel_reduce(name, Policy2D({starty, startx}, {stopy, stopx}), function,
                          reduced);
#else
  for (int iy = starty; iy < stopy; iy++) {
    for (int ix = startx; ix < stopx; ix++) {
      function(iy, ix, reduced);
    }
  }
#endif
}

template <typename Function, typename T>
void portableReduce([[maybe_unused]] const char *name, int startz, int stopz, int starty,
                    int stopy, int startx, int stopx, Function function, T &reduced) {
#ifdef PORTABILITY_STRATEGY_KOKKOS
  using Policy3D = Kokkos::MDRangePolicy<Kokkos::Rank<3>>;
  Kokkos::parallel_reduce(name, Policy3D({startz, starty, startx}, {stopz, stopy, stopx}),
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

template <typename Function, typename T>
void portableReduce([[maybe_unused]] const char *name, int starta, int stopa, int startz,
                    int stopz, int starty, int stopy, int startx, int stopx,
                    Function function, T &reduced) {
#ifdef PORTABILITY_STRATEGY_KOKKOS
  using Policy4D = Kokkos::MDRangePolicy<Kokkos::Rank<4>>;
  Kokkos::parallel_reduce(
      name, Policy4D({starta, startz, starty, startx}, {stopa, stopz, stopy, stopx}),
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

template <typename Function, typename T>
void portableReduce([[maybe_unused]] const char *name, int startb, int stopb, int starta,
                    int stopa, int startz, int stopz, int starty, int stopy, int startx,
                    int stopx, Function function, T &reduced) {
#ifdef PORTABILITY_STRATEGY_KOKKOS
  using Policy5D = Kokkos::MDRangePolicy<Kokkos::Rank<5>>;
  Kokkos::parallel_reduce(name,
                          Policy5D({startb, starta, startz, starty, startx},
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

#endif // PORTABILITY_HPP
