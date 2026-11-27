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

The docs are built but not deployed on PRs from forks, and the
internal tests will not be run automatically. So when the code is
ready for merge, you must ask a project maintainer to trigger the
remaining tests for you.

Expectations for code review
-----------------------------

From the perspective of the contributor
````````````````````````````````````````

Code review is an integral part of the development process
for ``ports-of-call``. You can expect at least one, perhaps many,
core developers to read your code and offer suggestions.
You should treat this much like scientific or academic peer review.
You should listen to suggestions but also feel entitled to push back
if you believe the suggestions or comments are incorrect or
are requesting too much effort.

Reviewers may offer conflicting advice, if this is the case, it's an
opportunity to open a discussion and communally arrive at a good
approach. You should feel empowered to argue for which of the
conflicting solutions you prefer or to suggest a compromise. If you
don't feel strongly, that's fine too, but it's best to say so to keep
the lines of communication open.

Big contributions may be difficult to review in one piece and you may
be requested to split your pull request into two or more separate
contributions. You may also receive many "nitpicky" comments about
code style or structure. These comments help keep a broad codebase,
with many contributors uniform in style and maintainable with
consistent expectations accross the code base. While there is no
formal style guide for now, the regular contributors have a sense for
the broad style of the project. You should take these stylistic and
"nitpicky" suggestions seriously, but you should also feel free to
push back.

As with any creative endeavor, we put a lot of ourselves into our
code. It can be painful to receive criticism on your contribution and
easy to take it personally. While you should resist the urge to take
offense, it is also partly code reviewer's responsiblity to create a
constructive environment, as discussed below.

Expectations of code reviewers
````````````````````````````````

A good code review builds a contribution up, rather than tearing it
down. Here are a few rules to keep code reviews constructive and
congenial:

* You should take the time needed to review a contribution and offer
  meaningful advice. Unless a contribution is very small, limit
  the times you simply click "approve" with a "looks good to me."

* You should keep your comments constructive. For example, rather than
  saying "this pattern is bad," try saying "at this point, you may
  want to try this other pattern."

* Avoid language that can be misconstrued, even if it's common
  notation in the commnunity. For example, avoid phrases like "code
  smell."

* Explain why you make a suggestion. In addition to saying "try X
  instead of Y" explain why you like pattern X more than pattern Y.

* A contributor may push back on your suggestion. Be open to the
  possibility that you're either asking too much or are incorrect in
  this instance. Code review is an opportunity for everyone to learn.

* Don't just highlight what you don't like. Also highlight the parts
  of the pull request you do like and thank the contributor for their
  effort.

Some notes on style and code architecture
``````````````````````````````````````````

* A major influence on code style and architecture is the
  `ten rules for developing safety-critical code`_, by Gerard Holzmann.
  Safety critical code is code that exists in a context where failure
  implies serious harm. A flight controler on an airplane or
  spacecraft or the microcontroller in a car are examples of
  safety-critical contexts. ``ports-of-call`` is not safety-critical
  but many of the coding habits advocated for by Holzmann produce
  long-lived, easy to understand, easy to parse, and easy to maintain code.
  And we take many of the rules to heart. Here are a few that are most
  relevant to ``ports-of-call``. They have been adapted slightly to
  our context.

    #. Avoid complex flow constructs such as gotos.

    #. All loops must have fixed bounds. This prevents runaway
       code. (Note this implies that as a general rule, one should use
       ``for`` loops, not ``while`` loops. It also implies one should
       keep recursion to a minimum.)

    #. Heap memory allocation should only be performed at
       initialization. Heap memory de-allocation should only be
       performed at cleanup.

    #. Restrict the length of functions to a single printed page.

    #. Restrict the scope of data to the smallest possible.

    #. Limit pointer use to a single dereference. Avoid pointers of
       pointers when possible.

    #. Be compiler warning aware. Try to address compiler warnings as
       they come up.

.. _ten rules for developing safety-critical code: http://web.eecs.umich.edu/~imarkov/10rules.pdf

* ``ports-of-call`` is a modern C++ code
  and both standard template library capabilities and template
  metaprogramming are leveraged frequently. This can sometimes make
  parsing the code difficult. If you see something you don't
  understand, ask. It may be it can be refactored to be more simple or
  better documented.
