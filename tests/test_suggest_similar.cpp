#include <polycpp/commander/detail/suggest_similar.hpp>

#include <gtest/gtest.h>

#include <string>
#include <vector>

using polycpp::commander::suggestSimilar;

// Helper to extract just the suggestion text (without newline prefix and parentheses)
// for easier assertion. Returns empty string if no suggestion.
static std::string extractSuggestion(const std::string& result) {
    return result;
}

// -- Basic behavior tests --

TEST(SuggestSimilarTest, ExactMatchNotSuggested) {
    // An exact match is distance 0, which is < bestDistance (3), so it will
    // actually be included. But commander.js doesn't call suggestSimilar with
    // the word itself in candidates. We test this mirrors the JS behavior where
    // exact matches ARE returned (distance 0 is valid).
    auto result = suggestSimilar("serve", {"serve", "start", "stop"});
    EXPECT_EQ(result, "\n(Did you mean serve?)");
}

TEST(SuggestSimilarTest, CloseMatch) {
    auto result = suggestSimilar("serv", {"serve", "start", "stop"});
    EXPECT_EQ(result, "\n(Did you mean serve?)");
}

TEST(SuggestSimilarTest, Transposition) {
    // "sever" -> "serve" is a transposition + other edits
    auto result = suggestSimilar("act", {"cat"});
    EXPECT_EQ(result, "\n(Did you mean cat?)");
}

TEST(SuggestSimilarTest, NoMatch) {
    auto result = suggestSimilar("yyy", {"zzz"});
    EXPECT_EQ(result, "");
}

TEST(SuggestSimilarTest, EmptyCandidates) {
    auto result = suggestSimilar("serve", {});
    EXPECT_EQ(result, "");
}

TEST(SuggestSimilarTest, EmptyWord) {
    auto result = suggestSimilar("", {"serve", "start"});
    EXPECT_EQ(result, "");
}

TEST(SuggestSimilarTest, MultipleSuggestions) {
    auto result = suggestSimilar("xat", {"rat", "cat", "bat"});
    EXPECT_EQ(result, "\n(Did you mean one of bat, cat, rat?)");
}

TEST(SuggestSimilarTest, CaseSensitive) {
    // "Serve" vs "serve" differ by 1 substitution, distance=1, still similar
    // But "Serve" vs "start" is very different
    auto result = suggestSimilar("Serve", {"serve", "start", "stop"});
    EXPECT_EQ(result, "\n(Did you mean serve?)");
}

TEST(SuggestSimilarTest, SingleCharDifference) {
    // "instll" -> "install" is distance 1 (insertion of 'a')
    auto result = suggestSimilar("instll", {"install", "remove", "update"});
    EXPECT_EQ(result, "\n(Did you mean install?)");
}

// -- Tests ported from commander.js help.suggestion.test.js (subcommand scenario) --

TEST(SuggestSimilarTest, NoneSimilar) {
    auto result = suggestSimilar("yyy", {"zzz"});
    EXPECT_EQ(result, "");
}

TEST(SuggestSimilarTest, OneEditAwayButNotSimilar) {
    // "a" vs "b": distance 1, length 1, similarity = 0.0 -> not enough
    auto result = suggestSimilar("a", {"b"});
    EXPECT_EQ(result, "");
}

TEST(SuggestSimilarTest, OneEditAwayShortStringAccepted) {
    // "a" vs "ab": distance 1, length 2, similarity = 0.5 > 0.4
    // But "ab" has length 2 > 1, so it passes the length filter.
    auto result = suggestSimilar("a", {"ab"});
    EXPECT_EQ(result, "\n(Did you mean ab?)");
}

TEST(SuggestSimilarTest, OneEditAwayShortCandidateRejected) {
    // "ab" vs "a": candidate length is 1, skipped (no one-char guesses)
    auto result = suggestSimilar("ab", {"a"});
    EXPECT_EQ(result, "");
}

TEST(SuggestSimilarTest, OneInsertion) {
    auto result = suggestSimilar("at", {"cat"});
    EXPECT_EQ(result, "\n(Did you mean cat?)");
}

TEST(SuggestSimilarTest, OneDeletion) {
    auto result = suggestSimilar("cat", {"at"});
    EXPECT_EQ(result, "\n(Did you mean at?)");
}

TEST(SuggestSimilarTest, OneSubstitution) {
    auto result = suggestSimilar("bat", {"cat"});
    EXPECT_EQ(result, "\n(Did you mean cat?)");
}

TEST(SuggestSimilarTest, OneTransposition) {
    auto result = suggestSimilar("act", {"cat"});
    EXPECT_EQ(result, "\n(Did you mean cat?)");
}

TEST(SuggestSimilarTest, TwoEditsShortString) {
    // "cxx" vs "cat": distance 2, length 3, similarity = 0.333 < 0.4
    auto result = suggestSimilar("cxx", {"cat"});
    EXPECT_EQ(result, "");
}

TEST(SuggestSimilarTest, TwoEditsLongerString) {
    // "caxx" vs "cart": distance 2, length 4, similarity = 0.5 > 0.4
    auto result = suggestSimilar("caxx", {"cart"});
    EXPECT_EQ(result, "\n(Did you mean cart?)");
}

TEST(SuggestSimilarTest, ThreeEditsLongStringSimilar) {
    // "1234567" vs "1234567890": distance 3, length 10, similarity = 0.7 > 0.4
    auto result = suggestSimilar("1234567", {"1234567890"});
    EXPECT_EQ(result, "\n(Did you mean 1234567890?)");
}

TEST(SuggestSimilarTest, FourEditsTooFar) {
    // "123456" vs "1234567890": distance 4 > maxDistance(3)
    auto result = suggestSimilar("123456", {"1234567890"});
    EXPECT_EQ(result, "");
}

TEST(SuggestSimilarTest, SortedPossibles) {
    auto result = suggestSimilar("xat", {"rat", "cat", "bat"});
    EXPECT_EQ(result, "\n(Did you mean one of bat, cat, rat?)");
}

TEST(SuggestSimilarTest, OnlyClosestOfDifferentDistances) {
    // "cart" vs "camb" (distance 2), "cant" (distance 1), "bard" (distance 2)
    // Only "cant" returned since it has best distance.
    auto result = suggestSimilar("cart", {"camb", "cant", "bard"});
    EXPECT_EQ(result, "\n(Did you mean cant?)");
}

// -- Option-style tests (with -- prefix) --

TEST(SuggestSimilarTest, OptionNoneSimilar) {
    auto result = suggestSimilar("--yyy", {"--zzz"});
    EXPECT_EQ(result, "");
}

TEST(SuggestSimilarTest, OptionOneEditAwayNotSimilar) {
    auto result = suggestSimilar("--a", {"--b"});
    EXPECT_EQ(result, "");
}

TEST(SuggestSimilarTest, OptionOneEditAwayAccepted) {
    auto result = suggestSimilar("--a", {"--ab"});
    EXPECT_EQ(result, "\n(Did you mean --ab?)");
}

TEST(SuggestSimilarTest, OptionOneEditAwayShortCandidateRejected) {
    auto result = suggestSimilar("--ab", {"--a"});
    EXPECT_EQ(result, "");
}

TEST(SuggestSimilarTest, OptionOneInsertion) {
    auto result = suggestSimilar("--at", {"--cat"});
    EXPECT_EQ(result, "\n(Did you mean --cat?)");
}

TEST(SuggestSimilarTest, OptionOneDeletion) {
    auto result = suggestSimilar("--cat", {"--at"});
    EXPECT_EQ(result, "\n(Did you mean --at?)");
}

TEST(SuggestSimilarTest, OptionOneSubstitution) {
    auto result = suggestSimilar("--bat", {"--cat"});
    EXPECT_EQ(result, "\n(Did you mean --cat?)");
}

TEST(SuggestSimilarTest, OptionOneTransposition) {
    auto result = suggestSimilar("--act", {"--cat"});
    EXPECT_EQ(result, "\n(Did you mean --cat?)");
}

TEST(SuggestSimilarTest, OptionTwoEditsShortString) {
    auto result = suggestSimilar("--cxx", {"--cat"});
    EXPECT_EQ(result, "");
}

TEST(SuggestSimilarTest, OptionTwoEditsLongerString) {
    auto result = suggestSimilar("--caxx", {"--cart"});
    EXPECT_EQ(result, "\n(Did you mean --cart?)");
}

TEST(SuggestSimilarTest, OptionThreeEditsLongStringSimilar) {
    auto result = suggestSimilar("--1234567", {"--1234567890"});
    EXPECT_EQ(result, "\n(Did you mean --1234567890?)");
}

TEST(SuggestSimilarTest, OptionFourEditsTooFar) {
    auto result = suggestSimilar("--123456", {"--1234567890"});
    EXPECT_EQ(result, "");
}

TEST(SuggestSimilarTest, OptionSortedPossibles) {
    auto result = suggestSimilar("--xat", {"--rat", "--cat", "--bat"});
    EXPECT_EQ(result, "\n(Did you mean one of --bat, --cat, --rat?)");
}

TEST(SuggestSimilarTest, OptionOnlyClosestDistance) {
    auto result = suggestSimilar("--cart", {"--camb", "--cant", "--bard"});
    EXPECT_EQ(result, "\n(Did you mean --cant?)");
}

// -- Deduplication test --

TEST(SuggestSimilarTest, DuplicateCandidatesDeduped) {
    auto result = suggestSimilar("serv", {"serve", "serve", "serve"});
    EXPECT_EQ(result, "\n(Did you mean serve?)");
}

// -- Edit distance unit tests --

TEST(EditDistanceTest, IdenticalStrings) {
    EXPECT_EQ(polycpp::commander::detail::editDistance("hello", "hello"), 0);
}

TEST(EditDistanceTest, SingleInsertion) {
    EXPECT_EQ(polycpp::commander::detail::editDistance("at", "cat"), 1);
}

TEST(EditDistanceTest, SingleDeletion) {
    EXPECT_EQ(polycpp::commander::detail::editDistance("cat", "at"), 1);
}

TEST(EditDistanceTest, SingleSubstitution) {
    EXPECT_EQ(polycpp::commander::detail::editDistance("bat", "cat"), 1);
}

TEST(EditDistanceTest, SingleTransposition) {
    EXPECT_EQ(polycpp::commander::detail::editDistance("act", "cat"), 1);
}

TEST(EditDistanceTest, EmptyStrings) {
    EXPECT_EQ(polycpp::commander::detail::editDistance("", ""), 0);
    EXPECT_EQ(polycpp::commander::detail::editDistance("abc", ""), 3);
    EXPECT_EQ(polycpp::commander::detail::editDistance("", "abc"), 3);
}

TEST(EditDistanceTest, LargeDistanceEarlyExit) {
    // Length difference > 3, should return worst case quickly
    EXPECT_EQ(polycpp::commander::detail::editDistance("a", "abcde"), 5);
}
