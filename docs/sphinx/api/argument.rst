Argument
========

A positional parameter — required (``<name>``), optional (``[name]``),
or variadic (``<name...>`` / ``[name...]``). Attach one to a command
with :cpp:func:`addArgument()
<polycpp::commander::Command::addArgument>`, or let :cpp:func:`argument()
<polycpp::commander::Command::argument>` construct and register it in a
single call.

.. doxygenclass:: polycpp::commander::Argument
   :members:
