#pragma once

/**
 * @file detail/commander.hpp
 * @brief Inline implementations for module-level factory functions.
 * @since 0.1.0
 */

#include <polycpp/commander/commander.hpp>

namespace polycpp {
namespace commander {

inline std::unique_ptr<Command> createCommand(const std::string& name) {
    return std::make_unique<Command>(name);
}

inline Option createOption(const std::string& flags, const std::string& description) {
    return Option(flags, description);
}

inline Argument createArgument(const std::string& name, const std::string& description) {
    return Argument(name, description);
}

} // namespace commander
} // namespace polycpp
