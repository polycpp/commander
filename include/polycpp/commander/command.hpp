#pragma once

/**
 * @file command.hpp
 * @brief Command class — the heart of the commander library.
 * @see https://github.com/tj/commander.js
 * @since 0.1.0
 */

#include <polycpp/commander/argument.hpp>
#include <polycpp/commander/error.hpp>
#include <polycpp/commander/help.hpp>
#include <polycpp/commander/option.hpp>

#include <polycpp/events/detail/aggregator.hpp>
#include <polycpp/core/promise.hpp>

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

/// @brief Options controlling how argv is parsed.
/// @since 0.1.0
struct ParseOptions {
    /// @brief Where the args are from: "node", "user", or "electron".
    std::string from = "node";
};

/// @brief Result of parseOptions() — operands and unknown arguments.
/// @since 0.1.0
struct ParseOptionsResult {
    std::vector<std::string> operands; ///< Positional operands.
    std::vector<std::string> unknown;  ///< Unknown arguments.
};

/// @brief Context for help output.
/// @since 0.1.0
struct HelpContext {
    bool error = false; ///< Whether help is being shown due to an error.
};

/// @brief Configuration for output writing.
/// @since 0.1.0
struct OutputConfiguration {
    std::function<void(const std::string&)> writeOut;   ///< Write to stdout.
    std::function<void(const std::string&)> writeErr;   ///< Write to stderr.
    std::function<void(const std::string&, std::function<void(const std::string&)>)> outputError; ///< Error output handler.
    std::function<int()> getOutHelpWidth;               ///< Get stdout help width.
    std::function<int()> getErrHelpWidth;               ///< Get stderr help width.
    std::function<bool()> getOutHasColors;              ///< Whether stdout supports ANSI colors.
    std::function<bool()> getErrHasColors;              ///< Whether stderr supports ANSI colors.
};

/// @brief Type alias for option value storage (always a JsonObject).
/// @since 0.1.0
using OptionValues = polycpp::JsonValue;

/// @brief Type alias for option value source tracking.
/// @since 0.1.0
using OptionValueSources = std::unordered_map<std::string, std::string>;

/// @brief Type alias for error options passed to error().
/// @since 0.1.0
struct ErrorOptions {
    int exitCode = 1;                    ///< Exit code for process.exit().
    std::string code = "commander.error"; ///< Error identifier code.
};

/// @brief Type alias for command options (isDefault, hidden).
/// @since 0.1.0
struct CommandOptions {
    bool isDefault = false; ///< Whether this is the default command.
    bool hidden = false;    ///< Whether to hide from help.
};

/// @brief Type alias for action handler functions.
///
/// The action handler receives (processedArgs..., opts, command).
/// In C++ we pass the processed args as a vector of polycpp::JsonValue,
/// the opts as OptionValues (a JsonValue object), and a reference to the command.
/// @since 0.1.0
using ActionFn = std::function<void(const std::vector<polycpp::JsonValue>& args,
                                     const polycpp::JsonValue& opts,
                                     Command& cmd)>;

/// @brief Type alias for hook functions.
/// @since 0.1.0
using HookFn = std::function<void(Command& thisCommand, Command& actionCommand)>;

/// @brief Type alias for async action handler functions.
///
/// Like ActionFn but returns a Promise<void> for async operations.
/// @since 0.2.0
using AsyncActionFn = std::function<polycpp::Promise<void>(
    const std::vector<polycpp::JsonValue>& args,
    const polycpp::JsonValue& opts,
    Command& cmd)>;

/// @brief Type alias for async hook functions.
/// @since 0.2.0
using AsyncHookFn = std::function<polycpp::Promise<void>(Command& thisCommand, Command& actionCommand)>;

/**
 * @brief The main Command class — CLI command with options, arguments, and subcommands.
 *
 * Extends polycpp::EventEmitter. Mirrors npm commander's Command class.
 * Commands own their subcommands via unique_ptr (non-copyable due to EventEmitter).
 *
 * @par Example
 * @code{.cpp}
 *   Command program;
 *   program.name("my-app")
 *          .version("1.0.0")
 *          .description("My CLI application");
 *
 *   program.option("-v, --verbose", "enable verbose output");
 *
 *   program.command("serve")
 *          .description("start the server")
 *          .option("-p, --port <number>", "port to listen on")
 *          .action([](auto& args, auto& opts, auto& cmd) {
 *              // handle serve command
 *          });
 *
 *   program.parse({"node", "app", "serve", "--port", "3000"});
 * @endcode
 *
 * @see https://github.com/tj/commander.js
 * @since 0.1.0
 */
class Command : public polycpp::events::EventEmitter {
public:
    // --- Constructors ---

    /**
     * @brief Construct a Command with an optional name.
     * @param name The command name (empty for root program).
     * @par Example
     * @code{.cpp}
     *   Command program;
     *   Command sub("serve");
     * @endcode
     * @see https://github.com/tj/commander.js
     * @since 0.1.0
     */
    explicit Command(const std::string& name = "");

    /// @brief Virtual destructor.
    /// @since 0.1.0
    virtual ~Command() = default;

    // Non-copyable (inherited from EventEmitter), movable.

    // --- Metadata getters/setters (dual getter/setter pattern) ---

    /**
     * @brief Get or set the command name.
     * @param str If provided, sets the name and returns *this.
     * @return The command name when called without arguments.
     * @par Example
     * @code{.cpp}
     *   cmd.name("serve");
     *   std::string n = cmd.name();
     * @endcode
     * @see https://github.com/tj/commander.js
     * @since 0.1.0
     */
    std::string name() const;

    /**
     * @brief Set the command name.
     * @param str The new command name.
     * @return Reference to this command for chaining.
     * @see https://github.com/tj/commander.js
     * @since 0.1.0
     */
    Command& name(const std::string& str);

    /**
     * @brief Get the command description.
     * @return The description string.
     * @see https://github.com/tj/commander.js
     * @since 0.1.0
     */
    std::string description() const;

    /**
     * @brief Set the command description.
     * @param str The description text.
     * @return Reference to this command for chaining.
     * @see https://github.com/tj/commander.js
     * @since 0.1.0
     */
    Command& description(const std::string& str);

    /**
     * @brief Get the command summary.
     * @return The summary string.
     * @see https://github.com/tj/commander.js
     * @since 0.1.0
     */
    std::string summary() const;

    /**
     * @brief Set the command summary (shown in parent's subcommand list).
     * @param str The summary text.
     * @return Reference to this command for chaining.
     * @see https://github.com/tj/commander.js
     * @since 0.1.0
     */
    Command& summary(const std::string& str);

    /**
     * @brief Get the command usage string.
     *
     * If no custom usage is set, generates one from options, arguments, subcommands.
     *
     * @return The usage string.
     * @see https://github.com/tj/commander.js
     * @since 0.1.0
     */
    std::string usage() const;

    /**
     * @brief Set a custom usage string.
     * @param str The custom usage text.
     * @return Reference to this command for chaining.
     * @see https://github.com/tj/commander.js
     * @since 0.1.0
     */
    Command& usage(const std::string& str);

    /**
     * @brief Get the program version.
     * @return The version string, or empty if not set.
     * @see https://github.com/tj/commander.js
     * @since 0.1.0
     */
    std::string version() const;

    /**
     * @brief Set the program version and register -V, --version option.
     * @param str The version string.
     * @param flags Optional custom flags (default: "-V, --version").
     * @param desc Optional custom description (default: "output the version number").
     * @return Reference to this command for chaining.
     * @par Example
     * @code{.cpp}
     *   program.version("1.0.0");
     *   program.version("1.0.0", "-v, --vers", "show version");
     * @endcode
     * @see https://github.com/tj/commander.js
     * @since 0.1.0
     */
    Command& version(const std::string& str,
                     const std::string& flags = "-V, --version",
                     const std::string& desc = "output the version number");

    // --- Alias ---

    /**
     * @brief Get the first alias.
     * @return The first alias, or empty string if none.
     * @see https://github.com/tj/commander.js
     * @since 0.1.0
     */
    std::string alias() const;

    /**
     * @brief Set an alias for the command.
     * @param alias The alias name.
     * @return Reference to this command for chaining.
     * @see https://github.com/tj/commander.js
     * @since 0.1.0
     */
    Command& alias(const std::string& alias);

    /**
     * @brief Get all aliases.
     * @return Vector of alias strings.
     * @see https://github.com/tj/commander.js
     * @since 0.1.0
     */
    std::vector<std::string> aliases() const;

    /**
     * @brief Set multiple aliases.
     * @param aliases Vector of alias names.
     * @return Reference to this command for chaining.
     * @see https://github.com/tj/commander.js
     * @since 0.1.0
     */
    Command& aliases(const std::vector<std::string>& aliases);

    /**
     * @brief Set the name from a filename (strips path and extension).
     * @param filename The filename to derive the name from.
     * @return Reference to this command for chaining.
     * @see https://github.com/tj/commander.js
     * @since 0.1.0
     */
    Command& nameFromFilename(const std::string& filename);

    // --- Options ---

    /**
     * @brief Add an option with flags, description, and optional custom processing.
     * @param flags Flag specification (e.g., "-v, --verbose").
     * @param description Human-readable description.
     * @return Reference to this command for chaining.
     * @par Example
     * @code{.cpp}
     *   program.option("-v, --verbose", "enable verbose output");
     * @endcode
     * @see https://github.com/tj/commander.js#options
     * @since 0.1.0
     */
    Command& option(const std::string& flags, const std::string& description = "");

    /**
     * @brief Add an option with flags, description, and default value.
     * @param flags Flag specification.
     * @param description Human-readable description.
     * @param defaultValue Default value for the option.
     * @return Reference to this command for chaining.
     * @see https://github.com/tj/commander.js#options
     * @since 0.1.0
     */
    Command& option(const std::string& flags, const std::string& description,
                    const polycpp::JsonValue& defaultValue);

    /**
     * @brief Add an option with flags, description, custom parser, and default value.
     * @param flags Flag specification.
     * @param description Human-readable description.
     * @param parseArg Custom processing function.
     * @param defaultValue Default value for the option.
     * @return Reference to this command for chaining.
     * @see https://github.com/tj/commander.js#custom-option-processing
     * @since 0.1.0
     */
    Command& option(const std::string& flags, const std::string& description,
                    ParseFn parseArg, const polycpp::JsonValue& defaultValue = {});

    /**
     * @brief Add a required option (must be specified on command line).
     * @param flags Flag specification.
     * @param description Human-readable description.
     * @return Reference to this command for chaining.
     * @see https://github.com/tj/commander.js#options
     * @since 0.1.0
     */
    Command& requiredOption(const std::string& flags, const std::string& description = "");

    /**
     * @brief Add a required option with default value.
     * @param flags Flag specification.
     * @param description Human-readable description.
     * @param defaultValue Default value.
     * @return Reference to this command for chaining.
     * @see https://github.com/tj/commander.js#options
     * @since 0.1.0
     */
    Command& requiredOption(const std::string& flags, const std::string& description,
                            const polycpp::JsonValue& defaultValue);

    /**
     * @brief Add a required option with custom parser and default value.
     * @param flags Flag specification.
     * @param description Human-readable description.
     * @param parseArg Custom processing function.
     * @param defaultValue Default value.
     * @return Reference to this command for chaining.
     * @see https://github.com/tj/commander.js#options
     * @since 0.1.0
     */
    Command& requiredOption(const std::string& flags, const std::string& description,
                            ParseFn parseArg, const polycpp::JsonValue& defaultValue = {});

    /**
     * @brief Add a prepared Option object.
     *
     * This is the core method — option() and requiredOption() delegate to it.
     * Registers event listeners for CLI and env parsing.
     *
     * @param option The Option to add.
     * @return Reference to this command for chaining.
     * @see https://github.com/tj/commander.js#options
     * @since 0.1.0
     */
    Command& addOption(Option option);

    /**
     * @brief Factory routine to create a new unattached Option.
     * @param flags Flag specification.
     * @param description Human-readable description.
     * @return New Option object.
     * @see https://github.com/tj/commander.js
     * @since 0.1.0
     */
    virtual Option createOption(const std::string& flags, const std::string& description = "") const;

    // --- Arguments ---

    /**
     * @brief Define a command argument.
     * @param name Argument name with optional brackets.
     * @param description Human-readable description.
     * @return Reference to this command for chaining.
     * @par Example
     * @code{.cpp}
     *   program.argument("<file>", "input file to process");
     * @endcode
     * @see https://github.com/tj/commander.js#more-configuration-2
     * @since 0.1.0
     */
    Command& argument(const std::string& name, const std::string& description = "");

    /**
     * @brief Define multiple command arguments from a space-separated string.
     * @param names Space-separated argument names.
     * @return Reference to this command for chaining.
     * @par Example
     * @code{.cpp}
     *   program.arguments("<source> [destination]");
     * @endcode
     * @see https://github.com/tj/commander.js
     * @since 0.1.0
     */
    Command& arguments(const std::string& names);

    /**
     * @brief Add a prepared Argument object.
     * @param argument The Argument to add.
     * @return Reference to this command for chaining.
     * @see https://github.com/tj/commander.js
     * @since 0.1.0
     */
    Command& addArgument(Argument argument);

    /**
     * @brief Factory routine to create a new unattached Argument.
     * @param name Argument name.
     * @param description Human-readable description.
     * @return New Argument object.
     * @see https://github.com/tj/commander.js
     * @since 0.1.0
     */
    virtual Argument createArgument(const std::string& name, const std::string& description = "") const;

    // --- Subcommands ---

    /**
     * @brief Define a subcommand.
     *
     * Parses "name <required> [optional]" to extract name and arguments.
     * Returns a reference to the NEW subcommand (not *this).
     *
     * @param nameAndArgs Command name and argument definitions.
     * @param opts Command options (isDefault, hidden).
     * @return Reference to the new subcommand for chaining.
     * @par Example
     * @code{.cpp}
     *   program.command("clone <source> [destination]")
     *          .description("clone a repository")
     *          .action([](auto&, auto&, auto&) {});
     * @endcode
     * @see https://github.com/tj/commander.js#commands
     * @since 0.1.0
     */
    Command& command(const std::string& nameAndArgs, const CommandOptions& opts = {});

    /**
     * @brief Define an executable subcommand that spawns `<program>-<subcommand>`.
     *
     * Instead of calling an action handler, the parent program spawns an external
     * process named `<program>-<subcommand>` (or a custom executable name).
     * Returns `*this` (not the new subcommand) for chaining.
     *
     * @param nameAndArgs Command name and optional argument definitions.
     * @param description Human-readable description of the subcommand.
     * @param executableFile Optional custom executable name or path. If empty,
     *        defaults to `<parentName>-<subcommandName>`.
     * @return Reference to this (parent) command for chaining.
     * @par Example
     * @code{.cpp}
     *   program.executableCommand("install", "install packages");
     *   program.executableCommand("build", "build the project", "my-builder");
     * @endcode
     * @see https://github.com/tj/commander.js#commands
     * @since 0.1.0
     */
    Command& executableCommand(const std::string& nameAndArgs,
                               const std::string& description,
                               const std::string& executableFile = "");

    /**
     * @brief Set the directory to search for executable subcommands.
     * @param path The directory path.
     * @return Reference to this command for chaining.
     * @see https://github.com/tj/commander.js
     * @since 0.1.0
     */
    Command& executableDir(const std::string& path);

    /**
     * @brief Get the directory to search for executable subcommands.
     * @return The executable directory path, or empty string if not set.
     * @see https://github.com/tj/commander.js
     * @since 0.1.0
     */
    std::string executableDir() const;

    /**
     * @brief Add a prepared subcommand.
     *
     * Unlike command(), this does NOT return the new subcommand —
     * it returns *this for chaining.
     *
     * @param cmd The subcommand to add (moved via unique_ptr).
     * @param opts Command options.
     * @return Reference to this command for chaining.
     * @see https://github.com/tj/commander.js
     * @since 0.1.0
     */
    Command& addCommand(std::unique_ptr<Command> cmd, const CommandOptions& opts = {});

    /**
     * @brief Factory routine to create a new unattached Command.
     * @param name Command name.
     * @return New Command as unique_ptr.
     * @see https://github.com/tj/commander.js
     * @since 0.1.0
     */
    virtual std::unique_ptr<Command> createCommand(const std::string& name = "") const;

    // --- Action ---

    /**
     * @brief Register the action handler for this command.
     *
     * The handler receives (processedArgs, opts, command).
     *
     * @param fn The action function.
     * @return Reference to this command for chaining.
     * @par Example
     * @code{.cpp}
     *   program.action([](auto& args, auto& opts, auto& cmd) {
     *       // handle the command
     *   });
     * @endcode
     * @see https://github.com/tj/commander.js#action-handler-subcommands
     * @since 0.1.0
     */
    Command& action(ActionFn fn);

    /**
     * @brief Register an async action handler for this command.
     *
     * The handler receives (processedArgs, opts, command) and returns
     * a Promise<void> that resolves when the async work completes.
     *
     * @param fn The async action function.
     * @return Reference to this command for chaining.
     * @par Example
     * @code{.cpp}
     *   program.actionAsync([](auto& args, auto& opts, auto& cmd) {
     *       return polycpp::Promise<void>::resolve();
     *   });
     * @endcode
     * @see https://github.com/tj/commander.js#action-handler-subcommands
     * @since 0.2.0
     */
    Command& actionAsync(AsyncActionFn fn);

    // --- Parsing ---

    /**
     * @brief Parse argv, setting options and invoking commands.
     *
     * Call with no arguments to parse process::argv().
     * Or pass a vector of strings with parseOptions specifying the format.
     *
     * @param argv The argument vector (default: process::argv()).
     * @param parseOpts Parsing options (default: from="node").
     * @return Reference to this command for chaining.
     * @par Example
     * @code{.cpp}
     *   program.parse({"node", "app", "--verbose", "file.txt"});
     *   program.parse({"--verbose", "file.txt"}, {.from = "user"});
     * @endcode
     * @see https://github.com/tj/commander.js#parse-and-parseasync
     * @since 0.1.0
     */
    Command& parse(const std::vector<std::string>& argv, const ParseOptions& parseOpts = {});

    /**
     * @brief Parse argv from process::argv() with default options.
     * @return Reference to this command for chaining.
     * @see https://github.com/tj/commander.js#parse-and-parseasync
     * @since 0.1.0
     */
    Command& parse();

    /**
     * @brief Parse arguments asynchronously.
     *
     * Like parse(), but returns a Promise that resolves after all async
     * action handlers and hooks complete. Supports both sync and async
     * handlers — sync handlers are wrapped in immediately-resolving promises.
     *
     * @param argv The argument vector.
     * @param parseOpts Parsing options (default: from="node").
     * @return Promise resolving to a reference to this command.
     * @par Example
     * @code{.cpp}
     *   auto promise = program.parseAsync({"app", "--verbose"}, {.from = "user"});
     *   polycpp::EventLoop::instance().run();
     * @endcode
     * @see https://github.com/tj/commander.js#parse-and-parseasync
     * @since 0.2.0
     */
    polycpp::Promise<std::reference_wrapper<Command>> parseAsync(
        const std::vector<std::string>& argv,
        const ParseOptions& parseOpts = {});

    /**
     * @brief Parse options from argv, removing known options.
     *
     * Returns argv split into operands and unknown arguments.
     * This is the core state machine that handles --, -abc, --flag=value, --no-*, etc.
     *
     * @param args The arguments to parse.
     * @return ParseOptionsResult with operands and unknown vectors.
     * @see https://github.com/tj/commander.js
     * @since 0.1.0
     */
    ParseOptionsResult parseOptions(const std::vector<std::string>& args);

    // --- Option values ---

    /**
     * @brief Get local option values as key-value pairs.
     * @return Map of option attributeName -> value.
     * @par Example
     * @code{.cpp}
     *   auto opts = program.opts();
     *   bool verbose = opts["verbose"].asBool();
     * @endcode
     * @see https://github.com/tj/commander.js
     * @since 0.1.0
     */
    OptionValues opts() const;

    /**
     * @brief Get merged local and global option values.
     *
     * Global (ancestor) values overwrite local values.
     *
     * @return Merged option values map.
     * @see https://github.com/tj/commander.js
     * @since 0.1.0
     */
    OptionValues optsWithGlobals() const;

    /**
     * @brief Retrieve a single option value.
     * @param key The option attribute name (camelCase).
     * @return The option value, or null JsonValue if not set.
     * @see https://github.com/tj/commander.js
     * @since 0.1.0
     */
    polycpp::JsonValue getOptionValue(const std::string& key) const;

    /**
     * @brief Store an option value.
     * @param key The option attribute name.
     * @param value The value to store.
     * @return Reference to this command for chaining.
     * @see https://github.com/tj/commander.js
     * @since 0.1.0
     */
    Command& setOptionValue(const std::string& key, const polycpp::JsonValue& value);

    /**
     * @brief Store option value with source tracking.
     * @param key The option attribute name.
     * @param value The value to store.
     * @param source Where the value came from (default/cli/env/implied).
     * @return Reference to this command for chaining.
     * @see https://github.com/tj/commander.js
     * @since 0.1.0
     */
    Command& setOptionValueWithSource(const std::string& key, const polycpp::JsonValue& value,
                                       const std::string& source);

    /**
     * @brief Get the source of an option value.
     * @param key The option attribute name.
     * @return Source string, or empty if not set.
     * @see https://github.com/tj/commander.js
     * @since 0.1.0
     */
    std::string getOptionValueSource(const std::string& key) const;

    /**
     * @brief Get the source of an option value including globals.
     * @param key The option attribute name.
     * @return Source string from this command or nearest ancestor.
     * @see https://github.com/tj/commander.js
     * @since 0.1.0
     */
    std::string getOptionValueSourceWithGlobals(const std::string& key) const;

    // --- Help configuration ---

    /**
     * @brief Customize the built-in help option.
     *
     * Pass false to disable. Pass flags/description to customize.
     *
     * @param flags Custom flags string, or false-like empty string to disable.
     * @param description Custom description.
     * @return Reference to this command for chaining.
     * @see https://github.com/tj/commander.js#custom-help
     * @since 0.1.0
     */
    Command& helpOption(const std::string& flags, const std::string& description = "");

    /**
     * @brief Disable the built-in help option.
     * @param enable false to disable, true to re-enable.
     * @return Reference to this command for chaining.
     * @see https://github.com/tj/commander.js#custom-help
     * @since 0.1.0
     */
    Command& helpOption(bool enable);

    /**
     * @brief Customize or override the default help subcommand.
     *
     * By default a help subcommand is automatically added if your command has
     * subcommands and no root action handler. Use this to set a custom name
     * and/or description.
     *
     * @param nameAndArgs Custom name and arguments (e.g. "assist [cmd]").
     * @param description Custom description (default: "display help for command").
     * @return Reference to this command for chaining.
     * @par Example
     * @code{.cpp}
     *   program.helpCommand("assist [cmd]", "show help");
     * @endcode
     * @see https://github.com/tj/commander.js#custom-help
     * @since 0.1.0
     */
    Command& helpCommand(const std::string& nameAndArgs, const std::string& description = "");

    /**
     * @brief Enable or disable the implicit help subcommand.
     *
     * Pass false to suppress the automatic help subcommand.
     * Pass true to force-add it even when there are no subcommands.
     *
     * @param enable Whether to enable or disable the help subcommand.
     * @return Reference to this command for chaining.
     * @par Example
     * @code{.cpp}
     *   program.helpCommand(false); // suppress help subcommand
     *   program.helpCommand(true);  // force-add even without subcommands
     * @endcode
     * @see https://github.com/tj/commander.js#custom-help
     * @since 0.1.0
     */
    Command& helpCommand(bool enable);

    /**
     * @brief Add a prepared custom help command.
     *
     * The command will be used as the help subcommand, replacing the default.
     *
     * @param cmd The custom help command (moved via unique_ptr).
     * @return Reference to this command for chaining.
     * @see https://github.com/tj/commander.js#custom-help
     * @since 0.1.0
     */
    Command& addHelpCommand(std::unique_ptr<Command> cmd);

    /**
     * @brief Output help information for this command.
     * @param context Help context (error: true for stderr).
     * @see https://github.com/tj/commander.js#custom-help
     * @since 0.1.0
     */
    void outputHelp(const HelpContext& context = {});

    /**
     * @brief Return help text as a string.
     * @param context Help context.
     * @return The formatted help text.
     * @see https://github.com/tj/commander.js#custom-help
     * @since 0.1.0
     */
    std::string helpInformation(const HelpContext& context = {}) const;

    /**
     * @brief Output help and exit.
     * @param context Help context (error: true for stderr, exit code 1).
     * @see https://github.com/tj/commander.js#custom-help
     * @since 0.1.0
     */
    void help(const HelpContext& context = {});

    /**
     * @brief Add additional text to the built-in help.
     * @param position "before", "after", "beforeAll", or "afterAll".
     * @param text Text to add, or function returning text.
     * @return Reference to this command for chaining.
     * @see https://github.com/tj/commander.js#custom-help
     * @since 0.1.0
     */
    Command& addHelpText(const std::string& position, const std::string& text);

    /**
     * @brief Configure help formatting options.
     *
     * Accepted keys: helpWidth, sortSubcommands, sortOptions, showGlobalOptions.
     *
     * @param config Configuration map.
     * @return Reference to this command for chaining.
     * @see https://github.com/tj/commander.js#custom-help
     * @since 0.1.0
     */
    Command& configureHelp(const std::map<std::string, polycpp::JsonValue>& config);

    /**
     * @brief Get current help configuration.
     * @return Help configuration map.
     * @see https://github.com/tj/commander.js#custom-help
     * @since 0.1.0
     */
    std::map<std::string, polycpp::JsonValue> configureHelp() const;

    /**
     * @brief Configure output functions.
     * @param config Output configuration.
     * @return Reference to this command for chaining.
     * @see https://github.com/tj/commander.js#custom-help
     * @since 0.1.0
     */
    Command& configureOutput(const OutputConfiguration& config);

    /**
     * @brief Get current output configuration.
     * @return Output configuration.
     * @see https://github.com/tj/commander.js#custom-help
     * @since 0.1.0
     */
    OutputConfiguration configureOutput() const;

    /**
     * @brief Show help or custom message after an error.
     * @param displayHelp true to show help, or a custom message string.
     * @return Reference to this command for chaining.
     * @see https://github.com/tj/commander.js
     * @since 0.1.0
     */
    Command& showHelpAfterError(bool displayHelp = true);

    /**
     * @brief Show help or custom message after an error.
     * @param message Custom message to display after error.
     * @return Reference to this command for chaining.
     * @see https://github.com/tj/commander.js
     * @since 0.1.0
     */
    Command& showHelpAfterError(const std::string& message);

    /**
     * @brief Enable/disable suggestion of similar commands after error.
     * @param displaySuggestion Whether to show suggestions.
     * @return Reference to this command for chaining.
     * @see https://github.com/tj/commander.js
     * @since 0.1.0
     */
    Command& showSuggestionAfterError(bool displaySuggestion = true);

    // --- Error handling ---

    /**
     * @brief Display error message and exit (or throw with exitOverride).
     * @param message The error message.
     * @param opts Error options (exitCode, code).
     * @see https://github.com/tj/commander.js
     * @since 0.1.0
     */
    void error(const std::string& message, const ErrorOptions& opts = {});

    /**
     * @brief Register callback to use instead of process::exit().
     *
     * With no arguments, throws CommanderError instead of exiting.
     *
     * @param fn Optional callback receiving CommanderError.
     * @return Reference to this command for chaining.
     * @par Example
     * @code{.cpp}
     *   program.exitOverride(); // throws instead of exiting
     * @endcode
     * @see https://github.com/tj/commander.js#override-exit-handling
     * @since 0.1.0
     */
    Command& exitOverride(std::function<void(const CommanderError&)> fn = {});

    // --- Hooks ---

    /**
     * @brief Add a hook for a lifecycle event.
     * @param event "preSubcommand", "preAction", or "postAction".
     * @param listener Hook function.
     * @return Reference to this command for chaining.
     * @see https://github.com/tj/commander.js#life-cycle-hooks
     * @since 0.1.0
     */
    Command& hook(const std::string& event, HookFn listener);

    /**
     * @brief Add an async hook for a lifecycle event.
     * @param event "preSubcommand", "preAction", or "postAction".
     * @param listener Async hook function returning Promise<void>.
     * @return Reference to this command for chaining.
     * @see https://github.com/tj/commander.js#life-cycle-hooks
     * @since 0.2.0
     */
    Command& hookAsync(const std::string& event, AsyncHookFn listener);

    // --- Settings ---

    /**
     * @brief Copy inherited settings from a parent command.
     * @param sourceCommand The source command to copy settings from.
     * @return Reference to this command for chaining.
     * @see https://github.com/tj/commander.js
     * @since 0.1.0
     */
    Command& copyInheritedSettings(const Command& sourceCommand);

    /**
     * @brief Allow unknown options on the command line.
     * @param allowUnknown Whether to allow unknown options.
     * @return Reference to this command for chaining.
     * @see https://github.com/tj/commander.js
     * @since 0.1.0
     */
    Command& allowUnknownOption(bool allowUnknown = true);

    /**
     * @brief Allow excess command-arguments.
     * @param allowExcess Whether to allow excess arguments.
     * @return Reference to this command for chaining.
     * @see https://github.com/tj/commander.js
     * @since 0.1.0
     */
    Command& allowExcessArguments(bool allowExcess = true);

    /**
     * @brief Enable positional options.
     * @param positional Whether to enable positional options.
     * @return Reference to this command for chaining.
     * @see https://github.com/tj/commander.js
     * @since 0.1.0
     */
    Command& enablePositionalOptions(bool positional = true);

    /**
     * @brief Pass through options after command-arguments.
     * @param passThrough Whether to pass through options.
     * @return Reference to this command for chaining.
     * @see https://github.com/tj/commander.js
     * @since 0.1.0
     */
    Command& passThroughOptions(bool passThrough = true);

    /**
     * @brief Alter parsing of short flags with optional values.
     * @param combine Whether to combine flag and optional value (default: true).
     * @return Reference to this command for chaining.
     * @see https://github.com/tj/commander.js
     * @since 0.1.0
     */
    Command& combineFlagAndOptionalValue(bool combine = true);

    /**
     * @brief Create a Help instance with current help configuration applied.
     * @return Help object with configuration applied.
     * @see https://github.com/tj/commander.js#custom-help
     * @since 0.1.0
     */
    virtual Help createHelp() const;

    // --- Public fields (matching JS) ---

    /// @brief Subcommands (owned).
    /// @since 0.1.0
    std::vector<std::unique_ptr<Command>> commands;

    /// @brief Registered options.
    /// @since 0.1.0
    std::vector<Option> options;

    /// @brief Parent command (non-owning).
    /// @since 0.1.0
    Command* parent = nullptr;

    /// @brief Registered arguments.
    /// @since 0.1.0
    std::vector<Argument> registeredArguments;

    /// @brief CLI args with options removed (raw strings after parse).
    /// @since 0.1.0
    std::vector<std::string> args;

    /// @brief Raw args as passed to parse().
    /// @since 0.1.0
    std::vector<std::string> rawArgs;

    /// @brief Processed args (after custom processing, collecting variadics).
    /// @since 0.1.0
    std::vector<polycpp::JsonValue> processedArgs;

    // --- Error reporting methods (public for test access) ---

    /**
     * @brief Report a missing required argument.
     * @param name The argument name.
     * @since 0.1.0
     */
    void missingArgument(const std::string& name);

    /**
     * @brief Report a missing option argument.
     * @param option The option missing its argument.
     * @since 0.1.0
     */
    void optionMissingArgument(const Option& option);

    /**
     * @brief Report a missing mandatory option value.
     * @param option The mandatory option.
     * @since 0.1.0
     */
    void missingMandatoryOptionValue(const Option& option);

    /**
     * @brief Report an unknown option.
     * @param flag The unknown flag.
     * @since 0.1.0
     */
    void unknownOption(const std::string& flag);

    /**
     * @brief Report an unknown command.
     * @since 0.1.0
     */
    void unknownCommand();

private:
    // --- Private helper methods ---

    void exit_(int exitCode, const std::string& code, const std::string& message);
    std::vector<std::string> prepareUserArgs_(const std::vector<std::string>& argv,
                                              const ParseOptions& parseOpts);
    void parseCommand_(const std::vector<std::string>& operands,
                       const std::vector<std::string>& unknown);
    void parseOptionsEnv_();
    void parseOptionsImplied_();
    void checkNumberOfArguments_();
    void processArguments_();
    void checkForMissingMandatoryOptions_();
    void checkForConflictingOptions_();
    void checkForConflictingLocalOptions_();
    void checkForBrokenPassThrough_();
    void excessArguments_(const std::vector<std::string>& receivedArgs);
    void conflictingOption_(const Option& option, const Option& conflictingOption);
    void outputHelpIfRequested_(const std::vector<std::string>& args);
    void dispatchSubcommand_(const std::string& commandName,
                             const std::vector<std::string>& operands,
                             const std::vector<std::string>& unknown);
    void executeSubCommand_(Command& subCommand,
                            const std::vector<std::string>& operands,
                            const std::vector<std::string>& unknown);
    std::string findProgram_(const std::string& name) const;
    void dispatchHelpCommand_(const std::string& subcommandName);
    void callHooks_(const std::string& event);
    void callSubCommandHook_(Command& subCommand, const std::string& event);
    polycpp::JsonValue callParseArg_(const Option& target, const std::string& value,
                           const polycpp::JsonValue& previous, const std::string& invalidArgMsg);
    polycpp::JsonValue callParseArg_(const Argument& target, const std::string& value,
                           const polycpp::JsonValue& previous, const std::string& invalidArgMsg);
    void registerOption_(Option& option);
    void registerCommand_(Command& command);
    Command* findCommand_(const std::string& name);
    const Option* findOption_(const std::string& arg) const;
    const Option* getHelpOption_();
    Command* getHelpCommand_();
    std::vector<const Command*> getCommandAndAncestors_() const;

    void prepareForParse_();
    void saveStateBeforeParse_();
    void restoreStateBeforeParse_();

    polycpp::Promise<void> parseCommandAsync_(const std::vector<std::string>& operands,
                                               const std::vector<std::string>& unknown);
    polycpp::Promise<void> dispatchSubcommandAsync_(const std::string& commandName,
                                                     const std::vector<std::string>& operands,
                                                     const std::vector<std::string>& unknown);
    polycpp::Promise<void> callHooksAsync_(const std::string& event);

    // --- Private fields ---

    std::string name_;
    std::string description_;
    std::string summary_;
    std::string usage_;
    std::string version_;
    std::string versionOptionName_;
    std::vector<std::string> aliases_;
    std::string scriptPath_;

    bool executableHandler_ = false;
    std::optional<std::string> executableFile_;
    std::optional<std::string> executableDir_;

    bool allowUnknownOption_ = false;
    bool allowExcessArguments_ = false;
    bool enablePositionalOptions_ = false;
    bool passThroughOptions_ = false;
    bool combineFlagAndOptionalValue_ = true;
    bool hidden_ = false;
    bool showSuggestionAfterError_ = true;

    /// @brief false = no help after error, true = show help, string = custom message.
    std::variant<bool, std::string> showHelpAfterError_ = false;

    OptionValues optionValues_;
    OptionValueSources optionValueSources_;

    /// Internal action handler takes just processedArgs; action() wraps the user's ActionFn.
    std::function<void(const std::vector<polycpp::JsonValue>&)> actionHandler_;
    /// Internal async action handler; set by actionAsync().
    std::function<polycpp::Promise<void>(const std::vector<polycpp::JsonValue>&)> asyncActionHandler_;
    std::function<void(const CommanderError&)> exitCallback_;

    std::unordered_map<std::string, std::vector<HookFn>> lifeCycleHooks_;
    std::unordered_map<std::string, std::vector<AsyncHookFn>> asyncLifeCycleHooks_;

    OutputConfiguration outputConfiguration_;

    /// @brief nullptr = help disabled; nullopt = lazy create; has_value = configured.
    std::optional<std::unique_ptr<Option>> helpOption_;

    /// @brief nullptr = no help cmd; nullopt = lazy create; has_value = configured.
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

    // Give Help full access to read Command internals.
    friend class Help;
};

} // namespace commander
} // namespace polycpp
