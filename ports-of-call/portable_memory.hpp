//========================================================================================
// (C) (or copyright) 2026. Triad National Security, LLC. All rights reserved.
//
// This program was produced under U.S. Government contract 89233218CNA000001 for Los
// Alamos National Laboratory (LANL), which is operated by Triad National Security, LLC
// for the U.S. Department of Energy/National Nuclear Security Administration. All rights
// in the program are reserved by Triad National Security, LLC, and the U.S. Department
// of Energy/National Nuclear Security Administration. The Government is granted for
// itself and others acting on its behalf a nonexclusive, paid-up, irrevocable worldwide
// license in this material to reproduce, prepare derivative works, distribute copies to
// the public, perform publicly and display publicly, and to permit others to do so.
//========================================================================================

// This file was created in part or in whole by generative AI

#ifndef _PORTABLE_MEMORY_HPP_
#define _PORTABLE_MEMORY_HPP_

#include <array>
#include <cassert>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <map>
#include <mutex>
#include <new>
#include <stdexcept>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <vector>

#include <ports-of-call/portable_errors.hpp>

// Maximum slab block size, expressed as a base-2 exponent (2^N bytes). Requests
// whose padded size exceeds this take the large-request cache path. Defaults to
// 30 (1 GiB), the historical maximum block size. Define it before including
// this header (e.g. -DPORTS_OF_CALL_MAX_SLAB_BLOCK_LOG2=24) to shrink the slab
// ceiling -- useful so large-path tests exercise the cache without allocating
// more than 1 GiB per block.
#ifndef PORTS_OF_CALL_MAX_SLAB_BLOCK_LOG2
#define PORTS_OF_CALL_MAX_SLAB_BLOCK_LOG2 30
#endif

namespace PortsOfCall {

namespace memory_detail {
#ifdef PORTABILITY_STRATEGY_KOKKOS
template <class Space>
struct host_accessible
    : std::bool_constant<
          Kokkos::SpaceAccessibility<Kokkos::HostSpace, Space>::accessible> {};
template <>
struct host_accessible<void> : std::true_type {};
#else

template <class Space>
struct host_accessible : std::true_type {};
#endif

} // namespace memory_detail

template <class MemorySpace = void>
class SlabArenaPool {
 public:
  using memory_space = MemorySpace;

  // Tunables for the large-request cache (requests above the maximum slab block
  // size). See configure_large_allocations().
  struct LargeAllocationConfig {
    // Maximum total capacity retained in the free large-block cache. 0 disables
    // retention (freed large blocks go straight back to the backend).
    std::size_t max_cached_bytes = 8ull << 30;
    // Maximum number of free cached blocks.
    std::size_t max_cached_blocks = 16;
    // Blocks larger than this are returned to the backend on free rather than
    // cached.
    std::size_t max_single_cached_bytes = 4ull << 30;
    // Relative oversize tolerance for reuse, as an integer fraction:
    // slack <= need * numerator / denominator.
    std::size_t max_oversize_numerator = 1;
    std::size_t max_oversize_denominator = 4;
    // Absolute minimum slack permitted during matching, so tight relative
    // tolerances near the threshold do not reject otherwise-good reuse.
    std::size_t min_oversize_bytes = 64ull << 20;
    // Granularity to which new backend allocations are rounded up.
    std::size_t allocation_granularity_bytes = 64ull << 20;
  };

  struct LargeAllocationStats {
    std::uint64_t requests = 0;
    std::uint64_t cache_hits = 0;
    std::uint64_t cache_misses = 0;
    std::uint64_t backend_allocations = 0;
    std::uint64_t backend_deallocations = 0;
    std::uint64_t direct_frees = 0;
    std::uint64_t evictions = 0;
    std::uint64_t allocation_retries_succeeded = 0;

    std::size_t active_capacity_bytes = 0;
    std::size_t active_blocks = 0;
    std::size_t cached_bytes = 0;
    std::size_t cached_blocks = 0;

    std::size_t peak_active_capacity_bytes = 0;
    std::size_t peak_cached_bytes = 0;
  };

  // Process-wide singleton pool for this memory space.
  //
  // The singleton is a Meyers singleton, so it is constructed lazily on first
  // use with the default configuration (8 MiB minimum slab, 0.25 growth). To
  // size it differently, call instance().configure(...) *before* the first
  // allocation -- configure() throws once any slab has been allocated.
  //
  // Lifetime note: a function-local static is destroyed at program exit, which
  // for Kokkos builds happens *after* Kokkos::finalize(). Freeing slabs at that
  // point (kokkos_free) aborts. To avoid this we register a finalize hook the
  // first time the singleton is created so its slabs are released while Kokkos
  // is still live; release_all() is idempotent, so the eventual destructor call
  // becomes a harmless no-op. instance() must therefore first be called while
  // Kokkos is initialized (true for any allocation performed inside main()).
  static SlabArenaPool &instance() {
    static SlabArenaPool pool;
#ifdef PORTABILITY_STRATEGY_KOKKOS
    static const bool registered = []() {
      Kokkos::push_finalize_hook([]() { instance().release_all(); });
      return true;
    }();
    (void)registered;
#endif
    return pool;
  }

  explicit SlabArenaPool(std::size_t min_slab_bytes = 8ull << 20 /* 8 MiB */,
                         double growth_factor = 0.25) {
    configure(min_slab_bytes, growth_factor);
  }

  ~SlabArenaPool() { release_all(); }

  SlabArenaPool(const SlabArenaPool &) = delete;
  SlabArenaPool &operator=(const SlabArenaPool &) = delete;

  void configure(std::size_t min_slab_bytes, double growth_factor) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (!std::isfinite(growth_factor) || growth_factor < 0.0) {
      throw std::invalid_argument(
          "SlabArenaPool growth_factor must be finite and non-negative");
    }
    if (!slabs_.empty()) {
      throw std::runtime_error(
          "Cannot configure SlabArenaPool after allocations have occurred");
    }

    min_slab_bytes_ = std::max(min_slab_bytes, std::size_t(64 << 10));
    next_slab_bytes_ = min_slab_bytes_;
    growth_factor_ = growth_factor;
  }

  // Largest slab-served block, in bytes. Requests whose padded size exceeds
  // this take the large-request path. Exposed so callers and tests can size
  // requests relative to the (build-configurable) slab ceiling.
  static constexpr std::size_t max_slab_block_bytes() noexcept {
    return std::size_t(1) << max_size;
  }

  // Largest request (in bytes) that alloc_bytes can accept for a given
  // alignment. Requests up to max_block_bytes() are served by the slab path;
  // anything larger now goes to the large-request cache, so the effective
  // ceiling is bounded only by the overflow checks in alloc_bytes rather than
  // by the maximum slab block size. This lets PoolAllocator-backed containers
  // grow past 1 GiB and reach the large path.
  static std::size_t
  max_allocation_bytes(std::size_t alignment = alignof(std::max_align_t)) noexcept {
    if (!is_power_of_two(alignment)) {
      return 0;
    }

    alignment = std::max(alignment, alignof(std::max_align_t));

    const std::size_t padding = alignment - 1;

    if (overhead_bytes() > std::numeric_limits<std::size_t>::max() - padding) {
      return 0;
    }

    const std::size_t overhead = overhead_bytes() + padding;

    // alloc_bytes rejects (request + overhead) that overflows size_t; report
    // the largest request that survives that check.
    return std::numeric_limits<std::size_t>::max() - overhead;
  }

  void *alloc_bytes(std::size_t request,
                    std::size_t alignment = alignof(std::max_align_t)) {
    if (request == 0) {
      request = 1;
    }
    if (!is_power_of_two(alignment)) {
      throw std::invalid_argument("SlabArenaPool alignment must be a power of two");
    }

    alignment = std::max(alignment, alignof(std::max_align_t));

    const std::size_t padding = alignment - 1;

    std::size_t need = request;
    if (need > std::numeric_limits<std::size_t>::max() - overhead_bytes()) {
      throw std::bad_alloc{};
    }

    need += overhead_bytes();

    if (need > std::numeric_limits<std::size_t>::max() - padding) {
      throw std::bad_alloc{};
    }

    need += padding;

    if (need > max_block_bytes()) {
      // Requests that cannot be served by any power-of-two slab class take the
      // separate large-request path. `need` already folds in the header
      // overhead and alignment padding, so it is the true minimum number of
      // backend bytes required.
      return alloc_large(request, alignment, need);
    }

    const std::uint32_t idx = get_block_index(need);
    const std::size_t block_bytes = block_size(idx);

    std::lock_guard<std::mutex> lock(mutex_);
    any_allocation_ = true;

    if (void *p = pop_block(idx, request, alignment)) {
      return p;
    }

    if (slabs_.empty() || remaining_in_current_slab() < block_bytes) {
      add_slab(block_bytes);
    }

    return bump_slab(block_bytes, request, alignment, idx);
  }

  void free_bytes(void *p) noexcept {
    if (p == nullptr) {
      return;
    }

    if constexpr (host_accessible_) {
      // The kind and (for large blocks) the backend metadata live in the
      // in-band header, so classification needs no lock and no map lookup --
      // the small fast path stays free of ordered-container operations.
      Header *h =
          reinterpret_cast<Header *>(static_cast<std::byte *>(p) - sizeof(Header));
      if ((h == nullptr) || (h->sentinel != sentinel_)) {
        PORTABLE_ALWAYS_ABORT("SlabArenaPool corrupted header");
        return;
      }

      if (h->kind == AllocationKind::LargePool) {
        std::byte *const backend_base = h->block_start;
        const std::size_t capacity = h->block_bytes;
        h->sentinel = 0;
        {
          std::lock_guard<std::mutex> lock(mutex_);
          live_large_.erase(p);
        }
        free_large_after_unregister(backend_base, capacity);
        return;
      }

      if ((h->idx >= num_blocks) || (h->block_start == nullptr) ||
          (h->block_bytes != block_size(h->idx))) {
        PORTABLE_ALWAYS_ABORT("SlabArenaPool corrupted block metadata");
        return;
      }
      const std::uint32_t idx = h->idx;
      std::byte *const block_start = h->block_start;
      h->sentinel = 0;

      std::lock_guard<std::mutex> lock(mutex_);
      push_block(block_start, idx);
    } else {
      // Non-host-accessible spaces cannot embed a readable header, so both the
      // small live-block map and the large active registry are consulted.
      std::byte *backend_base = nullptr;
      std::size_t capacity = 0;
      {
        std::lock_guard<std::mutex> lock(mutex_);

        auto lit = live_large_.find(p);
        if (lit != live_large_.end()) {
          backend_base = static_cast<std::byte *>(lit->second.base_ptr);
          capacity = lit->second.capacity_bytes;
          live_large_.erase(lit);
        } else {
          auto it = meta_.live_blocks.find(p);
          if (it == meta_.live_blocks.end()) {
            PORTABLE_ALWAYS_ABORT("SlabArenaPool called with unknown pointer");
            return;
          }
          std::byte *const block_start = it->second.block_start;
          const std::uint32_t idx = it->second.idx;
          meta_.live_blocks.erase(it);
          push_block(block_start, idx);
          return;
        }
      }
      // Large block: registry entry already removed above.
      free_large_after_unregister(backend_base, capacity);
    }
  }

  void release_all() {
    std::vector<void *> backend_frees;
    {
      std::lock_guard<std::mutex> lock(mutex_);

      if constexpr (host_accessible_) {
        meta_.free_heads.fill(nullptr);
      } else {
        meta_.live_blocks.clear();
        for (auto &stack : meta_.free_blocks) {
          stack.clear();
        }
      }

      for (auto &slab : slabs_) {
        backend_deallocate(slab.base);
      }

      slabs_.clear();
      current_slab_ = 0;
      next_slab_bytes_ = min_slab_bytes_;

      // Large allocations never live in slabs_, so they must be released here
      // explicitly. This matches the existing "release everything" contract for
      // small allocations and, critically, ensures kokkos_malloc-backed large
      // blocks are freed while Kokkos is still live (release_all runs from the
      // finalize hook). Both cached-free and still-active large blocks go.
      backend_frees.reserve(free_large_blocks_.size() + live_large_.size());
      for (auto &kv : free_large_blocks_) {
        backend_frees.push_back(kv.second.base_ptr);
      }
      for (auto &kv : live_large_) {
        backend_frees.push_back(kv.second.base_ptr);
      }
      free_large_blocks_.clear();
      live_large_.clear();
      cached_large_bytes_ = 0;
      large_stats_.active_capacity_bytes = 0;
      large_stats_.active_blocks = 0;
      large_stats_.cached_bytes = 0;
      large_stats_.cached_blocks = 0;
    }

    for (void *p : backend_frees) {
      backend_deallocate(p);
    }
  }

  // Release every currently free (cached) large block, leaving active large
  // allocations untouched. Thread-safe with normal allocations.
  void trim_large_cache() { trim_large_cache_to(0); }

  // Release cached large blocks, oldest first, until the retained byte total is
  // at most target_bytes.
  void trim_large_cache_to(std::size_t target_bytes) {
    std::vector<LargeBlock> victims;
    {
      std::lock_guard<std::mutex> lock(mutex_);
      while (cached_large_bytes_ > target_bytes && !free_large_blocks_.empty()) {
        auto oldest = free_large_blocks_.begin();
        for (auto it = free_large_blocks_.begin(); it != free_large_blocks_.end();
             ++it) {
          if (it->second.last_used_epoch < oldest->second.last_used_epoch) {
            oldest = it;
          }
        }
        victims.push_back(oldest->second);
        const std::size_t victim_capacity = oldest->second.capacity_bytes;
        free_large_blocks_.erase(oldest);
        note_cached_removed(victim_capacity);
      }
    }
    release_large_blocks(victims);
  }

  // Configure the large-request cache. Like configure(), this must be called
  // before the first allocation of any kind.
  void configure_large_allocations(const LargeAllocationConfig &config) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (any_allocation_ || !slabs_.empty()) {
      throw std::runtime_error(
          "Cannot configure SlabArenaPool large allocations after allocations "
          "have occurred");
    }
    large_config_ = config;
  }

  LargeAllocationStats large_allocation_stats() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return large_stats_;
  }

  // Profiling functions for tuning slab sizes
  std::size_t slab_count() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return slabs_.size();
  }

  std::size_t total_slab_bytes() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::size_t total = 0;
    for (const auto &s : slabs_) {
      total += s.bytes;
    }
    return total;
  }

  std::size_t current_slab_remaining() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return remaining_in_current_slab();
  }

  // -------------------------------------------------------------------------
  // Device scratch support
  //
  // A common device pattern is to reserve one equally sized, aligned "slice"
  // of scratch per logical instance (e.g. one per team/thread) out of a single
  // pooled allocation, then sub-allocate arrays within each slice on device.
  // ScratchLayout describes the sub-arrays, reserve_device_scratch() carves the
  // pooled block, and DeviceBump does the device-side sub-allocation.
  // -------------------------------------------------------------------------

  // Describes a set of sub-arrays packed into one per-instance slice. Offsets
  // are computed host-side and are stable for the life of the layout.
  struct ScratchLayout {
    struct Field {
      std::size_t offset = 0;
      std::size_t bytes = 0;
      std::size_t align = 0;
    };

    // Append an array of n T's, returning its field id. Offset is aligned to
    // alignof(T) within the slice.
    template <class T>
    int add_array(std::size_t n) {
      const std::size_t a = alignof(T);
      cursor_ = align_up(cursor_, a);
      const std::size_t off = cursor_;
      const std::size_t sz = n * sizeof(T);
      fields_.push_back(Field{off, sz, a});
      cursor_ = off + sz;
      max_align_ = std::max(max_align_, a);
      return static_cast<int>(fields_.size() - 1);
    }

    const Field &operator[](int id) const {
      return fields_[static_cast<std::size_t>(id)];
    }

    // Size of one instance slice, rounded up so the next slice stays aligned.
    std::size_t bytes_per_instance() const { return align_up(cursor_, max_align_); }

    std::size_t alignment() const { return max_align_; }

   private:
    std::vector<Field> fields_;
    std::size_t cursor_ = 0;
    std::size_t max_align_ = alignof(std::max_align_t);
  };

  // Result of a scratch reservation: a single pooled block partitioned into
  // `instances` slices of `bytes_per_instance` each. Free with
  // free_bytes(reservation.base).
  struct Reservation {
    void *base = nullptr;
    std::size_t instances = 0;
    std::size_t bytes_per_instance = 0;
    std::size_t total_bytes = 0;

#ifdef PORTABILITY_STRATEGY_KOKKOS
    Kokkos::View<std::byte *, memory_space, Kokkos::MemoryTraits<Kokkos::Unmanaged>>
    view() const {
      return Kokkos::View<std::byte *, memory_space,
                          Kokkos::MemoryTraits<Kokkos::Unmanaged>>(
          static_cast<std::byte *>(base), total_bytes);
    }
#endif
  };

  // Device-callable bump allocator over a single slice of scratch memory.
  struct DeviceBump {
    std::byte *base = nullptr;
    std::size_t capacity = 0;
    std::size_t offset = 0;

    DeviceBump() = default;

    PORTABLE_INLINE_FUNCTION
    DeviceBump(void *p, std::size_t bytes)
        : base(static_cast<std::byte *>(p)), capacity(bytes), offset(0) {}

    PORTABLE_INLINE_FUNCTION
    std::size_t align_up_local(std::size_t x, std::size_t a) const {
      return (x + (a - 1)) & ~(a - 1);
    }

    PORTABLE_INLINE_FUNCTION
    void *alloc_bytes(std::size_t bytes, std::size_t alignment) {
      const std::size_t pos = align_up_local(offset, alignment);
      if (pos + bytes > capacity) {
        return nullptr;
      }
      void *p = base + pos;
      offset = pos + bytes;
      return p;
    }

    template <class T>
    PORTABLE_INLINE_FUNCTION T *alloc(std::size_t n) {
      return static_cast<T *>(alloc_bytes(n * sizeof(T), alignof(T)));
    }

    // Retained spelling for existing callers.
    template <class T>
    PORTABLE_INLINE_FUNCTION T *allocate(std::size_t n) {
      return alloc<T>(n);
    }

    PORTABLE_INLINE_FUNCTION
    void reset() { offset = 0; }
  };

  // Reserve `instances` slices sized/aligned to hold `layout`.
  Reservation reserve_device_scratch(const ScratchLayout &layout, std::size_t instances,
                                     std::size_t alignment = alignof(std::max_align_t)) {
    return reserve_device_scratch(layout.bytes_per_instance(), instances,
                                  std::max(alignment, layout.alignment()));
  }

  // Reserve `instances` slices of (at least) `bytes_per_instance` each. Each
  // slice is aligned to `alignment`; the returned stride (bytes_per_instance)
  // is rounded up so every slice base is aligned.
  Reservation reserve_device_scratch(std::size_t bytes_per_instance,
                                     std::size_t instances,
                                     std::size_t alignment = alignof(std::max_align_t)) {
    if (!is_power_of_two(alignment)) {
      throw std::invalid_argument(
          "reserve_device_scratch alignment must be a power of two");
    }
    if (instances == 0) {
      instances = 1;
    }
    if (bytes_per_instance == 0) {
      bytes_per_instance = 1;
    }

    const std::size_t stride = align_up(bytes_per_instance, alignment);
    if (stride < bytes_per_instance) { // align_up wrapped
      throw std::bad_alloc{};
    }
    if (stride > std::numeric_limits<std::size_t>::max() / instances) {
      throw std::bad_alloc{};
    }
    const std::size_t total = stride * instances;

    Reservation r;
    r.base = alloc_bytes(total, alignment);
    r.instances = instances;
    r.bytes_per_instance = stride;
    r.total_bytes = total;
    return r;
  }

 private:
  static constexpr bool host_accessible_ =
      memory_detail::host_accessible<memory_space>::value;

  enum class AllocationKind : std::uint8_t { SmallPool = 0, LargePool = 1 };

  // In-band header for host-accessible spaces. The layout is unchanged in size
  // and field alignment from the original small-only version: `kind` and its
  // padding occupy what used to be the `reserved` word. For SmallPool blocks
  // the fields keep their original meaning; for LargePool blocks `block_bytes`
  // holds the backend capacity and `block_start` holds the backend base
  // pointer (`idx` is unused).
  struct Header {
    std::uint32_t idx = 0;
    AllocationKind kind = AllocationKind::SmallPool;
    std::uint8_t pad[3] = {}; // explicit padding; kept for 8-byte field alignment
    std::size_t block_bytes = 0;
    std::byte *block_start = nullptr;
    std::uint64_t sentinel = 0;
  };

  struct FreeNode {
    FreeNode *next = nullptr;
  };

  struct BlockRecord {
    std::byte *block_start = nullptr;
    std::uint32_t idx = 0;
  };

  struct Slab {
    std::byte *base = nullptr;
    std::size_t bytes = 0;
    std::size_t offset = 0;
  };

  static constexpr std::uint64_t sentinel_ = 0x656D7070726F7473ull;

  // Power-of-two block sizes from 16 B (min_size) up to 2^max_size. The upper
  // exponent (PORTS_OF_CALL_MAX_SLAB_BLOCK_LOG2, defined near the top of this
  // header) is configurable so builds and tests can shrink the slab ceiling --
  // and therefore the size at which the large-request path engages.
  static constexpr std::uint32_t min_size = 4;
  static constexpr std::uint32_t max_size = PORTS_OF_CALL_MAX_SLAB_BLOCK_LOG2;
  static constexpr std::size_t num_blocks = std::size_t(max_size - min_size + 1);
  static_assert(max_size > min_size, "max slab block must exceed min block size");

  static_assert((std::size_t{1} << min_size) >= sizeof(FreeNode),
                "Minimum block size must be large enough to store FreeNode");

  mutable std::mutex mutex_;

  std::vector<Slab> slabs_;
  std::size_t current_slab_ = 0;

  // Free-list metadata is strategy-specific, and only one strategy is ever used
  // for a given memory space. Carrying both wastes per-instance memory, so we
  // select the relevant one at compile time via host_accessible_.

  // Host-accessible spaces embed a Header in each block and thread an intrusive
  // free list through the freed blocks themselves.
  struct HostMeta {
    std::array<FreeNode *, num_blocks> free_heads{};
  };

  // Non-host-accessible spaces cannot touch device memory from the host, so
  // free-block starts and live-allocation records are tracked host-side.
  struct DeviceMeta {
    std::array<std::vector<std::byte *>, num_blocks> free_blocks{};
    std::unordered_map<void *, BlockRecord> live_blocks;
  };

  std::conditional_t<host_accessible_, HostMeta, DeviceMeta> meta_;

  std::size_t min_slab_bytes_ = 8ull << 20;
  std::size_t next_slab_bytes_ = 8ull << 20;
  double growth_factor_ = 0.25;

  // ---- Large-request cache state (guarded by mutex_) ---------------------
  //
  // A complete backend allocation used for a single caller. `base_ptr` is the
  // pointer that must be returned to the backend; for host-accessible spaces
  // an in-band Header precedes the user pointer, so base_ptr differs from the
  // user pointer, and capacity_bytes excludes nothing -- it is the full
  // backend size.
  struct LargeBlock {
    void *base_ptr = nullptr;
    std::size_t capacity_bytes = 0;
    std::uint64_t last_used_epoch = 0;
  };

  // Free large blocks available for reuse, ordered by capacity so a request
  // can lower_bound() the smallest acceptable block.
  std::multimap<std::size_t, LargeBlock> free_large_blocks_;
  std::size_t cached_large_bytes_ = 0;
  std::uint64_t large_epoch_ = 0;

  // Active large allocations keyed by the user pointer. Maintained for every
  // memory space (host and device) so release_all() -- which also runs from
  // the Kokkos finalize hook -- can enumerate and free live large blocks that
  // never appear in slabs_.
  std::unordered_map<void *, LargeBlock> live_large_;

  bool any_allocation_ = false;
  bool shutting_down_ = false;

  LargeAllocationConfig large_config_;
  LargeAllocationStats large_stats_;

  static constexpr std::size_t overhead_bytes() noexcept {
    if constexpr (host_accessible_) {
      return sizeof(Header);
    }
    return 0;
  }

  static std::size_t align_up(std::size_t x, std::size_t a) {
    return (x + (a - 1)) & ~(a - 1);
  }

  static std::uintptr_t align_up_ptr(std::uintptr_t x, std::size_t a) {
    return (x + (a - 1)) & ~(std::uintptr_t(a - 1));
  }

  static constexpr std::size_t max_block_bytes() { return std::size_t(1) << max_size; }

  static bool is_power_of_two(std::size_t x) { return x != 0 && ((x & (x - 1)) == 0); }

  // Narrow wrappers over the memory-space backend. All backend calls in the
  // pool funnel through these so the strategy #ifdef lives in exactly one place
  // and the large-allocation path can share it with the slab path. Returns
  // nullptr on failure (caller decides how to react); never throws.
  static void *backend_allocate(std::size_t bytes) noexcept {
#ifdef PORTABILITY_STRATEGY_KOKKOS
    return Kokkos::kokkos_malloc<memory_space>(bytes);
#else
    return PORTABLE_MALLOC(bytes);
#endif
  }

  static void backend_deallocate(void *p) noexcept {
    if (p == nullptr) {
      return;
    }
#ifdef PORTABILITY_STRATEGY_KOKKOS
    Kokkos::kokkos_free<memory_space>(p);
#else
    PORTABLE_FREE(p);
#endif
  }

  // ---- Large-request path ------------------------------------------------

  // Round a required backend size up to the configured granularity, with
  // overflow checking. Returns 0 on overflow.
  std::size_t round_large_capacity(std::size_t need) const noexcept {
    const std::size_t g = large_config_.allocation_granularity_bytes;
    if (g <= 1) {
      return need;
    }
    if (need > std::numeric_limits<std::size_t>::max() - (g - 1)) {
      return 0;
    }
    return ((need + (g - 1)) / g) * g;
  }

  // A cached block of `capacity` is reusable for `need` bytes only if it is at
  // least `need` and not excessively larger than it.
  bool acceptable_oversize(std::size_t capacity, std::size_t need) const noexcept {
    if (capacity < need) {
      return false;
    }
    std::size_t relative_slack = 0;
    if (large_config_.max_oversize_denominator != 0) {
      // need * num / den without overflowing: divide first.
      relative_slack = (need / large_config_.max_oversize_denominator) *
                       large_config_.max_oversize_numerator;
    }
    const std::size_t allowed_slack =
        std::max(relative_slack, large_config_.min_oversize_bytes);
    return capacity - need <= allowed_slack;
  }

  void note_active_large_added(std::size_t capacity) noexcept {
    large_stats_.active_capacity_bytes += capacity;
    ++large_stats_.active_blocks;
    large_stats_.peak_active_capacity_bytes = std::max(
        large_stats_.peak_active_capacity_bytes, large_stats_.active_capacity_bytes);
  }

  void note_active_large_removed(std::size_t capacity) noexcept {
    large_stats_.active_capacity_bytes -= capacity;
    --large_stats_.active_blocks;
  }

  void note_cached_added(std::size_t capacity) noexcept {
    cached_large_bytes_ += capacity;
    large_stats_.cached_bytes = cached_large_bytes_;
    large_stats_.cached_blocks = free_large_blocks_.size();
    large_stats_.peak_cached_bytes =
        std::max(large_stats_.peak_cached_bytes, cached_large_bytes_);
  }

  void note_cached_removed(std::size_t capacity) noexcept {
    cached_large_bytes_ -= capacity;
    large_stats_.cached_bytes = cached_large_bytes_;
    large_stats_.cached_blocks = free_large_blocks_.size();
  }

  // Build the user pointer inside a large backend block and register active
  // metadata. Mirrors make_allocation()'s alignment/header logic. Must be
  // called with mutex_ held (device registry) but the host header write needs
  // no lock; we take the lock for both to keep the accounting consistent.
  void *activate_large_block(void *base, std::size_t capacity, std::size_t request,
                             std::size_t alignment) {
    const std::uintptr_t raw_user =
        reinterpret_cast<std::uintptr_t>(base) + overhead_bytes();
    const std::uintptr_t aligned_user = align_up_ptr(raw_user, alignment);
    auto *const user_ptr = reinterpret_cast<std::byte *>(aligned_user);

    [[maybe_unused]] const std::byte *const block_end =
        static_cast<std::byte *>(base) + capacity;
    [[maybe_unused]] const std::byte *const user_end = user_ptr + request;
    assert(user_end <= block_end);
    assert(reinterpret_cast<std::uintptr_t>(user_ptr) % alignment == 0);

    if constexpr (host_accessible_) {
      auto *const h = reinterpret_cast<Header *>(user_ptr - sizeof(Header));
      h->idx = 0;
      h->kind = AllocationKind::LargePool;
      h->block_bytes = capacity;                     // backend capacity
      h->block_start = static_cast<std::byte *>(base); // backend base pointer
      h->sentinel = sentinel_;
    }

    const auto &[it, inserted] = live_large_.emplace(
        static_cast<void *>(user_ptr), LargeBlock{base, capacity, 0});
    if (!inserted) {
      throw std::runtime_error("SlabArenaPool: duplicate large allocation pointer");
    }
    note_active_large_added(capacity);
    return static_cast<void *>(user_ptr);
  }

  void *alloc_large(std::size_t request, std::size_t alignment, std::size_t need) {
    const std::size_t required_capacity = round_large_capacity(need);
    if (required_capacity == 0) {
      throw std::bad_alloc{};
    }

    {
      std::lock_guard<std::mutex> lock(mutex_);
      any_allocation_ = true;
      ++large_stats_.requests;

      auto it = free_large_blocks_.lower_bound(required_capacity);
      if (it != free_large_blocks_.end() &&
          acceptable_oversize(it->second.capacity_bytes, required_capacity)) {
        LargeBlock block = it->second;
        free_large_blocks_.erase(it);
        note_cached_removed(block.capacity_bytes);
        ++large_stats_.cache_hits;
        return activate_large_block(block.base_ptr, block.capacity_bytes, request,
                                    alignment);
      }
      ++large_stats_.cache_misses;
    }

    // Cache miss: allocate from the backend outside the lock so a slow
    // multi-gigabyte call does not block small allocations. On failure, trim
    // the cache and retry once (cached blocks may be holding the memory we
    // need at a different capacity).
    void *base = backend_allocate(required_capacity);
    if (base == nullptr) {
      trim_large_cache();
      base = backend_allocate(required_capacity);
      if (base == nullptr) {
        throw std::bad_alloc{};
      }
      std::lock_guard<std::mutex> lock(mutex_);
      ++large_stats_.allocation_retries_succeeded;
    }

    std::lock_guard<std::mutex> lock(mutex_);
    ++large_stats_.backend_allocations;
    try {
      return activate_large_block(base, required_capacity, request, alignment);
    } catch (...) {
      backend_deallocate(base);
      throw;
    }
  }

  // Common tail for freeing a large block whose active registry entry has
  // already been removed (both host and device paths). Decides whether to
  // cache or return to the backend, and performs any backend calls outside the
  // lock.
  void free_large_after_unregister(std::byte *backend_base,
                                   std::size_t capacity) noexcept {
    std::vector<LargeBlock> victims;
    bool release_directly = false;
    {
      std::lock_guard<std::mutex> lock(mutex_);
      note_active_large_removed(capacity);

      release_directly = shutting_down_ || large_config_.max_cached_bytes == 0 ||
                         capacity > large_config_.max_single_cached_bytes;

      if (!release_directly) {
        // std::multimap::emplace can throw; if it does we fall back to a direct
        // backend free rather than leaking, preserving noexcept.
        try {
          free_large_blocks_.emplace(capacity,
                                     LargeBlock{backend_base, capacity, ++large_epoch_});
          note_cached_added(capacity);
          victims = collect_eviction_victims_locked();
        } catch (...) {
          release_directly = true;
        }
      }
    }

    if (release_directly) {
      ++large_stats_.direct_frees;
      ++large_stats_.backend_deallocations;
      backend_deallocate(backend_base);
    }

    release_large_blocks(victims);
  }

  // Select oldest-epoch victims until both cache limits are satisfied. Caller
  // holds mutex_. Returns blocks to free outside the lock.
  std::vector<LargeBlock> collect_eviction_victims_locked() {
    std::vector<LargeBlock> victims;
    while (cached_large_bytes_ > large_config_.max_cached_bytes ||
           free_large_blocks_.size() > large_config_.max_cached_blocks) {
      auto oldest = free_large_blocks_.begin();
      for (auto it = free_large_blocks_.begin(); it != free_large_blocks_.end(); ++it) {
        if (it->second.last_used_epoch < oldest->second.last_used_epoch) {
          oldest = it;
        }
      }
      victims.push_back(oldest->second);
      const std::size_t victim_capacity = oldest->second.capacity_bytes;
      free_large_blocks_.erase(oldest);
      note_cached_removed(victim_capacity);
      ++large_stats_.evictions;
    }
    return victims;
  }

  void release_large_blocks(const std::vector<LargeBlock> &blocks) noexcept {
    for (const LargeBlock &b : blocks) {
      ++large_stats_.backend_deallocations;
      backend_deallocate(b.base_ptr);
    }
  }

  static std::uint32_t get_block_index(std::size_t bytes_needed) {
    std::size_t block = std::size_t{1} << min_size;
    std::uint32_t idx = 0;

    while (block < bytes_needed && idx + min_size < max_size) {
      block <<= 1;
      ++idx;
    }

    return idx;
  }

  static std::size_t block_size(std::uint32_t idx) {
    return std::size_t(1) << (idx + min_size);
  }

  std::size_t remaining_in_current_slab() const {
    if (slabs_.empty()) {
      return 0;
    }
    const auto &s = slabs_[current_slab_];
    return s.bytes - s.offset;
  }

  void add_slab(std::size_t min_required_bytes) {
    const std::size_t slab_bytes =
        std::max(std::max(min_slab_bytes_, next_slab_bytes_), min_required_bytes);

    void *raw = backend_allocate(slab_bytes);
    if (!raw) {
      throw std::bad_alloc{};
    }

    try {
      slabs_.push_back(Slab{static_cast<std::byte *>(raw), slab_bytes, 0});
    } catch (...) {
      backend_deallocate(raw);
      throw;
    }
    current_slab_ = slabs_.size() - 1;

    next_slab_bytes_ = get_next_slab(slab_bytes);
  }

  std::size_t get_next_slab(std::size_t slab_bytes) const noexcept {
    if (growth_factor_ == 0.0) return slab_bytes;
    constexpr std::size_t max_size_t = std::numeric_limits<std::size_t>::max();
    constexpr std::size_t min_bump = std::size_t{64} << 10;

    if (max_size_t - slab_bytes < min_bump) return max_size_t;

    // the growth factor could be too big for size_t, so we do it in long double
    const long double new_size = static_cast<long double>(slab_bytes) *
                                 (1.0L + static_cast<long double>(growth_factor_));
    if (new_size >= static_cast<long double>(max_size_t)) return max_size_t;

    // make sure the next slab grows by at least 64 KiB
    return std::max(static_cast<std::size_t>(new_size), slab_bytes + min_bump);
  }

  void *make_allocation(std::byte *block_start, std::size_t block_bytes,
                        std::size_t request, std::size_t alignment, std::uint32_t idx) {
    assert(block_start != nullptr);
    assert(idx < num_blocks);
    assert(block_bytes == block_size(idx));

    const std::uintptr_t raw_user =
        reinterpret_cast<std::uintptr_t>(block_start) + overhead_bytes();
    const std::uintptr_t aligned_user = align_up_ptr(raw_user, alignment);

    auto *const user_ptr = reinterpret_cast<std::byte *>(aligned_user);
    [[maybe_unused]] const std::byte *const block_end = block_start + block_bytes;
    [[maybe_unused]] const std::byte *const user_end = user_ptr + request;
    assert(user_end <= block_end);
    assert(reinterpret_cast<std::uintptr_t>(user_ptr) % alignment == 0);

    // Keep an allocation record for when this is freed
    if constexpr (host_accessible_) {
      auto *const h = reinterpret_cast<Header *>(user_ptr - sizeof(Header));
      h->idx = idx;
      h->kind = AllocationKind::SmallPool;
      h->block_bytes = block_bytes;
      h->block_start = block_start;
      h->sentinel = sentinel_;
    } else {
      const auto &[it, inserted] = meta_.live_blocks.emplace(
          static_cast<void *>(user_ptr), BlockRecord{block_start, idx});
      if (!inserted) {
        throw std::runtime_error("SlabArenaPool: duplicate allocation pointer");
      }
    }

    return static_cast<void *>(user_ptr);
  }

  void *bump_slab(std::size_t block_bytes, std::size_t request, std::size_t alignment,
                  std::uint32_t idx) {
    Slab &slab = slabs_[current_slab_];
    assert(block_bytes <= slab.bytes - slab.offset);

    // Free-list capacity invariant (device strategy):
    //   For non-host-accessible spaces the free list lives in a host-side
    //   std::vector that must never allocate inside the noexcept free path
    //   (push_block). We guarantee capacity here, at bump time: every block
    //   that is ever bump-allocated from a slab reserves one free-list slot for
    //   itself up front. A block can only be freed after it was bump-allocated,
    //   so the number of live+freed blocks of a class never exceeds the number
    //   of reserve() calls made for that class -> push_block can always
    //   push_back without reallocating. The abort in push_block is a defensive
    //   backstop for this invariant, not an expected path.
    if constexpr (!host_accessible_) {
      auto &stack = meta_.free_blocks[idx];
      if (stack.capacity() == stack.size()) {
        stack.reserve(stack.size() + 1);
      }
    }
    const auto old_offset = slab.offset;

    std::byte *const block_start = slab.base + slab.offset;
    slab.offset += block_bytes;

    try {
      return make_allocation(block_start, block_bytes, request, alignment, idx);
    } catch (...) {
      slab.offset = old_offset;
      throw;
    }
    return nullptr;
  }

  void *pop_block(std::uint32_t idx, std::size_t request, std::size_t alignment) {
    assert(idx < num_blocks);
    if constexpr (host_accessible_) {
      FreeNode *head = meta_.free_heads[idx];
      if (!head) {
        return nullptr;
      }

      meta_.free_heads[idx] = head->next;

      auto *const block_start = reinterpret_cast<std::byte *>(head);

      return make_allocation(block_start, block_size(idx), request, alignment, idx);
    } else {
      auto &stack = meta_.free_blocks[idx];
      if (stack.empty()) return nullptr;
      std::byte *const block_start = stack.back();
      stack.pop_back();
      try {
        return make_allocation(block_start, block_size(idx), request, alignment, idx);
      } catch (...) {
        stack.push_back(block_start);
        throw;
      }
    }
    return nullptr;
  }

  void push_block(std::byte *block_start, std::uint32_t idx) noexcept {
    assert(block_start != nullptr);
    assert(idx < num_blocks);
    if constexpr (host_accessible_) {
      auto *node = reinterpret_cast<FreeNode *>(block_start);
      node->next = meta_.free_heads[idx];
      meta_.free_heads[idx] = node;
    } else {
      auto &stack = meta_.free_blocks[idx];
      // Capacity was reserved up front in bump_slab() (see invariant there);
      // this must never reallocate inside the noexcept free path.
      if (stack.size() == stack.capacity()) {
        PORTABLE_ALWAYS_ABORT("SlabArenaPool free-list capacity exceeded");
      }
      stack.push_back(block_start);
    }
  };
};

// A std::allocator compliant allocator
template <class T, class Pool>
class PoolAllocator {
   public:
    using value_type = T;
    using pointer = T *;
    using const_pointer = const T *;
    using size_type = std::size_t;
    using difference_type = std::ptrdiff_t;

    template <class U>
    struct rebind {
      using other = PoolAllocator<U, Pool>;
    };

    using propagate_on_container_copy_assignment = std::false_type;
    using propagate_on_container_move_assignment = std::true_type;
    using propagate_on_container_swap = std::true_type;
    using is_always_equal = std::false_type;

    PoolAllocator() noexcept : pool_(&Pool::instance()) {}

    explicit PoolAllocator(Pool &pool) noexcept : pool_(&pool) {}

    template <class U>
    PoolAllocator(const PoolAllocator<U, Pool> &other) noexcept : pool_(other.pool_) {}

    [[nodiscard]] T *allocate(size_type n) {
      if (n > max_size()) {
        throw std::bad_array_new_length{};
      }
      void *p = pool_->alloc_bytes(n * sizeof(T), alignof(T));
      return static_cast<T *>(p);
    }

    void deallocate(T *p, size_type = 0) noexcept {
      pool_->free_bytes(static_cast<void *>(p));
    }

    Pool *pool() const noexcept { return pool_; }

    [[nodiscard]] size_type max_size() const noexcept {
      return Pool::max_allocation_bytes(alignof(T)) / sizeof(T);
    }

    template <class U, class... Args>
    void construct(U *p, Args &&...args) {
      // this is placement new, it constructs the object from already allocated memory
      ::new (static_cast<void *>(p)) U(std::forward<Args>(args)...);
    }

    template <class U>
    void destroy(U *p) {
      p->~U();
    }

    template <class U>
    bool operator==(const PoolAllocator<U, Pool> &rhs) const noexcept {
      return pool_ == rhs.pool_;
    }

    template <class U>
    bool operator!=(const PoolAllocator<U, Pool> &rhs) const noexcept {
      return !(*this == rhs);
    }

   private:
    template <class, class>
    friend class PoolAllocator;

    Pool *pool_;
  };
} // namespace PortsOfCall
#endif // _PORTABLE_MEMORY_HPP_
