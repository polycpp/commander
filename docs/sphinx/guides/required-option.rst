Require an option to be specified
=================================

**When to reach for this:** a flag is load-bearing enough that missing
it should be an error, not a silent default.

Use :cpp:func:`requiredOption()
<polycpp::commander::Command::requiredOption>`. The signature mirrors
:cpp:func:`option() <polycpp::commander::Command::option>`, but if the
option is absent after parsing the command reports
``commander.missingMandatoryOptionValue`` and exits.

.. code-block:: cpp

   prog.requiredOption("--config <path>", "configuration file");
   prog.parse();                  // exits if --config missing

A required option can still have a default value — a default turns
"must be specified on the command line" into "must have *some* source
(CLI, env, default, implied)". Combine with ``.env("FOO")`` on an
:cpp:class:`Option <polycpp::commander::Option>` to accept either:

.. code-block:: cpp

   using namespace polycpp::commander;
   Option cfg("--config <path>", "configuration file");
   cfg.env("APP_CONFIG");
   prog.addOption(std::move(cfg)).requiredOption("--config <path>", "configuration file");

If you just want to validate the value itself, use ``argParser`` plus
:cpp:class:`InvalidArgumentError
<polycpp::commander::InvalidArgumentError>` — see
:doc:`parse-numeric-option`.
