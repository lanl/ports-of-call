.. _using:

Using Ports of Call
====================

Ports of call is a header-only library that provides a bit of
flexibility for performance portability. At the moment it mainly
provides a one-header abstraction to enable or disable `Kokkos`_ in a
code. However other backends can be added. (If you're interested in
adding a backend, please let us know!)

.. _Kokkos: https://github.com/kokkos/kokkos

To include Ports of Call in your project, simply include the directory
(e.g., as a submodule) in your include path.

1. ``PORTABLE_FUNCTION``: decorators necessary for compiling a kernel function
2. ``PORTABLE_INLINE_FUNCTION``: ditto, but for when functions ought to be inlined
3. ``PORTABLE_FORCEINLINE_FUNCTION``: forces the compiler to inline
4. ``PORTABLE_LAMBDA``: Resolves to a ``KOKKOS_LAMBDA`` or to ``[=]`` depending on context
5. ``_WITH_KOKKOS_``: Defined if Kokkos is enabled.
6. ``_WITH_CUDA_``: Defined when Cuda is enabled
7. ``Real``: a typedef to double (default) or float (if you define ``SINGLE_PRECISION_ENABLED``)
8. ``PORTABLE_FENCE()``: A wrapper for ``kokkos::fence`` or ``cudaDeviceSynchronize()``

At compile time, you define
``PORTABILITY_STRATEGY_{KOKKOS,CUDA,NONE}`` (if you don't define it,
it defaults to NONE). The above macros then behave as expected. In
particular, ``PORTABLE_FUNCTION`` and friends resolve to ``__host__
__device__`` decorators as appropriate.

There are several headers in this library, for different use cases.

portability.hpp
^^^^^^^^^^^^^^^^

``portability.hpp`` provides the above-mentioned macros for decorating
functions. It also provides several additional abstractions:

.. cpp:function:: template <typename E>PortsOfCall::portableMalloc(E e, std::size_t size_bytes)

and

.. cpp:function:: template<typename E, typename T> PortsOfCall::portableFree(E e, T *ptr)

allocate and free memory respectively, where the type ``E`` determines
the space where data is allocated and may be
``PortsOfCall::Exec::Host``, ``PortsOfCall::Exec::Device``, or any
type allowed by your backend. The selection of memory/execution space
is optional, and defaults to ``PortsOfCall::Exec::Device``.

These are also the backend for macros ``PORTABLE_MALLOC``
and ``PORTABLE_FREE``.

``portability.hpp`` also provides loop abstractions that can be
leveraged by a code. These loop abstractions are of the form:

.. cpp:function:: template <typename E, typename Function> void portableFor(const char *name, E e, int start, int stop, Function function)

where ``Function`` is a template parameter and should be set to a
functor that takes one index, e.g., an index in an array. For example:

.. code-block:: cpp

  portableFor("Example host", PortsOfCall::Exec::Host, 0, 5,
    PORTABLE_LAMBDA(int i) {
      printf("hello from thread %d\n", i);
  });

The optional template parameter ``E`` selects the execution space to
launch on and is defined above. It is optional, and defaults to device:

.. code-block:: cpp

  portableFor("Example device", 0, 5,
    PORTABLE_LAMBDA(int i) {
      printf("hello from host thread %d\n", i);
  });

``start`` is inclusive, ``stop`` is exclusive. Up to five-dimensional
``portableFor`` loops are available. For example:

.. code-block:: cpp

  template <typename E, typename Function>
  void portableFor(const char *name, E e, int startb, int stopb, int starta, int stopa,
    int startz, int stopz, int starty, int stopy, int startx,
    int stopx, Function function) {

We also provide ``portableReduce``, however the functionality is very
limited. The syntax is:

.. code-block::

  template <typename E, typename Function, typename T>
  void portableReduce(const char *name, E e, int starta, int stopa, int startz,
    int stopz, int starty, int stopy, int startx, int stopx,
    Function function, T &reduced) {

where ``Function`` now takes as many indices are required and
``reduced`` as arguments. Note that a ``portableReduce()`` is blocking (i.e. a
device synchronization step is performed) while `portableFor()` may not be,
possibly requiring a ``PORTABLE_FENCE`` to avoid any race conditions.

As with ``portableFor``, the optional ``E`` argument can be
used to select host or device execution explicitly. For example:

.. code-block:: cpp

  int sum = 0;
  portableReduce(
    "HostReduce", PortsOfCall::Exec::Host(), 0, 5,
    PORTABLE_LAMBDA(int i, int &local_sum) {
      local_sum += i;
    }, sum);

When selecting ``PortsOfCall::Exec::Host``, the lambda or functor and
the memory it touches must be valid on host. When selecting
``PortsOfCall::Exec::Device``, device-accessible storage should be used,
for example memory returned by ``PORTABLE_MALLOC()`` under device
backends.

Also provided are host to device and device to host memory transfers of the form:

.. cpp:function:: void portableCopyToHost(T * const to, T const * const from, size_t const size_bytes)

.. cpp:function:: void portableCopyToDevice(T * const to, T const * const from, size_t const size_bytes)

with `to` being the target location, from being the source location, and size_bytes is
the size of the transfer in bytes. This has implemenatations for kokkos and none 
portability strategies.

It may be useful to query the execution space, for example to know where memory needs to be copied.
To this end, a compile-time constant boolean can be queried:

.. cpp:var:: PortsOfCall::EXECUTION_IS_HOST

which is `true` if the host execution space can trivially access device memory space. For example,
for `PORTABILITY_STRATEGY_CUDA`, `PortsOfCall::EXECUTION_IS_HOST == false`.

portable_errors.hpp
^^^^^^^^^^^^^^^^^^^^

``portable_errors.hpp`` provides error handling that works with
different portability backends, such as `Kokkos`. We provide several
useful macros. All the macros in this file will print the file and
line number where the macro was called, enabling easier debugging.

The following macros are **disabled** automaticaly for production
builds (e.g., when the ``NDEBUG`` preprocessor macro is defined):

* ``PORTABLE_REQUIRE(condition, message)`` prints an error message and aborts the program (without throwing an exception) if compiled in debug mode and ``condition`` is not satisfied.
* ``PORTABLE_ABORT(message)`` prints an error message and aborts the program when compiled in debug mode.
* ``PORTABLE_WARN(message)`` prints a warning message if compiled in debug mode.
* ``PORTABLE_THROW_OR_ABORT(message)`` prints an error message and then raises a runtime error if ``PORTABILITY_STRATEGY`` is ``NONE`` and otherwise aborts the program without an exception. This macro is disabled in production.

Each of the above macros is **disabled** and becomes a no-op for most builds and only enabled for ``Debug`` builds. However, for each of the above macros there is an equivalent ``PORTABLE_ALWAYS_*`` macro, which **always** functions and is **never** a no-op:

* ``PORTABLE_ALWAYS_REQUIRE(condition, message)`` prints an error message and aborts the program (without throwing an exception) if ``condition`` is not satisfied.
* ``PORTABLE_ALWAYS_ABORT(message)`` prints an error message and aborts the program.
* ``PORTABLE_ALWAYS_WARN(message)`` prints a warning message.
* ``PORTABLE_ALWAYS_THROW_OR_ABORT(message)`` prints an error message and then raises a runtime error if ``PORTABILITY_STRATEGY`` is ``NONE`` and otherwise aborts the program without an exception.

Additionally the macro

* ``PORTABLE_ERROR_MESSAGE(message, output)`` fills an output ``char*`` with a useful error message containing the filename and line number where the macro is called. Note there is no bounds checking so you **must** provide the macro with a sufficiently large ``char*`` array.

The ``message`` parameter in the above macros can be ``char*`` arrays and string literals on device and additionally accepts ``std::string`` and ``std::stringstream`` on host.

Please note that none of these functions are thread or MPI aware. In a parallel program, the same message may be called **many times**. Therefore caution should be used with this machinery and you may wish to hide these macros in if statements, for example,

.. code-block::

  if (rank == 0) PORTABLE_REQUIRE(my_condition, my_message);

as appropriate.

robust_utils.hpp
^^^^^^^^^^^^^^^^^^^

``robust_utils.hpp`` contains small utility functions for numerical
robustness, especially around floating point numbers. The available
functionality is contained in the namespace ``PortsOfCall::Robust`` and includes:

* ``constexpr bool is_normal(const T val, const T )`` returns whether or not a number is normal
* ``constexpr bool is_normal_or_zero(const T val)`` returns whether or not a float is normal or zero
* ``constexpr auto SMALL<T>()`` returns a small number of type ``T``.
* ``constexpr auto EPS<T>()`` returns a value of type ``T`` close to machine epsilon.
* ``constexpr auto min_exp_arg<T>()`` returns the smallest safe value of type ``T`` to pass into an exponent.
* ``constexpr auto max_exp_exp_arg<T>()`` returns the max safe value of type ``T`` to pass into an exponent.
* ``auto make_positive(const T val)`` makes the argument of type ``T`` positive.

where here all functionality is templated on type ``T`` and marked
with ``PORTABLE_INLINE_FUNCTION``. The default type ``T`` is always
``Real``.

The function

.. code-block:: cpp

  template <typename T>
  PORTABLE_FORCEINLINE_FUNCTION
  Real make_bounded(const T val, const T vmin, const T vmax);

bounds ``val`` between ``vmin`` and ``vmax``, exclusive. Note this is
slightly different than ``std::clamp``, which uses inclusive bounds.

The function

.. code-block:: cpp

  template <typename T>
  PORTABLE_FORCEINLINE_FUNCTION int sgn(const T &val);

returns the sign of a quantity ``val``.

.. note::

  Note this implementation **never** returns zero. It **always**
  returns :math:`\pm 1`.

The function

.. code-block:: cpp

  template <typename A, typename B>
  PORTABLE_FORCEINLINE_FUNCTION auto ratio(const A &a, const B &b)

computes the ratio :math:`A/B` but in a way robust to 0/0 errors. If
both :math:`A` and :math:`B` are zero, this function will return 0. If
:math:`|A| > 0` and :math:`B=0`, then it will return a very large,
possibly (but not guaranteed to be) infinite number.

The function

.. code-block:: cpp

  template <typename T>
  PORTABLE_FORCEINLINE_FUNCTION T safe_arg_exp(const T &x)

returns exponentiation in such a way that avoids floating point
exceptions. For very large negative inputs, it returns 0. For very
large positive ones, it returns
``std::numeric_limits<T>::infinity()``.

The function

.. code-block:: cpp

  template <typename T>
  PORTABLE_FUNCTION constexpr bool check_nonnegative(const T t)

checks if the value is non-negative (:math:`t \geq 0`).  There are two
versions: one for signed values (performs the check and returns the result) and
one for unsigned values (simply returns true, since unsigned values can never
be negative).  This is typically used in generic code where a value must be
non-negative, but the type is unknown and therefore may be either signed or
unsigned.  Simply using ``t >= 0`` can cause undesirable warnings about
unsigned integer comparisons, so ``check_nonnegative`` is provided.

math_utils.hpp
^^^^^^^^^^^^^^^^^^^

``math_utils.hpp`` contains math operations intended to be both performant and
portable to GPUs.

The function

.. code-block:: cpp

  template <typename base_t, typename exp_t>
  PORTABLE_FUNCTION constexpr inline base_t int_power(base_t base, exp_t exp)

is equivalent to ``std::pow`` except that the exponent is required to be an
integer.  For small integer powers, ``int_power`` is faster than ``std::pow``.
For sufficiently large integer powers, ``std::pow`` may be faster, but testing
indicates ``int_power`` is significantly faster (roughly a factor of two or
better) up to power of at least 100.

The function

.. code-block:: cpp

  template <
    typename IterB,
    typename IterE,
    typename Value,
    typename Op = singe::util::plus<Value>>
  PORTABLE_FUNCTION constexpr Value accumulate(
    IterB begin,
    IterE end,
    Value accum,
    Op && op = singe::util::plus<Value>{})

is a simple ``constexpr`` implementation of ``std::accumulate`` from the STL.
Ports-of-Call also provides a ``constexpr`` implementation of ``std::plus``
(which is the default operator for ``accumulate``).

The function

.. code-block:: cpp
  PORTABLE_FORCEINLINE_FUNCTION float expm1(float x)

and 
.. code-block:: cpp
  PORTABLE_FORCEINLINE_FUNCTION double expm1(double x)

compute :math:`\exp(x) - 1`

macros_arrays.hpp
^^^^^^^^^^^^^^^^^^^

``portable_arrays.hpp`` provides a wrapper class, ``PortableMDArray``,
around a contiguous block of host or device memory that knows stride
and layout, enabling one to mock up multidimensional arrays from a
pointer to memory. The design is heavily inspired by the
``AthenaArray`` class from `Athena++`_.

.. _`Athena++`: https://www.athena-astro.app

One constructs a ``PortableMDArray`` by passing it a pointer to
underlying data and a shape. For example:

.. code-block:: cpp

  #include <portability.hpp>
  #include <portable_arrays.hpp>
  constexpr int NX = 2;
  constexpr int NY = 3;
  constexpr int NZ = 4;
  Real *data = (Real*)PORTABLE_MALLOC(NX*NY*NZ*sizeof(Real));
  PortableMDArray<Real> my_3d_array(data, NZ, NY, NX);

.. note::
  ``PortableMDArray`` is templated on underlying data
  type.

.. note::
   ``PortableMDArray`` is column-major-ordered. The
  slowest moving index is ``z`` and the fastest is ``x``.

You can then set or access an element by reference as:

.. code-block:: cpp

  // z = 3, y = 2, x = 1
  my_3d_array(3,2,1) = 5.0;

You can always access the "flat" array by simply using the 1D bracket operator:

.. code-block:: cpp

  my_3d_array[6] = 2.0;

.. warning::

  Currently, the 1D parentheses operator currently also accesses the
  flat array. However, this syntax may eventually be deprecated.

By default ``PortableMDArray`` has reference-semantics. In
other words, copies are shallow.

You can assign new data and a new shape to a ``PortableMDArray`` with
the ``NewPortableMDArray`` function. For example:

.. code-block:: cpp

  my_3d_array.NewPortableArray(new_data, 9, 8, 7);

would reshape ``my_3d_array`` to be of shape 7x8x9 and point it at the
``new_data`` pointer.

``PortableMDArray`` also provides a few useful methods:

.. cpp:function:: size_t PortableMDArray::GetRank()

provides the number of dimensions of the array.

.. cpp:function:: int PortableMDArray::GetDim(size_t i)

returns the size of a given dimension (indexed from 1, not 0).

.. cpp:function:: int PortableMDArray::GetSize()

returns the size of the flattened array.

.. cpp:function:: size_t PortableMDArray::GetSizeInBytes()

returns the size of the flattened array in bytes.

.. cpp:function:: bool PortableMDArray::IsEmpty()

returns true if the array is empty and false otherwise.

.. cpp:function:: T* PortableMDArray::data()

returns the underlying pointer. The ``begin()`` and ``end()``
functions return pointers to the beginning and end of the array.

.. cpp:function:: void PortableMDArray::Reshape(int nx3, int nx2, int nx1)

resets the shape of the array without pointing to a new underlying
data pointer. It accepts anywhere between 1 and 6 sizes.

``PortableMDArray`` also supports some simple boolean comparitors,
such as ``==`` and arithmetic such as ``+``, and ``-``.

array.hpp
^^^^^^^^^

``PortsOfCall::array`` is intended to be a drop-in replacement for ``std::array``, with the
exception that it works on GPUs.  As of C++17, ``std::array::fill`` and ``std::array::swap`` are
not yet ``constexpr``, so even with the "relaxed ``constexpr``" compilation mode ``std::array`` is
not feature-complete on GPUs.  This will change when those member functions become ``constexpr`` in
C++20.


span.hpp


``PortsOfCall::span`` is implements ``std::span`` for C++17 as a view
over contiguous data. ``span`` may have compile-time static extent, or
a dynamic extent.  ``span`` provides iterator functions similar to
containers.

.. code-block:: cpp
  
 int arr[] = {1, 2, 3};
 auto s = span{arr};
 for(auto & i : s)
 {
   i -= 1;
 }

``span::subspan`` returns a span over a subrange. Element access uses ``span::operator[]``. For 
more information, see `C++ reference page <https://en.cppreference.com/w/cpp/container/span>`_.

.. note::

  Note that now that we rely on C++20, ``PortsOfCall::span`` may
  eventually be replaced by ``std::span``.

static_vector.hpp
^^^^^^^^^^^^^^^^^

``PortsOfCall::static_vector`` is a GPU-compatible data structure that provides a
``std::vector``-like interface, but uses ``std::array``-like backing storage.  That means that the
size is variable, but the capacity is fixed at runtime.  This allows the creation of a data
structure of non-default-constructible objects like with a ``std::vector``.  This also allows the
type to be self-contained: no pointers, so a ``PortsOfCall::static_vector`` can be memcopied
between CPU and GPU.  It is related to a `proposed data structure
https://www.open-std.org/jtc1/sc22/wg21/docs/papers/2023/p0843r8.html`_ that may be included in a
future C++ standard.

variant.hpp
^^^^^^^^^^^

``PortsOfCall::variant`` is a GPU capable backport of ``std::variant``
to C++11. It is a lightly modified copy of `mpark variant`_, that has
been modified to be GPU capable and function on both NVIDIA and AMD
GPUs. The API is identical to that for the standards provided
`variant`_, with the exception that all API calls are in the
``PortsOfCall`` namespace rather than ``std`` and that exceptions are
optional. The code **will** throw exceptions on CPUs, but they will
not be thrown on device.

.. _`mpark variant`:  https://github.com/mpark/variant
.. _`variant`: https://en.cppreference.com/w/cpp/utility/variant.html

portable_memory.hpp
^^^^^^^^^^^^^^^^^^^

``PortsOfCall::SlabArenaPool`` is a portable memory pool capable of fast allocations on host or device memory.
The pool works by allocating "slabs" of memory that start at a user-specified size but then grow at a fixed factor as 
more memory is requested. 
During an allocation call, the requested number of bytes are rounded up to the nearest fixed-block size. The block sizes step by 16 bytes up to 64 bytes and then split every power-of-two range into four equal steps (80, 96, 112, 128, 160, 192, 224, 256, ...), so rounding wastes less than 25% of a block (about 10% on average) and power-of-two requests fit exactly. 
If there is a free and already allocated block of that size, a valid pointer is returned. 
If there is no free block of that size, the pool next tries to reuse a free block from one of the next four (one doubling's worth of) larger size classes, so idle memory is used before new space is carved; the block keeps its own size class and returns to that class's free list when freed.
Otherwise a new block is carved out of the remaining space in the current slab.
If the request does not fit in that space, the unused tail of the current slab is first cut into free blocks of the largest classes that fit, so it can still be reused, and then a new slab is created with a size slightly larger than the last slab and a new block is carved from it.
When the pointer is freed, the block that backed that allocation request is added to the list of available blocks.
The time for allocation and freeing is thus O(1), so long as a new slab is not needed.
Note that, because new slabs are added, the size of the pool can grow throughout the lifetime of the program.
Each live allocation costs a small host-side table entry, and each allocation and free is a hash-table insert or erase under the pool's mutex.
The pool is therefore aimed at array-sized allocations, particularly in device and managed memory where backend allocation is expensive, rather than at large numbers of tiny host allocations.

The fixed-block size classes go up to a maximum block size (2 :sup:`30` bytes, or 1 GiB, by default).
Requests larger than this maximum block size are served by a separate large-request path rather than being rejected.
The large-request path allocates a whole backing allocation directly from the memory-space backend, sized to the request rounded up to a coarse granularity, and returns it to a single caller.
When a large allocation is freed, its backing allocation is not immediately returned to the backend; instead it is retained in a cache keyed by capacity so that a later request of a similar size can reuse it without a fresh backend allocation.
A cached block is reused for a new request when its capacity is at least the requested size and not excessively larger than it, so nearby request sizes share backing allocations.
The cache is bounded by configurable limits on the total retained bytes, the number of retained blocks, and the largest single block that will be retained; when a limit is exceeded the oldest cached blocks are evicted and returned to the backend.
Because the large-request path is only reached above the maximum block size, the O(1) small-allocation fast path is unaffected.
The maximum block size is controlled by the ``PORTS_OF_CALL_MAX_SLAB_BLOCK_LOG2`` macro (a base-2 exponent, defaulting to ``30``), which may be defined before including the header to shrink the slab ceiling.

Creating and configuring a pool
"""""""""""""""""""""""""""""""

``SlabArenaPool`` takes the memory space as a template parameter.
In Kokkos builds this is a Kokkos memory space, e.g. ``SlabArenaPool<Kokkos::HostSpace>`` or ``SlabArenaPool<Kokkos::DefaultExecutionSpace::memory_space>``; otherwise use the default, ``SlabArenaPool<>``, which allocates with ``PORTABLE_MALLOC``.
A pool can be used either as a process-wide singleton or as a standalone object:

.. code-block:: cpp

  using HostPool = PortsOfCall::SlabArenaPool<Kokkos::HostSpace>;

  // The singleton, shared by every default-constructed PoolAllocator.
  HostPool &pool = HostPool::instance();
  pool.configure(64ull << 20, 0.5); // must precede the first allocation

  // A standalone pool with its own slabs.
  HostPool local(16ull << 20, 0.25);

.. cpp:function:: static SlabArenaPool &SlabArenaPool::instance()

  Returns the singleton pool for this memory space, constructing it with the default configuration on first use.
  In Kokkos builds the first call registers a ``Kokkos::finalize`` hook that calls ``release_all``, so the singleton must first be used while Kokkos is initialized, and pointers from it must not be used after ``Kokkos::finalize``.

.. cpp:function:: explicit SlabArenaPool::SlabArenaPool(std::size_t min_slab_bytes = 8ull << 20, double growth_factor = 0.25)

  Constructs a standalone pool. Its destructor calls ``release_all``. Pools cannot be copied.

.. cpp:function:: void SlabArenaPool::configure(std::size_t min_slab_bytes, double growth_factor)

  Sets the slab sizing.
  The first slab is ``min_slab_bytes`` (raised to at least 64 KiB) and each later slab is ``growth_factor`` larger than the previous one, by at least 64 KiB; a ``growth_factor`` of 0 keeps every slab the same size.
  A slab is always made large enough for the block that triggered it.
  Throws ``std::invalid_argument`` if ``growth_factor`` is negative or not finite, and ``std::runtime_error`` if the pool already holds slabs.

.. cpp:function:: void SlabArenaPool::configure_large_allocations(const LargeAllocationConfig &config)

  Sets the large-request cache limits.
  Throws ``std::runtime_error`` if the pool has ever served an allocation; unlike ``configure``, this is not reset by ``release_all``.

The fields of ``SlabArenaPool::LargeAllocationConfig`` are:

.. list-table::
  :header-rows: 1

  * - Field
    - Default
    - Meaning
  * - ``max_cached_bytes``
    - 8 GiB
    - Most backend bytes kept in the free large-block cache. 0 disables caching.
  * - ``max_cached_blocks``
    - 16
    - Most blocks kept in the cache.
  * - ``max_single_cached_bytes``
    - 4 GiB
    - Freed blocks larger than this go straight back to the backend.
  * - ``max_oversize_numerator`` / ``max_oversize_denominator``
    - 1 / 4
    - A cached block is reused only if it exceeds the (rounded) request by at most ``request * numerator / denominator`` ...
  * - ``min_oversize_bytes``
    - 64 MiB
    - ... or by at most this many bytes, whichever is larger.
  * - ``allocation_granularity_bytes``
    - 64 MiB
    - New large backend allocations are rounded up to a multiple of this.

For example, to cap the cache at 2 GiB:

.. code-block:: cpp

  HostPool::LargeAllocationConfig config;
  config.max_cached_bytes = 2ull << 30;
  config.max_single_cached_bytes = 1ull << 30;
  HostPool::instance().configure_large_allocations(config);

Allocating and freeing
""""""""""""""""""""""

All of the following are host functions, and all except the ``static`` ones lock the pool's mutex, so a pool may be shared between host threads.

.. cpp:function:: void *SlabArenaPool::alloc_bytes(std::size_t request, std::size_t alignment = alignof(std::max_align_t))

  Allocates ``request`` bytes in the pool's memory space, aligned to ``alignment``, which must be a power of two and is raised to at least ``alignof(std::max_align_t)``.
  A request of 0 bytes is treated as 1.
  The memory is uninitialized, and the pool never reads or writes it, so device-only memory is fine.
  Throws ``std::invalid_argument`` for an invalid alignment and ``std::bad_alloc`` if the size overflows or the backend is out of memory.

.. cpp:function:: void SlabArenaPool::free_bytes(void *p) noexcept

  Returns memory from ``alloc_bytes`` (or a ``Reservation``) to the pool.
  Passing ``nullptr`` does nothing; passing a pointer the pool does not own, including a double free, aborts.

.. cpp:function:: void SlabArenaPool::release_all()

  Returns every slab and every large block, cached or live, to the backend.
  Every outstanding pointer becomes invalid, and the next slab starts again at the minimum slab size.
  Calling it twice is harmless.

.. cpp:function:: void SlabArenaPool::trim_large_cache()

  Returns every cached (free) large block to the backend, leaving live allocations alone.

.. cpp:function:: void SlabArenaPool::trim_large_cache_to(std::size_t target_bytes)

  Returns cached large blocks to the backend, least recently used first, until at most ``target_bytes`` remain cached.

.. cpp:function:: static constexpr std::size_t SlabArenaPool::max_slab_block_bytes() noexcept

  The largest slab block size. A request whose size plus alignment padding exceeds this takes the large-request path.

.. cpp:function:: static std::size_t SlabArenaPool::max_allocation_bytes(std::size_t alignment = alignof(std::max_align_t)) noexcept

  The largest request ``alloc_bytes`` accepts for ``alignment``, or 0 if ``alignment`` is not a power of two.

Statistics
""""""""""

The pool tracks usage of the slab and large-request paths separately.
``slab_allocation_stats()`` and ``large_allocation_stats()`` each return a snapshot struct by value, so the values do not change after the call:

.. code-block:: cpp

  auto &pool = HostPool::instance();
  // ... run the workload ...

  const auto slab = pool.slab_allocation_stats();
  std::printf("slabs: %zu bytes, live: %zu requested in %zu block bytes "
              "(%zu lost to rounding), %zu on free lists\n",
              slab.slab_bytes, slab.live_requested_bytes, slab.live_block_bytes,
              slab.internal_fragmentation_bytes(), slab.free_list_bytes);

  const auto large = pool.large_allocation_stats();
  std::printf("large: %llu requests, %llu cache hits, %zu bytes cached\n",
              (unsigned long long)large.requests,
              (unsigned long long)large.cache_hits, large.cached_bytes);

Counters (the ``std::uint64_t`` fields) and peaks are cumulative over the life of the pool.
``release_all`` resets the live, active, and cached totals but keeps the counters and peaks.

.. cpp:function:: SlabAllocationStats SlabArenaPool::slab_allocation_stats() const

  Usage of the slab path. "Requested" bytes are what callers asked for; "block" bytes are the size-class blocks that served them.

  .. list-table::
    :header-rows: 1

    * - Field
      - Meaning
    * - ``requests``
      - Slab-path allocations served (cumulative).
    * - ``borrowed_allocations``
      - Allocations served by a free block of a larger class (cumulative).
    * - ``recycled_tail_bytes``
      - Slab tail bytes turned into free blocks when the pool moved to a new slab (cumulative).
    * - ``live_blocks``
      - Slab blocks currently allocated.
    * - ``live_requested_bytes`` / ``live_block_bytes``
      - Bytes requested by, and block bytes backing, the live allocations.
    * - ``peak_requested_bytes`` / ``peak_block_bytes``
      - High-water marks of the two live totals.
    * - ``free_list_bytes``
      - Bytes in freed blocks waiting for reuse.
    * - ``uncarved_bytes``
      - Slab bytes never handed out as blocks.
    * - ``slab_bytes``
      - Total backend bytes held in slabs.
    * - ``internal_fragmentation_bytes()``
      - ``live_block_bytes - live_requested_bytes``: bytes lost to size-class rounding.

  ``slab_bytes`` is always the sum of ``live_block_bytes``, ``free_list_bytes``, and ``uncarved_bytes``.

.. cpp:function:: LargeAllocationStats SlabArenaPool::large_allocation_stats() const

  Usage of the large-request path.

  .. list-table::
    :header-rows: 1

    * - Field
      - Meaning
    * - ``requests``
      - Large-path allocations (``cache_hits + cache_misses``).
    * - ``cache_hits`` / ``cache_misses``
      - Requests served from the cache, and requests that needed a new backend allocation.
    * - ``backend_allocations`` / ``backend_deallocations``
      - Large backend allocations and frees, excluding those made by ``release_all``.
    * - ``direct_frees``
      - Frees returned straight to the backend instead of being cached.
    * - ``evictions``
      - Cached blocks dropped to stay within the cache limits.
    * - ``allocation_retries_succeeded``
      - Backend allocations that succeeded only after the cache was trimmed.
    * - ``active_blocks``
      - Live large allocations.
    * - ``active_requested_bytes`` / ``active_capacity_bytes``
      - Bytes requested by, and backend bytes reserved for, the live large allocations.
    * - ``cached_blocks`` / ``cached_bytes``
      - Blocks and backend bytes held in the cache.
    * - ``peak_active_requested_bytes`` / ``peak_active_capacity_bytes`` / ``peak_cached_bytes``
      - High-water marks of the corresponding totals.

.. cpp:function:: std::size_t SlabArenaPool::slab_count() const

  The number of slabs.

.. cpp:function:: std::size_t SlabArenaPool::total_slab_bytes() const

  The total size of all slabs (the same as ``slab_allocation_stats().slab_bytes``).

.. cpp:function:: std::size_t SlabArenaPool::current_slab_remaining() const

  Bytes left to carve in the current slab.

PoolAllocator
"""""""""""""

``PortsOfCall::PoolAllocator<T, Pool>`` is a ``std::allocator`` compatible allocator that draws from a ``SlabArenaPool``, so standard containers can use the pool:

.. code-block:: cpp

  using HostPool = PortsOfCall::SlabArenaPool<Kokkos::HostSpace>;
  template <class T>
  using PoolVector = std::vector<T, PortsOfCall::PoolAllocator<T, HostPool>>;

  PoolVector<double> a(1000);                    // uses HostPool::instance()

  HostPool local;
  PoolVector<int> b{PortsOfCall::PoolAllocator<int, HostPool>(local)}; // uses local

``PoolAllocator`` should only be used on host, and since containers construct their elements through it, ``Pool`` must use host-accessible memory.

.. cpp:function:: PoolAllocator::PoolAllocator() noexcept

  Uses ``Pool::instance()``.

.. cpp:function:: explicit PoolAllocator::PoolAllocator(Pool &pool) noexcept

  Uses ``pool``, which must outlive the allocator and every allocation made through it.

.. cpp:function:: T *PoolAllocator::allocate(std::size_t n)

  Allocates space for ``n`` objects of type ``T``, aligned to ``alignof(T)``. Throws ``std::bad_array_new_length`` if ``n > max_size()``.

.. cpp:function:: void PoolAllocator::deallocate(T *p, std::size_t n = 0) noexcept

  Returns ``p`` to the pool. ``n`` is ignored.

.. cpp:function:: Pool *PoolAllocator::pool() const noexcept

  The pool the allocator draws from, e.g. for reading its statistics.

Two ``PoolAllocator`` objects compare equal when they use the same pool.
The allocator follows a container on move assignment and swap, but not on copy assignment.

Device scratch memory
"""""""""""""""""""""

A common device pattern is to give each team or thread its own equally sized slice of scratch memory, carved out of one pooled allocation, and to sub-allocate arrays within each slice inside the kernel.
``SlabArenaPool`` provides three nested types for this.

``ScratchLayout`` describes the arrays in one slice, computing their offsets on host:

.. cpp:function:: template <class T> int ScratchLayout::add_array(std::size_t n)

  Appends an array of ``n`` objects of type ``T``, aligned to ``alignof(T)``, and returns its id.
  ``layout[id]`` gives the array's ``offset``, ``bytes``, and ``align`` within the slice.

.. cpp:function:: std::size_t ScratchLayout::bytes_per_instance() const

  The size of one slice, rounded up so consecutive slices stay aligned.

.. cpp:function:: std::size_t ScratchLayout::alignment() const

  The strictest alignment of the arrays.

``reserve_device_scratch`` allocates the slices from the pool and returns a ``Reservation``:

.. cpp:function:: Reservation SlabArenaPool::reserve_device_scratch(const ScratchLayout &layout, std::size_t instances, std::size_t alignment = alignof(std::max_align_t))

.. cpp:function:: Reservation SlabArenaPool::reserve_device_scratch(std::size_t bytes_per_instance, std::size_t instances, std::size_t alignment = alignof(std::max_align_t))

  Allocates ``instances`` slices, each aligned to ``alignment``.
  The returned ``Reservation`` holds ``base``, ``instances``, ``total_bytes``, and the slice stride ``bytes_per_instance``, which may be larger than requested so that every slice is aligned.
  In Kokkos builds, ``Reservation::view()`` wraps the block in an unmanaged ``Kokkos::View<std::byte *>``.
  Free the block with ``pool.free_bytes(reservation.base)``.

``DeviceBump`` is a bump allocator over one slice that can be built and used inside a kernel:

.. cpp:function:: DeviceBump::DeviceBump(void *p, std::size_t bytes)

  Sub-allocates from the ``bytes`` bytes starting at ``p``.

.. cpp:function:: void *DeviceBump::alloc_bytes(std::size_t bytes, std::size_t alignment)

  Returns the next ``bytes`` bytes aligned to ``alignment`` (a power of two), or ``nullptr`` if the slice is full.

.. cpp:function:: template <class T> T *DeviceBump::alloc(std::size_t n)

  Returns space for ``n`` objects of type ``T``, or ``nullptr``. ``allocate<T>(n)`` is a synonym.

.. cpp:function:: void DeviceBump::reset()

  Makes the whole slice available again. Individual allocations cannot be freed.

For example:

.. code-block:: cpp

  using DevPool = PortsOfCall::SlabArenaPool<Kokkos::DefaultExecutionSpace::memory_space>;
  auto &pool = DevPool::instance();

  DevPool::ScratchLayout layout;
  const int a_id = layout.add_array<double>(32);
  const int b_id = layout.add_array<int>(16);
  auto reservation = pool.reserve_device_scratch(layout, nteams);

  auto scratch = reservation.view();
  const std::size_t stride = reservation.bytes_per_instance;
  const std::size_t off_a = layout[a_id].offset;
  const std::size_t off_b = layout[b_id].offset;

  Kokkos::parallel_for("work", nteams, KOKKOS_LAMBDA(const int i) {
    std::byte *slice = scratch.data() + i * stride;
    double *a = reinterpret_cast<double *>(slice + off_a);
    int *b = reinterpret_cast<int *>(slice + off_b);
    // ... or, without a layout:
    //   DevPool::DeviceBump arena(slice, stride);
    //   double *a = arena.alloc<double>(32);
  });

  pool.free_bytes(reservation.base);

.. note::

  This file was generated in part with generative AI.
