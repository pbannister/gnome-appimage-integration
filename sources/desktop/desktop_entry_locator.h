#pragma once

#include <cstddef>
#include <map>
#include <optional>
#include <string>
#include <vector>

namespace gnome_appimage::desktop {

// One desktop entry found on the search path.
struct desktop_entry_candidate_o {
    std::string id;
    std::string path;
    std::string data_directory;
    std::string relative_path;
    std::size_t priority = 0;
};

// Locate desktop entry files the way GNOME does, through the XDG base directories.
class desktop_entry_locator_c {
public:
    // Build the search path from the process environment.
    desktop_entry_locator_c();

    // Build the search path from an explicit environment, for tests.
    explicit desktop_entry_locator_c(const std::map<std::string, std::string> &o_environment);

    // Base data directories: $XDG_DATA_HOME first, then each $XDG_DATA_DIRS entry.
    const std::vector<std::string> &data_directories() const;

    // `<data-directory>/applications` in the same order.
    const std::vector<std::string> &application_directories() const;

    // Base config directories: $XDG_CONFIG_HOME first, then each $XDG_CONFIG_DIRS entry.
    const std::vector<std::string> &config_directories() const;

    // `<config-directory>/autostart` in the same order.
    const std::vector<std::string> &autostart_directories() const;

    // Enumerate every entry on the application search path, first match per ID wins.
    std::vector<desktop_entry_candidate_o> list() const;

    // Resolve one desktop file ID, or return no value when it is absent.
    std::optional<desktop_entry_candidate_o> locate(const std::string &s_id) const;

    // Compute the desktop file ID of a path relative to one data directory.
    static std::string desktop_id_for_path(const std::string &s_path,
                                           const std::string &s_data_directory);

private:
    void build();

    std::map<std::string, std::string> o_environment_;
    std::vector<std::string> o_data_directories_;
    std::vector<std::string> o_application_directories_;
    std::vector<std::string> o_config_directories_;
    std::vector<std::string> o_autostart_directories_;
};

}  // namespace gnome_appimage::desktop
