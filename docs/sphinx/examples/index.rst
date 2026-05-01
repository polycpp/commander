Examples
========

Self-contained programs exercising the main features of commander. Each
example compiles against the public API only — no private headers, no
non-exported targets.

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

Replace ``greet`` with the slug of any example listed above.
Examples are built by default for standalone builds and only when
``POLYCPP_COMMANDER_BUILD_EXAMPLES=ON`` is passed when this repo is
embedded as a CMake subproject.
