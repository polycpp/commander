Help
====

Formatter for the built-in help text. Override virtuals and plug your
subclass in via :cpp:func:`Command::createHelp()
<polycpp::commander::Command::createHelp>` to change what help looks
like; for simpler knobs use :cpp:func:`Command::configureHelp()
<polycpp::commander::Command::configureHelp>`.

.. doxygenclass:: polycpp::commander::Help
   :members:

Helpers
-------

.. doxygenstruct:: polycpp::commander::PrepareContextOptions
   :members:
