.. _developing:

Developing Ports of Call
========================

As Ports of Call is only a few small header files, not much is
required. We do require the files be formatted using
``clang-format``. The format version is pinned 12. Please call
```clang-format`` on your files before merging them.

You may do so automatically either by making the ``format_ports``
target in the build system, e.g.,

.. code-block:: bash

  mkdir -p ports-of-call/build
  cd ports-of-call/build
  cmake ..
  make format_ports

or you may call the ``format.sh`` script in the ``scripts``
directory. This script supports two environment variables, ``CFM`` and
``VERBOSE``. ``CFM`` is the path (or name) of your ``clang-format``
executable. ``VERBOSE=1`` will cause it to output all files it
modifies.
