// A minimal commander program with one flag, one option, and one argument.
//
//   $ ./greet alice
//   hello, alice
//   $ ./greet --shout alice
//   HELLO, ALICE
//   $ ./greet -s --lang es alice
//   hola, alice

#include <cctype>
#include <iostream>
#include <string>

#include <polycpp/commander/commander.hpp>
#include <polycpp/process.hpp>

int main(int argc, char** argv) {
    polycpp::process::init(argc, argv);

    auto& prog = polycpp::commander::program();
    prog.name("greet")
        .description("print a greeting")
        .version("1.0.0");

    prog.option("-s, --shout", "uppercase the output")
        .option("--lang <code>", "language (en, es, fr)",
                polycpp::JsonValue(std::string("en")))
        .argument("<name>", "who to greet")
        .action([](const auto& args, const auto& opts, auto&) {
            const std::string lang = opts["lang"].asString();
            std::string hello = (lang == "es") ? "hola"
                              : (lang == "fr") ? "bonjour"
                                               : "hello";
            std::string msg = hello + ", " + args[0].asString();
            const bool shout = opts["shout"].isBool() && opts["shout"].asBool();
            if (shout)
                for (auto& c : msg)
                    c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
            std::cout << msg << '\n';
        });

    prog.parse();
    return 0;
}
