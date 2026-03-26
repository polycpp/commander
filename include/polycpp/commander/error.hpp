#pragma once

/**
 * @file error.hpp
 * @brief Error classes for polycpp commander.
 * @see https://github.com/tj/commander.js
 * @since 0.1.0
 */

#include <polycpp/core/error.hpp>
#include <string>

namespace polycpp {
namespace commander {

/**
 * @brief Error class for commander-specific errors.
 *
 * Extends polycpp::Error with an exit code and error code string,
 * mirroring commander.js's CommanderError.
 *
 * @par Example
 * @code{.cpp}
 *   throw CommanderError(1, "commander.missingArgument", "missing required argument 'name'");
 * @endcode
 *
 * @see https://github.com/tj/commander.js#custom-error-handling
 * @since 0.1.0
 */
class CommanderError : public polycpp::Error {
public:
    /**
     * @brief Construct a CommanderError.
     * @param exitCode Suggested exit code for process.exit().
     * @param code Error identifier string (e.g., "commander.missingArgument").
     * @param message Human-readable error description.
     */
    CommanderError(int exitCode, const std::string& code, const std::string& message);

    int exitCode;       ///< Suggested exit code.
    std::string code;   ///< Error identifier string.
};

/**
 * @brief Error for invalid argument values (bad user input).
 *
 * Used when a custom argument parser or choices validation fails.
 * Always uses exitCode=1 and code="commander.invalidArgument".
 *
 * @par Example
 * @code{.cpp}
 *   throw InvalidArgumentError("invalid value 'xyz' for argument 'color'");
 * @endcode
 *
 * @see https://github.com/tj/commander.js#custom-argument-processing
 * @since 0.1.0
 */
class InvalidArgumentError : public CommanderError {
public:
    /**
     * @brief Construct an InvalidArgumentError.
     * @param message Explanation of why the argument is invalid.
     */
    explicit InvalidArgumentError(const std::string& message);
};

} // namespace commander
} // namespace polycpp
