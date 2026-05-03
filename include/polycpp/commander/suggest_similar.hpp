#pragma once

/**
 * @file suggest_similar.hpp
 * @brief "Did you mean ...?" suggestions using Damerau-Levenshtein distance.
 * @see https://github.com/tj/commander.js
 * @since 1.0.0
 */

#include <string>
#include <vector>

namespace polycpp {
namespace commander {

/**
 * @brief Suggest similar strings from a list of candidates.
 *
 * Uses Damerau-Levenshtein edit distance to find candidates similar to the
 * given word. Candidates with the best (lowest) edit distance are returned,
 * provided they exceed a minimum similarity threshold. If the word and
 * candidates start with "--", the prefix is stripped before comparison and
 * re-added to the results.
 *
 * @param word The input word to find suggestions for.
 * @param candidates The list of possible matches.
 * @return A suggestion string like "\\n(Did you mean serve?)" or
 *         "\\n(Did you mean one of bat, cat, rat?)" or empty string if no match.
 *
 * @par Example
 * @code{.cpp}
 *   auto suggestion = suggestSimilar("serv", {"serve", "start", "stop"});
 *   // suggestion == "\n(Did you mean serve?)"
 *
 *   auto suggestion2 = suggestSimilar("--hepl", {"--help", "--version"});
 *   // suggestion2 == "\n(Did you mean --help?)"
 * @endcode
 *
 * @see https://github.com/tj/commander.js
 * @since 1.0.0
 */
std::string suggestSimilar(const std::string& word, const std::vector<std::string>& candidates);

} // namespace commander
} // namespace polycpp
