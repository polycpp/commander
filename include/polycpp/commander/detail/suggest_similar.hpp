#pragma once

#include <polycpp/commander/suggest_similar.hpp>

#include <algorithm>
#include <cmath>
#include <set>
#include <string>
#include <vector>

namespace polycpp {
namespace commander {

namespace detail {

/**
 * @brief Maximum edit distance to consider a candidate similar.
 */
inline constexpr int kMaxDistance = 3;

/**
 * @brief Minimum similarity ratio (0.0 to 1.0) to consider a candidate.
 */
inline constexpr double kMinSimilarity = 0.4;

/**
 * @brief Compute the Damerau-Levenshtein (optimal string alignment) distance.
 *
 * Uses the standard DP algorithm with transposition support.
 * No substring is edited more than once (OSA variant).
 *
 * @param a First string.
 * @param b Second string.
 * @return The edit distance between a and b.
 */
inline int editDistance(const std::string& a, const std::string& b) {
    // Quick early exit: return worst case if length difference exceeds max.
    if (std::abs(static_cast<int>(a.size()) - static_cast<int>(b.size())) > kMaxDistance) {
        return static_cast<int>(std::max(a.size(), b.size()));
    }

    const auto m = a.size();
    const auto n = b.size();

    // Distance between prefix substrings of a and b.
    // d[i][j] = distance between a[0..i-1] and b[0..j-1].
    std::vector<std::vector<int>> d(m + 1, std::vector<int>(n + 1, 0));

    // Pure deletions turn a into empty string.
    for (std::size_t i = 0; i <= m; ++i) {
        d[i][0] = static_cast<int>(i);
    }
    // Pure insertions turn empty string into b.
    for (std::size_t j = 0; j <= n; ++j) {
        d[0][j] = static_cast<int>(j);
    }

    // Fill matrix.
    for (std::size_t j = 1; j <= n; ++j) {
        for (std::size_t i = 1; i <= m; ++i) {
            int cost = (a[i - 1] == b[j - 1]) ? 0 : 1;

            d[i][j] = std::min({
                d[i - 1][j] + 1,       // deletion
                d[i][j - 1] + 1,       // insertion
                d[i - 1][j - 1] + cost // substitution
            });

            // Transposition.
            if (i > 1 && j > 1 && a[i - 1] == b[j - 2] && a[i - 2] == b[j - 1]) {
                d[i][j] = std::min(d[i][j], d[i - 2][j - 2] + 1);
            }
        }
    }

    return d[m][n];
}

} // namespace detail

inline std::string suggestSimilar(const std::string& word, const std::vector<std::string>& candidates) {
    if (candidates.empty()) {
        return "";
    }

    // Remove duplicates.
    std::set<std::string> uniqueSet(candidates.begin(), candidates.end());
    std::vector<std::string> uniqueCandidates(uniqueSet.begin(), uniqueSet.end());

    // If searching for options (word starts with "--"), strip the prefix for comparison.
    bool searchingOptions = (word.size() >= 2 && word[0] == '-' && word[1] == '-');

    std::string searchWord = word;
    if (searchingOptions) {
        searchWord = word.substr(2);
        for (auto& c : uniqueCandidates) {
            if (c.size() >= 2 && c[0] == '-' && c[1] == '-') {
                c = c.substr(2);
            }
        }
    }

    std::vector<std::string> similar;
    int bestDistance = detail::kMaxDistance;

    for (const auto& candidate : uniqueCandidates) {
        // No one-character guesses.
        if (candidate.size() <= 1) {
            continue;
        }

        const int distance = detail::editDistance(searchWord, candidate);
        const auto length = std::max(searchWord.size(), candidate.size());
        const double similarity = static_cast<double>(length - static_cast<std::size_t>(distance))
                                / static_cast<double>(length);

        if (similarity > detail::kMinSimilarity) {
            if (distance < bestDistance) {
                // Better edit distance: throw away previous worse matches.
                bestDistance = distance;
                similar.clear();
                similar.push_back(candidate);
            } else if (distance == bestDistance) {
                similar.push_back(candidate);
            }
        }
    }

    std::sort(similar.begin(), similar.end());

    if (searchingOptions) {
        for (auto& s : similar) {
            s = "--" + s;
        }
    }

    if (similar.size() > 1) {
        std::string joined;
        for (std::size_t i = 0; i < similar.size(); ++i) {
            if (i > 0) {
                joined += ", ";
            }
            joined += similar[i];
        }
        return "\n(Did you mean one of " + joined + "?)";
    }
    if (similar.size() == 1) {
        return "\n(Did you mean " + similar[0] + "?)";
    }
    return "";
}

} // namespace commander
} // namespace polycpp
