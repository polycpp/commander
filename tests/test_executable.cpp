/**
 * @file test_executable.cpp
 * @brief Comprehensive POSIX coverage for stand-alone executable subcommands.
 *
 * Targets `polycpp::commander::Command::executableCommand(...)` and the
 * dispatch path that follows it: `dispatchSubcommand_` ->
 * `executeSubCommand_` -> `findProgram_` -> `polycpp::child_process::spawn`.
 *
 * The cases here are adapted from upstream commander.js's
 * `tests/command.executableSubcommand.*.test.js` cluster (lookup, search,
 * mock, signals, plus the umbrella `command.executableSubcommand.test.js`).
 * Windows-specific scenarios (`.cmd`/`.bat` shim lookup, `node --inspect`
 * argv rewriting) are intentionally skipped — see `docs/divergences.md`.
 *
 * Fixture programs (built alongside this test target by CMake) live in
 * `${CMAKE_BINARY_DIR}/test_fixtures/`:
 *   - `echo_argv`       — echoes argv (one `arg:`-prefixed line per entry)
 *                         to the file named by `$ECHO_ARGV_OUTPUT`.
 *   - `exit_with_code`  — `exit(atoi(argv[1]))`, defaults to 0.
 *   - `signal_writer`   — installs SIGTERM/SIGINT/SIGHUP/SIGUSR1/SIGUSR2
 *                         handlers, writes the signal name to
 *                         `$SIGNAL_WRITER_OUTPUT` and exits with `128+signum`.
 *                         Used to verify parent → child signal forwarding.
 *
 * Tests that need the parent program to spawn `<prog>-<sub>` create a
 * symlink to one of those fixtures inside a per-test temp directory and
 * then point `executableDir(...)` at that directory.
 *
 * @see https://github.com/tj/commander.js/tree/master/tests
 */

#include <polycpp/commander/detail/aggregator.hpp>
#include <polycpp/event_loop.hpp>
#include <polycpp/process/process.hpp>
#include <gtest/gtest.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <csignal>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <sys/stat.h>
#include <sys/types.h>
#include <thread>
#include <unistd.h>
#include <vector>

#ifndef POLYCPP_COMMANDER_TEST_FIXTURE_DIR
#error "POLYCPP_COMMANDER_TEST_FIXTURE_DIR must be defined by the build system"
#endif

using namespace polycpp::commander;

namespace {

namespace fs = std::filesystem;

/// @brief Absolute path to the directory containing the compiled fixtures.
constexpr const char* kFixtureDir = POLYCPP_COMMANDER_TEST_FIXTURE_DIR;

/**
 * @brief Build the absolute path to a fixture binary by name.
 * @param name The fixture executable's file name (e.g. "echo_argv").
 * @return Absolute path string.
 */
std::string fixturePath(const std::string& name) {
    return std::string(kFixtureDir) + "/" + name;
}

/**
 * @brief Build a unique sandbox path under `/tmp` for a single test case.
 *
 * The path embeds the gtest test/suite name plus the process pid so that
 * concurrent ctest -j runs do not collide. The returned path is created on
 * disk and removed by `Sandbox::~Sandbox`.
 */
class Sandbox {
public:
    Sandbox() {
        auto* info = ::testing::UnitTest::GetInstance()->current_test_info();
        std::string base = "polycpp-commander-exec-";
        if (info) {
            base += info->test_suite_name();
            base += "-";
            base += info->name();
        } else {
            base += "noinfo";
        }
        base += "-" + std::to_string(::getpid());
        // A monotonically increasing per-process counter ensures multiple
        // Sandbox instances within the SAME test case get distinct paths.
        static std::atomic<unsigned> kSeq{0};
        base += "-" + std::to_string(kSeq.fetch_add(1, std::memory_order_relaxed));
        path_ = fs::temp_directory_path() / base;
        fs::remove_all(path_);
        fs::create_directories(path_);
    }

    Sandbox(const Sandbox&) = delete;
    Sandbox& operator=(const Sandbox&) = delete;

    ~Sandbox() {
        std::error_code ec;
        fs::remove_all(path_, ec);
    }

    /// @brief Absolute sandbox directory path.
    const fs::path& path() const noexcept { return path_; }

    /// @brief Convenience: return `path() / leaf` as an absolute string.
    std::string file(const std::string& leaf) const {
        return (path_ / leaf).string();
    }

    /**
     * @brief Place a copy of fixture `src` under `dstName` in the sandbox.
     * @param src     Fixture file name (e.g. "echo_argv").
     * @param dstName Destination basename inside the sandbox.
     * @return Absolute path to the copy, ready to be exec'd.
     *
     * Uses copy_file rather than symlinks so that
     * `polycpp::fs::realpathSync` (called by `executeSubCommand_`) resolves
     * to a path inside the sandbox itself, keeping the search behaviour
     * predictable across kernels with different symlink semantics.
     */
    std::string installFixture(const std::string& src, const std::string& dstName) const {
        fs::path dst = path_ / dstName;
        fs::copy_file(fixturePath(src), dst, fs::copy_options::overwrite_existing);
        fs::permissions(dst,
                        fs::perms::owner_all | fs::perms::group_read |
                            fs::perms::group_exec | fs::perms::others_read |
                            fs::perms::others_exec);
        return dst.string();
    }

private:
    fs::path path_;
};

/**
 * @brief RAII guard for temporarily mutating `$PATH`.
 *
 * Constructor records the current PATH value and overwrites it via
 * `polycpp::process::setenv` (which is what `findProgram_` reads through
 * `polycpp::process::getenv`). Destructor restores the original value, even
 * when the test body throws a `CommanderError`.
 */
class PathGuard {
public:
    explicit PathGuard(const std::string& newPath) {
        original_ = polycpp::process::getenv("PATH");
        polycpp::process::setenv("PATH", newPath);
    }

    PathGuard(const PathGuard&) = delete;
    PathGuard& operator=(const PathGuard&) = delete;

    ~PathGuard() {
        polycpp::process::setenv("PATH", original_);
    }

private:
    std::string original_;
};

/**
 * @brief RAII guard for temporarily setting `$ECHO_ARGV_OUTPUT`.
 *
 * Sets the env var on construction and unsets on destruction so concurrent
 * tests that don't expect the variable to be present aren't affected by
 * leftover state. Pairs with the `echo_argv` fixture.
 */
class EchoSinkGuard {
public:
    explicit EchoSinkGuard(const std::string& path) {
        polycpp::process::setenv("ECHO_ARGV_OUTPUT", path);
    }

    EchoSinkGuard(const EchoSinkGuard&) = delete;
    EchoSinkGuard& operator=(const EchoSinkGuard&) = delete;

    ~EchoSinkGuard() {
        polycpp::process::unsetenv("ECHO_ARGV_OUTPUT");
    }
};

/**
 * @brief Read the contents of `path` line-by-line as the captured argv.
 *
 * Returns an empty vector when the file does not exist (caller asserts).
 */
std::vector<std::string> readEchoOutput(const std::string& path) {
    std::vector<std::string> lines;
    std::ifstream in(path);
    std::string line;
    while (std::getline(in, line)) {
        lines.push_back(std::move(line));
    }
    return lines;
}

/**
 * @brief Make a writable file with mode 644 (i.e. NOT executable).
 *
 * Used by `NotExecutableSearchContinues` to verify that
 * `findProgram_` skips entries that fail `accessSync(..., kX_OK)`.
 */
void writeNonExecutable(const std::string& path, const std::string& body) {
    std::ofstream f(path, std::ios::trunc);
    f << body;
    f.close();
    fs::permissions(path, fs::perms::owner_read | fs::perms::owner_write |
                              fs::perms::group_read | fs::perms::others_read);
}

/**
 * @brief Drain `cmd.parse(...)`, swallowing any thrown `CommanderError`.
 *
 * The dispatch path under test calls `error(...)` which under
 * `exitOverride()` raises `CommanderError`; some tests want to inspect the
 * caught error, others only want to know whether it was raised. This helper
 * captures whichever was thrown and returns it via the optional.
 */
std::optional<CommanderError> parseUserCatching(Command& cmd,
                                                std::vector<std::string> args) {
    try {
        cmd.parse(args, {.from = "user"});
        return std::nullopt;
    } catch (const CommanderError& e) {
        return e;
    }
}

/**
 * @brief Suppress writes to stderr / outputError so failing parses stay quiet
 * in the gtest console. Otherwise the dispatch helper noisily prints the
 * "<prog-sub> does not exist" diagnostic on every negative test.
 */
void quiet(Command& cmd) {
    cmd.configureOutput({.writeErr = [](const std::string&) {},
                         .outputError = [](const std::string&, auto) {}});
}

} // namespace

// ──────────────────────────────────────────────────────────────────────
// 1. Default name resolution: <programName>-<subcommandName>
// ──────────────────────────────────────────────────────────────────────

TEST(ExecutableSubcommand, DefaultNameIsProgramDashSub) {
    Sandbox sb;
    sb.installFixture("echo_argv", "tprog-greet");

    std::string out = sb.file("argv.out");
    EchoSinkGuard sink(out);

    Command prog("tprog");
    prog.exitOverride();
    prog.executableDir(sb.path().string());
    prog.executableCommand("greet", "say hi");

    auto err = parseUserCatching(prog, {"greet"});
    ASSERT_FALSE(err.has_value()) << err->what();

    auto lines = readEchoOutput(out);
    ASSERT_FALSE(lines.empty());
    EXPECT_NE(lines[0].find("tprog-greet"), std::string::npos)
        << "argv[0] should reflect the spawned program name; got: " << lines[0];
}

// ──────────────────────────────────────────────────────────────────────
// 2. Custom executableFile overrides default <prog>-<sub>
// ──────────────────────────────────────────────────────────────────────

TEST(ExecutableSubcommand, CustomExecutableFileOverridesDefault) {
    Sandbox sb;
    sb.installFixture("echo_argv", "custom-runner");

    std::string out = sb.file("argv.out");
    EchoSinkGuard sink(out);

    // programName "weird" + subcommand "greet" would default to "weird-greet";
    // the explicit executableFile MUST win.
    Command prog("weird");
    prog.exitOverride();
    prog.executableDir(sb.path().string());
    prog.executableCommand("greet", "say hi", "custom-runner");

    auto err = parseUserCatching(prog, {"greet"});
    ASSERT_FALSE(err.has_value()) << err->what();

    auto lines = readEchoOutput(out);
    ASSERT_FALSE(lines.empty());
    EXPECT_NE(lines[0].find("custom-runner"), std::string::npos);
    // No "weird-greet" anywhere in the captured argv.
    for (const auto& l : lines) {
        EXPECT_EQ(l.find("weird-greet"), std::string::npos);
    }
}

// ──────────────────────────────────────────────────────────────────────
// 3. Explicit executableDir wins over PATH
// ──────────────────────────────────────────────────────────────────────

TEST(ExecutableSubcommand, ExecutableDirWinsOverPath) {
    Sandbox local;
    Sandbox elsewhere;
    // Two fixtures with the same target name, in different dirs. The local
    // one writes "LOCAL", the PATH one writes "PATH". executableDir(local)
    // should win.
    std::string localCopy = local.installFixture("echo_argv", "tprog-which");
    std::string pathCopy = elsewhere.installFixture("echo_argv", "tprog-which");

    std::string out = local.file("argv.out");
    EchoSinkGuard sink(out);

    PathGuard guard(elsewhere.path().string() + ":" +
                    polycpp::process::getenv("PATH"));

    Command prog("tprog");
    prog.exitOverride();
    prog.executableDir(local.path().string());
    prog.executableCommand("which", "find me");

    auto err = parseUserCatching(prog, {"which"});
    ASSERT_FALSE(err.has_value()) << err->what();

    auto lines = readEchoOutput(out);
    ASSERT_FALSE(lines.empty());
    // argv[0] is the resolved path that was actually exec'd.
    EXPECT_EQ(lines[0], "arg:" + localCopy)
        << "Expected the local executableDir copy to win; got: " << lines[0];
    // Sanity: it is NOT the PATH copy.
    EXPECT_NE(lines[0], "arg:" + pathCopy);
}

// ──────────────────────────────────────────────────────────────────────
// 4. executableDir relative to scriptPath
// ──────────────────────────────────────────────────────────────────────

TEST(ExecutableSubcommand, ExecutableDirIsRelativeToScriptPath) {
    Sandbox sb;
    // Layout:
    //   <sb>/
    //     fakeprog            (the "script")
    //     subdir/
    //       fakeprog-task     (the executable subcommand target)
    fs::create_directories(sb.path() / "subdir");
    sb.installFixture("echo_argv", "subdir/fakeprog-task");

    // Create an empty placeholder for the "script" so realpathSync can
    // resolve it. Contents are irrelevant — commander only takes its dirname.
    std::ofstream(sb.file("fakeprog")).put('\n');

    std::string out = sb.file("argv.out");
    EchoSinkGuard sink(out);

    Command prog("fakeprog");
    prog.exitOverride();
    quiet(prog);
    // Relative path — the dispatch path joins this with dirname(scriptPath).
    prog.executableDir("subdir");
    prog.executableCommand("task", "do work");

    // node-mode parse: argv[1] is the script path, so scriptPath_ is set.
    try {
        prog.parse({"node", sb.file("fakeprog"), "task"});
    } catch (const CommanderError& e) {
        FAIL() << "unexpected CommanderError: " << e.what();
    }

    auto lines = readEchoOutput(out);
    ASSERT_FALSE(lines.empty());
    EXPECT_NE(lines[0].find("subdir/fakeprog-task"), std::string::npos)
        << "argv[0]: " << lines[0];
}

// ──────────────────────────────────────────────────────────────────────
// 5. Search falls back to PATH when not found in executableDir
// ──────────────────────────────────────────────────────────────────────

TEST(ExecutableSubcommand, FallsBackToPathSearch) {
    Sandbox sb;
    sb.installFixture("echo_argv", "pathprog-action");

    std::string out = sb.file("argv.out");
    EchoSinkGuard sink(out);

    PathGuard guard(sb.path().string() + ":" +
                    polycpp::process::getenv("PATH"));

    Command prog("pathprog");
    prog.exitOverride();
    // No executableDir set, no script-mode parse → only PATH lookup applies.
    prog.executableCommand("action", "act");

    auto err = parseUserCatching(prog, {"action"});
    ASSERT_FALSE(err.has_value()) << err->what();

    auto lines = readEchoOutput(out);
    ASSERT_FALSE(lines.empty());
    EXPECT_NE(lines[0].find("pathprog-action"), std::string::npos);
}

// ──────────────────────────────────────────────────────────────────────
// 6. Missing executable raises commander.executeSubCommandAsync
// ──────────────────────────────────────────────────────────────────────

TEST(ExecutableSubcommand, MissingExecutableRaisesError) {
    // Empty PATH + no executableDir + no fixture installed → guaranteed miss.
    PathGuard guard("");

    Command prog("ghostprog");
    prog.exitOverride();
    quiet(prog);
    prog.executableCommand("vanish", "no body");

    auto err = parseUserCatching(prog, {"vanish"});
    ASSERT_TRUE(err.has_value()) << "expected a CommanderError";
    EXPECT_EQ(err->code, "commander.executeSubCommandAsync");
    EXPECT_EQ(err->exitCode, 1);
}

// ──────────────────────────────────────────────────────────────────────
// 7. Operands forwarded to child
// ──────────────────────────────────────────────────────────────────────

TEST(ExecutableSubcommand, OperandsForwardedToChild) {
    Sandbox sb;
    sb.installFixture("echo_argv", "opprog-runop");

    std::string out = sb.file("argv.out");
    EchoSinkGuard sink(out);

    Command prog("opprog");
    prog.exitOverride();
    prog.executableDir(sb.path().string());
    prog.executableCommand("runop", "run with operands");

    auto err = parseUserCatching(prog, {"runop", "alpha", "beta", "gamma"});
    ASSERT_FALSE(err.has_value()) << err->what();

    auto lines = readEchoOutput(out);
    // Expected layout: argv[0] = path, argv[1..3] = the operands.
    ASSERT_GE(lines.size(), 4u);
    EXPECT_EQ(lines[1], "arg:alpha");
    EXPECT_EQ(lines[2], "arg:beta");
    EXPECT_EQ(lines[3], "arg:gamma");
}

// ──────────────────────────────────────────────────────────────────────
// 8. Unknown args forwarded to child (operands first, then unknown)
// ──────────────────────────────────────────────────────────────────────

TEST(ExecutableSubcommand, UnknownArgsForwardedAfterOperands) {
    Sandbox sb;
    sb.installFixture("echo_argv", "uprog-mix");

    std::string out = sb.file("argv.out");
    EchoSinkGuard sink(out);

    Command prog("uprog");
    prog.exitOverride();
    prog.executableDir(sb.path().string());
    prog.executableCommand("mix", "mix args");
    // Note: the parent doesn't declare --weird, so it lands in `unknown`.
    auto err = parseUserCatching(prog,
        {"mix", "operandA", "--weird", "value", "operandB"});
    ASSERT_FALSE(err.has_value()) << err->what();

    auto lines = readEchoOutput(out);
    ASSERT_GE(lines.size(), 5u);
    // argv[0] = path; the rest are operands first, unknown second.
    // commander.js groups operandA + operandB as operands and the --weird
    // value pair as unknown.
    std::vector<std::string> tail(lines.begin() + 1, lines.end());
    auto find = [&](const std::string& v) {
        return std::find(tail.begin(), tail.end(), "arg:" + v);
    };
    auto a = find("operandA");
    auto b = find("operandB");
    auto w = find("--weird");
    ASSERT_NE(a, tail.end());
    ASSERT_NE(b, tail.end());
    ASSERT_NE(w, tail.end());
    // operands come before unknowns.
    EXPECT_LT(std::min(a, b), w);
}

// ──────────────────────────────────────────────────────────────────────
// 9. Non-zero child exit propagates
// ──────────────────────────────────────────────────────────────────────

TEST(ExecutableSubcommand, NonZeroChildExitPropagates) {
    Sandbox sb;
    sb.installFixture("exit_with_code", "exprog-fail");

    Command prog("exprog");
    prog.exitOverride();
    prog.executableDir(sb.path().string());
    prog.executableCommand("fail", "exits 17");

    auto err = parseUserCatching(prog, {"fail", "17"});
    ASSERT_TRUE(err.has_value());
    EXPECT_EQ(err->exitCode, 17);
    EXPECT_EQ(err->code, "commander.executeSubCommandAsync");
}

// ──────────────────────────────────────────────────────────────────────
// 10. Zero child exit returns normally
// ──────────────────────────────────────────────────────────────────────

TEST(ExecutableSubcommand, ZeroChildExitReturnsNormally) {
    Sandbox sb;
    sb.installFixture("exit_with_code", "exprog-ok");

    Command prog("exprog");
    prog.exitOverride();
    prog.executableDir(sb.path().string());
    prog.executableCommand("ok", "exits 0");

    EXPECT_NO_THROW(prog.parse({"ok"}, {.from = "user"}));
}

// ──────────────────────────────────────────────────────────────────────
// 11. Non-executable candidate is skipped
// ──────────────────────────────────────────────────────────────────────

TEST(ExecutableSubcommand, NotExecutableSearchContinues) {
    Sandbox local;
    Sandbox onPath;
    // local: a 644 file — must be SKIPPED by accessSync(..., kX_OK).
    writeNonExecutable(local.file("npxprog-go"), "I am not executable");
    // PATH: an exec'able copy. If the search correctly continues, this wins.
    std::string pathCopy = onPath.installFixture("echo_argv", "npxprog-go");

    std::string out = onPath.file("argv.out");
    EchoSinkGuard sink(out);

    PathGuard guard(onPath.path().string() + ":" +
                    polycpp::process::getenv("PATH"));

    Command prog("npxprog");
    prog.exitOverride();
    prog.executableDir(local.path().string());
    prog.executableCommand("go", "search past 644");

    auto err = parseUserCatching(prog, {"go"});
    ASSERT_FALSE(err.has_value()) << err->what();

    auto lines = readEchoOutput(out);
    ASSERT_FALSE(lines.empty());
    EXPECT_EQ(lines[0], "arg:" + pathCopy);
}

// ──────────────────────────────────────────────────────────────────────
// 12. executableCommand chain returns parent
// ──────────────────────────────────────────────────────────────────────

TEST(ExecutableSubcommand, ChainReturnsParentForFluentApi) {
    Command prog("chprog");
    prog.exitOverride();
    auto& a = prog.executableCommand("a", "first");
    EXPECT_EQ(&a, &prog) << "executableCommand should return *this";

    // Fluent: chain two executable subcommands on the parent.
    prog.executableCommand("b", "second").executableCommand("c", "third");
    ASSERT_EQ(prog.commands().size(), 3u);
    EXPECT_EQ(prog.commands()[0].name(), "a");
    EXPECT_EQ(prog.commands()[1].name(), "b");
    EXPECT_EQ(prog.commands()[2].name(), "c");
}

// ──────────────────────────────────────────────────────────────────────
// 13. executableHandler flag is on (verified by behaviour)
// ──────────────────────────────────────────────────────────────────────

TEST(ExecutableSubcommand, ExecutableHandlerFlagDispatchesToSpawn) {
    // Behavioural check: an executableCommand that has NO action handler
    // and points at a missing program should yield a SPAWN-path error
    // (commander.executeSubCommandAsync), not a no-op or default-help.
    PathGuard guard("");

    Command prog("flagprog");
    prog.exitOverride();
    quiet(prog);
    prog.executableCommand("walk", "walk away");
    // No .action() registered — under the action-handler path, dispatch
    // would either no-op or drop to help. Under the spawn path, it errors
    // with the unique executeSubCommandAsync code.
    auto err = parseUserCatching(prog, {"walk"});
    ASSERT_TRUE(err.has_value());
    EXPECT_EQ(err->code, "commander.executeSubCommandAsync");
}

// ──────────────────────────────────────────────────────────────────────
// 14. Mandatory option check fires before spawn
// ──────────────────────────────────────────────────────────────────────

TEST(ExecutableSubcommand, MandatoryOptionEnforcedBeforeSpawn) {
    Sandbox sb;
    // Install a working fixture so the only failure mode is the mandatory
    // option check. If the check were skipped, parse would succeed silently
    // (zero exit from echo_argv).
    sb.installFixture("echo_argv", "manprog-go");

    Command prog("manprog");
    prog.exitOverride();
    quiet(prog);
    prog.requiredOption("-r, --required <v>", "must be set");
    prog.executableDir(sb.path().string());
    prog.executableCommand("go", "go anywhere");

    auto err = parseUserCatching(prog, {"go"});
    ASSERT_TRUE(err.has_value());
    EXPECT_EQ(err->code, "commander.missingMandatoryOptionValue");
}

// ──────────────────────────────────────────────────────────────────────
// 15. Conflicting option check fires before spawn
// ──────────────────────────────────────────────────────────────────────

TEST(ExecutableSubcommand, ConflictingOptionEnforcedBeforeSpawn) {
    Sandbox sb;
    sb.installFixture("echo_argv", "conprog-go");

    Command prog("conprog");
    prog.exitOverride();
    quiet(prog);
    Option a("--alpha", "alpha");
    a.conflicts("beta");
    prog.addOption(std::move(a));
    prog.option("--beta", "beta");
    prog.executableDir(sb.path().string());
    prog.executableCommand("go", "go conflict");

    auto err = parseUserCatching(prog, {"--alpha", "--beta", "go"});
    ASSERT_TRUE(err.has_value());
    EXPECT_EQ(err->code, "commander.conflictingOption");
}

// ──────────────────────────────────────────────────────────────────────
// 16. Custom executableFile with absolute path bypasses search
// ──────────────────────────────────────────────────────────────────────

TEST(ExecutableSubcommand, CustomExecutableFileAbsolutePath) {
    Sandbox sb;
    // Install under a name that does NOT match either the default
    // <prog-sub> convention or anything on PATH.
    std::string absExec = sb.installFixture("echo_argv", "anyname.bin");

    std::string out = sb.file("argv.out");
    EchoSinkGuard sink(out);

    // Empty PATH and no executableDir — the only way this works is if the
    // dispatch path treats absExec as a literal filesystem path.
    PathGuard guard("");

    Command prog("absprog");
    prog.exitOverride();
    prog.executableCommand("run", "run by absolute path", absExec);

    auto err = parseUserCatching(prog, {"run"});
    ASSERT_FALSE(err.has_value()) << err->what();

    auto lines = readEchoOutput(out);
    ASSERT_FALSE(lines.empty());
    EXPECT_EQ(lines[0], "arg:" + absExec);
}

// ──────────────────────────────────────────────────────────────────────
// 17. Custom executableFile with a relative path containing '/'
// ──────────────────────────────────────────────────────────────────────

TEST(ExecutableSubcommand, CustomExecutableFileRelativePath) {
    // findProgram_ short-circuits names that contain '/' — they are treated
    // as filesystem paths and not searched on PATH. We exercise that path
    // by chdir'ing into a sandbox that holds a "./bin/runner" fixture.
    Sandbox sb;
    fs::create_directories(sb.path() / "bin");
    std::string runner = sb.installFixture("echo_argv", "bin/runner");

    std::string out = sb.file("argv.out");
    EchoSinkGuard sink(out);

    PathGuard guard("");

    auto cwdBefore = fs::current_path();
    fs::current_path(sb.path());
    struct CwdRestore {
        fs::path p;
        ~CwdRestore() { std::error_code ec; fs::current_path(p, ec); }
    } cwdRestore{cwdBefore};

    Command prog("relprog");
    prog.exitOverride();
    prog.executableCommand("run", "via relative path", "./bin/runner");

    auto err = parseUserCatching(prog, {"run"});
    ASSERT_FALSE(err.has_value()) << err->what();

    auto lines = readEchoOutput(out);
    ASSERT_FALSE(lines.empty());
    EXPECT_NE(lines[0].find("bin/runner"), std::string::npos);
    (void)runner;
}

// ──────────────────────────────────────────────────────────────────────
// 18. Help output lists executable subcommand and its description
// ──────────────────────────────────────────────────────────────────────

TEST(ExecutableSubcommand, HelpIncludesExecutableSubcommand) {
    Command prog("helpprog");
    prog.exitOverride();
    prog.executableCommand("deploy", "ship it to prod");
    prog.executableCommand("rollback", "undo the deploy");

    auto help = prog.helpInformation();
    EXPECT_NE(help.find("deploy"), std::string::npos);
    EXPECT_NE(help.find("ship it to prod"), std::string::npos);
    EXPECT_NE(help.find("rollback"), std::string::npos);
    EXPECT_NE(help.find("undo the deploy"), std::string::npos);
}

// ──────────────────────────────────────────────────────────────────────
// 19. addCommand of an executable command also dispatches to spawn
// ──────────────────────────────────────────────────────────────────────

TEST(ExecutableSubcommand, AddCommandWithExecutableHandlerDispatches) {
    Sandbox sb;
    // Build the sub via createCommand + executableCommand on a holder, then
    // verify that the same Impl reaches dispatch via the parent.
    // The simplest way to flip executableHandler_ from outside detail/ is
    // to register via executableCommand on a temporary holder and then
    // addCommand the resulting handle.
    Command holder("holderprog");
    holder.exitOverride();
    holder.executableCommand("go", "exec via holder");
    // Pull the configured sub out of holder.commands() and reattach it.
    Command sub = holder.commands()[0];

    sb.installFixture("echo_argv", "addprog-go");
    std::string out = sb.file("argv.out");
    EchoSinkGuard sink(out);

    Command prog("addprog");
    prog.exitOverride();
    prog.executableDir(sb.path().string());
    prog.addCommand(sub);

    auto err = parseUserCatching(prog, {"go"});
    ASSERT_FALSE(err.has_value()) << err->what();

    auto lines = readEchoOutput(out);
    ASSERT_FALSE(lines.empty());
    EXPECT_NE(lines[0].find("addprog-go"), std::string::npos);
}

// ──────────────────────────────────────────────────────────────────────
// 20. Signal forwarding — see the dedicated cluster at the bottom of this
// file (TEST(ExecutableSubcommand, Forwards*) and friends). The dispatch
// path now uses async `polycpp::child_process::spawn()` and forwards
// SIGUSR1/SIGUSR2/SIGTERM/SIGINT/SIGHUP from the parent to the child via
// `child.kill(signum)`, mirroring upstream commander.js's `signals.forEach`.
// ──────────────────────────────────────────────────────────────────────

// ──────────────────────────────────────────────────────────────────────
// Substitute case A: parseAsync also reaches the spawn path.
// ──────────────────────────────────────────────────────────────────────

TEST(ExecutableSubcommand, ParseAsyncReachesSpawn) {
    Sandbox sb;
    sb.installFixture("echo_argv", "asyncprog-go");

    std::string out = sb.file("argv.out");
    EchoSinkGuard sink(out);

    Command prog("asyncprog");
    prog.exitOverride();
    prog.executableDir(sb.path().string());
    prog.executableCommand("go", "async dispatch");

    auto promise = prog.parseAsync({"go"}, {.from = "user"});
    polycpp::EventLoop::instance().run();

    auto lines = readEchoOutput(out);
    ASSERT_FALSE(lines.empty());
    EXPECT_NE(lines[0].find("asyncprog-go"), std::string::npos);
}

// ──────────────────────────────────────────────────────────────────────
// Substitute case B: required-argument parsing on executableCommand decl.
// ──────────────────────────────────────────────────────────────────────

TEST(ExecutableSubcommand, RequiredArgumentParsedOnDeclaration) {
    Command prog("argprog");
    prog.exitOverride();
    prog.executableCommand("install <pkg> [version]", "install a package");

    ASSERT_EQ(prog.commands().size(), 1u);
    const auto& sub = prog.commands()[0];
    EXPECT_EQ(sub.name(), "install");
    ASSERT_EQ(sub.registeredArguments().size(), 2u);
    EXPECT_EQ(sub.registeredArguments()[0].name(), "pkg");
    EXPECT_TRUE(sub.registeredArguments()[0].required);
    EXPECT_EQ(sub.registeredArguments()[1].name(), "version");
    EXPECT_FALSE(sub.registeredArguments()[1].required);
}

// ──────────────────────────────────────────────────────────────────────
// Substitute case C: explicit absolute executableDir resolves the script
// without consulting scriptPath. Verifies `executableDir` short-circuits
// the relative-to-script join when the supplied path is already absolute
// AND the join path stays inside it.
// ──────────────────────────────────────────────────────────────────────

TEST(ExecutableSubcommand, AbsoluteExecutableDirIgnoresRelativeJoinSuffix) {
    Sandbox sb;
    sb.installFixture("echo_argv", "absdirprog-touch");
    std::string out = sb.file("argv.out");
    EchoSinkGuard sink(out);

    Command prog("absdirprog");
    prog.exitOverride();
    prog.executableDir(sb.path().string()); // absolute
    prog.executableCommand("touch", "touch a file");

    auto err = parseUserCatching(prog, {"touch"});
    ASSERT_FALSE(err.has_value()) << err->what();

    auto lines = readEchoOutput(out);
    ASSERT_FALSE(lines.empty());
    EXPECT_EQ(lines[0], "arg:" + sb.file("absdirprog-touch"));
}

// ──────────────────────────────────────────────────────────────────────
// Bonus: defaults survive when user passes only "--" before the subname.
// ──────────────────────────────────────────────────────────────────────

TEST(ExecutableSubcommand, OperandsAfterDoubleDashStillForwarded) {
    Sandbox sb;
    sb.installFixture("echo_argv", "ddprog-do");
    std::string out = sb.file("argv.out");
    EchoSinkGuard sink(out);

    Command prog("ddprog");
    prog.exitOverride();
    prog.executableDir(sb.path().string());
    prog.executableCommand("do", "do with -- separator");

    auto err = parseUserCatching(prog, {"do", "--", "--flag-like-operand"});
    ASSERT_FALSE(err.has_value()) << err->what();

    auto lines = readEchoOutput(out);
    bool seen = false;
    for (const auto& l : lines) {
        if (l == "arg:--flag-like-operand") { seen = true; break; }
    }
    EXPECT_TRUE(seen) << "operand after -- should reach the child";
}

// ──────────────────────────────────────────────────────────────────────
// Signal forwarding cluster.
//
// Replaces the upstream `tests/command.executableSubcommand.signals.test.js`
// case that was previously omitted. The dispatch path now uses async
// `polycpp::child_process::spawn()`, installs `process::on(<sig>, ...)`
// handlers for each of {SIGUSR1, SIGUSR2, SIGTERM, SIGINT, SIGHUP}, and
// drives the polycpp EventLoop until the child exits — matching upstream
// commander.js's `signals.forEach(sig => process.on(sig, ...))` pattern.
//
// Test pattern: each test
//   1. installs the `signal_writer` fixture under a per-test sandbox name,
//   2. wires `$SIGNAL_WRITER_OUTPUT` and `$SIGNAL_WRITER_READY` to that
//      sandbox so the fixture's signal-handler-side write is observable,
//   3. spins a sender `std::thread` that polls for the READY file then
//      sends the chosen signal to the parent process,
//   4. calls `parse({sub}, {.from = "user"})` — which spawns the fixture
//      and blocks on the EventLoop until the fixture exits,
//   5. joins the sender and asserts the OUTPUT file contains the expected
//      signal name.
//
// The polling loop has a 2-second total timeout (400 iterations of 5ms);
// exceeding it means the spawn pipeline stalled, which is a real bug.
// ──────────────────────────────────────────────────────────────────────

namespace {

/// @brief Async-safe-ish file existence check used by the sender thread.
bool fileExists(const std::string& path) {
    struct stat st;
    return ::stat(path.c_str(), &st) == 0;
}

/// @brief Read the full contents of a file as a string. Returns "" if absent.
std::string readFile(const std::string& path) {
    std::ifstream in(path);
    if (!in) return {};
    std::ostringstream buf;
    buf << in.rdbuf();
    return buf.str();
}

/// @brief Block until `path` exists or the deadline expires.
/// @param path        Filesystem path to poll for.
/// @param deadline_ms Total budget in milliseconds (default 2000 = 2 seconds).
/// @return true if the path appeared in time, false on timeout.
bool waitForFile(const std::string& path, int deadline_ms = 2000) {
    using namespace std::chrono;
    auto deadline = steady_clock::now() + milliseconds(deadline_ms);
    while (steady_clock::now() < deadline) {
        if (fileExists(path)) return true;
        std::this_thread::sleep_for(milliseconds(5));
    }
    return fileExists(path);
}

/**
 * @brief Drive a single signal-forwarding scenario.
 * @param sub        The subcommand name (also used to derive the executable
 *                   name `<prog>-<sub>` installed in the sandbox).
 * @param prog       The program name passed to `Command(...)`.
 * @param signum     The signal the sender thread sends to the parent (which
 *                   the dispatch path is expected to forward to the child).
 * @param expectName The substring to find in the captured output (e.g.
 *                   "SIGTERM" — the fixture writes the name with a newline).
 *
 * Asserts the captured output contains the expected name. Uses gtest's
 * non-fatal `EXPECT_*` so the sender thread always gets joined.
 */
void runSignalForwardingCase(const std::string& prog,
                             const std::string& sub,
                             int signum,
                             const std::string& expectName) {
    Sandbox sb;
    const std::string execName = prog + "-" + sub;
    sb.installFixture("signal_writer", execName);

    const std::string outPath   = sb.file("out");
    const std::string readyPath = sb.file("ready");
    polycpp::process::setenv("SIGNAL_WRITER_OUTPUT", outPath);
    polycpp::process::setenv("SIGNAL_WRITER_READY",  readyPath);
    struct EnvCleanup {
        ~EnvCleanup() {
            polycpp::process::unsetenv("SIGNAL_WRITER_OUTPUT");
            polycpp::process::unsetenv("SIGNAL_WRITER_READY");
        }
    } envCleanup;

    Command cmd(prog);
    cmd.exitOverride();
    quiet(cmd);
    cmd.executableDir(sb.path().string());
    cmd.executableCommand(sub, "send a signal to me");

    // Sender thread: wait for the child to be ready, then signal the parent.
    std::atomic<bool> readySeen{false};
    std::thread sender([&] {
        if (!waitForFile(readyPath)) {
            // Child never wrote the ready marker — let the test fail loudly
            // when it inspects readySeen.
            return;
        }
        readySeen = true;
        ::kill(::getpid(), signum);
    });

    // The fixture exits with 128+signum, so commander surfaces a non-zero
    // exit through CommanderError under exitOverride().
    auto err = parseUserCatching(cmd, {sub});

    sender.join();

    EXPECT_TRUE(readySeen) << "signal_writer never reached its READY marker";
    if (err.has_value()) {
        EXPECT_EQ(err->code, "commander.executeSubCommandAsync");
        EXPECT_EQ(err->exitCode, 128 + signum) << err->what();
    } else {
        ADD_FAILURE() << "expected non-zero exit propagation under exitOverride()";
    }

    auto contents = readFile(outPath);
    EXPECT_NE(contents.find(expectName), std::string::npos)
        << "captured output: " << contents;
}

} // namespace

TEST(ExecutableSubcommand, ForwardsSIGTERM) {
    runSignalForwardingCase("sigprog", "term", SIGTERM, "SIGTERM");
}

TEST(ExecutableSubcommand, ForwardsSIGINT) {
    runSignalForwardingCase("sigprog", "intsub", SIGINT, "SIGINT");
}

TEST(ExecutableSubcommand, ForwardsSIGHUP) {
    runSignalForwardingCase("sigprog", "hup", SIGHUP, "SIGHUP");
}

TEST(ExecutableSubcommand, ForwardsSIGUSR1) {
    runSignalForwardingCase("sigprog", "usr1", SIGUSR1, "SIGUSR1");
}

TEST(ExecutableSubcommand, ForwardsSIGUSR2) {
    runSignalForwardingCase("sigprog", "usr2", SIGUSR2, "SIGUSR2");
}

// ──────────────────────────────────────────────────────────────────────
// After the child exits cleanly, the per-spawn forwarder lambda must
// no-op — sending a signal to the parent should NOT be forwarded to a
// (now-dead) child PID, and the parent process must remain alive.
// ──────────────────────────────────────────────────────────────────────

TEST(ExecutableSubcommand, ForwarderInactiveAfterChildExit) {
    Sandbox sb;
    sb.installFixture("exit_with_code", "doneprog-bye");

    Command cmd("doneprog");
    cmd.exitOverride();
    cmd.executableDir(sb.path().string());
    cmd.executableCommand("bye", "exit cleanly");

    // The fixture exits 0 immediately; parse() returns normally.
    EXPECT_NO_THROW(cmd.parse({"bye"}, {.from = "user"}));

    // After parse() returns, the forwarder is destructed and its lambdas are
    // marked inactive. The lambdas remain on the SignalRegistry though, so a
    // SIGUSR1 to the parent still dispatches them on the next event loop
    // run. We install a backstop user handler that flips a flag so we can
    // confirm the signal arrived AND the parent survived.
    std::atomic<bool> backstopFired{false};
    polycpp::process::on("SIGUSR1", [&](int) { backstopFired = true; });

    ::kill(::getpid(), SIGUSR1);
    polycpp::process::drainPendingSignals();

    EXPECT_TRUE(backstopFired) << "user handler should still fire";
    // The fact that we're still executing here is itself the assertion that
    // the no-op forwarder did not crash trying to kill a dead PID.
    SUCCEED();
}

// ──────────────────────────────────────────────────────────────────────
// Multiple sequential spawns: each must install its own active forwarder,
// and residual no-op lambdas from the first spawn must not interfere.
// ──────────────────────────────────────────────────────────────────────

TEST(ExecutableSubcommand, MultipleSpawnsEachInstallTheirOwnForwarder) {
    runSignalForwardingCase("multprog", "term1", SIGTERM, "SIGTERM");
    runSignalForwardingCase("multprog", "term2", SIGTERM, "SIGTERM");
}

// ──────────────────────────────────────────────────────────────────────
// User-installed signal handler must coexist with the dispatch-path
// forwarder. Validates the no-clobber design enabled by polycpp's
// process::off(name, id) per-listener removal (commit 8561254f).
// ──────────────────────────────────────────────────────────────────────

TEST(ExecutableSubcommand, SignalForwardingPreservesUserHandler) {
    // Install user handler BEFORE the dispatch path adds its forwarder.
    std::atomic<int> userFires{0};
    auto userId = polycpp::process::on("SIGUSR1",
                                       [&](int) { userFires.fetch_add(1); });
    struct ListenerCleanup {
        polycpp::process::SignalListenerId id;
        ~ListenerCleanup() { polycpp::process::off("SIGUSR1", id); }
    } listenerCleanup{userId};

    Sandbox sb;
    sb.installFixture("signal_writer", "userprog-go");

    const std::string outPath   = sb.file("out");
    const std::string readyPath = sb.file("ready");
    polycpp::process::setenv("SIGNAL_WRITER_OUTPUT", outPath);
    polycpp::process::setenv("SIGNAL_WRITER_READY",  readyPath);
    struct EnvCleanup {
        ~EnvCleanup() {
            polycpp::process::unsetenv("SIGNAL_WRITER_OUTPUT");
            polycpp::process::unsetenv("SIGNAL_WRITER_READY");
        }
    } envCleanup;

    Command cmd("userprog");
    cmd.exitOverride();
    quiet(cmd);
    cmd.executableDir(sb.path().string());
    cmd.executableCommand("go", "send a signal");

    std::thread sender([&] {
        if (!waitForFile(readyPath)) return;
        ::kill(::getpid(), SIGUSR1);
    });

    auto err = parseUserCatching(cmd, {"go"});
    sender.join();

    // Forwarded to child:
    EXPECT_NE(readFile(outPath).find("SIGUSR1"), std::string::npos);
    // Surfaced as a non-zero exit because fixture exited with 128+SIGUSR1:
    ASSERT_TRUE(err.has_value());
    EXPECT_EQ(err->exitCode, 128 + SIGUSR1);

    // The user handler fired alongside the forwarder.
    polycpp::process::drainPendingSignals();
    EXPECT_GE(userFires.load(), 1)
        << "user-installed SIGUSR1 handler must fire alongside the forwarder";

    // The user handler is still installed; only commander's own forwarder
    // listener was removed at SignalForwarder dtor.
    EXPECT_EQ(polycpp::process::listenerCount("SIGUSR1"), 1u)
        << "commander must remove only its own forwarder, not the user's "
           "pre-installed handler";
}

// ──────────────────────────────────────────────────────────────────────
// Standalone exit-code surfacing: 128 + SIGTERM under exitOverride().
// Mirrors the upstream signals.test assertion that the "(close)" exit
// code is propagated through commander's exit handling.
// ──────────────────────────────────────────────────────────────────────

TEST(ExecutableSubcommand, ExitCodeAfterSIGTERM) {
    Sandbox sb;
    sb.installFixture("signal_writer", "exitprog-go");

    const std::string outPath   = sb.file("out");
    const std::string readyPath = sb.file("ready");
    polycpp::process::setenv("SIGNAL_WRITER_OUTPUT", outPath);
    polycpp::process::setenv("SIGNAL_WRITER_READY",  readyPath);
    struct EnvCleanup {
        ~EnvCleanup() {
            polycpp::process::unsetenv("SIGNAL_WRITER_OUTPUT");
            polycpp::process::unsetenv("SIGNAL_WRITER_READY");
        }
    } envCleanup;

    Command cmd("exitprog");
    cmd.exitOverride();
    quiet(cmd);
    cmd.executableDir(sb.path().string());
    cmd.executableCommand("go", "exit on signal");

    std::thread sender([&] {
        if (!waitForFile(readyPath)) return;
        ::kill(::getpid(), SIGTERM);
    });

    auto err = parseUserCatching(cmd, {"go"});
    sender.join();

    ASSERT_TRUE(err.has_value());
    EXPECT_EQ(err->code, "commander.executeSubCommandAsync");
    EXPECT_EQ(err->exitCode, 128 + SIGTERM);
}
