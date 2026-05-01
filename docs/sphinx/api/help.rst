Help
====

Formatter for the built-in help text. Override virtuals and plug your
subclass in via :cpp:func:`Command::createHelp()
<polycpp::commander::Command::createHelp>` to change what help looks
like; for simpler knobs use :cpp:func:`Command::configureHelp()
<polycpp::commander::Command::configureHelp>`.

The six configuration knobs (:cpp:func:`helpWidth()
<polycpp::commander::Help::helpWidth>`, :cpp:func:`minWidthToWrap()
<polycpp::commander::Help::minWidthToWrap>`, :cpp:func:`sortSubcommands()
<polycpp::commander::Help::sortSubcommands>`, :cpp:func:`sortOptions()
<polycpp::commander::Help::sortOptions>`, :cpp:func:`showGlobalOptions()
<polycpp::commander::Help::showGlobalOptions>`,
:cpp:func:`outputHasColors()
<polycpp::commander::Help::outputHasColors>`) follow the same dual
getter/setter style ``Command`` uses: ``help.helpWidth()`` reads,
``help.helpWidth(80)`` writes and chains. Apply several at once with
:cpp:func:`configure() <polycpp::commander::Help::configure>` and a
:cpp:struct:`HelpConfiguration
<polycpp::commander::Help::HelpConfiguration>`.

.. doxygenclass:: polycpp::commander::Help
   :members:

Helpers
-------

.. doxygenstruct:: polycpp::commander::PrepareContextOptions
   :members:
