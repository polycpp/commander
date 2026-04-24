Option
======

A single command-line option — short flag, long flag, value
specification, default, preset, choices, env binding, conflicts,
implications.

Construct one from a flags string like ``"-p, --port <number>"`` then
attach it to a :cpp:class:`Command <polycpp::commander::Command>` with
:cpp:func:`addOption() <polycpp::commander::Command::addOption>`, or
use the :cpp:func:`Command::option()
<polycpp::commander::Command::option>` shorthand which builds the
Option for you.

.. doxygenclass:: polycpp::commander::Option
   :members:

Supporting types
----------------

.. doxygenstruct:: polycpp::commander::SplitFlags
   :members:
