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
8. ``PORTABLE_MALLOC()``, ``PORTABLE_FREE()``: A wrapper for kokkos_malloc or cudaMalloc, or raw malloc and equivalent free.

At compile time, you define
``PORTABILITY_STRATEGY_{KOKKOS,CUDA,NONE}`` (if you don't define it,
it defaults to NONE). The above macros then behave as expected. In
particular, ``PORTABLE_FUNCTION`` and friends resolve to ``__host__
__device__`` decorators as appropriate.

There are several headers in this library, for different use cases.

portability.hpp
^^^^^^^^^^^^^^^^

``portability.hpp`` provides the above-mentioned macros for decorating
functions. Also provides loop abstractions that can be leveraged by a
code. These loop abstractions are of the form:

.. cpp:function:: void portableFor(const char *name, int start, int stop, Function Function)

where ``Function`` is a template parameter and should be set to a
functor that takes one index, e.g., an index in an array. For example:

.. code-block:: cpp

  portableFor("Example", 0, 5,
    PORTABLE_LAMBDA(int i) {
      printf("hello from thread %d\n", i);
  });

``start`` is inclusive, ``stop`` is exclusive. Up to five-dimensional
``portableFor`` loops are available. For example:

.. code-block:: cpp

  template <typename Function>
  void portableFor(const char *name, int startb, int stopb, int starta, int stopa,
    int startz, int stopz, int starty, int stopy, int startx,
    int stopx, Function function) {

We also provide ``portableReduce``, however the functionality is very
limited. The syntax is:

.. code-block::

  template <typename Function, typename T>
  void portableReduce(const char *name, int starta, int stopa, int startz,
    int stopz, int starty, int stopy, int startx, int stopx,
    Function function, T &reduced) {

where ``Function`` now takes as many indices are required and
``reduced`` as arguments.

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

  template <typename DataType>
  PORTABLE_FUNCTION constexpr DataType reldiff(const DataType actual, const DataType expected)

returns the relative difference between the expected value and the actual
result (:math:`\left|(A-E)/E\right|`).  If the expected and actual values are
both zero, this is defined to be a relative difference of 0%.  If the expected
value is zero and the actual value is non-zero, this is defined to be a
relative difference of 100%.  Unlike ``ratio``, ``reldiff`` is tuned to get
meaningful results for the use-case of unit tests, rather than for performance.

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

You can always access the "flat" array by simply using the 1D accessor:

.. code-block:: cpp

  my_3d_array(6) = 2.0;

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


``PortsOfCall::span`` is implements ``std::span`` for C++17 (uses native implmentation in C++20) 
as a view over contiguous data. ``span`` may have compile-time static extent, or a dynamic extent. 
``span`` provides iterator functions similar to containers.

.. code-block:: cpp
  
 int arr[] = {1, 2, 3};
 auto s = span{arr};
 for(auto & i : s)
 {
   i -= 1;
 }

``span::subspan`` returns a span over a subrange. Element access uses ``span::operator[]``. For 
more information, see `C++ reference page <https://en.cppreference.com/w/cpp/container/span>`_.



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
