// ragged_test.cpp
//
// Assert-based tests for the implementations:
//
//   * ragged.hpp (selector)
//   * ragged_offsets_core.hpp (core)
//   * ragged_{std|thrust|kokkos}.hpp (backends)
//
// These tests are intended to be compiled and run once per backend selection:
//
//   STL backend (default):
//     g++ -std=c++20 -O2 ragged_test.cpp -o ragged_test
//
//   Thrust backend:
//     nvcc -std=c++20 -O2 ragged_test.cpp -o ragged_test -DRAGGED_USE_THRUST
//     # add -DRAGGED_THRUST_USE_HOST for host_vector backend
//
//   Kokkos backend:
//     g++ -std=c++20 -O2 ragged_test.cpp -o ragged_test -DRAGGED_USE_KOKKOS <kokkos flags>
//
// Constraints:
//   * All asserts are host-side.
//   * For device backends, we validate root-view indexing inside device kernels by writing flags,
//     then copy flags back to host and assert.
//
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <type_traits>
#include <vector>

#include <ports-of-call/ragged.hpp>

#if defined(PORTABILITY_STRATEGY_KOKKOS)
  #include <Kokkos_Core.hpp>
#endif

#if defined(PORTABILITY_STRATEGY_CUDA)
  #include <thrust/device_vector.h>
  #include <thrust/host_vector.h>
  #include <thrust/device_ptr.h>
  #include <thrust/for_each.h>
  #include <thrust/copy.h>
  #include <thrust/iterator/counting_iterator.h>
#endif

namespace test_detail {

template <class Ragged>
auto values_to_host(const Ragged& r) {
  using T = typename std::remove_reference_t<Ragged>::value_type;
  std::vector<T> host;

#if defined(RAGGED_USE_KOKKOS)
  {
    auto v_d = r.values.v;
    auto v_h = Kokkos::create_mirror_view_and_copy(Kokkos::HostSpace(), v_d);
    host.resize(v_h.extent(0));
    for (std::size_t i = 0; i < host.size(); ++i) host[i] = v_h(i);
  }
#elif defined(RAGGED_USE_THRUST)
  {
    thrust::host_vector<T> h = r.values;
    host.assign(h.begin(), h.end());
  }
#else
  {
    host = r.values;
  }
#endif
  return host;
}

template <class Ragged>
auto offsets_to_host(const Ragged& r, int level) {
  using Index = typename std::remove_reference_t<Ragged>::index_type;
  std::vector<Index> host;

#if defined(RAGGED_USE_KOKKOS)
  {
    auto off_d = r.offsets[level].v;
    auto off_h = Kokkos::create_mirror_view_and_copy(Kokkos::HostSpace(), off_d);
    host.resize(off_h.extent(0));
    for (std::size_t i = 0; i < host.size(); ++i) host[i] = off_h(i);
  }
#elif defined(RAGGED_USE_THRUST)
  {
    thrust::host_vector<Index> h = r.offsets[level];
    host.assign(h.begin(), h.end());
  }
#else
  {
    const auto& off = r.offsets[level];
    host.assign(off.begin(), off.end());
  }
#endif
  return host;
}

template <class Index>
void assert_monotone_offsets(const std::vector<Index>& off) {
  assert(!off.empty());
  assert(off.front() == Index{0});
  for (std::size_t i = 1; i < off.size(); ++i) assert(off[i] >= off[i - 1]);
}

#if defined(RAGGED_USE_KOKKOS)

template <class RootView>
void kokkos_check_depth2_root_view(const RootView& root, std::size_t n_root, double baseA, double baseB,
                                   Kokkos::DefaultExecutionSpace exec) {
  Kokkos::View<int*, Kokkos::DefaultExecutionSpace::memory_space> flags("flags", n_root);

  const std::size_t imax = static_cast<std::size_t>((std::numeric_limits<int>::max)());
  const int n_int = static_cast<int>(n_root <= imax ? n_root : imax);

  Kokkos::parallel_for(
    "ragged_check_depth2",
    Kokkos::RangePolicy<Kokkos::DefaultExecutionSpace, Kokkos::IndexType<int>>(exec, 0, n_int),
    PORTABLE_LAMBDA(const int ii) {
      const std::size_t i = static_cast<std::size_t>(ii);
      int ok = 1;
      auto leaf = root[i];
      if (leaf.extent(0) != i + 2) ok = 0;
      for (std::size_t j = 0; j < i + 2 && ok; ++j) {
        const double expected = baseA + baseB * double(i) + double(j);
        if (double(leaf[j]) != expected) ok = 0;
      }
      flags(ii) = ok;
    }
  );
  exec.fence();

  auto flags_h = Kokkos::create_mirror_view_and_copy(Kokkos::HostSpace(), flags);
  for (int i = 0; i < n_int; ++i) assert(flags_h(i) == 1);
}

template <class RootView>
void kokkos_check_depth3_root_view(const RootView& root, std::size_t n_root, double ai, double aj,
                                   Kokkos::DefaultExecutionSpace exec) {
  Kokkos::View<int*, Kokkos::DefaultExecutionSpace::memory_space> flags("flags", n_root);

  const std::size_t imax = static_cast<std::size_t>((std::numeric_limits<int>::max)());
  const int n_int = static_cast<int>(n_root <= imax ? n_root : imax);

  Kokkos::parallel_for(
    "ragged_check_depth3",
    Kokkos::RangePolicy<Kokkos::DefaultExecutionSpace, Kokkos::IndexType<int>>(exec, 0, n_int),
    PORTABLE_LAMBDA(const int ii) {
      const std::size_t i = static_cast<std::size_t>(ii);
      int ok = 1;
      auto lvl2 = root[i];
      if (lvl2.extent(0) != i + 1) ok = 0;
      for (std::size_t j = 0; j < i + 1 && ok; ++j) {
        auto leaf = lvl2[j];
        if (leaf.extent(0) != j + 1) ok = 0;
        for (std::size_t k = 0; k < j + 1 && ok; ++k) {
          const double expected = ai * double(i) + aj * double(j) + double(k);
          if (double(leaf[k]) != expected) ok = 0;
        }
      }
      flags(ii) = ok;
    }
  );
  exec.fence();

  auto flags_h = Kokkos::create_mirror_view_and_copy(Kokkos::HostSpace(), flags);
  for (int i = 0; i < n_int; ++i) assert(flags_h(i) == 1);
}

#endif // RAGGED_USE_KOKKOS

#if defined(RAGGED_USE_THRUST) && !defined(RAGGED_THRUST_USE_HOST)

template <class RootView>
void thrust_check_depth2_root_view(const RootView& root, std::size_t n_root, double baseA, double baseB) {
  thrust::device_vector<int> flags(n_root);
  int* flags_p = thrust::raw_pointer_cast(flags.data());

  auto begin = thrust::make_counting_iterator<std::size_t>(0);
  auto end   = begin + static_cast<std::ptrdiff_t>(n_root);

  thrust::for_each(
    thrust::device,
    begin, end,
    PORTABLE_LAMBDA(const std::size_t i) {
      int ok = 1;
      auto leaf = root[i];
      if (leaf.extent(0) != i + 2) ok = 0;
      for (std::size_t j = 0; j < i + 2 && ok; ++j) {
        const double expected = baseA + baseB * double(i) + double(j);
        if (double(leaf[j]) != expected) ok = 0;
      }
      flags_p[i] = ok;
    }
  );

  thrust::host_vector<int> flags_h = flags;
  for (std::size_t i = 0; i < n_root; ++i) assert(flags_h[i] == 1);
}

template <class RootView>
void thrust_check_depth3_root_view(const RootView& root, std::size_t n_root, double ai, double aj) {
  thrust::device_vector<int> flags(n_root);
  int* flags_p = thrust::raw_pointer_cast(flags.data());

  auto begin = thrust::make_counting_iterator<std::size_t>(0);
  auto end   = begin + static_cast<std::ptrdiff_t>(n_root);

  thrust::for_each(
    thrust::device,
    begin, end,
    PORTABLE_LAMBDA(const std::size_t i) {
      int ok = 1;
      auto lvl2 = root[i];
      if (lvl2.extent(0) != i + 1) ok = 0;
      for (std::size_t j = 0; j < i + 1 && ok; ++j) {
        auto leaf = lvl2[j];
        if (leaf.extent(0) != j + 1) ok = 0;
        for (std::size_t k = 0; k < j + 1 && ok; ++k) {
          const double expected = ai * double(i) + aj * double(j) + double(k);
          if (double(leaf[k]) != expected) ok = 0;
        }
      }
      flags_p[i] = ok;
    }
  );

  thrust::host_vector<int> flags_h = flags;
  for (std::size_t i = 0; i < n_root; ++i) assert(flags_h[i] == 1);
}

#endif // RAGGED_USE_THRUST && !RAGGED_THRUST_USE_HOST

} // namespace test_detail

// ---------------- Tests ----------------

template <class T>
void test_depth1_flat_onlyfill(std::size_t n_root) {
#if defined(RAGGED_USE_KOKKOS)
  Kokkos::DefaultExecutionSpace exec;
  auto rag = PortsOfCall::ragged::build_nd<T, 1>(n_root,
    PORTABLE_LAMBDA(std::size_t i, auto& row) {
      const std::size_t n = i + 2;
      row.resize(n);
      for (std::size_t j = 0; j < n; ++j) {
        const T v = PortsOfCall::ragged::only_fill(row, [&] { return T(5000) + T(10) * T(i) + T(j); });
        row[j] = v;
      }
    },
    exec
  );
#else
  auto rag = PortsOfCall::ragged::build_nd<T, 1>(n_root,
    PORTABLE_LAMBDA(std::size_t i, auto& row) {
      const std::size_t n = i + 2;
      row.resize(n);
      for (std::size_t j = 0; j < n; ++j) {
        const T v = PortsOfCall::ragged::only_fill(row, [&] { return T(5000) + T(10) * T(i) + T(j); });
        row[j] = v;
      }
    }
  );
#endif

  auto vals = test_detail::values_to_host(rag);

  std::size_t expected_total = 0;
  for (std::size_t i = 0; i < n_root; ++i) expected_total += (i + 2);

  assert(rag.total_leaves == expected_total);
  assert(vals.size() == expected_total);
  assert(std::size_t(rag.root.extent(0)) == expected_total);

  std::size_t cursor = 0;
  for (std::size_t i = 0; i < n_root; ++i) {
    for (std::size_t j = 0; j < i + 2; ++j) {
      const T expected = T(5000) + T(10) * T(i) + T(j);
      assert(vals[cursor + j] == expected);
    }
    cursor += (i + 2);
  }

  rag.destroy();
  assert(rag.n_root == 0);
  assert(rag.total_leaves == 0);
#if defined(RAGGED_USE_KOKKOS)
  assert(rag.values.size() == 0);
#else
  assert(rag.values.size() == 0);
#endif
}

template <class T>
void test_depth2_resize_onlyfill(std::size_t n_root) {
  constexpr double baseA = 1000.0;
  constexpr double baseB = 10.0;

#if defined(RAGGED_USE_KOKKOS)
  Kokkos::DefaultExecutionSpace exec;
  auto rag = PortsOfCall::ragged::build_nd<T, 2>(n_root,
    PORTABLE_LAMBDA(std::size_t i, auto& row) {
      const std::size_t n = i + 2;
      row.resize(n);
      for (std::size_t j = 0; j < n; ++j) {
        const T v = PortsOfCall::ragged::only_fill(row, [&] { return T(baseA) + T(baseB) * T(i) + T(j); });
        row[j] = v;
      }
    },
    exec
  );
#else
  auto rag = PortsOfCall::ragged::build_nd<T, 2>(n_root,
    PORTABLE_LAMBDA(std::size_t i, auto& row) {
      const std::size_t n = i + 2;
      row.resize(n);
      for (std::size_t j = 0; j < n; ++j) {
        const T v = PortsOfCall::ragged::only_fill(row, [&] { return T(baseA) + T(baseB) * T(i) + T(j); });
        row[j] = v;
      }
    }
  );
#endif

  // Host validation via offsets + values
  auto off1 = test_detail::offsets_to_host(rag, 1);
  auto vals = test_detail::values_to_host(rag);
  test_detail::assert_monotone_offsets(off1);
  assert(std::size_t(off1.back()) == vals.size());
  assert(rag.total_leaves == vals.size());

  std::size_t expected_total = 0;
  for (std::size_t i = 0; i < n_root; ++i) expected_total += (i + 2);
  assert(vals.size() == expected_total);

  for (std::size_t i = 0; i < n_root; ++i) {
    const std::size_t b = static_cast<std::size_t>(off1[i]);
    const std::size_t e = static_cast<std::size_t>(off1[i + 1]);
    assert(e - b == i + 2);
    for (std::size_t j = 0; j < i + 2; ++j) {
      const T expected = T(baseA) + T(baseB) * T(i) + T(j);
      assert(vals[b + j] == expected);
    }
  }

  // Root-view API validation (device-safe on device backends)
#if defined(RAGGED_USE_KOKKOS)
  test_detail::kokkos_check_depth2_root_view(rag.root, n_root, baseA, baseB, exec);
#elif defined(RAGGED_USE_THRUST) && !defined(RAGGED_THRUST_USE_HOST)
  test_detail::thrust_check_depth2_root_view(rag.root, n_root, baseA, baseB);
#else
  // Host-only check (STL or Thrust host mode)
  for (std::size_t i = 0; i < n_root; ++i) {
    auto leaf = rag.root[i];
    assert(std::size_t(leaf.extent(0)) == i + 2);
    // Also check leaf_span() convenience on host backends
    auto s = leaf.leaf_span();
    assert(std::size_t(s.extent(0)) == i + 2);
    for (std::size_t j = 0; j < i + 2; ++j) {
      const double expected = baseA + baseB * double(i) + double(j);
      assert(double(leaf[j]) == expected);
      assert(double(s[j]) == expected);
    }
  }
#endif

  rag.destroy();
  assert(rag.n_root == 0);
  assert(rag.total_leaves == 0);
}

template <class T>
void test_depth3_resize_leaf_values(std::size_t n_root) {
  constexpr double ai = 100.0;
  constexpr double aj = 10.0;

#if defined(RAGGED_USE_KOKKOS)
  Kokkos::DefaultExecutionSpace exec;
  auto rag = PortsOfCall::ragged::build_nd<T, 3>(n_root,
    PORTABLE_LAMBDA(std::size_t i, auto& row) {
      const std::size_t n2 = i + 1;
      row.resize(n2);
      for (std::size_t j = 0; j < n2; ++j) {
        auto child = row[j];
        const std::size_t n3 = j + 1;
        child.resize(n3);
        for (std::size_t k = 0; k < n3; ++k) {
          const T v = PortsOfCall::ragged::only_fill(child, [&] { return T(ai) * T(i) + T(aj) * T(j) + T(k); });
          child[k] = v;
        }
      }
    },
    exec
  );
#else
  auto rag = PortsOfCall::ragged::build_nd<T, 3>(n_root,
    PORTABLE_LAMBDA(std::size_t i, auto& row) {
      const std::size_t n2 = i + 1;
      row.resize(n2);
      for (std::size_t j = 0; j < n2; ++j) {
        auto child = row[j];
        const std::size_t n3 = j + 1;
        child.resize(n3);
        for (std::size_t k = 0; k < n3; ++k) {
          const T v = PortsOfCall::ragged::only_fill(child, [&] { return T(ai) * T(i) + T(aj) * T(j) + T(k); });
          child[k] = v;
        }
      }
    }
  );
#endif

  auto off1 = test_detail::offsets_to_host(rag, 1);
  auto off2 = test_detail::offsets_to_host(rag, 2);
  auto vals = test_detail::values_to_host(rag);

  test_detail::assert_monotone_offsets(off1);
  test_detail::assert_monotone_offsets(off2);
  assert(std::size_t(off2.back()) == vals.size());
  assert(rag.total_leaves == vals.size());

  std::size_t expected_total = 0;
  for (std::size_t i = 0; i < n_root; ++i) expected_total += (i + 1) * (i + 2) / 2;
  assert(vals.size() == expected_total);

  // Validate offsets mapping and values
  for (std::size_t i = 0; i < n_root; ++i) {
    const std::size_t b2 = static_cast<std::size_t>(off1[i]);
    const std::size_t e2 = static_cast<std::size_t>(off1[i + 1]);
    assert(e2 - b2 == i + 1);

    for (std::size_t j = 0; j < i + 1; ++j) {
      const std::size_t node2 = b2 + j;
      const std::size_t bl = static_cast<std::size_t>(off2[node2]);
      const std::size_t el = static_cast<std::size_t>(off2[node2 + 1]);
      assert(el - bl == j + 1);
      for (std::size_t k = 0; k < j + 1; ++k) {
        const T expected = T(ai) * T(i) + T(aj) * T(j) + T(k);
        assert(vals[bl + k] == expected);
      }
    }
  }

  // Root-view API validation
#if defined(RAGGED_USE_KOKKOS)
  test_detail::kokkos_check_depth3_root_view(rag.root, n_root, ai, aj, exec);
#elif defined(RAGGED_USE_THRUST) && !defined(RAGGED_THRUST_USE_HOST)
  test_detail::thrust_check_depth3_root_view(rag.root, n_root, ai, aj);
#else
  // Host-only: also check leaf_span() for a couple leaves
  for (std::size_t i = 0; i < n_root; ++i) {
    auto lvl2 = rag.root[i];
    assert(std::size_t(lvl2.extent(0)) == i + 1);
    for (std::size_t j = 0; j < i + 1; ++j) {
      auto leaf = lvl2[j];
      assert(std::size_t(leaf.extent(0)) == j + 1);
      auto s = leaf.leaf_span();
      assert(std::size_t(s.extent(0)) == j + 1);
      for (std::size_t k = 0; k < j + 1; ++k) {
        const double expected = ai * double(i) + aj * double(j) + double(k);
        assert(double(leaf[k]) == expected);
        assert(double(s[k]) == expected);
      }
    }
  }
#endif

  rag.destroy();
  assert(rag.n_root == 0);
  assert(rag.total_leaves == 0);
}

template <class T>
void test_depth3_emplace_push(std::size_t n_root) {
  constexpr double ai = 200.0;
  constexpr double aj = 20.0;

#if defined(RAGGED_USE_KOKKOS)
  Kokkos::DefaultExecutionSpace exec;
  auto rag = PortsOfCall::ragged::build_nd<T, 3>(n_root,
    PORTABLE_LAMBDA(std::size_t i, auto& row) {
      for (std::size_t j = 0; j < i + 1; ++j) {
        auto child = row.emplace_back();
        for (std::size_t k = 0; k < j + 1; ++k) {
          const T v = PortsOfCall::ragged::only_fill(child, [&] { return T(ai) * T(i) + T(aj) * T(j) + T(k); });
          child.push_back(v);
        }
      }
    },
    exec
  );
#else
  auto rag = PortsOfCall::ragged::build_nd<T, 3>(n_root,
    PORTABLE_LAMBDA(std::size_t i, auto& row) {
      for (std::size_t j = 0; j < i + 1; ++j) {
        auto child = row.emplace_back();
        for (std::size_t k = 0; k < j + 1; ++k) {
          const T v = PortsOfCall::ragged::only_fill(child, [&] { return T(ai) * T(i) + T(aj) * T(j) + T(k); });
          child.push_back(v);
        }
      }
    }
  );
#endif

  auto off1 = test_detail::offsets_to_host(rag, 1);
  auto off2 = test_detail::offsets_to_host(rag, 2);
  auto vals = test_detail::values_to_host(rag);

  test_detail::assert_monotone_offsets(off1);
  test_detail::assert_monotone_offsets(off2);
  assert(std::size_t(off2.back()) == vals.size());
  assert(rag.total_leaves == vals.size());

  std::size_t expected_total = 0;
  for (std::size_t i = 0; i < n_root; ++i) expected_total += (i + 1) * (i + 2) / 2;
  assert(vals.size() == expected_total);

  // Validate values via offsets mapping
  for (std::size_t i = 0; i < n_root; ++i) {
    const std::size_t b2 = static_cast<std::size_t>(off1[i]);
    for (std::size_t j = 0; j < i + 1; ++j) {
      const std::size_t node2 = b2 + j;
      const std::size_t bl = static_cast<std::size_t>(off2[node2]);
      for (std::size_t k = 0; k < j + 1; ++k) {
        const T expected = T(ai) * T(i) + T(aj) * T(j) + T(k);
        assert(vals[bl + k] == expected);
      }
    }
  }

#if defined(RAGGED_USE_KOKKOS)
  test_detail::kokkos_check_depth3_root_view(rag.root, n_root, ai, aj, exec);
#elif defined(RAGGED_USE_THRUST) && !defined(RAGGED_THRUST_USE_HOST)
  test_detail::thrust_check_depth3_root_view(rag.root, n_root, ai, aj);
#else
  // Host-only
  for (std::size_t i = 0; i < n_root; ++i) {
    auto lvl2 = rag.root[i];
    assert(std::size_t(lvl2.extent(0)) == i + 1);
    for (std::size_t j = 0; j < i + 1; ++j) {
      auto leaf = lvl2[j];
      assert(std::size_t(leaf.extent(0)) == j + 1);
      for (std::size_t k = 0; k < j + 1; ++k) {
        const double expected = ai * double(i) + aj * double(j) + double(k);
        assert(double(leaf[k]) == expected);
      }
    }
  }
#endif

  rag.destroy();
  assert(rag.n_root == 0);
  assert(rag.total_leaves == 0);
}

/*int main(int , char** ) {
#if defined(RAGGED_USE_KOKKOS)
  Kokkos::initialize(argc, argv);
#endif

  test_depth1_flat_onlyfill<double>(8);
  test_depth2_resize_onlyfill<double>(8);
  test_depth3_resize_leaf_values<double>(7);
  test_depth3_emplace_push<double>(6);

#if defined(RAGGED_USE_KOKKOS)
  Kokkos::finalize();
#endif

  std::cout << "ragged_test: all tests passed.\n";
  return 0;
}*/
