#include "desktop/icon_theme_locator.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <optional>
#include <set>
#include <system_error>
#include <utility>

// Practical subset of the freedesktop Icon Theme Specification:
//
//   * Base icon directories are, in order, $XDG_DATA_HOME/icons,
//     every $XDG_DATA_DIRS entry plus /icons, and /usr/share/pixmaps.
//     Relative environment entries and missing directories are ignored.
//   * /usr/share/pixmaps is searched directly for the candidate file names,
//     because a pixmaps directory holds files, not themes.  The data-home
//     pixmaps directory is deliberately not part of the base list; the module
//     only knows the system pixmaps path required by the specification.
//   * index.theme is parsed only for the simple "Inherits" and "Directories"
//     keys.  Per-directory "Size", "Context", "Type", "MinSize", "MaxSize",
//     "Threshold", and "Scale" entries are not interpreted.
//   * "Directories" entries are used verbatim as size directory names relative
//     to the theme directory; an entry that already names a context directory
//     is still searched, and its files directly inside it are found.
//   * Only .png, .svg, and .xpm files are considered.  Every match is
//     collected so callers can report all sources, and no size or scale
//     preference beyond the directory ordering is applied.

namespace gnome_appimage::desktop {
namespace {

namespace fs = std::filesystem;

constexpr const char *VARIABLE_HOME = "HOME";
constexpr const char *VARIABLE_DATA_HOME = "XDG_DATA_HOME";
constexpr const char *VARIABLE_DATA_DIRS = "XDG_DATA_DIRS";

constexpr const char *DIRECTORY_LOCAL_SHARE = ".local/share";
constexpr const char *DIRECTORY_ICONS = "icons";
constexpr const char *DIRECTORY_PIXMAPS = "/usr/share/pixmaps";
constexpr const char *DIRECTORY_DEFAULT_DATA_LOCAL = "/usr/local/share";
constexpr const char *DIRECTORY_DEFAULT_DATA_SYSTEM = "/usr/share";

constexpr const char *FILE_NAME_THEME_INDEX = "index.theme";
constexpr const char *KEY_INHERITS = "Inherits";
constexpr const char *KEY_DIRECTORIES = "Directories";
constexpr const char *THEME_HICOLOR = "hicolor";
constexpr const char *CONTEXT_APPS = "apps";

constexpr const char *EXTENSION_PNG = ".png";
constexpr const char *EXTENSION_SVG = ".svg";
constexpr const char *EXTENSION_XPM = ".xpm";

constexpr std::size_t THEME_DEPTH_LIMIT = 4;
constexpr int MAXIMUM_SIZE_VALUE = 1000000;

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

std::string trim(const std::string &s_text) {
    const std::string s_whitespace = " \t\r\n";
    const std::size_t i_begin = s_text.find_first_not_of(s_whitespace);
    if (std::string::npos == i_begin) {
        return {};
    }
    const std::size_t i_end = s_text.find_last_not_of(s_whitespace);
    return s_text.substr(i_begin, i_end - i_begin + 1);
}

std::vector<std::string> split_comma_list(const std::string &s_text) {
    std::vector<std::string> o_parts;
    std::size_t i_start = 0;
    while (i_start <= s_text.size()) {
        const std::size_t i_end = s_text.find(',', i_start);
        if (std::string::npos == i_end) {
            o_parts.push_back(trim(s_text.substr(i_start)));
            break;
        }
        o_parts.push_back(trim(s_text.substr(i_start, i_end - i_start)));
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

bool ends_with(const std::string &s_text, const std::string &s_suffix) {
    if (s_suffix.size() > s_text.size()) {
        return false;
    }
    return 0 == s_text.compare(s_text.size() - s_suffix.size(), s_suffix.size(), s_suffix);
}

bool has_icon_extension(const std::string &s_icon_name) {
    return ends_with(s_icon_name, EXTENSION_PNG) || ends_with(s_icon_name, EXTENSION_SVG)
           || ends_with(s_icon_name, EXTENSION_XPM);
}

std::vector<std::string> candidate_file_names(const std::string &s_icon_name) {
    if (has_icon_extension(s_icon_name)) {
        return {s_icon_name};
    }
    std::vector<std::string> o_file_names;
    o_file_names.push_back(s_icon_name + EXTENSION_PNG);
    o_file_names.push_back(s_icon_name + EXTENSION_SVG);
    o_file_names.push_back(s_icon_name + EXTENSION_XPM);
    return o_file_names;
}

void add_existing_directory(std::vector<std::string> &o_directories, const std::string &s_path) {
    std::error_code o_error;
    if (fs::is_directory(s_path, o_error) && !o_error) {
        o_directories.push_back(s_path);
    }
}

std::vector<std::string> list_subdirectories(const fs::path &o_directory) {
    std::vector<std::string> o_names;
    std::error_code o_error;
    fs::directory_iterator o_iterator(o_directory, fs::directory_options::skip_permission_denied,
                                      o_error);
    const fs::directory_iterator o_end;
    while (!o_error && o_iterator != o_end) {
        std::error_code o_entry_error;
        if (o_iterator->is_directory(o_entry_error) && !o_entry_error) {
            o_names.push_back(o_iterator->path().filename().string());
        }
        o_iterator.increment(o_error);
    }
    std::sort(o_names.begin(), o_names.end());
    return o_names;
}

// Read one simple "key=value" entry from an index.theme file.
std::vector<std::string> read_index_key(const fs::path &o_index_path, const std::string &s_key) {
    std::vector<std::string> o_values;
    std::ifstream o_input(o_index_path);
    if (!o_input.is_open()) {
        return o_values;
    }
    std::string s_line;
    while (std::getline(o_input, s_line)) {
        if (!s_line.empty() && '\r' == s_line.back()) {
            s_line.pop_back();
        }
        const std::string s_stripped = trim(s_line);
        if (s_stripped.empty() || '#' == s_stripped[0]) {
            continue;
        }
        const std::size_t i_separator = s_stripped.find('=');
        if (std::string::npos == i_separator) {
            continue;
        }
        if (s_key != trim(s_stripped.substr(0, i_separator))) {
            continue;
        }
        for (const std::string &s_part : split_comma_list(s_stripped.substr(i_separator + 1))) {
            if (!s_part.empty()) {
                o_values.push_back(s_part);
            }
        }
    }
    return o_values;
}

// Append one theme and, depth first, the themes it inherits.
void collect_theme(const std::string &s_icons_directory, const std::string &s_theme,
                   std::size_t i_depth, std::set<std::string> &o_visited,
                   std::vector<std::string> &o_themes) {
    if (THEME_DEPTH_LIMIT < i_depth) {
        return;
    }
    if (!o_visited.insert(s_theme).second) {
        return;
    }
    o_themes.push_back(s_theme);

    const fs::path o_index_path =
        fs::path(s_icons_directory) / s_theme / FILE_NAME_THEME_INDEX;
    for (const std::string &s_inherited : read_index_key(o_index_path, KEY_INHERITS)) {
        collect_theme(s_icons_directory, s_inherited, i_depth + 1, o_visited, o_themes);
    }
}

int size_rank(int i_size) {
    if (0 == i_size) {
        return 0;
    }
    if (0 < i_size) {
        return 1;
    }
    return 2;
}

bool compare_size_names(const std::string &s_left, const std::string &s_right) {
    const int i_size_left = icon_theme_locator_c::size_from_directory_name(s_left);
    const int i_size_right = icon_theme_locator_c::size_from_directory_name(s_right);
    const int i_rank_left = size_rank(i_size_left);
    const int i_rank_right = size_rank(i_size_right);
    if (i_rank_left != i_rank_right) {
        return i_rank_left < i_rank_right;
    }
    if (1 == i_rank_left) {
        return i_size_right < i_size_left;
    }
    return false;
}

void append_candidates(const std::string &s_directory, const std::string &s_theme,
                       const std::string &s_size, const std::string &s_context,
                       const std::vector<std::string> &o_file_names, icon_lookup_o &o_result) {
    for (const std::string &s_file_name : o_file_names) {
        const std::string s_path = join_path(s_directory, s_file_name);
        std::error_code o_error;
        if (!fs::is_regular_file(s_path, o_error) || o_error) {
            continue;
        }

        icon_candidate_o o_candidate;
        o_candidate.path = s_path;
        o_candidate.theme = s_theme;
        o_candidate.size = s_size;
        o_candidate.context = s_context;
        o_candidate.priority = o_result.candidates.size();
        o_result.candidates.push_back(std::move(o_candidate));
    }
}

void search_theme_directory(const std::string &s_icons_directory, const std::string &s_theme,
                            const std::vector<std::string> &o_file_names,
                            icon_lookup_o &o_result) {
    const fs::path o_theme_directory = fs::path(s_icons_directory) / s_theme;
    std::error_code o_theme_error;
    if (!fs::is_directory(o_theme_directory, o_theme_error) || o_theme_error) {
        return;
    }

    const fs::path o_index_path = o_theme_directory / FILE_NAME_THEME_INDEX;
    std::vector<std::string> o_size_names = read_index_key(o_index_path, KEY_DIRECTORIES);
    if (o_size_names.empty()) {
        o_size_names = list_subdirectories(o_theme_directory);
    }
    std::stable_sort(o_size_names.begin(), o_size_names.end(), compare_size_names);

    for (const std::string &s_size_name : o_size_names) {
        const fs::path o_size_directory = o_theme_directory / s_size_name;
        std::error_code o_size_error;
        if (!fs::is_directory(o_size_directory, o_size_error) || o_size_error) {
            continue;
        }
        o_result.searched_directories.push_back(o_size_directory.string());

        std::vector<std::string> o_contexts;
        const fs::path o_apps_directory = o_size_directory / CONTEXT_APPS;
        std::error_code o_apps_error;
        if (fs::is_directory(o_apps_directory, o_apps_error) && !o_apps_error) {
            o_contexts.push_back(CONTEXT_APPS);
        }
        for (const std::string &s_name : list_subdirectories(o_size_directory)) {
            if (CONTEXT_APPS != s_name) {
                o_contexts.push_back(s_name);
            }
        }

        for (const std::string &s_context : o_contexts) {
            const std::string s_context_directory = join_path(o_size_directory.string(),
                                                              s_context);
            o_result.searched_directories.push_back(s_context_directory);
            append_candidates(s_context_directory, s_theme, s_size_name, s_context, o_file_names,
                              o_result);
        }

        // Files directly in the size directory come last.
        append_candidates(o_size_directory.string(), s_theme, s_size_name, std::string(),
                          o_file_names, o_result);
    }
}

}  // namespace

icon_theme_locator_c::icon_theme_locator_c() {
    for (const char *s_name : {VARIABLE_HOME, VARIABLE_DATA_HOME, VARIABLE_DATA_DIRS}) {
        const char *s_value = std::getenv(s_name);
        if (nullptr != s_value) {
            o_environment_[s_name] = s_value;
        }
    }
    build();
}

icon_theme_locator_c::icon_theme_locator_c(
    const std::map<std::string, std::string> &o_environment)
    : o_environment_(o_environment) {
    build();
}

void icon_theme_locator_c::build() {
    o_base_directories_.clear();

    const std::optional<std::string> s_home = get_nonempty(o_environment_, VARIABLE_HOME);

    std::string s_data_home;
    const std::optional<std::string> s_data_home_value =
        get_nonempty(o_environment_, VARIABLE_DATA_HOME);
    if (s_data_home_value.has_value()) {
        if (is_absolute_path(*s_data_home_value)) {
            s_data_home = *s_data_home_value;
        }
    } else if (s_home.has_value()) {
        s_data_home = join_path(*s_home, DIRECTORY_LOCAL_SHARE);
    }
    if (!s_data_home.empty()) {
        add_existing_directory(o_base_directories_,
                               join_path(s_data_home, DIRECTORY_ICONS));
    }

    std::vector<std::string> o_data_directories;
    const std::optional<std::string> s_data_dirs =
        get_nonempty(o_environment_, VARIABLE_DATA_DIRS);
    if (s_data_dirs.has_value()) {
        for (const std::string &s_directory : split_colon_list(*s_data_dirs)) {
            if (is_absolute_path(s_directory)) {
                o_data_directories.push_back(s_directory);
            }
        }
    } else {
        o_data_directories.push_back(DIRECTORY_DEFAULT_DATA_LOCAL);
        o_data_directories.push_back(DIRECTORY_DEFAULT_DATA_SYSTEM);
    }
    for (const std::string &s_directory : o_data_directories) {
        add_existing_directory(o_base_directories_, join_path(s_directory, DIRECTORY_ICONS));
    }

    add_existing_directory(o_base_directories_, DIRECTORY_PIXMAPS);
}

const std::vector<std::string> &icon_theme_locator_c::icon_base_directories() const {
    return o_base_directories_;
}

int icon_theme_locator_c::size_from_directory_name(const std::string &s_directory) {
    if ("scalable" == s_directory) {
        return 0;
    }

    std::size_t i_index = 0;
    int i_size = 0;
    bool b_first_number = false;
    while ((i_index < s_directory.size())
           && (0 != std::isdigit(static_cast<unsigned char>(s_directory[i_index])))) {
        b_first_number = true;
        if (MAXIMUM_SIZE_VALUE > i_size) {
            i_size = (i_size * 10) + (s_directory[i_index] - '0');
        }
        i_index++;
    }
    if (!b_first_number) {
        return -1;
    }
    if ((i_index >= s_directory.size()) || ('x' != s_directory[i_index])) {
        return -1;
    }
    i_index++;

    bool b_second_number = false;
    while ((i_index < s_directory.size())
           && (0 != std::isdigit(static_cast<unsigned char>(s_directory[i_index])))) {
        b_second_number = true;
        i_index++;
    }
    if (!b_second_number) {
        return -1;
    }

    // Any suffix after the second number is accepted and ignored.
    return i_size;
}

icon_lookup_o icon_theme_locator_c::lookup(const std::string &s_icon_name,
                                           const std::string &s_preferred_theme) const {
    icon_lookup_o o_result;
    o_result.icon_name = s_icon_name;

    if (is_absolute_path(s_icon_name)) {
        for (const std::string &s_base_directory : o_base_directories_) {
            o_result.searched_directories.push_back(s_base_directory);
        }
        std::error_code o_error;
        if (fs::exists(s_icon_name, o_error) && !o_error) {
            icon_candidate_o o_candidate;
            o_candidate.path = s_icon_name;
            o_candidate.priority = 0;
            o_result.candidates.push_back(std::move(o_candidate));
            o_result.best_path = s_icon_name;
            o_result.found = true;
        }
        return o_result;
    }

    if (s_icon_name.empty()) {
        return o_result;
    }

    const std::vector<std::string> o_file_names = candidate_file_names(s_icon_name);
    std::set<std::string> o_recorded_themes;

    for (const std::string &s_base_directory : o_base_directories_) {
        if (DIRECTORY_PIXMAPS == s_base_directory) {
            o_result.searched_directories.push_back(s_base_directory);
            append_candidates(s_base_directory, std::string(), std::string(), std::string(),
                              o_file_names, o_result);
            continue;
        }

        std::vector<std::string> o_themes;
        std::set<std::string> o_visited;
        if (!s_preferred_theme.empty()) {
            collect_theme(s_base_directory, s_preferred_theme, 0, o_visited, o_themes);
        }
        if (o_themes.end() == std::find(o_themes.begin(), o_themes.end(), THEME_HICOLOR)) {
            o_themes.push_back(THEME_HICOLOR);
        }

        for (const std::string &s_theme : o_themes) {
            if (o_recorded_themes.insert(s_theme).second) {
                o_result.searched_themes.push_back(s_theme);
            }
            search_theme_directory(s_base_directory, s_theme, o_file_names, o_result);
        }
    }

    if (!o_result.candidates.empty()) {
        o_result.found = true;
        o_result.best_path = o_result.candidates.front().path;
    }
    return o_result;
}

}  // namespace gnome_appimage::desktop
