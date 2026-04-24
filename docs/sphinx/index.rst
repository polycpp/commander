commander
=========

**Command-line option parser for polycpp**

Define options, arguments, subcommands, action handlers, and lifecycle hooks with the same fluent API as the npm commander package. Includes help formatting, "did you mean?" suggestions, environment-variable binding, custom parsers, and Promise-based async actions on top of polycpp's event loop.

.. code-block:: cpp

   #include <iostream>
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
               if (opts["shout"].asBool())
                   for (auto& c : msg) c = std::toupper(static_cast<unsigned char>(c));
               std::cout << msg << '\n';
           });

       prog.parse();
   }

.. grid:: 2

   .. grid-item-card:: Drop-in familiarity
      :margin: 1

      Mirrors the npm ``commander`` package — ``.option()``, ``.argument()``, ``.command()``, ``.action()``, and ``.parse()`` all keep their JS semantics, including camelCase attribute names, ``--no-*`` negation, variadic ``<values...>``, and chained subcommands.

   .. grid-item-card:: C++20 native
      :margin: 1

      Header-only where possible, zero-overhead abstractions, ``constexpr``
      and ``std::string_view`` throughout.

   .. grid-item-card:: Tested
      :margin: 1

      Ported from commander.js's own test suite — 600+ assertions across arguments, options, error reporting, help layout, suggestions, and subcommand dispatch.

   .. grid-item-card:: Plays well with polycpp
      :margin: 1

      Uses the same JSON value, error, and typed-event types as the rest of
      the polycpp ecosystem — no impedance mismatch.

Getting started
---------------

.. code-block:: bash

   # With FetchContent (recommended)
   FetchContent_Declare(
       polycpp_commander
       GIT_REPOSITORY https://github.com/polycpp/commander.git
       GIT_TAG        master
   )
   FetchContent_MakeAvailable(polycpp_commander)
   target_link_libraries(my_app PRIVATE polycpp::commander)

:doc:`Installation <getting-started/installation>` · :doc:`Quickstart <getting-started/quickstart>` · :doc:`Tutorials <tutorials/index>` · :doc:`API reference <api/index>`

.. toctree::
   :hidden:
   :caption: Getting started

   getting-started/installation
   getting-started/quickstart

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
