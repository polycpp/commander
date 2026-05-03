#pragma once

/**
 * @file detail/command.hpp
 * @brief Inline implementations for the Command handle.
 *
 * Bodies operate on `impl_->...` (the per-command shared state defined in
 * `detail/command_impl.hpp`). The aggregator includes that file before this
 * one so all members are fully visible.
 *
 * @since 0.1.0
 */

#include <polycpp/commander/command.hpp>
#include <polycpp/commander/suggest_similar.hpp>
#include <polycpp/platform/console.hpp>
#include <polycpp/process.hpp>

#include <algorithm>
#include <array>
#include <atomic>
#include <cctype>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <regex>
#include <sstream>
#include <stdexcept>
#include <utility>

#include <polycpp/child_process.hpp>
#include <polycpp/event_loop.hpp>
#include <polycpp/fs.hpp>
#include <polycpp/path.hpp>
#include <polycpp/process.hpp>
#include <polycpp/timers.hpp>

namespace polycpp {
namespace commander {

namespace detail_exec {

inline bool isPathSeparator(char c) noexcept {
#if defined(_WIN32)
    return c == '/' || c == '\\';
#else
    return c == '/';
#endif
}

inline bool containsPathSeparator(const std::string& s) noexcept {
    return std::any_of(s.begin(), s.end(), isPathSeparator);
}

inline std::string dirname(const std::string& path) {
#if defined(_WIN32)
    return polycpp::path::win32::dirname(path);
#else
    return polycpp::path::dirname(path);
#endif
}

inline std::string extname(const std::string& path) {
#if defined(_WIN32)
    return polycpp::path::win32::extname(path);
#else
    return polycpp::path::extname(path);
#endif
}

inline bool isAbsolute(const std::string& path) noexcept {
#if defined(_WIN32)
    return polycpp::path::win32::isAbsolute(path);
#else
    return polycpp::path::isAbsolute(path);
#endif
}

inline std::string join(const std::string& a, const std::string& b) {
#if defined(_WIN32)
    return polycpp::path::win32::join(a, b);
#else
    return polycpp::path::join(a, b);
#endif
}

inline char pathDelimiter() noexcept {
#if defined(_WIN32)
    return ';';
#else
    return ':';
#endif
}

inline std::string toLowerAscii(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return s;
}

inline bool isBatchFile(const std::string& path) {
#if defined(_WIN32)
    const std::string ext = toLowerAscii(extname(path));
    return ext == ".cmd" || ext == ".bat";
#else
    (void)path;
    return false;
#endif
}

inline std::vector<std::string> windowsExecutableExtensions() {
    std::vector<std::string> result;
#if defined(_WIN32)
    std::string pathext = polycpp::process::getenv("PATHEXT");
    if (pathext.empty()) pathext = ".COM;.EXE;.BAT;.CMD";

    auto add = [&](std::string ext) {
        if (ext.empty()) return;
        if (ext.front() != '.') ext.insert(ext.begin(), '.');
        ext = toLowerAscii(std::move(ext));
        if (ext != ".com" && ext != ".exe" && ext != ".bat" && ext != ".cmd") return;
        if (std::find(result.begin(), result.end(), ext) == result.end()) {
            result.push_back(std::move(ext));
        }
    };

    std::istringstream stream(pathext);
    std::string ext;
    while (std::getline(stream, ext, ';')) add(std::move(ext));
    for (const auto* fallback : {".com", ".exe", ".bat", ".cmd"}) add(fallback);
#endif
    return result;
}

inline std::vector<std::string> executableCandidates(const std::string& base) {
#if defined(_WIN32)
    if (!extname(base).empty()) return {base};

    std::vector<std::string> candidates;
    for (const auto& ext : windowsExecutableExtensions()) {
        candidates.push_back(base + ext);
    }
    return candidates;
#else
    return {base};
#endif
}

inline bool canRunFile(const std::string& path) {
    try {
        auto stats = polycpp::fs::statSync(path);
        if (!stats.isFile()) return false;
#if defined(_WIN32)
        polycpp::fs::accessSync(path, polycpp::fs::constants::kF_OK);
#else
        polycpp::fs::accessSync(path, polycpp::fs::constants::kX_OK);
#endif
        return true;
    } catch (...) {
        return false;
    }
}

inline std::string findExecutableInDirectory(const std::string& dir,
                                             const std::string& name) {
    for (const auto& candidateName : executableCandidates(name)) {
        const std::string candidate = join(dir, candidateName);
        if (canRunFile(candidate)) return candidate;
    }
    return "";
}

inline std::string findExecutablePath(const std::string& path) {
    for (const auto& candidate : executableCandidates(path)) {
        if (canRunFile(candidate)) return candidate;
    }
    return "";
}

inline std::string windowsCommandInterpreter() {
#if defined(_WIN32)
    std::string comspec = polycpp::process::getenv("COMSPEC");
    return comspec.empty() ? "cmd.exe" : comspec;
#else
    return "";
#endif
}

/**
 * @brief RAII helper that forwards parent-received signals to a spawned child.
 *
 * Mirrors the upstream commander.js behaviour where the parent process
 * installs handlers for SIGUSR1, SIGUSR2, SIGTERM, SIGINT, and SIGHUP and
 * forwards each one to the spawned stand-alone executable subcommand via
 * `child.kill(signal)`.
 *
 * Implementation notes:
 * - polycpp's `process::on(SIG, ...)` (since plan 1273, commit `bbf2f0c1`)
 *   automatically wires the signal-pipe read end into the EventContext so
 *   queued signals dispatch from the loop tick.
 * - polycpp's `process::on(SIG, ...)` returns a `SignalListenerId` (since
 *   plan 1274, commit `8561254f`), so the forwarder can call
 *   `process::off(name, id)` to remove only its own listeners on
 *   destruction. Any user-installed handlers on those signals survive the
 *   spawn unchanged.
 * - The destructor must remove the listeners (not just deactivate via the
 *   `active_` flag): polycpp's auto-drain integration keeps the
 *   signal-pipe descriptor in `async_wait` while any listener is
 *   registered, which would otherwise prevent `EventLoop::run()` from
 *   returning naturally after parse() finishes — both for the sync path
 *   and the parseAsync nested-run case.
 * - The `active_` shared atomic is retained as a defensive no-op so that
 *   handler entries already in flight (sigaction was raised but the drain
 *   has not yet run) skip the kill once `deactivate()` is called.
 * - The lambda captures the `ChildProcess` handle (a thin shared-ptr handle,
 *   cheap to copy) and the `active_` flag by value so the listener remains
 *   safe even before removal completes.
 *
 * @since 0.1.0
 */
struct SignalForwarder {
    /**
     * @brief Install the forwarders. Must be called from the thread that owns
     *        the polycpp event loop (signal handlers run on the loop).
     */
    explicit SignalForwarder(polycpp::child_process::ChildProcess child)
        : child_(std::move(child)),
          active_(std::make_shared<std::atomic<bool>>(true)) {
        // Names match upstream commander.js exactly. Order is not significant
        // beyond the kForwarded() / installed_ index correspondence below.
        const auto& names = kForwarded();
        for (std::size_t i = 0; i < names.size(); ++i) {
            auto active = active_;
            auto child  = child_;
            try {
                installed_[i] = polycpp::process::on(
                    names[i], [active, child](int signum) mutable {
                        if (!active->load()) return; // no-op after deactivate()
                        // Best-effort forward; ignore errors (child may already be gone).
                        try { child.kill(signum); } catch (...) {}
                    });
                installedActive_[i] = true;
            } catch (...) {
                // Some platforms expose signal constants for API parity but
                // cannot install handlers for all POSIX signals (notably Win32).
                installed_[i] = 0;
            }
        }
    }

    ~SignalForwarder() {
        deactivate();
        // Remove only our own listeners; user-installed handlers on the same
        // signals are unaffected. Required so polycpp's signal-pipe descriptor
        // stops pinning the EventContext alive once the spawn is over.
        const auto& names = kForwarded();
        for (std::size_t i = 0; i < names.size(); ++i) {
            if (installedActive_[i]) {
                polycpp::process::off(names[i], installed_[i]);
            }
        }
    }

    SignalForwarder(const SignalForwarder&) = delete;
    SignalForwarder& operator=(const SignalForwarder&) = delete;
    SignalForwarder(SignalForwarder&&) = delete;
    SignalForwarder& operator=(SignalForwarder&&) = delete;

    /**
     * @brief Stop forwarding immediately. Idempotent. Safe from any thread.
     *        The underlying listeners are removed by the destructor.
     */
    void deactivate() noexcept {
        if (active_) active_->store(false);
    }

private:
    /// Signal names commander forwards. Single source of truth for ctor + dtor.
    static const std::array<const char*, 5>& kForwarded() {
        static const std::array<const char*, 5> kNames = {{
            "SIGUSR1", "SIGUSR2", "SIGTERM", "SIGINT", "SIGHUP",
        }};
        return kNames;
    }

    polycpp::child_process::ChildProcess child_;
    std::shared_ptr<std::atomic<bool>> active_;
    /// Listener tokens returned by `polycpp::process::on(...)`. Indexed by
    /// `kForwarded()` position so the destructor can remove them.
    std::array<polycpp::process::SignalListenerId, 5> installed_{};
    std::array<bool, 5> installedActive_{};
};

} // namespace detail_exec

// ---- Constructor / destructor ----

inline Command::Command(const std::string& name)
    : impl_(std::make_shared<Impl>()) {
    impl_->name_ = name;
    impl_->optionValues_ = polycpp::JsonValue(polycpp::JsonObject{});
    setEmitter_(impl_->ee);

    // Set up default output configuration
    impl_->outputConfiguration_.writeOut = [](const std::string& str) {
        std::cout << str;
    };
    impl_->outputConfiguration_.writeErr = [](const std::string& str) {
        std::cerr << str;
    };
    impl_->outputConfiguration_.outputError = [](const std::string& str,
                                           std::function<void(const std::string&)> write) {
        write(str);
    };
    impl_->outputConfiguration_.getOutHelpWidth = []() -> int { return 80; };
    impl_->outputConfiguration_.getErrHelpWidth = []() -> int { return 80; };
    impl_->outputConfiguration_.getOutHasColors = []() -> bool {
        if (!polycpp::platform::isTerminal(1)) return false;
        auto env = polycpp::process::env();
        if (env.count("NO_COLOR")) return false;
        auto it = env.find("FORCE_COLOR");
        if (it != env.end()) {
            return it->second != "0";
        }
        return true;
    };
    impl_->outputConfiguration_.getErrHasColors = []() -> bool {
        if (!polycpp::platform::isTerminal(2)) return false;
        auto env = polycpp::process::env();
        if (env.count("NO_COLOR")) return false;
        auto it = env.find("FORCE_COLOR");
        if (it != env.end()) {
            return it->second != "0";
        }
        return true;
    };
}

inline Command::~Command() = default;

// ---- Metadata getters/setters ----

inline std::string Command::name() const { return impl_->name_; }
inline Command& Command::name(const std::string& str) { impl_->name_ = str; return *this; }

inline std::string Command::description() const { return impl_->description_; }
inline Command& Command::description(const std::string& str) { impl_->description_ = str; return *this; }

inline std::string Command::summary() const { return impl_->summary_; }
inline Command& Command::summary(const std::string& str) { impl_->summary_ = str; return *this; }

inline std::string Command::usage() const {
    if (!impl_->usage_.empty()) return impl_->usage_;

    std::vector<std::string> parts;
    if (!impl_->options_.empty() || !impl_->helpOption_.has_value() ||
        impl_->helpOption_ != std::nullopt) {
        // Has options or help option not disabled
        bool hasHelpOption = true;
        if (impl_->helpOption_.has_value() && !*impl_->helpOption_) {
            hasHelpOption = false;
        }
        if (!impl_->options_.empty() || hasHelpOption) {
            parts.push_back("[options]");
        }
    }
    if (!impl_->commands_.empty()) {
        parts.push_back("[command]");
    }
    for (const auto& arg : impl_->registeredArguments_) {
        parts.push_back(humanReadableArgName(arg));
    }

    std::string result;
    for (size_t i = 0; i < parts.size(); ++i) {
        if (i > 0) result += ' ';
        result += parts[i];
    }
    return result;
}

inline Command& Command::usage(const std::string& str) { impl_->usage_ = str; return *this; }

inline std::string Command::version() const { return impl_->version_; }

inline Command& Command::version(const std::string& str,
                                  const std::string& flags,
                                  const std::string& desc) {
    impl_->version_ = str;
    Option versionOption = createOption(flags, desc);
    impl_->versionOptionName_ = versionOption.attributeName();
    registerOption_(versionOption);

    std::string versionStr = str;
    onInternalEvent_("option:" + versionOption.name(), [this, versionStr](const std::optional<std::string>&) {
        impl_->outputConfiguration_.writeOut(versionStr + "\n");
        exit_(0, "commander.version", versionStr);
    });
    return *this;
}

// ---- Aliases ----

inline std::string Command::alias() const {
    if (impl_->aliases_.empty()) return "";
    return impl_->aliases_[0];
}

inline Command& Command::alias(const std::string& aliasName) {
    if (aliasName == impl_->name_) {
        throw std::runtime_error("Command alias can't be the same as its name");
    }
    impl_->aliases_.push_back(aliasName);
    return *this;
}

inline std::vector<std::string> Command::aliases() const { return impl_->aliases_; }

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
    impl_->name_ = name;
    return *this;
}

// ---- Options ----

inline Command& Command::option(const std::string& flags, const std::string& description) {
    Option opt = createOption(flags, description);
    return addOption(std::move(opt));
}

inline Command& Command::option(const std::string& flags, const std::string& description,
                                 const polycpp::JsonValue& defaultValue) {
    Option opt = createOption(flags, description);
    opt.defaultValue(defaultValue);
    return addOption(std::move(opt));
}

inline Command& Command::option(const std::string& flags, const std::string& description,
                                 ParseFn parseArg, const polycpp::JsonValue& defaultValue) {
    Option opt = createOption(flags, description);
    if (!defaultValue.isNull()) {
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
                                         const polycpp::JsonValue& defaultValue) {
    Option opt = createOption(flags, description);
    opt.makeOptionMandatory(true);
    opt.defaultValue(defaultValue);
    return addOption(std::move(opt));
}

inline Command& Command::requiredOption(const std::string& flags, const std::string& description,
                                         ParseFn parseArg, const polycpp::JsonValue& defaultValue) {
    Option opt = createOption(flags, description);
    opt.makeOptionMandatory(true);
    if (!defaultValue.isNull()) {
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
            if (!option.defaultValue_.isNull()) {
                setOptionValueWithSource(attrName, option.defaultValue_, "default");
            } else {
                setOptionValueWithSource(attrName, polycpp::JsonValue(true), "default");
            }
        }
    } else if (!option.defaultValue_.isNull()) {
        setOptionValueWithSource(attrName, option.defaultValue_, "default");
    }

    // Capture option properties for the lambda closures
    bool optNegate = option.negate;
    bool optVariadic = option.variadic;
    bool optIsBoolean = option.isBoolean();
    bool optOptional = option.optional;
    bool hasParseArg = static_cast<bool>(option.parseArg_);
    bool hasPresetArg = !option.presetArg_.isNull();
    polycpp::JsonValue presetArg = option.presetArg_;
    std::string optFlags = option.flags;

    // Handler for cli and env supplied values
    auto handleOptionValue = [this, attrName, optNegate, optIsBoolean, optOptional,
                               optVariadic, hasParseArg, hasPresetArg, presetArg,
                               optFlags](polycpp::JsonValue val, const std::string& invalidValueMessage,
                                         const std::string& valueSource) {
        // val is null for boolean/negate, or null for optional without arg
        if (val.isNull() && hasPresetArg) {
            val = presetArg;
        }

        // custom processing
        polycpp::JsonValue oldValue = getOptionValue(attrName);

        if (!val.isNull() && hasParseArg) {
            // Find the option to get parseArg_
            for (auto& opt : impl_->options_) {
                if (opt.attributeName() == attrName && opt.parseArg_) {
                    try {
                        std::string valStr;
                        if (val.isString()) {
                            valStr = val.asString();
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
        } else if (!val.isNull() && optVariadic) {
            // Collect into array
            std::string valStr;
            if (val.isString()) {
                valStr = val.asString();
            }
            if (!oldValue.isNull() && oldValue.isArray()) {
                polycpp::JsonValue result = oldValue;
                result.asArray().push_back(polycpp::JsonValue(valStr));
                val = result;
            } else {
                val = polycpp::JsonValue(polycpp::JsonArray{polycpp::JsonValue(valStr)});
            }
        }

        // Fill in appropriate missing values
        if (val.isNull()) {
            if (optNegate) {
                val = polycpp::JsonValue(false);
            } else if (optIsBoolean || optOptional) {
                val = polycpp::JsonValue(true);
            } else {
                val = polycpp::JsonValue("");
            }
        }

        setOptionValueWithSource(attrName, val, valueSource);
    };

    onInternalEvent_("option:" + oname, [handleOptionValue, optFlags](const std::optional<std::string>& eventArg) {
        polycpp::JsonValue val;  // null
        if (eventArg.has_value()) {
            val = polycpp::JsonValue(*eventArg);
        }
        std::string valStr;
        if (val.isString()) {
            valStr = val.asString();
        }
        std::string invalidValueMessage = "error: option '" + optFlags + "' argument '" + valStr + "' is invalid.";
        handleOptionValue(val, invalidValueMessage, "cli");
    });

    if (option.envVar_.has_value()) {
        onInternalEvent_("optionEnv:" + oname,
                         [handleOptionValue, optFlags, option](const std::optional<std::string>& eventArg) {
            polycpp::JsonValue val;  // null
            if (eventArg.has_value()) {
                val = polycpp::JsonValue(*eventArg);
            }
            std::string valStr;
            if (val.isString()) {
                valStr = val.asString();
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
    if (!impl_->registeredArguments_.empty() && impl_->registeredArguments_.back().variadic) {
        throw std::runtime_error(
            "only the last argument can be variadic '" +
            impl_->registeredArguments_.back().name() + "'");
    }
    // Check: required arg with default and no parser doesn't make sense
    if (argument.required && !argument.defaultValue_.isNull() && !argument.parseArg_) {
        throw std::runtime_error(
            "a default value for a required argument is never used: '" + argument.name() + "'");
    }
    impl_->registeredArguments_.push_back(std::move(argument));
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

    Command cmd = createCommand(cmdName);

    if (opts.isDefault) impl_->defaultCommandName_ = cmd.impl_->name_;
    cmd.impl_->hidden_ = opts.hidden;
    if (!argsDef.empty()) cmd.arguments(argsDef);

    registerCommand_(cmd);
    // weak_ptr to the parent's Impl: see detail/command_impl.hpp.
    cmd.impl_->parent_ = this->impl_;
    cmd.copyInheritedSettings(*this);

    impl_->commands_.push_back(std::move(cmd));
    return impl_->commands_.back();
}

inline Command& Command::addCommand(Command cmd, const CommandOptions& opts) {
    if (cmd.impl_->name_.empty()) {
        throw std::runtime_error(
            "Command passed to .addCommand() must have a name\n"
            "- specify the name in Command constructor or using .name()");
    }

    if (opts.isDefault) impl_->defaultCommandName_ = cmd.impl_->name_;
    if (opts.hidden) cmd.impl_->hidden_ = true;

    registerCommand_(cmd);
    // weak_ptr to the parent's Impl: see detail/command_impl.hpp.
    cmd.impl_->parent_ = this->impl_;

    impl_->commands_.push_back(std::move(cmd));
    return *this;
}

inline Command Command::createCommand(const std::string& name) const {
    return Command(name);
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

    Command cmd = createCommand(cmdName);
    cmd.description(description);
    cmd.impl_->executableHandler_ = true;
    if (!executableFile.empty()) {
        cmd.impl_->executableFile_ = executableFile;
    }
    if (!argsDef.empty()) cmd.arguments(argsDef);

    registerCommand_(cmd);
    // weak_ptr to the parent's Impl: see detail/command_impl.hpp.
    cmd.impl_->parent_ = this->impl_;
    cmd.copyInheritedSettings(*this);

    impl_->commands_.push_back(std::move(cmd));
    return *this; // Return parent, not subcommand
}

inline Command& Command::executableDir(const std::string& path) {
    impl_->executableDir_ = path;
    return *this;
}

inline std::string Command::executableDir() const {
    return impl_->executableDir_.value_or("");
}

// ---- Action ----

inline Command& Command::action(ActionFn fn) {
    impl_->actionHandler_ = [this, fn = std::move(fn)](const std::vector<polycpp::JsonValue>& processedArgs) {
        auto optsMap = opts();
        fn(processedArgs, optsMap, *this);
    };
    return *this;
}

// ---- Parsing ----

inline Command& Command::parse() {
    return parse(polycpp::process::argv(), {.from = "native"});
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
        // Walk the impl chain (root included). parent_ is a weak_ptr — lock
        // at each step; bail out if an ancestor's Impl has already died.
        std::shared_ptr<Command::Impl> current = impl_;
        while (current) {
            for (const auto& opt : current->options_) {
                if (opt.short_.has_value()) {
                    std::regex digitShortRe("^-\\d$");
                    if (std::regex_match(*opt.short_, digitShortRe)) {
                        return false;
                    }
                }
            }
            current = current->parent_.lock();
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
            emitInternalEvent_("option:" + activeVariadicOption->name(), std::string(arg));
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
                    emitInternalEvent_("option:" + option->name(), std::string(value));
                } else if (option->optional) {
                    // Historical behaviour: optional value is following arg unless it's an option
                    if (i < inputArgs.size() && (!maybeOption(inputArgs[i]) || negativeNumberArg(inputArgs[i]))) {
                        emitInternalEvent_("option:" + option->name(), std::string(inputArgs[i++]));
                    } else {
                        emitInternalEvent_("option:" + option->name());
                    }
                } else {
                    // boolean flag
                    emitInternalEvent_("option:" + option->name());
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
                    (option->optional && impl_->combineFlagAndOptionalValue_)) {
                    // Value following in same argument
                    emitInternalEvent_("option:" + option->name(), std::string(arg.substr(2)));
                } else {
                    // Boolean; remove processed option and keep processing group
                    emitInternalEvent_("option:" + option->name());
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
                    emitInternalEvent_("option:" + option->name(), std::string(arg.substr(eqPos + 1)));
                    continue;
                }
            }
        }

        // Not a recognised option
        if (!destIsUnknown && maybeOption(arg) && !(impl_->commands_.empty() && negativeNumberArg(arg))) {
            destIsUnknown = true;
        }

        // Positional options: stop at subcommand
        if ((impl_->enablePositionalOptions_ || impl_->passThroughOptions_) &&
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
            } else if (!impl_->defaultCommandName_.empty()) {
                while (i < inputArgs.size()) {
                    unknown.push_back(inputArgs[i++]);
                }
                unknown.insert(unknown.begin(), arg);
                break;
            }
        }

        // Pass through options: stop at first command-argument
        if (impl_->passThroughOptions_) {
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
    return impl_->optionValues_;
}

inline OptionValues Command::optsWithGlobals() const {
    polycpp::JsonValue result(polycpp::JsonObject{});
    // Collect impls along the chain (root-most first) so values from the
    // closer (more specific) command win. parent_ is a weak_ptr; lock at
    // each step and stop when an ancestor's Impl has already died.
    std::vector<std::shared_ptr<Command::Impl>> chain;
    for (auto current = impl_; current; current = current->parent_.lock()) {
        chain.push_back(current);
    }
    for (auto it = chain.rbegin(); it != chain.rend(); ++it) {
        const auto& cmdOpts = (*it)->optionValues_;
        if (cmdOpts.isObject()) {
            for (const auto& [key, val] : cmdOpts.asObject()) {
                result[key] = val;
            }
        }
    }
    return result;
}

inline polycpp::JsonValue Command::getOptionValue(const std::string& key) const {
    if (impl_->optionValues_.isObject() && impl_->optionValues_.hasKey(key)) {
        return impl_->optionValues_.asObject().at(key);
    }
    return polycpp::JsonValue();  // null
}

inline Command& Command::setOptionValue(const std::string& key, const polycpp::JsonValue& value) {
    return setOptionValueWithSource(key, value, "");
}

inline Command& Command::setOptionValueWithSource(const std::string& key, const polycpp::JsonValue& value,
                                                    const std::string& source) {
    if (!impl_->optionValues_.isObject()) {
        impl_->optionValues_ = polycpp::JsonValue(polycpp::JsonObject{});
    }
    impl_->optionValues_[key] = value;
    impl_->optionValueSources_[key] = source;
    return *this;
}

inline std::string Command::getOptionValueSource(const std::string& key) const {
    auto it = impl_->optionValueSources_.find(key);
    if (it != impl_->optionValueSources_.end()) return it->second;
    return "";
}

inline std::string Command::getOptionValueSourceWithGlobals(const std::string& key) const {
    std::string source;
    // Walk the impl chain so the root command's source is included.
    // parent_ is a weak_ptr; lock at each step.
    for (auto current = impl_; current; current = current->parent_.lock()) {
        auto it = current->optionValueSources_.find(key);
        if (it != current->optionValueSources_.end() && !it->second.empty()) {
            source = it->second;
        }
    }
    return source;
}

// ---- Help configuration ----

inline Command& Command::helpOption(const std::string& flags, const std::string& description) {
    std::string f = flags.empty() ? "-h, --help" : flags;
    std::string d = description.empty() ? "display help for command" : description;
    impl_->helpOption_ = std::make_unique<Option>(f, d);
    return *this;
}

inline Command& Command::helpOption(bool enable) {
    if (!enable) {
        impl_->helpOption_ = std::unique_ptr<Option>(nullptr); // disabled
    } else {
        impl_->helpOption_.reset(); // re-enable lazy creation
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

    auto helpCmd = std::make_unique<Command>(helpName);
    helpCmd->helpOption(false);
    if (!helpArgs.empty()) helpCmd->arguments(helpArgs);
    if (!effectiveDescription.empty()) helpCmd->description(effectiveDescription);

    impl_->addImplicitHelpCommand_ = true;
    impl_->helpCommand_ = std::move(helpCmd);
    return *this;
}

inline Command& Command::helpCommand(bool enable) {
    impl_->addImplicitHelpCommand_ = enable;
    return *this;
}

inline Command& Command::addHelpCommand(Command cmd) {
    impl_->addImplicitHelpCommand_ = true;
    impl_->helpCommand_ = std::make_unique<Command>(std::move(cmd));
    return *this;
}

inline void Command::outputHelp(const HelpContext& context) {
    auto writeFunc = context.error
        ? impl_->outputConfiguration_.writeErr
        : impl_->outputConfiguration_.writeOut;

    std::string helpText = helpInformation(context);
    writeFunc(helpText);
}

inline std::string Command::helpInformation(const HelpContext& context) const {
    Help helper = createHelp();
    int hw = context.error
        ? impl_->outputConfiguration_.getErrHelpWidth()
        : impl_->outputConfiguration_.getOutHelpWidth();
    bool hasColors = false;
    if (context.error && impl_->outputConfiguration_.getErrHasColors) {
        hasColors = impl_->outputConfiguration_.getErrHasColors();
    } else if (!context.error && impl_->outputConfiguration_.getOutHasColors) {
        hasColors = impl_->outputConfiguration_.getOutHasColors();
    }
    helper.prepareContext({.helpWidth = hw, .outputHasColors = hasColors});
    std::string text = helper.formatHelp(*this, helper);
    text = renderHelpText_("beforeAll") + renderHelpText_("before") + text +
           renderHelpText_("after") + renderHelpText_("afterAll");
    // Strip color if output does not support it (safety net).
    if (!hasColors) {
        text = stripColor(text);
    }
    return text;
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

    impl_->helpText_[position].push_back(text);
    return *this;
}

inline Command& Command::configureHelp(const std::map<std::string, polycpp::JsonValue>& config) {
    impl_->helpConfiguration_ = config;
    return *this;
}

inline std::map<std::string, polycpp::JsonValue> Command::configureHelp() const {
    return impl_->helpConfiguration_;
}

inline Command& Command::configureOutput(const OutputConfiguration& config) {
    if (config.writeOut) impl_->outputConfiguration_.writeOut = config.writeOut;
    if (config.writeErr) impl_->outputConfiguration_.writeErr = config.writeErr;
    if (config.outputError) impl_->outputConfiguration_.outputError = config.outputError;
    if (config.getOutHelpWidth) impl_->outputConfiguration_.getOutHelpWidth = config.getOutHelpWidth;
    if (config.getErrHelpWidth) impl_->outputConfiguration_.getErrHelpWidth = config.getErrHelpWidth;
    if (config.getOutHasColors) impl_->outputConfiguration_.getOutHasColors = config.getOutHasColors;
    if (config.getErrHasColors) impl_->outputConfiguration_.getErrHasColors = config.getErrHasColors;
    return *this;
}

inline OutputConfiguration Command::configureOutput() const {
    return impl_->outputConfiguration_;
}

inline Command& Command::showHelpAfterError(bool displayHelp) {
    impl_->showHelpAfterError_ = displayHelp;
    return *this;
}

inline Command& Command::showHelpAfterError(const std::string& message) {
    impl_->showHelpAfterError_ = message;
    return *this;
}

inline Command& Command::showSuggestionAfterError(bool displaySuggestion) {
    impl_->showSuggestionAfterError_ = displaySuggestion;
    return *this;
}

// ---- Error handling ----

inline void Command::error(const std::string& message, const ErrorOptions& opts) {
    impl_->outputConfiguration_.outputError(message + "\n", impl_->outputConfiguration_.writeErr);

    if (std::holds_alternative<std::string>(impl_->showHelpAfterError_)) {
        impl_->outputConfiguration_.writeErr(std::get<std::string>(impl_->showHelpAfterError_) + "\n");
    } else if (std::holds_alternative<bool>(impl_->showHelpAfterError_) &&
               std::get<bool>(impl_->showHelpAfterError_)) {
        impl_->outputConfiguration_.writeErr("\n");
        outputHelp({.error = true});
    }

    exit_(opts.exitCode, opts.code, message);
}

inline Command& Command::exitOverride(std::function<void(const CommanderError&)> fn) {
    if (fn) {
        impl_->exitCallback_ = std::move(fn);
    } else {
        impl_->exitCallback_ = [](const CommanderError& err) {
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
    impl_->lifeCycleHooks_[event].push_back(std::move(listener));
    return *this;
}

// ---- Settings ----

inline Command& Command::copyInheritedSettings(const Command& sourceCommand) {
    impl_->outputConfiguration_ = sourceCommand.impl_->outputConfiguration_;
    impl_->exitCallback_ = sourceCommand.impl_->exitCallback_;
    impl_->combineFlagAndOptionalValue_ = sourceCommand.impl_->combineFlagAndOptionalValue_;
    impl_->allowExcessArguments_ = sourceCommand.impl_->allowExcessArguments_;
    impl_->enablePositionalOptions_ = sourceCommand.impl_->enablePositionalOptions_;
    impl_->showHelpAfterError_ = sourceCommand.impl_->showHelpAfterError_;
    impl_->showSuggestionAfterError_ = sourceCommand.impl_->showSuggestionAfterError_;
    impl_->helpConfiguration_ = sourceCommand.impl_->helpConfiguration_;
    impl_->addImplicitHelpCommand_ = sourceCommand.impl_->addImplicitHelpCommand_;
    return *this;
}

inline Command& Command::allowUnknownOption(bool allowUnknown) {
    impl_->allowUnknownOption_ = allowUnknown;
    return *this;
}

inline Command& Command::allowExcessArguments(bool allowExcess) {
    impl_->allowExcessArguments_ = allowExcess;
    return *this;
}

inline Command& Command::enablePositionalOptions(bool positional) {
    impl_->enablePositionalOptions_ = positional;
    return *this;
}

inline Command& Command::passThroughOptions(bool passThrough) {
    impl_->passThroughOptions_ = passThrough;
    checkForBrokenPassThrough_();
    return *this;
}

inline Command& Command::combineFlagAndOptionalValue(bool combine) {
    impl_->combineFlagAndOptionalValue_ = combine;
    return *this;
}

inline Help Command::createHelp() const {
    Help help;
    // Apply configuration via the fluent setters.
    for (const auto& [key, val] : impl_->helpConfiguration_) {
        if (key == "helpWidth" && val.isNumber()) {
            help.helpWidth(val.asInt());
        } else if (key == "sortSubcommands" && val.isBool()) {
            help.sortSubcommands(val.asBool());
        } else if (key == "sortOptions" && val.isBool()) {
            help.sortOptions(val.asBool());
        } else if (key == "showGlobalOptions" && val.isBool()) {
            help.showGlobalOptions(val.asBool());
        }
    }
    return help;
}

// ---- Object-property accessors ----

inline const std::deque<Command>& Command::commands() const { return impl_->commands_; }
inline const std::vector<Option>& Command::options() const { return impl_->options_; }
inline const std::vector<Argument>& Command::registeredArguments() const {
    return impl_->registeredArguments_;
}
inline const std::vector<std::string>& Command::args() const { return impl_->args_; }
inline const std::vector<std::string>& Command::rawArgs() const { return impl_->rawArgs_; }
inline const std::vector<polycpp::JsonValue>& Command::processedArgs() const {
    return impl_->processedArgs_;
}
inline bool Command::hidden() const { return impl_->hidden_; }
inline bool Command::hasParent() const noexcept {
    return !impl_->parent_.expired();
}

inline std::optional<Command> Command::parent() const {
    auto parentSp = impl_->parent_.lock();
    if (!parentSp) return std::nullopt;
    // Build a handle that shares ownership of the parent's Impl (the
    // shared_ptr came from `lock()`, so the Impl is guaranteed alive
    // for the lifetime of this returned Command).
    Command result;
    result.impl_ = std::move(parentSp);
    result.setEmitter_(result.impl_->ee);
    return result;
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
    if (impl_->allowUnknownOption_) return;
    std::string suggestion;

    if (flag.size() >= 2 && flag[0] == '-' && flag[1] == '-' && impl_->showSuggestionAfterError_) {
        std::vector<std::string> candidateFlags;
        // Walk this command + ancestors. We cannot rely on
        // getCommandAndAncestors_() here because that function stops at the
        // root (the root's handle is not nested in any commands_ deque).
        // For suggestion building it is sufficient to walk the impl chain
        // and read options off each ancestor's Impl directly.
        auto helper = createHelp();
        auto visOpts = helper.visibleOptions(*this);
        for (const auto& opt : visOpts) {
            if (opt.long_.has_value()) {
                candidateFlags.push_back(*opt.long_);
            }
        }
        // parent_ is a weak_ptr; lock at each step.
        std::shared_ptr<Command::Impl> ancestorImpl = impl_->parent_.lock();
        std::shared_ptr<Command::Impl> prevImpl = impl_;
        while (ancestorImpl && !prevImpl->enablePositionalOptions_) {
            for (const auto& opt : ancestorImpl->options_) {
                if (!opt.hidden && opt.long_.has_value()) {
                    candidateFlags.push_back(*opt.long_);
                }
            }
            prevImpl = ancestorImpl;
            ancestorImpl = ancestorImpl->parent_.lock();
        }
        suggestion = suggestSimilar(flag, candidateFlags);
    }

    error("error: unknown option '" + flag + "'" + suggestion,
          {.exitCode = 1, .code = "commander.unknownOption"});
}

inline void Command::unknownCommand() {
    std::string unknownName = impl_->args_.empty() ? "" : impl_->args_[0];
    std::string suggestion;

    if (impl_->showSuggestionAfterError_) {
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
    if (impl_->exitCallback_) {
        impl_->exitCallback_(CommanderError(exitCode, code, message));
        return; // In case the callback returns
    }
    std::exit(exitCode);
}

inline std::vector<std::string> Command::prepareUserArgs_(const std::vector<std::string>& argv,
                                                           const ParseOptions& parseOpts) {
    std::vector<std::string> effectiveArgv = argv;
    impl_->rawArgs_ = effectiveArgv;

    std::vector<std::string> userArgs;
    if (parseOpts.from == "node") {
        if (effectiveArgv.size() > 1) {
            impl_->scriptPath_ = effectiveArgv[1];
        }
        if (effectiveArgv.size() > 2) {
            userArgs.assign(effectiveArgv.begin() + 2, effectiveArgv.end());
        }
    } else if (parseOpts.from == "user") {
        userArgs = effectiveArgv;
    } else if (parseOpts.from == "electron") {
        if (effectiveArgv.size() > 1) {
            impl_->scriptPath_ = effectiveArgv[1];
        }
        if (effectiveArgv.size() > 2) {
            userArgs.assign(effectiveArgv.begin() + 2, effectiveArgv.end());
        }
    } else if (parseOpts.from == "native") {
        // C++-runtime argv shape: [program, ...userArgs]. Skip just the
        // program name. Used by parse() with no arguments.
        if (!effectiveArgv.empty()) {
            impl_->scriptPath_ = effectiveArgv.front();
        }
        if (effectiveArgv.size() > 1) {
            userArgs.assign(effectiveArgv.begin() + 1, effectiveArgv.end());
        }
    } else {
        throw std::runtime_error("unexpected parse option { from: '" + parseOpts.from + "' }");
    }

    // Find default name
    if (impl_->name_.empty() && !impl_->scriptPath_.empty()) {
        nameFromFilename(impl_->scriptPath_);
    }
    if (impl_->name_.empty()) {
        impl_->name_ = "program";
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
    impl_->args_ = operands;
    impl_->args_.insert(impl_->args_.end(), unknownArgs.begin(), unknownArgs.end());

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
    if (!impl_->defaultCommandName_.empty()) {
        outputHelpIfRequested_(unknownArgs);
        dispatchSubcommand_(impl_->defaultCommandName_, operands, unknownArgs);
        return;
    }

    // No subcommands matched, no action, and has subcommands: show help
    if (!impl_->commands_.empty() && impl_->args_.empty() && !impl_->actionHandler_ &&
        impl_->defaultCommandName_.empty()) {
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

    if (impl_->actionHandler_) {
        checkForUnknownOptions();
        processArguments_();

        callHooks_("preAction");
        impl_->actionHandler_(impl_->processedArgs_);
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
        if (!impl_->commands_.empty()) {
            unknownCommand();
            return;
        }
        checkForUnknownOptions();
        processArguments_();
    } else if (!impl_->commands_.empty()) {
        checkForUnknownOptions();
        help({.error = true});
    } else {
        checkForUnknownOptions();
        processArguments_();
    }
}

inline void Command::parseOptionsEnv_() {
    auto envObj = polycpp::process::env();
    for (const auto& option : impl_->options_) {
        if (option.envVar_.has_value()) {
            auto envIt = envObj.find(*option.envVar_);
            if (envIt != envObj.end()) {
                std::string envVal = envIt->second;
                std::string optionKey = option.attributeName();
                auto currentValue = getOptionValue(optionKey);
                auto currentSource = getOptionValueSource(optionKey);

                if (currentValue.isNull() ||
                    currentSource == "default" || currentSource == "config" || currentSource == "env") {
                    if (option.required || option.optional) {
                        emitInternalEvent_("optionEnv:" + option.name(), envVal);
                    } else {
                        emitInternalEvent_("optionEnv:" + option.name());
                    }
                }
            }
        }
    }
}

inline void Command::onInternalEvent_(
    const std::string& eventName,
    std::function<void(const std::optional<std::string>&)> listener) {
    impl_->internalEventHandlers_[eventName].push_back(std::move(listener));
}

inline void Command::emitInternalEvent_(const std::string& eventName, std::optional<std::string> value) {
    auto it = impl_->internalEventHandlers_.find(eventName);
    if (it == impl_->internalEventHandlers_.end()) {
        return;
    }
    for (const auto& listener : it->second) {
        listener(value);
    }
}

inline std::string Command::renderHelpText_(const std::string& position) const {
    auto it = impl_->helpText_.find(position);
    if (it == impl_->helpText_.end()) {
        return "";
    }

    std::string rendered;
    for (const auto& text : it->second) {
        if (text.empty()) {
            continue;
        }
        rendered += text;
        if (text.back() != '\n') {
            rendered += '\n';
        }
    }
    return rendered;
}

inline void Command::parseOptionsImplied_() {
    auto hasCustomOptionValue = [this](const std::string& optionKey) -> bool {
        auto val = getOptionValue(optionKey);
        if (val.isNull()) return false;
        auto src = getOptionValueSource(optionKey);
        return src != "default" && src != "implied";
    };

    for (const auto& option : impl_->options_) {
        if (option.implied_.has_value() && hasCustomOptionValue(option.attributeName())) {
            // Check value from option (simplified DualOptions logic)
            auto val = getOptionValue(option.attributeName());
            bool valueFromOption = true;

            // Simple dual options check
            if (option.negate) {
                // For negate options, only apply implies if value is false
                if (val.isBool()) {
                    valueFromOption = !val.asBool();
                }
            } else {
                // For positive options, apply if value is truthy
                if (val.isBool()) {
                    valueFromOption = val.asBool();
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
    for (size_t i = 0; i < impl_->registeredArguments_.size(); ++i) {
        if (impl_->registeredArguments_[i].required && i >= impl_->args_.size()) {
            missingArgument(impl_->registeredArguments_[i].name());
        }
    }
    // too many
    if (!impl_->registeredArguments_.empty() && impl_->registeredArguments_.back().variadic) {
        return;
    }
    if (impl_->args_.size() > impl_->registeredArguments_.size()) {
        excessArguments_(impl_->args_);
    }
}

inline void Command::processArguments_() {
    checkNumberOfArguments_();

    impl_->processedArgs_.clear();
    for (size_t index = 0; index < impl_->registeredArguments_.size(); ++index) {
        const auto& declaredArg = impl_->registeredArguments_[index];
        polycpp::JsonValue value = declaredArg.defaultValue_;

        if (declaredArg.variadic) {
            if (index < impl_->args_.size()) {
                std::vector<std::string> values(impl_->args_.begin() + index, impl_->args_.end());
                if (declaredArg.parseArg_) {
                    polycpp::JsonValue processed = declaredArg.defaultValue_;
                    for (const auto& v : values) {
                        std::string invalidMsg = "error: command-argument value '" + v +
                                                  "' is invalid for argument '" + declaredArg.name() + "'.";
                        processed = callParseArg_(declaredArg, v, processed, invalidMsg);
                    }
                    value = processed;
                } else {
                    polycpp::JsonArray arr;
                    for (const auto& v : values) {
                        arr.push_back(polycpp::JsonValue(v));
                    }
                    value = polycpp::JsonValue(std::move(arr));
                }
            } else if (value.isNull()) {
                value = polycpp::JsonValue(polycpp::JsonArray{});
            }
        } else if (index < impl_->args_.size()) {
            std::string argValue = impl_->args_[index];
            if (declaredArg.parseArg_) {
                std::string invalidMsg = "error: command-argument value '" + argValue +
                                          "' is invalid for argument '" + declaredArg.name() + "'.";
                value = callParseArg_(declaredArg, argValue, declaredArg.defaultValue_, invalidMsg);
            } else {
                value = polycpp::JsonValue(argValue);
            }
        }
        impl_->processedArgs_.push_back(value);
    }
}

inline void Command::checkForMissingMandatoryOptions_() {
    // Walk impl chain to cover the root command (which does not appear in
    // getCommandAndAncestors_ when reached from a descendant). parent_ is
    // a weak_ptr; lock at each step.
    for (auto current = impl_; current; current = current->parent_.lock()) {
        for (const auto& opt : current->options_) {
            if (opt.mandatory) {
                bool missing = !current->optionValues_.isObject() ||
                               !current->optionValues_.hasKey(opt.attributeName()) ||
                               current->optionValues_.asObject().at(opt.attributeName()).isNull();
                if (missing) {
                    // Use *this for error reporting (output config inherits).
                    missingMandatoryOptionValue(opt);
                }
            }
        }
    }
}

inline void Command::checkForConflictingOptions_() {
    // Walk the impl chain so that we cover the root command too. parent_
    // is a weak_ptr; lock at each step.
    for (auto current = impl_; current; current = current->parent_.lock()) {
        std::vector<const Option*> definedNonDefault;
        for (const auto& opt : current->options_) {
            std::string key = opt.attributeName();
            if (!current->optionValues_.isObject() || !current->optionValues_.hasKey(key)) continue;
            auto val = current->optionValues_.asObject().at(key);
            if (val.isNull()) continue;
            auto srcIt = current->optionValueSources_.find(key);
            if (srcIt != current->optionValueSources_.end() && srcIt->second == "default") continue;
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
}

inline void Command::checkForConflictingLocalOptions_() {
    std::vector<const Option*> definedNonDefault;
    for (const auto& opt : impl_->options_) {
        std::string key = opt.attributeName();
        auto val = getOptionValue(key);
        if (val.isNull()) continue;
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
    auto parentSp = impl_->parent_.lock();
    if (parentSp && impl_->passThroughOptions_ && !parentSp->enablePositionalOptions_) {
        throw std::runtime_error(
            "passThroughOptions cannot be used for '" + impl_->name_ +
            "' without turning on enablePositionalOptions for parent command(s)");
    }
}

inline void Command::excessArguments_(const std::vector<std::string>& receivedArgs) {
    if (impl_->allowExcessArguments_) return;
    int expected = static_cast<int>(impl_->registeredArguments_.size());
    std::string s = (expected == 1) ? "" : "s";
    std::string forSubcommand = !impl_->parent_.expired() ? " for '" + name() + "'" : "";
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

    if (subCommand->impl_->executableHandler_) {
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
    std::string execName = subCommand.impl_->executableFile_.value_or(
        impl_->name_ + "-" + subCommand.name());

    // Determine search directory
    std::string searchDir;
    if (impl_->executableDir_.has_value()) {
        searchDir = *impl_->executableDir_;
    }

    // If we have a scriptPath_, resolve relative to its directory
    if (!impl_->scriptPath_.empty()) {
        std::string resolvedScript;
        try {
            resolvedScript = polycpp::fs::realpathSync(impl_->scriptPath_);
        } catch (...) {
            resolvedScript = impl_->scriptPath_;
        }
        std::string scriptDir = detail_exec::dirname(resolvedScript);
        if (!searchDir.empty()) {
            if (!detail_exec::isAbsolute(searchDir)) {
                searchDir = detail_exec::join(scriptDir, searchDir);
            }
        } else {
            searchDir = scriptDir;
        }
    }

    // Look for a local file in the search directory first
    std::string resolvedPath;
    if (!searchDir.empty()) {
        resolvedPath = detail_exec::findExecutableInDirectory(searchDir, execName);
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

    // Spawn the child process asynchronously and drive the EventLoop until
    // it exits. Async spawn (rather than spawnSync) is required so the parent
    // can react to incoming signals (SIGINT/SIGTERM/...) and forward them to
    // the child via `ChildProcess::kill(signum)` — see SignalForwarder above.
    polycpp::child_process::SpawnOptions spawnOpts;
    spawnOpts.stdio = "inherit";

    std::string spawnFile = resolvedPath;
#if defined(_WIN32)
    if (detail_exec::isBatchFile(resolvedPath)) {
        launchArgs.insert(launchArgs.begin(), resolvedPath);
        launchArgs.insert(launchArgs.begin(), "/c");
        launchArgs.insert(launchArgs.begin(), "/s");
        launchArgs.insert(launchArgs.begin(), "/d");
        spawnFile = detail_exec::windowsCommandInterpreter();
    }
#endif

    auto child = polycpp::child_process::spawn(spawnFile, launchArgs, spawnOpts);

    // Forward parent-received signals to the child while it is running.
    // Destroying `forwarder` deactivates the per-spawn lambdas (no-op after
    // exit), preventing residual listeners from interfering with future
    // spawns or with user-installed handlers.
    detail_exec::SignalForwarder forwarder(child);

    // Capture the result of the spawn lifecycle on the heap so the lambdas
    // can safely outlive the stack frame (they run on the EventLoop thread).
    struct ExitState {
        bool        seen       = false;
        int         code       = 0;
        std::string signalName;
        std::string spawnError;
    };
    auto state = std::make_shared<ExitState>();

    child.on(polycpp::child_process::event::Exit,
             [state](int code, const std::string& signalName) {
        state->seen       = true;
        state->code       = code;
        state->signalName = signalName;
        polycpp::EventLoop::instance().stop();
    });
    child.on(polycpp::child_process::event::Error_,
             [state](const polycpp::Error& err) {
        state->seen       = true;
        state->spawnError = err.what();
        polycpp::EventLoop::instance().stop();
    });

    // Run the EventLoop until one of the handlers above stops it. Queued
    // signals dispatch through the SignalForwarder while we wait.
    polycpp::EventLoop::instance().run();
    polycpp::EventLoop::instance().restart(); // reset for the caller's next run()

    forwarder.deactivate(); // child is gone; stop forwarding immediately.

    if (!state->spawnError.empty()) {
        if (state->spawnError.find("ENOENT") != std::string::npos) {
            std::string msg = "'" + execName + "' does not exist\n - searched for: " + resolvedPath;
            error(msg, {.exitCode = 1, .code = "commander.executeSubCommandAsync"});
        } else {
            error(state->spawnError, {.exitCode = 1, .code = "commander.executeSubCommandAsync"});
        }
        return;
    }

    if (state->code != 0) {
        exit_(state->code, "commander.executeSubCommandAsync", "(close)");
    }
}

inline std::string Command::findProgram_(const std::string& name) const {
    // If the name contains a path separator, treat as a path
    if (detail_exec::containsPathSeparator(name)) {
        return detail_exec::findExecutablePath(name);
    }

    // Search PATH
    std::string pathStr = polycpp::process::getenv("PATH");
    if (pathStr.empty()) return "";

    std::istringstream stream(pathStr);
    std::string dir;
    while (std::getline(stream, dir, detail_exec::pathDelimiter())) {
        if (dir.empty()) continue;
        std::string candidate = detail_exec::findExecutableInDirectory(dir, name);
        if (!candidate.empty()) return candidate;
    }
    return "";
}

inline void Command::dispatchHelpCommand_(const std::string& subcommandName) {
    if (subcommandName.empty()) {
        help();
        return;
    }
    Command* subCommand = findCommand_(subcommandName);
    if (subCommand && !subCommand->impl_->executableHandler_) {
        subCommand->help();
        return;
    }
    // Fallback: dispatch with help flag (also used for executable subcommands)
    auto* helpOpt = getHelpOption_();
    std::string helpFlag = helpOpt && helpOpt->long_.has_value() ? *helpOpt->long_ : "--help";
    dispatchSubcommand_(subcommandName, {}, {helpFlag});
}

inline void Command::callHooks_(const std::string& event) {
    // Walk impl chain so we cover the root command too. parent_ is a
    // weak_ptr; lock at each step so we keep ancestors alive for the
    // duration of the walk and the synthesised handles below.
    struct Entry { std::shared_ptr<Command::Impl> impl; HookFn fn; };
    std::vector<Entry> hooks;
    for (auto current = impl_; current; current = current->parent_.lock()) {
        auto hookIt = current->lifeCycleHooks_.find(event);
        if (hookIt != current->lifeCycleHooks_.end()) {
            for (const auto& callback : hookIt->second) {
                hooks.push_back({current, callback});
            }
        }
    }
    // Reverse so root is first.
    std::reverse(hooks.begin(), hooks.end());
    if (event == "postAction") {
        std::reverse(hooks.begin(), hooks.end());
    }
    for (auto& entry : hooks) {
        // We need a Command& for the callback. Synthesise a transient
        // handle whose impl_ shares ownership of the ancestor's Impl.
        Command hookedCommand;
        hookedCommand.impl_ = entry.impl;
        hookedCommand.setEmitter_(hookedCommand.impl_->ee);
        entry.fn(hookedCommand, *this);
    }
}

inline void Command::callSubCommandHook_(Command& subCommand, const std::string& event) {
    auto hookIt = impl_->lifeCycleHooks_.find(event);
    if (hookIt != impl_->lifeCycleHooks_.end()) {
        for (const auto& hookFn : hookIt->second) {
            hookFn(*this, subCommand);
        }
    }
}

inline polycpp::JsonValue Command::callParseArg_(const Option& target, const std::string& value,
                                        const polycpp::JsonValue& previous, const std::string& invalidArgMsg) {
    try {
        if (target.parseArg_) {
            return target.parseArg_(value, previous);
        }
        return polycpp::JsonValue(value);
    } catch (const InvalidArgumentError& err) {
        std::string message = invalidArgMsg + " " + std::string(err.what()).substr(7);
        error(message, {err.exitCode, err.code});
        throw;
    }
}

inline polycpp::JsonValue Command::callParseArg_(const Argument& target, const std::string& value,
                                        const polycpp::JsonValue& previous, const std::string& invalidArgMsg) {
    try {
        if (target.parseArg_) {
            return target.parseArg_(value, previous);
        }
        return polycpp::JsonValue(value);
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
            (impl_->name_.empty() ? "" : " to command '" + impl_->name_ + "'") +
            " due to conflicting flag '" + matchingFlag +
            "'\n-  already used by option '" + matchingOption->flags + "'");
    }
    impl_->options_.push_back(option);
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
    for (auto& cmd : impl_->commands_) {
        if (cmd.impl_->name_ == name) return &cmd;
        for (const auto& a : cmd.impl_->aliases_) {
            if (a == name) return &cmd;
        }
    }
    return nullptr;
}

inline const Option* Command::findOption_(const std::string& arg) const {
    for (const auto& opt : impl_->options_) {
        if (opt.is(arg)) return &opt;
    }
    return nullptr;
}

inline const Option* Command::getHelpOption_() {
    if (!impl_->helpOption_.has_value()) {
        // Lazy create
        impl_->helpOption_ = std::make_unique<Option>("-h, --help", "display help for command");
    }
    return impl_->helpOption_->get();
}

inline Command* Command::getHelpCommand_() {
    bool hasImplicit = false;
    if (impl_->addImplicitHelpCommand_.has_value()) {
        hasImplicit = *impl_->addImplicitHelpCommand_;
    } else {
        hasImplicit = !impl_->commands_.empty() && !impl_->actionHandler_ && !findCommand_("help");
    }

    if (hasImplicit) {
        if (!impl_->helpCommand_.has_value()) {
            auto helpCmd = std::make_unique<Command>("help");
            helpCmd->helpOption(false);
            helpCmd->arguments("[command]");
            helpCmd->description("display help for command");
            impl_->helpCommand_ = std::move(helpCmd);
        }
        return impl_->helpCommand_->get();
    }
    return nullptr;
}

inline std::vector<const Command*> Command::getCommandAndAncestors_() const {
    // Returns this command first, then walks up the parent chain. The root
    // command may not be reachable as a stable `const Command*` if it lives
    // outside any commands_ deque (e.g. the user's stack-allocated root);
    // we still return its self pointer when we are it. Callers that only
    // need to iterate options/values up the chain should use the impl
    // walk directly via `impl_->parent_.lock()`.
    std::vector<const Command*> result;
    result.push_back(this);
    auto parentImpl = impl_->parent_.lock();
    while (parentImpl) {
        const Command* parentHandle = nullptr;
        if (auto grandparentImpl = parentImpl->parent_.lock()) {
            for (const auto& sibling : grandparentImpl->commands_) {
                if (sibling.impl_.get() == parentImpl.get()) {
                    parentHandle = &sibling;
                    break;
                }
            }
        }
        if (!parentHandle) break;
        result.push_back(parentHandle);
        parentImpl = parentImpl->parent_.lock();
    }
    return result;
}

inline void Command::prepareForParse_() {
    if (!impl_->savedState_.has_value()) {
        saveStateBeforeParse_();
    } else {
        restoreStateBeforeParse_();
    }
}

inline void Command::saveStateBeforeParse_() {
    impl_->savedState_ = Impl::SavedState{
        .name = impl_->name_,
        .optionValues = impl_->optionValues_,
        .optionValueSources = impl_->optionValueSources_
    };
}

inline void Command::restoreStateBeforeParse_() {
    if (!impl_->savedState_.has_value()) return;
    impl_->name_ = impl_->savedState_->name;
    impl_->scriptPath_.clear();
    impl_->rawArgs_.clear();
    impl_->optionValues_ = impl_->savedState_->optionValues;
    impl_->optionValueSources_ = impl_->savedState_->optionValueSources;
    impl_->args_.clear();
    impl_->processedArgs_.clear();
}

// ---- Async API ----

inline Command& Command::actionAsync(AsyncActionFn fn) {
    impl_->asyncActionHandler_ = [this, fn = std::move(fn)](const std::vector<polycpp::JsonValue>& processedArgs)
        -> polycpp::Promise<void> {
        auto optsMap = opts();
        return fn(processedArgs, optsMap, *this);
    };
    return *this;
}

inline Command& Command::hookAsync(const std::string& event, AsyncHookFn listener) {
    std::vector<std::string> allowed = {"preSubcommand", "preAction", "postAction"};
    if (std::find(allowed.begin(), allowed.end(), event) == allowed.end()) {
        throw std::runtime_error(
            "Unexpected value for event passed to hookAsync : '" + event +
            "'.\nExpecting one of 'preSubcommand', 'preAction', 'postAction'");
    }
    impl_->asyncLifeCycleHooks_[event].push_back(std::move(listener));
    return *this;
}

inline polycpp::Promise<std::reference_wrapper<Command>>
Command::parseAsync(const std::vector<std::string>& argv, const ParseOptions& parseOpts) {
    prepareForParse_();
    auto userArgs = prepareUserArgs_(argv, parseOpts);

    return parseCommandAsync_({}, userArgs)
        .then([this]() -> std::reference_wrapper<Command> {
            return std::ref(*this);
        });
}

inline polycpp::Promise<void>
Command::parseCommandAsync_(const std::vector<std::string>& initialOperands,
                              const std::vector<std::string>& unknown) {
    // Option parsing is always synchronous
    auto parsed = parseOptions(unknown);
    parseOptionsEnv_();
    parseOptionsImplied_();

    std::vector<std::string> operands = initialOperands;
    operands.insert(operands.end(), parsed.operands.begin(), parsed.operands.end());
    auto unknownArgs = parsed.unknown;
    impl_->args_ = operands;
    impl_->args_.insert(impl_->args_.end(), unknownArgs.begin(), unknownArgs.end());

    // Subcommand dispatch
    if (!operands.empty() && findCommand_(operands[0])) {
        std::vector<std::string> subOperands(operands.begin() + 1, operands.end());
        return dispatchSubcommandAsync_(operands[0], subOperands, unknownArgs);
    }

    // Help command dispatch
    if (getHelpCommand_() && !operands.empty() && operands[0] == getHelpCommand_()->name()) {
        std::string subName = operands.size() > 1 ? operands[1] : "";
        dispatchHelpCommand_(subName);
        return polycpp::Promise<void>::resolve();
    }

    // Default command
    if (!impl_->defaultCommandName_.empty()) {
        outputHelpIfRequested_(unknownArgs);
        return dispatchSubcommandAsync_(impl_->defaultCommandName_, operands, unknownArgs);
    }

    // No subcommands matched, no action, and has subcommands: show help
    if (!impl_->commands_.empty() && impl_->args_.empty() && !impl_->actionHandler_ &&
        !impl_->asyncActionHandler_ && impl_->defaultCommandName_.empty()) {
        help({.error = true});
        return polycpp::Promise<void>::resolve();
    }

    outputHelpIfRequested_(parsed.unknown);
    checkForMissingMandatoryOptions_();
    checkForConflictingOptions_();

    auto checkForUnknownOptions = [&]() {
        if (!parsed.unknown.empty()) {
            unknownOption(parsed.unknown[0]);
        }
    };

    if (impl_->actionHandler_ || impl_->asyncActionHandler_) {
        checkForUnknownOptions();
        processArguments_();

        // Chain: preAction hooks → action → postAction hooks
        return callHooksAsync_("preAction")
            .then([this]() -> polycpp::Promise<void> {
                if (impl_->asyncActionHandler_) {
                    return impl_->asyncActionHandler_(impl_->processedArgs_);
                } else if (impl_->actionHandler_) {
                    impl_->actionHandler_(impl_->processedArgs_);
                    return polycpp::Promise<void>::resolve();
                }
                return polycpp::Promise<void>::resolve();
            })
            .then([this]() -> polycpp::Promise<void> {
                return callHooksAsync_("postAction");
            });
    }

    // No action handler
    if (!operands.empty()) {
        if (findCommand_("*")) {
            std::vector<std::string> subOperands(operands.begin(), operands.end());
            return dispatchSubcommandAsync_("*", subOperands, unknownArgs);
        }
        if (!impl_->commands_.empty()) {
            unknownCommand();
            return polycpp::Promise<void>::resolve();
        }
        checkForUnknownOptions();
        processArguments_();
    } else if (!impl_->commands_.empty()) {
        checkForUnknownOptions();
        help({.error = true});
    } else {
        checkForUnknownOptions();
        processArguments_();
    }

    return polycpp::Promise<void>::resolve();
}

inline polycpp::Promise<void>
Command::dispatchSubcommandAsync_(const std::string& commandName,
                                    const std::vector<std::string>& operands,
                                    const std::vector<std::string>& unknown) {
    Command* subCommand = findCommand_(commandName);
    if (!subCommand) {
        help({.error = true});
        return polycpp::Promise<void>::resolve();
    }

    subCommand->prepareForParse_();
    callSubCommandHook_(*subCommand, "preSubcommand");

    if (subCommand->impl_->executableHandler_) {
        executeSubCommand_(*subCommand, operands, unknown);
        return polycpp::Promise<void>::resolve();
    }

    return subCommand->parseCommandAsync_(operands, unknown);
}

inline polycpp::Promise<void>
Command::callHooksAsync_(const std::string& event) {
    // Collect impls along the chain, root first. parent_ is a weak_ptr;
    // lock at each step. The locked shared_ptrs are also what we capture
    // into the chained lambdas below to keep ancestors alive across the
    // promise-chain's asynchronous boundaries.
    std::vector<std::shared_ptr<Command::Impl>> chainImpls;
    for (auto current = impl_; current; current = current->parent_.lock()) {
        chainImpls.push_back(current);
    }
    std::reverse(chainImpls.begin(), chainImpls.end());

    // Collect all hooks (sync + async) from ancestors
    struct HookEntry {
        std::shared_ptr<Command::Impl> implPtr;
        bool isAsync;
        size_t syncIdx;
        size_t asyncIdx;
    };
    std::vector<HookEntry> hooks;

    for (const auto& implPtr : chainImpls) {
        auto syncIt = implPtr->lifeCycleHooks_.find(event);
        if (syncIt != implPtr->lifeCycleHooks_.end()) {
            for (size_t i = 0; i < syncIt->second.size(); ++i) {
                hooks.push_back({implPtr, false, i, 0});
            }
        }
        auto asyncIt = implPtr->asyncLifeCycleHooks_.find(event);
        if (asyncIt != implPtr->asyncLifeCycleHooks_.end()) {
            for (size_t i = 0; i < asyncIt->second.size(); ++i) {
                hooks.push_back({implPtr, true, 0, i});
            }
        }
    }

    if (event == "postAction") {
        std::reverse(hooks.begin(), hooks.end());
    }

    // Chain all hooks sequentially
    polycpp::Promise<void> chain = polycpp::Promise<void>::resolve();
    auto selfImpl = impl_;  // keep self alive across the chain
    Command* actionCommand = this;

    for (const auto& entry : hooks) {
        auto hookedImpl = entry.implPtr;  // keeps the ancestor alive in the lambda

        if (entry.isAsync) {
            auto& hookFn = hookedImpl->asyncLifeCycleHooks_[event][entry.asyncIdx];
            chain = chain.then([selfImpl, hookedImpl, actionCommand, &hookFn]() -> polycpp::Promise<void> {
                Command hookedCommand;
                hookedCommand.impl_ = hookedImpl;
                hookedCommand.setEmitter_(hookedCommand.impl_->ee);
                return hookFn(hookedCommand, *actionCommand);
            });
        } else {
            auto& hookFn = hookedImpl->lifeCycleHooks_[event][entry.syncIdx];
            chain = chain.then([selfImpl, hookedImpl, actionCommand, &hookFn]() {
                Command hookedCommand;
                hookedCommand.impl_ = hookedImpl;
                hookedCommand.setEmitter_(hookedCommand.impl_->ee);
                hookFn(hookedCommand, *actionCommand);
            });
        }
    }

    return chain;
}

} // namespace commander
} // namespace polycpp
