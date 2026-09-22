// Portable unit tests for the MIME association reader.
// Usage: mime-association-reader-test <temporary-root>
#include "desktop/mime_association_reader.h"

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <map>
#include <string>

using gnome_appimage::desktop::mime_association_reader_c;
using gnome_appimage::desktop::mime_lookup_o;

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

void write_text_file(const fs::path &o_path, const std::string &s_text) {
    std::ofstream o_output(o_path);
    o_output << s_text;
}

void write_desktop_file(const fs::path &o_path, const std::string &s_name,
                        const std::string &s_exec) {
    write_text_file(o_path, "[Desktop Entry]\nType=Application\nName=" + s_name + "\nExec="
                                + s_exec + "\n");
}

}  // namespace

int main(int i_argument_count, char **p_arguments) {
    if (2 > i_argument_count) {
        std::fprintf(stderr, "usage: mime-association-reader-test <temporary-root>\n");
        return 2;
    }

    const fs::path o_root = p_arguments[1];
    const fs::path o_home = o_root / "home";
    const fs::path o_config_home = o_home / ".config";
    const fs::path o_data_home = o_home / ".local" / "share";
    const fs::path o_config_directory = o_root / "etc" / "xdg";
    const fs::path o_data_directory = o_root / "data";

    fs::create_directories(o_config_home);
    fs::create_directories(o_data_home / "applications");
    fs::create_directories(o_config_directory);
    fs::create_directories(o_data_directory / "applications" / "kde");

    write_text_file(o_config_home / "mimeapps.list",
                    "[Default Applications]\n"
                    "application/x-test=first.desktop;second.desktop;\n"
                    "application/x-removed=removed.desktop;\n"
                    "[Removed Associations]\n"
                    "application/x-test=removed.desktop;\n");
    write_text_file(o_config_directory / "mimeapps.list",
                    "[Default Applications]\n"
                    "application/x-other=third.desktop;\n");

    write_desktop_file(o_data_home / "applications" / "second.desktop", "Second", "second");
    write_desktop_file(o_data_directory / "applications" / "third.desktop", "Third", "third");
    write_desktop_file(o_data_directory / "applications" / "kde" / "fourth.desktop", "Fourth",
                       "fourth");

    std::map<std::string, std::string> o_environment;
    o_environment["HOME"] = o_home.string();
    o_environment["XDG_DATA_HOME"] = o_data_home.string();
    o_environment["XDG_DATA_DIRS"] = o_data_directory.string();
    o_environment["XDG_CONFIG_HOME"] = o_config_home.string();
    o_environment["XDG_CONFIG_DIRS"] = o_config_directory.string();

    const mime_association_reader_c o_reader(o_environment);

    CHECK(4 <= o_reader.configuration_files().size());
    CHECK((o_config_home / "mimeapps.list").string() == o_reader.configuration_files()[0]);
    CHECK((o_config_directory / "mimeapps.list").string() == o_reader.configuration_files()[1]);
    CHECK((o_data_home / "applications" / "mimeapps.list").string()
          == o_reader.configuration_files()[2]);
    CHECK((o_data_directory / "applications" / "mimeapps.list").string()
          == o_reader.configuration_files()[3]);

    CHECK((o_data_home / "applications" / "second.desktop").string()
          == o_reader.resolve_desktop_id("second.desktop"));
    CHECK((o_data_directory / "applications" / "kde" / "fourth.desktop").string()
          == o_reader.resolve_desktop_id("kde-fourth.desktop"));
    CHECK(o_reader.resolve_desktop_id("missing.desktop").empty());

    // Only second.desktop exists, so the first ID falls through to the second.
    const mime_lookup_o o_test = o_reader.lookup("application/x-test");
    CHECK(o_test.found);
    CHECK("application/x-test" == o_test.mime_type);
    CHECK("second.desktop" == o_test.default_application.desktop_id);
    CHECK((o_data_home / "applications" / "second.desktop").string()
          == o_test.default_application.desktop_path);
    CHECK((o_config_home / "mimeapps.list").string() == o_test.default_application.source_file);
    CHECK("Default Applications" == o_test.default_application.group);
    CHECK(0 == o_test.default_application.priority);
    CHECK(!o_test.associations.empty());
    CHECK(o_reader.configuration_files().size() == o_test.searched_files.size());

    // A lower-precedence data file supplies a default when no config file does.
    const mime_lookup_o o_other = o_reader.lookup("application/x-other");
    CHECK(o_other.found);
    CHECK("third.desktop" == o_other.default_application.desktop_id);
    CHECK((o_data_directory / "applications" / "third.desktop").string()
          == o_other.default_application.desktop_path);
    CHECK((o_config_directory / "mimeapps.list").string()
          == o_other.default_application.source_file);

    const mime_lookup_o o_absent = o_reader.lookup("application/nope");
    CHECK(!o_absent.found);
    CHECK("application/nope" == o_absent.mime_type);
    CHECK(o_absent.associations.empty());

    // Creating first.desktop makes the first ID in the value win again.
    write_desktop_file(o_data_home / "applications" / "first.desktop", "First", "first");
    const mime_association_reader_c o_reader_after(o_environment);
    const mime_lookup_o o_test_after = o_reader_after.lookup("application/x-test");
    CHECK(o_test_after.found);
    CHECK("first.desktop" == o_test_after.default_application.desktop_id);
    CHECK((o_data_home / "applications" / "first.desktop").string()
          == o_test_after.default_application.desktop_path);

    if (0 != g_count_failure) {
        std::fprintf(stderr, "mime-association-reader-test: %d failure(s)\n", g_count_failure);
        return 1;
    }
    std::printf("mime-association-reader-test: all checks passed\n");
    return 0;
}
