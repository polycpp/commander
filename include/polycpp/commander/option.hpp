#pragma once

/**
 * @file option.hpp
 * @brief Option class for defining command-line options.
 * @see https://github.com/tj/commander.js#options
 * @since 1.0.0
 */

#include <polycpp/commander/argument.hpp>

#include <functional>
#include <map>
#include <optional>
#include <string>
#include <vector>

namespace polycpp {
namespace commander {

/**
 * @brief Represents a command-line option (flag with optional value).
 *
 * Mirrors npm commander's Option class. Constructor parses a flags string
 * to extract short flag, long flag, value specification, and negate status.
 *
 * Flag string formats:
 * - `"-f"`                  -> short flag only
 * - `"--flag"`              -> long flag only
 * - `"-f, --flag"`          -> short and long flag
 * - `"-f, --flag <value>"`  -> required value
 * - `"-f, --flag [value]"`  -> optional value
 * - `"--flag <values...>"`  -> variadic
 * - `"--no-flag"`           -> negate boolean
 *
 * @par Example
 * @code{.cpp}
 *   Option opt("-v, --verbose", "enable verbose output");
 *   assert(opt.name() == "verbose");
 *   assert(opt.isBoolean());
 * @endcode
 *
 * @see https://github.com/tj/commander.js#options
 * @since 1.0.0
 */
class Option {
public:
    /**
     * @brief Construct an Option by parsing a flags string.
     * @param flags Flag specification (e.g., "-f, --flag <value>").
     * @param description Human-readable description for help output.
     * @throws std::runtime_error If the flags string cannot be parsed.
     * @since 1.0.0
     */
    Option(const std::string& flags, const std::string& description = "");

    /**
     * @brief Return option name derived from the long flag (or short flag).
     *
     * For `--no-foo`, returns `"foo"` (strips `--no-` prefix after stripping dashes).
     * For `--dry-run`, returns `"dry-run"`.
     * For `-f` (short only), returns `"f"`.
     *
     * @return The option name.
     * @par Example
     * @code{.cpp}
     *   Option opt("--dry-run");
     *   assert(opt.name() == "dry-run");
     * @endcode
     * @see https://github.com/tj/commander.js#options
     * @since 1.0.0
     */
    std::string name() const;

    /**
     * @brief Return option name in camelCase, suitable as an attribute key.
     *
     * Converts kebab-case to camelCase. For negate options (`--no-foo`), strips
     * the `no-` prefix before conversion.
     *
     * @return camelCase option name.
     * @par Example
     * @code{.cpp}
     *   Option opt("--dry-run");
     *   assert(opt.attributeName() == "dryRun");
     * @endcode
     * @see https://github.com/tj/commander.js#options
     * @since 1.0.0
     */
    std::string attributeName() const;

    /**
     * @brief Return whether this is a boolean option (no value, no negate).
     * @return true if the option takes no argument and is not a negate option.
     * @par Example
     * @code{.cpp}
     *   Option opt("--verbose");
     *   assert(opt.isBoolean() == true);
     * @endcode
     * @see https://github.com/tj/commander.js#options
     * @since 1.0.0
     */
    bool isBoolean() const;

    /**
     * @brief Check if the given argument string matches this option's short or long flag.
     * @param arg Argument string to check (e.g., "-v" or "--verbose").
     * @return true if arg matches the short or long flag.
     * @par Example
     * @code{.cpp}
     *   Option opt("-v, --verbose");
     *   assert(opt.is("--verbose"));
     * @endcode
     * @see https://github.com/tj/commander.js
     * @since 1.0.0
     */
    bool is(const std::string& arg) const;

    /**
     * @brief Set the default value, and optionally supply the description for help output.
     * @param value Default value (stored as polycpp::JsonValue).
     * @param description Optional human-readable description of the default.
     * @return Reference to this Option for chaining.
     * @par Example
     * @code{.cpp}
     *   opt.defaultValue(polycpp::JsonValue("red"), "favorite color");
     * @endcode
     * @see https://github.com/tj/commander.js#options
     * @since 1.0.0
     */
    Option& defaultValue(const polycpp::JsonValue& value, const std::string& description = "");

    /**
     * @brief Set the preset value when option is used without an option-argument.
     * @param arg Preset value.
     * @return Reference to this Option for chaining.
     * @par Example
     * @code{.cpp}
     *   Option opt("--color").defaultValue(polycpp::JsonValue("GREYSCALE")).preset(polycpp::JsonValue("RGB"));
     * @endcode
     * @see https://github.com/tj/commander.js#options
     * @since 1.0.0
     */
    Option& preset(const polycpp::JsonValue& arg);

    /**
     * @brief Add option name(s) that conflict with this option.
     * @param name A single conflicting option name.
     * @return Reference to this Option for chaining.
     * @par Example
     * @code{.cpp}
     *   opt.conflicts("cmyk");
     * @endcode
     * @see https://github.com/tj/commander.js#options
     * @since 1.0.0
     */
    Option& conflicts(const std::string& name);

    /**
     * @brief Add option name(s) that conflict with this option.
     * @param names Vector of conflicting option names.
     * @return Reference to this Option for chaining.
     * @par Example
     * @code{.cpp}
     *   opt.conflicts({"ts", "jsx"});
     * @endcode
     * @see https://github.com/tj/commander.js#options
     * @since 1.0.0
     */
    Option& conflicts(const std::vector<std::string>& names);

    /**
     * @brief Specify implied option values for when this option is set.
     * @param values Map of option names to their implied values.
     * @return Reference to this Option for chaining.
     * @par Example
     * @code{.cpp}
     *   opt.implies({{"log", polycpp::JsonValue("trace.txt")}});
     * @endcode
     * @see https://github.com/tj/commander.js#options
     * @since 1.0.0
     */
    Option& implies(const std::map<std::string, polycpp::JsonValue>& values);

    /**
     * @brief Set environment variable to check for option value.
     * @param name Environment variable name.
     * @return Reference to this Option for chaining.
     * @par Example
     * @code{.cpp}
     *   opt.env("MY_APP_VERBOSE");
     * @endcode
     * @see https://github.com/tj/commander.js#options
     * @since 1.0.0
     */
    Option& env(const std::string& name);

    /**
     * @brief Set the custom handler for processing CLI option arguments.
     * @param fn Parser function taking (value, previous) and returning processed value.
     * @return Reference to this Option for chaining.
     * @par Example
     * @code{.cpp}
     *   opt.argParser([](const std::string& value, const polycpp::JsonValue&) -> polycpp::JsonValue {
     *       return polycpp::JsonValue(std::stoi(value));
     *   });
     * @endcode
     * @see https://github.com/tj/commander.js#custom-option-processing
     * @since 1.0.0
     */
    Option& argParser(ParseFn fn);

    /**
     * @brief Make this option mandatory (must have a value after parsing).
     * @param mandatory Whether the option is mandatory (default: true).
     * @return Reference to this Option for chaining.
     * @par Example
     * @code{.cpp}
     *   opt.makeOptionMandatory();
     * @endcode
     * @see https://github.com/tj/commander.js#options
     * @since 1.0.0
     */
    Option& makeOptionMandatory(bool mandatory = true);

    /**
     * @brief Hide this option from help output.
     * @param hide Whether to hide (default: true).
     * @return Reference to this Option for chaining.
     * @par Example
     * @code{.cpp}
     *   opt.hideHelp();
     * @endcode
     * @see https://github.com/tj/commander.js#options
     * @since 1.0.0
     */
    Option& hideHelp(bool hide = true);

    /**
     * @brief Only allow option value to be one of the specified choices.
     * @param values Allowed values for this option.
     * @return Reference to this Option for chaining.
     * @par Example
     * @code{.cpp}
     *   opt.choices({"small", "medium", "large"});
     * @endcode
     * @see https://github.com/tj/commander.js#options
     * @since 1.0.0
     */
    Option& choices(const std::vector<std::string>& values);

    std::string flags;          ///< Original flags string.
    std::string description;    ///< Human-readable description.
    bool required;              ///< A value must be supplied when option is specified.
    bool optional;              ///< A value is optional when option is specified.
    bool variadic;              ///< Option can take multiple values.
    bool mandatory;             ///< Option must be specified on command line.
    bool negate;                ///< Whether this is a --no-* negation option.
    bool hidden;                ///< Hide from help output.
    std::optional<std::string> short_;  ///< Short flag (e.g., "-f").
    std::optional<std::string> long_;   ///< Long flag (e.g., "--file").
    polycpp::JsonValue defaultValue_;    ///< Default value (null if not set).
    std::string defaultValueDescription_; ///< Description of the default value.
    polycpp::JsonValue presetArg_;       ///< Preset value when used without argument (null if not set).
    std::optional<std::string> envVar_; ///< Environment variable name.
    ParseFn parseArg_;                  ///< Custom argument parser.
    std::optional<std::vector<std::string>> argChoices_; ///< Allowed values.
    std::vector<std::string> conflictsWith_; ///< Names of conflicting options.
    std::optional<std::map<std::string, polycpp::JsonValue>> implied_; ///< Implied option values.

private:
    // (no additional private members beyond public fields)
};

/**
 * @brief Convert a kebab-case string to camelCase.
 * @param str Input string (e.g., "dry-run").
 * @return camelCase output (e.g., "dryRun").
 * @par Example
 * @code{.cpp}
 *   assert(camelcase("dry-run") == "dryRun");
 * @endcode
 * @since 1.0.0
 */
std::string camelcase(const std::string& str);

/**
 * @brief Result of splitting option flags.
 * @since 1.0.0
 */
struct SplitFlags {
    std::optional<std::string> shortFlag; ///< Short flag (e.g., "-f").
    std::optional<std::string> longFlag;  ///< Long flag (e.g., "--file").
};

/**
 * @brief Split the short and long flag out of a combined flags string.
 * @param flags Combined flags string (e.g., "-m, --mixed <value>").
 * @return SplitFlags struct with separated short and long flags.
 * @throws std::runtime_error If flags string contains invalid flag patterns.
 * @par Example
 * @code{.cpp}
 *   auto result = splitOptionFlags("-f, --file <path>");
 *   assert(result.shortFlag == "-f");
 *   assert(result.longFlag == "--file");
 * @endcode
 * @since 1.0.0
 */
SplitFlags splitOptionFlags(const std::string& flags);

} // namespace commander
} // namespace polycpp
