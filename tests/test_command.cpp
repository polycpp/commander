#include <polycpp/commander/detail/aggregator.hpp>
#include <polycpp/event_loop.hpp>
#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#if !defined(_WIN32)
#include <sys/stat.h>
#include <unistd.h>
#endif

using namespace polycpp::commander;

// Helper to parse from "user" mode (no node/script prefix)
static void parseUser(Command& cmd, std::initializer_list<std::string> args) {
    cmd.parse(std::vector<std::string>(args), {.from = "user"});
}

// ============ Basic Construction ============

TEST(CommandTest, DefaultConstructor) {
    Command cmd;
    EXPECT_EQ(cmd.name(), "");
    EXPECT_TRUE(cmd.options().empty());
    EXPECT_TRUE(cmd.registeredArguments().empty());
    EXPECT_TRUE(cmd.commands().empty());
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
    EXPECT_TRUE(o["verbose"].asBool());
}

TEST(CommandTest, BooleanOptionShort) {
    Command cmd;
    cmd.exitOverride();
    cmd.option("-v, --verbose", "enable verbose output");
    parseUser(cmd, {"-v"});
    auto o = cmd.opts();
    EXPECT_TRUE(o["verbose"].asBool());
}

TEST(CommandTest, OptionWithRequiredValue) {
    Command cmd;
    cmd.exitOverride();
    cmd.option("-n, --name <value>", "your name");
    parseUser(cmd, {"--name", "Alice"});
    auto o = cmd.opts();
    EXPECT_EQ(o["name"].asString(), "Alice");
}

TEST(CommandTest, OptionWithEqualsValue) {
    Command cmd;
    cmd.exitOverride();
    cmd.option("-n, --name <value>", "your name");
    parseUser(cmd, {"--name=Alice"});
    auto o = cmd.opts();
    EXPECT_EQ(o["name"].asString(), "Alice");
}

TEST(CommandTest, CombinedShortFlags) {
    Command cmd;
    cmd.exitOverride();
    cmd.option("-a", "flag a");
    cmd.option("-b", "flag b");
    cmd.option("-c", "flag c");
    parseUser(cmd, {"-abc"});
    auto o = cmd.opts();
    EXPECT_TRUE(o["a"].asBool());
    EXPECT_TRUE(o["b"].asBool());
    EXPECT_TRUE(o["c"].asBool());
}

TEST(CommandTest, NegateOption) {
    Command cmd;
    cmd.exitOverride();
    cmd.option("--no-color", "disable color");
    parseUser(cmd, {"--no-color"});
    auto o = cmd.opts();
    EXPECT_FALSE(o["color"].asBool());
}

TEST(CommandTest, NegateOptionDefault) {
    Command cmd;
    cmd.exitOverride();
    cmd.option("--no-color", "disable color");
    parseUser(cmd, {});
    auto o = cmd.opts();
    // Default for --no-color is true (the positive)
    EXPECT_TRUE(o["color"].asBool());
}

TEST(CommandTest, DefaultOptionValue) {
    Command cmd;
    cmd.exitOverride();
    cmd.option("-p, --port <number>", "port number", polycpp::JsonValue("3000"));
    parseUser(cmd, {});
    auto o = cmd.opts();
    EXPECT_EQ(o["port"].asString(), "3000");
}

TEST(CommandTest, VariadicOption) {
    Command cmd;
    cmd.exitOverride();
    cmd.option("-f, --files <values...>", "input files");
    parseUser(cmd, {"--files", "a.txt", "b.txt", "c.txt"});
    auto o = cmd.opts();
    ASSERT_TRUE(o["files"].isArray());
    auto& files = o["files"].asArray();
    ASSERT_EQ(files.size(), 3u);
    EXPECT_EQ(files[0].asString(), "a.txt");
    EXPECT_EQ(files[1].asString(), "b.txt");
    EXPECT_EQ(files[2].asString(), "c.txt");
}

TEST(CommandTest, RequiredOptionPresent) {
    Command cmd;
    cmd.exitOverride();
    cmd.requiredOption("-c, --cheese <type>", "pizza cheese type");
    parseUser(cmd, {"--cheese", "mozzarella"});
    auto o = cmd.opts();
    EXPECT_EQ(o["cheese"].asString(), "mozzarella");
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
    ParseFn parser = [](const std::string& v, const polycpp::JsonValue&) -> polycpp::JsonValue {
        return polycpp::JsonValue(std::stoi(v));
    };
    cmd.option("-p, --port <number>", "port number", parser);
    parseUser(cmd, {"--port", "3000"});
    auto o = cmd.opts();
    EXPECT_EQ(o["port"].asInt(), 3000);
}

TEST(CommandTest, OptionChoicesValid) {
    Command cmd;
    cmd.exitOverride();
    auto opt = Option("-s, --size <value>", "size");
    opt.choices({"small", "medium", "large"});
    cmd.addOption(std::move(opt));
    parseUser(cmd, {"--size", "medium"});
    auto o = cmd.opts();
    EXPECT_EQ(o["size"].asString(), "medium");
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
    opt.implies({{"logLevel", polycpp::JsonValue("debug")}});
    cmd.addOption(std::move(opt));
    cmd.option("--log-level <level>", "log level", polycpp::JsonValue("info"));
    parseUser(cmd, {"--verbose"});
    auto o = cmd.opts();
    EXPECT_EQ(o["logLevel"].asString(), "debug");
}

TEST(CommandTest, OptionPreset) {
    Command cmd;
    cmd.exitOverride();
    auto opt = Option("--color [when]", "use color");
    opt.defaultValue(polycpp::JsonValue("auto"));
    opt.preset(polycpp::JsonValue("always"));
    cmd.addOption(std::move(opt));
    // Without option-argument, preset value should be used
    parseUser(cmd, {"--color"});
    auto o = cmd.opts();
    EXPECT_EQ(o["color"].asString(), "always");
}

// ============ Arguments ============

TEST(CommandTest, PositionalArgument) {
    Command cmd;
    cmd.exitOverride();
    cmd.argument("<file>", "input file");
    cmd.action([](const std::vector<polycpp::JsonValue>& args, const polycpp::JsonValue&, Command&) {
        EXPECT_EQ(args[0].asString(), "test.txt");
    });
    parseUser(cmd, {"test.txt"});
}

TEST(CommandTest, MissingRequiredArgument) {
    Command cmd;
    cmd.exitOverride();
    cmd.argument("<file>", "input file");
    cmd.action([](const std::vector<polycpp::JsonValue>&, const polycpp::JsonValue&, Command&) {});
    EXPECT_THROW({
        parseUser(cmd, {});
    }, CommanderError);
}

TEST(CommandTest, VariadicArgument) {
    Command cmd;
    cmd.exitOverride();
    cmd.argument("<files...>", "input files");
    cmd.action([](const std::vector<polycpp::JsonValue>& args, const polycpp::JsonValue&, Command&) {
        ASSERT_TRUE(args[0].isArray());
        auto& files = args[0].asArray();
        ASSERT_EQ(files.size(), 3u);
        EXPECT_EQ(files[0].asString(), "a.txt");
        EXPECT_EQ(files[1].asString(), "b.txt");
        EXPECT_EQ(files[2].asString(), "c.txt");
    });
    parseUser(cmd, {"a.txt", "b.txt", "c.txt"});
}

TEST(CommandTest, ExcessArguments) {
    Command cmd;
    cmd.exitOverride();
    cmd.argument("<file>", "input file");
    cmd.action([](const std::vector<polycpp::JsonValue>&, const polycpp::JsonValue&, Command&) {});
    EXPECT_THROW({
        parseUser(cmd, {"a.txt", "b.txt"});
    }, CommanderError);
}

TEST(CommandTest, AllowExcessArguments) {
    Command cmd;
    cmd.exitOverride();
    cmd.argument("<file>", "input file");
    cmd.allowExcessArguments();
    cmd.action([](const std::vector<polycpp::JsonValue>&, const polycpp::JsonValue&, Command&) {});
    EXPECT_NO_THROW(parseUser(cmd, {"a.txt", "b.txt"}));
}

TEST(CommandTest, ArgumentsFromString) {
    Command cmd;
    cmd.exitOverride();
    cmd.arguments("<source> [destination]");
    cmd.action([](const std::vector<polycpp::JsonValue>& args, const polycpp::JsonValue&, Command&) {
        EXPECT_EQ(args[0].asString(), "src");
        EXPECT_EQ(args[1].asString(), "dst");
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
       .action([&](const std::vector<polycpp::JsonValue>&, const polycpp::JsonValue&, Command&) { called = true; });
    parseUser(cmd, {"serve"});
    EXPECT_TRUE(called);
}

TEST(CommandTest, SubcommandWithArgs) {
    Command cmd;
    cmd.exitOverride();
    std::string capturedSrc;
    cmd.command("clone <source> [destination]")
       .description("clone a repo")
       .action([&](const std::vector<polycpp::JsonValue>& args, const polycpp::JsonValue&, Command&) {
           capturedSrc = args[0].asString();
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
       .action([&](const std::vector<polycpp::JsonValue>&, const polycpp::JsonValue& opts, Command&) {
           capturedPort = std::stoi(opts.asObject().at("port").asString());
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
       .action([&](const std::vector<polycpp::JsonValue>&, const polycpp::JsonValue&, Command&) { called = true; });
    parseUser(cmd, {"i"});
    EXPECT_TRUE(called);
}

TEST(CommandTest, UnknownCommand) {
    Command cmd;
    cmd.exitOverride();
    cmd.command("serve").action([](const std::vector<polycpp::JsonValue>&, const polycpp::JsonValue&, Command&) {});
    EXPECT_THROW({
        parseUser(cmd, {"strat"});
    }, CommanderError);
}

TEST(CommandTest, UnknownCommandWithSuggestion) {
    Command cmd;
    cmd.exitOverride();
    cmd.command("serve").action([](const std::vector<polycpp::JsonValue>&, const polycpp::JsonValue&, Command&) {});
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
    cmd.action([&](const std::vector<polycpp::JsonValue>& args, const polycpp::JsonValue& opts, Command& command) {
        actionCalled = true;
        EXPECT_EQ(args[0].asString(), "Alice");
        EXPECT_TRUE(opts.asObject().at("verbose").asBool());
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
    cmd.action([](const std::vector<polycpp::JsonValue>&, const polycpp::JsonValue&, Command&) {});
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
    cmd.action([](const std::vector<polycpp::JsonValue>&, const polycpp::JsonValue&, Command&) {});
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
    cmd.action([](const std::vector<polycpp::JsonValue>&, const polycpp::JsonValue&, Command&) {});
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
    cmd.action([&](const std::vector<polycpp::JsonValue>&, const polycpp::JsonValue&, Command&) { order.push_back("action"); });
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
    cmd.action([&](const std::vector<polycpp::JsonValue>&, const polycpp::JsonValue&, Command&) { order.push_back("action"); });
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
       .action([](const std::vector<polycpp::JsonValue>&, const polycpp::JsonValue&, Command&) {});
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
       .action([&](const std::vector<polycpp::JsonValue>&, const polycpp::JsonValue&, Command& c) {
           auto allOpts = c.optsWithGlobals();
           if (allOpts.hasKey("verbose") && !allOpts["verbose"].isNull()) {
               captured++;
           }
           if (allOpts.hasKey("port") && !allOpts["port"].isNull()) {
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
    cmd.action([](const std::vector<polycpp::JsonValue>&, const polycpp::JsonValue&, Command&) {});
    cmd.parse({"node", "app.js", "--verbose"});
    auto o = cmd.opts();
    EXPECT_TRUE(o["verbose"].asBool());
}

// ============ CopyInheritedSettings ============

TEST(CommandTest, CopyInheritedSettings) {
    Command parent;
    parent.exitOverride();
    parent.showSuggestionAfterError(false);

    Command child("sub");
    child.copyInheritedSettings(parent);
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
    cmd.configureHelp({{"sortOptions", polycpp::JsonValue(true)}});
    cmd.option("-z, --zeta", "zeta option");
    cmd.option("-a, --alpha", "alpha option");

    auto helpText = cmd.helpInformation();
    auto alphaPos = helpText.find("--alpha");
    auto zetaPos = helpText.find("--zeta");
    EXPECT_LT(alphaPos, zetaPos);
}

// ============ ShowSuggestionAfterError ============

TEST(CommandTest, ShowSuggestionDisabled) {
    Command cmd;
    cmd.exitOverride();
    cmd.showSuggestionAfterError(false);
    cmd.command("serve").action([](const std::vector<polycpp::JsonValue>&, const polycpp::JsonValue&, Command&) {});
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
    cmd.setOptionValue("foo", polycpp::JsonValue("bar"));
    auto val = cmd.getOptionValue("foo");
    EXPECT_EQ(val.asString(), "bar");
}

TEST(CommandTest, GetOptionValueSource) {
    Command cmd;
    cmd.setOptionValueWithSource("foo", polycpp::JsonValue(42), "cli");
    EXPECT_EQ(cmd.getOptionValueSource("foo"), "cli");
}

// ============ Double-dash terminates options ============

TEST(CommandTest, DoubleDashTerminatesOptions) {
    Command cmd;
    cmd.exitOverride();
    cmd.option("-v, --verbose", "verbose");
    cmd.argument("[args...]", "remaining args");
    cmd.action([](const std::vector<polycpp::JsonValue>&, const polycpp::JsonValue&, Command&) {});
    parseUser(cmd, {"--", "--verbose"});
    auto o = cmd.opts();
    // verbose should NOT be set (it was after --)
    EXPECT_TRUE(!o.hasKey("verbose") || o["verbose"].isNull() ||
                (o["verbose"].isBool() && !o["verbose"].asBool()));
}

// ============ Help Subcommand ============

TEST(CommandTest, HelpCommandShowsRootHelpWhenNoSubcommand) {
    Command cmd;
    cmd.exitOverride();
    cmd.name("myapp");
    cmd.command("sub1").action([](const std::vector<polycpp::JsonValue>&, const polycpp::JsonValue&, Command&) {});
    std::string captured;
    cmd.configureOutput({.writeOut = [&](const std::string& s) { captured += s; }});
    try {
        parseUser(cmd, {"help"});
    } catch (const CommanderError& e) {
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
    cmd.command("foo").action([](const std::vector<polycpp::JsonValue>&, const polycpp::JsonValue&, Command&) {});
    auto helpText = cmd.helpInformation();
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
    cmd.command("sub1").action([](const std::vector<polycpp::JsonValue>&, const polycpp::JsonValue&, Command&) {});
    cmd.action([](const std::vector<polycpp::JsonValue>&, const polycpp::JsonValue&, Command&) {});
    auto helpText = cmd.helpInformation();
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
    Command customHelp("aide");
    customHelp.description("custom help command");
    customHelp.helpOption(false);
    customHelp.arguments("[cmd]");
    cmd.addHelpCommand(std::move(customHelp));
    auto helpText = cmd.helpInformation();
    EXPECT_NE(helpText.find("aide"), std::string::npos);
    EXPECT_NE(helpText.find("custom help command"), std::string::npos);
}

TEST(CommandTest, HelpCommandDispatchesUnknownSubcommand) {
    Command cmd;
    cmd.exitOverride();
    cmd.name("myapp");
    cmd.command("sub1").action([](const std::vector<polycpp::JsonValue>&, const polycpp::JsonValue&, Command&) {});
    std::string errCaptured;
    cmd.configureOutput({
        .writeOut = [](const std::string&) {},
        .writeErr = [&](const std::string& s) { errCaptured += s; }
    });
    try {
        parseUser(cmd, {"help", "unknown"});
    } catch (const CommanderError&) {
    }
}

TEST(CommandTest, HelpCommandInheritedBySubcommand) {
    Command cmd;
    cmd.exitOverride();
    cmd.name("myapp");
    cmd.helpCommand(false);

    auto& sub = cmd.command("sub1");
    sub.command("nested").action([](const std::vector<polycpp::JsonValue>&, const polycpp::JsonValue&, Command&) {});
    auto helpText = sub.helpInformation();
    EXPECT_EQ(helpText.find("help [command]"), std::string::npos);
}

// ============ Executable Subcommands ============

TEST(CommandTest, ExecutableSubcommandDefined) {
    Command cmd("myapp");
    cmd.exitOverride();
    cmd.executableCommand("install", "install packages");
    EXPECT_EQ(cmd.commands().size(), 1u);
}

TEST(CommandTest, ExecutableSubcommandReturnsThis) {
    Command cmd("myapp");
    cmd.exitOverride();
    auto& ret = cmd.executableCommand("install", "install packages");
    EXPECT_EQ(&ret, &cmd);
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

#if !defined(_WIN32)

TEST(CommandTest, ExecutableSubcommandRun) {
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

    std::ifstream in(output);
    std::string content;
    std::getline(in, content);
    EXPECT_EQ(content, "hello world");

    std::filesystem::remove(output);
    std::filesystem::remove(script);
}

#endif

TEST(CommandTest, ExecutableSubcommandInHelp) {
    Command cmd("myapp");
    cmd.exitOverride();
    cmd.executableCommand("install", "install packages");
    auto help = cmd.helpInformation();
    EXPECT_TRUE(help.find("install") != std::string::npos);
    EXPECT_TRUE(help.find("install packages") != std::string::npos);
}

#if !defined(_WIN32)

TEST(CommandTest, ExecutableSubcommandCustomExecutable) {
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

#endif

TEST(CommandTest, ExecutableSubcommandWithArguments) {
    Command cmd("myapp");
    cmd.exitOverride();
    cmd.executableCommand("install <pkg>", "install a package");
    EXPECT_EQ(cmd.commands().size(), 1u);
    EXPECT_EQ(cmd.commands()[0].name(), "install");
    EXPECT_EQ(cmd.commands()[0].registeredArguments().size(), 1u);
}

// ---- parseAsync tests ----

TEST(CommandTest, ParseAsyncBasicAction) {
    Command cmd("app");
    cmd.exitOverride();
    bool called = false;
    cmd.option("-v, --verbose", "verbose");
    cmd.actionAsync([&](const std::vector<polycpp::JsonValue>& args,
                         const polycpp::JsonValue& opts, Command&) {
        called = true;
        EXPECT_TRUE(opts["verbose"].asBool());
        return polycpp::Promise<void>::resolve();
    });

    auto promise = cmd.parseAsync({"--verbose"}, {.from = "user"});
    polycpp::EventLoop::instance().run();
    EXPECT_TRUE(called);
}

TEST(CommandTest, ParseAsyncSyncAction) {
    Command cmd("app");
    cmd.exitOverride();
    bool called = false;
    cmd.action([&](const std::vector<polycpp::JsonValue>& args,
                    const polycpp::JsonValue& opts, Command&) {
        called = true;
    });

    auto promise = cmd.parseAsync({}, {.from = "user"});
    polycpp::EventLoop::instance().run();
    EXPECT_TRUE(called);
}

TEST(CommandTest, ParseAsyncHookOrder) {
    Command cmd("app");
    cmd.exitOverride();
    std::vector<std::string> order;

    cmd.hook("preAction", [&](Command&, Command&) {
        order.push_back("syncPre");
    });
    cmd.hookAsync("preAction", [&](Command&, Command&) {
        order.push_back("asyncPre");
        return polycpp::Promise<void>::resolve();
    });
    cmd.action([&](const std::vector<polycpp::JsonValue>&,
                    const polycpp::JsonValue&, Command&) {
        order.push_back("action");
    });
    cmd.hook("postAction", [&](Command&, Command&) {
        order.push_back("syncPost");
    });
    cmd.hookAsync("postAction", [&](Command&, Command&) {
        order.push_back("asyncPost");
        return polycpp::Promise<void>::resolve();
    });

    cmd.parseAsync({}, {.from = "user"});
    polycpp::EventLoop::instance().run();

    ASSERT_EQ(order.size(), 5u);
    EXPECT_EQ(order[0], "syncPre");
    EXPECT_EQ(order[1], "asyncPre");
    EXPECT_EQ(order[2], "action");
    // postAction hooks run in reverse registration order
    EXPECT_EQ(order[3], "asyncPost");
    EXPECT_EQ(order[4], "syncPost");
}

TEST(CommandTest, ParseAsyncWithArgs) {
    Command cmd("app");
    cmd.exitOverride();
    std::string capturedFile;
    cmd.argument("<file>", "input file");
    cmd.actionAsync([&](const std::vector<polycpp::JsonValue>& args,
                         const polycpp::JsonValue& opts, Command&) {
        capturedFile = args[0].asString();
        return polycpp::Promise<void>::resolve();
    });

    cmd.parseAsync({"test.txt"}, {.from = "user"});
    polycpp::EventLoop::instance().run();
    EXPECT_EQ(capturedFile, "test.txt");
}

TEST(CommandTest, ParseAsyncSubcommand) {
    Command cmd("app");
    cmd.exitOverride();
    bool called = false;
    cmd.command("serve")
        .actionAsync([&](const std::vector<polycpp::JsonValue>&,
                          const polycpp::JsonValue&, Command&) {
            called = true;
            return polycpp::Promise<void>::resolve();
        });

    cmd.parseAsync({"serve"}, {.from = "user"});
    polycpp::EventLoop::instance().run();
    EXPECT_TRUE(called);
}

TEST(CommandTest, ParseAsyncReturnsCommand) {
    Command cmd("app");
    cmd.exitOverride();
    cmd.option("-n, --name <name>", "name");
    cmd.actionAsync([](const std::vector<polycpp::JsonValue>&,
                        const polycpp::JsonValue&, Command&) {
        return polycpp::Promise<void>::resolve();
    });

    Command* resultCmd = nullptr;
    cmd.parseAsync({"--name", "Alice"}, {.from = "user"})
        .then([&](std::reference_wrapper<Command> ref) {
            resultCmd = &ref.get();
        });
    polycpp::EventLoop::instance().run();

    ASSERT_NE(resultCmd, nullptr);
    EXPECT_EQ(resultCmd->opts()["name"].asString(), "Alice");
}

TEST(CommandTest, SyncParseStillWorksAfterAsyncAdded) {
    Command cmd("app");
    cmd.exitOverride();
    bool called = false;
    cmd.action([&](const std::vector<polycpp::JsonValue>&,
                    const polycpp::JsonValue&, Command&) { called = true; });
    cmd.parse({}, {.from = "user"});
    EXPECT_TRUE(called);
}

TEST(CommandTest, ParseAsyncAsyncActionHandler) {
    Command cmd("app");
    cmd.exitOverride();
    std::string result;
    cmd.actionAsync([&](const std::vector<polycpp::JsonValue>&,
                         const polycpp::JsonValue&, Command&) {
        return polycpp::Promise<void>([&](auto resolve, auto) {
            result = "async_done";
            resolve();
        });
    });

    cmd.parseAsync({}, {.from = "user"});
    polycpp::EventLoop::instance().run();
    EXPECT_EQ(result, "async_done");
}

TEST(CommandTest, HookAsyncValidation) {
    Command cmd("app");
    EXPECT_THROW(cmd.hookAsync("invalid", [](Command&, Command&) {
        return polycpp::Promise<void>::resolve();
    }), std::runtime_error);
}

TEST(CommandTest, ActionAsyncReturnsThis) {
    Command cmd("app");
    auto& ref = cmd.actionAsync([](const std::vector<polycpp::JsonValue>&,
                                    const polycpp::JsonValue&, Command&) {
        return polycpp::Promise<void>::resolve();
    });
    EXPECT_EQ(&ref, &cmd);
}

// ============ Parent (weak_ptr) lifetime tests ============

TEST(CommandTest, ParentReturnsNulloptForRoot) {
    Command prog;
    EXPECT_FALSE(prog.hasParent());
    EXPECT_FALSE(prog.parent().has_value());
}

TEST(CommandTest, ParentReturnsAliveParent) {
    Command prog("app");
    auto sub = prog.command("sub");
    ASSERT_TRUE(sub.hasParent());
    auto parent = sub.parent();
    ASSERT_TRUE(parent.has_value());
    EXPECT_EQ(parent->name(), "app");
}

TEST(CommandTest, ParentReturnsNulloptAfterParentDestroyed) {
    // Reproduces the dangling-pointer bug that motivated weak_ptr migration.
    // Copy a child handle out of its parent, then drop the parent. The
    // child's parent() must report nullopt rather than dereference a
    // freed Command::Impl.
    std::optional<Command> sub;
    {
        Command prog("app");
        prog.command("sub");
        sub = prog.commands().front();   // copy the child handle out
    }
    ASSERT_TRUE(sub.has_value());
    EXPECT_FALSE(sub->hasParent());
    EXPECT_FALSE(sub->parent().has_value());
}

TEST(CommandTest, ParentChainSurvivesIntermediateAddCommand) {
    // Inserting more siblings into the parent's deque must not invalidate
    // the parent_ weak_ptr — the parent's Impl lifetime is untouched by
    // deque growth.
    Command prog("app");
    auto first = prog.command("first");
    prog.command("second");
    prog.command("third");
    ASSERT_TRUE(first.hasParent());
    EXPECT_EQ(first.parent()->name(), "app");
}

TEST(CommandTest, OptsWithGlobalsStillReachesRootAncestor) {
    // Sanity check that ancestor-walking code paths still work end-to-end
    // after the weak_ptr migration. optsWithGlobals walks parent chain.
    Command prog("app");
    prog.option("--global");
    Command sub = prog.command("sub");
    sub.action([](const std::vector<polycpp::JsonValue>&,
                  const polycpp::JsonValue&, Command&) {});
    prog.parse({"--global", "sub"}, {.from = "user"});
    auto opts = sub.optsWithGlobals();
    EXPECT_TRUE(opts["global"].isBool());
    EXPECT_TRUE(opts["global"].asBool());
}
