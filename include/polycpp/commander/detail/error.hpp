#pragma once

#include <polycpp/commander/error.hpp>

namespace polycpp {
namespace commander {

inline CommanderError::CommanderError(int exitCode, const std::string& code, const std::string& message)
    : polycpp::Error(message), exitCode(exitCode), code(code) {}

inline InvalidArgumentError::InvalidArgumentError(const std::string& message)
    : CommanderError(1, "commander.invalidArgument", message) {}

} // namespace commander
} // namespace polycpp
