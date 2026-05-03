Examples
========

Self-contained programs exercising the main features of commander. Each
example compiles against the public API only — no private headers, no
non-exported targets.

- :doc:`greet`: a small single-command CLI with a boolean flag, a value-bearing
  option, and one required positional.
- :doc:`todo`: a git-style multi-command CLI whose task store is intentionally
  in-memory, so each process starts from an empty list.

.. toctree::
   :maxdepth: 1

   greet
   todo

Running an example
------------------

From the repository root:

.. code-block:: bash

   cmake -B build -G Ninja -DPOLYCPP_COMMANDER_BUILD_EXAMPLES=ON
   cmake --build build --target polycpp_commander_example_greet
   ./build/examples/greet alice

Replace ``greet`` with the example slug to form the build target name
``polycpp_commander_example_<slug>``.
Examples are built by default for standalone builds and only when
``POLYCPP_COMMANDER_BUILD_EXAMPLES=ON`` is passed when this repo is
embedded as a CMake subproject.
