Accept multiple values for one option
=====================================

**When to reach for this:** a flag like ``--include <path>`` the user
will pass several times, or ``<files...>`` at the end of the argument
list.

Two flavours — both produce a :cpp:class:`polycpp::JsonValue` array in
the parsed opts:

Repeated-flag form
------------------

Attach a parser that accumulates into the previous value. Each
occurrence runs the parser with the freshly seen value as its first
argument and the previous result as the second:

.. code-block:: cpp

   prog.option("-I, --include <path>", "add to search path",
       [](const std::string& value, const polycpp::JsonValue& prev) {
           auto arr = prev.isArray() ? prev : polycpp::JsonValue::array();
           arr.push_back(polycpp::JsonValue(value));
           return arr;
       },
       polycpp::JsonValue::array());   // default = empty array

   prog.parse({"-I", "./src", "-I", "./vendor"}, {.from = "user"});
   // prog.opts()["include"] == ["./src", "./vendor"]

Variadic form
-------------

Write ``<values...>`` or ``[values...]`` in the flag string to slurp
every subsequent token until the next flag or ``--``:

.. code-block:: cpp

   prog.option("--files <files...>", "files to process");
   prog.parse({"--files", "a.txt", "b.txt", "c.txt"}, {.from = "user"});
   // prog.opts()["files"] == ["a.txt","b.txt","c.txt"]

Variadic **arguments** work the same way at the positional level:

.. code-block:: cpp

   prog.argument("<files...>");
   prog.action([](const auto& args, auto, auto&) {
       // args[0] is a JsonValue array
       for (auto& f : args[0].asArray()) std::cout << f.asString() << '\n';
   });

Mix the two only when the variadic is last — commander won't figure out
where a middle-positional variadic ends.
