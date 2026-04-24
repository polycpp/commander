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

Step 1 — sketch the root command
--------------------------------

Every CLI starts with one root :cpp:class:`Command
<polycpp::commander::Command>`. Give it a name, a description, and a
version — the latter registers ``-V, --version`` for free:

.. code-block:: cpp

   auto& prog = polycpp::commander::program();
   prog.name("todo")
       .description("a tiny task tracker")
       .version("0.1.0")
       .showSuggestionAfterError(true);

   prog.option("-v, --verbose", "log every action")
       .option("-f, --file <path>", "task database path",
               polycpp::JsonValue("tasks.json"))
       .option("--color", "colorize output (auto when TTY)");

Hook the global ``--file`` to an environment variable so the user can
skip the flag once they've set ``TODO_FILE=...``:

.. code-block:: cpp

   using namespace polycpp::commander;
   Option fileOpt("-f, --file <path>", "task database path");
   fileOpt.defaultValue(polycpp::JsonValue("tasks.json")).env("TODO_FILE");
   prog.addOption(std::move(fileOpt));

Step 2 — add ``add``, ``list``, and ``done`` subcommands
--------------------------------------------------------

:cpp:func:`command() <polycpp::commander::Command::command>` parses the
``"name <required> [optional]"`` form in one go, and returns a reference
to the *new* subcommand — chain ``.description()``, ``.argument()``,
``.option()`` and ``.action()`` directly:

.. code-block:: cpp

   prog.command("add <title>")
       .description("add a new task")
       .option("-p, --priority <n>", "1 = high, 3 = low",
               polycpp::JsonValue(2))
       .action([&](const auto& args, const auto& opts, auto& cmd) {
           auto parentOpts = cmd.optsWithGlobals();
           if (parentOpts["verbose"].asBool()) std::cerr << "[add]\n";
           tasks::create(args[0].asString(), opts["priority"].asInt());
       });

   prog.command("list")
       .description("list tasks (newest first)")
       .option("--done", "show completed tasks")
       .action([&](const auto&, const auto& opts, auto& cmd) {
           for (auto& t : tasks::all(cmd.optsWithGlobals()["file"].asString()))
               if (t.done == opts["done"].asBool())
                   std::cout << t.id << "  " << t.title << '\n';
       });

   prog.command("done <id>")
       .description("mark a task done")
       .action([&](const auto& args, const auto&, auto&) {
           tasks::complete(args[0].asInt());
       });

The inner lambda calls :cpp:func:`optsWithGlobals()
<polycpp::commander::Command::optsWithGlobals>` to read the root
command's ``--verbose`` and ``--file`` without plumbing them through
every subcommand.

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
