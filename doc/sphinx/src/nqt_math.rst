Not-quite-transcendental math
=============================

``nqt_math.hpp`` provides fast, invertible approximations to base-2
logarithms and powers of two.  These functions are useful when an
application needs approximately logarithmic spacing and an
inexpensive, but exact, inverse, rather than the exact transcendental
functions.

The implementation is based on the underlying structure of floating
point numbers:

.. math::

   x = m 2^e,

where ``m`` is the mantissa and ``e`` is the integer exponent.  The
NQT functions approximate the mantissa contribution while preserving
the useful properties of logarithmic spacing and invertibility.

Background and references
-------------------------

The NQT approach and its motivation are described in:

* Jonah M. Miller, Joshua C. Dolence, and Daniel Holladay,
  ``Not-Quite Transcendental Functions and their Applications``,
  `arXiv:2206.08957 <https://arxiv.org/abs/2206.08957>`_.
* Peter C. Hammond, Jacob M. Fields, Jonah M. Miller, and Brandon L. Barker,
  ``Not-quite-transcendental Functions for Logarithmic Interpolation of
  Tabulated Data``, *The Astrophysical Journal Supplement Series* 277,
  65 (2025),
  `doi:10.3847/1538-4365/adbbd4 <https://doi.org/10.3847/1538-4365/adbbd4>`_,
  also available as `arXiv:2501.05410 <https://arxiv.org/abs/2501.05410>`_.

The first paper introduces the general not-quite-transcendental construction;
the second develops the higher-order form used by ``NQT::O2`` for logarithmic
interpolation of tabulated data.

API organization
----------------

The public API is organized first by approximation order and then by
implementation strategy:

* ``PortsOfCall::NQT::O1`` contains the first-order, piecewise-linear
  approximation.
* ``PortsOfCall::NQT::O2`` contains the second-order approximation, which
  provides a smoother derivative.
* ``Portable`` implementations use ``frexp`` and ``ldexp`` and do not inspect
  the binary representation of a ``double``.
* ``Aliased`` implementations use the IEEE-754 binary representation
  through the ``FP64LE`` helpers and are intended for platforms with
  the expected 64-bit floating-point representation. These methods are
  faster than the ``Portable`` implementations.

Each implementation namespace provides ``lg`` and ``pow2``:

.. code-block:: cpp

   #include <ports-of-call/nqt_math.hpp>

   const double y = PortsOfCall::NQT::O2::Portable::lg(x);
   const double x_round_trip = PortsOfCall::NQT::O2::Portable::pow2(y);

The first-order namespaces also provide matched ``asinh`` and ``sinh``
approximations:

.. code-block:: cpp

   const double y = PortsOfCall::NQT::O1::Aliased::sinh(x);
   const double x_round_trip = PortsOfCall::NQT::O1::Aliased::asinh(y);

For every implementation, ``lg`` requires a finite positive argument.  The
``pow2`` functions accept finite arguments in the range ``[-1022, 1024]``.
The aliased ``lg`` functions additionally reject subnormal values because the
integer representation technique is not valid for them.

The functions are decorated with the portability macros and can therefore be
called from portable host/device code.  ``Portable`` describes the numerical
implementation strategy; it does not mean that the ``Aliased`` functions are
host-only.
