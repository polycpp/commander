Entry points
============

The top-level ``polycpp::commander`` functions — a singleton accessor
and three factories that return fresh, unattached building blocks.

.. doxygenfunction:: polycpp::commander::program
.. doxygenfunction:: polycpp::commander::createCommand
.. doxygenfunction:: polycpp::commander::createOption
.. doxygenfunction:: polycpp::commander::createArgument

Utilities
---------

.. doxygenfunction:: polycpp::commander::camelcase
.. doxygenfunction:: polycpp::commander::splitOptionFlags
.. doxygenfunction:: polycpp::commander::suggestSimilar
.. doxygenfunction:: polycpp::commander::stripColor
.. doxygenfunction:: polycpp::commander::humanReadableArgName

Type aliases
------------

.. doxygentypedef:: polycpp::commander::ParseFn
.. doxygentypedef:: polycpp::commander::ActionFn
.. doxygentypedef:: polycpp::commander::HookFn
.. doxygentypedef:: polycpp::commander::AsyncActionFn
.. doxygentypedef:: polycpp::commander::AsyncHookFn
.. doxygentypedef:: polycpp::commander::OptionValues
.. doxygentypedef:: polycpp::commander::OptionValueSources
