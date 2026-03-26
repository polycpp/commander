#include <polycpp/commander/detail/aggregator.hpp>
#include <gtest/gtest.h>

using namespace polycpp::commander;

// --- Constructor / flag parsing tests ---

TEST(OptionTest, ShortAndLongFlag) {
    Option opt("-f, --file <path>");
    ASSERT_TRUE(opt.short_.has_value());
    ASSERT_TRUE(opt.long_.has_value());
    EXPECT_EQ(*opt.short_, "-f");
    EXPECT_EQ(*opt.long_, "--file");
    EXPECT_TRUE(opt.required);
    EXPECT_FALSE(opt.optional);
}

TEST(OptionTest, LongOnly) {
    Option opt("--verbose");
    EXPECT_FALSE(opt.short_.has_value());
    ASSERT_TRUE(opt.long_.has_value());
    EXPECT_EQ(*opt.long_, "--verbose");
}

TEST(OptionTest, ShortOnly) {
    Option opt("-v");
    ASSERT_TRUE(opt.short_.has_value());
    EXPECT_FALSE(opt.long_.has_value());
    EXPECT_EQ(*opt.short_, "-v");
}

TEST(OptionTest, RequiredValue) {
    Option opt("-f, --file <path>");
    EXPECT_TRUE(opt.required);
    EXPECT_FALSE(opt.optional);
}

TEST(OptionTest, OptionalValue) {
    Option opt("-f, --file [path]");
    EXPECT_FALSE(opt.required);
    EXPECT_TRUE(opt.optional);
}

TEST(OptionTest, BooleanOption) {
    Option opt("-v, --verbose");
    EXPECT_FALSE(opt.required);
    EXPECT_FALSE(opt.optional);
    EXPECT_TRUE(opt.isBoolean());
}

TEST(OptionTest, NegateOption) {
    Option opt("--no-color");
    EXPECT_TRUE(opt.negate);
    EXPECT_FALSE(opt.isBoolean());
}

TEST(OptionTest, VariadicOption) {
    Option opt("-v, --variadic <files...>");
    EXPECT_TRUE(opt.variadic);
    EXPECT_TRUE(opt.required);
}

TEST(OptionTest, VariadicOptionalValue) {
    Option opt("--items [values...]");
    EXPECT_TRUE(opt.variadic);
    EXPECT_TRUE(opt.optional);
}

TEST(OptionTest, DescriptionStored) {
    Option opt("-v, --verbose", "enable verbose output");
    EXPECT_EQ(opt.description, "enable verbose output");
}

TEST(OptionTest, EmptyDescription) {
    Option opt("--flag");
    EXPECT_EQ(opt.description, "");
}

TEST(OptionTest, DefaultsAfterConstruction) {
    Option opt("--flag");
    EXPECT_FALSE(opt.mandatory);
    EXPECT_FALSE(opt.hidden);
    EXPECT_FALSE(opt.envVar_.has_value());
    EXPECT_FALSE(opt.argChoices_.has_value());
    EXPECT_TRUE(opt.conflictsWith_.empty());
    EXPECT_FALSE(opt.implied_.has_value());
}

// --- name() tests ---

TEST(OptionTest, NameFromLongFlag) {
    Option opt("--verbose");
    EXPECT_EQ(opt.name(), "verbose");
}

TEST(OptionTest, NameFromShortFlag) {
    Option opt("-v");
    EXPECT_EQ(opt.name(), "v");
}

TEST(OptionTest, NameFromLongWithDash) {
    Option opt("--dry-run");
    EXPECT_EQ(opt.name(), "dry-run");
}

TEST(OptionTest, NameFromNegateStripsNo) {
    // name() for --no-color returns "no-color" (the full name after --)
    // attributeName() strips the "no-" prefix
    Option opt("--no-color");
    EXPECT_EQ(opt.name(), "no-color");
}

TEST(OptionTest, NamePrefersLongOverShort) {
    Option opt("-v, --verbose");
    EXPECT_EQ(opt.name(), "verbose");
}

// --- attributeName() tests ---

TEST(OptionTest, AttributeNameSimple) {
    Option opt("--verbose");
    EXPECT_EQ(opt.attributeName(), "verbose");
}

TEST(OptionTest, AttributeNameKebabCase) {
    Option opt("--dry-run");
    EXPECT_EQ(opt.attributeName(), "dryRun");
}

TEST(OptionTest, AttributeNameNegate) {
    Option opt("--no-color");
    EXPECT_EQ(opt.attributeName(), "color");
}

TEST(OptionTest, AttributeNameNegateKebab) {
    Option opt("--no-dry-run");
    EXPECT_EQ(opt.attributeName(), "dryRun");
}

TEST(OptionTest, AttributeNameShortOnly) {
    Option opt("-v");
    EXPECT_EQ(opt.attributeName(), "v");
}

// --- isBoolean() tests ---

TEST(OptionTest, IsBooleanForBoolFlag) {
    Option opt("--verbose");
    EXPECT_TRUE(opt.isBoolean());
}

TEST(OptionTest, IsBooleanFalseForRequired) {
    Option opt("--file <path>");
    EXPECT_FALSE(opt.isBoolean());
}

TEST(OptionTest, IsBooleanFalseForOptional) {
    Option opt("--file [path]");
    EXPECT_FALSE(opt.isBoolean());
}

TEST(OptionTest, IsBooleanFalseForNegate) {
    Option opt("--no-color");
    EXPECT_FALSE(opt.isBoolean());
}

// --- is() tests ---

TEST(OptionTest, IsMatchesShort) {
    Option opt("-v, --verbose");
    EXPECT_TRUE(opt.is("-v"));
    EXPECT_FALSE(opt.is("-x"));
}

TEST(OptionTest, IsMatchesLong) {
    Option opt("-v, --verbose");
    EXPECT_TRUE(opt.is("--verbose"));
    EXPECT_FALSE(opt.is("--debug"));
}

// --- defaultValue tests ---

TEST(OptionTest, DefaultValueSetsValue) {
    Option opt("--color [value]");
    opt.defaultValue(std::string("blue"), "favorite color");
    EXPECT_EQ(std::any_cast<std::string>(opt.defaultValue_), "blue");
    EXPECT_EQ(opt.defaultValueDescription_, "favorite color");
}

// --- preset tests ---

TEST(OptionTest, PresetSetsValue) {
    Option opt("--color");
    opt.preset(std::string("RGB"));
    EXPECT_EQ(std::any_cast<std::string>(opt.presetArg_), "RGB");
}

// --- conflicts tests ---

TEST(OptionTest, ConflictsSingleName) {
    Option opt("--rgb");
    opt.conflicts("cmyk");
    ASSERT_EQ(opt.conflictsWith_.size(), 1u);
    EXPECT_EQ(opt.conflictsWith_[0], "cmyk");
}

TEST(OptionTest, ConflictsMultipleNames) {
    Option opt("--js");
    opt.conflicts(std::vector<std::string>{"ts", "jsx"});
    ASSERT_EQ(opt.conflictsWith_.size(), 2u);
    EXPECT_EQ(opt.conflictsWith_[0], "ts");
    EXPECT_EQ(opt.conflictsWith_[1], "jsx");
}

TEST(OptionTest, ConflictsAccumulates) {
    Option opt("--a");
    opt.conflicts("b");
    opt.conflicts("c");
    ASSERT_EQ(opt.conflictsWith_.size(), 2u);
    EXPECT_EQ(opt.conflictsWith_[0], "b");
    EXPECT_EQ(opt.conflictsWith_[1], "c");
}

// --- implies tests ---

TEST(OptionTest, ImpliesSetsValues) {
    Option opt("--trace");
    opt.implies({{"log", std::any(std::string("trace.txt"))}});
    ASSERT_TRUE(opt.implied_.has_value());
    EXPECT_EQ(opt.implied_->size(), 1u);
    EXPECT_EQ(std::any_cast<std::string>(opt.implied_->at("log")), "trace.txt");
}

TEST(OptionTest, ImpliesMerges) {
    Option opt("--trace");
    opt.implies({{"log", std::any(std::string("trace.txt"))}});
    opt.implies({{"verbose", std::any(true)}});
    ASSERT_TRUE(opt.implied_.has_value());
    EXPECT_EQ(opt.implied_->size(), 2u);
}

// --- env tests ---

TEST(OptionTest, EnvSetsVariable) {
    Option opt("--port <number>");
    opt.env("MY_APP_PORT");
    ASSERT_TRUE(opt.envVar_.has_value());
    EXPECT_EQ(*opt.envVar_, "MY_APP_PORT");
}

// --- argParser tests ---

TEST(OptionTest, ArgParserSetsFunction) {
    Option opt("--port <number>");
    opt.argParser([](const std::string& value, const std::any&) -> std::any {
        return std::stoi(value);
    });
    EXPECT_TRUE(static_cast<bool>(opt.parseArg_));
    auto result = opt.parseArg_("8080", std::any{});
    EXPECT_EQ(std::any_cast<int>(result), 8080);
}

// --- makeOptionMandatory tests ---

TEST(OptionTest, MandatoryDefault) {
    Option opt("--file <path>");
    opt.makeOptionMandatory();
    EXPECT_TRUE(opt.mandatory);
}

TEST(OptionTest, MandatoryFalse) {
    Option opt("--file <path>");
    opt.makeOptionMandatory(true);
    EXPECT_TRUE(opt.mandatory);
    opt.makeOptionMandatory(false);
    EXPECT_FALSE(opt.mandatory);
}

// --- hideHelp tests ---

TEST(OptionTest, HideHelpDefault) {
    Option opt("--debug");
    opt.hideHelp();
    EXPECT_TRUE(opt.hidden);
}

TEST(OptionTest, HideHelpFalse) {
    Option opt("--debug");
    opt.hideHelp(true);
    EXPECT_TRUE(opt.hidden);
    opt.hideHelp(false);
    EXPECT_FALSE(opt.hidden);
}

// --- choices tests ---

TEST(OptionTest, ChoicesSetsValues) {
    Option opt("-s, --size <value>");
    opt.choices({"small", "medium", "large"});
    ASSERT_TRUE(opt.argChoices_.has_value());
    EXPECT_EQ(opt.argChoices_->size(), 3u);
}

TEST(OptionTest, ChoicesValidatorAcceptsValid) {
    Option opt("-s, --size <value>");
    opt.choices({"small", "medium", "large"});
    auto result = opt.parseArg_("medium", std::any{});
    EXPECT_EQ(std::any_cast<std::string>(result), "medium");
}

TEST(OptionTest, ChoicesValidatorRejectsInvalid) {
    Option opt("-s, --size <value>");
    opt.choices({"small", "medium", "large"});
    EXPECT_THROW(opt.parseArg_("huge", std::any{}), InvalidArgumentError);
}

// --- Fluent chaining tests ---

TEST(OptionTest, DefaultValueReturnsThis) {
    Option opt("-e, --example <value>");
    Option& ref = opt.defaultValue(3);
    EXPECT_EQ(&ref, &opt);
}

TEST(OptionTest, ArgParserReturnsThis) {
    Option opt("-e, --example <value>");
    Option& ref = opt.argParser([](const std::string& v, const std::any&) -> std::any { return v; });
    EXPECT_EQ(&ref, &opt);
}

TEST(OptionTest, MakeOptionMandatoryReturnsThis) {
    Option opt("-e, --example <value>");
    Option& ref = opt.makeOptionMandatory();
    EXPECT_EQ(&ref, &opt);
}

TEST(OptionTest, HideHelpReturnsThis) {
    Option opt("-e, --example <value>");
    Option& ref = opt.hideHelp();
    EXPECT_EQ(&ref, &opt);
}

TEST(OptionTest, ChoicesReturnsThis) {
    Option opt("-e, --example <value>");
    Option& ref = opt.choices({"a"});
    EXPECT_EQ(&ref, &opt);
}

TEST(OptionTest, EnvReturnsThis) {
    Option opt("-e, --example <value>");
    Option& ref = opt.env("e");
    EXPECT_EQ(&ref, &opt);
}

TEST(OptionTest, ConflictsVectorReturnsThis) {
    Option opt("-e, --example <value>");
    Option& ref = opt.conflicts(std::vector<std::string>{"a"});
    EXPECT_EQ(&ref, &opt);
}

TEST(OptionTest, ConflictsStringReturnsThis) {
    Option opt("-e, --example <value>");
    Option& ref = opt.conflicts("a");
    EXPECT_EQ(&ref, &opt);
}

TEST(OptionTest, PresetReturnsThis) {
    Option opt("-e, --example");
    Option& ref = opt.preset(std::string("value"));
    EXPECT_EQ(&ref, &opt);
}

TEST(OptionTest, ImpliesReturnsThis) {
    Option opt("-e, --example");
    Option& ref = opt.implies({{"log", std::any(true)}});
    EXPECT_EQ(&ref, &opt);
}

// --- splitOptionFlags edge cases ---

TEST(OptionTest, TwoLongFlags) {
    // --ws, --workspace pattern
    Option opt("--ws, --workspace");
    // short_ gets the first long flag, long_ gets the second
    ASSERT_TRUE(opt.short_.has_value());
    ASSERT_TRUE(opt.long_.has_value());
    EXPECT_EQ(*opt.short_, "--ws");
    EXPECT_EQ(*opt.long_, "--workspace");
}

TEST(OptionTest, LongThenShort) {
    Option opt("--both, -b");
    ASSERT_TRUE(opt.short_.has_value());
    ASSERT_TRUE(opt.long_.has_value());
    EXPECT_EQ(*opt.short_, "-b");
    EXPECT_EQ(*opt.long_, "--both");
}

TEST(OptionTest, CommaNoSpace) {
    Option opt("-b,--both <comma>");
    ASSERT_TRUE(opt.short_.has_value());
    ASSERT_TRUE(opt.long_.has_value());
    EXPECT_EQ(*opt.short_, "-b");
    EXPECT_EQ(*opt.long_, "--both");
    EXPECT_TRUE(opt.required);
}

TEST(OptionTest, PipeSeparator) {
    Option opt("-b|--both <bar>");
    ASSERT_TRUE(opt.short_.has_value());
    ASSERT_TRUE(opt.long_.has_value());
    EXPECT_EQ(*opt.short_, "-b");
    EXPECT_EQ(*opt.long_, "--both");
}

TEST(OptionTest, SpaceSeparator) {
    Option opt("-b --both [space]");
    ASSERT_TRUE(opt.short_.has_value());
    ASSERT_TRUE(opt.long_.has_value());
    EXPECT_EQ(*opt.short_, "-b");
    EXPECT_EQ(*opt.long_, "--both");
    EXPECT_TRUE(opt.optional);
}

// --- Bad flag tests (should throw) ---

TEST(OptionTest, BadFlagsTwoShort) {
    EXPECT_THROW(Option("-a, -b"), std::runtime_error);
}

TEST(OptionTest, BadFlagsTwoShortWithValue) {
    EXPECT_THROW(Option("-a, -b <value>"), std::runtime_error);
}

TEST(OptionTest, BadFlagsThreeLong) {
    EXPECT_THROW(Option("--one, --two, --three"), std::runtime_error);
}

TEST(OptionTest, BadFlagsMultiCharShort) {
    EXPECT_THROW(Option("-ws"), std::runtime_error);
}

TEST(OptionTest, BadFlagsTripleDash) {
    EXPECT_THROW(Option("---triple"), std::runtime_error);
}

TEST(OptionTest, BadFlagsNoFlags) {
    EXPECT_THROW(Option("sdkjhskjh"), std::runtime_error);
}

TEST(OptionTest, BadFlagsCommaNoSpaceTwoShort) {
    EXPECT_THROW(Option("-a,-b"), std::runtime_error);
}

TEST(OptionTest, BadFlagsPipeTwoShort) {
    EXPECT_THROW(Option("-a|-b"), std::runtime_error);
}

TEST(OptionTest, BadFlagsSpaceTwoShort) {
    EXPECT_THROW(Option("-a -b"), std::runtime_error);
}

// --- camelcase utility tests ---

TEST(CamelcaseTest, SimpleWord) {
    EXPECT_EQ(camelcase("verbose"), "verbose");
}

TEST(CamelcaseTest, TwoWords) {
    EXPECT_EQ(camelcase("dry-run"), "dryRun");
}

TEST(CamelcaseTest, ThreeWords) {
    EXPECT_EQ(camelcase("no-dry-run"), "noDryRun");
}

TEST(CamelcaseTest, SingleChar) {
    EXPECT_EQ(camelcase("v"), "v");
}

// --- Variadic flag formats ---

TEST(OptionTest, VariadicWithAngleBrackets) {
    Option opt("-v, --variadic <files...>");
    EXPECT_TRUE(opt.variadic);
}

TEST(OptionTest, VariadicWithSquareBrackets) {
    Option opt("--items [values...]");
    EXPECT_TRUE(opt.variadic);
}

TEST(OptionTest, NonVariadic) {
    Option opt("--file <path>");
    EXPECT_FALSE(opt.variadic);
}

// --- Complete fluent chain ---

TEST(OptionTest, FluentChainComplete) {
    Option opt("-p, --port <number>");
    opt.defaultValue(3000, "default port")
       .env("PORT")
       .makeOptionMandatory()
       .argParser([](const std::string& value, const std::any&) -> std::any {
           return std::stoi(value);
       })
       .hideHelp(false);

    EXPECT_TRUE(opt.mandatory);
    EXPECT_FALSE(opt.hidden);
    ASSERT_TRUE(opt.envVar_.has_value());
    EXPECT_EQ(*opt.envVar_, "PORT");
    EXPECT_EQ(std::any_cast<int>(opt.defaultValue_), 3000);
}
