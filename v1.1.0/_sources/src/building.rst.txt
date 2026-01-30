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

Spack
^^^^^^

.. warning::
  The spack build is currently experimental. 
  Please report problems you have as github issues.

Although the spackage has not yet made it to the main `Spack`_
repositories, we provide a spackage for Ports of Call within the
the source repository. If you have spack installed,
simply call

.. _Spack: https://spack.io/

.. code-block:: bash

  spack repo add ports-of-call/spack-repo
  spack install ports-of-call

The spack repo supports one variant, ``+doc``, which adds tooling for
building the docs.
