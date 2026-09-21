// Portable unit tests for the desktop entry locator.
// Usage: desktop-entry-locator-test <temporary-root>
#include "desktop/desktop_entry_locator.h"

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <map>
#include <string>
#include <vector>

using gnome_appimage::desktop::desktop_entry_candidate_o;
using gnome_appimage::desktop::desktop_entry_locator_c;

namespace {

namespace fs = std::filesystem;

int g_count_failure = 0;

void check(bool b_condition, const char *s_expression, int i_line) {
    if (!b_condition) {
        std::fprintf(stderr, "FAIL line %d: %s\n", i_line, s_expression);
        g_count_failure++;
    }
}

#define CHECK(expression) check((expression), #expression, __LINE__)

void write_desktop_file(const fs::path &o_path, const std::string &s_name) {
    std::ofstream o_output(o_path);
    o_output << "[Desktop Entry]\nType=Application\nName=" << s_name << "\nExec=true\n";
}

const desktop_entry_candidate_o *find_candidate(
    const std::vector<desktop_entry_candidate_o> &o_candidates, const std::string &s_id) {
    for (const desktop_entry_candidate_o &o_candidate : o_candidates) {
        if (o_candidate.id == s_id) {
            return &o_candidate;
        }
    }
    return nullptr;
}

}  // namespace

int main(int i_argument_count, char **p_arguments) {
    if (2 > i_argument_count) {
        std::fprintf(stderr, "usage: desktop-entry-locator-test <temporary-root>\n");
        return 2;
    }

    const fs::path o_root = p_arguments[1];
    const fs::path o_home = o_root / "home";
    const fs::path o_data_one = o_root / "data1";
    const fs::path o_data_two = o_root / "data2";
    const fs::path o_data_three = o_root / "data3";

    fs::create_directories(o_home);
    fs::create_directories(o_data_one / "applications");
    fs::create_directories(o_data_two / "applications" / "kde");
    fs::create_directories(o_data_three / "applications");

    write_desktop_file(o_data_one / "applications" / "a.desktop", "A user");
    write_desktop_file(o_data_two / "applications" / "b.desktop", "B system");
    write_desktop_file(o_data_two / "applications" / "kde" / "c.desktop", "C nested");
    write_desktop_file(o_data_three / "applications" / "a.desktop", "A shadowed");
    write_desktop_file(o_data_three / "applications" / "ignored.txt", "Not desktop");

    std::map<std::string, std::string> o_environment;
    o_environment["HOME"] = o_home.string();
    o_environment["XDG_DATA_HOME"] = o_data_one.string();
    o_environment["XDG_DATA_DIRS"] =
        o_data_two.string() + ":" + o_data_three.string() + ":relative/path";

    const desktop_entry_locator_c o_locator(o_environment);

    CHECK(3 == o_locator.data_directories().size());
    CHECK(o_data_one.string() == o_locator.data_directories()[0]);
    CHECK(o_data_two.string() == o_locator.data_directories()[1]);
    CHECK(o_data_three.string() == o_locator.data_directories()[2]);

    CHECK(3 == o_locator.application_directories().size());
    CHECK((o_data_one / "applications").string()
          == o_locator.application_directories()[0]);

    CHECK(2 == o_locator.autostart_directories().size());
    CHECK((o_home / ".config" / "autostart").string()
          == o_locator.autostart_directories()[0]);
    CHECK("/etc/xdg/autostart" == o_locator.autostart_directories()[1]);

    const std::vector<desktop_entry_candidate_o> o_candidates = o_locator.list();
    CHECK(3 == o_candidates.size());

    const desktop_entry_candidate_o *p_user = find_candidate(o_candidates, "a.desktop");
    CHECK(nullptr != p_user);
    if (nullptr != p_user) {
        CHECK(o_data_one.string() == p_user->data_directory);
        CHECK(0 == p_user->priority);
        CHECK("a.desktop" == p_user->relative_path);
    }

    const desktop_entry_candidate_o *p_nested = find_candidate(o_candidates, "kde-c.desktop");
    CHECK(nullptr != p_nested);
    if (nullptr != p_nested) {
        CHECK((o_data_two / "applications" / "kde" / "c.desktop").string() == p_nested->path);
        CHECK("kde/c.desktop" == p_nested->relative_path);
    }

    CHECK(!o_locator.locate("missing.desktop").has_value());
    const std::optional<desktop_entry_candidate_o> o_located = o_locator.locate("a.desktop");
    CHECK(o_located.has_value());
    if (o_located.has_value()) {
        CHECK(o_data_one.string() == o_located->data_directory);
    }

    CHECK("a.desktop"
          == desktop_entry_locator_c::desktop_id_for_path(
              (o_data_one / "applications" / "a.desktop").string(), o_data_one.string()));
    CHECK("kde-c.desktop"
          == desktop_entry_locator_c::desktop_id_for_path(
              (o_data_two / "applications" / "kde" / "c.desktop").string(),
              o_data_two.string()));
    CHECK(desktop_entry_locator_c::desktop_id_for_path(
              (o_data_one / "other" / "a.desktop").string(), o_data_one.string())
              .empty());

    std::map<std::string, std::string> o_default_environment;
    o_default_environment["HOME"] = o_home.string();
    const desktop_entry_locator_c o_default_locator(o_default_environment);
    CHECK(3 == o_default_locator.data_directories().size());
    CHECK((o_home / ".local/share").string() == o_default_locator.data_directories()[0]);
    CHECK("/usr/local/share" == o_default_locator.data_directories()[1]);
    CHECK("/usr/share" == o_default_locator.data_directories()[2]);
    CHECK((o_home / ".config").string() == o_default_locator.config_directories()[0]);
    CHECK("/etc/xdg" == o_default_locator.config_directories()[1]);

    if (0 != g_count_failure) {
        std::fprintf(stderr, "desktop-entry-locator-test: %d failure(s)\n", g_count_failure);
        return 1;
    }
    std::printf("desktop-entry-locator-test: all checks passed\n");
    return 0;
}
