API reference
=============

The complete public surface of commander, generated from the Doxygen
comments in the headers. If a symbol is missing here, either it's internal
(under a ``detail`` namespace) or its header comment needs more love —
please open an issue.

Start here if you want the API by task instead of by symbol:

- :doc:`entry-points`: choose between the singleton ``program()`` and the
  factory helpers.
- :doc:`command`: build command trees, parse argv, install hooks, customize
  help, and handle errors.
- :doc:`option`: define flags, defaults, env bindings, conflicts, and parsers.
- :doc:`argument`: define positional operands and argument parsers.
- :doc:`help`: customize formatting and rendering.
- :doc:`errors`: catch and interpret parse failures when using
  ``exitOverride()``.

Module index
------------

.. toctree::
   :maxdepth: 1

   entry-points
   command
   option
   argument
   help
   errors

Namespace overview
------------------

.. doxygennamespace:: polycpp::commander
   :desc-only:
