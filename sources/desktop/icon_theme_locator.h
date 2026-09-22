#pragma once

#include <cstddef>
#include <map>
#include <string>
#include <vector>

namespace gnome_appimage::desktop {

// One icon file found by a lookup.
struct icon_candidate_o {
    std::string path;
    std::string theme;
    std::string size;
    std::string context;
    std::size_t priority = 0;
};

// The complete result of one icon lookup.
struct icon_lookup_o {
    bool found = false;
    std::string icon_name;
    std::string best_path;
    std::vector<icon_candidate_o> candidates;
    std::vector<std::string> searched_themes;
    std::vector<std::string> searched_directories;
};

// Resolve icon names the way the freedesktop Icon Theme Specification does.
class icon_theme_locator_c {
public:
    icon_theme_locator_c();
    explicit icon_theme_locator_c(const std::map<std::string, std::string> &o_environment);

    icon_lookup_o lookup(const std::string &s_icon_name,
                         const std::string &s_preferred_theme = std::string()) const;

    const std::vector<std::string> &icon_base_directories() const;

    // "48x48" -> 48, "scalable" -> 0, anything else -> -1.
    static int size_from_directory_name(const std::string &s_directory);

private:
    std::map<std::string, std::string> o_environment_;
    std::vector<std::string> o_base_directories_;
    void build();
};

}  // namespace gnome_appimage::desktop
