.. _sphinx-doc:

.. _Sphinx CheatSheet: https://thomas-cokelaer.info/tutorials/sphinx/rest_syntax.html

How to Use Sphinx for Writing Docs
===================================

How to Get the Dependencies
---------------------------

With your favorite python package manager, e.g., ``pip``, install
``sphinx``, ``spinx_multiversion``, and ``sphinx_rtd_theme``. For
example:

.. code-block::

  pip install sphinx
  pip install sphinx_multiversion
  pip install sphinx_rtd_theme

How to Build .rst into .html
----------------------------

After you have the dependencies in your environment, then simply build your documentation as the following:

.. code-block::

  make html

.. note:: 
   You can view the documentation webpage locally on your web browser by passing in the URL as :code:`file:///path/to/spiner/doc/sphinx/_build/html/index.html`

How to Deploy
-------------

#. Submit a PR with your .rst changes for documentation on `Github ports-of-call`_
#. Get your PR reviewed and merged into main
#. Make sure the :code:`pages` CI job passes in the CI pipeline

.. _Github ports-of-call: https://github.com/lanl/ports-of-call

As soon as the PR is merged into main, this will trigger the Pages deployment automatically if the :code:`pages` CI job passes.

Documentation is available on `github-pages`_.

.. _github-pages: https://lanl.github.io/ports-of-call/

More Info.
----------

* `Sphinx Installation`_

.. _Sphinx Installation: https://www.sphinx-doc.org/en/master/usage/installation.html

* `Sphinx reStructuredText Documentation`_

.. _Sphinx reStructuredText Documentation: https://www.sphinx-doc.org/en/master/usage/restructuredtext/basics.html
