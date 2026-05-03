Multi-command ``todo`` CLI
==========================

Three subcommands — ``add``, ``list``, ``done`` — sharing a global
``--verbose`` flag, with error suggestions for unknown commands. The
task store is in-process; swap it for a real backing store when
adapting this to your app. Because the demo store lives only for the
life of one process, every separate shell invocation starts empty.

.. literalinclude:: ../../../examples/todo.cpp
   :language: cpp
   :linenos:

Build and run:

.. code-block:: bash

   cmake -B build -G Ninja -DPOLYCPP_COMMANDER_BUILD_EXAMPLES=ON
   cmake --build build --target polycpp_commander_example_todo
   ./build/examples/todo add "write docs"
   ./build/examples/todo --help
