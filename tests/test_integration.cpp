#include <polycpp/commander/detail/aggregator.hpp>
#include <gtest/gtest.h>

using namespace polycpp::commander;

// ============ End-to-end Integration Tests ============

TEST(IntegrationTest, BasicCLIProgram) {
    Command program;
    program.exitOverride();
    program.name("myapp");
    program.version("1.0.0");
    program.description("My test CLI application");
    program.option("-v, --verbose", "enable verbose output");
    program.option("-o, --output <path>", "output path");
    program.argument("<input>", "input file");

    bool actionCalled = false;
    std::string capturedInput;
    std::string capturedOutput;
    bool capturedVerbose = false;

    program.action([&](auto& args, auto& opts, auto&) {
        actionCalled = true;
        capturedInput = std::any_cast<std::string>(args[0]);
        if (opts.count("output") && opts.at("output").has_value() &&
            opts.at("output").type() == typeid(std::string)) {
            capturedOutput = std::any_cast<std::string>(opts.at("output"));
        }
        if (opts.count("verbose") && opts.at("verbose").has_value() &&
            opts.at("verbose").type() == typeid(bool)) {
            capturedVerbose = std::any_cast<bool>(opts.at("verbose"));
        }
    });

    program.parse({"node", "myapp", "--verbose", "-o", "out.txt", "input.txt"});

    EXPECT_TRUE(actionCalled);
    EXPECT_EQ(capturedInput, "input.txt");
    EXPECT_EQ(capturedOutput, "out.txt");
    EXPECT_TRUE(capturedVerbose);
}

TEST(IntegrationTest, SubcommandProgram) {
    Command program;
    program.exitOverride();
    program.name("git");
    program.option("-v, --verbose", "verbose output");

    bool cloneCalled = false;
    std::string clonedUrl;

    program.command("clone <url> [dir]")
           .description("clone a repository")
           .option("-b, --branch <name>", "branch to clone")
           .action([&](auto& args, auto& opts, auto&) {
               cloneCalled = true;
               clonedUrl = std::any_cast<std::string>(args[0]);
           });

    bool pushCalled = false;
    program.command("push")
           .description("push changes")
           .option("-f, --force", "force push")
           .action([&](auto& args, auto& opts, auto&) {
               pushCalled = true;
           });

    program.parse({"node", "git", "clone", "https://github.com/foo/bar"});

    EXPECT_TRUE(cloneCalled);
    EXPECT_FALSE(pushCalled);
    EXPECT_EQ(clonedUrl, "https://github.com/foo/bar");
}

TEST(IntegrationTest, MissingRequiredOptionError) {
    Command program;
    program.exitOverride();
    program.name("myapp");
    program.requiredOption("-c, --config <path>", "config file path");
    program.action([](auto&, auto&, auto&) {});

    std::string errorOutput;
    program.configureOutput({
        .writeErr = [&](const std::string& s) { errorOutput += s; }
    });

    try {
        program.parse({"node", "myapp"}, {.from = "node"});
        FAIL() << "Expected CommanderError";
    } catch (const CommanderError& e) {
        EXPECT_EQ(e.code, "commander.missingMandatoryOptionValue");
        EXPECT_NE(errorOutput.find("--config"), std::string::npos);
    }
}

TEST(IntegrationTest, HelpOutputContent) {
    Command program;
    program.exitOverride();
    program.name("myapp");
    program.version("2.5.0");
    program.description("A multi-purpose CLI tool");
    program.option("-d, --debug", "enable debug mode");
    program.option("-o, --output <path>", "output file path");
    program.argument("[source]", "source directory");

    program.command("init")
           .description("initialize a new project");
    program.command("build")
           .description("build the project");

    auto helpText = program.helpInformation();

    // Check all sections are present
    EXPECT_NE(helpText.find("Usage:"), std::string::npos);
    EXPECT_NE(helpText.find("myapp"), std::string::npos);
    EXPECT_NE(helpText.find("A multi-purpose CLI tool"), std::string::npos);
    EXPECT_NE(helpText.find("Arguments:"), std::string::npos);
    EXPECT_NE(helpText.find("source"), std::string::npos);
    EXPECT_NE(helpText.find("Options:"), std::string::npos);
    EXPECT_NE(helpText.find("--debug"), std::string::npos);
    EXPECT_NE(helpText.find("--output"), std::string::npos);
    EXPECT_NE(helpText.find("--help"), std::string::npos);
    EXPECT_NE(helpText.find("Commands:"), std::string::npos);
    EXPECT_NE(helpText.find("init"), std::string::npos);
    EXPECT_NE(helpText.find("build"), std::string::npos);
}

TEST(IntegrationTest, VersionFromHelpText) {
    Command program;
    program.exitOverride();
    program.name("myapp");
    program.version("3.0.0");

    auto helpText = program.helpInformation();
    // Version option should appear in help
    EXPECT_NE(helpText.find("--version"), std::string::npos);
}

TEST(IntegrationTest, NestedSubcommands) {
    Command program;
    program.exitOverride();
    program.name("app");

    bool innerCalled = false;
    auto& outer = program.command("remote")
                         .description("manage remotes");
    outer.command("add <name> <url>")
         .description("add a remote")
         .action([&](auto& args, auto&, auto&) {
             innerCalled = true;
             EXPECT_EQ(std::any_cast<std::string>(args[0]), "origin");
             EXPECT_EQ(std::any_cast<std::string>(args[1]), "https://example.com");
         });

    program.parse({"node", "app", "remote", "add", "origin", "https://example.com"});
    EXPECT_TRUE(innerCalled);
}

TEST(IntegrationTest, FactoryFunctions) {
    auto cmd = createCommand("myapp");
    EXPECT_EQ(cmd->name(), "myapp");

    auto opt = createOption("-v, --verbose", "verbose");
    EXPECT_EQ(opt.name(), "verbose");

    auto arg = createArgument("<file>", "input file");
    EXPECT_EQ(arg.name(), "file");
}

TEST(IntegrationTest, MultipleParseCalls) {
    Command program;
    program.exitOverride();
    program.name("myapp");
    program.option("-v, --verbose", "verbose");
    program.action([](auto&, auto&, auto&) {});

    // First parse
    program.parse({"node", "myapp", "--verbose"}, {.from = "node"});
    EXPECT_TRUE(std::any_cast<bool>(program.opts()["verbose"]));

    // Second parse should reset state
    program.parse({"node", "myapp"}, {.from = "node"});
    // verbose should be back to default (not set)
    auto o = program.opts();
    EXPECT_FALSE(o.count("verbose") && o["verbose"].has_value() &&
                 o["verbose"].type() == typeid(bool) && std::any_cast<bool>(o["verbose"]));
}

TEST(IntegrationTest, ErrorOutputConfiguration) {
    Command program;
    program.exitOverride();
    program.name("myapp");

    std::string errOutput;
    std::string stdOutput;
    program.configureOutput({
        .writeOut = [&](const std::string& s) { stdOutput += s; },
        .writeErr = [&](const std::string& s) { errOutput += s; }
    });

    try {
        program.error("something went wrong", {.exitCode = 1, .code = "test.error"});
    } catch (const CommanderError&) {}

    EXPECT_NE(errOutput.find("something went wrong"), std::string::npos);
    EXPECT_TRUE(stdOutput.empty());
}
