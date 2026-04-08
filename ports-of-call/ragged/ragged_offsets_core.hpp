#pragma once
// ragged_offsets_core_min_v1.hpp
//
// Minimal, low-memory ragged-ND builder core (offsets-only).
//
// Public surface is provided by backend wrappers (std / thrust / kokkos).
// This core contains:
//   * small portable span
//   * lazy offsets-based views (no span-of-spans materialization)
//   * count/fill proxies with vector-like API: resize / operator[] / emplace_back / push_back
//   * only_fill(row, lambda) helper (compile-time eliminated in count phase)
//
// Assumptions / simplifications vs earlier versions:
//   * body is required to be invocable as: body(std::size_t i, Row& row)
//   * no exec-first overloads; wrappers may choose their own convenience overloads.
//   * only a single public builder name is expected: build_nd(...)
//
// Backend concept required by build_nd_impl:
//   struct Backend {
//     template<class U> using vector = ...;
//     static U* raw_ptr(vector<U>&);
//     static const U* raw_ptr(const vector<U>&);
//     static void fill(vector<U>&, const U& value, Exec exec);
//     static void inclusive_scan_inplace(vector<U>&, size_t start, Exec exec); // start is always 1 in this core
//     static void for_each_index(size_t n, Functor f, Exec exec);              // calls f(size_t)
//     static U read_back(const vector<U>&, size_t idx, Exec exec);
//   };
//
// Portability macros:
//   PORTABLE_INLINE_FUNCTION, PORTABLE_LAMBDA, PORTABLE_ABORT are used.

#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <type_traits>
#include <utility>

namespace ragged_detail {

// ============================================================
// Small portable span
// ============================================================

template <class T>
struct span {
  using element_type = T;
  using value_type = std::remove_cv_t<T>;
  using size_type = std::size_t;
  using difference_type = std::ptrdiff_t;
  using pointer = T*;
  using reference = T&;

  pointer p = nullptr;
  size_type n = 0;

  PORTABLE_INLINE_FUNCTION constexpr span() noexcept = default;
  PORTABLE_INLINE_FUNCTION constexpr span(pointer ptr, size_type len) noexcept : p(ptr), n(len) {}

  PORTABLE_INLINE_FUNCTION constexpr pointer data() const noexcept { return p; }
  PORTABLE_INLINE_FUNCTION constexpr size_type size() const noexcept { return n; }
  PORTABLE_INLINE_FUNCTION constexpr size_type extent(const int d) const noexcept { return (d == 0) ? n : 0u; }

  PORTABLE_INLINE_FUNCTION constexpr reference operator[](size_type i) const noexcept { return p[i]; }
  PORTABLE_INLINE_FUNCTION constexpr pointer begin() const noexcept { return p; }
  PORTABLE_INLINE_FUNCTION constexpr pointer end() const noexcept { return p + n; }
};

// ============================================================
// only_fill(row, lambda)
// ============================================================

template <class R>
struct LazyValue {
  R value;
  PORTABLE_INLINE_FUNCTION operator R() const { return value; }
};

struct NoOpValue {
  template <class R>
  PORTABLE_INLINE_FUNCTION operator R() const { return R{}; }
};

template <class Row, class F>
PORTABLE_INLINE_FUNCTION auto only_fill(const Row& /*row*/, F&& f) {
  // Requires Row to have: static constexpr bool is_fill_phase;
  if constexpr (std::decay_t<Row>::is_fill_phase) {
    using R = decltype(f());
    return LazyValue<R>{ f() };
  } else {
    return NoOpValue{};
  }
}

// ============================================================
// Pointer table (device-friendly) for offsets pointers
// ============================================================

template <class P, int N>
struct ptr_table {
  P p[N]{};
  PORTABLE_INLINE_FUNCTION constexpr const P* data() const noexcept { return p; }
  PORTABLE_INLINE_FUNCTION constexpr P* data() noexcept { return p; }
};

// ============================================================
// Shared utilities
// ============================================================

struct NoOpElem {
  template <class U>
  PORTABLE_INLINE_FUNCTION constexpr NoOpElem& operator=(const U&) noexcept { return *this; }
};

template <class T>
struct dependent_false : std::false_type {};

// ============================================================
// Lazy offsets-based access views (offsets-only)
// ============================================================

template <class T, int DepthDims, int CurDepth, class Index, class ValuePtr>
struct OffsetsSubView;

template <class T, int DepthDims, class Index, class ValuePtr>
struct OffsetsRootView {
  static_assert(DepthDims >= 2);
  static_assert(std::is_integral_v<Index> && std::is_unsigned_v<Index>, "Index must be an unsigned integral type.");
  static_assert(std::is_pointer_v<ValuePtr>, "ValuePtr must be a pointer type.");
  static_assert(std::is_same_v<std::remove_cv_t<std::remove_pointer_t<ValuePtr>>, T>,
                "ValuePtr must be a pointer to T (or const T).");

  ptr_table<const Index*, DepthDims> off{}; // off.p[d] valid for d=1..DepthDims-1
  ValuePtr values = nullptr;
  std::size_t n_root = 0;

  PORTABLE_INLINE_FUNCTION
  std::size_t extent(const int d) const noexcept { return (d == 0) ? n_root : 0u; }

  PORTABLE_INLINE_FUNCTION
  auto operator[](const std::size_t i) const noexcept {
    return OffsetsSubView<T, DepthDims, 1, Index, ValuePtr>(off, values, static_cast<Index>(i));
  }
};

template <class T, int DepthDims, int CurDepth, class Index, class ValuePtr>
struct OffsetsSubView {
  static_assert(DepthDims >= 2);
  static_assert(1 <= CurDepth && CurDepth < DepthDims);
  static_assert(std::is_integral_v<Index> && std::is_unsigned_v<Index>, "Index must be an unsigned integral type.");
  static_assert(std::is_pointer_v<ValuePtr>, "ValuePtr must be a pointer type.");
  static_assert(std::is_same_v<std::remove_cv_t<std::remove_pointer_t<ValuePtr>>, T>,
                "ValuePtr must be a pointer to T (or const T).");

  ptr_table<const Index*, DepthDims> off{};
  ValuePtr values = nullptr;
  Index node_id = 0;

  PORTABLE_INLINE_FUNCTION constexpr OffsetsSubView() noexcept = default;
  PORTABLE_INLINE_FUNCTION constexpr OffsetsSubView(ptr_table<const Index*, DepthDims> o, ValuePtr v, Index id) noexcept
    : off(o), values(v), node_id(id) {}

  PORTABLE_INLINE_FUNCTION
  std::size_t extent(const int d) const noexcept {
    if (d != 0) return 0u;
    const std::size_t b = static_cast<std::size_t>(off.p[CurDepth][node_id]);
    const std::size_t e = static_cast<std::size_t>(off.p[CurDepth][node_id + 1]);
    return e - b;
  }

  template <int D = CurDepth, typename std::enable_if_t<(D < DepthDims - 1), int> = 0>
  PORTABLE_INLINE_FUNCTION
  auto operator[](const std::size_t idx) const noexcept {
    const Index child = static_cast<Index>(off.p[CurDepth][node_id] + static_cast<Index>(idx));
    return OffsetsSubView<T, DepthDims, CurDepth + 1, Index, ValuePtr>(off, values, child);
  }

  template <int D = CurDepth, typename std::enable_if_t<(D == DepthDims - 1), int> = 0>
  PORTABLE_INLINE_FUNCTION
  std::remove_pointer_t<ValuePtr>& operator[](const std::size_t idx) const noexcept {
    const std::size_t start = static_cast<std::size_t>(off.p[CurDepth][node_id]);
    return values[start + idx];
  }

  // Leaf-only contiguous pointer
  template <int D = CurDepth, typename std::enable_if_t<(D == DepthDims - 1), int> = 0>
  PORTABLE_INLINE_FUNCTION
  ValuePtr data() const noexcept {
    return values + static_cast<std::size_t>(off.p[CurDepth][node_id]);
  }

  // Leaf-only contiguous span
  template <int D = CurDepth, typename std::enable_if_t<(D == DepthDims - 1), int> = 0>
  PORTABLE_INLINE_FUNCTION
  span<std::remove_pointer_t<ValuePtr>> leaf_span() const noexcept {
    return span<std::remove_pointer_t<ValuePtr>>(data(), extent(0));
  }
};

// ============================================================
// Count + fill proxies (body(i, row))
// ============================================================

// 1D count/fill rows ---------------------------------------------

template <class T>
struct RowCounter1D {
  static constexpr bool is_fill_phase = false;
  std::size_t c = 0;

  PORTABLE_INLINE_FUNCTION void resize(std::size_t n) noexcept { c = n; } // last wins
  PORTABLE_INLINE_FUNCTION void push_back(const T&) noexcept { ++c; }
  PORTABLE_INLINE_FUNCTION NoOpElem operator[](std::size_t) const noexcept { return {}; }
};

template <class T, class ValuePtr>
struct RowFiller1DPtr {
  static constexpr bool is_fill_phase = true;

  static_assert(std::is_pointer_v<ValuePtr>, "ValuePtr must be a pointer type.");
  static_assert(std::is_same_v<std::remove_cv_t<std::remove_pointer_t<ValuePtr>>, T>,
                "ValuePtr must be a pointer to T (or const T).");

  ValuePtr p = nullptr;
  std::size_t k = 0;

  PORTABLE_INLINE_FUNCTION void resize(std::size_t n) noexcept { k = n; }
  PORTABLE_INLINE_FUNCTION void push_back(const T& v) noexcept { p[k++] = v; }
  PORTABLE_INLINE_FUNCTION std::remove_pointer_t<ValuePtr>& operator[](std::size_t i) const noexcept { return p[i]; }
};

// ND count proxy ---------------------------------------------------

template <class T, int DepthDims, int TargetDepth, int CurDepth, class Index>
struct CountProxy {
  static constexpr bool is_fill_phase = false;

  static_assert(DepthDims >= 2);
  static_assert(1 <= TargetDepth && TargetDepth <= DepthDims - 1);
  static_assert(1 <= CurDepth && CurDepth <= DepthDims - 1);
  static_assert(std::is_integral_v<Index> && std::is_unsigned_v<Index>, "Index must be an unsigned integral type.");

  Index* counts = nullptr;             // counts for TargetDepth
  const Index* const* off = nullptr;   // offsets pointers for depths < TargetDepth
  Index node_id = 0;
  Index cursor = 0; // append cursor (after resize)

  PORTABLE_INLINE_FUNCTION
  constexpr CountProxy(Index* c, const Index* const* o, Index id) noexcept : counts(c), off(o), node_id(id) {}

  PORTABLE_INLINE_FUNCTION
  void resize(const std::size_t n_) noexcept {
    const Index maxv = (std::numeric_limits<Index>::max)();
    const std::size_t imax = static_cast<std::size_t>(maxv);
    const Index n = static_cast<Index>(n_ > imax ? imax : n_);
    cursor = n;
    if constexpr (CurDepth == TargetDepth) counts[node_id] = n;
  }

  template <int D = CurDepth, typename std::enable_if_t<(D < DepthDims - 1), int> = 0>
  PORTABLE_INLINE_FUNCTION
  auto emplace_back() noexcept {
    const Index idx = cursor++;
    if constexpr (CurDepth == TargetDepth) {
      counts[node_id] = static_cast<Index>(counts[node_id] + 1);
      return CountProxy<T, DepthDims, TargetDepth, CurDepth + 1, Index>(counts, off, 0);
    } else if constexpr (CurDepth < TargetDepth) {
      const Index child = static_cast<Index>(off[CurDepth][node_id] + idx);
      return CountProxy<T, DepthDims, TargetDepth, CurDepth + 1, Index>(counts, off, child);
    } else {
      return CountProxy<T, DepthDims, TargetDepth, CurDepth + 1, Index>(counts, off, 0);
    }
  }

  template <int D = CurDepth, typename std::enable_if_t<(D < DepthDims - 1), int> = 0>
  PORTABLE_INLINE_FUNCTION
  auto operator[](const std::size_t idx_) const noexcept {
    const Index idx = static_cast<Index>(idx_);
    if constexpr (CurDepth < TargetDepth) {
      const Index child = static_cast<Index>(off[CurDepth][node_id] + idx);
      return CountProxy<T, DepthDims, TargetDepth, CurDepth + 1, Index>(counts, off, child);
    } else {
      return CountProxy<T, DepthDims, TargetDepth, CurDepth + 1, Index>(counts, off, 0);
    }
  }

  template <int D = CurDepth, typename std::enable_if_t<(D == DepthDims - 1), int> = 0>
  PORTABLE_INLINE_FUNCTION
  NoOpElem operator[](std::size_t) const noexcept { return {}; }

  // leaf-only push_back: counts only when TargetDepth is the leaf depth
  template <int D = CurDepth, typename std::enable_if_t<(D == DepthDims - 1), int> = 0>
  PORTABLE_INLINE_FUNCTION
  void push_back(const T&) noexcept {
    if constexpr (TargetDepth == DepthDims - 1) {
      counts[node_id] = static_cast<Index>(counts[node_id] + 1);
      cursor = static_cast<Index>(cursor + 1);
    }
  }

  template <int D = CurDepth, typename std::enable_if_t<(D < DepthDims - 1), int> = 0>
  PORTABLE_INLINE_FUNCTION
  void push_back(const T&) noexcept {
    static_assert(dependent_false<T>::value,
                  "ragged count phase: push_back(value) is only valid at the leaf container depth.");
  }
};

// ND fill proxy -----------------------------------------------------

template <class T, int DepthDims, int CurDepth, class Index, class ValuePtr>
struct FillProxy {
  static constexpr bool is_fill_phase = true;

  static_assert(DepthDims >= 2);
  static_assert(1 <= CurDepth && CurDepth <= DepthDims - 1);
  static_assert(std::is_integral_v<Index> && std::is_unsigned_v<Index>, "Index must be an unsigned integral type.");
  static_assert(std::is_pointer_v<ValuePtr>, "ValuePtr must be a pointer type.");
  static_assert(std::is_same_v<std::remove_cv_t<std::remove_pointer_t<ValuePtr>>, T>,
                "ValuePtr must be a pointer to T (or const T).");

  const Index* const* off = nullptr;
  ValuePtr values = nullptr;
  Index node_id = 0;
  Index cursor = 0;

  PORTABLE_INLINE_FUNCTION
  constexpr FillProxy(const Index* const* o, ValuePtr v, Index id) noexcept : off(o), values(v), node_id(id) {}

  PORTABLE_INLINE_FUNCTION
  std::size_t extent(const int d) const noexcept {
    if (d != 0) return 0u;
    const std::size_t b = static_cast<std::size_t>(off[CurDepth][node_id]);
    const std::size_t e = static_cast<std::size_t>(off[CurDepth][node_id + 1]);
    return e - b;
  }

  PORTABLE_INLINE_FUNCTION
  void resize(const std::size_t n) noexcept { cursor = static_cast<Index>(n); }

  template <int D = CurDepth, typename std::enable_if_t<(D < DepthDims - 1), int> = 0>
  PORTABLE_INLINE_FUNCTION
  auto emplace_back() noexcept {
    const Index child = static_cast<Index>(off[CurDepth][node_id] + cursor++);
    return FillProxy<T, DepthDims, CurDepth + 1, Index, ValuePtr>(off, values, child);
  }

  template <int D = CurDepth, typename std::enable_if_t<(D == DepthDims - 1), int> = 0>
  PORTABLE_INLINE_FUNCTION
  void push_back(const T& v) noexcept {
    const std::size_t start = static_cast<std::size_t>(off[CurDepth][node_id]);
    values[start + static_cast<std::size_t>(cursor++)] = v;
  }

  template <int D = CurDepth, typename std::enable_if_t<(D < DepthDims - 1), int> = 0>
  PORTABLE_INLINE_FUNCTION
  void push_back(const T&) noexcept {
    static_assert(dependent_false<T>::value,
                  "ragged fill phase: push_back(value) is only valid at the leaf container depth.");
  }

  template <int D = CurDepth, typename std::enable_if_t<(D < DepthDims - 1), int> = 0>
  PORTABLE_INLINE_FUNCTION
  auto operator[](const std::size_t idx) const noexcept {
    const Index child = static_cast<Index>(off[CurDepth][node_id] + static_cast<Index>(idx));
    return FillProxy<T, DepthDims, CurDepth + 1, Index, ValuePtr>(off, values, child);
  }

  template <int D = CurDepth, typename std::enable_if_t<(D == DepthDims - 1), int> = 0>
  PORTABLE_INLINE_FUNCTION
  std::remove_pointer_t<ValuePtr>& operator[](const std::size_t idx) const noexcept {
    const std::size_t start = static_cast<std::size_t>(off[CurDepth][node_id]);
    return values[start + idx];
  }
};

// ============================================================
// Owning container: contiguous values + offsets-only root view
// ============================================================

template <class Backend, class T, int DepthDims, class Index>
struct ContiguousRaggedOffsets {
  static_assert(DepthDims >= 1);
  static_assert(std::is_integral_v<Index> && std::is_unsigned_v<Index>, "Index must be an unsigned integral type.");

  template <class U>
  using vec_t = typename Backend::template vector<U>;

  using value_type = T;
  using index_type = Index;

  // offsets[d] valid for d=1..DepthDims-1 when DepthDims>=2. offsets[0] unused.
  std::array< vec_t<Index>, (DepthDims >= 2 ? DepthDims : 1) > offsets{};
  vec_t<T> values{};

  std::size_t n_root = 0;
  std::size_t total_leaves = 0;

  using root_view_t = std::conditional_t<
    (DepthDims == 1),
    span<T>,
    OffsetsRootView<T, DepthDims, Index, T*>
  >;

  root_view_t root{};

  void destroy() {
    values = vec_t<T>();
    for (auto& o : offsets) o = vec_t<Index>();
    root = root_view_t();
    n_root = 0;
    total_leaves = 0;
  }
};

// ============================================================
// Internal: build offsets (count + inclusive_scan) per depth
// ============================================================

template <class Backend, class T, int DepthDims, class Index, int TargetDepth, class Body, class Exec>
inline void count_and_scan_depth(
  std::size_t n_root,
  const std::array<typename Backend::template vector<Index>, (DepthDims >= 2 ? DepthDims : 1)>& offsets_prev,
  std::size_t n_nodes_at_target,
  typename Backend::template vector<Index>& out_offsets,
  std::size_t& out_total,
  const Body& body,
  Exec exec
) {
  static_assert(DepthDims >= 2);
  static_assert(1 <= TargetDepth && TargetDepth <= DepthDims - 1);

  if (n_nodes_at_target == 0) {
    out_offsets.resize(1);
    Backend::fill(out_offsets, Index(0), exec);
    out_total = 0;
    return;
  }

  out_offsets.resize(n_nodes_at_target + 1);
  Backend::fill(out_offsets, Index(0), exec); // counts start at 0 (also sets out_offsets[0]=0)

  Index* counts_p = Backend::raw_ptr(out_offsets) + 1; // counts live in out_offsets(i+1)

  ptr_table<const Index*, DepthDims> off_ptrs{};
  for (int d = 0; d < DepthDims; ++d) off_ptrs.p[d] = nullptr;
  for (int d = 1; d <= TargetDepth - 1; ++d) off_ptrs.p[d] = Backend::raw_ptr(offsets_prev[d]);

  // Capture body by value into the kernel (device correctness).
  const Body body_c = body;

  struct Kernel {
    Index* counts;
    ptr_table<const Index*, DepthDims> off;
    Body body;

    PORTABLE_INLINE_FUNCTION void operator()(std::size_t i) const {
      CountProxy<T, DepthDims, TargetDepth, 1, Index> row(counts, off.data(), static_cast<Index>(i));
      body(i, row);
    }
  };

  Backend::for_each_index(n_root, Kernel{counts_p, off_ptrs, body_c}, exec);

  Backend::inclusive_scan_inplace(out_offsets, 1, exec);

  const Index last = Backend::read_back(out_offsets, n_nodes_at_target, exec);
  out_total = static_cast<std::size_t>(last);
}

template <class Backend, class T, int DepthDims, class Index, int TargetDepth, class Body, class Exec>
struct BuildDepth {
  static void run(
    std::size_t n_root,
    std::array<typename Backend::template vector<Index>, (DepthDims >= 2 ? DepthDims : 1)>& offsets,
    std::array<std::size_t, DepthDims + 1>& n_nodes,
    std::size_t& total_leaves,
    const Body& body,
    Exec exec
  ) {
    static_assert(DepthDims >= 2);

    const std::size_t n_t = n_nodes[TargetDepth];

    std::size_t total_next = 0;
    count_and_scan_depth<Backend, T, DepthDims, Index, TargetDepth>(
      n_root, offsets, n_t, offsets[TargetDepth], total_next, body, exec
    );

    if constexpr (TargetDepth < DepthDims - 1) {
      n_nodes[TargetDepth + 1] = total_next;
      BuildDepth<Backend, T, DepthDims, Index, TargetDepth + 1, Body, Exec>::run(
        n_root, offsets, n_nodes, total_leaves, body, exec
      );
    } else {
      total_leaves = total_next;
    }
  }
};

// ============================================================
// Core builder entry point (backend-parametric)
// ============================================================

template <class T, int DepthDims, class Index, class Body, int TD>
struct check_count_proxy_invocable {
  static void run() {
    using row_t = CountProxy<T, DepthDims, TD, 1, Index>;
    static_assert(
      std::is_invocable<const Body&, std::size_t, row_t&>::value,
      "ragged::build_nd: body must be invocable as body(size_t, row)."
    );
  }
};

template <class T, int DepthDims, class Index, class Body, int... Is>
inline void check_all_count_proxies(std::integer_sequence<int, Is...>) {
  // Is... = 0..MaxT-1  => TargetDepth = Is+1
  (check_count_proxy_invocable<T, DepthDims, Index, Body, Is + 1>::run(), ...);
}

template <class Backend, class T, int DepthDims, class Index, class Body, class Exec>
ContiguousRaggedOffsets<Backend, T, DepthDims, Index>
build_nd_impl(std::size_t n_root, Body body, Exec exec) {
  static_assert(DepthDims >= 1);
  static_assert(std::is_integral_v<Index> && std::is_unsigned_v<Index>, "Index must be an unsigned integral type.");

  using out_t = ContiguousRaggedOffsets<Backend, T, DepthDims, Index>;

  out_t out{};
  out.n_root = n_root;
  out.total_leaves = 0;

  if (n_root == 0) return out;

  // Signature checks (compile time).
  if constexpr (DepthDims == 1) {
    using row_count_t = RowCounter1D<T>;
    using row_fill_t  = RowFiller1DPtr<T, T*>;
    static_assert(std::is_invocable_v<const Body&, std::size_t, row_count_t&>,
                  "ragged::build_nd: body must be invocable as body(size_t, row).");
    static_assert(std::is_invocable_v<const Body&, std::size_t, row_fill_t&>,
                  "ragged::build_nd: body must be invocable as body(size_t, row).");
  } else {
    constexpr int MaxT = DepthDims - 1;

    check_all_count_proxies<T, DepthDims, Index, Body>(
    std::make_integer_sequence<int, MaxT>{});

    using row_fill_t = FillProxy<T, DepthDims, 1, Index, T*>;
    static_assert(std::is_invocable_v<const Body&, std::size_t, row_fill_t&>,
                  "ragged::build_nd: body must be invocable as body(size_t, row).");
  }

  const Body& body_ref = body;

  if constexpr (DepthDims == 1) {
    // Ordered flat-vector build:
    // store counts in off(i+1), inclusive-scan to offsets, allocate values, then fill.
    typename Backend::template vector<Index> off;
    off.resize(n_root + 1);
    Backend::fill(off, Index(0), exec);

    Index* off_p = Backend::raw_ptr(off);

    const Body body_c = body_ref;

    struct Count1 {
      Index* off;
      Body body;

      PORTABLE_INLINE_FUNCTION void operator()(std::size_t i) const {
        RowCounter1D<T> cnt{};
        body(i, cnt);

        const std::size_t c = cnt.c;
        const std::size_t imax = static_cast<std::size_t>((std::numeric_limits<Index>::max)());
        off[i + 1] = static_cast<Index>(c > imax ? imax : c);
      }
    };

    Backend::for_each_index(n_root, Count1{off_p, body_c}, exec);

    Backend::inclusive_scan_inplace(off, 1, exec);

    const Index total_i = Backend::read_back(off, n_root, exec);
    const std::size_t total = static_cast<std::size_t>(total_i);

    out.total_leaves = total;
    out.values.resize(total);

    T* values_p = Backend::raw_ptr(out.values);

    const Body body_c2 = body_ref;

    struct Fill1 {
      const Index* off;
      T* values;
      Body body;

      PORTABLE_INLINE_FUNCTION void operator()(std::size_t i) const {
        const std::size_t base = static_cast<std::size_t>(off[i]);
        RowFiller1DPtr<T, T*> row{values + base, 0};
        body(i, row);
      }
    };

    Backend::for_each_index(n_root, Fill1{off_p, values_p, body_c2}, exec);

    out.root = typename out_t::root_view_t(values_p, total);
    return out;

  } else {
    // ND: one count+scan per depth to build offsets[1..DepthDims-1].
    std::array<std::size_t, DepthDims + 1> n_nodes{};
    for (auto& x : n_nodes) x = 0;
    n_nodes[1] = n_root;

    std::size_t total_leaves = 0;
    BuildDepth<Backend, T, DepthDims, Index, 1, Body, Exec>::run(
      n_root, out.offsets, n_nodes, total_leaves, body_ref, exec
    );

    out.total_leaves = total_leaves;
    out.values.resize(total_leaves);

    // Fill pass.
    ptr_table<const Index*, DepthDims> off_ptrs{};
    for (int d = 0; d < DepthDims; ++d) off_ptrs.p[d] = nullptr;
    for (int d = 1; d <= DepthDims - 1; ++d) off_ptrs.p[d] = Backend::raw_ptr(out.offsets[d]);

    T* values_p = Backend::raw_ptr(out.values);

    const Body body_c = body_ref;

    struct KernelFill {
      ptr_table<const Index*, DepthDims> off;
      T* values;
      Body body;

      PORTABLE_INLINE_FUNCTION void operator()(std::size_t i) const {
        FillProxy<T, DepthDims, 1, Index, T*> row(off.data(), values, static_cast<Index>(i));
        body(i, row);
      }
    };

    Backend::for_each_index(n_root, KernelFill{off_ptrs, values_p, body_c}, exec);

    out.root = typename out_t::root_view_t{off_ptrs, values_p, n_root};
    return out;
  }
}

} // namespace ragged_detail
