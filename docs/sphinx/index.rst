commander
=========

**Command-line option parser for polycpp**

Build command-line interfaces with the same fluent API as the npm
``commander`` package, but in modern C++. The library covers options,
arguments, subcommands, help formatting, environment-variable binding,
custom parsers, "did you mean?" suggestions, and Promise-based async
actions on top of polycpp's event loop.

.. code-block:: cpp

   #include <cctype>
   #include <iostream>
   #include <string>
   #include <polycpp/commander/commander.hpp>
   #include <polycpp/process.hpp>

   int main(int argc, char** argv) {
       polycpp::process::init(argc, argv);

       auto& prog = polycpp::commander::program();
       prog.name("greet")
           .description("print a greeting")
           .version("1.0.0")
           .option("-s, --shout", "uppercase the output")
           .argument("<name>", "who to greet")
           .action([](const auto& args, const auto& opts, auto&) {
               std::string msg = "hello, " + args[0].asString();
               const auto shout = opts["shout"];
               if (shout.isBool() && shout.asBool())
                   for (auto& c : msg)
                       c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
               std::cout << msg << '\n';
           });

       prog.parse();
       return 0;
   }

.. grid:: 2

   .. grid-item-card:: Drop-in familiarity
      :margin: 1

      Mirrors the npm ``commander`` package — ``.option()``, ``.argument()``, ``.command()``, ``.action()``, and ``.parse()`` all keep their JS semantics, including camelCase attribute names, ``--no-*`` negation, variadic ``<values...>``, and chained subcommands.

   .. grid-item-card:: C++20 native
      :margin: 1

      Mostly inline public headers with a small library target
      (``polycpp::commander``). It builds on ``polycpp::events::EventEmitter``,
      ``polycpp::JsonValue``, and ``polycpp::Promise`` so it composes naturally
      with the rest of the polycpp ecosystem.

   .. grid-item-card:: Tested
      :margin: 1

      Ported from commander.js's own test suite, with GoogleTest coverage across arguments, options, error reporting, help layout, suggestions, executable subcommands, and Windows/MSVC lookup behavior.

   .. grid-item-card:: Plays well with polycpp
      :margin: 1

      Uses the same JSON value, error, and typed-event types as the rest of
      the polycpp ecosystem — no impedance mismatch.

Getting started
---------------

.. code-block:: cmake

   cmake_minimum_required(VERSION 3.20)
   project(my_app LANGUAGES CXX)
   set(CMAKE_CXX_STANDARD 20)
   set(CMAKE_CXX_STANDARD_REQUIRED ON)

   include(FetchContent)
   FetchContent_Declare(
       polycpp_commander
       GIT_REPOSITORY https://github.com/polycpp/commander.git
       GIT_TAG        v1.0.0
   )
   FetchContent_MakeAvailable(polycpp_commander)

   add_executable(my_app main.cpp)
   target_link_libraries(my_app PRIVATE polycpp::commander)

Choose your path
----------------

- :doc:`Installation <getting-started/installation>`: compiler support, the
  recommended FetchContent setup, local-clone workflows, and build options.
- :doc:`Quickstart <getting-started/quickstart>`: the smallest runnable CLI you
  can build from an empty directory.
- :doc:`Core concepts <getting-started/core-concepts>`: how command ownership,
  ``JsonValue`` option storage, parsing modes, and error handling fit together.
- :doc:`Tutorials <tutorials/index>`: longer walkthroughs for multi-command
  CLIs, async actions, and help customization.
- :doc:`How-to guides <guides/index>`: short answers for one specific problem.
- :doc:`Examples <examples/index>`: runnable programs pulled directly from the
  repository.
- :doc:`API reference <api/index>`: the full public surface from the headers.

.. toctree::
   :hidden:
   :caption: Getting started

   getting-started/installation
   getting-started/quickstart
   getting-started/core-concepts

.. toctree::
   :hidden:
   :caption: Tutorials

   tutorials/index

.. toctree::
   :hidden:
   :caption: How-to guides

   guides/index

.. toctree::
   :hidden:
   :caption: API reference

   api/index

.. toctree::
   :hidden:
   :caption: Examples

   examples/index
