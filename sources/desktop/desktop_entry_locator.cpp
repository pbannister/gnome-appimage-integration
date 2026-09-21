#include "desktop/desktop_entry_locator.h"

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <set>
#include <system_error>

namespace gnome_appimage::desktop {
namespace {

namespace fs = std::filesystem;

constexpr const char *VARIABLE_HOME = "HOME";
constexpr const char *VARIABLE_DATA_HOME = "XDG_DATA_HOME";
constexpr const char *VARIABLE_DATA_DIRS = "XDG_DATA_DIRS";
constexpr const char *VARIABLE_CONFIG_HOME = "XDG_CONFIG_HOME";
constexpr const char *VARIABLE_CONFIG_DIRS = "XDG_CONFIG_DIRS";

std::vector<std::string> split_colon_list(const std::string &s_text) {
    std::vector<std::string> o_parts;
    std::size_t i_start = 0;
    while (i_start < s_text.size()) {
        const std::size_t i_end = s_text.find(':', i_start);
        if (std::string::npos == i_end) {
            o_parts.push_back(s_text.substr(i_start));
            break;
        }
        o_parts.push_back(s_text.substr(i_start, i_end - i_start));
        i_start = i_end + 1;
    }
    return o_parts;
}

std::optional<std::string> get_nonempty(const std::map<std::string, std::string> &o_environment,
                                        const char *s_name) {
    const auto o_iterator = o_environment.find(s_name);
    if (o_environment.end() == o_iterator || o_iterator->second.empty()) {
        return std::nullopt;
    }
    return o_iterator->second;
}

bool is_absolute_path(const std::string &s_path) {
    return fs::path(s_path).is_absolute();
}

std::string join_path(const std::string &s_base, const std::string &s_child) {
    return (fs::path(s_base) / s_child).string();
}

}  // namespace

desktop_entry_locator_c::desktop_entry_locator_c() {
    for (const char *s_name : {VARIABLE_HOME, VARIABLE_DATA_HOME, VARIABLE_DATA_DIRS,
                               VARIABLE_CONFIG_HOME, VARIABLE_CONFIG_DIRS}) {
        const char *s_value = std::getenv(s_name);
        if (nullptr != s_value) {
            o_environment_[s_name] = s_value;
        }
    }
    build();
}

desktop_entry_locator_c::desktop_entry_locator_c(
    const std::map<std::string, std::string> &o_environment)
    : o_environment_(o_environment) {
    build();
}

void desktop_entry_locator_c::build() {
    o_data_directories_.clear();
    o_application_directories_.clear();
    o_config_directories_.clear();
    o_autostart_directories_.clear();

    const std::optional<std::string> s_home = get_nonempty(o_environment_, VARIABLE_HOME);

    const std::optional<std::string> s_data_home =
        get_nonempty(o_environment_, VARIABLE_DATA_HOME);
    if (s_data_home.has_value()) {
        if (is_absolute_path(*s_data_home)) {
            o_data_directories_.push_back(*s_data_home);
        }
    } else if (s_home.has_value()) {
        o_data_directories_.push_back(join_path(*s_home, ".local/share"));
    }

    const std::optional<std::string> s_data_dirs =
        get_nonempty(o_environment_, VARIABLE_DATA_DIRS);
    std::vector<std::string> o_system_data_dirs;
    if (s_data_dirs.has_value()) {
        o_system_data_dirs = split_colon_list(*s_data_dirs);
    } else {
        o_system_data_dirs.push_back("/usr/local/share");
        o_system_data_dirs.push_back("/usr/share");
    }
    for (const std::string &s_directory : o_system_data_dirs) {
        if (is_absolute_path(s_directory)) {
            o_data_directories_.push_back(s_directory);
        }
    }
    for (const std::string &s_directory : o_data_directories_) {
        o_application_directories_.push_back(join_path(s_directory, "applications"));
    }

    const std::optional<std::string> s_config_home =
        get_nonempty(o_environment_, VARIABLE_CONFIG_HOME);
    if (s_config_home.has_value()) {
        if (is_absolute_path(*s_config_home)) {
            o_config_directories_.push_back(*s_config_home);
        }
    } else if (s_home.has_value()) {
        o_config_directories_.push_back(join_path(*s_home, ".config"));
    }

    const std::optional<std::string> s_config_dirs =
        get_nonempty(o_environment_, VARIABLE_CONFIG_DIRS);
    std::vector<std::string> o_system_config_dirs;
    if (s_config_dirs.has_value()) {
        o_system_config_dirs = split_colon_list(*s_config_dirs);
    } else {
        o_system_config_dirs.push_back("/etc/xdg");
    }
    for (const std::string &s_directory : o_system_config_dirs) {
        if (is_absolute_path(s_directory)) {
            o_config_directories_.push_back(s_directory);
        }
    }
    for (const std::string &s_directory : o_config_directories_) {
        o_autostart_directories_.push_back(join_path(s_directory, "autostart"));
    }
}

const std::vector<std::string> &desktop_entry_locator_c::data_directories() const {
    return o_data_directories_;
}

const std::vector<std::string> &desktop_entry_locator_c::application_directories() const {
    return o_application_directories_;
}

const std::vector<std::string> &desktop_entry_locator_c::config_directories() const {
    return o_config_directories_;
}

const std::vector<std::string> &desktop_entry_locator_c::autostart_directories() const {
    return o_autostart_directories_;
}

std::string desktop_entry_locator_c::desktop_id_for_path(const std::string &s_path,
                                                         const std::string &s_data_directory) {
    const fs::path o_path = fs::path(s_path).lexically_normal();
    const fs::path o_base = fs::path(s_data_directory).lexically_normal();
    const fs::path o_relative = o_path.lexically_relative(o_base);
    if (o_relative.empty()) {
        return {};
    }

    auto o_iterator = o_relative.begin();
    if (o_relative.end() == o_iterator || "applications" != o_iterator->string()) {
        return {};
    }
    ++o_iterator;

    std::string s_id;
    for (; o_relative.end() != o_iterator; ++o_iterator) {
        const std::string s_component = o_iterator->string();
        if (".." == s_component) {
            return {};
        }
        if (!s_id.empty()) {
            s_id += '-';
        }
        s_id += s_component;
    }
    if (s_id.empty() || ".desktop" != fs::path(s_id).extension().string()) {
        return {};
    }
    return s_id;
}

std::vector<desktop_entry_candidate_o> desktop_entry_locator_c::list() const {
    std::vector<desktop_entry_candidate_o> o_candidates;
    std::set<std::string> o_seen;

    for (std::size_t i_priority = 0; i_priority < o_application_directories_.size();
         i_priority++) {
        const std::string &s_application_directory = o_application_directories_[i_priority];
        const std::string &s_data_directory = o_data_directories_[i_priority];

        std::error_code o_error;
        if (!fs::is_directory(s_application_directory, o_error)) {
            continue;
        }

        std::vector<std::string> o_paths;
        fs::recursive_directory_iterator o_iterator(
            s_application_directory, fs::directory_options::skip_permission_denied, o_error);
        const fs::recursive_directory_iterator o_end;
        while (!o_error && o_iterator != o_end) {
            std::error_code o_entry_error;
            if (o_iterator->is_regular_file(o_entry_error) && !o_entry_error
                && ".desktop" == o_iterator->path().extension().string()) {
                o_paths.push_back(o_iterator->path().string());
            }
            o_iterator.increment(o_error);
        }
        std::sort(o_paths.begin(), o_paths.end());

        for (const std::string &s_path : o_paths) {
            const std::string s_id = desktop_id_for_path(s_path, s_data_directory);
            if (s_id.empty() || 0 != o_seen.count(s_id)) {
                continue;
            }
            o_seen.insert(s_id);

            desktop_entry_candidate_o o_candidate;
            o_candidate.id = s_id;
            o_candidate.path = s_path;
            o_candidate.data_directory = s_data_directory;
            o_candidate.relative_path =
                fs::path(s_path).lexically_relative(s_application_directory).string();
            o_candidate.priority = i_priority;
            o_candidates.push_back(std::move(o_candidate));
        }
    }

    std::sort(o_candidates.begin(), o_candidates.end(),
              [](const desktop_entry_candidate_o &o_left,
                 const desktop_entry_candidate_o &o_right) {
                  if (o_left.priority != o_right.priority) {
                      return o_left.priority < o_right.priority;
                  }
                  return o_left.id < o_right.id;
              });
    return o_candidates;
}

std::optional<desktop_entry_candidate_o> desktop_entry_locator_c::locate(
    const std::string &s_id) const {
    for (const desktop_entry_candidate_o &o_candidate : list()) {
        if (o_candidate.id == s_id) {
            return o_candidate;
        }
    }
    return std::nullopt;
}

}  // namespace gnome_appimage::desktop
