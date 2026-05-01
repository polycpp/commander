#pragma once

/**
 * @file detail/command_impl.hpp
 * @brief Definition of `Command::Impl` — the per-command shared state.
 *
 * `Command` is a handle that owns a `std::shared_ptr<Command::Impl>`; copies
 * of the handle share the same Impl. All inline method bodies in
 * `detail/command.hpp` and the Help internals in `detail/help.hpp` access
 * Impl members through that pointer.
 *
 * This file lives in the aggregator just after the type declarations and
 * before any inline-implementation header that needs to read or write Impl
 * fields.
 *
 * @since 0.2.0
 */

#include <polycpp/commander/command.hpp>

#include <polycpp/events/events.hpp>

#include <deque>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <variant>
#include <vector>

namespace polycpp {
namespace commander {

/**
 * @brief Internal state for a Command handle.
 *
 * Holds every field that used to be a private member of `Command`, plus the
 * `polycpp::events::EventEmitter` that the handle's
 * `EventEmitterForwarder` base forwards to.
 */
struct Command::Impl {
    // Events forwarded by the handle.
    polycpp::events::EventEmitter ee;

    // Identity
    std::string name_;
    std::string description_;
    std::string summary_;
    std::string usage_;
    std::string version_;
    std::string versionOptionName_;
    std::vector<std::string> aliases_;
    std::string scriptPath_;

    // Subcommand graph. `std::deque` so references handed out by
    // `Command::command(...)` survive subsequent inserts.
    std::deque<Command> commands_;
    /// Weak reference to the parent command's Impl. weak_ptr (rather
    /// than a raw pointer) ensures `Command::parent()` cannot
    /// dereference freed memory if a child handle is copied out and
    /// outlives its parent.
    std::weak_ptr<Command::Impl> parent_;

    // Options & arguments
    std::vector<Option> options_;
    std::vector<Argument> registeredArguments_;
    std::vector<std::string> args_;
    std::vector<std::string> rawArgs_;
    std::vector<polycpp::JsonValue> processedArgs_;

    // Executable subcommand state
    bool executableHandler_ = false;
    std::optional<std::string> executableFile_;
    std::optional<std::string> executableDir_;

    // Parsing flags
    bool allowUnknownOption_ = false;
    bool allowExcessArguments_ = false;
    bool enablePositionalOptions_ = false;
    bool passThroughOptions_ = false;
    bool combineFlagAndOptionalValue_ = true;
    bool hidden_ = false;
    bool showSuggestionAfterError_ = true;

    /// false = no help after error, true = show help, string = custom message.
    std::variant<bool, std::string> showHelpAfterError_ = false;

    // Option values
    OptionValues optionValues_;
    OptionValueSources optionValueSources_;

    /// Internal action handler takes just processedArgs; action() wraps the user's ActionFn.
    std::function<void(const std::vector<polycpp::JsonValue>&)> actionHandler_;
    /// Internal async action handler; set by actionAsync().
    std::function<polycpp::Promise<void>(const std::vector<polycpp::JsonValue>&)> asyncActionHandler_;
    std::function<void(const CommanderError&)> exitCallback_;

    std::unordered_map<std::string, std::vector<HookFn>> lifeCycleHooks_;
    std::unordered_map<std::string, std::vector<AsyncHookFn>> asyncLifeCycleHooks_;
    std::unordered_map<std::string,
                       std::vector<std::function<void(const std::optional<std::string>&)>>>
        internalEventHandlers_;
    std::unordered_map<std::string, std::vector<std::string>> helpText_;

    OutputConfiguration outputConfiguration_;

    /// nullptr = help disabled; nullopt = lazy create; has_value = configured.
    std::optional<std::unique_ptr<Option>> helpOption_;

    /// nullptr = no help cmd; nullopt = lazy create; has_value = configured.
    std::optional<std::unique_ptr<Command>> helpCommand_;
    std::optional<bool> addImplicitHelpCommand_;

    std::string defaultCommandName_;

    std::map<std::string, polycpp::JsonValue> helpConfiguration_;

    struct SavedState {
        std::string name;
        OptionValues optionValues;
        OptionValueSources optionValueSources;
    };
    std::optional<SavedState> savedState_;
};

} // namespace commander
} // namespace polycpp
