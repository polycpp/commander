Dispatch to an external ``my-app-<sub>`` binary
===============================================

**When to reach for this:** you want ``git``-style plugins where
``my-app foo`` runs a separate executable, letting third parties ship
subcommands without linking your main binary.

Register the subcommand with :cpp:func:`executableCommand()
<polycpp::commander::Command::executableCommand>` instead of
``command()``:

.. code-block:: cpp

   prog.executableCommand("install", "install a plugin");
   prog.executableCommand("build",   "build the project", "my-builder");

With no action handler, commander spawns ``<parentName>-<sub>`` on the
PATH — ``my-app-install`` for the first line. Pass an explicit
executable name (``my-builder``) when the binary isn't named after the
subcommand.

Use :cpp:func:`executableDir()
<polycpp::commander::Command::executableDir>` to restrict the lookup
to a directory (absolute, or relative to ``argv[0]``):

.. code-block:: cpp

   prog.executableDir("/usr/lib/my-app/plugins");

Arguments after the subcommand are forwarded verbatim. The parent
command's options (both seen and unseen) stay behind — if you need
them on the child, pass them explicitly via an environment variable set
from a ``preSubcommand`` hook:

.. code-block:: cpp

   prog.hook("preSubcommand", [](auto& self, auto&) {
       const auto verbose = self.optsWithGlobals()["verbose"];
       if (verbose.isBool() && verbose.asBool())
           polycpp::process::env().set("MY_APP_VERBOSE", "1");
   });

The external process inherits stdin/stdout/stderr by default; its exit
code becomes the parent's exit code.
