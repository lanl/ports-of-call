//========================================================================================
// (C) (or copyright) 2026. Triad National Security, LLC. All rights reserved.
//
// This program was produced under U.S. Government contract 89233218CNA000001 for Los
// Alamos National Laboratory (LANL), which is operated by Triad National Security, LLC
// for the U.S. Department of Energy/National Nuclear Security Administration. All rights
// in the program are reserved by Triad National Security, LLC, and the U.S. Department
// of Energy/National Nuclear Security Administration. The Government is granted for
// itself and others acting on its behalf a nonexclusive, paid-up, irrevocable worldwide
//  license in this material to reproduce, prepare derivative works, distribute copies to
//  the public, perform publicly and display publicly, and to permit others to do so.
//========================================================================================

// This file was created in part or in whole by generative AI

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cstddef>
#include <cstdint>
#include <new>
#include <type_traits>
#include <vector>

#include <ports-of-call/portable_memory.hpp>

namespace {

#ifdef PORTABILITY_STRATEGY_KOKKOS
using ExecSpace = Kokkos::DefaultExecutionSpace;
using DevMemSpace = ExecSpace::memory_space;
using HostMemSpace = Kokkos::HostSpace;
using DevPool = PortsOfCall::SlabArenaPool<DevMemSpace>;
using HostPool = PortsOfCall::SlabArenaPool<HostMemSpace>;
#else
using DevPool = PortsOfCall::SlabArenaPool<>;
using HostPool = PortsOfCall::SlabArenaPool<>;
#endif

struct alignas(64) Aligned64 {
  double x[8];
};

struct NonTrivial {
  static int live_count;

  int value;

  NonTrivial() : value(0) { ++live_count; }
  explicit NonTrivial(int v) : value(v) { ++live_count; }
  NonTrivial(const NonTrivial &other) : value(other.value) { ++live_count; }

  ~NonTrivial() { --live_count; }
};

using Catch::Approx;

int NonTrivial::live_count = 0;

template <class T>
bool is_aligned(T *p) {
  auto addr = reinterpret_cast<std::uintptr_t>(p);
  return (addr % alignof(T)) == 0;
}

bool is_aligned_bytes(void *p, std::size_t alignment) {
  auto addr = reinterpret_cast<std::uintptr_t>(p);
  return (addr % alignment) == 0;
}

} // namespace

TEST_CASE("SlabArenaPool basic byte allocation/free/reuse", "[pool][slab][bytes]") {
  HostPool pool(1 << 20);

  void *p1 = pool.alloc_bytes(1024, alignof(std::max_align_t));
  REQUIRE(p1 != nullptr);

  pool.free_bytes(p1);

  void *p2 = pool.alloc_bytes(1024, alignof(std::max_align_t));
  REQUIRE(p2 != nullptr);

  // Same-size request should normally reuse the same block.
  REQUIRE(p2 == p1);
}

TEST_CASE("SlabArenaPool returns aligned pointers", "[pool][slab][alignment]") {
  HostPool pool(1 << 20);

  void *p1 = pool.alloc_bytes(sizeof(double) * 17, alignof(double));
  REQUIRE(p1 != nullptr);
  REQUIRE(is_aligned_bytes(p1, alignof(double)));

  void *p2 = pool.alloc_bytes(sizeof(Aligned64), alignof(Aligned64));
  REQUIRE(p2 != nullptr);
  REQUIRE(is_aligned_bytes(p2, alignof(Aligned64)));
}

TEST_CASE("SlabArenaPool can allocate multiple blocks from one slab",
          "[pool][slab][carve]") {
  HostPool pool(1 << 20);

  const auto slabs_before = pool.slab_count();

  void *p1 = pool.alloc_bytes(4096, alignof(double));
  void *p2 = pool.alloc_bytes(4096, alignof(double));
  void *p3 = pool.alloc_bytes(4096, alignof(double));

  REQUIRE(p1 != nullptr);
  REQUIRE(p2 != nullptr);
  REQUIRE(p3 != nullptr);

  REQUIRE(p1 != p2);
  REQUIRE(p2 != p3);
  REQUIRE(p1 != p3);

  // With a 1 MiB slab, these small allocations should come from the same slab.
  REQUIRE(pool.slab_count() == std::max<std::size_t>(std::size_t{1}, slabs_before + 1));
}

TEST_CASE("SlabArenaPool grows by adding slabs when needed", "[pool][slab][growth]") {
  HostPool pool(64 << 10); // small slabs to force growth

  const auto slabs_before = pool.slab_count();

  // Large enough to likely require a new slab each time with this config.
  void *p1 = pool.alloc_bytes(96 << 10, alignof(double));
  REQUIRE(p1 != nullptr);

  const auto slabs_after_first = pool.slab_count();
  REQUIRE(slabs_after_first >= slabs_before + 1);

  void *p2 = pool.alloc_bytes(96 << 10, alignof(double));
  REQUIRE(p2 != nullptr);

  const auto slabs_after_second = pool.slab_count();
  REQUIRE(slabs_after_second >= slabs_after_first);
}

TEST_CASE("Freed same-size byte allocations are reused", "[pool][slab][freelist]") {
  HostPool pool(1 << 20);

  void *p1 = pool.alloc_bytes(8192, 64);
  void *p2 = pool.alloc_bytes(8192, 64);

  REQUIRE(p1 != nullptr);
  REQUIRE(p2 != nullptr);
  REQUIRE(p1 != p2);

  pool.free_bytes(p1);
  pool.free_bytes(p2);

  // LIFO freelist behavior is expected in this implementation.
  void *q1 = pool.alloc_bytes(8192, 64);
  void *q2 = pool.alloc_bytes(8192, 64);

  REQUIRE(q1 == p2);
  REQUIRE(q2 == p1);
}

TEST_CASE("Odd-size tail-fit path can be reused", "[pool][slab][odd]") {
  // Small slab and coarse classing make it easier to hit tail-fit behavior.
  HostPool pool(4096);

  // First allocation consumes some slab space.
  void *a = pool.alloc_bytes(1500, 16);
  REQUIRE(a != nullptr);

  // This request may not fit its rounded class in the remainder, but can fit as odd
  // tail-fit.
  void *odd1 = pool.alloc_bytes(600, 16);
  REQUIRE(odd1 != nullptr);

  pool.free_bytes(odd1);

  void *odd2 = pool.alloc_bytes(600, 16);
  REQUIRE(odd2 != nullptr);

  // If odd freelist reuse is working, this should usually be the same pointer.
  REQUIRE(odd2 == odd1);

  pool.free_bytes(a);
  pool.free_bytes(odd2);
}

TEST_CASE("release_all clears slabs and allows fresh reuse", "[pool][slab][release]") {
  HostPool pool(64 << 10);

  void *p1 = pool.alloc_bytes(8192, 16);
  void *p2 = pool.alloc_bytes(8192, 16);

  REQUIRE(p1 != nullptr);
  REQUIRE(p2 != nullptr);
  REQUIRE(pool.slab_count() >= 1);

  pool.release_all();

  REQUIRE(pool.slab_count() == 0);
  REQUIRE(pool.total_slab_bytes() == 0);

  void *p3 = pool.alloc_bytes(4096, 16);
  REQUIRE(p3 != nullptr);
  REQUIRE(pool.slab_count() >= 1);
}

TEST_CASE("PoolAllocator allocates typed storage with correct alignment",
          "[pool][allocator][typed]") {
  HostPool pool(1 << 20);
  PortsOfCall::PoolAllocator<double, HostPool> alloc(pool);

  double *p = alloc.allocate(128);
  REQUIRE(p != nullptr);
  REQUIRE(is_aligned(p));

  for (int i = 0; i < 128; ++i) {
    p[i] = 1.5 * i;
  }

  for (int i = 0; i < 128; ++i) {
    REQUIRE(p[i] == Approx(1.5 * i));
  }

  alloc.deallocate(p, 128);
}

TEST_CASE("PoolAllocator supports higher-alignment types",
          "[pool][allocator][alignment]") {
  HostPool pool(1 << 20);
  PortsOfCall::PoolAllocator<Aligned64, HostPool> alloc(pool);

  Aligned64 *p = alloc.allocate(4);
  REQUIRE(p != nullptr);
  REQUIRE(is_aligned(p));

  alloc.deallocate(p, 4);
}

TEST_CASE("PoolAllocator construct/destroy works for non-trivial types",
          "[pool][allocator][lifetime]") {
  HostPool pool(1 << 20);
  PortsOfCall::PoolAllocator<NonTrivial, HostPool> alloc(pool);

  REQUIRE(NonTrivial::live_count == 0);

  NonTrivial *p = alloc.allocate(3);
  REQUIRE(p != nullptr);

  alloc.construct(p + 0, 10);
  alloc.construct(p + 1, 20);
  alloc.construct(p + 2, 30);

  REQUIRE(NonTrivial::live_count == 3);
  REQUIRE(p[0].value == 10);
  REQUIRE(p[1].value == 20);
  REQUIRE(p[2].value == 30);

  alloc.destroy(p + 0);
  alloc.destroy(p + 1);
  alloc.destroy(p + 2);

  REQUIRE(NonTrivial::live_count == 0);

  alloc.deallocate(p, 3);
}

TEST_CASE("PoolAllocator rebind shares the same underlying pool",
          "[pool][allocator][rebind]") {
  HostPool pool(1 << 20);

  PortsOfCall::PoolAllocator<double, HostPool> a(pool);
  PortsOfCall::PoolAllocator<int, HostPool> b(a);

  REQUIRE(a.pool() == b.pool());

  double *pd = a.allocate(8);
  int *pi = b.allocate(16);

  REQUIRE(pd != nullptr);
  REQUIRE(pi != nullptr);

  b.deallocate(pi, 16);
  a.deallocate(pd, 8);
}

TEST_CASE("PoolAllocator deallocate can ignore count parameter",
          "[pool][allocator][deallocate]") {
  HostPool pool(1 << 20);
  PortsOfCall::PoolAllocator<double, HostPool> alloc(pool);

  double *p = alloc.allocate(32);
  REQUIRE(p != nullptr);

  // Your adapter allows defaulted n.
  alloc.deallocate(p);
}

TEST_CASE("Freed typed allocations can be reused through PoolAllocator",
          "[pool][allocator][reuse]") {
  HostPool pool(1 << 20);
  PortsOfCall::PoolAllocator<double, HostPool> alloc(pool);

  double *p1 = alloc.allocate(64);
  REQUIRE(p1 != nullptr);

  alloc.deallocate(p1, 64);

  double *p2 = alloc.allocate(64);
  REQUIRE(p2 != nullptr);

  REQUIRE(p2 == p1);

  alloc.deallocate(p2, 64);
}

TEST_CASE("SlabArenaPool release_all should reset next slab size",
          "[pool][SlabArenaPool][release_all][growth]") {
  HostPool pool;

  constexpr std::size_t min_slab = 64 << 10;

  pool.configure(min_slab, 1.0);

  void *p = pool.alloc_bytes(1, alignof(std::max_align_t));

  REQUIRE(pool.slab_count() == 1);
  REQUIRE(pool.total_slab_bytes() == min_slab);

  pool.free_bytes(p);
  pool.release_all();

  REQUIRE(pool.slab_count() == 0);
  REQUIRE(pool.total_slab_bytes() == 0);

  /*
    Expected behavior after release_all():
      The pool is empty, so the next allocation should start again from min_slab.

    Current behavior:
      next_slab_bytes_ was already grown to 128 KiB and release_all() does not
      reset it, so the next slab is larger than min_slab.
  */
  void *q = pool.alloc_bytes(1, alignof(std::max_align_t));

  REQUIRE(pool.slab_count() == 1);
  REQUIRE(pool.total_slab_bytes() == min_slab);

  pool.free_bytes(q);
}

TEST_CASE("SlabArenaPool should reject non-power-of-two alignments",
          "[pool][SlabArenaPool][alignment][validation]") {
  HostPool pool;
  pool.configure(64 << 10, 0.0);

  /*
    The implementation uses this style of alignment:

      (x + (a - 1)) & ~(a - 1)

    That only works when a is a power of two.

    Since alloc_bytes() is public and accepts a raw std::size_t alignment, it
    should either:
      1. reject non-power-of-two alignments, or
      2. implement general alignment correctly.

    This test encodes option 1.
  */
  REQUIRE_THROWS_AS(pool.alloc_bytes(16, 24), std::invalid_argument);
}

TEST_CASE("SlabArenaPool recomputes alignment when reusing freed blocks",
          "[pool][alignment][free-list][regression]") {
  HostPool pool;
  pool.configure(64 << 10, 0.0);

  std::vector<void *> ptrs;

  for (int i = 0; i < 64; ++i) {
    void *p = pool.alloc_bytes(80, 16);
    REQUIRE(is_aligned_bytes(p, 16));
    ptrs.push_back(p);
  }

  for (void *p : ptrs) {
    pool.free_bytes(p);
  }

  constexpr std::size_t alignments[] = {16, 32, 64, 128, 256};

  for (std::size_t alignment : alignments) {
    void *q = pool.alloc_bytes(1, alignment);
    REQUIRE(is_aligned_bytes(q, alignment));
    pool.free_bytes(q);
  }
}

TEST_CASE("SlabArenaPool reuses blocks while recomputing alignment",
          "[pool][alignment][free-list]") {
  HostPool pool;
  pool.configure(64 << 10, 0.0);

  std::vector<void *> candidates;

  /*
    Create several 16-byte-aligned allocations that fall into the same size class
    as the later 64-byte-aligned allocation.

    request=80, alignment=16 typically maps to the 128-byte size class.
    request=1, alignment=64 also typically maps to the 128-byte size class.
  */
  for (int i = 0; i < 16; ++i) {
    void *p = pool.alloc_bytes(80, 16);
    REQUIRE(is_aligned_bytes(p, 16));
    candidates.push_back(p);
  }

  /*
    Free all candidates. The free list now contains block starts for the same
    size class.
  */
  for (void *p : candidates) {
    pool.free_bytes(p);
  }

  /*
    Request stricter alignment from the same size class.

    Correct behavior:
      The allocator may reuse one of the same underlying blocks, but it must
      recompute the user pointer so the returned address is 64-byte aligned.

    Old buggy behavior:
      It could return an old 16-byte-aligned user pointer directly.
  */
  void *q = pool.alloc_bytes(1, 64);

  REQUIRE(is_aligned_bytes(q, 64));

  pool.free_bytes(q);
}
TEST_CASE(
    "SlabArenaPool recomputes user pointer when reusing block with stricter alignment",
    "[pool][free-list][alignment]") {
  HostPool pool;
  pool.configure(64 << 10, 0.0);

  void *p = pool.alloc_bytes(80, 16);
  REQUIRE(is_aligned_bytes(p, 16));

  pool.free_bytes(p);

  void *q = pool.alloc_bytes(1, 64);

  REQUIRE(is_aligned_bytes(q, 64));

  pool.free_bytes(q);
}

struct alignas(64) OverAligned64 {
  char bytes[64];
};

TEST_CASE("PoolAllocator returns correctly aligned storage for over-aligned types",
          "[pool][alignment]") {
  HostPool pool;
  pool.configure(64 << 10, 0.0);

  PortsOfCall::PoolAllocator<OverAligned64, HostPool> alloc(pool);

  OverAligned64 *p = alloc.allocate(1);

  REQUIRE(is_aligned_bytes(p, alignof(OverAligned64)));

  alloc.deallocate(p, 1);
}

TEST_CASE("PoolAllocator preserves over-alignment after free-list reuse",
          "[pool][alignment][free-list]") {
  HostPool pool;
  pool.configure(64 << 10, 0.0);

  PortsOfCall::PoolAllocator<std::byte, HostPool> byte_alloc(pool);

  void *p = byte_alloc.allocate(80);
  byte_alloc.deallocate(static_cast<std::byte *>(p), 80);

  PortsOfCall::PoolAllocator<OverAligned64, HostPool> aligned_alloc(pool);

  OverAligned64 *q = aligned_alloc.allocate(1);

  REQUIRE(is_aligned_bytes(q, alignof(OverAligned64)));

  aligned_alloc.deallocate(q, 1);
}
#include <catch2/catch_test_macros.hpp>

#include <limits>
#include <stdexcept>

TEST_CASE("SlabArenaPool rejects invalid growth factors", "[pool][growth-factor]") {

  REQUIRE_THROWS_AS(HostPool(64 << 10, -0.25), std::invalid_argument);

  REQUIRE_THROWS_AS(HostPool(64 << 10, std::numeric_limits<double>::infinity()),
                    std::invalid_argument);

  REQUIRE_THROWS_AS(HostPool(64 << 10, std::numeric_limits<double>::quiet_NaN()),
                    std::invalid_argument);

  HostPool pool;

  REQUIRE_THROWS_AS(pool.configure(64 << 10, -0.25), std::invalid_argument);

  REQUIRE_THROWS_AS(pool.configure(64 << 10, std::numeric_limits<double>::infinity()),
                    std::invalid_argument);

  REQUIRE_THROWS_AS(pool.configure(64 << 10, std::numeric_limits<double>::quiet_NaN()),
                    std::invalid_argument);
}

TEST_CASE("PoolAllocator max_size reflects SlabArenaPool maximum block size",
          "[pool][max_size]") {

  HostPool pool;
  PortsOfCall::PoolAllocator<std::byte, HostPool> alloc(pool);

  const std::size_t max_n = alloc.max_size();

  REQUIRE(max_n < (std::size_t{1} << 30));

  REQUIRE_THROWS_AS(alloc.allocate(max_n + 1), std::bad_array_new_length);
}

// ---------------------------------------------------------------------------
// Singleton pathway
//
// SlabArenaPool::instance() is a Meyers singleton: a single, lazily
// constructed pool per memory-space type. PoolAllocator's default constructor
// binds to that singleton. Because the singleton is process-global shared
// state that persists across Catch2 test cases, these tests normalize the
// pool with release_all() rather than assuming a pristine pool, and they
// always free what they allocate so later test cases start from a clean slate.
// ---------------------------------------------------------------------------

TEST_CASE("SlabArenaPool::instance returns the same object every call",
          "[pool][singleton]") {
  HostPool &a = HostPool::instance();
  HostPool &b = HostPool::instance();

  // Same underlying object: identical addresses.
  REQUIRE(&a == &b);

  // Mutating through one reference is visible through the other.
  a.release_all();
  REQUIRE(b.slab_count() == 0);

  void *p = a.alloc_bytes(4096, alignof(std::max_align_t));
  REQUIRE(p != nullptr);
  REQUIRE(b.slab_count() == a.slab_count());
  REQUIRE(b.slab_count() >= 1);

  a.free_bytes(p);
  a.release_all();
}

TEST_CASE("Default-constructed PoolAllocator binds to the singleton",
          "[pool][singleton][allocator]") {
  HostPool::instance().release_all();

  // A default-constructed allocator should reference the singleton, not some
  // other pool instance.
  PortsOfCall::PoolAllocator<double, HostPool> alloc;
  REQUIRE(alloc.pool() == &HostPool::instance());

  double *p = alloc.allocate(64);
  REQUIRE(p != nullptr);
  REQUIRE(is_aligned(p));

  // The allocation must have gone through the singleton.
  REQUIRE(HostPool::instance().slab_count() >= 1);

  alloc.deallocate(p, 64);
  HostPool::instance().release_all();
}

TEST_CASE("Independently default-constructed allocators share the singleton",
          "[pool][singleton][allocator]") {
  HostPool::instance().release_all();

  PortsOfCall::PoolAllocator<double, HostPool> a;
  PortsOfCall::PoolAllocator<int, HostPool> b;

  // Both bind to the same singleton, so they compare equal (is_always_equal is
  // false, so equality is decided by the pool pointer).
  REQUIRE(a.pool() == b.pool());
  REQUIRE(a.pool() == &HostPool::instance());
  REQUIRE(a == PortsOfCall::PoolAllocator<int, HostPool>(b));

  // Memory allocated through one can be freed through the other since they
  // share the same pool.
  double *pd = a.allocate(8);
  REQUIRE(pd != nullptr);

  PortsOfCall::PoolAllocator<double, HostPool> a2(b); // rebind from b
  a2.deallocate(pd, 8);

  HostPool::instance().release_all();
}

TEST_CASE("Singleton reuses freed blocks across allocator instances",
          "[pool][singleton][allocator][reuse]") {
  HostPool::instance().release_all();

  PortsOfCall::PoolAllocator<double, HostPool> a;
  double *p1 = a.allocate(64);
  REQUIRE(p1 != nullptr);
  a.deallocate(p1, 64);

  // A distinct, default-constructed allocator hits the same singleton free
  // list and should hand back the same block.
  PortsOfCall::PoolAllocator<double, HostPool> b;
  double *p2 = b.allocate(64);
  REQUIRE(p2 == p1);

  b.deallocate(p2, 64);
  HostPool::instance().release_all();
}

TEST_CASE("Singleton and local pools are independent instances",
          "[pool][singleton]") {
  HostPool::instance().release_all();

  // A stack-allocated pool must not be the singleton.
  HostPool local(1 << 20);
  REQUIRE(&local != &HostPool::instance());

  void *lp = local.alloc_bytes(4096, alignof(std::max_align_t));
  REQUIRE(lp != nullptr);
  REQUIRE(local.slab_count() >= 1);

  // The local allocation must not have touched the singleton.
  REQUIRE(HostPool::instance().slab_count() == 0);

  local.free_bytes(lp);
}

#ifdef PORTABILITY_STRATEGY_KOKKOS
TEST_CASE("device: pooled bytes can be wrapped in unmanaged View and written in kernel",
          "[pool][slab][device][view]") {
  DevPool pool(1 << 20);

  constexpr int N = 256;

  void *raw = pool.alloc_bytes(N * sizeof(double), alignof(double));
  REQUIRE(raw != nullptr);

  using view_type =
      Kokkos::View<double *, DevMemSpace, Kokkos::MemoryTraits<Kokkos::Unmanaged>>;
  view_type x(static_cast<double *>(raw), N);

  Kokkos::parallel_for(
      "fill_unmanaged_from_pool", Kokkos::RangePolicy<ExecSpace>(0, N),
      KOKKOS_LAMBDA(const int i) { x(i) = 2.0 * i + 1.0; });

  auto host = Kokkos::create_mirror_view_and_copy(Kokkos::HostSpace(), x);

  for (int i = 0; i < N; ++i) {
    REQUIRE(host(i) == Approx(2.0 * i + 1.0));
  }

  pool.free_bytes(raw);
}

TEST_CASE("device: pooled allocation honors alignment for device kernel use",
          "[pool][slab][device][alignment]") {
  DevPool pool(1 << 20);

  void *raw = pool.alloc_bytes(8 * sizeof(Aligned64), alignof(Aligned64));
  REQUIRE(raw != nullptr);

  using flag_view = Kokkos::View<int, DevMemSpace>;
  flag_view ok("ok");

  Kokkos::parallel_for(
      "check_alignment", Kokkos::RangePolicy<ExecSpace>(0, 1), KOKKOS_LAMBDA(const int) {
        auto *p = static_cast<Aligned64 *>(raw);
        auto addr = reinterpret_cast<std::uintptr_t>(p);
        ok() = (addr % alignof(Aligned64) == 0) ? 1 : 0;
      });

  auto ok_h = Kokkos::create_mirror_view_and_copy(Kokkos::HostSpace(), ok);
  REQUIRE(ok_h() == 1);

  pool.free_bytes(raw);
}

TEST_CASE("device: reserve_device_scratch gives one slice per logical instance",
          "[pool][slab][device][scratch]") {
  DevPool pool(1 << 20);

  DevPool::ScratchLayout layout;
  const int a_id = layout.add_array<double>(16);
  const int b_id = layout.add_array<int>(8);

  constexpr int instances = 64;
  auto reservation = pool.reserve_device_scratch(layout, instances, 64);
  REQUIRE(reservation.base != nullptr);
  REQUIRE(reservation.instances == static_cast<std::size_t>(instances));

  auto scratch = reservation.view();

  const std::size_t bytes_per_instance = reservation.bytes_per_instance;
  const std::size_t off_a = layout[a_id].offset;
  const std::size_t off_b = layout[b_id].offset;

  Kokkos::View<int *, DevMemSpace> check_a("check_a", instances);
  Kokkos::View<int *, DevMemSpace> check_b("check_b", instances);

  Kokkos::parallel_for(
      "use_reserved_scratch", Kokkos::RangePolicy<ExecSpace>(0, instances),
      KOKKOS_LAMBDA(const int i) {
        std::byte *base = scratch.data() + std::size_t(i) * bytes_per_instance;

        double *a = reinterpret_cast<double *>(base + off_a);
        int *b = reinterpret_cast<int *>(base + off_b);

        for (int j = 0; j < 16; ++j)
          a[j] = 1000.0 + i + j;
        for (int j = 0; j < 8; ++j)
          b[j] = 2000 + 10 * i + j;

        check_a(i) = (a[0] == 1000.0 + i) ? 1 : 0;
        check_b(i) = (b[7] == 2000 + 10 * i + 7) ? 1 : 0;
      });

  auto check_a_h = Kokkos::create_mirror_view_and_copy(Kokkos::HostSpace(), check_a);
  auto check_b_h = Kokkos::create_mirror_view_and_copy(Kokkos::HostSpace(), check_b);

  for (int i = 0; i < instances; ++i) {
    REQUIRE(check_a_h(i) == 1);
    REQUIRE(check_b_h(i) == 1);
  }

  pool.free_bytes(reservation.base);
}

TEST_CASE("device: DeviceBump allocates disjoint arrays inside each slice",
          "[pool][slab][device][bump]") {
  DevPool pool(1 << 20);

  constexpr int instances = 128;
  constexpr std::size_t bytes_per_instance = 32 * sizeof(double) + 16 * sizeof(int) + 256;

  auto reservation = pool.reserve_device_scratch(bytes_per_instance, instances, 64);
  REQUIRE(reservation.base != nullptr);

  auto scratch = reservation.view();

  Kokkos::View<int *, DevMemSpace> ok("ok", instances);

  Kokkos::parallel_for(
      "device_bump_alloc", Kokkos::RangePolicy<ExecSpace>(0, instances),
      KOKKOS_LAMBDA(const int i) {
        std::byte *base = scratch.data() + std::size_t(i) * bytes_per_instance;

        DevPool::DeviceBump arena(base, bytes_per_instance);

        double *a = arena.template alloc<double>(32);
        int *b = arena.template alloc<int>(16);

        int pass = 1;
        if (a == nullptr || b == nullptr) pass = 0;

        if (pass) {
          for (int j = 0; j < 32; ++j)
            a[j] = i + 0.5 * j;
          for (int j = 0; j < 16; ++j)
            b[j] = i + j;

          if (a[31] != i + 0.5 * 31) pass = 0;
          if (b[15] != i + 15) pass = 0;
        }

        ok(i) = pass;
      });

  auto ok_h = Kokkos::create_mirror_view_and_copy(Kokkos::HostSpace(), ok);

  for (int i = 0; i < instances; ++i) {
    REQUIRE(ok_h(i) == 1);
  }

  pool.free_bytes(reservation.base);
}

TEST_CASE("device: PoolAllocator-backed unmanaged View works from device",
          "[pool][allocator][device]") {
  DevPool pool(1 << 20);
  PortsOfCall::PoolAllocator<double, DevPool> alloc(pool);

  constexpr int N = 512;
  double *ptr = alloc.allocate(N);
  REQUIRE(ptr != nullptr);

  using view_type =
      Kokkos::View<double *, DevMemSpace, Kokkos::MemoryTraits<Kokkos::Unmanaged>>;
  view_type y(ptr, N);

  Kokkos::parallel_for(
      "allocator_unmanaged_view", Kokkos::RangePolicy<ExecSpace>(0, N),
      KOKKOS_LAMBDA(const int i) { y(i) = 3.0 * i; });

  auto y_h = Kokkos::create_mirror_view_and_copy(Kokkos::HostSpace(), y);

  for (int i = 0; i < N; ++i) {
    REQUIRE(y_h(i) == Approx(3.0 * i));
  }

  alloc.deallocate(ptr, N);
}

TEST_CASE("device: freeing and reallocating same byte size remains usable",
          "[pool][slab][device][reuse]") {
  DevPool pool(1 << 20);

  constexpr int N = 128;

  void *p1 = pool.alloc_bytes(N * sizeof(int), alignof(int));
  REQUIRE(p1 != nullptr);
  pool.free_bytes(p1);

  void *p2 = pool.alloc_bytes(N * sizeof(int), alignof(int));
  REQUIRE(p2 != nullptr);

  using view_type =
      Kokkos::View<int *, DevMemSpace, Kokkos::MemoryTraits<Kokkos::Unmanaged>>;
  view_type v(static_cast<int *>(p2), N);

  Kokkos::parallel_for(
      "reuse_device_write", Kokkos::RangePolicy<ExecSpace>(0, N),
      KOKKOS_LAMBDA(const int i) { v(i) = 7 * i; });

  auto v_h = Kokkos::create_mirror_view_and_copy(Kokkos::HostSpace(), v);

  for (int i = 0; i < N; ++i) {
    REQUIRE(v_h(i) == 7 * i);
  }

  pool.free_bytes(p2);
}
TEST_CASE("SlabArenaPool works with device space without host-touching device metadata",
          "[pool][kokkos][device-memory]") {

  DevPool pool;
  pool.configure(64 << 10, 0.0);

  void *p = pool.alloc_bytes(256, 64);

  REQUIRE(p != nullptr);
  REQUIRE(reinterpret_cast<std::uintptr_t>(p) % 64 == 0);

  auto *device_ptr = static_cast<unsigned char *>(p);

  Kokkos::parallel_for(
      "write pooled device memory", Kokkos::RangePolicy<ExecSpace>(0, 256),
      KOKKOS_LAMBDA(const int i) { device_ptr[i] = static_cast<unsigned char>(i); });

  Kokkos::fence();

  pool.free_bytes(p);
}

// ---------------------------------------------------------------------------
// Singleton pathway for the device (heterogeneous) memory space
//
// The device pool tracks live allocations in a host-side unordered_map keyed
// by the returned pointer (no host-touchable header lives in device memory),
// but the singleton semantics are identical: one shared pool per memory-space
// type, reachable through instance() and through default-constructed
// allocators. These tests normalize with release_all() to stay independent of
// ordering against other test cases that use the device singleton.
// ---------------------------------------------------------------------------

TEST_CASE("device: DevPool::instance returns the same object every call",
          "[pool][singleton][device]") {
  DevPool &a = DevPool::instance();
  DevPool &b = DevPool::instance();

  REQUIRE(&a == &b);

  a.release_all();
  REQUIRE(b.slab_count() == 0);

  void *p = a.alloc_bytes(4096, 64);
  REQUIRE(p != nullptr);
  REQUIRE(b.slab_count() == a.slab_count());
  REQUIRE(b.slab_count() >= 1);

  a.free_bytes(p);
  a.release_all();
}

TEST_CASE("device: host and device singletons are distinct pools",
          "[pool][singleton][device]") {
  HostPool::instance().release_all();
  DevPool::instance().release_all();

  // The two memory-space specializations are different types, so their
  // singletons are different objects with independent state.
  void *hp = HostPool::instance().alloc_bytes(4096, 64);
  REQUIRE(hp != nullptr);
  REQUIRE(HostPool::instance().slab_count() >= 1);

  // Allocating on the host singleton must not perturb the device singleton.
  REQUIRE(DevPool::instance().slab_count() == 0);

  HostPool::instance().free_bytes(hp);
  HostPool::instance().release_all();
}

TEST_CASE("device: default-constructed PoolAllocator binds to the device singleton",
          "[pool][singleton][device][allocator]") {
  DevPool::instance().release_all();

  PortsOfCall::PoolAllocator<double, DevPool> alloc;
  REQUIRE(alloc.pool() == &DevPool::instance());

  constexpr int N = 256;
  double *ptr = alloc.allocate(N);
  REQUIRE(ptr != nullptr);
  REQUIRE(DevPool::instance().slab_count() >= 1);

  // Write through the singleton-backed pointer from a device kernel to make
  // sure the memory is genuinely usable on the device.
  using view_type =
      Kokkos::View<double *, DevMemSpace, Kokkos::MemoryTraits<Kokkos::Unmanaged>>;
  view_type v(ptr, N);

  Kokkos::parallel_for(
      "singleton_device_write", Kokkos::RangePolicy<ExecSpace>(0, N),
      KOKKOS_LAMBDA(const int i) { v(i) = 4.0 * i; });

  auto v_h = Kokkos::create_mirror_view_and_copy(Kokkos::HostSpace(), v);
  for (int i = 0; i < N; ++i) {
    REQUIRE(v_h(i) == Approx(4.0 * i));
  }

  alloc.deallocate(ptr, N);
  DevPool::instance().release_all();
}

TEST_CASE("device: singleton reuses freed blocks across allocator instances",
          "[pool][singleton][device][reuse]") {
  DevPool::instance().release_all();

  PortsOfCall::PoolAllocator<int, DevPool> a;
  constexpr int N = 128;
  int *p1 = a.allocate(N);
  REQUIRE(p1 != nullptr);
  a.deallocate(p1, N);

  // A distinct default-constructed allocator shares the singleton's free list
  // (tracked host-side for the non-host-accessible space) and reuses the block.
  PortsOfCall::PoolAllocator<int, DevPool> b;
  int *p2 = b.allocate(N);
  REQUIRE(p2 == p1);

  b.deallocate(p2, N);
  DevPool::instance().release_all();
}
#endif
