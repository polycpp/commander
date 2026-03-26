#pragma once

/**
 * @file help.hpp
 * @brief Help class for formatting command-line help output.
 * @see https://github.com/tj/commander.js#custom-help
 * @since 0.1.0
 */

#include <polycpp/commander/argument.hpp>
#include <polycpp/commander/option.hpp>

#include <optional>
#include <string>
#include <vector>

namespace polycpp {
namespace commander {

// Forward declare Command — full definition in command.hpp.
class Command;

/**
 * @brief Options for Help::prepareContext().
 * @see https://github.com/tj/commander.js#custom-help
 * @since 0.1.0
 */
struct PrepareContextOptions {
    int helpWidth = 80;             ///< Help text display width.
    bool outputHasColors = false;   ///< Whether the output supports ANSI colors.
};

/**
 * @brief Help formatting class for generating CLI help text.
 *
 * Mirrors npm commander's Help class. All methods are virtual to allow
 * customization through subclassing. Methods that need command information
 * take a `const Command&` parameter.
 *
 * @par Example
 * @code{.cpp}
 *   Help help;
 *   help.helpWidth = 80;
 *   help.sortOptions = true;
 *   std::string text = help.formatHelp(cmd, help);
 * @endcode
 *
 * @see https://github.com/tj/commander.js#custom-help
 * @since 0.1.0
 */
class Help {
public:
    /**
     * @brief Construct a Help with default configuration.
     * @since 0.1.0
     */
    Help() = default;

    /**
     * @brief Virtual destructor for subclassing.
     * @since 0.1.0
     */
    virtual ~Help() = default;

    // --- Configuration properties ---

    /// @brief Display width for wrapping help text. 0 means use default (80).
    /// @since 0.1.0
    int helpWidth = 0;

    /// @brief Minimum width before wrapping is applied.
    /// @since 0.1.0
    int minWidthToWrap = 40;

    /// @brief Whether to sort subcommands alphabetically.
    /// @since 0.1.0
    bool sortSubcommands = false;

    /// @brief Whether to sort options alphabetically.
    /// @since 0.1.0
    bool sortOptions = false;

    /// @brief Whether to show global options from parent commands.
    /// @since 0.1.0
    bool showGlobalOptions = false;

    /// @brief Whether the output supports ANSI color codes.
    /// @since 0.1.0
    bool outputHasColors = false;

    // --- Prepare context ---

    /**
     * @brief Prepare help context before formatting (legacy overload).
     *
     * Called by Command after applying overrides from configureHelp()
     * and just before calling formatHelp().
     *
     * @param contextHelpWidth Optional help width override.
     * @par Example
     * @code{.cpp}
     *   help.prepareContext(120);
     * @endcode
     * @see https://github.com/tj/commander.js#custom-help
     * @since 0.1.0
     */
    virtual void prepareContext(int contextHelpWidth = 80);

    /**
     * @brief Prepare help context with full options (width + color).
     *
     * Called by Command after applying overrides from configureHelp()
     * and just before calling formatHelp().
     *
     * @param options Context options including helpWidth and outputHasColors.
     * @par Example
     * @code{.cpp}
     *   help.prepareContext({.helpWidth = 120, .outputHasColors = true});
     * @endcode
     * @see https://github.com/tj/commander.js#custom-help
     * @since 0.1.0
     */
    virtual void prepareContext(const PrepareContextOptions& options);

    // --- Visible items ---

    /**
     * @brief Get an array of the visible subcommands.
     *
     * Includes a placeholder for the implicit help command, if there is one.
     *
     * @param cmd The command whose subcommands to inspect.
     * @return Vector of pointers to visible subcommands.
     * @par Example
     * @code{.cpp}
     *   auto cmds = help.visibleCommands(program);
     * @endcode
     * @see https://github.com/tj/commander.js#custom-help
     * @since 0.1.0
     */
    virtual std::vector<const Command*> visibleCommands(const Command& cmd) const;

    /**
     * @brief Get an array of the visible options.
     *
     * Includes a placeholder for the implicit help option, if there is one.
     *
     * @param cmd The command whose options to inspect.
     * @return Vector of visible options (copies, since help option may be synthesized).
     * @par Example
     * @code{.cpp}
     *   auto opts = help.visibleOptions(program);
     * @endcode
     * @see https://github.com/tj/commander.js#custom-help
     * @since 0.1.0
     */
    virtual std::vector<Option> visibleOptions(const Command& cmd) const;

    /**
     * @brief Get an array of the visible global options (from parent commands).
     *
     * Returns empty vector if showGlobalOptions is false.
     *
     * @param cmd The command whose ancestors to inspect.
     * @return Vector of visible global options.
     * @par Example
     * @code{.cpp}
     *   help.showGlobalOptions = true;
     *   auto globalOpts = help.visibleGlobalOptions(program);
     * @endcode
     * @see https://github.com/tj/commander.js#custom-help
     * @since 0.1.0
     */
    virtual std::vector<Option> visibleGlobalOptions(const Command& cmd) const;

    /**
     * @brief Get an array of visible arguments.
     *
     * Returns arguments if any have a description; otherwise returns empty.
     *
     * @param cmd The command whose arguments to inspect.
     * @return Vector of pointers to visible arguments.
     * @par Example
     * @code{.cpp}
     *   auto args = help.visibleArguments(program);
     * @endcode
     * @see https://github.com/tj/commander.js#custom-help
     * @since 0.1.0
     */
    virtual std::vector<const Argument*> visibleArguments(const Command& cmd) const;

    // --- Term methods ---

    /**
     * @brief Get the command term to show in the list of subcommands.
     * @param cmd The subcommand.
     * @return Formatted term string.
     * @par Example
     * @code{.cpp}
     *   auto term = help.subcommandTerm(sub);
     * @endcode
     * @see https://github.com/tj/commander.js#custom-help
     * @since 0.1.0
     */
    virtual std::string subcommandTerm(const Command& cmd) const;

    /**
     * @brief Get the subcommand description for help listing.
     *
     * Falls back to description if no summary is set.
     *
     * @param cmd The subcommand.
     * @return Description string.
     * @par Example
     * @code{.cpp}
     *   auto desc = help.subcommandDescription(sub);
     * @endcode
     * @see https://github.com/tj/commander.js#custom-help
     * @since 0.1.0
     */
    virtual std::string subcommandDescription(const Command& cmd) const;

    /**
     * @brief Get the option term to show in the list of options.
     * @param option The option.
     * @return The flags string.
     * @par Example
     * @code{.cpp}
     *   auto term = help.optionTerm(opt);
     * @endcode
     * @see https://github.com/tj/commander.js#custom-help
     * @since 0.1.0
     */
    virtual std::string optionTerm(const Option& option) const;

    /**
     * @brief Get the option description with extra info (choices, default, env).
     * @param option The option.
     * @return Description string with appended metadata.
     * @par Example
     * @code{.cpp}
     *   auto desc = help.optionDescription(opt);
     * @endcode
     * @see https://github.com/tj/commander.js#custom-help
     * @since 0.1.0
     */
    virtual std::string optionDescription(const Option& option) const;

    /**
     * @brief Get the argument term to show in the list of arguments.
     * @param argument The argument.
     * @return The argument name.
     * @par Example
     * @code{.cpp}
     *   auto term = help.argumentTerm(arg);
     * @endcode
     * @see https://github.com/tj/commander.js#custom-help
     * @since 0.1.0
     */
    virtual std::string argumentTerm(const Argument& argument) const;

    /**
     * @brief Get the argument description with extra info (choices, default).
     * @param argument The argument.
     * @return Description string with appended metadata.
     * @par Example
     * @code{.cpp}
     *   auto desc = help.argumentDescription(arg);
     * @endcode
     * @see https://github.com/tj/commander.js#custom-help
     * @since 0.1.0
     */
    virtual std::string argumentDescription(const Argument& argument) const;

    // --- Length calculations ---

    /**
     * @brief Get the longest subcommand term length.
     * @param cmd The parent command.
     * @return Maximum display width of subcommand terms.
     * @par Example
     * @code{.cpp}
     *   auto len = help.longestSubcommandTermLength(program);
     * @endcode
     * @see https://github.com/tj/commander.js#custom-help
     * @since 0.1.0
     */
    virtual int longestSubcommandTermLength(const Command& cmd) const;

    /**
     * @brief Get the longest option term length.
     * @param cmd The command.
     * @return Maximum display width of option terms.
     * @par Example
     * @code{.cpp}
     *   auto len = help.longestOptionTermLength(program);
     * @endcode
     * @see https://github.com/tj/commander.js#custom-help
     * @since 0.1.0
     */
    virtual int longestOptionTermLength(const Command& cmd) const;

    /**
     * @brief Get the longest global option term length.
     * @param cmd The command.
     * @return Maximum display width of global option terms.
     * @par Example
     * @code{.cpp}
     *   auto len = help.longestGlobalOptionTermLength(program);
     * @endcode
     * @see https://github.com/tj/commander.js#custom-help
     * @since 0.1.0
     */
    virtual int longestGlobalOptionTermLength(const Command& cmd) const;

    /**
     * @brief Get the longest argument term length.
     * @param cmd The command.
     * @return Maximum display width of argument terms.
     * @par Example
     * @code{.cpp}
     *   auto len = help.longestArgumentTermLength(program);
     * @endcode
     * @see https://github.com/tj/commander.js#custom-help
     * @since 0.1.0
     */
    virtual int longestArgumentTermLength(const Command& cmd) const;

    // --- Usage and description ---

    /**
     * @brief Get the command usage string for the top of help.
     * @param cmd The command.
     * @return Usage string including ancestors and arguments.
     * @par Example
     * @code{.cpp}
     *   auto usage = help.commandUsage(program);
     * @endcode
     * @see https://github.com/tj/commander.js#custom-help
     * @since 0.1.0
     */
    virtual std::string commandUsage(const Command& cmd) const;

    /**
     * @brief Get the description for the command.
     * @param cmd The command.
     * @return Description string.
     * @par Example
     * @code{.cpp}
     *   auto desc = help.commandDescription(program);
     * @endcode
     * @see https://github.com/tj/commander.js#custom-help
     * @since 0.1.0
     */
    virtual std::string commandDescription(const Command& cmd) const;

    // --- Style methods ---

    /**
     * @brief Style a section title (e.g., "Usage:", "Options:").
     *
     * Bold when colors are enabled.
     *
     * @param str The title string.
     * @return Styled string.
     * @see https://github.com/tj/commander.js#custom-help
     * @since 0.1.0
     */
    virtual std::string styleTitle(const std::string& str) const;

    /**
     * @brief Style the usage line content.
     *
     * Bold when colors are enabled.
     *
     * @param str The usage string.
     * @return Styled string.
     * @see https://github.com/tj/commander.js#custom-help
     * @since 0.1.0
     */
    virtual std::string styleUsage(const std::string& str) const;

    /**
     * @brief Style the command description text.
     *
     * Delegates to styleDescriptionText().
     *
     * @param str The description string.
     * @return Styled string.
     * @see https://github.com/tj/commander.js#custom-help
     * @since 0.1.0
     */
    virtual std::string styleCommandDescription(const std::string& str) const;

    /**
     * @brief Style an option term (e.g., "-v, --verbose").
     *
     * Yellow when colors are enabled.
     *
     * @param str The option term string.
     * @return Styled string.
     * @see https://github.com/tj/commander.js#custom-help
     * @since 0.1.0
     */
    virtual std::string styleOptionTerm(const std::string& str) const;

    /**
     * @brief Style an option description.
     *
     * Delegates to styleDescriptionText().
     *
     * @param str The description string.
     * @return Styled string.
     * @see https://github.com/tj/commander.js#custom-help
     * @since 0.1.0
     */
    virtual std::string styleOptionDescription(const std::string& str) const;

    /**
     * @brief Style a subcommand term (e.g., "serve [options]").
     *
     * Yellow when colors are enabled.
     *
     * @param str The subcommand term string.
     * @return Styled string.
     * @see https://github.com/tj/commander.js#custom-help
     * @since 0.1.0
     */
    virtual std::string styleSubcommandTerm(const std::string& str) const;

    /**
     * @brief Style a subcommand description.
     *
     * Delegates to styleDescriptionText().
     *
     * @param str The description string.
     * @return Styled string.
     * @see https://github.com/tj/commander.js#custom-help
     * @since 0.1.0
     */
    virtual std::string styleSubcommandDescription(const std::string& str) const;

    /**
     * @brief Style an argument term (e.g., "<file>").
     *
     * Yellow when colors are enabled.
     *
     * @param str The argument term string.
     * @return Styled string.
     * @see https://github.com/tj/commander.js#custom-help
     * @since 0.1.0
     */
    virtual std::string styleArgumentTerm(const std::string& str) const;

    /**
     * @brief Style an argument description.
     *
     * Delegates to styleDescriptionText().
     *
     * @param str The description string.
     * @return Styled string.
     * @see https://github.com/tj/commander.js#custom-help
     * @since 0.1.0
     */
    virtual std::string styleArgumentDescription(const std::string& str) const;

    /**
     * @brief Style description text.
     *
     * Dim when colors are enabled.
     *
     * @param str The description string.
     * @return Styled string.
     * @see https://github.com/tj/commander.js#custom-help
     * @since 0.1.0
     */
    virtual std::string styleDescriptionText(const std::string& str) const;

    // --- Formatting ---

    /**
     * @brief Calculate the pad width from the maximum term length.
     * @param cmd The command.
     * @return Maximum of all term lengths.
     * @par Example
     * @code{.cpp}
     *   auto width = help.padWidth(program);
     * @endcode
     * @see https://github.com/tj/commander.js#custom-help
     * @since 0.1.0
     */
    virtual int padWidth(const Command& cmd) const;

    /**
     * @brief Return display width of string, ignoring ANSI escape sequences.
     * @param str Input string.
     * @return Visual width of the string.
     * @par Example
     * @code{.cpp}
     *   auto w = help.displayWidth("hello");  // 5
     * @endcode
     * @see https://github.com/tj/commander.js#custom-help
     * @since 0.1.0
     */
    virtual int displayWidth(const std::string& str) const;

    /**
     * @brief Detect manually wrapped and indented strings.
     *
     * Checks for line break followed by whitespace.
     *
     * @param str Input string.
     * @return true if the string appears to be pre-formatted.
     * @par Example
     * @code{.cpp}
     *   bool pre = help.preformatted("line1\n  indented");  // true
     * @endcode
     * @see https://github.com/tj/commander.js#custom-help
     * @since 0.1.0
     */
    virtual bool preformatted(const std::string& str) const;

    /**
     * @brief Wrap a string at whitespace, preserving existing line breaks.
     *
     * Wrapping is skipped if the width is less than minWidthToWrap.
     *
     * @param str Input string.
     * @param width Maximum line width.
     * @return Wrapped string.
     * @par Example
     * @code{.cpp}
     *   auto wrapped = help.boxWrap("long text here", 20);
     * @endcode
     * @see https://github.com/tj/commander.js#custom-help
     * @since 0.1.0
     */
    virtual std::string boxWrap(const std::string& str, int width) const;

    /**
     * @brief Format a term-description pair with padding and wrapping.
     *
     * Pads the term to termWidth and wraps the description to fit within
     * helpWidth, indenting continuation lines.
     *
     * @param term The term (option flags, command name, etc.).
     * @param termWidth The target width for the term column.
     * @param description The description text.
     * @return Formatted string.
     * @par Example
     * @code{.cpp}
     *   auto item = help.formatItem("-v, --verbose", 20, "enable verbose output");
     * @endcode
     * @see https://github.com/tj/commander.js#custom-help
     * @since 0.1.0
     */
    virtual std::string formatItem(const std::string& term, int termWidth,
                                   const std::string& description) const;

    /**
     * @brief Generate the complete built-in help text.
     * @param cmd The command to generate help for.
     * @param helper The help instance to use for formatting.
     * @return Complete help text string.
     * @par Example
     * @code{.cpp}
     *   auto text = help.formatHelp(program, help);
     * @endcode
     * @see https://github.com/tj/commander.js#custom-help
     * @since 0.1.0
     */
    virtual std::string formatHelp(const Command& cmd, const Help& helper) const;
};

/**
 * @brief Strip ANSI SGR escape sequences from a string.
 * @param str Input string potentially containing ANSI codes.
 * @return String with ANSI SGR codes removed.
 * @par Example
 * @code{.cpp}
 *   auto plain = stripColor("\033[31mred\033[0m");  // "red"
 * @endcode
 * @since 0.1.0
 */
std::string stripColor(const std::string& str);

} // namespace commander
} // namespace polycpp
