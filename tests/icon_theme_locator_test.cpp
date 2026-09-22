// Portable unit tests for the icon theme locator.
// Usage: icon-theme-locator-test <temporary-root>
#include "desktop/icon_theme_locator.h"

#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <map>
#include <string>
#include <system_error>
#include <vector>

using gnome_appimage::desktop::icon_candidate_o;
using gnome_appimage::desktop::icon_lookup_o;
using gnome_appimage::desktop::icon_theme_locator_c;

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

void write_file(const fs::path &o_path, const std::string &s_content) {
    std::ofstream o_output(o_path);
    o_output << s_content;
}

// Return the first candidate whose path contains the fragment, or nullptr.
const icon_candidate_o *find_candidate(const std::vector<icon_candidate_o> &o_candidates,
                                       const std::string &s_path_fragment) {
    for (const icon_candidate_o &o_candidate : o_candidates) {
        if (std::string::npos != o_candidate.path.find(s_path_fragment)) {
            return &o_candidate;
        }
    }
    return nullptr;
}

// Return the index of the first candidate whose path contains the fragment, or -1.
int index_of_candidate(const std::vector<icon_candidate_o> &o_candidates,
                       const std::string &s_path_fragment) {
    for (std::size_t i_index = 0; i_index < o_candidates.size(); i_index++) {
        if (std::string::npos != o_candidates[i_index].path.find(s_path_fragment)) {
            return static_cast<int>(i_index);
        }
    }
    return -1;
}

bool contains(const std::vector<std::string> &o_values, const std::string &s_value) {
    return o_values.end() != std::find(o_values.begin(), o_values.end(), s_value);
}

}  // namespace

int main(int i_argument_count, char **p_arguments) {
    if (2 > i_argument_count) {
        std::fprintf(stderr, "usage: icon-theme-locator-test <temporary-root>\n");
        return 2;
    }

    const fs::path o_root = p_arguments[1];
    const fs::path o_home = o_root / "home";
    const fs::path o_data_home = o_home / ".local" / "share";
    const fs::path o_data = o_root / "data";

    const fs::path o_hicolor = o_data_home / "icons" / "hicolor";
    const fs::path o_adwaita = o_data_home / "icons" / "Adwaita";
    const fs::path o_data_hicolor = o_data / "icons" / "hicolor";

    fs::create_directories(o_hicolor / "48x48" / "apps");
    fs::create_directories(o_hicolor / "scalable" / "apps");
    fs::create_directories(o_hicolor / "256x256" / "apps");
    fs::create_directories(o_data_hicolor / "48x48" / "apps");
    fs::create_directories(o_adwaita / "24x24" / "apps");
    fs::create_directories(o_data_home / "pixmaps");
    fs::create_directories(o_home / ".config");

    write_file(o_hicolor / "48x48" / "apps" / "example.png", "png");
    write_file(o_hicolor / "scalable" / "apps" / "example.svg", "svg");
    write_file(o_hicolor / "256x256" / "apps" / "example.png", "png");
    write_file(o_data_hicolor / "48x48" / "apps" / "example.png", "png");
    write_file(o_adwaita / "24x24" / "apps" / "example.png", "png");
    write_file(o_adwaita / "index.theme", "[Icon Theme]\nName=Adwaita\nInherits=hicolor\n");
    // This pixmaps file is intentionally not on the search path: the locator
    // only knows the system pixmaps directory, so a data-home pixmaps tree is
    // never consulted.  It is created here to make that negative explicit.
    write_file(o_data_home / "pixmaps" / "legacy.png", "png");

    const fs::path o_absolute_icon = o_root / "absolute-icon.png";
    write_file(o_absolute_icon, "png");

    std::map<std::string, std::string> o_environment;
    o_environment["HOME"] = o_home.string();
    o_environment["XDG_DATA_HOME"] = o_data_home.string();
    o_environment["XDG_DATA_DIRS"] = o_data.string() + ":/usr/share";
    o_environment["XDG_CONFIG_HOME"] = (o_home / ".config").string();

    const icon_theme_locator_c o_locator(o_environment);

    // Base icon directories keep the documented precedence order.
    CHECK(!o_locator.icon_base_directories().empty());
    CHECK((o_data_home / "icons").string() == o_locator.icon_base_directories()[0]);
    CHECK((o_data / "icons").string() == o_locator.icon_base_directories()[1]);

    // Preferred theme first, then its inherited theme, then hicolor.
    const icon_lookup_o o_adwaita_lookup = o_locator.lookup("example", "Adwaita");
    CHECK(o_adwaita_lookup.found);
    CHECK("example" == o_adwaita_lookup.icon_name);
    CHECK(!o_adwaita_lookup.candidates.empty());
    if (!o_adwaita_lookup.candidates.empty()) {
        CHECK("Adwaita" == o_adwaita_lookup.candidates.front().theme);
        CHECK((o_adwaita / "24x24" / "apps" / "example.png").string()
              == o_adwaita_lookup.candidates.front().path);
        CHECK(0 == o_adwaita_lookup.candidates.front().priority);
        CHECK("24x24" == o_adwaita_lookup.candidates.front().size);
        CHECK("apps" == o_adwaita_lookup.candidates.front().context);
    }
    CHECK((o_adwaita / "24x24" / "apps" / "example.png").string()
          == o_adwaita_lookup.best_path);
    CHECK(nullptr
          != find_candidate(o_adwaita_lookup.candidates,
                            (o_hicolor / "48x48" / "apps" / "example.png").string()));
    CHECK(nullptr
          != find_candidate(o_adwaita_lookup.candidates,
                            (o_data_hicolor / "48x48" / "apps" / "example.png").string()));
    CHECK(!o_adwaita_lookup.searched_themes.empty());
    CHECK("Adwaita" == o_adwaita_lookup.searched_themes.front());
    CHECK(contains(o_adwaita_lookup.searched_themes, "hicolor"));

    // A hicolor lookup orders scalable before the raster sizes.
    const icon_lookup_o o_hicolor_lookup = o_locator.lookup("example", "hicolor");
    CHECK(o_hicolor_lookup.found);
    CHECK(!o_hicolor_lookup.candidates.empty());
    if (!o_hicolor_lookup.candidates.empty()) {
        CHECK(std::string::npos
              != o_hicolor_lookup.candidates.front().path.find(
                  (o_hicolor / "scalable" / "apps" / "example.svg").string()));
    }
    const int i_scalable =
        index_of_candidate(o_hicolor_lookup.candidates,
                           (o_hicolor / "scalable" / "apps" / "example.svg").string());
    const int i_256 =
        index_of_candidate(o_hicolor_lookup.candidates,
                           (o_hicolor / "256x256" / "apps" / "example.png").string());
    CHECK(0 <= i_scalable);
    CHECK(0 <= i_256);
    CHECK(i_scalable < i_256);

    // The module adds only the system pixmaps directory, so the data-home
    // pixmaps tree is not searched and "legacy" is not found.  This is the
    // documented consequence of not adding <XDG_DATA_HOME>/pixmaps to the
    // base list.
    const icon_lookup_o o_legacy_lookup = o_locator.lookup("legacy");
    CHECK(!o_legacy_lookup.found);
    CHECK(o_legacy_lookup.candidates.empty());

    // An absolute path is used verbatim, and a missing one is not found.
    const icon_lookup_o o_absolute_lookup = o_locator.lookup(o_absolute_icon.string());
    CHECK(o_absolute_lookup.found);
    CHECK(1 == o_absolute_lookup.candidates.size());
    CHECK(o_absolute_icon.string() == o_absolute_lookup.best_path);
    CHECK(!o_absolute_lookup.searched_directories.empty());
    if (!o_absolute_lookup.candidates.empty()) {
        CHECK(0 == o_absolute_lookup.candidates.front().priority);
        CHECK(o_absolute_lookup.candidates.front().theme.empty());
        CHECK(o_absolute_lookup.candidates.front().size.empty());
        CHECK(o_absolute_lookup.candidates.front().context.empty());
    }

    const icon_lookup_o o_missing_lookup = o_locator.lookup((o_root / "missing.png").string());
    CHECK(!o_missing_lookup.found);
    CHECK(o_missing_lookup.candidates.empty());
    CHECK(!o_missing_lookup.searched_directories.empty());

    // Size directory name parsing.  "0x0" also parses as digits-x-digits and
    // therefore returns 0, indistinguishable from the "scalable" sentinel.
    CHECK(48 == icon_theme_locator_c::size_from_directory_name("48x48"));
    CHECK(0 == icon_theme_locator_c::size_from_directory_name("scalable"));
    CHECK(0 == icon_theme_locator_c::size_from_directory_name("0x0"));
    CHECK(-1 == icon_theme_locator_c::size_from_directory_name("weird"));
    CHECK(48 == icon_theme_locator_c::size_from_directory_name("48x48/apps"));
    CHECK(-1 == icon_theme_locator_c::size_from_directory_name("48"));

    if (0 != g_count_failure) {
        std::fprintf(stderr, "icon-theme-locator-test: %d failure(s)\n", g_count_failure);
        return 1;
    }
    std::printf("icon-theme-locator-test: all checks passed\n");
    return 0;
}
