#pragma once

/**
 * @file commander.hpp
 * @brief Module entry point — factory functions and global program singleton.
 * @see https://github.com/tj/commander.js
 * @since 0.1.0
 */

#include <polycpp/commander/argument.hpp>
#include <polycpp/commander/command.hpp>
#include <polycpp/commander/error.hpp>
#include <polycpp/commander/help.hpp>
#include <polycpp/commander/option.hpp>

#include <memory>
#include <string>

namespace polycpp {
namespace commander {

/**
 * @brief Get the global program singleton.
 *
 * Equivalent to `require('commander').program` in Node.js.
 * The singleton is created on first access and persists for the process lifetime.
 *
 * @return Reference to the global Command instance.
 * @par Example
 * @code{.cpp}
 *   auto& prog = polycpp::commander::program();
 *   prog.option("-v, --verbose", "verbose mode");
 *   prog.parse();
 * @endcode
 * @see https://github.com/tj/commander.js
 * @since 0.1.0
 */
Command& program();

/**
 * @brief Create a new unattached Command.
 * @param name Optional command name.
 * @return New Command as unique_ptr.
 * @par Example
 * @code{.cpp}
 *   auto cmd = polycpp::commander::createCommand("serve");
 * @endcode
 * @see https://github.com/tj/commander.js
 * @since 0.1.0
 */
std::unique_ptr<Command> createCommand(const std::string& name = "");

/**
 * @brief Create a new unattached Option.
 * @param flags Flag specification.
 * @param description Human-readable description.
 * @return New Option object.
 * @par Example
 * @code{.cpp}
 *   auto opt = polycpp::commander::createOption("-v, --verbose", "verbose mode");
 * @endcode
 * @see https://github.com/tj/commander.js
 * @since 0.1.0
 */
Option createOption(const std::string& flags, const std::string& description = "");

/**
 * @brief Create a new unattached Argument.
 * @param name Argument name.
 * @param description Human-readable description.
 * @return New Argument object.
 * @par Example
 * @code{.cpp}
 *   auto arg = polycpp::commander::createArgument("<file>", "input file");
 * @endcode
 * @see https://github.com/tj/commander.js
 * @since 0.1.0
 */
Argument createArgument(const std::string& name, const std::string& description = "");

} // namespace commander
} // namespace polycpp

// Pull in the inline implementations so that consumers including this
// public umbrella header get a fully-linkable library, matching the
// `polycpp/process.hpp`-style umbrella pattern used elsewhere in the
// ecosystem. Internal implementation files live under `detail/` and are
// not part of the public API surface.
#include <polycpp/commander/detail/aggregator.hpp>
