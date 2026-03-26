#include <polycpp/commander/detail/aggregator.hpp>
#include <gtest/gtest.h>

using namespace polycpp::commander;

// --- Constructor parsing tests ---

TEST(ArgumentTest, RequiredArgument) {
    Argument arg("<name>");
    EXPECT_TRUE(arg.required);
    EXPECT_FALSE(arg.variadic);
    EXPECT_EQ(arg.name(), "name");
}

TEST(ArgumentTest, OptionalArgument) {
    Argument arg("[name]");
    EXPECT_FALSE(arg.required);
    EXPECT_FALSE(arg.variadic);
    EXPECT_EQ(arg.name(), "name");
}

TEST(ArgumentTest, VariadicRequired) {
    Argument arg("<files...>");
    EXPECT_TRUE(arg.required);
    EXPECT_TRUE(arg.variadic);
    EXPECT_EQ(arg.name(), "files");
}

TEST(ArgumentTest, VariadicOptional) {
    Argument arg("[files...]");
    EXPECT_FALSE(arg.required);
    EXPECT_TRUE(arg.variadic);
    EXPECT_EQ(arg.name(), "files");
}

TEST(ArgumentTest, PlainNameIsRequired) {
    Argument arg("name");
    EXPECT_TRUE(arg.required);
    EXPECT_FALSE(arg.variadic);
    EXPECT_EQ(arg.name(), "name");
}

TEST(ArgumentTest, DescriptionStored) {
    Argument arg("<file>", "input file to process");
    EXPECT_EQ(arg.description, "input file to process");
}

TEST(ArgumentTest, EmptyDescription) {
    Argument arg("<file>");
    EXPECT_EQ(arg.description, "");
}

// --- DefaultValue tests ---

TEST(ArgumentTest, DefaultValueString) {
    Argument arg("[color]");
    arg.defaultValue(polycpp::JsonValue("blue"), "favorite color");
    EXPECT_EQ(arg.defaultValue_.asString(), "blue");
    EXPECT_EQ(arg.defaultValueDescription_, "favorite color");
}

TEST(ArgumentTest, DefaultValueInt) {
    Argument arg("[count]");
    arg.defaultValue(polycpp::JsonValue(42));
    EXPECT_EQ(arg.defaultValue_.asInt(), 42);
}

TEST(ArgumentTest, DefaultValueNoDescription) {
    Argument arg("[name]");
    arg.defaultValue(polycpp::JsonValue("world"));
    EXPECT_EQ(arg.defaultValue_.asString(), "world");
    EXPECT_EQ(arg.defaultValueDescription_, "");
}

// --- ArgParser tests ---

TEST(ArgumentTest, ArgParserCustomFunction) {
    Argument arg("<number>");
    arg.argParser([](const std::string& value, const polycpp::JsonValue&) -> polycpp::JsonValue {
        return polycpp::JsonValue(std::stoi(value));
    });
    EXPECT_TRUE(static_cast<bool>(arg.parseArg_));
    auto result = arg.parseArg_("42", polycpp::JsonValue());
    EXPECT_EQ(result.asInt(), 42);
}

// --- Choices tests ---

TEST(ArgumentTest, ChoicesAllowedValue) {
    Argument arg("<size>");
    arg.choices({"small", "medium", "large"});
    ASSERT_TRUE(arg.argChoices_.has_value());
    EXPECT_EQ(arg.argChoices_->size(), 3u);
    EXPECT_EQ((*arg.argChoices_)[0], "small");
    EXPECT_EQ((*arg.argChoices_)[1], "medium");
    EXPECT_EQ((*arg.argChoices_)[2], "large");
}

TEST(ArgumentTest, ChoicesValidatorAcceptsValid) {
    Argument arg("<size>");
    arg.choices({"small", "medium", "large"});
    EXPECT_TRUE(static_cast<bool>(arg.parseArg_));
    auto result = arg.parseArg_("medium", polycpp::JsonValue());
    EXPECT_EQ(result.asString(), "medium");
}

TEST(ArgumentTest, ChoicesValidatorRejectsInvalid) {
    Argument arg("<size>");
    arg.choices({"small", "medium", "large"});
    EXPECT_THROW(arg.parseArg_("huge", polycpp::JsonValue()), InvalidArgumentError);
}

TEST(ArgumentTest, ChoicesVariadicCollects) {
    Argument arg("<sizes...>");
    arg.choices({"small", "medium", "large"});
    auto result1 = arg.parseArg_("small", polycpp::JsonValue());
    ASSERT_TRUE(result1.isArray());
    auto& arr1 = result1.asArray();
    EXPECT_EQ(arr1.size(), 1u);
    EXPECT_EQ(arr1[0].asString(), "small");

    auto result2 = arg.parseArg_("medium", result1);
    ASSERT_TRUE(result2.isArray());
    auto& arr2 = result2.asArray();
    EXPECT_EQ(arr2.size(), 2u);
    EXPECT_EQ(arr2[0].asString(), "small");
    EXPECT_EQ(arr2[1].asString(), "medium");
}

// --- argRequired / argOptional ---

TEST(ArgumentTest, ArgRequiredSetsRequired) {
    Argument arg("[name]");
    EXPECT_FALSE(arg.required);
    arg.argRequired();
    EXPECT_TRUE(arg.required);
}

TEST(ArgumentTest, ArgOptionalClearsRequired) {
    Argument arg("<name>");
    EXPECT_TRUE(arg.required);
    arg.argOptional();
    EXPECT_FALSE(arg.required);
}

// --- Fluent chaining ---

TEST(ArgumentTest, DefaultValueReturnsThis) {
    Argument arg("<value>");
    Argument& ref = arg.defaultValue(polycpp::JsonValue(3));
    EXPECT_EQ(&ref, &arg);
}

TEST(ArgumentTest, ArgParserReturnsThis) {
    Argument arg("<value>");
    Argument& ref = arg.argParser([](const std::string& v, const polycpp::JsonValue&) -> polycpp::JsonValue { return polycpp::JsonValue(v); });
    EXPECT_EQ(&ref, &arg);
}

TEST(ArgumentTest, ChoicesReturnsThis) {
    Argument arg("<value>");
    Argument& ref = arg.choices({"a"});
    EXPECT_EQ(&ref, &arg);
}

TEST(ArgumentTest, ArgRequiredReturnsThis) {
    Argument arg("<value>");
    Argument& ref = arg.argRequired();
    EXPECT_EQ(&ref, &arg);
}

TEST(ArgumentTest, ArgOptionalReturnsThis) {
    Argument arg("<value>");
    Argument& ref = arg.argOptional();
    EXPECT_EQ(&ref, &arg);
}

TEST(ArgumentTest, FluentChaining) {
    Argument arg("[size]");
    arg.defaultValue(polycpp::JsonValue("medium"), "default size")
       .argRequired()
       .choices({"small", "medium", "large"});
    EXPECT_TRUE(arg.required);
    EXPECT_TRUE(arg.argChoices_.has_value());
    EXPECT_EQ(arg.defaultValue_.asString(), "medium");
}

// --- humanReadableArgName ---

TEST(ArgumentTest, HumanReadableRequired) {
    Argument arg("<file>");
    EXPECT_EQ(humanReadableArgName(arg), "<file>");
}

TEST(ArgumentTest, HumanReadableOptional) {
    Argument arg("[file]");
    EXPECT_EQ(humanReadableArgName(arg), "[file]");
}

TEST(ArgumentTest, HumanReadableVariadicRequired) {
    Argument arg("<files...>");
    EXPECT_EQ(humanReadableArgName(arg), "<files...>");
}

TEST(ArgumentTest, HumanReadableVariadicOptional) {
    Argument arg("[files...]");
    EXPECT_EQ(humanReadableArgName(arg), "[files...]");
}
