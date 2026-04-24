Errors
======

Exceptions thrown by the parser when :cpp:func:`exitOverride()
<polycpp::commander::Command::exitOverride>` has been installed, or
inside user parsers to flag invalid input.

.. doxygenclass:: polycpp::commander::CommanderError
   :members:

.. doxygenclass:: polycpp::commander::InvalidArgumentError
   :members:

Error codes
-----------

:cpp:class:`CommanderError::code
<polycpp::commander::CommanderError>` uses these stable identifiers.
They match commander.js one-for-one:

- ``commander.missingArgument`` — a required positional was not
  provided.
- ``commander.missingMandatoryOptionValue`` — a
  :cpp:func:`requiredOption() <polycpp::commander::Command::requiredOption>`
  was not set (CLI, env, or default).
- ``commander.optionMissingArgument`` — a value-bearing option was
  given with no value.
- ``commander.unknownOption`` — an unrecognised flag was passed.
- ``commander.unknownCommand`` — an unrecognised subcommand was
  passed.
- ``commander.invalidArgument`` — a custom parser raised
  :cpp:class:`InvalidArgumentError
  <polycpp::commander::InvalidArgumentError>`.
- ``commander.excessArguments`` — extra operands after all positionals
  are bound.
- ``commander.helpDisplayed`` / ``commander.version`` — emitted when
  help or version output finishes; treat as success.
