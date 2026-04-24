Command
=======

The :cpp:class:`polycpp::commander::Command` class is the center of the
library. A single instance represents one CLI command — options,
arguments, subcommands, action handler, hooks, help configuration,
error handling, and parsed state.

.. doxygenclass:: polycpp::commander::Command
   :members:

Parse configuration
-------------------

.. doxygenstruct:: polycpp::commander::ParseOptions
   :members:

.. doxygenstruct:: polycpp::commander::ParseOptionsResult
   :members:

.. doxygenstruct:: polycpp::commander::CommandOptions
   :members:

.. doxygenstruct:: polycpp::commander::ErrorOptions
   :members:

.. doxygenstruct:: polycpp::commander::OutputConfiguration
   :members:

.. doxygenstruct:: polycpp::commander::HelpContext
   :members:
