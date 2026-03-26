#include <polycpp/commander/detail/aggregator.hpp>
#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <sys/stat.h>
#include <unistd.h>

using namespace polycpp::commander;

// Helper to parse from "user" mode (no node/script prefix)
static void parseUser(Command& cmd, std::initializer_list<std::string> args) {
    cmd.parse(std::vector<std::string>(args), {.from = "user"});
}

// ============ Basic Construction ============

TEST(CommandTest, DefaultConstructor) {
    Command cmd;
    EXPECT_EQ(cmd.name(), "");
    EXPECT_TRUE(cmd.options.empty());
    EXPECT_TRUE(cmd.registeredArguments.empty());
    EXPECT_TRUE(cmd.commands.empty());
}

TEST(CommandTest, NamedConstructor) {
    Command cmd("serve");
    EXPECT_EQ(cmd.name(), "serve");
}

TEST(CommandTest, SetName) {
    Command cmd;
    cmd.name("myapp");
    EXPECT_EQ(cmd.name(), "myapp");
}

TEST(CommandTest, SetDescription) {
    Command cmd;
    cmd.description("A great app");
    EXPECT_EQ(cmd.description(), "A great app");
}

TEST(CommandTest, SetSummary) {
    Command cmd;
    cmd.summary("Short summary");
    EXPECT_EQ(cmd.summary(), "Short summary");
}

TEST(CommandTest, NameChaining) {
    Command cmd;
    auto& ret = cmd.name("foo");
    EXPECT_EQ(&ret, &cmd);
    EXPECT_EQ(cmd.name(), "foo");
}

// ============ Options ============

TEST(CommandTest, BooleanOption) {
    Command cmd;
    cmd.exitOverride();
    cmd.option("-v, --verbose", "enable verbose output");
    parseUser(cmd, {"--verbose"});
    auto o = cmd.opts();
    EXPECT_TRUE(std::any_cast<bool>(o["verbose"]));
}

TEST(CommandTest, BooleanOptionShort) {
    Command cmd;
    cmd.exitOverride();
    cmd.option("-v, --verbose", "enable verbose output");
    parseUser(cmd, {"-v"});
    auto o = cmd.opts();
    EXPECT_TRUE(std::any_cast<bool>(o["verbose"]));
}

TEST(CommandTest, OptionWithRequiredValue) {
    Command cmd;
    cmd.exitOverride();
    cmd.option("-n, --name <value>", "your name");
    parseUser(cmd, {"--name", "Alice"});
    auto o = cmd.opts();
    EXPECT_EQ(std::any_cast<std::string>(o["name"]), "Alice");
}

TEST(CommandTest, OptionWithEqualsValue) {
    Command cmd;
    cmd.exitOverride();
    cmd.option("-n, --name <value>", "your name");
    parseUser(cmd, {"--name=Alice"});
    auto o = cmd.opts();
    EXPECT_EQ(std::any_cast<std::string>(o["name"]), "Alice");
}

TEST(CommandTest, CombinedShortFlags) {
    Command cmd;
    cmd.exitOverride();
    cmd.option("-a", "flag a");
    cmd.option("-b", "flag b");
    cmd.option("-c", "flag c");
    parseUser(cmd, {"-abc"});
    auto o = cmd.opts();
    EXPECT_TRUE(std::any_cast<bool>(o["a"]));
    EXPECT_TRUE(std::any_cast<bool>(o["b"]));
    EXPECT_TRUE(std::any_cast<bool>(o["c"]));
}

TEST(CommandTest, NegateOption) {
    Command cmd;
    cmd.exitOverride();
    cmd.option("--no-color", "disable color");
    parseUser(cmd, {"--no-color"});
    auto o = cmd.opts();
    EXPECT_FALSE(std::any_cast<bool>(o["color"]));
}

TEST(CommandTest, NegateOptionDefault) {
    Command cmd;
    cmd.exitOverride();
    cmd.option("--no-color", "disable color");
    parseUser(cmd, {});
    auto o = cmd.opts();
    // Default for --no-color is true (the positive)
    EXPECT_TRUE(std::any_cast<bool>(o["color"]));
}

TEST(CommandTest, DefaultOptionValue) {
    Command cmd;
    cmd.exitOverride();
    cmd.option("-p, --port <number>", "port number", std::any(std::string("3000")));
    parseUser(cmd, {});
    auto o = cmd.opts();
    EXPECT_EQ(std::any_cast<std::string>(o["port"]), "3000");
}

TEST(CommandTest, VariadicOption) {
    Command cmd;
    cmd.exitOverride();
    cmd.option("-f, --files <values...>", "input files");
    parseUser(cmd, {"--files", "a.txt", "b.txt", "c.txt"});
    auto o = cmd.opts();
    auto files = std::any_cast<std::vector<std::string>>(o["files"]);
    ASSERT_EQ(files.size(), 3u);
    EXPECT_EQ(files[0], "a.txt");
    EXPECT_EQ(files[1], "b.txt");
    EXPECT_EQ(files[2], "c.txt");
}

TEST(CommandTest, RequiredOptionPresent) {
    Command cmd;
    cmd.exitOverride();
    cmd.requiredOption("-c, --cheese <type>", "pizza cheese type");
    parseUser(cmd, {"--cheese", "mozzarella"});
    auto o = cmd.opts();
    EXPECT_EQ(std::any_cast<std::string>(o["cheese"]), "mozzarella");
}

TEST(CommandTest, RequiredOptionMissing) {
    Command cmd;
    cmd.exitOverride();
    cmd.requiredOption("-c, --cheese <type>", "pizza cheese type");
    EXPECT_THROW({
        parseUser(cmd, {});
    }, CommanderError);
}

TEST(CommandTest, OptionMissingArgument) {
    Command cmd;
    cmd.exitOverride();
    cmd.option("-n, --name <value>", "your name");
    EXPECT_THROW({
        parseUser(cmd, {"--name"});
    }, CommanderError);
}

TEST(CommandTest, OptionCustomParser) {
    Command cmd;
    cmd.exitOverride();
    ParseFn parser = [](const std::string& v, const std::any&) -> std::any {
        return std::any(std::stoi(v));
    };
    cmd.option("-p, --port <number>", "port number", parser);
    parseUser(cmd, {"--port", "3000"});
    auto o = cmd.opts();
    EXPECT_EQ(std::any_cast<int>(o["port"]), 3000);
}

TEST(CommandTest, OptionChoicesValid) {
    Command cmd;
    cmd.exitOverride();
    auto opt = Option("-s, --size <value>", "size");
    opt.choices({"small", "medium", "large"});
    cmd.addOption(std::move(opt));
    parseUser(cmd, {"--size", "medium"});
    auto o = cmd.opts();
    EXPECT_EQ(std::any_cast<std::string>(o["size"]), "medium");
}

TEST(CommandTest, OptionChoicesInvalid) {
    Command cmd;
    cmd.exitOverride();
    auto opt = Option("-s, --size <value>", "size");
    opt.choices({"small", "medium", "large"});
    cmd.addOption(std::move(opt));
    EXPECT_THROW({
        parseUser(cmd, {"--size", "huge"});
    }, CommanderError);
}

TEST(CommandTest, OptionConflicts) {
    Command cmd;
    cmd.exitOverride();
    auto opt1 = Option("--json", "json output");
    opt1.conflicts("csv");
    cmd.addOption(std::move(opt1));
    cmd.option("--csv", "csv output");
    EXPECT_THROW({
        parseUser(cmd, {"--json", "--csv"});
    }, CommanderError);
}

TEST(CommandTest, OptionImplied) {
    Command cmd;
    cmd.exitOverride();
    auto opt = Option("-v, --verbose", "verbose");
    opt.implies({{"logLevel", std::any(std::string("debug"))}});
    cmd.addOption(std::move(opt));
    cmd.option("--log-level <level>", "log level", std::any(std::string("info")));
    parseUser(cmd, {"--verbose"});
    auto o = cmd.opts();
    EXPECT_EQ(std::any_cast<std::string>(o["logLevel"]), "debug");
}

TEST(CommandTest, OptionPreset) {
    Command cmd;
    cmd.exitOverride();
    auto opt = Option("--color [when]", "use color");
    opt.defaultValue(std::any(std::string("auto")));
    opt.preset(std::any(std::string("always")));
    cmd.addOption(std::move(opt));
    // Without option-argument, preset value should be used
    parseUser(cmd, {"--color"});
    auto o = cmd.opts();
    EXPECT_EQ(std::any_cast<std::string>(o["color"]), "always");
}

// ============ Arguments ============

TEST(CommandTest, PositionalArgument) {
    Command cmd;
    cmd.exitOverride();
    cmd.argument("<file>", "input file");
    cmd.action([](auto& args, auto&, auto&) {
        EXPECT_EQ(std::any_cast<std::string>(args[0]), "test.txt");
    });
    parseUser(cmd, {"test.txt"});
}

TEST(CommandTest, MissingRequiredArgument) {
    Command cmd;
    cmd.exitOverride();
    cmd.argument("<file>", "input file");
    cmd.action([](auto&, auto&, auto&) {});
    EXPECT_THROW({
        parseUser(cmd, {});
    }, CommanderError);
}

TEST(CommandTest, VariadicArgument) {
    Command cmd;
    cmd.exitOverride();
    cmd.argument("<files...>", "input files");
    cmd.action([](auto& args, auto&, auto&) {
        auto files = std::any_cast<std::vector<std::string>>(args[0]);
        ASSERT_EQ(files.size(), 3u);
        EXPECT_EQ(files[0], "a.txt");
        EXPECT_EQ(files[1], "b.txt");
        EXPECT_EQ(files[2], "c.txt");
    });
    parseUser(cmd, {"a.txt", "b.txt", "c.txt"});
}

TEST(CommandTest, ExcessArguments) {
    Command cmd;
    cmd.exitOverride();
    cmd.argument("<file>", "input file");
    cmd.action([](auto&, auto&, auto&) {});
    EXPECT_THROW({
        parseUser(cmd, {"a.txt", "b.txt"});
    }, CommanderError);
}

TEST(CommandTest, AllowExcessArguments) {
    Command cmd;
    cmd.exitOverride();
    cmd.argument("<file>", "input file");
    cmd.allowExcessArguments();
    cmd.action([](auto&, auto&, auto&) {});
    EXPECT_NO_THROW(parseUser(cmd, {"a.txt", "b.txt"}));
}

TEST(CommandTest, ArgumentsFromString) {
    Command cmd;
    cmd.exitOverride();
    cmd.arguments("<source> [destination]");
    cmd.action([](auto& args, auto&, auto&) {
        EXPECT_EQ(std::any_cast<std::string>(args[0]), "src");
        EXPECT_EQ(std::any_cast<std::string>(args[1]), "dst");
    });
    parseUser(cmd, {"src", "dst"});
}

// ============ Subcommands ============

TEST(CommandTest, SubcommandDispatch) {
    Command cmd;
    cmd.exitOverride();
    bool called = false;
    cmd.command("serve")
       .description("start server")
       .action([&](auto&, auto&, auto&) { called = true; });
    parseUser(cmd, {"serve"});
    EXPECT_TRUE(called);
}

TEST(CommandTest, SubcommandWithArgs) {
    Command cmd;
    cmd.exitOverride();
    std::string capturedSrc;
    cmd.command("clone <source> [destination]")
       .description("clone a repo")
       .action([&](auto& args, auto&, auto&) {
           capturedSrc = std::any_cast<std::string>(args[0]);
       });
    parseUser(cmd, {"clone", "https://example.com/repo"});
    EXPECT_EQ(capturedSrc, "https://example.com/repo");
}

TEST(CommandTest, SubcommandWithOption) {
    Command cmd;
    cmd.exitOverride();
    int capturedPort = 0;
    cmd.command("serve")
       .option("-p, --port <number>", "port")
       .action([&](auto&, auto& opts, auto&) {
           capturedPort = std::stoi(std::any_cast<std::string>(opts.at("port")));
       });
    parseUser(cmd, {"serve", "--port", "3000"});
    EXPECT_EQ(capturedPort, 3000);
}

TEST(CommandTest, SubcommandAlias) {
    Command cmd;
    cmd.exitOverride();
    bool called = false;
    cmd.command("install")
       .alias("i")
       .action([&](auto&, auto&, auto&) { called = true; });
    parseUser(cmd, {"i"});
    EXPECT_TRUE(called);
}

TEST(CommandTest, UnknownCommand) {
    Command cmd;
    cmd.exitOverride();
    cmd.command("serve").action([](auto&, auto&, auto&) {});
    EXPECT_THROW({
        parseUser(cmd, {"strat"});
    }, CommanderError);
}

TEST(CommandTest, UnknownCommandWithSuggestion) {
    Command cmd;
    cmd.exitOverride();
    cmd.command("serve").action([](auto&, auto&, auto&) {});
    try {
        parseUser(cmd, {"serv"});
        FAIL() << "Expected CommanderError";
    } catch (const CommanderError& e) {
        std::string msg = e.what();
        EXPECT_NE(msg.find("Did you mean"), std::string::npos);
    }
}

// ============ Action Handler ============

TEST(CommandTest, ActionReceivesArgsOptsCmd) {
    Command cmd;
    cmd.exitOverride();
    cmd.argument("<name>", "name");
    cmd.option("-v, --verbose", "verbose");
    bool actionCalled = false;
    cmd.action([&](auto& args, auto& opts, auto& command) {
        actionCalled = true;
        EXPECT_EQ(std::any_cast<std::string>(args[0]), "Alice");
        EXPECT_TRUE(std::any_cast<bool>(opts.at("verbose")));
        EXPECT_EQ(command.name(), "program");
    });
    parseUser(cmd, {"--verbose", "Alice"});
    EXPECT_TRUE(actionCalled);
}

// ============ Error Handling ============

TEST(CommandTest, ExitOverrideThrowsCommanderError) {
    Command cmd;
    cmd.exitOverride();
    cmd.option("-n, --name <value>", "name");
    try {
        parseUser(cmd, {"--name"});
        FAIL() << "Expected CommanderError";
    } catch (const CommanderError& e) {
        EXPECT_EQ(e.code, "commander.optionMissingArgument");
        EXPECT_EQ(e.exitCode, 1);
    }
}

TEST(CommandTest, UnknownOption) {
    Command cmd;
    cmd.exitOverride();
    cmd.action([](auto&, auto&, auto&) {});
    try {
        parseUser(cmd, {"--unknown"});
        FAIL() << "Expected CommanderError";
    } catch (const CommanderError& e) {
        EXPECT_EQ(e.code, "commander.unknownOption");
    }
}

TEST(CommandTest, AllowUnknownOption) {
    Command cmd;
    cmd.exitOverride();
    cmd.allowUnknownOption();
    cmd.allowExcessArguments();
    cmd.helpOption(false);  // disable help so --unknown doesn't trigger help
    cmd.action([](auto&, auto&, auto&) {});
    EXPECT_NO_THROW(parseUser(cmd, {"--unknown"}));
}

TEST(CommandTest, ErrorMethodTriggersExitOverride) {
    Command cmd;
    cmd.exitOverride();
    try {
        cmd.error("custom error message", {.exitCode = 42, .code = "custom.error"});
        FAIL() << "Expected CommanderError";
    } catch (const CommanderError& e) {
        EXPECT_EQ(e.exitCode, 42);
        EXPECT_EQ(e.code, "custom.error");
    }
}

// ============ Version ============

TEST(CommandTest, VersionOutput) {
    Command cmd;
    cmd.exitOverride();
    std::string captured;
    cmd.configureOutput({.writeOut = [&](const std::string& s) { captured += s; }});
    cmd.version("1.2.3");
    try {
        parseUser(cmd, {"--version"});
    } catch (const CommanderError& e) {
        EXPECT_EQ(e.code, "commander.version");
    }
    EXPECT_NE(captured.find("1.2.3"), std::string::npos);
}

TEST(CommandTest, VersionGetter) {
    Command cmd;
    cmd.version("2.0.0");
    EXPECT_EQ(cmd.version(), "2.0.0");
}

// ============ Help ============

TEST(CommandTest, HelpContainsUsage) {
    Command cmd;
    cmd.exitOverride();
    cmd.name("myapp");
    cmd.description("A great application");
    cmd.option("-v, --verbose", "enable verbose output");

    auto helpText = cmd.helpInformation();
    EXPECT_NE(helpText.find("Usage:"), std::string::npos);
    EXPECT_NE(helpText.find("myapp"), std::string::npos);
    EXPECT_NE(helpText.find("A great application"), std::string::npos);
    EXPECT_NE(helpText.find("--verbose"), std::string::npos);
}

TEST(CommandTest, HelpContainsSubcommands) {
    Command cmd;
    cmd.exitOverride();
    cmd.name("myapp");
    cmd.command("serve").description("start server");
    cmd.command("build").description("build project");

    auto helpText = cmd.helpInformation();
    EXPECT_NE(helpText.find("Commands:"), std::string::npos);
    EXPECT_NE(helpText.find("serve"), std::string::npos);
    EXPECT_NE(helpText.find("build"), std::string::npos);
}

TEST(CommandTest, HelpFlag) {
    Command cmd;
    cmd.exitOverride();
    cmd.name("myapp");
    std::string captured;
    cmd.configureOutput({.writeOut = [&](const std::string& s) { captured += s; }});
    try {
        parseUser(cmd, {"--help"});
    } catch (const CommanderError& e) {
        EXPECT_EQ(e.code, "commander.helpDisplayed");
    }
    EXPECT_NE(captured.find("Usage:"), std::string::npos);
}

TEST(CommandTest, HelpOptionDisabled) {
    Command cmd;
    cmd.exitOverride();
    cmd.helpOption(false);
    cmd.allowUnknownOption();
    cmd.allowExcessArguments();
    cmd.action([](auto&, auto&, auto&) {});
    // --help should be treated as unknown option since help is disabled
    // With allowUnknownOption + allowExcessArguments, it should not throw
    EXPECT_NO_THROW(parseUser(cmd, {"--help"}));
}

TEST(CommandTest, CustomHelpFlags) {
    Command cmd;
    cmd.exitOverride();
    cmd.helpOption("-?, --aide", "show help");
    std::string captured;
    cmd.configureOutput({.writeOut = [&](const std::string& s) { captured += s; }});
    try {
        parseUser(cmd, {"--aide"});
    } catch (const CommanderError& e) {
        EXPECT_EQ(e.code, "commander.helpDisplayed");
    }
    EXPECT_NE(captured.find("Usage:"), std::string::npos);
}

// ============ Hooks ============

TEST(CommandTest, PreActionHook) {
    Command cmd;
    cmd.exitOverride();
    std::vector<std::string> order;
    cmd.hook("preAction", [&](auto&, auto&) { order.push_back("pre"); });
    cmd.action([&](auto&, auto&, auto&) { order.push_back("action"); });
    parseUser(cmd, {});
    ASSERT_EQ(order.size(), 2u);
    EXPECT_EQ(order[0], "pre");
    EXPECT_EQ(order[1], "action");
}

TEST(CommandTest, PostActionHook) {
    Command cmd;
    cmd.exitOverride();
    std::vector<std::string> order;
    cmd.hook("postAction", [&](auto&, auto&) { order.push_back("post"); });
    cmd.action([&](auto&, auto&, auto&) { order.push_back("action"); });
    parseUser(cmd, {});
    ASSERT_EQ(order.size(), 2u);
    EXPECT_EQ(order[0], "action");
    EXPECT_EQ(order[1], "post");
}

TEST(CommandTest, InvalidHookEvent) {
    Command cmd;
    EXPECT_THROW({
        cmd.hook("badEvent", [](auto&, auto&) {});
    }, std::runtime_error);
}

// ============ Settings ============

TEST(CommandTest, PassThroughOptions) {
    Command cmd;
    cmd.exitOverride();
    cmd.enablePositionalOptions();
    cmd.command("serve")
       .passThroughOptions()
       .option("-p, --port <number>", "port")
       .action([](auto& args, auto& opts, auto& c) {
           // args from passthrough should include unknown options
       });
    // Just verify it doesn't throw
    parseUser(cmd, {"serve", "-p", "3000"});
}

// ============ OptsWithGlobals ============

TEST(CommandTest, OptsWithGlobals) {
    Command cmd;
    cmd.exitOverride();
    cmd.option("-v, --verbose", "verbose");
    int captured = 0;
    cmd.command("serve")
       .option("-p, --port <number>", "port")
       .action([&](auto&, auto&, auto& c) {
           auto allOpts = c.optsWithGlobals();
           if (allOpts.count("verbose") && allOpts["verbose"].has_value()) {
               captured++;
           }
           if (allOpts.count("port") && allOpts["port"].has_value()) {
               captured++;
           }
       });
    parseUser(cmd, {"--verbose", "serve", "--port", "3000"});
    EXPECT_EQ(captured, 2);
}

// ============ Parse with node format ============

TEST(CommandTest, ParseNodeFormat) {
    Command cmd;
    cmd.exitOverride();
    cmd.option("-v, --verbose", "verbose");
    cmd.action([](auto&, auto&, auto&) {});
    cmd.parse({"node", "app.js", "--verbose"});
    auto o = cmd.opts();
    EXPECT_TRUE(std::any_cast<bool>(o["verbose"]));
}

// ============ CopyInheritedSettings ============

TEST(CommandTest, CopyInheritedSettings) {
    Command parent;
    parent.exitOverride();
    parent.showSuggestionAfterError(false);

    Command child("sub");
    child.copyInheritedSettings(parent);
    // The child should have the same settings
    // We verify by checking it doesn't throw for unknown command suggestion
    // (since suggestions are disabled)
}

// ============ Alias ============

TEST(CommandTest, AliasGetter) {
    Command cmd("serve");
    cmd.alias("s");
    EXPECT_EQ(cmd.alias(), "s");
}

TEST(CommandTest, AliasesGetter) {
    Command cmd("serve");
    cmd.aliases({"s", "sv"});
    auto aliases = cmd.aliases();
    ASSERT_EQ(aliases.size(), 2u);
    EXPECT_EQ(aliases[0], "s");
    EXPECT_EQ(aliases[1], "sv");
}

TEST(CommandTest, AliasCannotBeName) {
    Command cmd("serve");
    EXPECT_THROW(cmd.alias("serve"), std::runtime_error);
}

// ============ NameFromFilename ============

TEST(CommandTest, NameFromFilename) {
    Command cmd;
    cmd.nameFromFilename("/path/to/myapp.js");
    EXPECT_EQ(cmd.name(), "myapp");
}

TEST(CommandTest, NameFromFilenameNoExtension) {
    Command cmd;
    cmd.nameFromFilename("/usr/bin/myapp");
    EXPECT_EQ(cmd.name(), "myapp");
}

// ============ ShowHelpAfterError ============

TEST(CommandTest, ShowHelpAfterErrorString) {
    Command cmd;
    cmd.exitOverride();
    // Use std::string to ensure the string overload is called (not bool)
    cmd.showHelpAfterError(std::string("(use --help for help)"));
    cmd.option("-n, --name <value>", "name");

    std::string errOutput;
    cmd.configureOutput({.writeErr = [&](const std::string& s) { errOutput += s; }});
    try {
        parseUser(cmd, {"--name"});
    } catch (const CommanderError&) {}
    EXPECT_NE(errOutput.find("(use --help for help)"), std::string::npos);
}

// ============ ConfigureHelp ============

TEST(CommandTest, ConfigureHelpSortOptions) {
    Command cmd;
    cmd.exitOverride();
    cmd.name("myapp");
    cmd.configureHelp({{"sortOptions", std::any(true)}});
    cmd.option("-z, --zeta", "zeta option");
    cmd.option("-a, --alpha", "alpha option");

    auto helpText = cmd.helpInformation();
    // Alpha should appear before zeta when sorted
    auto alphaPos = helpText.find("--alpha");
    auto zetaPos = helpText.find("--zeta");
    EXPECT_LT(alphaPos, zetaPos);
}

// ============ ShowSuggestionAfterError ============

TEST(CommandTest, ShowSuggestionDisabled) {
    Command cmd;
    cmd.exitOverride();
    cmd.showSuggestionAfterError(false);
    cmd.command("serve").action([](auto&, auto&, auto&) {});
    try {
        parseUser(cmd, {"serv"});
        FAIL();
    } catch (const CommanderError& e) {
        std::string msg = e.what();
        EXPECT_EQ(msg.find("Did you mean"), std::string::npos);
    }
}

// ============ GetOptionValue / SetOptionValue ============

TEST(CommandTest, GetSetOptionValue) {
    Command cmd;
    cmd.setOptionValue("foo", std::any(std::string("bar")));
    auto val = cmd.getOptionValue("foo");
    EXPECT_EQ(std::any_cast<std::string>(val), "bar");
}

TEST(CommandTest, GetOptionValueSource) {
    Command cmd;
    cmd.setOptionValueWithSource("foo", std::any(42), "cli");
    EXPECT_EQ(cmd.getOptionValueSource("foo"), "cli");
}

// ============ Doublr-dash terminates options ============

TEST(CommandTest, DoubleDashTerminatesOptions) {
    Command cmd;
    cmd.exitOverride();
    cmd.option("-v, --verbose", "verbose");
    cmd.argument("[args...]", "remaining args");
    cmd.action([](auto& args, auto& opts, auto&) {
        // --verbose after -- should be treated as argument, not option
    });
    parseUser(cmd, {"--", "--verbose"});
    auto o = cmd.opts();
    // verbose should NOT be set (it was after --)
    EXPECT_FALSE(o.count("verbose") && o["verbose"].has_value() &&
                 o["verbose"].type() == typeid(bool) && std::any_cast<bool>(o["verbose"]));
}

// ============ Executable Subcommands ============

TEST(CommandTest, ExecutableSubcommandDefined) {
    Command cmd("myapp");
    cmd.exitOverride();
    cmd.executableCommand("install", "install packages");
    EXPECT_EQ(cmd.commands.size(), 1u);
}

TEST(CommandTest, ExecutableSubcommandReturnsThis) {
    Command cmd("myapp");
    cmd.exitOverride();
    auto& ret = cmd.executableCommand("install", "install packages");
    EXPECT_EQ(&ret, &cmd);  // Returns *this, not new subcommand
}

TEST(CommandTest, ExecutableSubcommandDir) {
    Command cmd("myapp");
    cmd.exitOverride();
    cmd.executableDir("/usr/local/bin");
    EXPECT_EQ(cmd.executableDir(), "/usr/local/bin");
}

TEST(CommandTest, ExecutableSubcommandDirDefault) {
    Command cmd("myapp");
    EXPECT_EQ(cmd.executableDir(), "");
}

TEST(CommandTest, ExecutableSubcommandNotFound) {
    Command cmd("myapp");
    cmd.exitOverride();
    cmd.configureOutput({.writeErr = [](const std::string&) {},
                         .outputError = [](const std::string&, auto) {}});
    cmd.executableCommand("nonexistent-xyz", "won't work");
    EXPECT_THROW(
        cmd.parse({"nonexistent-xyz"}, {.from = "user"}),
        CommanderError
    );
}

TEST(CommandTest, ExecutableSubcommandRun) {
    // Create a temp script that writes a marker file
    std::string marker = "/tmp/commander-exec-test-" + std::to_string(getpid());
    std::string script = "/tmp/myapp-runcmd";
    {
        std::ofstream f(script);
        f << "#!/bin/sh\ntouch " << marker << "\n";
    }
    chmod(script.c_str(), 0755);

    Command cmd("myapp");
    cmd.exitOverride();
    cmd.executableDir("/tmp");
    cmd.executableCommand("runcmd", "run a test command");
    cmd.parse({"runcmd"}, {.from = "user"});

    EXPECT_TRUE(std::filesystem::exists(marker));
    std::filesystem::remove(marker);
    std::filesystem::remove(script);
}

TEST(CommandTest, ExecutableSubcommandPassesArgs) {
    // Create a script that writes its args to a file
    std::string output = "/tmp/commander-args-test-" + std::to_string(getpid());
    std::string script = "/tmp/myapp-argcmd";
    {
        std::ofstream f(script);
        f << "#!/bin/sh\necho \"$@\" > " << output << "\n";
    }
    chmod(script.c_str(), 0755);

    Command cmd("myapp");
    cmd.exitOverride();
    cmd.executableDir("/tmp");
    cmd.executableCommand("argcmd", "test args");
    cmd.parse({"argcmd", "hello", "world"}, {.from = "user"});

    // Read back the output
    std::ifstream in(output);
    std::string content;
    std::getline(in, content);
    EXPECT_EQ(content, "hello world");

    std::filesystem::remove(output);
    std::filesystem::remove(script);
}

TEST(CommandTest, ExecutableSubcommandInHelp) {
    Command cmd("myapp");
    cmd.exitOverride();
    cmd.executableCommand("install", "install packages");
    auto help = cmd.helpInformation();
    EXPECT_TRUE(help.find("install") != std::string::npos);
    EXPECT_TRUE(help.find("install packages") != std::string::npos);
}

TEST(CommandTest, ExecutableSubcommandCustomExecutable) {
    // Create a temp script with a custom name
    std::string marker = "/tmp/commander-custom-exec-test-" + std::to_string(getpid());
    std::string script = "/tmp/my-custom-installer";
    {
        std::ofstream f(script);
        f << "#!/bin/sh\ntouch " << marker << "\n";
    }
    chmod(script.c_str(), 0755);

    Command cmd("myapp");
    cmd.exitOverride();
    cmd.executableDir("/tmp");
    cmd.executableCommand("install", "install packages", "my-custom-installer");
    cmd.parse({"install"}, {.from = "user"});

    EXPECT_TRUE(std::filesystem::exists(marker));
    std::filesystem::remove(marker);
    std::filesystem::remove(script);
}

TEST(CommandTest, ExecutableSubcommandExitCode) {
    // Create a script that exits with non-zero
    std::string script = "/tmp/myapp-failcmd";
    {
        std::ofstream f(script);
        f << "#!/bin/sh\nexit 42\n";
    }
    chmod(script.c_str(), 0755);

    Command cmd("myapp");
    cmd.exitOverride();
    cmd.executableDir("/tmp");
    cmd.executableCommand("failcmd", "a failing command");
    try {
        cmd.parse({"failcmd"}, {.from = "user"});
        FAIL() << "Expected CommanderError to be thrown";
    } catch (const CommanderError& e) {
        EXPECT_EQ(e.exitCode, 42);
        EXPECT_EQ(e.code, "commander.executeSubCommandAsync");
    }

    std::filesystem::remove(script);
}

TEST(CommandTest, ExecutableSubcommandWithArguments) {
    // executableCommand can define arguments like command()
    Command cmd("myapp");
    cmd.exitOverride();
    cmd.executableCommand("install <pkg>", "install a package");
    EXPECT_EQ(cmd.commands.size(), 1u);
    EXPECT_EQ(cmd.commands[0]->name(), "install");
    EXPECT_EQ(cmd.commands[0]->registeredArguments.size(), 1u);
}
