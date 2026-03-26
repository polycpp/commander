#pragma once

/**
 * @file detail/option.hpp
 * @brief Inline implementations for the Option class and related utilities.
 * @since 0.1.0
 */

#include <polycpp/commander/option.hpp>
#include <polycpp/commander/error.hpp>

#include <algorithm>
#include <regex>
#include <sstream>
#include <stdexcept>

namespace polycpp {
namespace commander {

inline std::string camelcase(const std::string& str) {
    std::string result;
    bool capitalizeNext = false;
    bool first = true;

    for (size_t i = 0; i < str.size(); ++i) {
        if (str[i] == '-') {
            capitalizeNext = true;
        } else {
            if (capitalizeNext && !first) {
                result += static_cast<char>(std::toupper(static_cast<unsigned char>(str[i])));
            } else {
                result += str[i];
            }
            capitalizeNext = false;
            first = false;
        }
    }
    return result;
}

inline SplitFlags splitOptionFlags(const std::string& flags) {
    std::optional<std::string> shortFlag;
    std::optional<std::string> longFlag;

    // Regex patterns matching the JS implementation
    std::regex shortFlagExp("^-[^-]$");
    std::regex longFlagExp("^--[^-]");

    // Split on space, comma, or pipe (with optional surrounding spaces)
    // Then add a "guard" sentinel
    std::vector<std::string> flagParts;
    {
        std::string current;
        for (size_t i = 0; i < flags.size(); ++i) {
            char c = flags[i];
            if (c == ' ' || c == ',' || c == '|') {
                if (!current.empty()) {
                    flagParts.push_back(current);
                    current.clear();
                }
            } else {
                current += c;
            }
        }
        if (!current.empty()) {
            flagParts.push_back(current);
        }
    }
    flagParts.push_back("guard");

    size_t idx = 0;

    // Normal is short and/or long.
    if (idx < flagParts.size() && std::regex_search(flagParts[idx], shortFlagExp)) {
        shortFlag = flagParts[idx];
        ++idx;
    }
    if (idx < flagParts.size() && std::regex_search(flagParts[idx], longFlagExp)) {
        longFlag = flagParts[idx];
        ++idx;
    }
    // Long then short. Rarely used but fine.
    if (!shortFlag.has_value() && idx < flagParts.size() &&
        std::regex_search(flagParts[idx], shortFlagExp)) {
        shortFlag = flagParts[idx];
        ++idx;
    }
    // Allow two long flags, like '--ws, --workspace'
    if (!shortFlag.has_value() && idx < flagParts.size() &&
        std::regex_search(flagParts[idx], longFlagExp)) {
        shortFlag = longFlag;
        longFlag = flagParts[idx];
        ++idx;
    }

    // Check for unprocessed flag. Fail noisily rather than silently ignore.
    if (idx < flagParts.size() && !flagParts[idx].empty() && flagParts[idx][0] == '-') {
        const auto& unsupportedFlag = flagParts[idx];
        std::string baseError = "option creation failed due to '" + unsupportedFlag +
                                "' in option flags '" + flags + "'";
        // Short flag with more than one character: -ws
        if (std::regex_search(unsupportedFlag, std::regex("^-[^-][^-]"))) {
            throw std::runtime_error(
                baseError + "\n"
                "- a short flag is a single dash and a single character\n"
                "  - either use a single dash and a single character (for a short flag)\n"
                "  - or use a double dash for a long option (and can have two, like '--ws, --workspace')");
        }
        if (std::regex_search(unsupportedFlag, shortFlagExp)) {
            throw std::runtime_error(baseError + "\n- too many short flags");
        }
        if (std::regex_search(unsupportedFlag, longFlagExp)) {
            throw std::runtime_error(baseError + "\n- too many long flags");
        }
        throw std::runtime_error(baseError + "\n- unrecognised flag format");
    }

    if (!shortFlag.has_value() && !longFlag.has_value()) {
        throw std::runtime_error(
            "option creation failed due to no flags found in '" + flags + "'.");
    }

    return {shortFlag, longFlag};
}

inline Option::Option(const std::string& flags, const std::string& description)
    : flags(flags),
      description(description),
      required(flags.find('<') != std::string::npos),
      optional(flags.find('[') != std::string::npos),
      variadic(false),
      mandatory(false),
      negate(false),
      hidden(false) {

    // variadic test: /\w\.\.\.[>\]]$/
    // Checks that a word character precedes "..." followed by > or ]
    std::regex variadicExp("\\w\\.\\.\\.[>\\]]$");
    variadic = std::regex_search(flags, variadicExp);

    auto splitResult = splitOptionFlags(flags);
    short_ = splitResult.shortFlag;
    long_ = splitResult.longFlag;

    if (long_.has_value()) {
        const auto& l = *long_;
        negate = (l.size() > 4 && l.substr(0, 5) == "--no-");
    }
}

inline std::string Option::name() const {
    if (long_.has_value()) {
        // Strip leading "--"
        return long_->substr(2);
    }
    // Strip leading "-"
    return short_->substr(1);
}

inline std::string Option::attributeName() const {
    if (negate) {
        std::string n = name();
        // Strip "no-" prefix
        if (n.size() > 3 && n.substr(0, 3) == "no-") {
            return camelcase(n.substr(3));
        }
        return camelcase(n);
    }
    return camelcase(name());
}

inline bool Option::isBoolean() const {
    return !required && !optional && !negate;
}

inline bool Option::is(const std::string& arg) const {
    return (short_.has_value() && *short_ == arg) ||
           (long_.has_value() && *long_ == arg);
}

inline Option& Option::defaultValue(std::any value, const std::string& description) {
    defaultValue_ = std::move(value);
    defaultValueDescription_ = description;
    return *this;
}

inline Option& Option::preset(std::any arg) {
    presetArg_ = std::move(arg);
    return *this;
}

inline Option& Option::conflicts(const std::string& name) {
    conflictsWith_.push_back(name);
    return *this;
}

inline Option& Option::conflicts(const std::vector<std::string>& names) {
    conflictsWith_.insert(conflictsWith_.end(), names.begin(), names.end());
    return *this;
}

inline Option& Option::implies(const std::unordered_map<std::string, std::any>& values) {
    if (!implied_.has_value()) {
        implied_ = std::unordered_map<std::string, std::any>{};
    }
    for (const auto& [key, val] : values) {
        (*implied_)[key] = val;
    }
    return *this;
}

inline Option& Option::env(const std::string& name) {
    envVar_ = name;
    return *this;
}

inline Option& Option::argParser(ParseFn fn) {
    parseArg_ = std::move(fn);
    return *this;
}

inline Option& Option::makeOptionMandatory(bool mandatory) {
    this->mandatory = mandatory;
    return *this;
}

inline Option& Option::hideHelp(bool hide) {
    hidden = hide;
    return *this;
}

inline Option& Option::choices(const std::vector<std::string>& values) {
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

} // namespace commander
} // namespace polycpp
