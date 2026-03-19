#pragma once
// ragged_thrust.hpp
//
// Minimal Thrust backend for ragged_offsets_core.hpp.
// Exposes only:
//   * ragged_thrust::build_nd<T,DepthDims,Index,Stream>(n_root, body [, stream])
//   * ragged_thrust::only_fill(row, lambda)
//   * ragged_thrust::span<T>
//   * ragged_thrust::ContiguousRagged<T,DepthDims,Index,Stream>

#include "ragged_offsets_core.hpp"

#include <cstddef>
#include <type_traits>
#include <utility>

#include <thrust/fill.h>
#include <thrust/scan.h>
#include <thrust/for_each.h>
#include <thrust/copy.h>
#include <thrust/iterator/counting_iterator.h>
#include <thrust/device_ptr.h>
#include <thrust/device_vector.h>
#include <thrust/host_vector.h>

namespace ragged_thrust {

namespace detail {

#if defined(RAGGED_THRUST_USE_HOST)

using stream_t = int;
inline stream_t default_stream() { return 0; }

template <class Stream>
inline auto policy(Stream) { return thrust::host; }

template <class U>
using vector_t = thrust::host_vector<U>;

#else

#if defined(__CUDACC__)
  #include <cuda_runtime_api.h>
  #include <thrust/system/cuda/execution_policy.h>
  using stream_t = cudaStream_t;
  inline stream_t default_stream() { return 0; }
  inline auto policy(stream_t s) { return thrust::cuda::par.on(s); }
#elif defined(__HIPCC__)
  #include <hip/hip_runtime_api.h>
  #include <thrust/system/hip/execution_policy.h>
  using stream_t = hipStream_t;
  inline stream_t default_stream() { return 0; }
  inline auto policy(stream_t s) { return thrust::hip::par.on(s); }
#else
  using stream_t = int;
  inline stream_t default_stream() { return 0; }
  inline auto policy(stream_t) { return thrust::device; }
#endif

template <class U>
using vector_t = thrust::device_vector<U>;

#endif // host/device

struct backend_thrust {
  template <class U>
  using vector = vector_t<U>;

  template <class Vec>
  static auto raw_ptr(Vec& v) { return thrust::raw_pointer_cast(v.data()); }

  template <class Vec>
  static auto raw_ptr(const Vec& v) { return thrust::raw_pointer_cast(v.data()); }

  template <class Vec, class V, class Stream>
  static void fill(Vec& v, const V& value, Stream s) {
    auto pol = policy(s);
    thrust::fill(pol, v.begin(), v.end(), value);
  }

  template <class Vec, class Stream>
  static void inclusive_scan_inplace(Vec& v, const std::size_t start, Stream s) {
    if (v.size() == 0 || start >= static_cast<std::size_t>(v.size())) return;
    auto pol = policy(s);
    thrust::inclusive_scan(pol,
                           v.begin() + static_cast<std::ptrdiff_t>(start),
                           v.end(),
                           v.begin() + static_cast<std::ptrdiff_t>(start));
  }

  template <class Vec, class Stream>
  static typename Vec::value_type read_back(const Vec& v, const std::size_t idx, Stream s) {
    using value_type = typename Vec::value_type;
    value_type out{};
#if defined(RAGGED_THRUST_USE_HOST)
    out = v[idx];
#else
    auto pol = policy(s);
    thrust::copy_n(pol, v.begin() + static_cast<std::ptrdiff_t>(idx), 1, &out);
#endif
    return out;
  }

  template <class F, class Stream>
  static void for_each_index(const std::size_t n, F f, Stream s) {
    if (n == 0) return;
    auto pol = policy(s);
    auto begin = thrust::make_counting_iterator<std::size_t>(0);
    thrust::for_each(pol, begin, begin + static_cast<std::ptrdiff_t>(n), f);
  }
};

} // namespace detail

template <class T>
using span = ::ragged_detail::span<T>;

using ::ragged_detail::only_fill;

template <class T, int DepthDims, class Index = std::size_t>
using ContiguousRagged =
  ::ragged_detail::ContiguousRaggedOffsets<detail::backend_thrust, T, DepthDims, Index>;

template <class T,
          int DepthDims,
          class Index = std::size_t,
          class Stream = detail::stream_t,
          class Body>
auto build_nd(const std::size_t n_root, Body body, Stream stream = detail::default_stream())
  -> ContiguousRagged<T, DepthDims, Index> {
  return ::ragged_detail::build_nd_impl<detail::backend_thrust, T, DepthDims, Index>(
    n_root, std::move(body), stream
  );
}

} // namespace ragged_thrust
