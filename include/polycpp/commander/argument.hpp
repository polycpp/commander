#pragma once

/**
 * @file argument.hpp
 * @brief Argument class for defining command arguments.
 * @see https://github.com/tj/commander.js#more-configuration-2
 * @since 0.1.0
 */

#include <polycpp/core/json.hpp>

#include <functional>
#include <optional>
#include <string>
#include <vector>

namespace polycpp {
namespace commander {

/// @brief Type alias for argument parsing callbacks.
/// @since 0.1.0
using ParseFn = std::function<polycpp::JsonValue(const std::string&, const polycpp::JsonValue&)>;

/**
 * @brief Represents a command argument (positional parameter).
 *
 * Mirrors npm commander's Argument class. Constructor parses the name
 * to determine whether the argument is required, optional, and/or variadic.
 *
 * - `<name>`    -> required
 * - `[name]`    -> optional
 * - `<name...>` -> required variadic
 * - `[name...]` -> optional variadic
 * - `name`      -> required (no brackets)
 *
 * @par Example
 * @code{.cpp}
 *   Argument arg("<file>", "input file to process");
 *   assert(arg.required);
 *   assert(arg.name() == "file");
 * @endcode
 *
 * @see https://github.com/tj/commander.js#more-configuration-2
 * @since 0.1.0
 */
class Argument {
public:
    /**
     * @brief Construct an Argument by parsing a name specification.
     * @param name Argument name with optional angle/square brackets and variadic dots.
     * @param description Human-readable description for help output.
     * @since 0.1.0
     */
    Argument(const std::string& name, const std::string& description = "");

    /**
     * @brief Return the argument name (without brackets or dots).
     * @return The parsed argument name.
     * @par Example
     * @code{.cpp}
     *   Argument arg("<file...>");
     *   assert(arg.name() == "file");
     * @endcode
     * @see https://github.com/tj/commander.js#more-configuration-2
     * @since 0.1.0
     */
    std::string name() const;

    /**
     * @brief Set the default value, and optionally supply the description for help output.
     * @param value Default value (stored as polycpp::JsonValue).
     * @param description Optional human-readable description of the default.
     * @return Reference to this Argument for chaining.
     * @par Example
     * @code{.cpp}
     *   Argument arg("[color]").defaultValue(polycpp::JsonValue("blue"), "favorite color");
     * @endcode
     * @see https://github.com/tj/commander.js#more-configuration-2
     * @since 0.1.0
     */
    Argument& defaultValue(const polycpp::JsonValue& value, const std::string& description = "");

    /**
     * @brief Set the custom handler for processing CLI argument values.
     * @param fn Parser function taking (value, previous) and returning processed value.
     * @return Reference to this Argument for chaining.
     * @par Example
     * @code{.cpp}
     *   arg.argParser([](const std::string& value, const polycpp::JsonValue&) -> polycpp::JsonValue {
     *       return polycpp::JsonValue(std::stoi(value));
     *   });
     * @endcode
     * @see https://github.com/tj/commander.js#custom-argument-processing
     * @since 0.1.0
     */
    Argument& argParser(ParseFn fn);

    /**
     * @brief Only allow argument value to be one of the specified choices.
     * @param values Allowed values for this argument.
     * @return Reference to this Argument for chaining.
     * @par Example
     * @code{.cpp}
     *   arg.choices({"small", "medium", "large"});
     * @endcode
     * @see https://github.com/tj/commander.js#more-configuration-2
     * @since 0.1.0
     */
    Argument& choices(const std::vector<std::string>& values);

    /**
     * @brief Make argument required.
     * @return Reference to this Argument for chaining.
     * @see https://github.com/tj/commander.js#more-configuration-2
     * @since 0.1.0
     */
    Argument& argRequired();

    /**
     * @brief Make argument optional.
     * @return Reference to this Argument for chaining.
     * @see https://github.com/tj/commander.js#more-configuration-2
     * @since 0.1.0
     */
    Argument& argOptional();

    std::string description;                ///< Human-readable description.
    bool required;                          ///< Whether the argument must be provided.
    bool variadic;                          ///< Whether the argument accepts multiple values.
    polycpp::JsonValue defaultValue_;        ///< Default value (null if not set).
    std::string defaultValueDescription_;   ///< Description of the default value.
    ParseFn parseArg_;                      ///< Custom argument parser.
    std::optional<std::vector<std::string>> argChoices_; ///< Allowed values.

private:
    std::string name_;                      ///< Parsed argument name.
};

/**
 * @brief Return human-readable representation of an argument for help output.
 * @param arg The argument to format.
 * @return Formatted string like "<file...>" or "[color]".
 * @par Example
 * @code{.cpp}
 *   Argument arg("<file...>");
 *   assert(humanReadableArgName(arg) == "<file...>");
 * @endcode
 * @see https://github.com/tj/commander.js
 * @since 0.1.0
 */
std::string humanReadableArgName(const Argument& arg);

} // namespace commander
} // namespace polycpp
