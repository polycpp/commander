/**
 * @file exit_with_code.cpp
 * @brief Test fixture: exit with the integer in argv[1] (or 0 if absent).
 *
 * Used by the polycpp_commander_test_executable suite to verify that the
 * child process's exit status propagates through
 * `polycpp::commander::Command::executeSubCommand_` to the parent's exit
 * handler (or `CommanderError::exitCode` under `exitOverride()`).
 *
 * Behaviour:
 *  - If `argc < 2`, exits 0.
 *  - Otherwise, parses `argv[1]` with `std::atoi` and exits with that code.
 *  - `argv[2..]` are ignored.
 *
 * Stand-alone: does NOT link against polycpp or polycpp_commander.
 */

#include <cstdlib>

int main(int argc, char** argv) {
    if (argc < 2) return 0;
    return std::atoi(argv[1]);
}
