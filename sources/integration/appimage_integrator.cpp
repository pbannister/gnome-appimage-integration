#include "integration/appimage_integrator.h"

#include "appimage/appimage_reader.h"
#include "appimage/squashfs_reader.h"
#include "desktop/desktop_entry_locator.h"
#include "desktop/icon_theme_locator.h"
#include "desktop/mime_association_reader.h"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <optional>
#include <set>
#include <sstream>
#include <string>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>
#include <utility>
#include <vector>

namespace gnome_appimage::integration {

namespace {

namespace fs = std::filesystem;

using gnome_appimage::appimage::appimage_detection_e;
using gnome_appimage::appimage::appimage_detection_name;
using gnome_appimage::appimage::appimage_info_o;
using gnome_appimage::appimage::appimage_reader_c;
using gnome_appimage::appimage::squashfs_compression_name;
using gnome_appimage::appimage::squashfs_entry_o;
using gnome_appimage::appimage::squashfs_node_type_e;
using gnome_appimage::appimage::squashfs_node_type_name;
using gnome_appimage::appimage::squashfs_reader_c;
using gnome_appimage::appimage::squashfs_stat_o;
using gnome_appimage::desktop::desktop_entry_file_o;
using gnome_appimage::desktop::desktop_entry_group_o;
using gnome_appimage::desktop::desktop_entry_key_o;
using gnome_appimage::desktop::desktop_entry_reader_c;
using gnome_appimage::desktop::desktop_entry_locator_c;
using gnome_appimage::desktop::icon_theme_locator_c;
using gnome_appimage::desktop::mime_association_reader_c;

constexpr const char *VARIABLE_HOME = "HOME";
constexpr const char *VARIABLE_DATA_HOME = "XDG_DATA_HOME";

bool is_space_character(char c_character) {
    return ' ' == c_character || '\t' == c_character;
}

std::string trim_spaces(const std::string &s_text) {
    std::size_t i_begin = 0;
    std::size_t i_end = s_text.size();
    while (i_begin < i_end && is_space_character(s_text[i_begin])) {
        i_begin++;
    }
    while (i_begin < i_end && is_space_character(s_text[i_end - 1])) {
        i_end--;
    }
    return s_text.substr(i_begin, i_end - i_begin);
}

// A version string, split into the runs a person reads: digits stay together and
// compare as numbers, letters stay together and compare case-insensitively, and
// separators are not tokens.  So 1.1.3 < 1.1.10, and 26.3.0 > 5.13.0.
std::vector<std::string> version_tokens(const std::string &s_version) {
    std::vector<std::string> o_tokens;
    std::string s_current;
    bool b_current_is_digits = false;
    const auto flush = [&o_tokens, &s_current, &b_current_is_digits]() {
        if (!s_current.empty()) {
            o_tokens.push_back(s_current);
            s_current.clear();
        }
        b_current_is_digits = false;
    };
    for (const char c_character : s_version) {
        const unsigned char u_character = static_cast<unsigned char>(c_character);
        const bool b_digits = 0 != std::isdigit(u_character);
        const bool b_letters = 0 != std::isalpha(u_character);
        if (!b_digits && !b_letters) {
            flush();
            continue;
        }
        if (!s_current.empty() && b_digits != b_current_is_digits) {
            flush();
        }
        if (s_current.empty()) {
            b_current_is_digits = b_digits;
        }
        s_current += static_cast<char>(b_digits ? c_character
                                                : static_cast<char>(std::tolower(u_character)));
    }
    flush();
    return o_tokens;
}

bool is_digit_token(const std::string &s_token) {
    for (const char c_character : s_token) {
        if (0 == std::isdigit(static_cast<unsigned char>(c_character))) {
            return false;
        }
    }
    return !s_token.empty();
}

// Compare two digit runs as numbers, without converting them.
int compare_digit_tokens(const std::string &s_left, const std::string &s_right) {
    std::size_t i_left = s_left.find_first_not_of('0');
    std::size_t i_right = s_right.find_first_not_of('0');
    const std::string s_left_trimmed =
        std::string::npos == i_left ? std::string() : s_left.substr(i_left);
    const std::string s_right_trimmed =
        std::string::npos == i_right ? std::string() : s_right.substr(i_right);
    if (s_left_trimmed.size() != s_right_trimmed.size()) {
        return s_left_trimmed.size() < s_right_trimmed.size() ? -1 : 1;
    }
    if (s_left_trimmed == s_right_trimmed) {
        return 0;
    }
    return s_left_trimmed < s_right_trimmed ? -1 : 1;
}

// Compare two version strings, oldest first.  A position with no token counts as
// zero against a number, and as a release against letters, so 1.0 < 1.0.1 and
// 1.0rc1 < 1.0.  Versions that are not comparable (empty, or no shared shape) still
// get a stable answer rather than an exception.
int compare_versions(const std::string &s_left, const std::string &s_right) {
    const std::vector<std::string> o_left = version_tokens(s_left);
    const std::vector<std::string> o_right = version_tokens(s_right);
    const std::size_t u_count = std::max(o_left.size(), o_right.size());
    for (std::size_t i_index = 0; i_index < u_count; i_index++) {
        const bool b_left_present = i_index < o_left.size();
        const bool b_right_present = i_index < o_right.size();
        const std::string s_left_token = b_left_present ? o_left[i_index] : std::string();
        const std::string s_right_token = b_right_present ? o_right[i_index] : std::string();
        if (!b_left_present && !b_right_present) {
            return 0;
        }
        const bool b_left_digits = b_left_present && is_digit_token(s_left_token);
        const bool b_right_digits = b_right_present && is_digit_token(s_right_token);
        if (b_left_present != b_right_present) {
            // The missing side is a release: it beats letters and loses to a number.
            const bool b_present_is_digits = b_left_present ? b_left_digits : b_right_digits;
            if (b_present_is_digits) {
                const std::string &s_number = b_left_present ? s_left_token : s_right_token;
                const int i_compare = compare_digit_tokens("0", s_number);
                if (0 != i_compare) {
                    return b_left_present ? i_compare : -i_compare;
                }
                continue;
            }
            return b_left_present ? -1 : 1;
        }
        if (b_left_digits && b_right_digits) {
            const int i_compare = compare_digit_tokens(s_left_token, s_right_token);
            if (0 != i_compare) {
                return i_compare;
            }
            continue;
        }
        if (b_left_digits != b_right_digits) {
            return b_left_digits ? 1 : -1;
        }
        if (s_left_token != s_right_token) {
            return s_left_token < s_right_token ? -1 : 1;
        }
    }
    return 0;
}

// A Name= value is shown in the menu and may not contain a newline; any control
// character becomes a single separating space.
std::string sanitize_name(const std::string &s_name) {    std::string s_result;
    for (const char c_character : s_name) {
        const unsigned char u_character = static_cast<unsigned char>(c_character);
        if (0x20 > u_character || 0x7f == u_character) {
            if (!s_result.empty() && ' ' != s_result.back()) {
                s_result += ' ';
            }
            continue;
        }
        s_result += c_character;
    }
    return trim_spaces(s_result);
}

std::optional<std::string> environment_value(const std::map<std::string, std::string> &o_environment,
                                             const char *s_name) {    const auto o_found = o_environment.find(s_name);
    if (o_environment.end() == o_found || o_found->second.empty()) {
        return std::nullopt;
    }
    return o_found->second;
}

std::string join_path(const std::string &s_base, const std::string &s_child) {
    return (fs::path(s_base) / s_child).string();
}

std::uint64_t fnv1a_64(const std::string &s_text) {
    std::uint64_t u_hash = 1469598103934665603ULL;
    for (const char c_character : s_text) {
        u_hash ^= static_cast<std::uint64_t>(static_cast<unsigned char>(c_character));
        u_hash *= 1099511628211ULL;
    }
    return u_hash;
}

std::string hex16(std::uint64_t u_value) {
    char s_buffer[32];
    std::snprintf(s_buffer, sizeof(s_buffer), "%016llx",
                  static_cast<unsigned long long>(u_value));
    return s_buffer;
}

bool write_file_bytes(const std::string &s_path, const std::string &s_data, std::string &s_error) {
    std::error_code o_error;
    fs::create_directories(fs::path(s_path).parent_path(), o_error);
    std::ofstream o_output(s_path, std::ios::binary | std::ios::trunc);
    if (!o_output) {
        s_error = "cannot write " + s_path;
        return false;
    }
    o_output.write(s_data.data(), static_cast<std::streamsize>(s_data.size()));
    if (!o_output.good()) {
        s_error = "failed while writing " + s_path;
        return false;
    }
    return true;
}

bool make_executable(const std::string &s_path, std::string &s_error) {
    struct stat o_status;
    if (0 != stat(s_path.c_str(), &o_status)) {
        s_error = "cannot stat " + s_path;
        return false;
    }
    const mode_t u_mode = static_cast<mode_t>(o_status.st_mode | S_IXUSR | S_IXGRP | S_IXOTH);
    if (0 != chmod(s_path.c_str(), u_mode)) {
        s_error = "cannot make " + s_path + " executable";
        return false;
    }
    return true;
}

bool command_exists(const std::string &s_command) {
    if (s_command.empty()) {
        return false;
    }
    if (std::string::npos != s_command.find('/')) {
        return 0 == access(s_command.c_str(), X_OK);
    }
    const char *s_path = std::getenv("PATH");
    if (nullptr == s_path) {
        return false;
    }
    std::string s_remaining = s_path;
    std::size_t i_start = 0;
    while (i_start <= s_remaining.size()) {
        const std::size_t i_end = s_remaining.find(':', i_start);
        const std::string s_directory = std::string::npos == i_end
                                            ? s_remaining.substr(i_start)
                                            : s_remaining.substr(i_start, i_end - i_start);
        if (!s_directory.empty()) {
            const std::string s_candidate = join_path(s_directory, s_command);
            if (0 == access(s_candidate.c_str(), X_OK)) {
                return true;
            }
        }
        if (std::string::npos == i_end) {
            break;
        }
        i_start = i_end + 1;
    }
    return false;
}

bool run_command(const std::vector<std::string> &o_arguments, int &o_exit_code) {
    o_exit_code = -1;
    if (o_arguments.empty()) {
        return false;
    }
    std::vector<char *> o_raw;
    o_raw.reserve(o_arguments.size() + 1);
    for (const std::string &s_argument : o_arguments) {
        o_raw.push_back(const_cast<char *>(s_argument.c_str()));
    }
    o_raw.push_back(nullptr);

    // The child inherits this process's buffered output, and the freopen() below
    // flushes it to the real stdout before reopening, which would print everything
    // written so far a second time. Flush first, so only the parent writes it.
    std::cout.flush();
    std::cerr.flush();
    const pid_t i_child = fork();
    if (0 > i_child) {
        return false;
    }
    if (0 == i_child) {
        // Cache maintenance is not the caller's output; keep it out of the way.
        if (nullptr == std::freopen("/dev/null", "w", stdout)) {
            _exit(127);
        }
        if (nullptr == std::freopen("/dev/null", "w", stderr)) {
            _exit(127);
        }
        execvp(o_raw[0], o_raw.data());
        _exit(127);
    }
    int i_status = 0;
    if (i_child != waitpid(i_child, &i_status, 0)) {
        return false;
    }
    if (WIFEXITED(i_status)) {
        o_exit_code = WEXITSTATUS(i_status);
        return true;
    }
    return false;
}

// After icons change, the theme cache must be refreshed.  GTK trusts a cache
// whose mtime is not older than the theme directory, and then never rescans the
// size directories, so a stale cache hides every newly installed icon.
void refresh_icon_cache(const std::string &s_theme_directory) {
    if (s_theme_directory.empty()) {
        return;
    }
    std::error_code o_error;
    if (!fs::is_directory(s_theme_directory, o_error)) {
        return;
    }
    const char *s_tool = nullptr;
    if (command_exists("gtk4-update-icon-cache")) {
        s_tool = "gtk4-update-icon-cache";
    } else if (command_exists("gtk-update-icon-cache")) {
        s_tool = "gtk-update-icon-cache";
    }
    if (nullptr == s_tool) {
        return;
    }
    int i_exit_code = 0;
    run_command({s_tool, "-f", "-t", s_theme_directory}, i_exit_code);
}

// True when a payload size directory is one the icon theme actually lists.
bool is_valid_icon_size_directory(const std::string &s_name) {
    if ("scalable" == s_name) {
        return true;
    }
    const std::size_t i_cross = s_name.find('x');
    if (std::string::npos == i_cross) {
        return false;
    }
    const std::string s_width = s_name.substr(0, i_cross);
    const std::string s_height = s_name.substr(i_cross + 1);
    const auto is_positive_number = [](const std::string &s_text) {
        if (s_text.empty() || std::string::npos != s_text.find_first_not_of("0123456789")) {
            return false;
        }
        return 0 != std::stoi(s_text);
    };
    return is_positive_number(s_width) && is_positive_number(s_height);
}

std::string current_timestamp() {
    const std::time_t i_now = std::time(nullptr);
    std::tm o_time{};
    gmtime_r(&i_now, &o_time);
    char s_buffer[32];
    std::strftime(s_buffer, sizeof(s_buffer), "%Y-%m-%dT%H:%M:%SZ", &o_time);
    return s_buffer;
}

// Quote one Exec argument the way the Desktop Entry Specification requires.
std::string exec_quote(const std::string &s_argument) {
    bool b_needs_quotes = s_argument.empty();
    for (const char c_character : s_argument) {
        if (is_space_character(c_character) || '"' == c_character || '\'' == c_character
            || '\\' == c_character || '>' == c_character || '<' == c_character
            || '~' == c_character || '|' == c_character || '&' == c_character
            || ';' == c_character || '$' == c_character || '*' == c_character
            || '?' == c_character || '#' == c_character || '(' == c_character
            || ')' == c_character || '`' == c_character) {
            b_needs_quotes = true;
            break;
        }
    }
    if (!b_needs_quotes) {
        return s_argument;
    }
    std::string s_result = "\"";
    for (const char c_character : s_argument) {
        if ('"' == c_character || '\\' == c_character || '$' == c_character
            || '`' == c_character) {
            s_result += '\\';
        }
        s_result += c_character;
    }
    s_result += '"';
    return s_result;
}

std::string exec_field_code(const std::string &s_exec) {
    for (std::size_t i_index = 0; i_index + 1 < s_exec.size(); i_index++) {
        if ('%' != s_exec[i_index]) {
            continue;
        }
        const char c_code = s_exec[i_index + 1];
        if ('%' == c_code) {
            i_index++;
            continue;
        }
        if ('f' == c_code || 'F' == c_code || 'u' == c_code || 'U' == c_code) {
            return std::string("%") + c_code;
        }
        return std::string("%") + c_code;
    }
    return "%U";
}

// Parse the leading executable token of an Exec line, honouring double quotes.
std::string exec_program(const std::string &s_exec) {
    std::string s_trimmed = trim_spaces(s_exec);
    if (s_trimmed.empty()) {
        return {};
    }
    if ('"' != s_trimmed[0]) {
        const std::size_t i_space = s_trimmed.find(' ');
        return std::string::npos == i_space ? s_trimmed : s_trimmed.substr(0, i_space);
    }
    std::string s_result;
    for (std::size_t i_index = 1; i_index < s_trimmed.size(); i_index++) {
        const char c_character = s_trimmed[i_index];
        if ('\\' == c_character && i_index + 1 < s_trimmed.size()) {
            s_result += s_trimmed[++i_index];
            continue;
        }
        if ('"' == c_character) {
            break;
        }
        s_result += c_character;
    }
    return s_result;
}

// Two spellings may name the same file, directly or through a symlink.
bool same_file_path(const std::string &s_left, const std::string &s_right) {
    if (s_left.empty() || s_right.empty()) {
        return false;
    }
    if (s_left == s_right) {
        return true;
    }
    std::error_code o_error;
    const fs::path o_left = fs::weakly_canonical(fs::path(s_left), o_error);
    if (o_error) {
        return false;
    }
    o_error.clear();
    const fs::path o_right = fs::weakly_canonical(fs::path(s_right), o_error);
    if (o_error) {
        return false;
    }
    return o_left == o_right;
}

std::string strip_extension(const std::string &s_name) {
    const std::size_t i_dot = s_name.rfind('.');
    if (std::string::npos == i_dot || 0 == i_dot) {
        return s_name;
    }
    return s_name.substr(0, i_dot);
}

// Never overwrite an earlier backup: add a numeric suffix when the name is taken.
std::string unique_backup_path(const std::string &s_directory, const std::string &s_filename) {
    std::string s_candidate = join_path(s_directory, s_filename);
    std::error_code o_error;
    if (!fs::exists(s_candidate, o_error)) {
        return s_candidate;
    }
    const std::string s_stem = strip_extension(s_filename);
    const std::string s_extension = fs::path(s_filename).extension().string();
    for (int i_suffix = 2;; i_suffix++) {
        s_candidate =
            join_path(s_directory, s_stem + "-" + std::to_string(i_suffix) + s_extension);
        o_error.clear();
        if (!fs::exists(s_candidate, o_error)) {
            return s_candidate;
        }
    }
}

// Reduce an application name to a comparison key: trim, drop a trailing " (N)",
// and lower-case, so "FreeCAD" and "FreeCAD (1)" compare equal.
std::string normalize_application_name(const std::string &s_name) {
    std::string s_result = trim_spaces(s_name);
    if (!s_result.empty() && ')' == s_result.back() && 3 < s_result.size()) {
        const std::size_t i_open = s_result.rfind(" (");
        if (std::string::npos != i_open) {
            bool b_digits = true;
            for (std::size_t i_index = i_open + 2; i_index + 1 < s_result.size(); i_index++) {
                if ('0' > s_result[i_index] || '9' < s_result[i_index]) {
                    b_digits = false;
                    break;
                }
            }
            if (b_digits) {
                s_result = trim_spaces(s_result.substr(0, i_open));
            }
        }
    }
    std::string s_lower;
    for (const char c_character : s_result) {
        s_lower += ('A' <= c_character && 'Z' >= c_character)
                       ? static_cast<char>(c_character - 'A' + 'a')
                       : c_character;
    }
    return s_lower;
}

// Find a dotted version in a filename, for example
// FreeCAD_1.1.3-Linux-x86_64.AppImage -> 1.1.3.
std::string version_from_filename(const std::string &s_filename) {
    for (std::size_t i_index = 0; i_index + 2 < s_filename.size(); i_index++) {
        if ('0' > s_filename[i_index] || '9' < s_filename[i_index]) {
            continue;
        }
        std::size_t i_end = i_index;
        bool b_saw_dot = false;
        while (i_end < s_filename.size()) {
            const char c_character = s_filename[i_end];
            if ('0' <= c_character && '9' >= c_character) {
                i_end++;
                continue;
            }
            if ('.' == c_character && !b_saw_dot) {
                b_saw_dot = true;
                i_end++;
                continue;
            }
            break;
        }
        if (b_saw_dot && i_end > i_index && '0' <= s_filename[i_end - 1]
            && '9' >= s_filename[i_end - 1]) {
            return s_filename.substr(i_index, i_end - i_index);
        }
    }
    return {};
}

// Read the first <release version="..."> from AppStream metadata.
std::string version_from_appstream_text(const std::string &s_text) {
    std::size_t i_release = s_text.find("<release");
    while (std::string::npos != i_release) {
        const std::size_t i_version = s_text.find("version=\"", i_release);
        const std::size_t i_tag_end = s_text.find('>', i_release);
        if (std::string::npos != i_version
            && (std::string::npos == i_tag_end || i_version < i_tag_end)) {
            const std::size_t i_begin = i_version + 9;
            const std::size_t i_end = s_text.find('"', i_begin);
            if (std::string::npos != i_end) {
                return s_text.substr(i_begin, i_end - i_begin);
            }
        }
        i_release = s_text.find("<release", i_release + 1);
    }
    return {};
}

// Extract the application version from an AppImage without running it.
// Sources are tried in order: X-AppImage-Version, AppStream, the file name.
std::string version_of_appimage(const std::string &s_path, std::string &s_source) {
    s_source.clear();
    appimage_info_o o_info;
    if (!appimage_reader_c::read(s_path, o_info)
        || appimage_detection_e::type2 != o_info.detection || !o_info.has_squashfs) {
        return {};
    }
    squashfs_reader_c o_reader;
    std::string s_error;
    if (!o_reader.open(s_path, o_info.payload_offset, s_error)) {
        return {};
    }

    std::vector<squashfs_entry_o> o_desktops;
    if (o_reader.list_root_files_with_extension(".desktop", o_desktops, s_error)
        && !o_desktops.empty()) {
        std::string s_content;
        if (o_reader.read_file(o_desktops[0].path, s_content, s_error)) {
            desktop_entry_file_o o_entry;
            std::vector<gnome_appimage::desktop::desktop_entry_diagnostic_o> o_diagnostics;
            desktop_entry_reader_c::parse_text(s_content, o_entry, o_diagnostics);
            const std::string s_value =
                o_entry.value("Desktop Entry", "X-AppImage-Version").value_or(std::string());
            if (!s_value.empty()) {
                s_source = "X-AppImage-Version";
                return s_value;
            }
        }
    }

    std::vector<squashfs_entry_o> o_metainfo;
    if (o_reader.list_directory("/usr/share/metainfo", o_metainfo, s_error)) {
        for (const squashfs_entry_o &o_file : o_metainfo) {
            if (std::string::npos == o_file.name.find(".appdata.xml")
                && std::string::npos == o_file.name.find(".metainfo.xml")) {
                continue;
            }
            std::string s_content;
            if (!o_reader.read_file(o_file.path, s_content, s_error)) {
                continue;
            }
            const std::string s_version = version_from_appstream_text(s_content);
            if (!s_version.empty()) {
                s_source = "AppStream";
                return s_version;
            }
        }
    }

    const std::string s_version = version_from_filename(fs::path(s_path).filename().string());
    if (!s_version.empty()) {
        s_source = "filename";
    }
    return s_version;
}

// Find launchers that already represent the application being installed.
std::vector<integration_conflict_o> detect_application_conflicts(
    const std::map<std::string, std::string> &o_environment,
    const std::string &s_new_name_key,
    const std::string &s_new_appimage_name,
    const std::string &s_new_wm_class,
    const std::string &s_new_appimage_stem,
    const std::string &s_new_appimage_path,
    const std::string &s_new_desktop_id,
    const std::string &s_new_identifier,
    const std::vector<installed_appimage_o> &o_installed) {
    std::vector<integration_conflict_o> o_conflicts;
    const desktop_entry_locator_c o_locator(o_environment);
    for (const gnome_appimage::desktop::desktop_entry_candidate_o &o_candidate :
         o_locator.list()) {
        const installed_appimage_o *p_record = nullptr;
        for (const installed_appimage_o &o_entry : o_installed) {
            if (o_entry.desktop_entry_path == o_candidate.path) {
                p_record = &o_entry;
                break;
            }
        }
        const bool b_managed = nullptr != p_record;
        // Our own launcher at our own target id belongs to this AppImage while it
        // either points at this very file or was written for this same AppImage; the
        // identifier is derived from the content, so a moved AppImage keeps it. It is
        // recorded so its record can be refreshed, but it is not a conflict. A
        // different AppImage claiming the same identifier must be an explicit choice,
        // never a silent takeover.
        desktop_entry_file_o o_entry;
        std::vector<gnome_appimage::desktop::desktop_entry_diagnostic_o> o_diagnostics;
        if (!desktop_entry_reader_c::parse_file(o_candidate.path, o_entry, o_diagnostics)) {
            continue;
        }
        if (b_managed && o_candidate.id == s_new_desktop_id) {
            const std::string s_existing_appimage =
                exec_program(o_entry.value("Desktop Entry", "Exec").value_or(std::string()));
            const bool b_same_file = same_file_path(s_existing_appimage, s_new_appimage_path);
            const bool b_same_appimage =
                b_same_file
                || (!s_new_identifier.empty() && p_record->identifier == s_new_identifier);
            if (b_same_appimage) {
                integration_conflict_o o_upgrade;
                o_upgrade.desktop_id = o_candidate.id;
                o_upgrade.path = o_candidate.path;
                o_upgrade.name = o_entry.value("Desktop Entry", "Name").value_or(std::string());
                o_upgrade.appimage_path = s_existing_appimage;
                o_upgrade.icon = o_entry.value("Desktop Entry", "Icon").value_or(std::string());
                o_upgrade.wm_class =
                    o_entry.value("Desktop Entry", "StartupWMClass").value_or(std::string());
                o_upgrade.exec_exists =
                    !s_existing_appimage.empty() && fs::exists(s_existing_appimage);
                if (o_upgrade.exec_exists) {
                    std::string s_version_source;
                    o_upgrade.version = version_of_appimage(s_existing_appimage, s_version_source);
                }
                o_upgrade.managed = true;
                o_upgrade.upgrade = true;
                if (b_same_file) {
                    o_upgrade.origin = "this tool (upgrade)";
                } else {
                    // The AppImage was moved or copied: the launcher points somewhere
                    // else, so it needs the entry rewritten rather than replaced.
                    o_upgrade.repair = true;
                    o_upgrade.origin =
                        o_upgrade.exec_exists
                            ? "this tool (this AppImage, now at a different path)"
                            : "this tool (this AppImage, and its launcher is broken)";
                }
                o_conflicts.push_back(std::move(o_upgrade));
                continue;
            }
        }
        const std::string s_name_key = normalize_application_name(
            o_entry.value("Desktop Entry", "Name").value_or(std::string()));
        const std::string s_appimage_name =
            o_entry.value("Desktop Entry", "X-AppImage-Name").value_or(std::string());
        const std::string s_wm_class =
            o_entry.value("Desktop Entry", "StartupWMClass").value_or(std::string());
        const std::string s_exec =
            exec_program(o_entry.value("Desktop Entry", "Exec").value_or(std::string()));
        const std::string s_exec_stem = strip_extension(fs::path(s_exec).filename().string());

        const bool b_name_match =
            !s_name_key.empty() && !s_new_name_key.empty() && s_name_key == s_new_name_key;
        const bool b_appimage_name_match = !s_appimage_name.empty() && !s_new_appimage_name.empty()
                                           && s_appimage_name == s_new_appimage_name;
        const bool b_wm_match = !s_wm_class.empty() && !s_new_wm_class.empty()
                                && s_wm_class == s_new_wm_class;
        const bool b_file_match = !s_exec_stem.empty() && !s_new_appimage_stem.empty()
                                  && s_exec_stem == s_new_appimage_stem;
        if (!b_name_match && !b_appimage_name_match && !b_wm_match && !b_file_match) {
            continue;
        }

        integration_conflict_o o_conflict;
        o_conflict.desktop_id = o_candidate.id;
        o_conflict.path = o_candidate.path;
        o_conflict.name = o_entry.value("Desktop Entry", "Name").value_or(std::string());
        o_conflict.appimage_path = s_exec;
        o_conflict.icon = o_entry.value("Desktop Entry", "Icon").value_or(std::string());
        o_conflict.wm_class = s_wm_class;
        o_conflict.exec_exists = !s_exec.empty() && fs::exists(s_exec);
        if (o_conflict.exec_exists) {
            std::string s_version_source;
            o_conflict.version = version_of_appimage(s_exec, s_version_source);
        }
        o_conflict.managed = b_managed;
        if (b_managed) {
            // The same identifier from this tool means a different AppImage wants it.
            o_conflict.origin = o_candidate.id == s_new_desktop_id
                                    ? "this tool (a different AppImage for the same identifier)"
                                    : "this tool";
        } else if (!o_entry.value("Desktop Entry", "X-AppImage-Identifier")
                        .value_or(std::string())
                        .empty()) {
            // This tool writes the same key, so provenance, not the key, decides.
            o_conflict.origin = !o_entry.value("Desktop Entry", "X-Integrated-By")
                                     .value_or(std::string())
                                     .empty()
                                    ? "this tool (launcher with no record)"
                                    : "AppImageLauncher";
        } else {
            o_conflict.origin = "unknown";
        }
        o_conflicts.push_back(std::move(o_conflict));
    }
    return o_conflicts;
}

bool has_icon_extension(const std::string &s_name) {
    return std::string::npos != s_name.find(".png") || std::string::npos != s_name.find(".svg")
           || std::string::npos != s_name.find(".svgz") || std::string::npos != s_name.find(".xpm");
}

bool read_png_size(const std::string &s_data, int &o_width, int &o_height) {
    if (24 > s_data.size()) {
        return false;
    }
    const unsigned char u_signature[8] = {0x89, 'P', 'N', 'G', 0x0d, 0x0a, 0x1a, 0x0a};
    for (std::size_t i_index = 0; i_index < 8; i_index++) {
        if (static_cast<unsigned char>(s_data[i_index]) != u_signature[i_index]) {
            return false;
        }
    }
    if (0 != std::memcmp(s_data.data() + 12, "IHDR", 4)) {
        return false;
    }
    const auto read_be32 = [&s_data](std::size_t i_offset) {
        return (static_cast<std::uint32_t>(static_cast<unsigned char>(s_data[i_offset])) << 24)
               | (static_cast<std::uint32_t>(static_cast<unsigned char>(s_data[i_offset + 1])) << 16)
               | (static_cast<std::uint32_t>(static_cast<unsigned char>(s_data[i_offset + 2])) << 8)
               | static_cast<std::uint32_t>(static_cast<unsigned char>(s_data[i_offset + 3]));
    };
    o_width = static_cast<int>(read_be32(16));
    o_height = static_cast<int>(read_be32(20));
    return 0 < o_width && 0 < o_height;
}

std::string manifest_get(const std::vector<std::pair<std::string, std::string>> &o_lines,
                         const std::string &s_key) {
    for (const std::pair<std::string, std::string> &o_line : o_lines) {
        if (o_line.first == s_key) {
            return o_line.second;
        }
    }
    return {};
}

std::vector<std::string> manifest_get_all(
    const std::vector<std::pair<std::string, std::string>> &o_lines, const std::string &s_key) {
    std::vector<std::string> o_values;
    for (const std::pair<std::string, std::string> &o_line : o_lines) {
        if (o_line.first == s_key) {
            o_values.push_back(o_line.second);
        }
    }
    return o_values;
}

// Restore "original|backup" recorded when a conflicting launcher was replaced.
bool restore_backup_pair(const std::string &s_pair, std::string &s_error) {
    const std::size_t i_bar = s_pair.find('|');
    if (std::string::npos == i_bar) {
        return false;
    }
    const std::string s_original = s_pair.substr(0, i_bar);
    const std::string s_backup = s_pair.substr(i_bar + 1);
    std::error_code o_error;
    if (!fs::exists(s_backup, o_error)) {
        return false;
    }
    fs::create_directories(fs::path(s_original).parent_path(), o_error);
    fs::rename(s_backup, s_original, o_error);
    if (o_error) {
        o_error.clear();
        fs::copy_file(s_backup, s_original, fs::copy_options::overwrite_existing, o_error);
        if (o_error) {
            s_error = "cannot restore " + s_original;
            return false;
        }
        fs::remove(s_backup, o_error);
    }
    return true;
}

std::vector<std::pair<std::string, std::string>> read_manifest(const std::string &s_path) {
    std::vector<std::pair<std::string, std::string>> o_lines;
    std::ifstream o_input(s_path);
    std::string s_line;
    while (std::getline(o_input, s_line)) {
        const std::size_t i_equal = s_line.find('=');
        if (std::string::npos == i_equal) {
            continue;
        }
        o_lines.emplace_back(s_line.substr(0, i_equal), s_line.substr(i_equal + 1));
    }
    return o_lines;
}

}  // namespace

std::string integration_identifier(const std::string &s_content_probe) {
    return hex16(fnv1a_64(s_content_probe));
}

std::string integration_sanitize_token(const std::string &s_text) {
    std::string s_result;
    for (const char c_character : s_text) {
        const bool b_allowed = ('A' <= c_character && c_character <= 'Z')
                               || ('a' <= c_character && c_character <= 'z')
                               || ('0' <= c_character && c_character <= '9')
                               || '-' == c_character || '_' == c_character || '.' == c_character;
        s_result += b_allowed ? c_character : '_';
    }
    if (s_result.empty()) {
        s_result = "appimage";
    }
    return s_result;
}

appimage_integrator_c::appimage_integrator_c() {
    for (const char *s_name : {VARIABLE_HOME, VARIABLE_DATA_HOME}) {
        const char *s_value = std::getenv(s_name);
        if (nullptr != s_value) {
            o_environment_[s_name] = s_value;
        }
    }
    build();
}

appimage_integrator_c::appimage_integrator_c(
    const std::map<std::string, std::string> &o_environment)
    : o_environment_(o_environment) {
    build();
}

void appimage_integrator_c::build() {
    const std::optional<std::string> s_home = environment_value(o_environment_, VARIABLE_HOME);
    const std::optional<std::string> s_data_home =
        environment_value(o_environment_, VARIABLE_DATA_HOME);
    s_data_home_ = s_data_home.has_value() ? *s_data_home
                                           : (s_home.has_value()
                                                  ? join_path(*s_home, ".local/share")
                                                  : std::string());
    s_applications_directory_ = join_path(s_data_home_, "applications");
    s_icon_directory_ = join_path(s_data_home_, "icons/hicolor");
    s_mime_packages_directory_ = join_path(s_data_home_, "mime/packages");
    s_state_directory_ = join_path(s_data_home_, "gnome-appimage-integration");
    s_install_directory_ = s_home.has_value() ? join_path(*s_home, "Applications")
                                              : std::string("Applications");
    o_application_directories_.push_back(s_applications_directory_);
    const desktop_entry_locator_c o_locator(o_environment_);
    for (const std::string &s_directory : o_locator.application_directories()) {
        if (s_directory != s_applications_directory_) {
            o_application_directories_.push_back(s_directory);
        }
    }
}

bool appimage_integrator_c::plan(const std::string &s_appimage_path,
                                 const integration_options_o &o_options,
                                 integration_plan_o &o_plan) const {
    o_plan = integration_plan_o{};
    try {
        std::error_code o_error;
        const std::string s_absolute =
            fs::absolute(s_appimage_path, o_error).lexically_normal().string();
        o_plan.appimage_path = o_error ? s_appimage_path : s_absolute;

        appimage_info_o o_info;
        if (!appimage_reader_c::read(o_plan.appimage_path, o_info)) {
            o_plan.error = o_info.error;
            return false;
        }
        if (appimage_detection_e::type2 != o_info.detection || !o_info.has_squashfs) {
            o_plan.error = "only type 2 AppImages can be integrated";
            return false;
        }

        squashfs_reader_c o_reader;
        std::string s_error;
        if (!o_reader.open(o_plan.appimage_path, o_info.payload_offset, s_error)) {
            o_plan.error = s_error;
            return false;
        }

        std::vector<squashfs_entry_o> o_desktop_entries;
        if (!o_reader.list_root_files_with_extension(".desktop", o_desktop_entries, s_error)) {
            o_plan.error = s_error;
            return false;
        }
        if (o_desktop_entries.empty()) {
            o_plan.error = "the AppImage has no root desktop entry";
            return false;
        }
        if (1 < o_desktop_entries.size()) {
            o_plan.warnings.push_back("the AppImage has several root desktop entries; using "
                                      + o_desktop_entries[0].name);
        }
        o_plan.embedded_desktop_path = o_desktop_entries[0].path;

        std::string s_embedded_text;
        if (!o_reader.read_file(o_plan.embedded_desktop_path, s_embedded_text, s_error)) {
            o_plan.error = s_error;
            return false;
        }
        desktop_entry_file_o o_entry;
        std::vector<gnome_appimage::desktop::desktop_entry_diagnostic_o> o_diagnostics;
        desktop_entry_reader_c::parse_text(s_embedded_text, o_entry, o_diagnostics);
        const desktop_entry_group_o *p_group = o_entry.desktop_entry_group();
        if (nullptr == p_group) {
            o_plan.error = "the embedded desktop entry has no [Desktop Entry] group";
            return false;
        }

        std::string s_probe = s_embedded_text;
        s_probe += "|";
        s_probe += std::to_string(o_info.file_size);
        s_probe += "|";
        s_probe += std::to_string(o_info.payload_offset);
        o_plan.identifier = integration_identifier(s_probe);

        o_plan.name = o_entry.value("Desktop Entry", "Name").value_or(std::string());
        if (!o_options.name_override.empty()) {
            // A launcher may be renamed so several of them can be told apart; the
            // embedded name still drives conflict detection.
            o_plan.name = sanitize_name(o_options.name_override);
            if (o_plan.name.empty()) {
                o_plan.warnings.push_back(
                    "the requested name has no printable characters; the name from the "
                    "embedded entry is used instead");
                o_plan.name = o_entry.value("Desktop Entry", "Name").value_or(std::string());
            } else if (o_plan.name != o_options.name_override) {
                o_plan.notes.push_back("launcher name: " + o_plan.name);
            }
        }
        o_plan.generic_name = o_entry.value("Desktop Entry", "GenericName").value_or(std::string());
        o_plan.comment = o_entry.value("Desktop Entry", "Comment").value_or(std::string());
        o_plan.detection_name = appimage_detection_name(o_info.detection);
        o_plan.file_size = o_info.file_size;
        o_plan.payload_size = o_info.payload_size;
        o_plan.payload_offset = o_info.payload_offset;
        if (o_info.signature_section.present) {
            o_plan.signature =
                o_info.signature_is_empty ? "present (empty padding)" : "present";
        }
        if (o_info.has_squashfs) {
            o_plan.compression_name = squashfs_compression_name(o_info.squashfs.compression);
        }
        if (o_info.update_information_section.present) {
            o_plan.update_information = o_info.update_information;
        }
        o_plan.version = version_of_appimage(o_plan.appimage_path, o_plan.version_source);

        const std::string s_stem = strip_extension(o_desktop_entries[0].name);
        o_plan.desktop_id = o_options.desktop_file_name.empty()
                                ? integration_sanitize_token(s_stem) + ".desktop"
                                : o_options.desktop_file_name;

        const std::string s_icon_value =
            o_entry.value("Desktop Entry", "Icon").value_or(std::string());
        const std::string s_icon_base = integration_sanitize_token(
            0 != s_icon_value.find('/') && has_icon_extension(s_icon_value)
                ? strip_extension(s_icon_value)
                : (s_icon_value.empty() ? s_stem : s_icon_value));
        o_plan.icon_name = o_options.icon_name_override.empty()
                               ? "appimage_" + o_plan.identifier.substr(0, 8) + "_" + s_icon_base
                               : o_options.icon_name_override;

        const std::string s_install_directory = o_options.install_directory.empty()
                                                    ? s_install_directory_
                                                    : o_options.install_directory;
        o_plan.installed_path =
            join_path(s_install_directory, fs::path(o_plan.appimage_path).filename().string());

        o_plan.field_code = exec_field_code(o_entry.value("Desktop Entry", "Exec").value_or(""));
        o_plan.extra_exec_arguments = o_options.extra_exec_arguments;
        o_plan.exec_command = exec_quote(o_plan.installed_path);
        if (!o_plan.extra_exec_arguments.empty()) {
            o_plan.exec_command += " " + o_plan.extra_exec_arguments;
        }
        o_plan.exec_command += " " + o_plan.field_code;

        o_plan.startup_wm_class = !o_options.startup_wm_class_override.empty()
                                      ? o_options.startup_wm_class_override
                                      : o_entry.value("Desktop Entry", "StartupWMClass")
                                            .value_or(std::string());
        if (o_plan.startup_wm_class.empty()) {
            // A class set by an earlier install of this same AppImage is remembered.
            const std::vector<std::pair<std::string, std::string>> o_installed_lines =
                read_manifest(join_path(s_state_directory_, o_plan.identifier + ".manifest"));
            o_plan.startup_wm_class = manifest_get(o_installed_lines, "startup_wm_class");
        }
        if (o_plan.startup_wm_class.empty()) {
            // Borrow the class from a launcher that already represents this application.
            for (const integration_conflict_o &o_conflict : o_plan.conflicts) {
                if (!o_conflict.wm_class.empty()) {
                    o_plan.startup_wm_class = o_conflict.wm_class;
                    o_plan.notes.push_back("adopted StartupWMClass=" + o_conflict.wm_class
                                           + " from " + o_conflict.path);
                    break;
                }
            }
        }
        if (o_plan.startup_wm_class.empty()) {
            // Repair path: a launcher this tool displaced may carry the class.
            const std::string s_backup_directory = join_path(s_state_directory_, "backup");
            std::error_code o_backup_error;
            if (fs::is_directory(s_backup_directory, o_backup_error)) {
                std::vector<std::string> o_backups;
                for (const fs::directory_entry &o_item :
                     fs::directory_iterator(s_backup_directory, o_backup_error)) {
                    if (".desktop" == o_item.path().extension().string()) {
                        o_backups.push_back(o_item.path().string());
                    }
                }
                std::sort(o_backups.begin(), o_backups.end());
                const std::string s_name_key = normalize_application_name(
                    o_entry.value("Desktop Entry", "Name").value_or(std::string()));
                for (const std::string &s_backup : o_backups) {
                    desktop_entry_file_o o_backup_entry;
                    std::vector<gnome_appimage::desktop::desktop_entry_diagnostic_o>
                        o_backup_diagnostics;
                    if (!desktop_entry_reader_c::parse_file(s_backup, o_backup_entry,
                                                            o_backup_diagnostics)) {
                        continue;
                    }
                    if (normalize_application_name(o_backup_entry
                                                       .value("Desktop Entry", "Name")
                                                       .value_or(std::string()))
                        != s_name_key) {
                        continue;
                    }
                    const std::string s_class =
                        o_backup_entry.value("Desktop Entry", "StartupWMClass")
                            .value_or(std::string());
                    if (!s_class.empty()) {
                        o_plan.startup_wm_class = s_class;
                        o_plan.notes.push_back("adopted StartupWMClass=" + s_class
                                               + " from the backed-up launcher " + s_backup);
                        break;
                    }
                }
            }
        }
        if (o_plan.startup_wm_class.empty()) {
            o_plan.warnings.push_back(
                "the embedded entry has no StartupWMClass and no earlier launcher supplied one; "
                "the dock may show a generic icon until one is set (read the window app id with "
                "'lg', then reinstall with --wm-class)");
        }

        // Launchers that already represent this application.
        const std::vector<installed_appimage_o> o_installed = list_installed();
        o_plan.conflicts = detect_application_conflicts(
            o_environment_,
            normalize_application_name(
                o_entry.value("Desktop Entry", "Name").value_or(std::string())),
            o_entry.value("Desktop Entry", "X-AppImage-Name").value_or(std::string()),
            o_plan.startup_wm_class,
            strip_extension(fs::path(o_plan.appimage_path).filename().string()),
            o_plan.appimage_path, o_plan.desktop_id, o_plan.identifier, o_installed);

        // How this file's version compares with what is already installed.  The
        // newest installed version is the one that matters: it is what the owner
        // would give up by integrating an older build.
        for (const integration_conflict_o &o_conflict : o_plan.conflicts) {
            if (o_conflict.version.empty()) {
                continue;
            }
            if (o_plan.installed_version.empty()
                || 0 < compare_versions(o_conflict.version, o_plan.installed_version)) {
                o_plan.installed_version = o_conflict.version;
            }
        }
        if (o_plan.version.empty() || o_plan.installed_version.empty()) {
            o_plan.version_relation = "unknown";
        } else {
            const int i_relation = compare_versions(o_plan.version, o_plan.installed_version);
            o_plan.version_relation =
                0 == i_relation ? "same" : (0 < i_relation ? "newer" : "older");
        }

        bool b_has_real_conflict = false;
        for (const integration_conflict_o &o_conflict : o_plan.conflicts) {
            if (!o_conflict.upgrade) {
                b_has_real_conflict = true;
                break;
            }
        }
        if (!o_plan.conflicts.empty()
            && integration_conflict_policy_e::add == o_options.conflict_policy) {
            const std::string s_stem_id = strip_extension(o_plan.desktop_id);
            const std::string s_plain_identifier = o_plan.identifier;
            const std::string s_plain_icon_name = o_plan.icon_name;
            std::string s_candidate = o_plan.desktop_id;
            const auto o_taken = [&](const std::string &s_id) {
                std::error_code o_check_error;
                if (fs::exists(join_path(s_applications_directory_, s_id), o_check_error)) {
                    return true;
                }
                for (const integration_conflict_o &o_conflict : o_plan.conflicts) {
                    if (o_conflict.desktop_id == s_id) {
                        return true;
                    }
                }
                return false;
            };
            // The record that still describes a launcher being kept must keep its name
            // as well, so the new record is suffixed in step with the new launcher.
            const auto o_manifest_taken = [&](int i_index) {
                std::error_code o_check_error;
                return fs::exists(join_path(s_state_directory_,
                                            s_plain_identifier + "-" + std::to_string(i_index)
                                                + ".manifest"),
                                  o_check_error);
            };
            // An existing launcher wins the plain identifier, so start suffixed.
            int i_suffix = 2;
            s_candidate = s_stem_id + "-" + std::to_string(i_suffix) + ".desktop";
            while (o_taken(s_candidate) || o_manifest_taken(i_suffix)) {
                i_suffix++;
                s_candidate = s_stem_id + "-" + std::to_string(i_suffix) + ".desktop";
            }
            o_plan.desktop_id = s_candidate;
            o_plan.identifier = s_plain_identifier + "-" + std::to_string(i_suffix);
            if (o_options.icon_name_override.empty()) {
                o_plan.icon_name = s_plain_icon_name + "-" + std::to_string(i_suffix);
            }
            o_plan.notes.push_back("installed alongside "
                                   + std::to_string(o_plan.conflicts.size())
                                   + " existing launcher(s) as " + o_plan.desktop_id
                                   + " (record " + o_plan.identifier + ")");
            o_plan.mode = "add alongside as " + o_plan.desktop_id;
        } else if (b_has_real_conflict
                   && integration_conflict_policy_e::replace == o_options.conflict_policy) {
            o_plan.replace_conflicts = true;
            o_plan.notes.push_back("replaces " + std::to_string(o_plan.conflicts.size())
                                   + " existing launcher(s); they are backed up and restored "
                                     "on uninstall");
            o_plan.mode = "replace an existing launcher";
        } else if (b_has_real_conflict) {
            std::ostringstream o_error;
            int i_count = 0;
            for (const integration_conflict_o &o_conflict : o_plan.conflicts) {
                if (!o_conflict.upgrade) {
                    i_count++;
                }
            }
            o_error << i_count << " existing launcher(s) already represent this application:\n";
            for (const integration_conflict_o &o_conflict : o_plan.conflicts) {
                if (o_conflict.upgrade) {
                    continue;
                }
                o_error << "  " << o_conflict.path << "  (" << o_conflict.origin << ")\n";
            }
            o_error << "choose --replace to back them up and install this version in their "
                       "place, or --add to install alongside them";
            o_plan.error = o_error.str();
            o_plan.mode = "another launcher already represents this application";
            return false;
        } else if (!o_plan.conflicts.empty()) {
            bool b_repair = false;
            for (const integration_conflict_o &o_conflict : o_plan.conflicts) {
                if (o_conflict.repair) {
                    b_repair = true;
                    break;
                }
            }
            if (b_repair) {
                o_plan.notes.push_back(
                    "repairs this tool's existing launcher: it points at "
                    + o_plan.conflicts.front().appimage_path
                    + ", and this run rewrites it to the AppImage's managed path");
                o_plan.mode = "repair the launcher (the AppImage is not where it was)";
            } else {
                // Reaching here means a record exists for this launcher, so the only
                // question left is whether the AppImage is already where the launcher
                // runs it from: then nothing is missing, and saying so is more useful
                // than reporting work that would only rewrite the same files.
                if (same_file_path(o_plan.appimage_path, o_plan.installed_path)) {
                    o_plan.notes.push_back(
                        "already integrated: the launcher runs this file, the file is at its "
                        "managed path, and the record exists");
                    o_plan.mode = "properly integrated";
                } else {
                    o_plan.notes.push_back(
                        "upgrades this tool's existing launcher in place, and places the "
                        "AppImage in the managed directory");
                    o_plan.mode = "update the launcher in place";
                }
            }
        } else {
            o_plan.mode = "new integration";
        }

        // Icons from the payload. Themed icons take preference over root icons.
        o_plan.move_appimage = o_options.move_appimage;
        if (o_options.write_icons) {
            std::set<std::string> o_installed_slots;

            std::vector<squashfs_entry_o> o_size_entries;
            if (o_reader.list_directory("/usr/share/icons/hicolor", o_size_entries, s_error)) {
                for (const squashfs_entry_o &o_size_entry : o_size_entries) {
                    if (squashfs_node_type_e::directory != o_size_entry.type) {
                        continue;
                    }
                    std::vector<squashfs_entry_o> o_apps_entries;
                    if (!o_reader.list_directory(o_size_entry.path + "/apps", o_apps_entries,
                                                 s_error)) {
                        continue;
                    }
                    for (const squashfs_entry_o &o_icon_entry : o_apps_entries) {
                        if (0 != o_icon_entry.name.compare(0, s_icon_base.size(), s_icon_base)) {
                            continue;
                        }
                        const std::string s_extension =
                            fs::path(o_icon_entry.name).extension().string();
                        if (".png" != s_extension && ".svg" != s_extension
                            && ".svgz" != s_extension && ".xpm" != s_extension) {
                            continue;
                        }
                        std::string s_size_directory = o_size_entry.name;
                        if (!is_valid_icon_size_directory(s_size_directory)) {
                            // A malformed AppImage may ship a size directory the
                            // theme does not list, such as 0x0; use a standard one.
                            s_size_directory = "256x256";
                        }
                        const std::string s_slot = s_size_directory + "|" + s_extension;
                        if (0 != o_installed_slots.count(s_slot)) {
                            continue;
                        }
                        std::string s_data;
                        if (!o_reader.read_file(o_icon_entry.path, s_data, s_error)) {
                            continue;
                        }
                        integration_icon_o o_icon;
                        o_icon.source_in_payload = o_icon_entry.path;
                        o_icon.installed_name = o_plan.icon_name;
                        o_icon.size_directory = s_size_directory;
                        o_icon.extension = s_extension.empty() ? std::string(".png") : s_extension;
                        o_icon.content = s_data;
                        o_icon.pixel_size = std::max(
                            0, icon_theme_locator_c::size_from_directory_name(s_size_directory));
                        o_installed_slots.insert(s_slot);
                        o_plan.icons.push_back(std::move(o_icon));
                    }
                }
            }

            std::vector<squashfs_entry_o> o_root_entries;
            if (o_reader.list_root(o_root_entries, s_error)) {
                for (const squashfs_entry_o &o_root_entry : o_root_entries) {
                    if (squashfs_node_type_e::directory == o_root_entry.type) {
                        continue;
                    }
                    const bool b_matches =
                        o_root_entry.name == ".DirIcon" || o_root_entry.name == s_icon_value
                        || 0 == o_root_entry.name.compare(0, s_icon_base.size(), s_icon_base);
                    if (!b_matches) {
                        continue;
                    }
                    // Only real image files are icons; a root .desktop file also
                    // starts with the application name and must not be copied.
                    const std::string s_candidate_extension =
                        fs::path(o_root_entry.name).extension().string();
                    const bool b_is_icon_extension =
                        ".png" == s_candidate_extension || ".svg" == s_candidate_extension
                        || ".svgz" == s_candidate_extension || ".xpm" == s_candidate_extension;
                    if (".DirIcon" != o_root_entry.name && !b_is_icon_extension) {
                        continue;
                    }
                    std::string s_data;
                    if (!o_reader.read_file(o_root_entry.path, s_data, s_error)) {
                        continue;
                    }
                    integration_icon_o o_icon;
                    o_icon.source_in_payload = o_root_entry.path;
                    o_icon.installed_name = o_plan.icon_name;
                    o_icon.extension = fs::path(o_root_entry.name).extension().string();
                    if (o_icon.extension.empty() || ".DirIcon" == o_root_entry.name) {
                        o_icon.extension = ".png";
                    }
                    o_icon.content = s_data;
                    int i_width = 0;
                    int i_height = 0;
                    if (read_png_size(s_data, i_width, i_height)) {
                        o_icon.pixel_size = std::max(i_width, i_height);
                        o_icon.size_directory =
                            std::to_string(i_width) + "x" + std::to_string(i_height);
                    } else if (std::string::npos != o_root_entry.name.find(".svg")) {
                        o_icon.pixel_size = 0;
                        o_icon.size_directory = "scalable";
                    } else {
                        o_icon.pixel_size = 256;
                        o_icon.size_directory = "256x256";
                    }
                    const std::string s_slot = o_icon.size_directory + "|" + o_icon.extension;
                    if (0 != o_installed_slots.count(s_slot)) {
                        continue;
                    }
                    o_installed_slots.insert(s_slot);
                    o_plan.icons.push_back(std::move(o_icon));
                }
            }
        }
        if (o_plan.icons.empty() && o_options.write_icons) {
            o_plan.warnings.push_back("no icon was found in the payload");
        }

        // MIME definition files shipped by the AppImage.
        std::vector<squashfs_entry_o> o_mime_entries;
        if (o_reader.list_directory("/usr/share/mime/packages", o_mime_entries, s_error)) {
            for (const squashfs_entry_o &o_mime_entry : o_mime_entries) {
                if (".xml" != fs::path(o_mime_entry.name).extension().string()) {
                    continue;
                }
                std::string s_data;
                if (!o_reader.read_file(o_mime_entry.path, s_data, s_error)) {
                    continue;
                }
                integration_mime_package_o o_package;
                o_package.source_in_payload = o_mime_entry.path;
                o_package.installed_name = o_plan.identifier + "_" + o_mime_entry.name;
                o_package.content = s_data;
                o_plan.mime_package_files.push_back(std::move(o_package));
            }
        }
        o_plan.mime_types = o_entry.list_value("Desktop Entry", "MimeType");

        o_plan.desktop_entry_path = join_path(s_applications_directory_, o_plan.desktop_id);
        o_plan.manifest_path = join_path(s_state_directory_, o_plan.identifier + ".manifest");

        const std::string s_exec_in_embedded =
            o_entry.value("Desktop Entry", "Exec").value_or(std::string());
        if (!s_exec_in_embedded.empty() && 0 == s_exec_in_embedded.find("AppRun")) {
            o_plan.notes.push_back(
                "the embedded Exec names AppRun, which only exists inside the payload; the "
                "launcher will run the AppImage instead");
        }

        // Build the desktop entry text.
        std::ostringstream o_text;
        o_text << "[Desktop Entry]\n";
        std::map<std::string, std::string> o_replacements;        o_replacements["Icon"] = o_plan.icon_name;
        o_replacements["Name"] = o_plan.name;
        o_replacements["Exec"] = o_plan.exec_command;
        o_replacements["TryExec"] = o_plan.installed_path;
        o_replacements["Terminal"] = "false";
        o_replacements["StartupNotify"] = "true";
        o_replacements["Actions"] = "AppImage-Activator;Remove-AppImage;";
        o_replacements["X-AppImage-Identifier"] = o_plan.identifier;
        o_replacements["X-AppImage-Source-Path"] = o_plan.appimage_path;
        o_replacements["X-Integrated-By"] = "gnome-appimage-integration";
        o_replacements["X-Integrated-At"] = current_timestamp();
        if (!o_plan.startup_wm_class.empty()) {
            o_replacements["StartupWMClass"] = o_plan.startup_wm_class;
        }
        const std::set<std::string> o_skipped = {"Path", "X-AppImage-Old-Icon",
                                                 "X-AppImageLauncher-Version",
                                                 "X-AppImage-Integrate"};
        std::set<std::string> o_emitted;
        for (const desktop_entry_key_o &o_key : p_group->keys) {
            if (o_skipped.end() != o_skipped.find(o_key.name)) {
                continue;
            }
            if (!o_key.locale.empty()) {
                o_text << o_key.name << '[' << o_key.locale << "]=" << o_key.value << '\n';
                continue;
            }
            const auto o_replacement = o_replacements.find(o_key.name);
            if (o_replacements.end() != o_replacement) {
                if (0 == o_emitted.count(o_key.name)) {
                    o_text << o_key.name << '=' << o_replacement->second << '\n';
                    o_emitted.insert(o_key.name);
                }
                continue;
            }
            o_text << o_key.name << '=' << o_key.value << '\n';
        }
        for (const std::pair<const std::string, std::string> &o_replacement : o_replacements) {
            if (0 == o_emitted.count(o_replacement.first)) {
                o_text << o_replacement.first << '=' << o_replacement.second << '\n';
                o_emitted.insert(o_replacement.first);
            }
        }
        const std::string s_tool = o_options.tool_path.empty() ? s_tool_path_ : o_options.tool_path;
        // The launcher's context menu in the shell is built from these actions, so the
        // first one opens the activator for this AppImage: the launcher is a download,
        // and the shell's own "App Details" offers the distribution's package instead.
        o_text << "\n[Desktop Action AppImage-Activator]\n"
               << "Name=AppImage Activator\n"
               << "Exec=" << exec_quote(s_tool.empty() ? "appimage-integrate" : s_tool)
               << " handle " << exec_quote(o_plan.installed_path) << "\n"
               << "\n[Desktop Action Remove-AppImage]\n"
               << "Name=Remove this AppImage\n"
               << "Exec=" << exec_quote(s_tool.empty() ? "appimage-integrate" : s_tool)
               << " uninstall --identifier " << o_plan.identifier << "\n";
        o_plan.desktop_entry_text = o_text.str();
        if (!o_options.name_override.empty()) {
            for (const desktop_entry_key_o &o_key : p_group->keys) {
                if ("Name" == o_key.name && !o_key.locale.empty()) {
                    o_plan.notes.push_back(
                        "the embedded entry's localised Name lines are kept as written; only "
                        "the plain Name was renamed");
                    break;
                }
            }
        }

        // Actions list.
        integration_action_o o_move;
        o_move.kind = o_options.move_appimage ? integration_action_o::kind_e::move_appimage
                                              : integration_action_o::kind_e::copy_appimage;
        o_move.source_path = o_plan.appimage_path;
        o_move.target_path = o_plan.installed_path;
        o_move.description = "place the AppImage at its managed location";
        o_plan.actions.push_back(o_move);
        for (const integration_icon_o &o_icon : o_plan.icons) {
            integration_action_o o_action;
            o_action.kind = integration_action_o::kind_e::install_icon;
            o_action.source_path = o_icon.source_in_payload;
            o_action.target_path = join_path(
                join_path(s_icon_directory_, o_icon.size_directory + "/apps"),
                o_plan.icon_name + o_icon.extension);
            o_action.description = "install the icon into the hicolor theme";
            o_plan.actions.push_back(o_action);
        }
        integration_action_o o_entry_action;
        o_entry_action.kind = integration_action_o::kind_e::write_desktop_entry;
        o_entry_action.target_path = o_plan.desktop_entry_path;
        o_entry_action.description = "write the launcher";
        o_plan.actions.push_back(o_entry_action);
        integration_action_o o_manifest_action;
        o_manifest_action.kind = integration_action_o::kind_e::write_manifest;
        o_manifest_action.target_path = o_plan.manifest_path;
        o_manifest_action.description = "record what was written so it can be reversed";
        o_plan.actions.push_back(o_manifest_action);

        o_plan.valid = true;
        return true;
    } catch (const std::exception &o_exception) {
        o_plan.valid = false;
        o_plan.error = o_exception.what();
        return false;
    }
}

bool appimage_integrator_c::install(const integration_plan_o &o_plan,
                                    std::string &s_error) const {
    try {
        if (!o_plan.valid) {
            s_error = "the plan is not valid";
            return false;
        }
        std::error_code o_error;
        const bool b_manifest_exists = fs::exists(o_plan.manifest_path, o_error);
        // The write is allowed when the launcher at the target path is one this plan
        // is already taking responsibility for: an in-place upgrade, or a launcher
        // that --replace displaces.
        bool b_target_is_ours = false;
        for (const integration_conflict_o &o_conflict : o_plan.conflicts) {
            if (o_conflict.path != o_plan.desktop_entry_path) {
                continue;
            }
            if (o_conflict.upgrade
                || (o_plan.replace_conflicts && !o_conflict.upgrade)) {
                b_target_is_ours = true;
                break;
            }
        }
        if (!b_manifest_exists && !b_target_is_ours
            && fs::exists(o_plan.desktop_entry_path, o_error)) {
            s_error = "a desktop entry already exists at " + o_plan.desktop_entry_path
                      + "; choose --replace to back it up, or uninstall it first";
            return false;
        }

        // Retire the manifest of a launcher this tool is upgrading, and back up
        // every launcher that --replace displaces.
        std::vector<std::pair<std::string, std::string>> o_removed_conflicts;
        std::vector<std::pair<std::string, std::string>> o_removed_manifests;
        if (!o_plan.conflicts.empty()) {
            const std::string s_backup_directory = join_path(s_state_directory_, "backup");
            fs::create_directories(s_backup_directory, o_error);
            for (const integration_conflict_o &o_conflict : o_plan.conflicts) {
                const bool b_remove_launcher = !o_conflict.upgrade && o_plan.replace_conflicts;
                // Retire a manifest only when this launcher really loses its place: it is
                // being replaced, or it sits at the identifier being upgraded in place.
                // --add leaves every existing launcher and its record untouched.
                const bool b_retire_manifest =
                    o_conflict.managed
                    && (b_remove_launcher
                        || (o_conflict.upgrade
                            && o_conflict.path == o_plan.desktop_entry_path));
                if (!b_remove_launcher && !b_retire_manifest) {
                    continue;
                }
                if (b_remove_launcher) {
                    std::error_code o_move_error;
                    const std::string s_backup = unique_backup_path(
                        s_backup_directory, fs::path(o_conflict.path).filename().string());
                    fs::rename(o_conflict.path, s_backup, o_move_error);
                    if (o_move_error) {
                        o_move_error.clear();
                        fs::copy_file(o_conflict.path, s_backup,
                                      fs::copy_options::overwrite_existing, o_move_error);
                        if (o_move_error) {
                            s_error = "cannot back up the existing launcher " + o_conflict.path
                                      + ": " + o_move_error.message();
                            return false;
                        }
                        fs::remove(o_conflict.path, o_move_error);
                    }
                    o_removed_conflicts.emplace_back(o_conflict.path, s_backup);
                }
                if (b_retire_manifest) {
                    for (const installed_appimage_o &o_entry : list_installed()) {
                        if (o_entry.desktop_entry_path != o_conflict.path) {
                            continue;
                        }
                        const std::string s_manifest_backup = unique_backup_path(
                            s_backup_directory, o_entry.identifier + ".manifest");
                        std::error_code o_manifest_error;
                        fs::rename(o_entry.manifest_path, s_manifest_backup, o_manifest_error);
                        if (!o_manifest_error) {
                            o_removed_manifests.emplace_back(o_entry.manifest_path,
                                                             s_manifest_backup);
                        }
                        break;
                    }
                }
            }
        }

        // Place the AppImage.
        if (o_plan.appimage_path != o_plan.installed_path) {
            const bool b_source_exists = fs::exists(o_plan.appimage_path, o_error);
            const bool b_target_exists = fs::exists(o_plan.installed_path, o_error);
            if (!b_source_exists && b_target_exists) {
                // A retry after a previous run already placed the file.
            } else if (!b_source_exists) {
                s_error = "the AppImage is no longer at " + o_plan.appimage_path
                          + " and it is not at " + o_plan.installed_path + " either";
                return false;
            } else {
                fs::create_directories(fs::path(o_plan.installed_path).parent_path(), o_error);
                bool b_placed = false;
                if (o_plan.move_appimage) {
                    fs::rename(o_plan.appimage_path, o_plan.installed_path, o_error);
                    b_placed = !o_error;
                }
                if (!b_placed) {
                    o_error.clear();
                    fs::copy_file(o_plan.appimage_path, o_plan.installed_path,
                                  fs::copy_options::overwrite_existing, o_error);
                    if (o_error) {
                        s_error = "cannot place the AppImage at " + o_plan.installed_path + ": "
                                  + o_error.message();
                        return false;
                    }
                    if (o_plan.move_appimage) {
                        fs::remove(o_plan.appimage_path, o_error);
                    }
                }
            }
        }
        if (!make_executable(o_plan.installed_path, s_error)) {
            return false;
        }

        // Install icons.
        std::vector<std::string> o_written_icons;
        for (const integration_icon_o &o_icon : o_plan.icons) {
            const std::string s_target =
                join_path(join_path(s_icon_directory_, o_icon.size_directory + "/apps"),
                          o_plan.icon_name + o_icon.extension);
            if (!write_file_bytes(s_target, o_icon.content, s_error)) {
                return false;
            }
            o_written_icons.push_back(s_target);
        }

        // Install MIME packages.
        std::vector<std::string> o_written_mime;
        for (const integration_mime_package_o &o_package : o_plan.mime_package_files) {
            const std::string s_target =
                join_path(s_mime_packages_directory_, o_package.installed_name);
            if (!write_file_bytes(s_target, o_package.content, s_error)) {
                return false;
            }
            o_written_mime.push_back(s_target);
        }

        // Write the desktop entry.
        if (!write_file_bytes(o_plan.desktop_entry_path, o_plan.desktop_entry_text, s_error)) {
            return false;
        }

        // Write the manifest.
        std::ostringstream o_manifest;
        o_manifest << "identifier=" << o_plan.identifier << '\n'
                   << "appimage_path=" << o_plan.installed_path << '\n'
                   << "source_path=" << o_plan.appimage_path << '\n'
                   << "desktop_id=" << o_plan.desktop_id << '\n'
                   << "desktop_entry=" << o_plan.desktop_entry_path << '\n'
                   << "icon_name=" << o_plan.icon_name << '\n';
        if (!o_plan.startup_wm_class.empty()) {
            o_manifest << "startup_wm_class=" << o_plan.startup_wm_class << '\n';
        }
        for (const std::string &s_path : o_written_icons) {
            o_manifest << "icon=" << s_path << '\n';
        }
        for (const std::string &s_path : o_written_mime) {
            o_manifest << "mime_package=" << s_path << '\n';
        }
        for (const std::pair<std::string, std::string> &o_pair : o_removed_conflicts) {
            o_manifest << "removed_conflict=" << o_pair.first << '|' << o_pair.second << '\n';
        }
        for (const std::pair<std::string, std::string> &o_pair : o_removed_manifests) {
            o_manifest << "removed_manifest=" << o_pair.first << '|' << o_pair.second << '\n';
        }
        if (!write_file_bytes(o_plan.manifest_path, o_manifest.str(), s_error)) {
            return false;
        }

        if (command_exists("update-desktop-database")) {
            int i_exit_code = 0;
            run_command({"update-desktop-database", s_applications_directory_}, i_exit_code);
        }
        if (!o_written_icons.empty()) {
            refresh_icon_cache(s_icon_directory_);
        }
        if (!o_written_mime.empty() && command_exists("update-mime-database")) {
            int i_exit_code = 0;
            run_command({"update-mime-database", join_path(s_data_home_, "mime")}, i_exit_code);
        }
        return true;
    } catch (const std::exception &o_exception) {
        s_error = o_exception.what();
        return false;
    }
}

bool appimage_integrator_c::uninstall(const std::string &s_identifier,
                                      bool b_remove_appimage,
                                      std::string &s_error) const {
    try {
        const std::string s_manifest = join_path(s_state_directory_, s_identifier + ".manifest");
        std::error_code o_error;
        if (!fs::exists(s_manifest, o_error)) {
            s_error = "no manifest for identifier " + s_identifier;
            return false;
        }
        const std::vector<std::pair<std::string, std::string>> o_lines = read_manifest(s_manifest);
        const std::string s_appimage = manifest_get(o_lines, "appimage_path");
        fs::remove(manifest_get(o_lines, "desktop_entry"), o_error);
        for (const std::string &s_path : manifest_get_all(o_lines, "icon")) {
            o_error.clear();
            fs::remove(s_path, o_error);
        }
        refresh_icon_cache(s_icon_directory_);
        for (const std::string &s_path : manifest_get_all(o_lines, "mime_package")) {
            o_error.clear();
            fs::remove(s_path, o_error);
        }
        for (const std::string &s_pair : manifest_get_all(o_lines, "removed_conflict")) {
            std::string s_restore_error;
            restore_backup_pair(s_pair, s_restore_error);
        }
        for (const std::string &s_pair : manifest_get_all(o_lines, "removed_manifest")) {
            std::string s_restore_error;
            restore_backup_pair(s_pair, s_restore_error);
        }
        if (b_remove_appimage && !s_appimage.empty()) {
            o_error.clear();
            fs::remove(s_appimage, o_error);
        }
        o_error.clear();
        fs::remove(s_manifest, o_error);

        if (command_exists("update-desktop-database")) {
            int i_exit_code = 0;
            run_command({"update-desktop-database", s_applications_directory_}, i_exit_code);
        }
        return true;
    } catch (const std::exception &o_exception) {
        s_error = o_exception.what();
        return false;
    }
}

std::vector<installed_appimage_o> appimage_integrator_c::list_installed() const {
    std::vector<installed_appimage_o> o_results;
    try {
        std::error_code o_error;
        if (!fs::is_directory(s_state_directory_, o_error)) {
            return o_results;
        }
        std::vector<std::string> o_manifests;
        for (const fs::directory_entry &o_entry :
             fs::directory_iterator(s_state_directory_, o_error)) {
            if (o_entry.is_regular_file(o_error)
                && ".manifest" == o_entry.path().extension().string()) {
                o_manifests.push_back(o_entry.path().string());
            }
        }
        std::sort(o_manifests.begin(), o_manifests.end());
        for (const std::string &s_manifest : o_manifests) {
            const std::vector<std::pair<std::string, std::string>> o_lines =
                read_manifest(s_manifest);
            installed_appimage_o o_installed;
            o_installed.identifier = manifest_get(o_lines, "identifier");
            // The handler keeps its own record in this directory, and it has no
            // identifier: it is not an installed AppImage.
            if (o_installed.identifier.empty()) {
                continue;
            }
            o_installed.appimage_path = manifest_get(o_lines, "appimage_path");
            o_installed.desktop_entry_path = manifest_get(o_lines, "desktop_entry");
            o_installed.desktop_id = manifest_get(o_lines, "desktop_id");
            o_installed.icon_name = manifest_get(o_lines, "icon_name");
            o_installed.manifest_path = s_manifest;
            o_results.push_back(std::move(o_installed));
        }
    } catch (const std::exception &) {
        return o_results;
    }
    return o_results;
}

std::string appimage_integrator_c::describe(const std::string &s_appimage_path,
                                            const integration_options_o &o_options) const {
    std::ostringstream o_out;
    try {
        appimage_info_o o_info;
        if (!appimage_reader_c::read(s_appimage_path, o_info)) {
            o_out << "Cannot read " << s_appimage_path << "\n" << o_info.error << "\n";
            return o_out.str();
        }
        o_out << "AppImage: " << s_appimage_path << "\n"
              << "detection: " << appimage_detection_name(o_info.detection) << "\n"
              << "size: " << o_info.file_size << " bytes\n"
              << "payload: offset " << o_info.payload_offset << ", size " << o_info.payload_size
              << "\n";
        std::string s_version_source;
        const std::string s_version = version_of_appimage(s_appimage_path, s_version_source);
        if (!s_version.empty()) {
            o_out << "version: " << s_version << "  (from " << s_version_source << ")\n";
        }
        if (o_info.has_squashfs) {
            o_out << "compression: " << squashfs_compression_name(o_info.squashfs.compression)
                  << "\n";
        }
        if (o_info.update_information_section.present) {
            o_out << "update information: " << o_info.update_information << "\n";
        }
        o_out << "signature: "
              << (o_info.signature_section.present
                      ? (o_info.signature_is_empty ? "present (empty padding)" : "present")
                      : "(absent)")
              << "\n";

        if (appimage_detection_e::type2 == o_info.detection && o_info.has_squashfs) {
            squashfs_reader_c o_reader;
            std::string s_error;
            if (o_reader.open(s_appimage_path, o_info.payload_offset, s_error)) {
                std::vector<squashfs_entry_o> o_desktop_entries;
                if (o_reader.list_root_files_with_extension(".desktop", o_desktop_entries, s_error)
                    && !o_desktop_entries.empty()) {
                    std::string s_content;
                    if (o_reader.read_file(o_desktop_entries[0].path, s_content, s_error)) {
                        o_out << "\nembedded desktop entry: " << o_desktop_entries[0].path << "\n"
                              << s_content;
                        if (s_content.empty() || '\n' != s_content.back()) {
                            o_out << "\n";
                        }
                    } else {
                        o_out << "\nembedded desktop entry: cannot read: " << s_error << "\n";
                    }
                } else {
                    o_out << "\nembedded desktop entry: (none)\n";
                }
                std::vector<squashfs_entry_o> o_root_entries;
                if (o_reader.list_root(o_root_entries, s_error)) {
                    o_out << "\npayload root:\n";
                    for (const squashfs_entry_o &o_entry : o_root_entries) {
                        std::uint64_t u_size = o_entry.size;
                        squashfs_stat_o o_stat;
                        if (o_reader.stat(o_entry.path, o_stat, s_error)) {
                            u_size = o_stat.size;
                        }
                        o_out << "  " << squashfs_node_type_name(o_entry.type) << "\t" << u_size
                              << "\t" << o_entry.path << "\n";
                    }
                }
            } else {
                o_out << "\npayload: " << s_error << "\n";
            }
        }

        integration_plan_o o_plan;
        if (plan(s_appimage_path, o_options, o_plan)) {
            o_out << "\n" << describe_plan(o_plan);
        } else {
            o_out << "\ninstall preview unavailable: " << o_plan.error << "\n";
            for (const integration_conflict_o &o_conflict : o_plan.conflicts) {
                o_out << "  existing launcher: " << o_conflict.path << "  (" << o_conflict.origin
                      << ")\n";
            }
        }
    } catch (const std::exception &o_exception) {
        o_out << "error: " << o_exception.what() << "\n";
    }
    return o_out.str();
}

std::string appimage_integrator_c::describe_plan(const integration_plan_o &o_plan) const {
    std::ostringstream o_out;
    if (!o_plan.valid) {
        o_out << o_plan.error << "\n";
        return o_out.str();
    }
    o_out << "this run:     " << (o_plan.mode.empty() ? "(unknown)" : o_plan.mode) << "\n"
          << "desktop id:   " << o_plan.desktop_id << "\n"
          << "launcher:     " << o_plan.desktop_entry_path << "\n"
          << "appimage:     " << o_plan.installed_path << "\n"
          << "exec:         " << o_plan.exec_command << "\n"
          << "icon:         " << o_plan.icon_name << "\n"
          << "manifest:     " << o_plan.manifest_path << "\n";
    if (!o_plan.version.empty()) {
        o_out << "version:      " << o_plan.version << "  (from " << o_plan.version_source << ")\n";
    }
    if (!o_plan.startup_wm_class.empty()) {
        o_out << "wm class:     " << o_plan.startup_wm_class << "\n";
    }
    if (!o_plan.conflicts.empty()) {
        o_out << "\nexisting launchers for this application:\n";
        for (const integration_conflict_o &o_conflict : o_plan.conflicts) {
            o_out << "  " << o_conflict.path << "  (" << o_conflict.origin << ")\n";
        }
    }
    if (!o_plan.icons.empty()) {
        o_out << "\nicons:\n";
        for (const integration_icon_o &o_icon : o_plan.icons) {
            o_out << "  " << o_icon.size_directory << " " << o_icon.extension << "  "
                  << o_icon.source_in_payload << "\n";
        }
    }
    for (const std::string &s_warning : o_plan.warnings) {
        o_out << "warning: " << s_warning << "\n";
    }
    for (const std::string &s_note : o_plan.notes) {
        o_out << "note: " << s_note << "\n";
    }
    o_out << "\nlauncher contents:\n" << o_plan.desktop_entry_text;
    return o_out.str();
}

std::vector<audit_finding_o> appimage_integrator_c::audit() const {
    std::vector<audit_finding_o> o_findings;
    try {
        const desktop_entry_locator_c o_locator(o_environment_);
        const icon_theme_locator_c o_icon_locator(o_environment_);
        const std::vector<std::string> o_common_themes = {"Yaru", "Adwaita", "gnome",
                                                          "Humanity", "HighContrast"};
        const std::vector<gnome_appimage::desktop::desktop_entry_candidate_o> o_candidates =
            o_locator.list();
        const std::vector<installed_appimage_o> o_installed = list_installed();

        std::map<std::string, std::vector<std::string>> o_groups;
        for (const gnome_appimage::desktop::desktop_entry_candidate_o &o_candidate :
             o_candidates) {
            desktop_entry_file_o o_entry;
            std::vector<gnome_appimage::desktop::desktop_entry_diagnostic_o> o_diagnostics;
            if (!desktop_entry_reader_c::parse_file(o_candidate.path, o_entry, o_diagnostics)) {
                o_findings.push_back({audit_finding_o::severity_e::error, o_candidate.id,
                                      "the desktop entry cannot be parsed",
                                      "run: desktop-inspect " + o_candidate.path});
                continue;
            }
            const std::string s_exec = o_entry.value("Desktop Entry", "Exec").value_or("");
            const std::string s_program = exec_program(s_exec);
            if (!s_program.empty()) {
                if (0 == s_program.find('/')) {
                    if (!fs::exists(s_program)) {
                        o_findings.push_back({audit_finding_o::severity_e::error, o_candidate.id,
                                              "Exec target is missing: " + s_program,
                                              "remove this launcher or restore the file"});
                    }
                } else if (!command_exists(s_program)) {
                    o_findings.push_back({audit_finding_o::severity_e::warning, o_candidate.id,
                                          "Exec program is not on PATH: " + s_program,
                                          "reinstall or edit the entry"});
                }
            }
            const std::string s_try_exec =
                o_entry.value("Desktop Entry", "TryExec").value_or("");
            if (!s_try_exec.empty() && !fs::exists(s_try_exec)) {
                o_findings.push_back({audit_finding_o::severity_e::error, o_candidate.id,
                                      "TryExec target is missing: " + s_try_exec,
                                      "remove this launcher or restore the file"});
            }
            for (const desktop_entry_group_o &o_group : o_entry.groups) {
                const std::string s_action_prefix = "Desktop Action ";
                if (0 != o_group.name.compare(0, s_action_prefix.size(), s_action_prefix)) {
                    continue;
                }
                const desktop_entry_key_o *p_action_exec = o_group.find_key("Exec");
                if (nullptr == p_action_exec) {
                    continue;
                }
                const std::string s_action_program = exec_program(p_action_exec->value);
                if (!s_action_program.empty() && 0 == s_action_program.find('/')
                    && !fs::exists(s_action_program)) {
                    o_findings.push_back(
                        {audit_finding_o::severity_e::warning, o_candidate.id,
                         "context action \"" + o_group.name + "\" runs a missing program: "
                             + s_action_program,
                         "re-integrate the AppImage to replace the action"});
                }
            }
            if (!o_entry.value("Desktop Entry", "X-AppImage-Identifier").value_or("").empty()) {
                // This tool writes X-AppImage-Identifier too, so match it against the
                // records before blaming AppImageLauncher.
                std::vector<const installed_appimage_o *> o_records;
                for (const installed_appimage_o &o_installed : o_installed) {
                    if (o_installed.desktop_entry_path == o_candidate.path) {
                        o_records.push_back(&o_installed);
                    }
                }
                if (o_records.empty()) {
                    const bool b_written_by_this_tool =
                        !o_entry.value("Desktop Entry", "X-Integrated-By").value_or("").empty();
                    o_findings.push_back(
                        {audit_finding_o::severity_e::info, o_candidate.id,
                         b_written_by_this_tool
                             ? "this tool wrote this launcher but has no record of it"
                             : "entry was written by AppImageLauncher (X-AppImage-Identifier "
                               "present)",
                         "re-integrate with: appimage-integrate install <AppImage>"});
                }
                for (const installed_appimage_o *p_record : o_records) {
                    // A record and its launcher disagree when a second AppImage took the
                    // identifier over, leaving the record pointing somewhere else.
                    if (!p_record->appimage_path.empty() && !s_program.empty()
                        && !same_file_path(s_program, p_record->appimage_path)) {
                        o_findings.push_back(
                            {audit_finding_o::severity_e::warning, o_candidate.id,
                             "the record " + p_record->identifier + " says "
                                 + p_record->appimage_path + " but the launcher runs "
                                 + s_program,
                             "re-integrate whichever AppImage should own this launcher"});
                    }
                }
                if (1 < o_records.size()) {
                    std::string s_identifiers;
                    for (const installed_appimage_o *p_record : o_records) {
                        s_identifiers += (s_identifiers.empty() ? "" : ", ") + p_record->identifier;
                    }
                    o_findings.push_back(
                        {audit_finding_o::severity_e::warning, o_candidate.id,
                         std::to_string(o_records.size()) + " records claim this launcher: "
                             + s_identifiers,
                         "uninstall the record that should not own this launcher"});
                }
            }
            const std::string s_icon = o_entry.value("Desktop Entry", "Icon").value_or("");
            if (!s_icon.empty() && 0 == s_icon.find('/')) {
                o_findings.push_back({audit_finding_o::severity_e::warning, o_candidate.id,
                                      "Icon is an absolute path, so themes and scaling do not "
                                      "apply: " + s_icon,
                                      "install the icon under hicolor and use a name"});
            } else if (!s_icon.empty()) {
                bool b_resolved = o_icon_locator.lookup(s_icon).found;
                for (const std::string &s_theme : o_common_themes) {
                    if (b_resolved) {
                        break;
                    }
                    b_resolved = o_icon_locator.lookup(s_icon, s_theme).found;
                }
                if (!b_resolved) {
                    o_findings.push_back({audit_finding_o::severity_e::warning, o_candidate.id,
                                          "Icon does not resolve in any known theme: " + s_icon,
                                          "run: desktop-inspect --icon " + s_icon + " --why"});
                }
            }

            // The remaining checks concern AppImage launchers, which are this
            // project's subject; ordinary desktop files are left alone.
            const bool b_appimage_launcher =
                std::string::npos != s_exec.find(".AppImage")
                || std::string::npos != s_try_exec.find(".AppImage");
            if (b_appimage_launcher) {
                if (o_entry.value("Desktop Entry", "StartupWMClass").value_or("").empty()) {
                    o_findings.push_back(
                        {audit_finding_o::severity_e::warning, o_candidate.id,
                         "AppImage launcher has no StartupWMClass, so the dock may not match the "
                         "window to it",
                         "run: xprop WM_CLASS, then reinstall with --wm-class"});
                }
                std::string s_key = o_entry.value("Desktop Entry", "Name").value_or("");
                const std::size_t i_suffix = s_key.rfind(" (");
                if (std::string::npos != i_suffix && ')' == s_key.back()) {
                    s_key = s_key.substr(0, i_suffix);
                }
                if (s_key.empty()) {
                    s_key = fs::path(s_program).filename().string();
                }
                if (!s_key.empty()) {
                    o_groups[s_key].push_back(o_candidate.id);
                }
            }
        }

        for (const std::pair<const std::string, std::vector<std::string>> &o_group : o_groups) {
            if (1 < o_group.second.size()) {
                std::string s_ids;
                for (const std::string &s_id : o_group.second) {
                    s_ids += (s_ids.empty() ? "" : ", ") + s_id;
                }
                o_findings.push_back(
                    {audit_finding_o::severity_e::warning, o_group.first,
                     "several launchers claim the same application: " + s_ids,
                     "keep one and remove the others"});
            }
        }

        // Icon size directories that no theme lookup will find.
        std::error_code o_error;
        if (fs::is_directory(s_icon_directory_, o_error)) {
            for (const fs::directory_entry &o_entry :
                 fs::directory_iterator(s_icon_directory_, o_error)) {
                if (!o_entry.is_directory(o_error)) {
                    continue;
                }
                const std::string s_name = o_entry.path().filename().string();
                bool b_valid_size = "scalable" == s_name;
                if (!b_valid_size) {
                    const std::size_t i_cross = s_name.find('x');
                    if (std::string::npos != i_cross) {
                        const std::string s_width = s_name.substr(0, i_cross);
                        const std::string s_height = s_name.substr(i_cross + 1);
                        const bool b_digits =
                            !s_width.empty() && !s_height.empty()
                            && std::string::npos == s_width.find_first_not_of("0123456789")
                            && std::string::npos == s_height.find_first_not_of("0123456789");
                        b_valid_size = b_digits && 0 != std::stoi(s_width)
                                       && 0 != std::stoi(s_height);
                    }
                }
                if (!b_valid_size) {
                    o_findings.push_back(
                        {audit_finding_o::severity_e::warning,
                         o_entry.path().string(),
                         "icon size directory is not a valid theme size: " + s_name,
                         "move the icons into an <N>x<N> or scalable directory"});
                }
            }
        }

        // Clutter in the applications directory.
        if (fs::is_directory(s_applications_directory_, o_error)) {
            for (const fs::directory_entry &o_entry :
                 fs::directory_iterator(s_applications_directory_, o_error)) {
                const std::string s_name = o_entry.path().filename().string();
                if (std::string::npos != s_name.find(".desktop.")) {
                    o_findings.push_back({audit_finding_o::severity_e::info,
                                          o_entry.path().string(),
                                          "backup file in the applications directory",
                                          "delete it if it is no longer needed"});
                }
            }
        }

        // The current AppImage MIME defaults.
        const mime_association_reader_c o_mime(o_environment_);
        for (const char *s_type : {"application/vnd.appimage", "application/x-appimage",
                                   "application/x-iso9660-appimage"}) {
            const gnome_appimage::desktop::mime_lookup_o o_lookup = o_mime.lookup(s_type);
            if (o_lookup.found) {
                o_findings.push_back(
                    {audit_finding_o::severity_e::info, s_type,
                     "default handler is " + o_lookup.default_application.desktop_id + " from "
                         + o_lookup.default_application.source_file,
                     "change it with: appimage-integrate handler install"});
            } else {
                o_findings.push_back({audit_finding_o::severity_e::info, s_type,
                                      "no default handler is recorded",
                                      "run: appimage-integrate handler install"});
            }
        }
    } catch (const std::exception &) {
        return o_findings;
    }
    return o_findings;
}

}  // namespace gnome_appimage::integration
