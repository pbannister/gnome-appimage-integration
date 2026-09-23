//
// version_compare_test.cpp: unit tests for the version ordering used to decide
// whether an installed AppImage is older than the newest release.
//
#include "version/version_compare.h"

#include <iostream>
#include <string>

using gnome_appimage::version::compare_versions;

namespace {

int i_failures = 0;

void check(bool b_condition, const std::string &s_description) {
    if (!b_condition) {
        std::cerr << "FAIL: " << s_description << '\n';
        i_failures++;
    }
}

void check_order(const std::string &s_older, const std::string &s_newer) {
    check(0 > compare_versions(s_older, s_newer),
          s_older + " must be older than " + s_newer);
    check(0 < compare_versions(s_newer, s_older),
          s_newer + " must be newer than " + s_older);
    check(0 == compare_versions(s_older, s_older), s_older + " must equal itself");
}

void test_numeric_runs() {
    check_order("1.1.3", "1.1.10");
    check_order("1.1.3", "1.1.4");
    check_order("5.13.0", "26.3.0");
    check_order("2.4.2", "2.5.0");
    check(0 == compare_versions("1.1.3", "1.1.3"), "the same version must compare equal");
    // A leading letter is a letter run against a number, so "v1.1.3" sorts before
    // "1.1.3"; the comparison is stable, which is what callers need.
    check_order("v1.1.3", "1.1.3");
    check(0 == compare_versions("1.1.3", "1.1.3.0"), "a trailing zero must not change it");
}

void test_letters_and_separators() {
    // Regression: the missing-token branch used to invert its answer, so a release with
    // no extra run sorted newer than the next one (1.0 > 1.0.1).
    check_order("1.0", "1.0.1");
    check_order("", "0.0.1");
    check_order("1.0rc1", "1.0");
    check_order("1.0-rc1", "1.0");
    check_order("1.0", "1.0.1");
    check_order("1.0.0", "1.0.1");
    check(0 == compare_versions("1.0", "1.0.0"), "a missing run counts as zero");
    // A trailing letter run loses to the release it follows, the same way 1.0rc1 does.
    check_order("1.1.3+build9", "1.1.3");
    check(0 == compare_versions("2.0-RC1", "2.0-rc1"), "letters compare case-insensitively");
    check_order("1.0alpha1", "1.0beta1");
}

void test_uncomparable_values() {
    // A stable answer, not an exception: an empty version is the oldest thing there is,
    // and two unrelated names compare by their letters.
    check(0 > compare_versions("", "1.0"), "an empty version must sort oldest");
    check(0 == compare_versions("", ""), "two empty versions must compare equal");
    check(0 != compare_versions("banana", "apple"), "unrelated names must still be ordered");
    check(0 == compare_versions("latest", "latest"), "the same word must compare equal");
}

}  // namespace

int main() {
    test_numeric_runs();
    test_letters_and_separators();
    test_uncomparable_values();
    if (0 != i_failures) {
        std::cerr << "version-compare-test: " << i_failures << " checks failed\n";
        return 1;
    }
    std::cout << "version-compare-test: all checks passed\n";
    return 0;
}
