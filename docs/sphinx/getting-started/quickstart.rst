Quickstart
==========

This page walks through a minimal commander program end-to-end. Copy the
snippet, run it, then jump to :doc:`../tutorials/index` for task-oriented
walkthroughs or :doc:`../api/index` for the full reference.

We'll wire up a tiny ``serve`` CLI with one flag, one value-bearing
option, one positional argument, and a default value — enough to hit
every part of the parser.

Full example
------------

.. code-block:: cpp

   #include <iostream>
   #include <polycpp/commander/commander.hpp>
   #include <polycpp/process.hpp>

   int main(int argc, char** argv) {
       polycpp::process::init(argc, argv);

       auto& prog = polycpp::commander::program();
       prog.name("serve")
           .description("serve files over HTTP")
           .version("1.0.0");

       prog.option("-v, --verbose", "log every request")
           .option("-p, --port <number>", "port to listen on", polycpp::JsonValue(8080))
           .argument("[dir]", "directory to serve", polycpp::JsonValue("."));

       prog.action([](const auto& args, const auto& opts, auto&) {
           std::cout << "serving " << args[0].asString()
                     << " on port " << opts["port"].asInt()
                     << (opts["verbose"].asBool() ? " (verbose)" : "")
                     << '\n';
       });

       prog.parse();
   }

Compile it with the same CMake wiring from :doc:`installation`:

.. code-block:: bash

   cmake -B build -G Ninja
   cmake --build build
   ./build/my_app -v --port 3000 ./public

Expected output:

.. code-block:: text

   serving ./public on port 3000 (verbose)

What just happened
------------------

1. ``polycpp::process::init(argc, argv)`` hands the real ``argv`` to
   polycpp so ``commander::program()`` can reach it on
   :cpp:func:`polycpp::commander::Command::parse` without arguments.

2. :cpp:func:`polycpp::commander::program` returns a singleton
   :cpp:class:`polycpp::commander::Command` — equivalent to
   ``require('commander').program`` in Node. Call
   :cpp:func:`polycpp::commander::createCommand` if you prefer to own
   the instance yourself.

3. ``.option("-p, --port <number>", ...)`` declares a value-bearing
   option; ``<number>`` means the value is required when the flag is
   used. ``[number]`` would make it optional, ``<numbers...>`` variadic,
   and ``--no-verbose`` would flip a boolean.

4. ``.argument("[dir]", ...)`` declares a positional. Square brackets
   mark it optional, and the second argument is the default when nothing
   is passed.

5. The action lambda receives processed args as a
   ``std::vector<polycpp::JsonValue>`` and options as a single
   ``polycpp::JsonValue`` keyed by camelCased attribute name
   (``--dry-run`` becomes ``opts["dryRun"]``).

Next steps
----------

- :doc:`../tutorials/index` — step-by-step walkthroughs of common tasks.
- :doc:`../guides/index` — short how-tos for specific problems.
- :doc:`../api/index` — every public type, function, and option.
- :doc:`../examples/index` — runnable programs you can drop into a sandbox.
