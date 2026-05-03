Core concepts
=============

This page fills in the mental model behind the :doc:`quickstart`. If you
already know commander.js, most names and call patterns carry across; the
main differences are ownership, typed storage, and how parsing interacts
with a native C++ process.

Command ownership
-----------------

There are two common entry points:

- :cpp:func:`polycpp::commander::program` returns a process-wide singleton.
  Use it when your app has one obvious root command.
- :cpp:func:`polycpp::commander::createCommand` returns an owned
  :cpp:class:`Command <polycpp::commander::Command>` handle. Use it when you
  want to construct commands explicitly, return them from helpers, or avoid
  global state.

``Command`` is a lightweight handle over shared state, so copying a command
handle keeps pointing at the same underlying command tree.

Command trees
-------------

One :cpp:class:`Command <polycpp::commander::Command>` represents one level
of a CLI:

- the root program usually declares global options and top-level behavior
- each ``command("name ...")`` call creates a child command
- each command can have its own options, arguments, help text, hooks, and
  action handler

This mirrors commander.js: the root command parses the argv vector, then the
selected subcommand receives its action callback.

Option and argument values
--------------------------

Parsed values are stored as :cpp:class:`polycpp::JsonValue`:

- boolean flags often read as ``true`` only when present
- value-bearing options default to strings unless you install a parser
- defaults, env bindings, and implied values also end up in the same
  ``JsonValue`` container
- positional arguments arrive as a ``std::vector<polycpp::JsonValue>``

In practice, that means two rules keep examples safe:

1. Guard optional booleans before calling ``asBool()``.
2. Add an ``argParser`` when you want ``int``, ``double``, enums, or other
   validated types instead of raw strings.

For example:

.. code-block:: cpp

   const auto verbose = opts["verbose"];
   const bool isVerbose = verbose.isBool() && verbose.asBool();

   prog.option("-p, --port <number>", "listen port",
       [](const std::string& s, const polycpp::JsonValue&) {
           return polycpp::JsonValue(std::stoi(s));
       },
       polycpp::JsonValue(8080));

Parsing modes
-------------

Use the parsing entry point that matches where your argv came from:

- ``prog.parse()``: normal native C++ entry point. It reads
  ``polycpp::process::argv()`` after ``polycpp::process::init(argc, argv)``.
- ``prog.parse(argv, {.from = "native"})``: same native argv shape, but
  supplied explicitly.
- ``prog.parse(argv, {.from = "user"})``: only user arguments, with the
  program name already stripped. This is the most convenient mode for tests.
- ``prog.parseAsync(...)``: async variant that resolves after async actions
  and hooks complete.

Error handling
--------------

By default, parse failures and ``--help`` / ``--version`` exit the process in
the same style as commander.js. When you want library-style control flow,
install :cpp:func:`exitOverride()
<polycpp::commander::Command::exitOverride>` and catch
:cpp:class:`CommanderError <polycpp::commander::CommanderError>` instead.

Where to go next
----------------

- :doc:`quickstart` for the smallest runnable setup.
- :doc:`../guides/parse-numeric-option` for validated numeric values.
- :doc:`../tutorials/git-style-cli` for subcommands and inherited options.
- :doc:`../api/command` for the full ``Command`` surface.
