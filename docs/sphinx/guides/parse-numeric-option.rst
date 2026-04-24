Parse an option value as a number (and reject bad input)
========================================================

**When to reach for this:** your option takes a number, a float, or an
enum-like string, and you want the user to get a sensible error instead
of a silent conversion.

Commander stores option values as :cpp:class:`polycpp::JsonValue`. A
custom :cpp:type:`ParseFn <polycpp::commander::ParseFn>` receives the
raw string and returns whatever typed value you want — any string the
parser can't handle should throw
:cpp:class:`InvalidArgumentError
<polycpp::commander::InvalidArgumentError>`:

.. code-block:: cpp

   auto intParser = [](const std::string& v, const polycpp::JsonValue&) {
       try {
           size_t end = 0;
           int n = std::stoi(v, &end);
           if (end != v.size())
               throw polycpp::commander::InvalidArgumentError(
                   "Not a whole number: " + v);
           return polycpp::JsonValue(n);
       } catch (const std::out_of_range&) {
           throw polycpp::commander::InvalidArgumentError(
               "Value out of range: " + v);
       }
   };

   prog.option("-p, --port <number>", "port", intParser,
               polycpp::JsonValue(8080));

If the user writes ``--port pizza`` the parser throws — commander turns
the throw into ``error: option '-p, --port <number>' argument 'pizza'
is invalid. Not a whole number: pizza`` and exits 1. With
:cpp:func:`exitOverride()
<polycpp::commander::Command::exitOverride>` set, you get the same
error as a :cpp:class:`CommanderError
<polycpp::commander::CommanderError>` you can catch.

For choices, use :cpp:func:`Option::choices()
<polycpp::commander::Option::choices>` instead of a hand-rolled parser:

.. code-block:: cpp

   using namespace polycpp::commander;
   Option opt("--env <name>");
   opt.choices({"dev", "stage", "prod"});
   prog.addOption(std::move(opt));
