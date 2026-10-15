// Unit tests for the AppImage specification's desktop-integration conditions.
//
// The check is a pure function so that every condition can be exercised, including the
// two system-wide sentinel paths, which no test could create under /usr or /etc.
//
// Usage: desktop-integration-optout-test
#include "integration/appimage_integrator.h"

#include <cstdio>
#include <map>
#include <string>
#include <vector>

using gnome_appimage::integration::desktop_integration_optouts;
using gnome_appimage::integration::integration_optout_o;
using gnome_appimage::integration::integration_optout_sentence;

namespace {

int g_count_failure = 0;

void check(bool b_condition, const char *s_expression, int i_line) {
    if (!b_condition) {
        std::fprintf(stderr, "FAIL line %d: %s\n", i_line, s_expression);
        g_count_failure++;
    }
}

#define CHECK(expression) check((expression), #expression, __LINE__)

std::string sentence_for(const std::vector<integration_optout_o> &o_optouts,
                         const std::string &s_needle) {
    for (const integration_optout_o &o_optout : o_optouts) {
        if (std::string::npos != o_optout.condition.find(s_needle)) {
            return integration_optout_sentence(o_optout);
        }
    }
    return {};
}

}  // namespace

int main() {
    const std::map<std::string, std::string> o_quiet;
    const std::vector<std::string> o_no_paths = {"/nonexistent/one", "/nonexistent/two",
                                                 "/nonexistent/three"};

    // Nothing holds: the usual case, and the one that must stay silent.
    CHECK(0 == desktop_integration_optouts(o_quiet, o_no_paths, false).size());

    // A sentinel that is there is reported, and the sentence names it.
    const std::string s_file = "/tmp/desktop-integration-optout-test.sentinel";
    std::FILE *p_sentinel = std::fopen(s_file.c_str(), "w");
    CHECK(nullptr != p_sentinel);
    if (nullptr != p_sentinel) {
        std::fclose(p_sentinel);
    }
    {
        const std::vector<integration_optout_o> o_found =
            desktop_integration_optouts(o_quiet, {"/nonexistent/one", s_file}, false);
        CHECK(1 == o_found.size());
        if (1 == o_found.size()) {
            CHECK(s_file == o_found[0].condition);
        }
        CHECK(!sentence_for(o_found, s_file).empty());
        CHECK(std::string::npos != sentence_for(o_found, s_file).find("specification"));
    }
    std::remove(s_file.c_str());

    // Every path that exists is reported, not only the first, because these are warnings
    // shown together rather than a reason to stop.
    const std::string s_second = "/tmp/desktop-integration-optout-test.sentinel-two";
    std::FILE *p_second = std::fopen(s_second.c_str(), "w");
    if (nullptr != p_second) {
        std::fclose(p_second);
    }
    {
        std::FILE *p_first = std::fopen(s_file.c_str(), "w");
        if (nullptr != p_first) {
            std::fclose(p_first);
        }
        const std::vector<integration_optout_o> o_found =
            desktop_integration_optouts(o_quiet, {s_file, s_second}, false);
        CHECK(2 == o_found.size());
        // Specification order is the order given, so the first path is reported first.
        if (2 == o_found.size()) {
            CHECK(s_file == o_found[0].condition);
            CHECK(s_second == o_found[1].condition);
        }
    }
    std::remove(s_file.c_str());
    std::remove(s_second.c_str());

    // A running appimaged is named as the condition.
    {
        const std::vector<integration_optout_o> o_found =
            desktop_integration_optouts(o_quiet, o_no_paths, true);
        CHECK(1 == o_found.size());
        CHECK(!sentence_for(o_found, "appimaged").empty());
    }

    // DESKTOPINTEGRATION, set and unset.  An empty value is not a value.
    {
        const std::map<std::string, std::string> o_set = {{"DESKTOPINTEGRATION", "1"}};
        const std::vector<integration_optout_o> o_found =
            desktop_integration_optouts(o_set, o_no_paths, false);
        CHECK(1 == o_found.size());
        CHECK(!sentence_for(o_found, "DESKTOPINTEGRATION").empty());

        const std::map<std::string, std::string> o_empty = {{"DESKTOPINTEGRATION", ""}};
        CHECK(0 == desktop_integration_optouts(o_empty, o_no_paths, false).size());
    }

    // All of them at once.
    {
        std::FILE *p_all = std::fopen(s_file.c_str(), "w");
        if (nullptr != p_all) {
            std::fclose(p_all);
        }
        const std::map<std::string, std::string> o_set = {{"DESKTOPINTEGRATION", "yes"}};
        const std::vector<integration_optout_o> o_found =
            desktop_integration_optouts(o_set, {s_file}, true);
        CHECK(3 == o_found.size());
        std::remove(s_file.c_str());
    }

    // The environment decides where the user-level sentinel is, and an unset
    // XDG_DATA_HOME falls back to $HOME/.local/share: both are the caller's job here,
    // so what this checks is that an empty path is skipped rather than looked for.
    CHECK(0 == desktop_integration_optouts(o_quiet, {""}, false).size());

    if (0 != g_count_failure) {
        std::fprintf(stderr, "desktop-integration-optout-test: %d failure(s)\n",
                     g_count_failure);
        return 1;
    }
    std::printf("desktop-integration-optout-test: all checks passed\n");
    return 0;
}
