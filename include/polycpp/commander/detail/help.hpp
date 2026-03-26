#pragma once

/**
 * @file detail/help.hpp
 * @brief Inline implementations for the Help class.
 *
 * This file comes AFTER command.hpp in the aggregator include order,
 * so the full Command definition is available.
 *
 * @since 0.1.0
 */

#include <polycpp/commander/help.hpp>
#include <polycpp/commander/command.hpp>

#include <algorithm>
#include <regex>
#include <sstream>
#include <string>
#include <vector>

namespace polycpp {
namespace commander {

inline std::string stripColor(const std::string& str) {
    // Strip SGR (Select Graphic Rendition) ANSI escape codes: \x1b[\d*(;\d*)*m
    std::regex sgrPattern("\\x1b\\[\\d*(;\\d*)*m");
    return std::regex_replace(str, sgrPattern, "");
}

inline void Help::prepareContext(int contextHelpWidth) {
    if (helpWidth == 0) {
        helpWidth = contextHelpWidth > 0 ? contextHelpWidth : 80;
    }
}

inline std::vector<const Command*> Help::visibleCommands(const Command& cmd) const {
    std::vector<const Command*> result;
    for (const auto& sub : cmd.commands) {
        if (!sub->hidden_) {
            result.push_back(sub.get());
        }
    }

    // Include implicit help command
    auto* helpCmd = const_cast<Command&>(cmd).getHelpCommand_();
    if (helpCmd && !helpCmd->hidden_) {
        result.push_back(helpCmd);
    }

    if (sortSubcommands) {
        std::sort(result.begin(), result.end(), [](const Command* a, const Command* b) {
            return a->name_ < b->name_;
        });
    }
    return result;
}

inline std::vector<Option> Help::visibleOptions(const Command& cmd) const {
    std::vector<Option> result;
    for (const auto& opt : cmd.options) {
        if (!opt.hidden) {
            result.push_back(opt);
        }
    }

    // Built-in help option
    auto* helpOpt = const_cast<Command&>(cmd).getHelpOption_();
    if (helpOpt && !helpOpt->hidden) {
        // Check for conflicting flags
        bool removeShort = helpOpt->short_.has_value() &&
                           cmd.findOption_(*helpOpt->short_) != nullptr;
        bool removeLong = helpOpt->long_.has_value() &&
                          cmd.findOption_(*helpOpt->long_) != nullptr;

        if (!removeShort && !removeLong) {
            result.push_back(*helpOpt);
        } else if (helpOpt->long_.has_value() && !removeLong) {
            result.push_back(cmd.createOption(*helpOpt->long_, helpOpt->description));
        } else if (helpOpt->short_.has_value() && !removeShort) {
            result.push_back(cmd.createOption(*helpOpt->short_, helpOpt->description));
        }
    }

    if (sortOptions) {
        std::sort(result.begin(), result.end(), [](const Option& a, const Option& b) {
            auto getSortKey = [](const Option& opt) -> std::string {
                if (opt.short_.has_value()) {
                    std::string s = *opt.short_;
                    if (!s.empty() && s[0] == '-') s = s.substr(1);
                    return s;
                }
                if (opt.long_.has_value()) {
                    std::string l = *opt.long_;
                    if (l.size() >= 2 && l[0] == '-' && l[1] == '-') l = l.substr(2);
                    return l;
                }
                return "";
            };
            return getSortKey(a) < getSortKey(b);
        });
    }
    return result;
}

inline std::vector<Option> Help::visibleGlobalOptions(const Command& cmd) const {
    if (!showGlobalOptions) return {};

    std::vector<Option> result;
    for (auto* ancestor = cmd.parent; ancestor != nullptr; ancestor = ancestor->parent) {
        for (const auto& opt : ancestor->options) {
            if (!opt.hidden) {
                result.push_back(opt);
            }
        }
    }

    if (sortOptions) {
        std::sort(result.begin(), result.end(), [](const Option& a, const Option& b) {
            auto getSortKey = [](const Option& opt) -> std::string {
                if (opt.short_.has_value()) {
                    std::string s = *opt.short_;
                    if (!s.empty() && s[0] == '-') s = s.substr(1);
                    return s;
                }
                if (opt.long_.has_value()) {
                    std::string l = *opt.long_;
                    if (l.size() >= 2 && l[0] == '-' && l[1] == '-') l = l.substr(2);
                    return l;
                }
                return "";
            };
            return getSortKey(a) < getSortKey(b);
        });
    }
    return result;
}

inline std::vector<const Argument*> Help::visibleArguments(const Command& cmd) const {
    // If any argument has a description, return all of them
    bool anyDescription = false;
    for (const auto& arg : cmd.registeredArguments) {
        if (!arg.description.empty()) {
            anyDescription = true;
            break;
        }
    }
    if (!anyDescription) return {};

    std::vector<const Argument*> result;
    for (const auto& arg : cmd.registeredArguments) {
        result.push_back(&arg);
    }
    return result;
}

inline std::string Help::subcommandTerm(const Command& cmd) const {
    std::string args;
    for (const auto& arg : cmd.registeredArguments) {
        if (!args.empty()) args += ' ';
        args += humanReadableArgName(arg);
    }

    std::string result = cmd.name_;
    if (!cmd.aliases_.empty()) {
        result += '|' + cmd.aliases_[0];
    }
    if (!cmd.options.empty()) {
        result += " [options]";
    }
    if (!args.empty()) {
        result += ' ' + args;
    }
    return result;
}

inline std::string Help::subcommandDescription(const Command& cmd) const {
    std::string s = cmd.summary_;
    if (!s.empty()) return s;
    return cmd.description_;
}

inline std::string Help::optionTerm(const Option& option) const {
    return option.flags;
}

inline std::string Help::optionDescription(const Option& option) const {
    std::vector<std::string> extraInfo;

    if (option.argChoices_.has_value()) {
        std::string choicesStr = "choices: ";
        const auto& choices = *option.argChoices_;
        for (size_t i = 0; i < choices.size(); ++i) {
            if (i > 0) choicesStr += ", ";
            choicesStr += "\"" + choices[i] + "\"";
        }
        extraInfo.push_back(choicesStr);
    }

    if (option.defaultValue_.has_value()) {
        bool showDefault = option.required || option.optional ||
                           (option.isBoolean() &&
                            option.defaultValue_.type() == typeid(bool));
        if (showDefault) {
            std::string defStr;
            if (!option.defaultValueDescription_.empty()) {
                defStr = option.defaultValueDescription_;
            } else if (option.defaultValue_.type() == typeid(std::string)) {
                defStr = "\"" + std::any_cast<std::string>(option.defaultValue_) + "\"";
            } else if (option.defaultValue_.type() == typeid(bool)) {
                defStr = std::any_cast<bool>(option.defaultValue_) ? "true" : "false";
            } else if (option.defaultValue_.type() == typeid(int)) {
                defStr = std::to_string(std::any_cast<int>(option.defaultValue_));
            } else if (option.defaultValue_.type() == typeid(double)) {
                defStr = std::to_string(std::any_cast<double>(option.defaultValue_));
            } else {
                defStr = "(complex)";
            }
            extraInfo.push_back("default: " + defStr);
        }
    }

    if (option.presetArg_.has_value() && option.optional) {
        std::string presetStr;
        if (option.presetArg_.type() == typeid(std::string)) {
            presetStr = "\"" + std::any_cast<std::string>(option.presetArg_) + "\"";
        } else if (option.presetArg_.type() == typeid(bool)) {
            presetStr = std::any_cast<bool>(option.presetArg_) ? "true" : "false";
        } else {
            presetStr = "(value)";
        }
        extraInfo.push_back("preset: " + presetStr);
    }

    if (option.envVar_.has_value()) {
        extraInfo.push_back("env: " + *option.envVar_);
    }

    if (!extraInfo.empty()) {
        std::string extra = "(";
        for (size_t i = 0; i < extraInfo.size(); ++i) {
            if (i > 0) extra += ", ";
            extra += extraInfo[i];
        }
        extra += ")";
        if (!option.description.empty()) {
            return option.description + " " + extra;
        }
        return extra;
    }
    return option.description;
}

inline std::string Help::argumentTerm(const Argument& argument) const {
    return argument.name();
}

inline std::string Help::argumentDescription(const Argument& argument) const {
    std::vector<std::string> extraInfo;

    if (argument.argChoices_.has_value()) {
        std::string choicesStr = "choices: ";
        const auto& choices = *argument.argChoices_;
        for (size_t i = 0; i < choices.size(); ++i) {
            if (i > 0) choicesStr += ", ";
            choicesStr += "\"" + choices[i] + "\"";
        }
        extraInfo.push_back(choicesStr);
    }

    if (argument.defaultValue_.has_value()) {
        std::string defStr;
        if (!argument.defaultValueDescription_.empty()) {
            defStr = argument.defaultValueDescription_;
        } else if (argument.defaultValue_.type() == typeid(std::string)) {
            defStr = "\"" + std::any_cast<std::string>(argument.defaultValue_) + "\"";
        } else if (argument.defaultValue_.type() == typeid(int)) {
            defStr = std::to_string(std::any_cast<int>(argument.defaultValue_));
        } else {
            defStr = "(complex)";
        }
        extraInfo.push_back("default: " + defStr);
    }

    if (!extraInfo.empty()) {
        std::string extra = "(";
        for (size_t i = 0; i < extraInfo.size(); ++i) {
            if (i > 0) extra += ", ";
            extra += extraInfo[i];
        }
        extra += ")";
        if (!argument.description.empty()) {
            return argument.description + " " + extra;
        }
        return extra;
    }
    return argument.description;
}

inline int Help::longestSubcommandTermLength(const Command& cmd) const {
    int maxLen = 0;
    for (const auto* sub : visibleCommands(cmd)) {
        int len = displayWidth(subcommandTerm(*sub));
        if (len > maxLen) maxLen = len;
    }
    return maxLen;
}

inline int Help::longestOptionTermLength(const Command& cmd) const {
    int maxLen = 0;
    for (const auto& opt : visibleOptions(cmd)) {
        int len = displayWidth(optionTerm(opt));
        if (len > maxLen) maxLen = len;
    }
    return maxLen;
}

inline int Help::longestGlobalOptionTermLength(const Command& cmd) const {
    int maxLen = 0;
    for (const auto& opt : visibleGlobalOptions(cmd)) {
        int len = displayWidth(optionTerm(opt));
        if (len > maxLen) maxLen = len;
    }
    return maxLen;
}

inline int Help::longestArgumentTermLength(const Command& cmd) const {
    int maxLen = 0;
    for (const auto* arg : visibleArguments(cmd)) {
        int len = displayWidth(argumentTerm(*arg));
        if (len > maxLen) maxLen = len;
    }
    return maxLen;
}

inline std::string Help::commandUsage(const Command& cmd) const {
    std::string cmdName = cmd.name_;
    if (!cmd.aliases_.empty()) {
        cmdName += '|' + cmd.aliases_[0];
    }

    std::string ancestorNames;
    for (auto* ancestor = cmd.parent; ancestor != nullptr; ancestor = ancestor->parent) {
        ancestorNames = ancestor->name() + " " + ancestorNames;
    }

    return ancestorNames + cmdName + " " + cmd.usage();
}

inline std::string Help::commandDescription(const Command& cmd) const {
    return cmd.description_;
}

inline int Help::padWidth(const Command& cmd) const {
    return std::max({
        longestOptionTermLength(cmd),
        longestGlobalOptionTermLength(cmd),
        longestSubcommandTermLength(cmd),
        longestArgumentTermLength(cmd)
    });
}

inline int Help::displayWidth(const std::string& str) const {
    return static_cast<int>(stripColor(str).size());
}

inline bool Help::preformatted(const std::string& str) const {
    // Check for newline followed by whitespace (but not \r)
    for (size_t i = 0; i < str.size(); ++i) {
        if (str[i] == '\n' && i + 1 < str.size()) {
            char next = str[i + 1];
            if (next != '\n' && next != '\r' && (next == ' ' || next == '\t')) {
                return true;
            }
        }
    }
    return false;
}

inline std::string Help::boxWrap(const std::string& str, int width) const {
    if (width < minWidthToWrap) return str;

    // Split by line breaks
    std::vector<std::string> rawLines;
    {
        std::istringstream stream(str);
        std::string line;
        while (std::getline(stream, line)) {
            // Remove trailing \r if present
            if (!line.empty() && line.back() == '\r') {
                line.pop_back();
            }
            rawLines.push_back(line);
        }
        // Handle case where string ends with newline
        if (!str.empty() && str.back() == '\n') {
            rawLines.push_back("");
        }
    }

    std::vector<std::string> wrappedLines;
    for (const auto& line : rawLines) {
        // Split into whitespace+word chunks
        std::vector<std::string> chunks;
        {
            std::regex chunkPattern("[\\s]*[^\\s]+");
            auto begin = std::sregex_iterator(line.begin(), line.end(), chunkPattern);
            auto end = std::sregex_iterator();
            for (auto it = begin; it != end; ++it) {
                chunks.push_back(it->str());
            }
        }

        if (chunks.empty()) {
            wrappedLines.push_back("");
            continue;
        }

        std::vector<std::string> sumChunks;
        sumChunks.push_back(chunks[0]);
        int sumWidth = displayWidth(chunks[0]);

        for (size_t i = 1; i < chunks.size(); ++i) {
            int visibleWidth = displayWidth(chunks[i]);
            if (sumWidth + visibleWidth <= width) {
                sumChunks.push_back(chunks[i]);
                sumWidth += visibleWidth;
            } else {
                // Flush accumulated chunks
                std::string joined;
                for (const auto& c : sumChunks) joined += c;
                wrappedLines.push_back(joined);

                // Trim leading space for next line
                std::string nextChunk = chunks[i];
                size_t start = nextChunk.find_first_not_of(" \t");
                if (start != std::string::npos) {
                    nextChunk = nextChunk.substr(start);
                }
                sumChunks.clear();
                sumChunks.push_back(nextChunk);
                sumWidth = displayWidth(nextChunk);
            }
        }
        // Flush remaining
        std::string joined;
        for (const auto& c : sumChunks) joined += c;
        wrappedLines.push_back(joined);
    }

    std::string result;
    for (size_t i = 0; i < wrappedLines.size(); ++i) {
        if (i > 0) result += '\n';
        result += wrappedLines[i];
    }
    return result;
}

inline std::string Help::formatItem(const std::string& term, int termWidth,
                                     const std::string& description) const {
    const int itemIndent = 2;
    std::string itemIndentStr(itemIndent, ' ');
    if (description.empty()) return itemIndentStr + term;

    // Pad the term to consistent width
    int termDisplayWidth = displayWidth(term);
    int padding = termWidth - termDisplayWidth;
    std::string paddedTerm = term;
    if (padding > 0) {
        paddedTerm += std::string(padding, ' ');
    }

    // Format the description
    const int spacerWidth = 2;
    int hw = helpWidth > 0 ? helpWidth : 80;
    int remainingWidth = hw - termWidth - spacerWidth - itemIndent;

    std::string formattedDescription;
    if (remainingWidth < minWidthToWrap || preformatted(description)) {
        formattedDescription = description;
    } else {
        std::string wrappedDescription = boxWrap(description, remainingWidth);
        // Indent continuation lines
        std::string contIndent(termWidth + spacerWidth, ' ');
        std::string result2;
        for (size_t i = 0; i < wrappedDescription.size(); ++i) {
            result2 += wrappedDescription[i];
            if (wrappedDescription[i] == '\n') {
                result2 += contIndent;
            }
        }
        formattedDescription = result2;
    }

    // Construct with overall indent
    std::string result = itemIndentStr + paddedTerm + std::string(spacerWidth, ' ');
    // Add formattedDescription, indenting all newlines
    for (size_t i = 0; i < formattedDescription.size(); ++i) {
        result += formattedDescription[i];
        if (formattedDescription[i] == '\n') {
            result += itemIndentStr;
        }
    }
    return result;
}

inline std::string Help::formatHelp(const Command& cmd, const Help& helper) const {
    int termWidth = helper.padWidth(cmd);

    auto callFormatItem = [&](const std::string& term, const std::string& description) {
        return helper.formatItem(term, termWidth, description);
    };

    // Usage
    std::string output = "Usage: " + helper.commandUsage(cmd) + "\n";

    // Description
    std::string desc = helper.commandDescription(cmd);
    if (!desc.empty()) {
        int hw = helper.helpWidth > 0 ? helper.helpWidth : 80;
        output += "\n" + helper.boxWrap(desc, hw) + "\n";
    }

    // Arguments
    auto visArgs = helper.visibleArguments(cmd);
    if (!visArgs.empty()) {
        output += "\nArguments:\n";
        for (const auto* arg : visArgs) {
            output += callFormatItem(helper.argumentTerm(*arg),
                                      helper.argumentDescription(*arg)) + "\n";
        }
    }

    // Options
    auto visOpts = helper.visibleOptions(cmd);
    if (!visOpts.empty()) {
        output += "\nOptions:\n";
        for (const auto& opt : visOpts) {
            output += callFormatItem(helper.optionTerm(opt),
                                      helper.optionDescription(opt)) + "\n";
        }
    }

    // Global Options
    if (helper.showGlobalOptions) {
        auto globalOpts = helper.visibleGlobalOptions(cmd);
        if (!globalOpts.empty()) {
            output += "\nGlobal Options:\n";
            for (const auto& opt : globalOpts) {
                output += callFormatItem(helper.optionTerm(opt),
                                          helper.optionDescription(opt)) + "\n";
            }
        }
    }

    // Commands
    auto visCmds = helper.visibleCommands(cmd);
    if (!visCmds.empty()) {
        output += "\nCommands:\n";
        for (const auto* sub : visCmds) {
            output += callFormatItem(helper.subcommandTerm(*sub),
                                      helper.subcommandDescription(*sub)) + "\n";
        }
    }

    return output;
}

} // namespace commander
} // namespace polycpp
