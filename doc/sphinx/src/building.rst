.. _building:

Building Ports of Call
========================

Since Ports of Call is header only, there is no build step. However,
it can be included in your project in multiple ways: it can be
pre-installed, or used in-tree in a larger project.

Installation
^^^^^^^^^^^^^

Clone Ports of call and install it with:

.. code-block:: bash

  git clone git@github.com:lanl/ports-of-call.git
  mkdir -p ports-of-call/build
  cd ports-of-call/build
  cmake -DCMAKE_INSTALL_PREFIX=/path/to/install/directory ..
  make install

Including in-tree
^^^^^^^^^^^^^^^^^^

If you want to include Ports of Call in a project in-tree, you can
easily just include the two header files. Alternatively, our ``cmake``
build system supports in-tree builds. Simply add the repository as a
subdirectory in your project. We ``ports-of-call::ports-of-call`` target,
which sets the appropriate include paths.

For in-tree builds, you can set the configure time option
``PORTABILITY_STRATEGY`` to ``Kokkos``, ``Cuda`` or ``None`` to set
the equivalent preprocessor macro. The default is ``None``.

By default ``cmake`` keeps a registry of packages with install logic
that it has built in a user's home directory. Because
``ports-of-call`` fixes portability strategy at ``cmake`` configure
time, this can conflict with in-tree builds. A parent code that
includes ``ports-of-call`` may find the wrong build by default, rather
than the version that it includes explicitly in the source tree. To
resolve this, we recommend disabling ``cmake``'s package registry
machinery via:

.. code-block:: cmake

  set(CMAKE_FIND_USE_PACKAGE_REGISTRY OFF CACHE BOOL "" FORCE)
  set(CMAKE_FIND_USE_SYSTEM_PACKAGE_REGISTRY OFF CACHE BOOL "" FORCE)

If, on the other hand, you install the dependencies of your code
one-by-one, you should not disable the package registry. If you
encounter an issue where your configuration settings for
``ports-of-call`` don't seem to stick when building a code, you might
attempt disabling the package registry at configure time via

.. code-block:: cmake

  -DCMAKE_FIND_USE_PACKAGE_REGISTRY=OFF -DCMAKE_FIND_USE_SYSTEM_PACKAGE_REGISTRY=OFF

For more details, see the documentation on the `cmake package
registry`_.

.. _cmake package registry: https://cmake.org/cmake/help/latest/manual/cmake-packages.7.html#package-registry

Spack
^^^^^^

Ports of call is available within `Spack`_.  If you have spack
installed, simply call

.. _Spack: https://spack.io/

.. code-block:: bash

  spack install ports-of-call

We also provide a spackage for Ports of Call within the
the source repository. To use it, call:

.. _Spack: https://spack.io/

.. code-block:: bash

  spack repo add ports-of-call/spack-repo
  spack install ports-of-call

The spack repo supports one variant, ``+doc``, which adds tooling for
building the docs.
