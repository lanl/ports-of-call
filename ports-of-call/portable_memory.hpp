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
#include <mutex>
#include <new>
#include <stdexcept>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <vector>

#include <ports-of-call/portable_errors.hpp>

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

  static SlabArenaPool &instance() {
    static SlabArenaPool pool;
    return pool;
  }

  explicit SlabArenaPool(std::size_t min_slab_bytes = 8ull << 20 /* 8 Mib */,
                         double growth_factor = 0.25) {
    configure(min_slab_bytes, growth_factor);
    free_heads_.fill(nullptr);
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

    if (overhead >= max_block_bytes()) {
      return 0;
    }

    return max_block_bytes() - overhead;
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
      throw std::bad_alloc{};
    }

    const std::uint32_t idx = get_block_index(need);
    const std::size_t block_bytes = block_size(idx);

    std::lock_guard<std::mutex> lock(mutex_);

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

    std::lock_guard<std::mutex> lock(mutex_);

    if constexpr (host_accessible_) {
      Header *h =
          reinterpret_cast<Header *>(static_cast<std::byte *>(p) - sizeof(Header));
      if ((h == nullptr) || (h->sentinel != sentinel_)) {
        PORTABLE_ALWAYS_ABORT("SlabArenaPool corrupted header");
        return;
      }

      if ((h->idx >= num_blocks) || (h->block_start == nullptr)) {
        PORTABLE_ALWAYS_ABORT("SlabArenaPool corrupted block metadata");
        return;
      }
      const std::uint32_t idx = h->idx;
      std::byte *const block_start = h->block_start;
      h->sentinel = 0;
      push_block(block_start, idx);
    } else {
      auto it = live_blocks_.find(p);
      if (it == live_blocks_.end()) {
        PORTABLE_ALWAYS_ABORT("SlabArenaPool called with unknown pointer");
        return;
      }
      std::byte *const block_start = it->second.block_start;
      const std::uint32_t idx = it->second.idx;
      live_blocks_.erase(it);
      push_block(block_start, idx);
    }
  }

  void release_all() {
    std::lock_guard<std::mutex> lock(mutex_);

    live_blocks_.clear();
    for (auto &stack : free_blocks_) {
      stack.clear();
    }
    free_heads_.fill(nullptr);

    for (auto &slab : slabs_) {
      if (slab.base != nullptr) {
#ifdef PORTABILITY_STRATEGY_KOKKOS
        Kokkos::kokkos_free<memory_space>(slab.base);
#else
        PORTABLE_FREE(slab.base);
#endif
      }
    }

    slabs_.clear();
    current_slab_ = 0;
    next_slab_bytes_ = min_slab_bytes_;
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

 private:
  static constexpr bool host_accessible_ =
      memory_detail::host_accessible<memory_space>::value;

  struct Header {
    std::uint32_t idx = 0;
    std::uint32_t reserved = 0;
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

  // Power-of-two block sizes from 16 B to 1 GiB.
  static constexpr std::uint32_t min_size = 4;
  static constexpr std::uint32_t max_size = 30;
  static constexpr std::size_t num_blocks = std::size_t(max_size - min_size + 1);

  static_assert((std::size_t{1} << min_size) >= sizeof(FreeNode),
                "Minimum block size must be large enough to store FreeNode");

  mutable std::mutex mutex_;

  std::vector<Slab> slabs_;
  std::size_t current_slab_ = 0;

  // Used for host accessible memory spaces
  std::array<FreeNode *, num_blocks> free_heads_{};

  // Used for non-host accessible memory spaces
  std::array<std::vector<std::byte *>, num_blocks> free_blocks_{};
  std::unordered_map<void *, BlockRecord> live_blocks_;

  std::size_t min_slab_bytes_ = 8ull << 20;
  std::size_t next_slab_bytes_ = 8ull << 20;
  double growth_factor_ = 0.25;

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

#ifdef PORTABILITY_STRATEGY_KOKKOS
    void *raw = Kokkos::kokkos_malloc<memory_space>(slab_bytes);
#else
    void *raw = PORTABLE_MALLOC(slab_bytes);
#endif
    if (!raw) {
      throw std::bad_alloc{};
    }

    try {
      slabs_.push_back(Slab{static_cast<std::byte *>(raw), slab_bytes, 0});
    } catch (...) {
#ifdef PORTABILITY_STRATEGY_KOKKOS
      Kokkos::kokkos_free<memory_space>(raw);
#else
      PORTABLE_FREE(raw);
#endif
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
    const std::byte *const block_end = block_start + block_bytes;
    const std::byte *const user_end = user_ptr + request;
    assert(user_end <= block_end);
    assert(reinterpret_cast<std::uintptr_t>(user_ptr) % alignment == 0);

    // Keep an allocation record for when this is freed
    if constexpr (host_accessible_) {
      auto *const h = reinterpret_cast<Header *>(user_ptr - sizeof(Header));
      h->idx = idx;
      h->reserved = 0;
      h->block_start = block_start;
      h->sentinel = sentinel_;
    } else {
      const auto &[it, inserted] = live_blocks_.emplace(static_cast<void *>(user_ptr),
                                                        BlockRecord{block_start, idx});
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

    if constexpr (!host_accessible_) {
      auto &stack = free_blocks_[idx];
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
      FreeNode *head = free_heads_[idx];
      if (!head) {
        return nullptr;
      }

      free_heads_[idx] = head->next;

      auto *const block_start = reinterpret_cast<std::byte *>(head);

      return make_allocation(block_start, block_size(idx), request, alignment, idx);
    } else {
      auto &stack = free_blocks_[idx];
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
      node->next = free_heads_[idx];
      free_heads_[idx] = node;
    } else {
      auto &stack = free_blocks_[idx];
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

  // Device compatible bump allocator
  struct BumpAllocator {
    std::byte *base = nullptr;
    std::size_t capacity = 0;
    std::size_t offset = 0;

    BumpAllocator() = default;

    PORTABLE_INLINE_FUNCTION
    BumpAllocator(void *p, std::size_t bytes)
        : base(static_cast<std::byte *>(p)), capacity(bytes), offset(0) {}

    PORTABLE_INLINE_FUNCTION
    std::size_t align_up(std::size_t x, std::size_t a) const {
      return (x + (a - 1)) & ~(a - 1);
    }

    PORTABLE_INLINE_FUNCTION
    void *alloc_bytes(std::size_t bytes, std::size_t alignment) {
      const std::size_t pos = align_up(offset, alignment);
      if (pos + bytes > capacity) {
        return nullptr;
      }
      void *p = base + pos;
      offset = pos + bytes;
      return p;
    }

    template <class T>
    PORTABLE_INLINE_FUNCTION T *allocate(std::size_t n) {
      return static_cast<T *>(alloc_bytes(n * sizeof(T), alignof(T)));
    }

    PORTABLE_INLINE_FUNCTION
    void reset() { offset = 0; }
  };
} // namespace PortsOfCall
#endif // _PORTABLE_MEMORY_HPP_
