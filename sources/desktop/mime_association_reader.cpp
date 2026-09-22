#include "desktop/mime_association_reader.h"

#include <cctype>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <optional>
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

constexpr const char *COMPONENT_APPLICATIONS = "applications";
constexpr const char *FILE_MIMEAPPS = "mimeapps.list";
constexpr const char *FILE_DEFAULTS = "defaults.list";

constexpr const char *GROUP_DEFAULT_APPLICATIONS = "Default Applications";
constexpr const char *GROUP_ADDED_ASSOCIATIONS = "Added Associations";
constexpr const char *GROUP_REMOVED_ASSOCIATIONS = "Removed Associations";

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

bool is_regular_file(const std::string &s_path) {
    std::error_code o_error;
    const bool b_regular = fs::is_regular_file(s_path, o_error);
    return b_regular && !o_error;
}

std::string trim(const std::string &s_text) {
    std::size_t i_begin = 0;
    while (i_begin < s_text.size()
           && 0 != std::isspace(static_cast<unsigned char>(s_text[i_begin]))) {
        i_begin++;
    }
    std::size_t i_end = s_text.size();
    while (i_end > i_begin
           && 0 != std::isspace(static_cast<unsigned char>(s_text[i_end - 1]))) {
        i_end--;
    }
    return s_text.substr(i_begin, i_end - i_begin);
}

std::vector<std::string> split_semicolon_list(const std::string &s_text) {
    std::vector<std::string> o_values;
    std::size_t i_start = 0;
    while (i_start <= s_text.size()) {
        const std::size_t i_end = s_text.find(';', i_start);
        const std::string s_part = (std::string::npos == i_end)
                                       ? s_text.substr(i_start)
                                       : s_text.substr(i_start, i_end - i_start);
        const std::string s_value = trim(s_part);
        if (!s_value.empty()) {
            o_values.push_back(s_value);
        }
        if (std::string::npos == i_end) {
            break;
        }
        i_start = i_end + 1;
    }
    return o_values;
}

// The MIME type entries of one association file, split by group.
struct association_file_o {
    std::vector<std::string> o_default_ids;
    std::vector<std::string> o_added_ids;
    std::vector<std::string> o_removed_ids;
};

// Parse one mimeapps.list file as a simple INI, keeping entries for one MIME type.
// A missing or unreadable file leaves the result empty and does not throw.
void parse_association_file(const std::string &s_path, const std::string &s_mime_type,
                            association_file_o &o_result) {
    std::ifstream o_input(s_path);
    if (!o_input.is_open()) {
        return;
    }

    std::string s_group;
    std::string s_line;
    while (std::getline(o_input, s_line)) {
        const std::string s_trimmed = trim(s_line);
        if (s_trimmed.empty()) {
            continue;
        }
        if ('#' == s_trimmed[0]) {
            continue;
        }
        if ('[' == s_trimmed[0]) {
            const std::size_t i_close = s_trimmed.find(']');
            if (std::string::npos == i_close) {
                s_group.clear();
                continue;
            }
            s_group = trim(s_trimmed.substr(1, i_close - 1));
            continue;
        }

        const std::size_t i_equals = s_trimmed.find('=');
        if (std::string::npos == i_equals) {
            continue;
        }
        const std::string s_key = trim(s_trimmed.substr(0, i_equals));
        if (s_key != s_mime_type) {
            continue;
        }
        const std::string s_value = trim(s_trimmed.substr(i_equals + 1));
        const std::vector<std::string> o_ids = split_semicolon_list(s_value);

        if (GROUP_DEFAULT_APPLICATIONS == s_group) {
            o_result.o_default_ids.insert(o_result.o_default_ids.end(), o_ids.begin(), o_ids.end());
        } else if (GROUP_ADDED_ASSOCIATIONS == s_group) {
            o_result.o_added_ids.insert(o_result.o_added_ids.end(), o_ids.begin(), o_ids.end());
        } else if (GROUP_REMOVED_ASSOCIATIONS == s_group) {
            o_result.o_removed_ids.insert(o_result.o_removed_ids.end(), o_ids.begin(), o_ids.end());
        }
    }
}

}  // namespace

mime_association_reader_c::mime_association_reader_c() {
    for (const char *s_name : {VARIABLE_HOME, VARIABLE_DATA_HOME, VARIABLE_DATA_DIRS,
                               VARIABLE_CONFIG_HOME, VARIABLE_CONFIG_DIRS}) {
        const char *s_value = std::getenv(s_name);
        if (nullptr != s_value) {
            o_environment_[s_name] = s_value;
        }
    }
    build();
}

mime_association_reader_c::mime_association_reader_c(
    const std::map<std::string, std::string> &o_environment)
    : o_environment_(o_environment) {
    build();
}

void mime_association_reader_c::build() {
    o_application_directories_.clear();
    o_configuration_files_.clear();

    const std::optional<std::string> s_home = get_nonempty(o_environment_, VARIABLE_HOME);

    std::vector<std::string> o_data_directories;
    const std::optional<std::string> s_data_home = get_nonempty(o_environment_, VARIABLE_DATA_HOME);
    if (s_data_home.has_value()) {
        if (is_absolute_path(*s_data_home)) {
            o_data_directories.push_back(*s_data_home);
        }
    } else if (s_home.has_value()) {
        o_data_directories.push_back(join_path(*s_home, ".local/share"));
    }

    const std::optional<std::string> s_data_dirs = get_nonempty(o_environment_, VARIABLE_DATA_DIRS);
    std::vector<std::string> o_system_data_directories;
    if (s_data_dirs.has_value()) {
        o_system_data_directories = split_colon_list(*s_data_dirs);
    } else {
        o_system_data_directories.push_back("/usr/local/share");
        o_system_data_directories.push_back("/usr/share");
    }
    for (const std::string &s_directory : o_system_data_directories) {
        if (is_absolute_path(s_directory)) {
            o_data_directories.push_back(s_directory);
        }
    }

    std::vector<std::string> o_config_directories;
    const std::optional<std::string> s_config_home =
        get_nonempty(o_environment_, VARIABLE_CONFIG_HOME);
    if (s_config_home.has_value()) {
        if (is_absolute_path(*s_config_home)) {
            o_config_directories.push_back(*s_config_home);
        }
    } else if (s_home.has_value()) {
        o_config_directories.push_back(join_path(*s_home, ".config"));
    }

    const std::optional<std::string> s_config_dirs =
        get_nonempty(o_environment_, VARIABLE_CONFIG_DIRS);
    std::vector<std::string> o_system_config_directories;
    if (s_config_dirs.has_value()) {
        o_system_config_directories = split_colon_list(*s_config_dirs);
    } else {
        o_system_config_directories.push_back("/etc/xdg");
    }
    for (const std::string &s_directory : o_system_config_directories) {
        if (is_absolute_path(s_directory)) {
            o_config_directories.push_back(s_directory);
        }
    }

    for (const std::string &s_directory : o_data_directories) {
        o_application_directories_.push_back(join_path(s_directory, COMPONENT_APPLICATIONS));
    }

    // mimeapps.list files, highest precedence first.
    for (const std::string &s_directory : o_config_directories) {
        o_configuration_files_.push_back(join_path(s_directory, FILE_MIMEAPPS));
    }
    for (const std::string &s_directory : o_data_directories) {
        o_configuration_files_.push_back(
            join_path(join_path(s_directory, COMPONENT_APPLICATIONS), FILE_MIMEAPPS));
    }

    // The deprecated defaults.list files come after every mimeapps.list file.
    for (const std::string &s_directory : o_data_directories) {
        o_configuration_files_.push_back(
            join_path(join_path(s_directory, COMPONENT_APPLICATIONS), FILE_DEFAULTS));
    }
}

const std::vector<std::string> &mime_association_reader_c::configuration_files() const {
    return o_configuration_files_;
}

std::string mime_association_reader_c::resolve_desktop_id(
    const std::string &s_desktop_id) const {
    if (s_desktop_id.empty()) {
        return {};
    }

    for (const std::string &s_directory : o_application_directories_) {
        const std::string s_direct = join_path(s_directory, s_desktop_id);
        if (is_regular_file(s_direct)) {
            return s_direct;
        }

        // Map a desktop ID back to a nested path: "kde-foo.desktop" to "kde/foo.desktop".
        for (std::size_t i_position = 0; i_position < s_desktop_id.size(); i_position++) {
            if ('-' != s_desktop_id[i_position]) {
                continue;
            }
            std::string s_relative = s_desktop_id;
            s_relative[i_position] = '/';
            const std::string s_candidate = join_path(s_directory, s_relative);
            if (is_regular_file(s_candidate)) {
                return s_candidate;
            }
        }
    }
    return {};
}

mime_lookup_o mime_association_reader_c::lookup(const std::string &s_mime_type) const {
    mime_lookup_o o_result;
    o_result.mime_type = s_mime_type;

    // IDs removed by a file at this precedence or higher.
    std::set<std::string> o_removed;

    for (std::size_t i_priority = 0; i_priority < o_configuration_files_.size(); i_priority++) {
        const std::string &s_source_file = o_configuration_files_[i_priority];
        o_result.searched_files.push_back(s_source_file);

        association_file_o o_file;
        parse_association_file(s_source_file, s_mime_type, o_file);

        for (const std::string &s_desktop_id : o_file.o_removed_ids) {
            o_removed.insert(s_desktop_id);
        }

        const auto o_add_associations = [&](const std::vector<std::string> &o_ids,
                                            const char *s_group) {
            for (const std::string &s_desktop_id : o_ids) {
                if (0 != o_removed.count(s_desktop_id)) {
                    continue;
                }
                mime_association_o o_association;
                o_association.mime_type = s_mime_type;
                o_association.desktop_id = s_desktop_id;
                o_association.desktop_path = resolve_desktop_id(s_desktop_id);
                o_association.source_file = s_source_file;
                o_association.group = s_group;
                o_association.priority = i_priority;
                o_result.associations.push_back(std::move(o_association));
            }
        };
        o_add_associations(o_file.o_default_ids, GROUP_DEFAULT_APPLICATIONS);
        o_add_associations(o_file.o_added_ids, GROUP_ADDED_ASSOCIATIONS);

        if (!o_result.found) {
            for (const std::string &s_desktop_id : o_file.o_default_ids) {
                if (0 != o_removed.count(s_desktop_id)) {
                    continue;
                }
                const std::string s_desktop_path = resolve_desktop_id(s_desktop_id);
                if (s_desktop_path.empty()) {
                    continue;
                }
                o_result.default_application.mime_type = s_mime_type;
                o_result.default_application.desktop_id = s_desktop_id;
                o_result.default_application.desktop_path = s_desktop_path;
                o_result.default_application.source_file = s_source_file;
                o_result.default_application.group = GROUP_DEFAULT_APPLICATIONS;
                o_result.default_application.priority = i_priority;
                o_result.found = true;
                break;
            }
        }
    }

    // No explicit default was recorded in any mimeapps.list. Fall back to the
    // cache update-desktop-database writes, which is what xdg-mime and the
    // desktop actually use for associations with no recorded default.
    if (!o_result.found) {
        for (std::size_t i_priority = 0; i_priority < o_application_directories_.size();
             i_priority++) {
            const std::string s_cache =
                join_path(o_application_directories_[i_priority], "mimeinfo.cache");
            o_result.searched_files.push_back(s_cache);

            std::ifstream o_input(s_cache);
            std::string s_line;
            while (std::getline(o_input, s_line)) {
                const std::size_t i_equal = s_line.find('=');
                if (std::string::npos == i_equal) {
                    continue;
                }
                if (s_line.substr(0, i_equal) != s_mime_type) {
                    continue;
                }
                std::string s_ids = s_line.substr(i_equal + 1);
                std::size_t i_start = 0;
                while (i_start <= s_ids.size()) {
                    const std::size_t i_end = s_ids.find(';', i_start);
                    const std::string s_desktop_id =
                        std::string::npos == i_end
                            ? s_ids.substr(i_start)
                            : s_ids.substr(i_start, i_end - i_start);
                    if (!s_desktop_id.empty()) {
                        const std::string s_desktop_path = resolve_desktop_id(s_desktop_id);
                        if (!s_desktop_path.empty() && !o_result.found) {
                            o_result.default_application.mime_type = s_mime_type;
                            o_result.default_application.desktop_id = s_desktop_id;
                            o_result.default_application.desktop_path = s_desktop_path;
                            o_result.default_application.source_file = s_cache;
                            o_result.default_application.group = "mimeinfo.cache";
                            o_result.default_application.priority =
                                o_configuration_files_.size() + i_priority;
                            o_result.found = true;
                        }
                        bool b_known = false;
                        for (const mime_association_o &o_association : o_result.associations) {
                            if (o_association.desktop_id == s_desktop_id) {
                                b_known = true;
                                break;
                            }
                        }
                        if (!b_known) {
                            mime_association_o o_association;
                            o_association.mime_type = s_mime_type;
                            o_association.desktop_id = s_desktop_id;
                            o_association.desktop_path = s_desktop_path;
                            o_association.source_file = s_cache;
                            o_association.group = "mimeinfo.cache";
                            o_association.priority = o_configuration_files_.size() + i_priority;
                            o_result.associations.push_back(std::move(o_association));
                        }
                    }
                    if (std::string::npos == i_end) {
                        break;
                    }
                    i_start = i_end + 1;
                }
            }
            if (o_result.found) {
                break;
            }
        }
    }

    return o_result;
}

}  // namespace gnome_appimage::desktop
