#pragma once

/**
 * @file detail/argument.hpp
 * @brief Inline implementations for the Argument class.
 * @since 0.1.0
 */

#include <polycpp/commander/argument.hpp>
#include <polycpp/commander/error.hpp>

#include <algorithm>

namespace polycpp {
namespace commander {

inline Argument::Argument(const std::string& name, const std::string& description)
    : description(description), required(true), variadic(false) {
    if (!name.empty() && name[0] == '<') {
        // e.g. <required>
        required = true;
        name_ = name.substr(1, name.size() - 2);
    } else if (!name.empty() && name[0] == '[') {
        // e.g. [optional]
        required = false;
        name_ = name.substr(1, name.size() - 2);
    } else {
        required = true;
        name_ = name;
    }

    if (name_.size() >= 3 && name_.substr(name_.size() - 3) == "...") {
        variadic = true;
        name_ = name_.substr(0, name_.size() - 3);
    }
}

inline std::string Argument::name() const {
    return name_;
}

inline Argument& Argument::defaultValue(std::any value, const std::string& description) {
    defaultValue_ = std::move(value);
    defaultValueDescription_ = description;
    return *this;
}

inline Argument& Argument::argParser(ParseFn fn) {
    parseArg_ = std::move(fn);
    return *this;
}

inline Argument& Argument::choices(const std::vector<std::string>& values) {
    argChoices_ = values;
    parseArg_ = [this](const std::string& arg, const std::any& previous) -> std::any {
        if (argChoices_) {
            const auto& choices = *argChoices_;
            if (std::find(choices.begin(), choices.end(), arg) == choices.end()) {
                std::string allowed;
                for (size_t i = 0; i < choices.size(); ++i) {
                    if (i > 0) allowed += ", ";
                    allowed += choices[i];
                }
                throw InvalidArgumentError("Allowed choices are " + allowed + ".");
            }
        }
        if (variadic) {
            // Collect into vector
            if (!previous.has_value()) {
                return std::any(std::vector<std::string>{arg});
            }
            try {
                auto vec = std::any_cast<std::vector<std::string>>(previous);
                vec.push_back(arg);
                return std::any(std::move(vec));
            } catch (const std::bad_any_cast&) {
                return std::any(std::vector<std::string>{arg});
            }
        }
        return std::any(arg);
    };
    return *this;
}

inline Argument& Argument::argRequired() {
    required = true;
    return *this;
}

inline Argument& Argument::argOptional() {
    required = false;
    return *this;
}

inline std::string humanReadableArgName(const Argument& arg) {
    std::string nameOutput = arg.name() + (arg.variadic ? "..." : "");
    if (arg.required) {
        return "<" + nameOutput + ">";
    }
    return "[" + nameOutput + "]";
}

} // namespace commander
} // namespace polycpp
