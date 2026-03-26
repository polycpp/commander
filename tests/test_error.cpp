#include <polycpp/commander/detail/aggregator.hpp>
#include <gtest/gtest.h>

using namespace polycpp::commander;

TEST(ErrorTest, CommanderErrorProperties) {
    CommanderError err(1, "commander.test", "test error message");
    EXPECT_EQ(err.exitCode, 1);
    EXPECT_EQ(err.code, "commander.test");
    EXPECT_EQ(std::string(err.what()), "Error: test error message");
}

TEST(ErrorTest, CommanderErrorCatchAsPolycppError) {
    try {
        throw CommanderError(2, "commander.foo", "foo error");
    } catch (const polycpp::Error& e) {
        EXPECT_EQ(std::string(e.what()), "Error: foo error");
    }
}

TEST(ErrorTest, InvalidArgumentErrorDefaults) {
    InvalidArgumentError err("bad value");
    EXPECT_EQ(err.exitCode, 1);
    EXPECT_EQ(err.code, "commander.invalidArgument");
    EXPECT_EQ(std::string(err.what()), "Error: bad value");
}

TEST(ErrorTest, InvalidArgumentErrorCatchAsCommanderError) {
    try {
        throw InvalidArgumentError("invalid");
    } catch (const CommanderError& e) {
        EXPECT_EQ(e.code, "commander.invalidArgument");
        EXPECT_EQ(e.exitCode, 1);
    }
}
