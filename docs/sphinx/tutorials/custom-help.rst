Tailor the help layout
======================

**You'll build:** a CLI whose help output is sorted, color-aware, has a
wider column for option terms, and prepends a banner above the usage
line.

**You'll use:**
:cpp:func:`polycpp::commander::Command::configureHelp`,
:cpp:func:`polycpp::commander::Command::addHelpText`, and a subclass
of :cpp:class:`polycpp::commander::Help` for the parts configuration
can't reach.

**Prerequisites:** comfortable with options and subcommands
(:doc:`git-style-cli`).

Step 1 — configuration-driven tweaks
------------------------------------

The cheap wins come from :cpp:func:`configureHelp()
<polycpp::commander::Command::configureHelp>`. It accepts a map of
well-known keys — ``sortOptions``, ``sortSubcommands``,
``showGlobalOptions``, and ``helpWidth`` — and leaves the surrounding
renderer alone:

.. code-block:: cpp

   prog.configureHelp({
       {"sortOptions",       polycpp::JsonValue(true)},
       {"sortSubcommands",   polycpp::JsonValue(true)},
       {"showGlobalOptions", polycpp::JsonValue(true)},
       {"helpWidth",         polycpp::JsonValue(100)},
   });

Alphabetised option lists are the single biggest usability win on CLIs
with more than ~8 flags, and ``showGlobalOptions`` surfaces inherited
options (``--verbose``, ``--file``) inside each subcommand's help page.

Step 2 — add surrounding text with ``addHelpText``
--------------------------------------------------

Four positions are supported:

- ``"beforeAll"``: above the top-level usage line of every command.
- ``"before"``: above this command's usage line.
- ``"after"``: below this command's last section.
- ``"afterAll"``: the very bottom of every rendering.

.. code-block:: cpp

   prog.addHelpText("beforeAll",
       "Todo 0.1.0 — https://github.com/acme/todo\n");
   prog.addHelpText("after",
       "\nEnvironment:\n"
       "  TODO_FILE  path used when --file is not set\n");

The strings flow through the same styling pipeline as the built-in
sections — no need to call ``styleTitle`` yourself.

Step 3 — subclass ``Help`` for deep customisation
-------------------------------------------------

Configuration stops at the column widths. For everything else, subclass
:cpp:class:`Help <polycpp::commander::Help>` and override the relevant
virtual:

.. code-block:: cpp

   class WideHelp : public polycpp::commander::Help {
   public:
       std::string subcommandTerm(
           const polycpp::commander::Command& cmd) const override {
           // show "▸ " prefix so subcommands jump out of the wall of text
           return "▸ " + Help::subcommandTerm(cmd);
       }
   };

Plug it into the command:

.. code-block:: cpp

   class WideCommand : public polycpp::commander::Command {
   public:
       using Command::Command;
       polycpp::commander::Help createHelp() const override {
           return WideHelp{};
       }
   };

   WideCommand prog("todo");

You only need to override the methods you actually change — ``Help``'s
other virtuals stay available through the base class.

Step 4 — color
--------------

Terminal color is opt-in per stream. Provide callbacks telling
commander whether stdout and stderr support ANSI:

.. code-block:: cpp

   prog.configureOutput({
       .writeOut = [](const std::string& s){ std::cout << s; },
       .writeErr = [](const std::string& s){ std::cerr << s; },
       .getOutHelpWidth  = []{ return 100; },
       .getErrHelpWidth  = []{ return 100; },
       .getOutHasColors  = []{ return ::isatty(1); },
       .getErrHasColors  = []{ return ::isatty(2); },
   });

When ``getOutHasColors()`` returns true the default ``Help`` applies
styles via ``styleOptionTerm`` / ``styleTitle`` / ``styleUsage``.

What you learned
----------------

- ``configureHelp()`` covers the 80 % case: sort, global options,
  column width.
- ``addHelpText()`` appends banners without custom rendering.
- Subclass :cpp:class:`Help <polycpp::commander::Help>` when you need
  to change what a term or description looks like; override
  :cpp:func:`createHelp() <polycpp::commander::Command::createHelp>`
  to plug the subclass in.

Next: try the :doc:`../guides/index` for short recipes on specific
help questions (hiding a subcommand, styling errors, passing a custom
writer).
