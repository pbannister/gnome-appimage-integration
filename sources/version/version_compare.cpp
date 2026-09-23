#include "version/version_compare.h"

#include <algorithm>
#include <cctype>
#include <vector>

namespace gnome_appimage::version {

namespace {

// A version string, split into the runs a person reads: digits stay together and
// compare as numbers, letters stay together and compare case-insensitively, and
// separators are not tokens.  So 1.1.3 < 1.1.10, and 26.3.0 > 5.13.0.
std::vector<std::string> version_tokens(const std::string &s_version) {
    std::vector<std::string> o_tokens;
    std::string s_current;
    bool b_current_is_digits = false;
    const auto flush = [&o_tokens, &s_current, &b_current_is_digits]() {
        if (!s_current.empty()) {
            o_tokens.push_back(s_current);
            s_current.clear();
        }
        b_current_is_digits = false;
    };
    for (const char c_character : s_version) {
        const unsigned char u_character = static_cast<unsigned char>(c_character);
        const bool b_digits = 0 != std::isdigit(u_character);
        const bool b_letters = 0 != std::isalpha(u_character);
        if (!b_digits && !b_letters) {
            flush();
            continue;
        }
        if (!s_current.empty() && b_digits != b_current_is_digits) {
            flush();
        }
        if (s_current.empty()) {
            b_current_is_digits = b_digits;
        }
        s_current += static_cast<char>(b_digits ? c_character
                                                : static_cast<char>(std::tolower(u_character)));
    }
    flush();
    return o_tokens;
}

bool is_digit_token(const std::string &s_token) {
    for (const char c_character : s_token) {
        if (0 == std::isdigit(static_cast<unsigned char>(c_character))) {
            return false;
        }
    }
    return !s_token.empty();
}

// Compare two digit runs as numbers, without converting them.
int compare_digit_tokens(const std::string &s_left, const std::string &s_right) {
    std::size_t i_left = s_left.find_first_not_of('0');
    std::size_t i_right = s_right.find_first_not_of('0');
    const std::string s_left_trimmed =
        std::string::npos == i_left ? std::string() : s_left.substr(i_left);
    const std::string s_right_trimmed =
        std::string::npos == i_right ? std::string() : s_right.substr(i_right);
    if (s_left_trimmed.size() != s_right_trimmed.size()) {
        return s_left_trimmed.size() < s_right_trimmed.size() ? -1 : 1;
    }
    if (s_left_trimmed == s_right_trimmed) {
        return 0;
    }
    return s_left_trimmed < s_right_trimmed ? -1 : 1;
}

}  // namespace

int compare_versions(const std::string &s_left, const std::string &s_right) {
    const std::vector<std::string> o_left = version_tokens(s_left);
    const std::vector<std::string> o_right = version_tokens(s_right);
    const std::size_t u_count = std::max(o_left.size(), o_right.size());
    for (std::size_t i_index = 0; i_index < u_count; i_index++) {
        const bool b_left_present = i_index < o_left.size();
        const bool b_right_present = i_index < o_right.size();
        const std::string s_left_token = b_left_present ? o_left[i_index] : std::string();
        const std::string s_right_token = b_right_present ? o_right[i_index] : std::string();
        if (!b_left_present && !b_right_present) {
            return 0;
        }
        const bool b_left_digits = b_left_present && is_digit_token(s_left_token);
        const bool b_right_digits = b_right_present && is_digit_token(s_right_token);
        if (b_left_present != b_right_present) {
            // The missing side is a release: it beats letters and loses to a number.
            const bool b_present_is_digits = b_left_present ? b_left_digits : b_right_digits;
            if (b_present_is_digits) {
                const std::string &s_number = b_left_present ? s_left_token : s_right_token;
                // The comparison is zero-against-number, so it is the answer for the
                // side that is missing; when that side is the right one, flip it.
                const int i_compare = compare_digit_tokens("0", s_number);
                if (0 != i_compare) {
                    return b_left_present ? -i_compare : i_compare;
                }
                continue;
            }
            return b_left_present ? -1 : 1;
        }
        if (b_left_digits && b_right_digits) {
            const int i_compare = compare_digit_tokens(s_left_token, s_right_token);
            if (0 != i_compare) {
                return i_compare;
            }
            continue;
        }
        if (b_left_digits != b_right_digits) {
            return b_left_digits ? 1 : -1;
        }
        if (s_left_token != s_right_token) {
            return s_left_token < s_right_token ? -1 : 1;
        }
    }
    return 0;
}

}  // namespace gnome_appimage::version
