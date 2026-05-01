/**
 * @file echo_argv.cpp
 * @brief Test fixture: echo argv to a file (or stdout) and exit 0.
 *
 * This fixture is spawned by the polycpp_commander_test_executable suite to
 * observe how `polycpp::commander::Command::executableCommand(...)` forwards
 * operands and unknown arguments to its child process.
 *
 * Behaviour:
 *  - Reads the env var `ECHO_ARGV_OUTPUT` (set by the GoogleTest case). If the
 *    var is non-empty, opens it as the destination file and writes one line
 *    per argv entry, prefixed with `arg:`.
 *  - If the env var is unset/empty, writes the same lines to stdout.
 *  - Exits 0 unconditionally so callers can disambiguate "spawn succeeded but
 *    forwarded the wrong argv" from "spawn failed with non-zero exit".
 *
 * The fixture deliberately does NOT depend on polycpp_commander or polycpp;
 * it is a stand-alone executable that must be discoverable on disk by the
 * commander dispatch path under test.
 */

#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <string>

int main(int argc, char** argv) {
    const char* outPath = std::getenv("ECHO_ARGV_OUTPUT");
    std::string sink = outPath ? outPath : "";

    std::ofstream file;
    bool useFile = !sink.empty();
    if (useFile) {
        file.open(sink, std::ios::out | std::ios::trunc);
        if (!file) {
            std::fprintf(stderr, "echo_argv: failed to open '%s'\n", sink.c_str());
            return 2;
        }
    }

    for (int i = 0; i < argc; ++i) {
        std::string line = std::string("arg:") + (argv[i] ? argv[i] : "");
        if (useFile) {
            file << line << "\n";
        } else {
            std::puts(line.c_str());
        }
    }
    return 0;
}
