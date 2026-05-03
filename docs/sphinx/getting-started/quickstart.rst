Quickstart
==========

This page builds a minimal commander program from an empty directory.
Create two files, run the commands exactly as shown, then jump to
:doc:`core-concepts` for the mental model or :doc:`../tutorials/index`
for task-oriented walkthroughs.

We'll wire up a tiny ``serve`` CLI with one flag, one value-bearing
option, one positional argument, and defaults for both values.

Create ``CMakeLists.txt``
-------------------------

.. code-block:: cmake

   cmake_minimum_required(VERSION 3.20)
   project(serve_demo LANGUAGES CXX)
   set(CMAKE_CXX_STANDARD 20)
   set(CMAKE_CXX_STANDARD_REQUIRED ON)

   include(FetchContent)
   FetchContent_Declare(
       polycpp_commander
       GIT_REPOSITORY https://github.com/polycpp/commander.git
       GIT_TAG        v1.0.0
   )
   FetchContent_MakeAvailable(polycpp_commander)

   add_executable(serve_demo main.cpp)
   target_link_libraries(serve_demo PRIVATE polycpp::commander)

Create ``main.cpp``
-------------------

.. code-block:: cpp

   #include <iostream>
   #include <string>
   #include <polycpp/commander/commander.hpp>
   #include <polycpp/process.hpp>

   int main(int argc, char** argv) {
       polycpp::process::init(argc, argv);

       auto& prog = polycpp::commander::program();
       prog.name("serve")
           .description("serve files over HTTP")
           .version("1.0.0");

       prog.option("-v, --verbose", "log every request")
           .option("-p, --port <port>", "port to listen on",
                   polycpp::JsonValue(std::string("8080")))
           .argument("[dir]", "directory to serve",
                     polycpp::JsonValue(std::string(".")));

       prog.action([](const auto& args, const auto& opts, auto&) {
           const auto verbose = opts["verbose"];
           const bool isVerbose = verbose.isBool() && verbose.asBool();
           std::cout << "serving " << args[0].asString()
                     << " on port " << opts["port"].asString()
                     << (isVerbose ? " (verbose)" : "")
                     << '\n';
       });

       prog.parse();
       return 0;
   }

Build and run
-------------

.. code-block:: bash

   cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug
   cmake --build build
   ./build/serve_demo -v --port 3000 ./public
   ./build/serve_demo --help

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

3. ``.option("-p, --port <port>", ...)`` declares a value-bearing
   option; ``<port>`` means the value is required when the flag is
   used. ``[number]`` would make it optional, ``<numbers...>`` variadic,
   and ``--no-verbose`` would flip a boolean.

4. ``.argument("[dir]", ...)`` declares a positional. Square brackets
   mark it optional, and the second argument is the default when nothing
   is passed.

5. The action lambda receives processed args as a
   ``std::vector<polycpp::JsonValue>`` and options as a single
   ``polycpp::JsonValue`` keyed by camelCased attribute name
   (``--dry-run`` becomes ``opts["dryRun"]``).

6. This quickstart keeps ``--port`` as a string to stay copy-paste simple.
   If you want integer validation, add an ``argParser`` as shown in
   :doc:`../guides/parse-numeric-option`.

Next steps
----------

- :doc:`core-concepts` — ownership, parsing modes, ``JsonValue``, and exits.
- :doc:`../tutorials/index` — step-by-step walkthroughs of common tasks.
- :doc:`../guides/index` — short how-tos for specific problems.
- :doc:`../api/index` — every public type, function, and option.
- :doc:`../examples/index` — runnable programs you can drop into a sandbox.
