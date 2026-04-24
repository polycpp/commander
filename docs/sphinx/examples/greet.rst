Greet with flag, option, and argument
=====================================

A four-flag CLI that exercises the most common option shapes — a
boolean flag (``-s, --shout``), a value-bearing option with a default
(``--lang <code>``), and one required positional (``<name>``).

.. literalinclude:: ../../../examples/greet.cpp
   :language: cpp
   :linenos:

Build and run:

.. code-block:: bash

   cmake -B build -G Ninja -DPOLYCPP_COMMANDER_BUILD_EXAMPLES=ON
   cmake --build build --target greet
   ./build/examples/greet --lang es --shout world
   # HOLA, WORLD
