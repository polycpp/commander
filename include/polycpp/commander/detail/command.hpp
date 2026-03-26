#pragma once

/**
 * @file detail/command.hpp
 * @brief Inline implementations for the Command class.
 *
 * This file comes after all declarations and detail/help.hpp in the aggregator,
 * so all types are fully defined.
 *
 * @since 0.1.0
 */

#include <polycpp/commander/command.hpp>
#include <polycpp/commander/suggest_similar.hpp>

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <regex>
#include <sstream>
#include <stdexcept>

#include <sys/wait.h>
#include <unistd.h>

namespace polycpp {
namespace commander {

// ---- Constructor ----

inline Command::Command(const std::string& name)
    : name_(name) {
    // Set up default output configuration
    outputConfiguration_.writeOut = [](const std::string& str) {
        std::cout << str;
    };
    outputConfiguration_.writeErr = [](const std::string& str) {
        std::cerr << str;
    };
    outputConfiguration_.outputError = [](const std::string& str,
                                           std::function<void(const std::string&)> write) {
        write(str);
    };
    outputConfiguration_.getOutHelpWidth = []() -> int { return 80; };
    outputConfiguration_.getErrHelpWidth = []() -> int { return 80; };
}

// ---- Metadata getters/setters ----

inline std::string Command::name() const { return name_; }
inline Command& Command::name(const std::string& str) { name_ = str; return *this; }

inline std::string Command::description() const { return description_; }
inline Command& Command::description(const std::string& str) { description_ = str; return *this; }

inline std::string Command::summary() const { return summary_; }
inline Command& Command::summary(const std::string& str) { summary_ = str; return *this; }

inline std::string Command::usage() const {
    if (!usage_.empty()) return usage_;

    std::vector<std::string> parts;
    if (!options.empty() || !helpOption_.has_value() || helpOption_ != std::nullopt) {
        // Has options or help option not disabled
        bool hasHelpOption = true;
        if (helpOption_.has_value() && !*helpOption_) {
            hasHelpOption = false;
        }
        if (!options.empty() || hasHelpOption) {
            parts.push_back("[options]");
        }
    }
    if (!commands.empty()) {
        parts.push_back("[command]");
    }
    for (const auto& arg : registeredArguments) {
        parts.push_back(humanReadableArgName(arg));
    }

    std::string result;
    for (size_t i = 0; i < parts.size(); ++i) {
        if (i > 0) result += ' ';
        result += parts[i];
    }
    return result;
}

inline Command& Command::usage(const std::string& str) { usage_ = str; return *this; }

inline std::string Command::version() const { return version_; }

inline Command& Command::version(const std::string& str,
                                  const std::string& flags,
                                  const std::string& desc) {
    version_ = str;
    Option versionOption = createOption(flags, desc);
    versionOptionName_ = versionOption.attributeName();
    registerOption_(versionOption);

    std::string versionStr = str;
    on("option:" + versionOption.name(), [this, versionStr](const std::vector<std::any>&) {
        outputConfiguration_.writeOut(versionStr + "\n");
        exit_(0, "commander.version", versionStr);
    });
    return *this;
}

// ---- Aliases ----

inline std::string Command::alias() const {
    if (aliases_.empty()) return "";
    return aliases_[0];
}

inline Command& Command::alias(const std::string& aliasName) {
    if (aliasName == name_) {
        throw std::runtime_error("Command alias can't be the same as its name");
    }
    aliases_.push_back(aliasName);
    return *this;
}

inline std::vector<std::string> Command::aliases() const { return aliases_; }

inline Command& Command::aliases(const std::vector<std::string>& aliasNames) {
    for (const auto& a : aliasNames) {
        alias(a);
    }
    return *this;
}

inline Command& Command::nameFromFilename(const std::string& filename) {
    // Extract basename without extension
    std::string name = filename;
    // Find last separator
    auto pos = name.find_last_of("/\\");
    if (pos != std::string::npos) {
        name = name.substr(pos + 1);
    }
    // Remove extension
    auto dotPos = name.rfind('.');
    if (dotPos != std::string::npos) {
        name = name.substr(0, dotPos);
    }
    name_ = name;
    return *this;
}

// ---- Options ----

inline Command& Command::option(const std::string& flags, const std::string& description) {
    Option opt = createOption(flags, description);
    return addOption(std::move(opt));
}

inline Command& Command::option(const std::string& flags, const std::string& description,
                                 const std::any& defaultValue) {
    Option opt = createOption(flags, description);
    opt.defaultValue(defaultValue);
    return addOption(std::move(opt));
}

inline Command& Command::option(const std::string& flags, const std::string& description,
                                 ParseFn parseArg, const std::any& defaultValue) {
    Option opt = createOption(flags, description);
    if (defaultValue.has_value()) {
        opt.defaultValue(defaultValue);
    }
    opt.argParser(std::move(parseArg));
    return addOption(std::move(opt));
}

inline Command& Command::requiredOption(const std::string& flags, const std::string& description) {
    Option opt = createOption(flags, description);
    opt.makeOptionMandatory(true);
    return addOption(std::move(opt));
}

inline Command& Command::requiredOption(const std::string& flags, const std::string& description,
                                         const std::any& defaultValue) {
    Option opt = createOption(flags, description);
    opt.makeOptionMandatory(true);
    opt.defaultValue(defaultValue);
    return addOption(std::move(opt));
}

inline Command& Command::requiredOption(const std::string& flags, const std::string& description,
                                         ParseFn parseArg, const std::any& defaultValue) {
    Option opt = createOption(flags, description);
    opt.makeOptionMandatory(true);
    if (defaultValue.has_value()) {
        opt.defaultValue(defaultValue);
    }
    opt.argParser(std::move(parseArg));
    return addOption(std::move(opt));
}

inline Command& Command::addOption(Option option) {
    registerOption_(option);

    const std::string oname = option.name();
    const std::string attrName = option.attributeName();

    // Store default value
    if (option.negate) {
        // --no-foo defaults foo to true, unless a --foo option already exists
        std::string positiveLongFlag = option.long_.has_value()
            ? std::regex_replace(*option.long_, std::regex("^--no-"), "--")
            : "";
        if (!positiveLongFlag.empty() && findOption_(positiveLongFlag) == nullptr) {
            if (option.defaultValue_.has_value()) {
                setOptionValueWithSource(attrName, option.defaultValue_, "default");
            } else {
                setOptionValueWithSource(attrName, std::any(true), "default");
            }
        }
    } else if (option.defaultValue_.has_value()) {
        setOptionValueWithSource(attrName, option.defaultValue_, "default");
    }

    // Capture option properties for the lambda closures
    bool optRequired = option.required;
    bool optOptional = option.optional;
    bool optNegate = option.negate;
    bool optVariadic = option.variadic;
    bool optIsBoolean = option.isBoolean();
    bool hasParseArg = static_cast<bool>(option.parseArg_);
    bool hasPresetArg = option.presetArg_.has_value();
    std::any presetArg = option.presetArg_;
    std::string optFlags = option.flags;

    // Handler for cli and env supplied values
    auto handleOptionValue = [this, attrName, optNegate, optIsBoolean, optOptional,
                               optVariadic, hasParseArg, hasPresetArg, presetArg,
                               optFlags](std::any val, const std::string& invalidValueMessage,
                                         const std::string& valueSource) {
        // val is empty for boolean/negate, or null for optional without arg
        if (!val.has_value() && hasPresetArg) {
            val = presetArg;
        }

        // custom processing
        std::any oldValue = getOptionValue(attrName);

        if (val.has_value() && hasParseArg) {
            // Find the option to get parseArg_
            for (auto& opt : options) {
                if (opt.attributeName() == attrName && opt.parseArg_) {
                    try {
                        std::string valStr;
                        if (val.type() == typeid(std::string)) {
                            valStr = std::any_cast<std::string>(val);
                        }
                        val = opt.parseArg_(valStr, oldValue);
                    } catch (const InvalidArgumentError& err) {
                        std::string message = invalidValueMessage + " " + std::string(err.what()).substr(7); // strip "Error: "
                        error(message, {err.exitCode, err.code});
                        throw; // won't reach here if error() exits
                    }
                    break;
                }
            }
        } else if (val.has_value() && optVariadic) {
            // Collect into vector
            std::string valStr;
            if (val.type() == typeid(std::string)) {
                valStr = std::any_cast<std::string>(val);
            }
            if (oldValue.has_value()) {
                try {
                    auto vec = std::any_cast<std::vector<std::string>>(oldValue);
                    vec.push_back(valStr);
                    val = std::any(std::move(vec));
                } catch (const std::bad_any_cast&) {
                    val = std::any(std::vector<std::string>{valStr});
                }
            } else {
                val = std::any(std::vector<std::string>{valStr});
            }
        }

        // Fill in appropriate missing values
        if (!val.has_value()) {
            if (optNegate) {
                val = std::any(false);
            } else if (optIsBoolean || optOptional) {
                val = std::any(true);
            } else {
                val = std::any(std::string(""));
            }
        }

        setOptionValueWithSource(attrName, val, valueSource);
    };

    on("option:" + oname, [handleOptionValue, optFlags](const std::vector<std::any>& eventArgs) {
        std::any val;
        if (!eventArgs.empty()) {
            val = eventArgs[0];
        }
        std::string valStr;
        if (val.has_value() && val.type() == typeid(std::string)) {
            valStr = std::any_cast<std::string>(val);
        }
        std::string invalidValueMessage = "error: option '" + optFlags + "' argument '" + valStr + "' is invalid.";
        handleOptionValue(val, invalidValueMessage, "cli");
    });

    if (option.envVar_.has_value()) {
        on("optionEnv:" + oname, [handleOptionValue, optFlags, option](const std::vector<std::any>& eventArgs) {
            std::any val;
            if (!eventArgs.empty()) {
                val = eventArgs[0];
            }
            std::string valStr;
            if (val.has_value() && val.type() == typeid(std::string)) {
                valStr = std::any_cast<std::string>(val);
            }
            std::string invalidValueMessage = "error: option '" + optFlags + "' value '" + valStr +
                                               "' from env '" + *option.envVar_ + "' is invalid.";
            handleOptionValue(val, invalidValueMessage, "env");
        });
    }

    return *this;
}

inline Option Command::createOption(const std::string& flags, const std::string& description) const {
    return Option(flags, description);
}

// ---- Arguments ----

inline Command& Command::argument(const std::string& name, const std::string& description) {
    Argument arg = createArgument(name, description);
    return addArgument(std::move(arg));
}

inline Command& Command::arguments(const std::string& names) {
    std::istringstream stream(names);
    std::string detail;
    while (stream >> detail) {
        argument(detail);
    }
    return *this;
}

inline Command& Command::addArgument(Argument argument) {
    // Check: only last argument can be variadic
    if (!registeredArguments.empty() && registeredArguments.back().variadic) {
        throw std::runtime_error(
            "only the last argument can be variadic '" + registeredArguments.back().name() + "'");
    }
    // Check: required arg with default and no parser doesn't make sense
    if (argument.required && argument.defaultValue_.has_value() && !argument.parseArg_) {
        throw std::runtime_error(
            "a default value for a required argument is never used: '" + argument.name() + "'");
    }
    registeredArguments.push_back(std::move(argument));
    return *this;
}

inline Argument Command::createArgument(const std::string& name, const std::string& description) const {
    return Argument(name, description);
}

// ---- Subcommands ----

inline Command& Command::command(const std::string& nameAndArgs, const CommandOptions& opts) {
    // Parse "name <required> [optional]"
    std::regex re("^([^ ]+)\\s*(.*)$");
    std::smatch match;
    std::string cmdName, argsDef;
    if (std::regex_match(nameAndArgs, match, re)) {
        cmdName = match[1].str();
        argsDef = match[2].str();
    } else {
        cmdName = nameAndArgs;
    }

    auto cmd = createCommand(cmdName);
    Command& cmdRef = *cmd;

    if (opts.isDefault) defaultCommandName_ = cmdRef.name_;
    cmdRef.hidden_ = opts.hidden;
    if (!argsDef.empty()) cmdRef.arguments(argsDef);

    registerCommand_(*cmd);
    cmd->parent = this;
    cmd->copyInheritedSettings(*this);

    commands.push_back(std::move(cmd));
    return cmdRef;
}

inline Command& Command::addCommand(std::unique_ptr<Command> cmd, const CommandOptions& opts) {
    if (cmd->name_.empty()) {
        throw std::runtime_error(
            "Command passed to .addCommand() must have a name\n"
            "- specify the name in Command constructor or using .name()");
    }

    if (opts.isDefault) defaultCommandName_ = cmd->name_;
    if (opts.hidden) cmd->hidden_ = true;

    registerCommand_(*cmd);
    cmd->parent = this;

    commands.push_back(std::move(cmd));
    return *this;
}

inline std::unique_ptr<Command> Command::createCommand(const std::string& name) const {
    return std::make_unique<Command>(name);
}

// ---- Executable Subcommands ----

inline Command& Command::executableCommand(const std::string& nameAndArgs,
                                            const std::string& description,
                                            const std::string& executableFile) {
    // Parse "name <required> [optional]"
    std::regex re("^([^ ]+)\\s*(.*)$");
    std::smatch match;
    std::string cmdName, argsDef;
    if (std::regex_match(nameAndArgs, match, re)) {
        cmdName = match[1].str();
        argsDef = match[2].str();
    } else {
        cmdName = nameAndArgs;
    }

    auto cmd = createCommand(cmdName);
    cmd->description(description);
    cmd->executableHandler_ = true;
    if (!executableFile.empty()) {
        cmd->executableFile_ = executableFile;
    }
    if (!argsDef.empty()) cmd->arguments(argsDef);

    registerCommand_(*cmd);
    cmd->parent = this;
    cmd->copyInheritedSettings(*this);

    commands.push_back(std::move(cmd));
    return *this; // Return parent, not subcommand
}

inline Command& Command::executableDir(const std::string& path) {
    executableDir_ = path;
    return *this;
}

inline std::string Command::executableDir() const {
    return executableDir_.value_or("");
}

// ---- Action ----

inline Command& Command::action(ActionFn fn) {
    actionHandler_ = [this, fn = std::move(fn)](const std::vector<std::any>& processedArgs) {
        auto optsMap = opts();
        fn(processedArgs, optsMap, *this);
    };
    return *this;
}

// ---- Parsing ----

inline Command& Command::parse() {
    // Use empty vector to signal "use process::argv()"
    return parse({}, {.from = "node"});
}

inline Command& Command::parse(const std::vector<std::string>& argv, const ParseOptions& parseOpts) {
    prepareForParse_();
    auto userArgs = prepareUserArgs_(argv, parseOpts);
    parseCommand_({}, userArgs);
    return *this;
}

inline ParseOptionsResult Command::parseOptions(const std::vector<std::string>& inputArgs) {
    std::vector<std::string> operands;
    std::vector<std::string> unknown;
    bool destIsUnknown = false;

    auto maybeOption = [](const std::string& arg) -> bool {
        return arg.size() > 1 && arg[0] == '-';
    };

    // Check if arg looks like a negative number and digit isn't used as short option
    auto negativeNumberArg = [this](const std::string& arg) -> bool {
        std::regex negNumRe("^-(\\d+|\\d*\\.\\d+)(e[+-]?\\d+)?$");
        if (!std::regex_match(arg, negNumRe)) return false;
        // Check no digit used as short option in command hierarchy
        for (auto* cmd : getCommandAndAncestors_()) {
            for (const auto& opt : cmd->options) {
                if (opt.short_.has_value()) {
                    std::regex digitShortRe("^-\\d$");
                    if (std::regex_match(*opt.short_, digitShortRe)) {
                        return false;
                    }
                }
            }
        }
        return true;
    };

    const Option* activeVariadicOption = nullptr;
    std::string activeGroup; // working through group of short options
    size_t i = 0;

    while (i < inputArgs.size() || !activeGroup.empty()) {
        std::string arg;
        if (!activeGroup.empty()) {
            arg = activeGroup;
            activeGroup.clear();
        } else {
            arg = inputArgs[i++];
        }

        // literal "--"
        if (arg == "--") {
            if (destIsUnknown) unknown.push_back(arg);
            // Push rest
            while (i < inputArgs.size()) {
                auto& dest = destIsUnknown ? unknown : operands;
                dest.push_back(inputArgs[i++]);
            }
            break;
        }

        // Active variadic option collects non-option args
        if (activeVariadicOption && (!maybeOption(arg) || negativeNumberArg(arg))) {
            emit("option:" + activeVariadicOption->name(), std::string(arg));
            continue;
        }
        activeVariadicOption = nullptr;

        if (maybeOption(arg)) {
            const Option* option = findOption_(arg);
            if (option) {
                if (option->required) {
                    if (i >= inputArgs.size()) {
                        optionMissingArgument(*option);
                    }
                    std::string value = inputArgs[i++];
                    emit("option:" + option->name(), std::string(value));
                } else if (option->optional) {
                    // Historical behaviour: optional value is following arg unless it's an option
                    std::any value;
                    if (i < inputArgs.size() && (!maybeOption(inputArgs[i]) || negativeNumberArg(inputArgs[i]))) {
                        value = std::any(std::string(inputArgs[i++]));
                    }
                    emit("option:" + option->name(), value.has_value() ? value : std::any());
                } else {
                    // boolean flag
                    emit("option:" + option->name());
                }
                activeVariadicOption = option->variadic ? option : nullptr;
                continue;
            }
        }

        // Look for combo options: -abc
        if (arg.size() > 2 && arg[0] == '-' && arg[1] != '-') {
            const Option* option = findOption_("-" + std::string(1, arg[1]));
            if (option) {
                if (option->required ||
                    (option->optional && combineFlagAndOptionalValue_)) {
                    // Value following in same argument
                    emit("option:" + option->name(), std::string(arg.substr(2)));
                } else {
                    // Boolean; remove processed option and keep processing group
                    emit("option:" + option->name());
                    activeGroup = "-" + arg.substr(2);
                }
                continue;
            }
        }

        // Look for --flag=value
        if (arg.size() > 2 && arg[0] == '-' && arg[1] == '-') {
            auto eqPos = arg.find('=');
            if (eqPos != std::string::npos) {
                const Option* option = findOption_(arg.substr(0, eqPos));
                if (option && (option->required || option->optional)) {
                    emit("option:" + option->name(), std::string(arg.substr(eqPos + 1)));
                    continue;
                }
            }
        }

        // Not a recognised option
        if (!destIsUnknown && maybeOption(arg) && !(commands.empty() && negativeNumberArg(arg))) {
            destIsUnknown = true;
        }

        // Positional options: stop at subcommand
        if ((enablePositionalOptions_ || passThroughOptions_) &&
            operands.empty() && unknown.empty()) {
            if (findCommand_(arg)) {
                operands.push_back(arg);
                while (i < inputArgs.size()) {
                    unknown.push_back(inputArgs[i++]);
                }
                break;
            } else if (getHelpCommand_() && arg == getHelpCommand_()->name()) {
                operands.push_back(arg);
                while (i < inputArgs.size()) {
                    operands.push_back(inputArgs[i++]);
                }
                break;
            } else if (!defaultCommandName_.empty()) {
                while (i < inputArgs.size()) {
                    unknown.push_back(inputArgs[i++]);
                }
                unknown.insert(unknown.begin(), arg);
                break;
            }
        }

        // Pass through options: stop at first command-argument
        if (passThroughOptions_) {
            auto& dest = destIsUnknown ? unknown : operands;
            dest.push_back(arg);
            while (i < inputArgs.size()) {
                dest.push_back(inputArgs[i++]);
            }
            break;
        }

        // Add arg to appropriate destination
        if (destIsUnknown) {
            unknown.push_back(arg);
        } else {
            operands.push_back(arg);
        }
    }

    return {operands, unknown};
}

// ---- Option values ----

inline OptionValues Command::opts() const {
    return optionValues_;
}

inline OptionValues Command::optsWithGlobals() const {
    OptionValues result;
    auto ancestors = getCommandAndAncestors_();
    for (auto it = ancestors.rbegin(); it != ancestors.rend(); ++it) {
        auto cmdOpts = (*it)->opts();
        for (auto& [key, val] : cmdOpts) {
            result[key] = val;
        }
    }
    return result;
}

inline std::any Command::getOptionValue(const std::string& key) const {
    auto it = optionValues_.find(key);
    if (it != optionValues_.end()) return it->second;
    return {};
}

inline Command& Command::setOptionValue(const std::string& key, std::any value) {
    return setOptionValueWithSource(key, std::move(value), "");
}

inline Command& Command::setOptionValueWithSource(const std::string& key, std::any value,
                                                    const std::string& source) {
    optionValues_[key] = std::move(value);
    optionValueSources_[key] = source;
    return *this;
}

inline std::string Command::getOptionValueSource(const std::string& key) const {
    auto it = optionValueSources_.find(key);
    if (it != optionValueSources_.end()) return it->second;
    return "";
}

inline std::string Command::getOptionValueSourceWithGlobals(const std::string& key) const {
    std::string source;
    for (auto* cmd : getCommandAndAncestors_()) {
        auto s = cmd->getOptionValueSource(key);
        if (!s.empty()) {
            source = s;
        }
    }
    return source;
}

// ---- Help configuration ----

inline Command& Command::helpOption(const std::string& flags, const std::string& description) {
    std::string f = flags.empty() ? "-h, --help" : flags;
    std::string d = description.empty() ? "display help for command" : description;
    helpOption_ = std::make_unique<Option>(f, d);
    return *this;
}

inline Command& Command::helpOption(bool enable) {
    if (!enable) {
        helpOption_ = std::unique_ptr<Option>(nullptr); // disabled
    } else {
        helpOption_.reset(); // re-enable lazy creation
    }
    return *this;
}

inline Command& Command::helpCommand(const std::string& nameAndArgs, const std::string& description) {
    std::string effectiveNameAndArgs = nameAndArgs.empty() ? "help [command]" : nameAndArgs;
    std::string effectiveDescription = description.empty() ? "display help for command" : description;

    // Parse "name <args...>"
    std::regex re("([^ ]+) *(.*)");
    std::smatch match;
    std::string helpName, helpArgs;
    if (std::regex_match(effectiveNameAndArgs, match, re)) {
        helpName = match[1].str();
        helpArgs = match[2].str();
    } else {
        helpName = effectiveNameAndArgs;
    }

    auto helpCmd = createCommand(helpName);
    helpCmd->helpOption(false);
    if (!helpArgs.empty()) helpCmd->arguments(helpArgs);
    if (!effectiveDescription.empty()) helpCmd->description(effectiveDescription);

    addImplicitHelpCommand_ = true;
    helpCommand_ = std::move(helpCmd);
    return *this;
}

inline Command& Command::helpCommand(bool enable) {
    addImplicitHelpCommand_ = enable;
    return *this;
}

inline Command& Command::addHelpCommand(std::unique_ptr<Command> cmd) {
    addImplicitHelpCommand_ = true;
    helpCommand_ = std::move(cmd);
    return *this;
}

inline void Command::outputHelp(const HelpContext& context) {
    auto writeFunc = context.error
        ? outputConfiguration_.writeErr
        : outputConfiguration_.writeOut;

    std::string helpText = helpInformation(context);
    writeFunc(helpText);
}

inline std::string Command::helpInformation(const HelpContext& context) const {
    Help helper = createHelp();
    int helpWidth = context.error
        ? outputConfiguration_.getErrHelpWidth()
        : outputConfiguration_.getOutHelpWidth();
    helper.prepareContext(helpWidth);
    return helper.formatHelp(*this, helper);
}

inline void Command::help(const HelpContext& context) {
    outputHelp(context);
    int exitCode = context.error ? 1 : 0;
    exit_(exitCode, "commander.help", "(outputHelp)");
}

inline Command& Command::addHelpText(const std::string& position, const std::string& text) {
    std::vector<std::string> allowed = {"beforeAll", "before", "after", "afterAll"};
    if (std::find(allowed.begin(), allowed.end(), position) == allowed.end()) {
        throw std::runtime_error(
            "Unexpected value for position to addHelpText.\n"
            "Expecting one of 'beforeAll', 'before', 'after', 'afterAll'");
    }

    std::string helpEvent = position + "Help";
    on(helpEvent, [text](const std::vector<std::any>&) {
        if (!text.empty()) {
            std::cout << text << "\n";
        }
    });
    return *this;
}

inline Command& Command::configureHelp(const std::unordered_map<std::string, std::any>& config) {
    helpConfiguration_ = config;
    return *this;
}

inline std::unordered_map<std::string, std::any> Command::configureHelp() const {
    return helpConfiguration_;
}

inline Command& Command::configureOutput(const OutputConfiguration& config) {
    if (config.writeOut) outputConfiguration_.writeOut = config.writeOut;
    if (config.writeErr) outputConfiguration_.writeErr = config.writeErr;
    if (config.outputError) outputConfiguration_.outputError = config.outputError;
    if (config.getOutHelpWidth) outputConfiguration_.getOutHelpWidth = config.getOutHelpWidth;
    if (config.getErrHelpWidth) outputConfiguration_.getErrHelpWidth = config.getErrHelpWidth;
    return *this;
}

inline OutputConfiguration Command::configureOutput() const {
    return outputConfiguration_;
}

inline Command& Command::showHelpAfterError(bool displayHelp) {
    showHelpAfterError_ = displayHelp;
    return *this;
}

inline Command& Command::showHelpAfterError(const std::string& message) {
    showHelpAfterError_ = message;
    return *this;
}

inline Command& Command::showSuggestionAfterError(bool displaySuggestion) {
    showSuggestionAfterError_ = displaySuggestion;
    return *this;
}

// ---- Error handling ----

inline void Command::error(const std::string& message, const ErrorOptions& opts) {
    outputConfiguration_.outputError(message + "\n", outputConfiguration_.writeErr);

    if (std::holds_alternative<std::string>(showHelpAfterError_)) {
        outputConfiguration_.writeErr(std::get<std::string>(showHelpAfterError_) + "\n");
    } else if (std::holds_alternative<bool>(showHelpAfterError_) &&
               std::get<bool>(showHelpAfterError_)) {
        outputConfiguration_.writeErr("\n");
        outputHelp({.error = true});
    }

    exit_(opts.exitCode, opts.code, message);
}

inline Command& Command::exitOverride(std::function<void(const CommanderError&)> fn) {
    if (fn) {
        exitCallback_ = std::move(fn);
    } else {
        exitCallback_ = [](const CommanderError& err) {
            throw err;
        };
    }
    return *this;
}

// ---- Hooks ----

inline Command& Command::hook(const std::string& event, HookFn listener) {
    std::vector<std::string> allowed = {"preSubcommand", "preAction", "postAction"};
    if (std::find(allowed.begin(), allowed.end(), event) == allowed.end()) {
        throw std::runtime_error(
            "Unexpected value for event passed to hook : '" + event +
            "'.\nExpecting one of 'preSubcommand', 'preAction', 'postAction'");
    }
    lifeCycleHooks_[event].push_back(std::move(listener));
    return *this;
}

// ---- Settings ----

inline Command& Command::copyInheritedSettings(const Command& sourceCommand) {
    outputConfiguration_ = sourceCommand.outputConfiguration_;
    exitCallback_ = sourceCommand.exitCallback_;
    combineFlagAndOptionalValue_ = sourceCommand.combineFlagAndOptionalValue_;
    allowExcessArguments_ = sourceCommand.allowExcessArguments_;
    enablePositionalOptions_ = sourceCommand.enablePositionalOptions_;
    showHelpAfterError_ = sourceCommand.showHelpAfterError_;
    showSuggestionAfterError_ = sourceCommand.showSuggestionAfterError_;
    helpConfiguration_ = sourceCommand.helpConfiguration_;
    addImplicitHelpCommand_ = sourceCommand.addImplicitHelpCommand_;
    return *this;
}

inline Command& Command::allowUnknownOption(bool allowUnknown) {
    allowUnknownOption_ = allowUnknown;
    return *this;
}

inline Command& Command::allowExcessArguments(bool allowExcess) {
    allowExcessArguments_ = allowExcess;
    return *this;
}

inline Command& Command::enablePositionalOptions(bool positional) {
    enablePositionalOptions_ = positional;
    return *this;
}

inline Command& Command::passThroughOptions(bool passThrough) {
    passThroughOptions_ = passThrough;
    checkForBrokenPassThrough_();
    return *this;
}

inline Command& Command::combineFlagAndOptionalValue(bool combine) {
    combineFlagAndOptionalValue_ = combine;
    return *this;
}

inline Help Command::createHelp() const {
    Help help;
    // Apply configuration
    for (const auto& [key, val] : helpConfiguration_) {
        if (key == "helpWidth" && val.type() == typeid(int)) {
            help.helpWidth = std::any_cast<int>(val);
        } else if (key == "sortSubcommands" && val.type() == typeid(bool)) {
            help.sortSubcommands = std::any_cast<bool>(val);
        } else if (key == "sortOptions" && val.type() == typeid(bool)) {
            help.sortOptions = std::any_cast<bool>(val);
        } else if (key == "showGlobalOptions" && val.type() == typeid(bool)) {
            help.showGlobalOptions = std::any_cast<bool>(val);
        }
    }
    return help;
}

// ---- Error reporting methods ----

inline void Command::missingArgument(const std::string& name) {
    error("error: missing required argument '" + name + "'",
          {.exitCode = 1, .code = "commander.missingArgument"});
}

inline void Command::optionMissingArgument(const Option& option) {
    error("error: option '" + option.flags + "' argument missing",
          {.exitCode = 1, .code = "commander.optionMissingArgument"});
}

inline void Command::missingMandatoryOptionValue(const Option& option) {
    error("error: required option '" + option.flags + "' not specified",
          {.exitCode = 1, .code = "commander.missingMandatoryOptionValue"});
}

inline void Command::unknownOption(const std::string& flag) {
    if (allowUnknownOption_) return;
    std::string suggestion;

    if (flag.size() >= 2 && flag[0] == '-' && flag[1] == '-' && showSuggestionAfterError_) {
        std::vector<std::string> candidateFlags;
        auto* cmd = this;
        do {
            auto helper = cmd->createHelp();
            auto visOpts = helper.visibleOptions(*cmd);
            for (const auto& opt : visOpts) {
                if (opt.long_.has_value()) {
                    candidateFlags.push_back(*opt.long_);
                }
            }
            cmd = cmd->parent;
        } while (cmd && !cmd->enablePositionalOptions_);
        suggestion = suggestSimilar(flag, candidateFlags);
    }

    error("error: unknown option '" + flag + "'" + suggestion,
          {.exitCode = 1, .code = "commander.unknownOption"});
}

inline void Command::unknownCommand() {
    std::string unknownName = args.empty() ? "" : args[0];
    std::string suggestion;

    if (showSuggestionAfterError_) {
        std::vector<std::string> candidateNames;
        auto helper = createHelp();
        for (const auto* sub : helper.visibleCommands(*this)) {
            candidateNames.push_back(sub->name());
            std::string a = sub->alias();
            if (!a.empty()) candidateNames.push_back(a);
        }
        suggestion = suggestSimilar(unknownName, candidateNames);
    }

    error("error: unknown command '" + unknownName + "'" + suggestion,
          {.exitCode = 1, .code = "commander.unknownCommand"});
}

// ---- Private helpers ----

inline void Command::exit_(int exitCode, const std::string& code, const std::string& message) {
    if (exitCallback_) {
        exitCallback_(CommanderError(exitCode, code, message));
        return; // In case the callback returns
    }
    std::exit(exitCode);
}

inline std::vector<std::string> Command::prepareUserArgs_(const std::vector<std::string>& argv,
                                                           const ParseOptions& parseOpts) {
    std::vector<std::string> effectiveArgv = argv;

    // If no argv provided, this would normally use process::argv()
    // For now, empty means empty args
    rawArgs = effectiveArgv;

    std::vector<std::string> userArgs;
    if (parseOpts.from == "node") {
        if (effectiveArgv.size() > 1) {
            scriptPath_ = effectiveArgv[1];
        }
        if (effectiveArgv.size() > 2) {
            userArgs.assign(effectiveArgv.begin() + 2, effectiveArgv.end());
        }
    } else if (parseOpts.from == "user") {
        userArgs = effectiveArgv;
    } else if (parseOpts.from == "electron") {
        if (effectiveArgv.size() > 1) {
            scriptPath_ = effectiveArgv[1];
        }
        if (effectiveArgv.size() > 2) {
            userArgs.assign(effectiveArgv.begin() + 2, effectiveArgv.end());
        }
    } else {
        throw std::runtime_error("unexpected parse option { from: '" + parseOpts.from + "' }");
    }

    // Find default name
    if (name_.empty() && !scriptPath_.empty()) {
        nameFromFilename(scriptPath_);
    }
    if (name_.empty()) {
        name_ = "program";
    }

    return userArgs;
}

inline void Command::parseCommand_(const std::vector<std::string>& initialOperands,
                                    const std::vector<std::string>& unknown) {
    auto parsed = parseOptions(unknown);
    parseOptionsEnv_();
    parseOptionsImplied_();

    std::vector<std::string> operands = initialOperands;
    operands.insert(operands.end(), parsed.operands.begin(), parsed.operands.end());
    auto unknownArgs = parsed.unknown;
    args = operands;
    args.insert(args.end(), unknownArgs.begin(), unknownArgs.end());

    // Check for subcommand dispatch
    if (!operands.empty() && findCommand_(operands[0])) {
        std::vector<std::string> subOperands(operands.begin() + 1, operands.end());
        dispatchSubcommand_(operands[0], subOperands, unknownArgs);
        return;
    }

    // Check for help command dispatch
    if (getHelpCommand_() && !operands.empty() && operands[0] == getHelpCommand_()->name()) {
        std::string subName = operands.size() > 1 ? operands[1] : "";
        dispatchHelpCommand_(subName);
        return;
    }

    // Default command
    if (!defaultCommandName_.empty()) {
        outputHelpIfRequested_(unknownArgs);
        dispatchSubcommand_(defaultCommandName_, operands, unknownArgs);
        return;
    }

    // No subcommands matched, no action, and has subcommands: show help
    if (!commands.empty() && args.empty() && !actionHandler_ && defaultCommandName_.empty()) {
        help({.error = true});
        return;
    }

    outputHelpIfRequested_(parsed.unknown);
    checkForMissingMandatoryOptions_();
    checkForConflictingOptions_();

    auto checkForUnknownOptions = [&]() {
        if (!parsed.unknown.empty()) {
            unknownOption(parsed.unknown[0]);
        }
    };

    if (actionHandler_) {
        checkForUnknownOptions();
        processArguments_();

        callHooks_("preAction");
        actionHandler_(processedArgs);
        callHooks_("postAction");
        return;
    }

    // Check for parent event listener (legacy)
    if (!operands.empty()) {
        if (findCommand_("*")) {
            std::vector<std::string> subOperands(operands.begin(), operands.end());
            dispatchSubcommand_("*", subOperands, unknownArgs);
            return;
        }
        if (!commands.empty()) {
            unknownCommand();
            return;
        }
        checkForUnknownOptions();
        processArguments_();
    } else if (!commands.empty()) {
        checkForUnknownOptions();
        help({.error = true});
    } else {
        checkForUnknownOptions();
        processArguments_();
    }
}

inline void Command::parseOptionsEnv_() {
    for (const auto& option : options) {
        if (option.envVar_.has_value()) {
            const char* envVal = std::getenv(option.envVar_->c_str());
            if (envVal != nullptr) {
                std::string optionKey = option.attributeName();
                auto currentValue = getOptionValue(optionKey);
                auto currentSource = getOptionValueSource(optionKey);

                if (!currentValue.has_value() ||
                    currentSource == "default" || currentSource == "config" || currentSource == "env") {
                    if (option.required || option.optional) {
                        emit("optionEnv:" + option.name(), std::string(envVal));
                    } else {
                        emit("optionEnv:" + option.name());
                    }
                }
            }
        }
    }
}

inline void Command::parseOptionsImplied_() {
    auto hasCustomOptionValue = [this](const std::string& optionKey) -> bool {
        auto val = getOptionValue(optionKey);
        if (!val.has_value()) return false;
        auto src = getOptionValueSource(optionKey);
        return src != "default" && src != "implied";
    };

    for (const auto& option : options) {
        if (option.implied_.has_value() && hasCustomOptionValue(option.attributeName())) {
            // Check value from option (simplified DualOptions logic)
            auto val = getOptionValue(option.attributeName());
            bool valueFromOption = true;

            // Simple dual options check
            if (option.negate) {
                // For negate options, only apply implies if value is false
                if (val.has_value() && val.type() == typeid(bool)) {
                    valueFromOption = !std::any_cast<bool>(val);
                }
            } else {
                // For positive options, apply if value is truthy
                if (val.has_value() && val.type() == typeid(bool)) {
                    valueFromOption = std::any_cast<bool>(val);
                }
            }

            if (valueFromOption) {
                for (const auto& [impliedKey, impliedValue] : *option.implied_) {
                    if (!hasCustomOptionValue(impliedKey)) {
                        setOptionValueWithSource(impliedKey, impliedValue, "implied");
                    }
                }
            }
        }
    }
}

inline void Command::checkNumberOfArguments_() {
    // too few
    for (size_t i = 0; i < registeredArguments.size(); ++i) {
        if (registeredArguments[i].required && i >= args.size()) {
            missingArgument(registeredArguments[i].name());
        }
    }
    // too many
    if (!registeredArguments.empty() && registeredArguments.back().variadic) {
        return;
    }
    if (args.size() > registeredArguments.size()) {
        excessArguments_(args);
    }
}

inline void Command::processArguments_() {
    checkNumberOfArguments_();

    processedArgs.clear();
    for (size_t index = 0; index < registeredArguments.size(); ++index) {
        const auto& declaredArg = registeredArguments[index];
        std::any value = declaredArg.defaultValue_;

        if (declaredArg.variadic) {
            if (index < args.size()) {
                std::vector<std::string> values(args.begin() + index, args.end());
                if (declaredArg.parseArg_) {
                    std::any processed = declaredArg.defaultValue_;
                    for (const auto& v : values) {
                        std::string invalidMsg = "error: command-argument value '" + v +
                                                  "' is invalid for argument '" + declaredArg.name() + "'.";
                        processed = callParseArg_(declaredArg, v, processed, invalidMsg);
                    }
                    value = processed;
                } else {
                    value = std::any(values);
                }
            } else if (!value.has_value()) {
                value = std::any(std::vector<std::string>{});
            }
        } else if (index < args.size()) {
            std::string argValue = args[index];
            if (declaredArg.parseArg_) {
                std::string invalidMsg = "error: command-argument value '" + argValue +
                                          "' is invalid for argument '" + declaredArg.name() + "'.";
                value = callParseArg_(declaredArg, argValue, declaredArg.defaultValue_, invalidMsg);
            } else {
                value = std::any(argValue);
            }
        }
        processedArgs.push_back(value);
    }
}

inline void Command::checkForMissingMandatoryOptions_() {
    for (const auto* cmd : getCommandAndAncestors_()) {
        for (const auto& opt : cmd->options) {
            if (opt.mandatory && !cmd->getOptionValue(opt.attributeName()).has_value()) {
                const_cast<Command*>(cmd)->missingMandatoryOptionValue(opt);
            }
        }
    }
}

inline void Command::checkForConflictingOptions_() {
    for (const auto* cmd : getCommandAndAncestors_()) {
        const_cast<Command*>(cmd)->checkForConflictingLocalOptions_();
    }
}

inline void Command::checkForConflictingLocalOptions_() {
    std::vector<const Option*> definedNonDefault;
    for (const auto& opt : options) {
        std::string key = opt.attributeName();
        auto val = getOptionValue(key);
        if (!val.has_value()) continue;
        auto src = getOptionValueSource(key);
        if (src == "default") continue;
        definedNonDefault.push_back(&opt);
    }

    for (const auto* opt : definedNonDefault) {
        if (opt->conflictsWith_.empty()) continue;
        for (const auto* defined : definedNonDefault) {
            if (std::find(opt->conflictsWith_.begin(), opt->conflictsWith_.end(),
                          defined->attributeName()) != opt->conflictsWith_.end()) {
                conflictingOption_(*opt, *defined);
                return;
            }
        }
    }
}

inline void Command::checkForBrokenPassThrough_() {
    if (parent && passThroughOptions_ && !parent->enablePositionalOptions_) {
        throw std::runtime_error(
            "passThroughOptions cannot be used for '" + name_ +
            "' without turning on enablePositionalOptions for parent command(s)");
    }
}

inline void Command::excessArguments_(const std::vector<std::string>& receivedArgs) {
    if (allowExcessArguments_) return;
    int expected = static_cast<int>(registeredArguments.size());
    std::string s = (expected == 1) ? "" : "s";
    std::string forSubcommand = parent ? " for '" + name() + "'" : "";
    error("error: too many arguments" + forSubcommand +
          ". Expected " + std::to_string(expected) + " argument" + s +
          " but got " + std::to_string(static_cast<int>(receivedArgs.size())) + ".",
          {.exitCode = 1, .code = "commander.excessArguments"});
}

inline void Command::conflictingOption_(const Option& option, const Option& conflictingOption) {
    error("error: option '" + option.flags + "' cannot be used with option '" +
          conflictingOption.flags + "'",
          {.exitCode = 1, .code = "commander.conflictingOption"});
}

inline void Command::outputHelpIfRequested_(const std::vector<std::string>& args) {
    auto* helpOpt = getHelpOption_();
    if (!helpOpt) return;

    for (const auto& arg : args) {
        if (helpOpt->is(arg)) {
            outputHelp();
            exit_(0, "commander.helpDisplayed", "(outputHelp)");
            return;
        }
    }
}

inline void Command::dispatchSubcommand_(const std::string& commandName,
                                          const std::vector<std::string>& operands,
                                          const std::vector<std::string>& unknown) {
    Command* subCommand = findCommand_(commandName);
    if (!subCommand) {
        help({.error = true});
        return;
    }

    subCommand->prepareForParse_();
    callSubCommandHook_(*subCommand, "preSubcommand");

    if (subCommand->executableHandler_) {
        executeSubCommand_(*subCommand, operands, unknown);
    } else {
        subCommand->parseCommand_(operands, unknown);
    }
}

inline void Command::executeSubCommand_(Command& subCommand,
                                         const std::vector<std::string>& operands,
                                         const std::vector<std::string>& unknown) {
    // Check mandatory options and conflicts on the parent before launching
    checkForMissingMandatoryOptions_();
    checkForConflictingOptions_();

    // Determine executable name
    std::string execName = subCommand.executableFile_.value_or(
        name_ + "-" + subCommand.name());

    // Determine search directory
    std::string searchDir;
    if (executableDir_.has_value()) {
        searchDir = *executableDir_;
    }

    // If we have a scriptPath_, resolve relative to its directory
    if (!scriptPath_.empty()) {
        namespace fs = std::filesystem;
        std::string resolvedScript;
        try {
            resolvedScript = fs::canonical(scriptPath_).string();
        } catch (...) {
            resolvedScript = scriptPath_;
        }
        std::string scriptDir = fs::path(resolvedScript).parent_path().string();
        if (!searchDir.empty()) {
            searchDir = (fs::path(scriptDir) / searchDir).string();
        } else {
            searchDir = scriptDir;
        }
    }

    // Look for a local file in the search directory first
    std::string resolvedPath;
    if (!searchDir.empty()) {
        namespace fs = std::filesystem;
        auto candidate = fs::path(searchDir) / execName;
        if (fs::exists(candidate)) {
            resolvedPath = candidate.string();
        }
    }

    // If not found locally, search PATH
    if (resolvedPath.empty()) {
        resolvedPath = findProgram_(execName);
    }

    if (resolvedPath.empty()) {
        std::string msg = "'" + execName + "' does not exist\n"
            " - if '" + subCommand.name() + "' is not meant to be an executable command, remove description parameter from '.executableCommand()' and use '.command()' instead\n"
            " - if the default executable name is not suitable, use the executableFile parameter to supply a custom name or path";
        error(msg, {.exitCode = 1, .code = "commander.executeSubCommandAsync"});
        return;
    }

    // Build args: operands + unknown
    std::vector<std::string> launchArgs;
    launchArgs.insert(launchArgs.end(), operands.begin(), operands.end());
    launchArgs.insert(launchArgs.end(), unknown.begin(), unknown.end());

    // Fork + exec
    pid_t pid = fork();
    if (pid == 0) {
        // Child process
        std::vector<const char*> cargs;
        cargs.push_back(resolvedPath.c_str());
        for (const auto& a : launchArgs) {
            cargs.push_back(a.c_str());
        }
        cargs.push_back(nullptr);
        execvp(resolvedPath.c_str(), const_cast<char* const*>(cargs.data()));
        // If execvp returns, it failed
        perror("execvp");
        _exit(127);
    } else if (pid > 0) {
        // Parent: wait for child
        int status = 0;
        waitpid(pid, &status, 0);
        if (WIFEXITED(status)) {
            int code = WEXITSTATUS(status);
            if (code != 0) {
                exit_(code, "commander.executeSubCommandAsync", "(close)");
            }
        } else if (WIFSIGNALED(status)) {
            // Child killed by signal
            exit_(1, "commander.executeSubCommandAsync", "(close)");
        }
    } else {
        // Fork failed
        error("failed to fork subprocess", {.exitCode = 1, .code = "commander.executeSubCommandAsync"});
    }
}

inline std::string Command::findProgram_(const std::string& name) const {
    namespace fs = std::filesystem;

    // If the name contains a path separator, treat as a path
    if (name.find('/') != std::string::npos) {
        if (access(name.c_str(), X_OK) == 0) {
            return name;
        }
        return "";
    }

    // Search PATH
    const char* pathEnv = std::getenv("PATH");
    if (!pathEnv) return "";

    std::string pathStr(pathEnv);
    std::istringstream stream(pathStr);
    std::string dir;
    while (std::getline(stream, dir, ':')) {
        if (dir.empty()) continue;
        auto candidate = fs::path(dir) / name;
        if (access(candidate.c_str(), X_OK) == 0) {
            return candidate.string();
        }
    }
    return "";
}

inline void Command::dispatchHelpCommand_(const std::string& subcommandName) {
    if (subcommandName.empty()) {
        help();
        return;
    }
    Command* subCommand = findCommand_(subcommandName);
    if (subCommand && !subCommand->executableHandler_) {
        subCommand->help();
        return;
    }
    // Fallback: dispatch with help flag (also used for executable subcommands)
    auto* helpOpt = getHelpOption_();
    std::string helpFlag = helpOpt && helpOpt->long_.has_value() ? *helpOpt->long_ : "--help";
    dispatchSubcommand_(subcommandName, {}, {helpFlag});
}

inline void Command::callHooks_(const std::string& event) {
    auto ancestors = getCommandAndAncestors_();
    std::vector<std::pair<Command*, HookFn>> hooks;

    // Collect hooks from ancestors (root first)
    for (auto it = ancestors.rbegin(); it != ancestors.rend(); ++it) {
        auto hookIt = (*it)->lifeCycleHooks_.find(event);
        if (hookIt != (*it)->lifeCycleHooks_.end()) {
            for (const auto& callback : hookIt->second) {
                hooks.push_back({const_cast<Command*>(*it), callback});
            }
        }
    }

    if (event == "postAction") {
        std::reverse(hooks.begin(), hooks.end());
    }

    for (auto& [hookedCommand, callback] : hooks) {
        callback(*hookedCommand, *this);
    }
}

inline void Command::callSubCommandHook_(Command& subCommand, const std::string& event) {
    auto hookIt = lifeCycleHooks_.find(event);
    if (hookIt != lifeCycleHooks_.end()) {
        for (const auto& hookFn : hookIt->second) {
            hookFn(*this, subCommand);
        }
    }
}

inline std::any Command::callParseArg_(const Option& target, const std::string& value,
                                        const std::any& previous, const std::string& invalidArgMsg) {
    try {
        if (target.parseArg_) {
            return target.parseArg_(value, previous);
        }
        return std::any(value);
    } catch (const InvalidArgumentError& err) {
        std::string message = invalidArgMsg + " " + std::string(err.what()).substr(7);
        error(message, {err.exitCode, err.code});
        throw;
    }
}

inline std::any Command::callParseArg_(const Argument& target, const std::string& value,
                                        const std::any& previous, const std::string& invalidArgMsg) {
    try {
        if (target.parseArg_) {
            return target.parseArg_(value, previous);
        }
        return std::any(value);
    } catch (const InvalidArgumentError& err) {
        std::string message = invalidArgMsg + " " + std::string(err.what()).substr(7);
        error(message, {.exitCode = err.exitCode, .code = err.code});
        throw;
    }
}

inline void Command::registerOption_(Option& option) {
    // Check for conflicting flags
    const Option* matchingOption = nullptr;
    if (option.short_.has_value()) {
        matchingOption = findOption_(*option.short_);
    }
    if (!matchingOption && option.long_.has_value()) {
        matchingOption = findOption_(*option.long_);
    }
    if (matchingOption) {
        std::string matchingFlag = (option.long_.has_value() && findOption_(*option.long_))
            ? *option.long_ : (option.short_.has_value() ? *option.short_ : "");
        throw std::runtime_error(
            "Cannot add option '" + option.flags + "'" +
            (name_.empty() ? "" : " to command '" + name_ + "'") +
            " due to conflicting flag '" + matchingFlag +
            "'\n-  already used by option '" + matchingOption->flags + "'");
    }
    options.push_back(option);
}

inline void Command::registerCommand_(Command& command) {
    // Check for name/alias conflicts
    auto knownBy = [](const Command& cmd) -> std::vector<std::string> {
        std::vector<std::string> names;
        names.push_back(cmd.name());
        auto a = cmd.aliases();
        names.insert(names.end(), a.begin(), a.end());
        return names;
    };

    auto names = knownBy(command);
    for (const auto& n : names) {
        if (findCommand_(n)) {
            auto existingNames = knownBy(*findCommand_(n));
            std::string existingStr;
            for (size_t i = 0; i < existingNames.size(); ++i) {
                if (i > 0) existingStr += "|";
                existingStr += existingNames[i];
            }
            std::string newStr;
            for (size_t i = 0; i < names.size(); ++i) {
                if (i > 0) newStr += "|";
                newStr += names[i];
            }
            throw std::runtime_error(
                "cannot add command '" + newStr + "' as already have command '" + existingStr + "'");
        }
    }
}

inline Command* Command::findCommand_(const std::string& name) {
    if (name.empty()) return nullptr;
    for (auto& cmd : commands) {
        if (cmd->name_ == name) return cmd.get();
        for (const auto& a : cmd->aliases_) {
            if (a == name) return cmd.get();
        }
    }
    return nullptr;
}

inline const Option* Command::findOption_(const std::string& arg) const {
    for (const auto& opt : options) {
        if (opt.is(arg)) return &opt;
    }
    return nullptr;
}

inline const Option* Command::getHelpOption_() {
    if (!helpOption_.has_value()) {
        // Lazy create
        helpOption_ = std::make_unique<Option>("-h, --help", "display help for command");
    }
    return helpOption_->get();
}

inline Command* Command::getHelpCommand_() {
    bool hasImplicit = false;
    if (addImplicitHelpCommand_.has_value()) {
        hasImplicit = *addImplicitHelpCommand_;
    } else {
        hasImplicit = !commands.empty() && !actionHandler_ && !findCommand_("help");
    }

    if (hasImplicit) {
        if (!helpCommand_.has_value()) {
            auto helpCmd = createCommand("help");
            helpCmd->helpOption(false);
            helpCmd->arguments("[command]");
            helpCmd->description("display help for command");
            helpCommand_ = std::move(helpCmd);
        }
        return helpCommand_->get();
    }
    return nullptr;
}

inline std::vector<const Command*> Command::getCommandAndAncestors_() const {
    std::vector<const Command*> result;
    for (const Command* cmd = this; cmd != nullptr; cmd = cmd->parent) {
        result.push_back(cmd);
    }
    return result;
}

inline void Command::prepareForParse_() {
    if (!savedState_.has_value()) {
        saveStateBeforeParse_();
    } else {
        restoreStateBeforeParse_();
    }
}

inline void Command::saveStateBeforeParse_() {
    savedState_ = SavedState{
        .name = name_,
        .optionValues = optionValues_,
        .optionValueSources = optionValueSources_
    };
}

inline void Command::restoreStateBeforeParse_() {
    if (!savedState_.has_value()) return;
    name_ = savedState_->name;
    scriptPath_.clear();
    rawArgs.clear();
    optionValues_ = savedState_->optionValues;
    optionValueSources_ = savedState_->optionValueSources;
    args.clear();
    processedArgs.clear();
}

} // namespace commander
} // namespace polycpp
