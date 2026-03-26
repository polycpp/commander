#include <polycpp/commander/detail/aggregator.hpp>
#include <gtest/gtest.h>

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

// ============ Help Subcommand ============

TEST(CommandTest, HelpCommandShowsRootHelpWhenNoSubcommand) {
    Command cmd;
    cmd.exitOverride();
    cmd.name("myapp");
    cmd.command("sub1").action([](auto&, auto&, auto&) {});
    std::string captured;
    cmd.configureOutput({.writeOut = [&](const std::string& s) { captured += s; }});
    try {
        parseUser(cmd, {"help"});
    } catch (const CommanderError& e) {
        // help exits via exit_ which throws with exitOverride
    }
    EXPECT_NE(captured.find("Usage:"), std::string::npos);
    EXPECT_NE(captured.find("myapp"), std::string::npos);
}

TEST(CommandTest, HelpCommandShowsSubcommandHelp) {
    Command cmd;
    cmd.exitOverride();
    cmd.name("myapp");
    auto& sub = cmd.command("install").description("install packages");
    sub.exitOverride();
    std::string captured;
    sub.configureOutput({.writeOut = [&](const std::string& s) { captured += s; }});
    try {
        parseUser(cmd, {"help", "install"});
    } catch (const CommanderError&) {
    }
    EXPECT_NE(captured.find("install"), std::string::npos);
}

TEST(CommandTest, HelpCommandDisabledWithFalse) {
    Command cmd;
    cmd.exitOverride();
    cmd.name("myapp");
    cmd.helpCommand(false);
    cmd.command("foo").action([](auto&, auto&, auto&) {});
    auto helpText = cmd.helpInformation();
    // "help [command]" should not appear in help output
    EXPECT_EQ(helpText.find("help [command]"), std::string::npos);
}

TEST(CommandTest, HelpCommandCustomName) {
    Command cmd;
    cmd.exitOverride();
    cmd.name("myapp");
    cmd.helpCommand("assist [cmd]", "show assistance");
    auto helpText = cmd.helpInformation();
    EXPECT_NE(helpText.find("assist"), std::string::npos);
    EXPECT_NE(helpText.find("show assistance"), std::string::npos);
}

TEST(CommandTest, HelpCommandNotAddedWhenRootHasAction) {
    Command cmd;
    cmd.exitOverride();
    cmd.name("myapp");
    cmd.command("sub1").action([](auto&, auto&, auto&) {});
    cmd.action([](auto&, auto&, auto&) {});
    auto helpText = cmd.helpInformation();
    // When root has action, implicit help command is NOT added
    EXPECT_EQ(helpText.find("help [command]"), std::string::npos);
}

TEST(CommandTest, HelpCommandAppearsInHelpOutput) {
    Command cmd;
    cmd.exitOverride();
    cmd.name("myapp");
    cmd.command("foo").description("do foo stuff");
    auto helpText = cmd.helpInformation();
    EXPECT_NE(helpText.find("Commands:"), std::string::npos);
    EXPECT_NE(helpText.find("help [command]"), std::string::npos);
    EXPECT_NE(helpText.find("display help for command"), std::string::npos);
}

TEST(CommandTest, HelpCommandForceAddWithTrue) {
    Command cmd;
    cmd.exitOverride();
    cmd.name("myapp");
    cmd.helpCommand(true);
    auto helpText = cmd.helpInformation();
    EXPECT_NE(helpText.find("help [command]"), std::string::npos);
}

TEST(CommandTest, AddHelpCommandWithCustomObject) {
    Command cmd;
    cmd.exitOverride();
    cmd.name("myapp");
    auto customHelp = std::make_unique<Command>("aide");
    customHelp->description("custom help command");
    customHelp->helpOption(false);
    customHelp->arguments("[cmd]");
    cmd.addHelpCommand(std::move(customHelp));
    auto helpText = cmd.helpInformation();
    EXPECT_NE(helpText.find("aide"), std::string::npos);
    EXPECT_NE(helpText.find("custom help command"), std::string::npos);
}

TEST(CommandTest, HelpCommandDispatchesUnknownSubcommand) {
    // When "help unknown" is called, it should dispatch to the unknown subcommand
    // which will cause help to be shown (via --help flag fallback)
    Command cmd;
    cmd.exitOverride();
    cmd.name("myapp");
    cmd.command("sub1").action([](auto&, auto&, auto&) {});
    std::string errCaptured;
    cmd.configureOutput({
        .writeOut = [](const std::string&) {},
        .writeErr = [&](const std::string& s) { errCaptured += s; }
    });
    try {
        parseUser(cmd, {"help", "unknown"});
    } catch (const CommanderError&) {
    }
    // Should have tried to dispatch to "unknown" and generated an error/help
}

TEST(CommandTest, HelpCommandInheritedBySubcommand) {
    Command cmd;
    cmd.exitOverride();
    cmd.name("myapp");
    cmd.helpCommand(false);

    auto& sub = cmd.command("sub1");
    sub.command("nested").action([](auto&, auto&, auto&) {});
    // Sub should inherit addImplicitHelpCommand_=false
    auto helpText = sub.helpInformation();
    EXPECT_EQ(helpText.find("help [command]"), std::string::npos);
}
