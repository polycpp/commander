Build a git-style multi-command CLI
===================================

**You'll build:** a ``todo`` CLI with three subcommands (``add``,
``list``, ``done``), a global ``--verbose`` flag, environment-variable
fallbacks, and a "did you mean?" suggestion when the user mistypes a
subcommand.

**You'll use:** :cpp:class:`polycpp::commander::Command`,
:cpp:func:`polycpp::commander::Command::command`,
:cpp:func:`polycpp::commander::Command::action`, and the
``showSuggestionAfterError`` setting.

**Prerequisites:** a working ``polycpp::commander`` install — see
:doc:`../getting-started/installation`.

This tutorial focuses on commander wiring. The storage layer is sketched as a
``tasks::`` namespace so the CLI flow stays readable; for a fully runnable
program, see :doc:`../examples/todo`.

Step 1 — sketch the root command
--------------------------------

Every CLI starts with one root :cpp:class:`Command
<polycpp::commander::Command>`. Give it a name, a description, and a
version — the latter registers ``-V, --version`` for free:

.. code-block:: cpp

   using namespace polycpp::commander;

   auto& prog = polycpp::commander::program();
   prog.name("todo")
       .description("a tiny task tracker")
       .version("0.1.0")
       .showSuggestionAfterError(true);

   Option fileOpt("-f, --file <path>", "task database path");
   fileOpt.defaultValue(polycpp::JsonValue("tasks.json")).env("TODO_FILE");
   prog.addOption(std::move(fileOpt))
       .option("-v, --verbose", "log every action")
       .option("--color", "colorize output (auto when TTY)");

The root ``--file`` now has three sources, in priority order: the command
line, ``TODO_FILE`` from the environment, or the ``tasks.json`` default.

Step 2 — add ``add``, ``list``, and ``done`` subcommands
--------------------------------------------------------

:cpp:func:`command() <polycpp::commander::Command::command>` parses the
``"name <required> [optional]"`` form in one go, and returns a reference
to the *new* subcommand — chain ``.description()``, ``.argument()``,
``.option()`` and ``.action()`` directly:

.. code-block:: cpp

   auto parseInt = [](const std::string& s, const polycpp::JsonValue&) {
       return polycpp::JsonValue(std::stoi(s));
   };

   prog.command("add <title>")
       .description("add a new task")
       .option("-p, --priority <n>", "1 = high, 3 = low",
               parseInt, polycpp::JsonValue(2))
       .action([&](const auto& args, const auto& opts, auto& cmd) {
           auto parentOpts = cmd.optsWithGlobals();
           const auto verbose = parentOpts["verbose"];
           if (verbose.isBool() && verbose.asBool()) std::cerr << "[add]\n";
           tasks::create(args[0].asString(), opts["priority"].asInt());
       });

   prog.command("list")
       .description("list tasks (newest first)")
       .option("--done", "show completed tasks")
       .action([&](const auto&, const auto& opts, auto& cmd) {
           const auto done = opts["done"];
           const bool showDone = done.isBool() && done.asBool();
           for (auto& t : tasks::all(cmd.optsWithGlobals()["file"].asString()))
               if (t.done == showDone)
                   std::cout << t.id << "  " << t.title << '\n';
       });

   prog.command("done <id>")
       .description("mark a task done")
       .action([&](const auto& args, const auto&, auto&) {
           tasks::complete(std::stoi(args[0].asString()));
       });

The inner lambda calls :cpp:func:`optsWithGlobals()
<polycpp::commander::Command::optsWithGlobals>` to read the root
command's ``--verbose`` and ``--file`` without plumbing them through
every subcommand.

Note that ``tasks::create`` / ``tasks::all`` / ``tasks::complete`` are your
application code, not part of commander itself.

Step 3 — run the parser and print help on error
-----------------------------------------------

.. code-block:: cpp

   prog.showHelpAfterError("(use --help for usage)");
   prog.parse();

With :cpp:func:`showSuggestionAfterError
<polycpp::commander::Command::showSuggestionAfterError>` enabled,
``todo lsit`` now prints ``error: unknown command 'lsit' (did you mean
list?)``. ``showHelpAfterError`` appends the usage text so the user
sees the valid subcommand set.

Step 4 — verify without spawning a process
------------------------------------------

You can drive the parser in-process by passing ``{.from = "user"}``:

.. code-block:: cpp

   prog.exitOverride();                                 // throw instead of exit
   prog.parse({"add", "write docs", "--priority", "1"}, {.from = "user"});

``exitOverride()`` converts every would-be ``process::exit`` into a
:cpp:class:`CommanderError <polycpp::commander::CommanderError>` —
perfect for unit tests. Wrap the call in ``try``/``catch`` to assert
the error code:

.. code-block:: cpp

   try {
       prog.parse({"nope"}, {.from = "user"});
   } catch (const polycpp::commander::CommanderError& e) {
       assert(e.code == "commander.unknownCommand");
   }

What you learned
----------------

- ``command("name <required>")`` returns the *child* command; chain
  ``.description()``, ``.option()``, ``.action()`` inline.
- ``optsWithGlobals()`` lets subcommand actions read ancestor options.
- ``exitOverride()`` replaces the ``process::exit`` call with a
  throwable :cpp:class:`CommanderError
  <polycpp::commander::CommanderError>`, so you can test CLIs without
  forking.

Next: :doc:`async-actions` shows how to mix async I/O into this
structure using :cpp:func:`parseAsync()
<polycpp::commander::Command::parseAsync>` and the polycpp event loop.
