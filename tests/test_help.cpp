#include <polycpp/commander/detail/aggregator.hpp>
#include <gtest/gtest.h>

using namespace polycpp::commander;

// ============ BoxWrap ============

TEST(HelpTest, BoxWrapBasic) {
    Help help;
    auto result = help.boxWrap("hello world", 80);
    EXPECT_EQ(result, "hello world");
}

TEST(HelpTest, BoxWrapWraps) {
    Help help;
    help.minWidthToWrap = 5;  // lower threshold so we can test with width=8
    auto result = help.boxWrap("aaa bbb ccc ddd", 8);
    // Should wrap after ~8 chars
    EXPECT_NE(result.find('\n'), std::string::npos);
}

TEST(HelpTest, BoxWrapTooNarrow) {
    Help help;
    help.minWidthToWrap = 40;
    auto result = help.boxWrap("hello world foo bar", 20);
    // Width < minWidthToWrap, no wrapping
    EXPECT_EQ(result, "hello world foo bar");
}

TEST(HelpTest, BoxWrapPreservesNewlines) {
    Help help;
    auto result = help.boxWrap("line1\nline2", 80);
    EXPECT_NE(result.find("line1\nline2"), std::string::npos);
}

// ============ Preformatted ============

TEST(HelpTest, PreformattedDetectsIndent) {
    Help help;
    EXPECT_TRUE(help.preformatted("line1\n  indented"));
}

TEST(HelpTest, PreformattedFalseForPlain) {
    Help help;
    EXPECT_FALSE(help.preformatted("just plain text"));
}

TEST(HelpTest, PreformattedFalseForDoubleNewline) {
    Help help;
    EXPECT_FALSE(help.preformatted("paragraph1\n\nparagraph2"));
}

// ============ DisplayWidth ============

TEST(HelpTest, DisplayWidthPlainText) {
    Help help;
    EXPECT_EQ(help.displayWidth("hello"), 5);
}

TEST(HelpTest, DisplayWidthStripAnsi) {
    Help help;
    EXPECT_EQ(help.displayWidth("\033[31mred\033[0m"), 3);
}

TEST(HelpTest, DisplayWidthEmpty) {
    Help help;
    EXPECT_EQ(help.displayWidth(""), 0);
}

// ============ StripColor ============

TEST(HelpTest, StripColorBasic) {
    auto result = stripColor("\033[31mred text\033[0m");
    EXPECT_EQ(result, "red text");
}

TEST(HelpTest, StripColorNoAnsi) {
    auto result = stripColor("plain text");
    EXPECT_EQ(result, "plain text");
}

// ============ FormatItem ============

TEST(HelpTest, FormatItemShortTerm) {
    Help help;
    help.helpWidth = 80;
    auto result = help.formatItem("-v", 10, "verbose output");
    // Should have 2-char indent, padded term, 2-space spacer, description
    EXPECT_NE(result.find("-v"), std::string::npos);
    EXPECT_NE(result.find("verbose output"), std::string::npos);
}

TEST(HelpTest, FormatItemNoDescription) {
    Help help;
    auto result = help.formatItem("-v, --verbose", 20, "");
    EXPECT_EQ(result, "  -v, --verbose");
}

TEST(HelpTest, FormatItemLongDescription) {
    Help help;
    help.helpWidth = 40;
    help.minWidthToWrap = 5;  // lower threshold to allow wrapping in short space
    auto result = help.formatItem("-v", 5, "this is a very long description that should wrap");
    EXPECT_NE(result.find('\n'), std::string::npos);
}

// ============ FormatHelp (integration with Command) ============

TEST(HelpTest, FormatHelpBasic) {
    Command cmd;
    cmd.exitOverride();
    cmd.name("myapp");
    cmd.description("My application");
    cmd.option("-v, --verbose", "enable verbose output");

    Help help;
    help.prepareContext(80);
    auto text = help.formatHelp(cmd, help);

    EXPECT_NE(text.find("Usage:"), std::string::npos);
    EXPECT_NE(text.find("myapp"), std::string::npos);
    EXPECT_NE(text.find("My application"), std::string::npos);
    EXPECT_NE(text.find("Options:"), std::string::npos);
    EXPECT_NE(text.find("--verbose"), std::string::npos);
}

TEST(HelpTest, FormatHelpWithSubcommands) {
    Command cmd;
    cmd.exitOverride();
    cmd.name("myapp");
    cmd.command("serve").description("start the server");
    cmd.command("build").description("build the project");

    Help help;
    help.prepareContext(80);
    auto text = help.formatHelp(cmd, help);

    EXPECT_NE(text.find("Commands:"), std::string::npos);
    EXPECT_NE(text.find("serve"), std::string::npos);
    EXPECT_NE(text.find("build"), std::string::npos);
}

TEST(HelpTest, FormatHelpWithArguments) {
    Command cmd;
    cmd.exitOverride();
    cmd.name("myapp");
    cmd.argument("<input>", "input file");
    cmd.argument("[output]", "output file");

    Help help;
    help.prepareContext(80);
    auto text = help.formatHelp(cmd, help);

    EXPECT_NE(text.find("Arguments:"), std::string::npos);
    EXPECT_NE(text.find("input"), std::string::npos);
    EXPECT_NE(text.find("output"), std::string::npos);
}

// ============ VisibleOptions ============

TEST(HelpTest, VisibleOptionsIncludesHelpOption) {
    Command cmd;
    cmd.exitOverride();
    cmd.option("-v, --verbose", "verbose");

    Help help;
    auto opts = help.visibleOptions(cmd);
    // Should include -v and -h (help option)
    EXPECT_GE(opts.size(), 2u);
}

TEST(HelpTest, VisibleOptionsHiddenExcluded) {
    Command cmd;
    cmd.exitOverride();
    auto opt = Option("-s, --secret", "secret option");
    opt.hideHelp();
    cmd.addOption(std::move(opt));

    Help help;
    auto opts = help.visibleOptions(cmd);
    bool found = false;
    for (const auto& o : opts) {
        if (o.name() == "secret") found = true;
    }
    EXPECT_FALSE(found);
}

TEST(HelpTest, SortOptions) {
    Command cmd;
    cmd.exitOverride();
    cmd.option("-z, --zeta", "zeta");
    cmd.option("-a, --alpha", "alpha");

    Help help;
    help.sortOptions = true;
    auto opts = help.visibleOptions(cmd);

    // Find positions
    int alphaIdx = -1, zetaIdx = -1;
    for (size_t i = 0; i < opts.size(); ++i) {
        if (opts[i].name() == "alpha") alphaIdx = static_cast<int>(i);
        if (opts[i].name() == "zeta") zetaIdx = static_cast<int>(i);
    }
    EXPECT_LT(alphaIdx, zetaIdx);
}

// ============ OptionDescription ============

TEST(HelpTest, OptionDescriptionWithChoices) {
    Option opt("-s, --size <value>", "set size");
    opt.choices({"small", "medium", "large"});

    Help help;
    auto desc = help.optionDescription(opt);
    EXPECT_NE(desc.find("choices:"), std::string::npos);
    EXPECT_NE(desc.find("\"small\""), std::string::npos);
}

TEST(HelpTest, OptionDescriptionWithDefault) {
    Option opt("-p, --port <number>", "port number");
    opt.defaultValue(polycpp::JsonValue("3000"));

    Help help;
    auto desc = help.optionDescription(opt);
    EXPECT_NE(desc.find("default:"), std::string::npos);
    EXPECT_NE(desc.find("3000"), std::string::npos);
}

TEST(HelpTest, OptionDescriptionWithEnv) {
    Option opt("-p, --port <number>", "port number");
    opt.env("PORT");

    Help help;
    auto desc = help.optionDescription(opt);
    EXPECT_NE(desc.find("env: PORT"), std::string::npos);
}

// ============ CommandUsage ============

TEST(HelpTest, CommandUsageBasic) {
    Command cmd;
    cmd.name("myapp");
    cmd.option("-v, --verbose", "verbose");

    Help help;
    auto usage = help.commandUsage(cmd);
    EXPECT_NE(usage.find("myapp"), std::string::npos);
    EXPECT_NE(usage.find("[options]"), std::string::npos);
}

TEST(HelpTest, CommandUsageWithAlias) {
    Command cmd;
    cmd.name("serve");
    cmd.alias("s");

    Help help;
    auto usage = help.commandUsage(cmd);
    EXPECT_NE(usage.find("serve|s"), std::string::npos);
}

// ============ Style Methods (no color) ============

TEST(HelpTest, StyleMethodsNoColor) {
    Help help;
    help.outputHasColors = false;
    EXPECT_EQ(help.styleTitle("Options:"), "Options:");
    EXPECT_EQ(help.styleUsage("myapp [options]"), "myapp [options]");
    EXPECT_EQ(help.styleOptionTerm("-v"), "-v");
    EXPECT_EQ(help.styleSubcommandTerm("serve"), "serve");
    EXPECT_EQ(help.styleArgumentTerm("<file>"), "<file>");
    EXPECT_EQ(help.styleDescriptionText("desc"), "desc");
    EXPECT_EQ(help.styleCommandDescription("desc"), "desc");
    EXPECT_EQ(help.styleOptionDescription("desc"), "desc");
    EXPECT_EQ(help.styleSubcommandDescription("desc"), "desc");
    EXPECT_EQ(help.styleArgumentDescription("desc"), "desc");
}

// ============ Style Methods (with color) ============

TEST(HelpTest, StyleMethodsWithColor) {
    Help help;
    help.outputHasColors = true;

    // Title should be bold
    auto title = help.styleTitle("Options:");
    EXPECT_NE(title, "Options:");
    EXPECT_NE(title.find("\033[1m"), std::string::npos);
    EXPECT_NE(title.find("\033[22m"), std::string::npos);

    // Usage should be bold
    auto usage = help.styleUsage("myapp [options]");
    EXPECT_NE(usage, "myapp [options]");
    EXPECT_NE(usage.find("\033[1m"), std::string::npos);

    // Option term should be yellow
    auto optTerm = help.styleOptionTerm("-v, --verbose");
    EXPECT_NE(optTerm, "-v, --verbose");
    EXPECT_NE(optTerm.find("\033[33m"), std::string::npos);

    // Subcommand term should be yellow
    auto subTerm = help.styleSubcommandTerm("serve");
    EXPECT_NE(subTerm.find("\033[33m"), std::string::npos);

    // Argument term should be yellow
    auto argTerm = help.styleArgumentTerm("<file>");
    EXPECT_NE(argTerm.find("\033[33m"), std::string::npos);

    // Description should be dim
    auto desc = help.styleDescriptionText("some description");
    EXPECT_NE(desc, "some description");
    EXPECT_NE(desc.find("\033[2m"), std::string::npos);
}

TEST(HelpTest, StyleMethodsEmptyString) {
    Help help;
    help.outputHasColors = true;
    // Empty strings should remain empty even with colors enabled
    EXPECT_EQ(help.styleTitle(""), "");
    EXPECT_EQ(help.styleOptionTerm(""), "");
    EXPECT_EQ(help.styleDescriptionText(""), "");
}

// ============ DisplayWidth with ANSI ============

TEST(HelpTest, DisplayWidthIgnoresAnsiCodes) {
    Help help;
    EXPECT_EQ(help.displayWidth("\033[1mBold\033[0m"), 4);
    EXPECT_EQ(help.displayWidth("\033[33mYellow\033[0m"), 6);
    EXPECT_EQ(help.displayWidth("\033[2mDim text\033[0m"), 8);
}

// ============ PrepareContext with options struct ============

TEST(HelpTest, PrepareContextWithOptions) {
    Help help;
    help.prepareContext({.helpWidth = 120, .outputHasColors = true});
    EXPECT_EQ(help.helpWidth, 120);
    EXPECT_TRUE(help.outputHasColors);
}

TEST(HelpTest, PrepareContextWithOptionsDefaultWidth) {
    Help help;
    help.prepareContext({.helpWidth = 0, .outputHasColors = false});
    EXPECT_EQ(help.helpWidth, 80);
    EXPECT_FALSE(help.outputHasColors);
}

TEST(HelpTest, PrepareContextPreservesExistingWidth) {
    Help help;
    help.helpWidth = 100;
    help.prepareContext({.helpWidth = 60, .outputHasColors = true});
    // helpWidth was already set, should not be overwritten
    EXPECT_EQ(help.helpWidth, 100);
    EXPECT_TRUE(help.outputHasColors);
}

// ============ FormatHelp with colors ============

TEST(HelpTest, FormattedHelpWithColorsEnabled) {
    Command cmd;
    cmd.exitOverride();
    cmd.name("test")
       .description("A test program")
       .option("-v, --verbose", "verbose output");
    // Force colors on
    cmd.configureOutput({
        .getOutHasColors = []() { return true; },
        .getErrHasColors = []() { return true; }
    });
    auto helpText = cmd.helpInformation();
    // Should contain ANSI codes
    EXPECT_NE(helpText.find("\033["), std::string::npos);
}

TEST(HelpTest, FormattedHelpWithColorsDisabled) {
    Command cmd;
    cmd.exitOverride();
    cmd.name("test")
       .description("A test program")
       .option("-v, --verbose", "verbose output");
    // Force colors off
    cmd.configureOutput({
        .getOutHasColors = []() { return false; },
        .getErrHasColors = []() { return false; }
    });
    auto helpText = cmd.helpInformation();
    // Should NOT contain ANSI codes
    EXPECT_EQ(helpText.find("\033["), std::string::npos);
}

TEST(HelpTest, FormattedHelpErrorContextUsesErrColors) {
    Command cmd;
    cmd.exitOverride();
    cmd.name("test")
       .option("-v, --verbose", "verbose output");
    // stdout has no colors, stderr has colors
    cmd.configureOutput({
        .getOutHasColors = []() { return false; },
        .getErrHasColors = []() { return true; }
    });
    // Error context should use stderr color setting
    auto helpText = cmd.helpInformation({.error = true});
    EXPECT_NE(helpText.find("\033["), std::string::npos);

    // Non-error context should use stdout color setting (no colors)
    auto helpTextOut = cmd.helpInformation({.error = false});
    EXPECT_EQ(helpTextOut.find("\033["), std::string::npos);
}

TEST(HelpTest, FormattedHelpWithColorsHasCorrectContent) {
    Command cmd;
    cmd.exitOverride();
    cmd.name("myapp")
       .description("My application")
       .option("-v, --verbose", "enable verbose output");
    cmd.argument("<file>", "input file");
    cmd.command("serve").description("start the server");
    cmd.configureOutput({
        .getOutHasColors = []() { return true; }
    });
    auto helpText = cmd.helpInformation();
    // Strip ANSI to verify content is correct
    auto plain = stripColor(helpText);
    EXPECT_NE(plain.find("Usage:"), std::string::npos);
    EXPECT_NE(plain.find("myapp"), std::string::npos);
    EXPECT_NE(plain.find("My application"), std::string::npos);
    EXPECT_NE(plain.find("Arguments:"), std::string::npos);
    EXPECT_NE(plain.find("Options:"), std::string::npos);
    EXPECT_NE(plain.find("Commands:"), std::string::npos);
    EXPECT_NE(plain.find("--verbose"), std::string::npos);
    EXPECT_NE(plain.find("serve"), std::string::npos);
}
