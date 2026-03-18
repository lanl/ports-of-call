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
#include <cstddef>
#include <cstdint>
#include <limits>
#include <mutex>
#include <new>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <vector>

#include "ports-of-call/portable_errors.hpp"


namespace PortsOfCall {

template <class MemorySpace = void>
class SlabArenaPool {
public:
  using memory_space = MemorySpace;

  static SlabArenaPool& instance() {
    static SlabArenaPool pool;
    return pool;
  }

  explicit SlabArenaPool(std::size_t min_slab_bytes = 8ull << 20 /* 8 Mib */,
                         double growth_factor = 0.25)
      : min_slab_bytes_(std::max(min_slab_bytes, std::size_t(64 << 10))),
        next_slab_bytes_(min_slab_bytes_),
        growth_factor_(growth_factor) {
    free_heads_.fill(nullptr);
  }

  ~SlabArenaPool() { release_all(); }

  SlabArenaPool(const SlabArenaPool&) = delete;
  SlabArenaPool& operator=(const SlabArenaPool&) = delete;

  void* alloc_bytes(std::size_t request,
                    std::size_t alignment = alignof(std::max_align_t)) {
    if (request == 0) {
      request = 1;
    }

    alignment = std::max(alignment, alignof(std::max_align_t));

    const std::size_t need =
        request + sizeof(Header) + (alignment - 1);

    const std::uint32_t idx = get_block_index(need);
    const std::size_t block_bytes = block_size(idx);

    std::lock_guard<std::mutex> lock(mutex_);

    if (void* p = pop_block(idx)) {
      return p;
    }

    if (slabs_.empty() || remaining_in_current_slab() < block_bytes) {
      add_slab(block_bytes);
    }

    return bump_slab(block_bytes, request, alignment,
                                   idx);
  }

  void free_bytes(void* p) noexcept {
    if (p == nullptr) {
      return;
    }

    std::lock_guard<std::mutex> lock(mutex_);

    Header* h = reinterpret_cast<Header*>(static_cast<std::byte*>(p) - sizeof(Header));
    assert(h != nullptr);
    assert(h->sentinel == sentinel);
    push_block(p, h->idx);
  }

  void release_all() {
    std::lock_guard<std::mutex> lock(mutex_);

    for (auto& slab : slabs_) {
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
    free_heads_.fill(nullptr);
  }

  // Profiling functions for tuning slab sizes
  std::size_t slab_count() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return slabs_.size();
  }

  std::size_t total_slab_bytes() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::size_t total = 0;
    for (const auto& s : slabs_) {
      total += s.bytes;
    }
    return total;
  }

  std::size_t current_slab_remaining() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return remaining_in_current_slab();
  }

private:
  struct Header {
    std::uint32_t idx = 0;
    std::uint32_t reserved = 0;
    std::size_t block_bytes = 0;
    std::uint64_t sentinel = sentinel;
  };

  struct FreeNode {
    FreeNode* next = nullptr;
  };

  struct Slab {
    std::byte* base = nullptr;
    std::size_t bytes = 0;
    std::size_t offset = 0;
  };

  static constexpr std::uint64_t sentinel = 0x656D7070726F7473ull;

  // Power-of-two block sizes from 16 B to 1 GiB.
  static constexpr std::uint32_t min_size = 4;
  static constexpr std::uint32_t max_size = 30;
  static constexpr std::size_t num_blocks =
      std::size_t(max_size - min_size + 1);

  mutable std::mutex mutex_;

  std::vector<Slab> slabs_;
  std::size_t current_slab_ = 0;

  std::array<FreeNode*, num_blocks> free_heads_{};

  std::size_t min_slab_bytes_ = 8ull << 20;
  std::size_t next_slab_bytes_ = 8ull << 20;
  double growth_factor_ = 0.25;

private:

  static std::size_t align_up(std::size_t x, std::size_t a) {
    return (x + (a - 1)) & ~(a - 1);
  }

  static std::uintptr_t align_up_ptr(std::uintptr_t x, std::size_t a) {
    return (x + (a - 1)) & ~(std::uintptr_t(a - 1));
  }

  static std::uint32_t get_block_index(std::size_t bytes_needed) {
    std::size_t block = 16;
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
    const auto& s = slabs_[current_slab_];
    return s.bytes - s.offset;
  }

  void add_slab(std::size_t min_required_bytes) {
    const std::size_t slab_bytes =
        std::max(std::max(min_slab_bytes_, next_slab_bytes_),
                  min_required_bytes);

#ifdef PORTABILITY_STRATEGY_KOKKOS
    void* raw = Kokkos::kokkos_malloc<memory_space>(slab_bytes);
#else
    void* raw = PORTABLE_MALLOC(slab_bytes);
#endif
    if (!raw) {
      PORTABLE_ALWAYS_ABORT("Kokkos malloc failed\n");
    }

    try {
      slabs_.push_back(Slab{static_cast<std::byte*>(raw), slab_bytes, 0});
    } catch (...) {
#ifdef PORTABILITY_STRATEGY_KOKKOS
      Kokkos::kokkos_free<memory_space>(raw);
#else
      PORTABLE_FREE(raw);
#endif
      PORTABLE_ALWAYS_ABORT("Could not push_back to the slabs vector?");
    }
    current_slab_ = slabs_.size() - 1;

    const std::size_t bump =
        std::max(static_cast<std::size_t>(slab_bytes * growth_factor_),
                  static_cast<std::size_t>(64 << 10));
    next_slab_bytes_ = slab_bytes + bump;
  }

  void* bump_slab(std::size_t block_bytes,
                                std::size_t request,
                                std::size_t alignment,
                                std::uint32_t idx) {
    Slab& slab = slabs_[current_slab_];
    assert(slab.offset + block_bytes <= slab.bytes);

    std::byte* const block_start = slab.base + slab.offset;

    const std::uintptr_t raw_user =
        reinterpret_cast<std::uintptr_t>(block_start) + sizeof(Header);
    const std::uintptr_t aligned_user = align_up_ptr(raw_user, alignment);

    auto* const user_ptr = reinterpret_cast<std::byte*>(aligned_user);
    auto* const h = reinterpret_cast<Header*>(user_ptr - sizeof(Header));

    const std::byte* const block_end = block_start + block_bytes;
    const std::byte* const user_end = user_ptr + request;
    assert(user_end <= block_end);

    h->idx = idx;
    h->reserved = 0;
    h->block_bytes = block_bytes;
    h->sentinel = sentinel;

    slab.offset += block_bytes;
    return static_cast<void*>(user_ptr);
  }

  void* pop_block(std::uint32_t idx) {
    FreeNode* head = free_heads_[idx];
    if (!head) {
      return nullptr;
    }

    free_heads_[idx] = head->next;
    void* p = static_cast<void*>(head);

    Header* h = reinterpret_cast<Header*>(static_cast<std::byte*>(p) - sizeof(Header));
    assert(h != nullptr);
    assert(h->sentinel == sentinel);
    assert(h->idx = idx);

    return p;
  }

  void push_block(void* p, std::uint32_t idx) noexcept {
    auto* node = static_cast<FreeNode*>(p);
    node->next = free_heads_[idx];
    free_heads_[idx] = node;
  }
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
    return std::numeric_limits<size_type>::max() / sizeof(T);
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
  std::byte* base = nullptr;
  std::size_t capacity = 0;
  std::size_t offset = 0;

  BumpAllocator() = default;

  PORTABLE_INLINE_FUNCTION
  BumpAllocator(void* p, std::size_t bytes)
      : base(static_cast<std::byte*>(p)), capacity(bytes), offset(0) {}

  PORTABLE_INLINE_FUNCTION
  std::size_t align_up(std::size_t x, std::size_t a) const {
    return (x + (a - 1)) & ~(a - 1);
  }

  PORTABLE_INLINE_FUNCTION
  void* alloc_bytes(std::size_t bytes, std::size_t alignment) {
    const std::size_t pos = align_up(offset, alignment);
    if (pos + bytes > capacity) {
      return nullptr;
    }
    void* p = base + pos;
    offset = pos + bytes;
    return p;
  }

  template <class T>
  PORTABLE_INLINE_FUNCTION
  T* allocate(std::size_t n) {
    return static_cast<T*>(alloc_bytes(n * sizeof(T), alignof(T)));
  }

  PORTABLE_INLINE_FUNCTION
  void reset() { offset = 0; }
};
} // namespace PortsofCall
#endif // _PORTABLE_MEMORY_HPP_
