Throw instead of calling ``process::exit``
==========================================

**When to reach for this:** unit tests, embedded scenarios, or a
library-like use where the host app wants to decide what happens on an
error.

Call :cpp:func:`exitOverride()
<polycpp::commander::Command::exitOverride>` once. With no argument, it
swaps the default exit with a throw of
:cpp:class:`CommanderError <polycpp::commander::CommanderError>`:

.. code-block:: cpp

   prog.exitOverride();
   try {
       prog.parse({"--nope"}, {.from = "user"});
   } catch (const polycpp::commander::CommanderError& e) {
       // e.code == "commander.unknownOption"
       // e.exitCode suggests an exit status
   }

``CommanderError::code`` is a stable identifier — the full list
includes ``commander.helpDisplayed``, ``commander.version``,
``commander.missingArgument``, ``commander.missingMandatoryOptionValue``,
``commander.unknownOption``, ``commander.unknownCommand``,
``commander.invalidArgument``, ``commander.excessArguments``.

Pass a callback to customise what happens:

.. code-block:: cpp

   prog.exitOverride([](const polycpp::commander::CommanderError& e) {
       myapp::log::error({{"code", e.code}, {"msg", e.what()}});
       std::exit(e.exitCode);
   });

Two codes are benign control-flow rather than errors —
``commander.helpDisplayed`` and ``commander.version`` fire after help
or version output and normally end the process. If you're running
commander from a REPL, handle those specifically and keep going.
