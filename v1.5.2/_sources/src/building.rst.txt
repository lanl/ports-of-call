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
subdirectory in your project. `ports-of-call` defines the CMake target
``ports-of-call::ports-of-call``, which sets the appropriate include paths.

For in-tree builds, you can define exactly one of the preprocessor macros
``PORTABILITY_STRATEGY_KOKKOS``, ``PORTABILITY_STRATEGY_CUDA`` or ``PORTABILITY_STRATEGY_NONE``.
If none are set, the default is ``PORTABILITY_STRATEGY_NONE``.

Note that it is the users repsonsibility to ensure that the appropriate 
portability strategy is set. Furthermore, `ports-of-call` does not engage in
any dependency resolution, so the user is further required to ensure that 
the appropriate dependencies are available when building.

CMake
^^^^^^

The prefered apporach for integratting into your project is is to use 
the CMake `find_package` pattern. If `ports-of-call` has been installed and
is visible to the environment

.. code-block:: cmake
  find_package(ports-of-call)
  target_link_libraries(myProj ports-of-call::ports-of-call)
  target_compile_definitions(myProj PORTABILITY_STRATEGY_KOKKOS)

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
