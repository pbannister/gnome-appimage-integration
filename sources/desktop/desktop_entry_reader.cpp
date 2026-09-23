#include "desktop/desktop_entry_reader.h"

#include <algorithm>
#include <cstdint>
#include <fstream>
#include <sstream>
#include <utility>

namespace gnome_appimage::desktop {
namespace {

constexpr const char *GROUP_DESKTOP_ENTRY = "Desktop Entry";
constexpr const char *GROUP_DESKTOP_ACTION_PREFIX = "Desktop Action ";
constexpr const char *TYPE_APPLICATION = "Application";
constexpr const char *TYPE_LINK = "Link";
constexpr const char *TYPE_DIRECTORY = "Directory";
constexpr const char *KNOWN_EXEC_FIELD_CODES = "fFuUick";
constexpr const char *DEPRECATED_EXEC_FIELD_CODES = "mvdDnN";

bool is_space_character(char c_character) {
    return ' ' == c_character || '\t' == c_character;
}

bool is_key_base_character(char c_character) {
    return ('A' <= c_character && c_character <= 'Z')
        || ('a' <= c_character && c_character <= 'z')
        || ('0' <= c_character && c_character <= '9')
        || '-' == c_character;
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

std::string to_lower_ascii(const std::string &s_text) {
    std::string s_result = s_text;
    for (char &c_character : s_result) {
        if ('A' <= c_character && c_character <= 'Z') {
            c_character = static_cast<char>(c_character - 'A' + 'a');
        }
    }
    return s_result;
}

bool contains_character(const char *s_set, char c_character) {
    for (const char *p_scan = s_set; '\0' != *p_scan; p_scan++) {
        if (c_character == *p_scan) {
            return true;
        }
    }
    return false;
}

// Validate that the text is well-formed UTF-8 and report the first bad offset.
bool is_valid_utf8(const std::string &s_text, std::size_t &o_bad_offset) {
    std::size_t i_index = 0;
    while (i_index < s_text.size()) {
        const unsigned char u_byte = static_cast<unsigned char>(s_text[i_index]);
        std::size_t i_length = 0;
        if (0x7f >= u_byte) {
            i_length = 1;
        } else if (0xc2 <= u_byte && u_byte <= 0xdf) {
            i_length = 2;
        } else if (0xe0 <= u_byte && u_byte <= 0xef) {
            i_length = 3;
        } else if (0xf0 <= u_byte && u_byte <= 0xf4) {
            i_length = 4;
        } else {
            o_bad_offset = i_index;
            return false;
        }
        if (s_text.size() < i_index + i_length) {
            o_bad_offset = i_index;
            return false;
        }
        for (std::size_t i_follow = 1; i_follow < i_length; i_follow++) {
            const unsigned char u_follow = static_cast<unsigned char>(s_text[i_index + i_follow]);
            if (!(0x80 <= u_follow && u_follow <= 0xbf)) {
                o_bad_offset = i_index;
                return false;
            }
        }
        i_index += i_length;
    }
    return true;
}

std::vector<std::string> split_lines(const std::string &s_body) {
    std::vector<std::string> o_lines;
    std::size_t i_start = 0;
    while (i_start < s_body.size()) {
        const std::size_t i_end = s_body.find('\n', i_start);
        if (std::string::npos == i_end) {
            o_lines.push_back(s_body.substr(i_start));
            break;
        }
        o_lines.push_back(s_body.substr(i_start, i_end - i_start));
        i_start = i_end + 1;
    }
    return o_lines;
}

// Decode the documented escapes and split a list value on unescaped semicolons.
void decode_value(const std::string &s_raw, std::string &s_decoded,
                  std::vector<std::string> &o_values) {
    s_decoded.clear();
    o_values.clear();
    std::string s_element;
    bool b_ended_with_separator = false;
    std::size_t i_index = 0;
    while (i_index < s_raw.size()) {
        const char c_character = s_raw[i_index];
        if ('\\' == c_character && i_index + 1 < s_raw.size()) {
            const char c_next = s_raw[i_index + 1];
            const bool b_documented = contains_character("sntr\\;", c_next);
            char c_mapped = c_next;
            if ('s' == c_next) {
                c_mapped = ' ';
            } else if ('n' == c_next) {
                c_mapped = '\n';
            } else if ('t' == c_next) {
                c_mapped = '\t';
            } else if ('r' == c_next) {
                c_mapped = '\r';
            }
            if (b_documented) {
                s_decoded.push_back(c_mapped);
                s_element.push_back(c_mapped);
            } else {
                s_decoded.push_back('\\');
                s_decoded.push_back(c_next);
                s_element.push_back('\\');
                s_element.push_back(c_next);
            }
            b_ended_with_separator = false;
            i_index += 2;
        } else if (';' == c_character) {
            o_values.push_back(s_element);
            s_element.clear();
            s_decoded.push_back(';');
            b_ended_with_separator = true;
            i_index += 1;
        } else {
            s_decoded.push_back(c_character);
            s_element.push_back(c_character);
            b_ended_with_separator = false;
            i_index += 1;
        }
    }
    o_values.push_back(s_element);
    if (b_ended_with_separator && !o_values.empty() && o_values.back().empty()) {
        o_values.pop_back();
    }
}

// Split Key[locale] into its base name and optional locale postfix.
bool split_localized_key(const std::string &s_key, std::string &s_name, std::string &s_locale) {
    const std::size_t i_open = s_key.find('[');
    if (std::string::npos == i_open) {
        s_name = s_key;
        s_locale.clear();
    } else {
        const std::size_t i_close = s_key.rfind(']');
        if (std::string::npos == i_close || i_close + 1 != s_key.size() || i_close <= i_open) {
            return false;
        }
        s_name = s_key.substr(0, i_open);
        s_locale = s_key.substr(i_open + 1, i_close - i_open - 1);
        if (s_locale.empty()) {
            return false;
        }
    }
    if (s_name.empty()) {
        return false;
    }
    for (const char c_character : s_name) {
        if (!is_key_base_character(c_character)) {
            return false;
        }
    }
    return true;
}

bool is_known_type(const std::string &s_type) {
    return TYPE_APPLICATION == s_type || TYPE_LINK == s_type || TYPE_DIRECTORY == s_type;
}

}  // namespace

// The program an Exec line runs: the first token, with quoting and escapes resolved.
std::string desktop_exec_program(const std::string &s_exec) {
    const std::string s_trimmed = trim_spaces(s_exec);
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


const desktop_entry_key_o *desktop_entry_group_o::find_key(const std::string &s_name) const {
    for (const desktop_entry_key_o &o_key : keys) {
        if (o_key.name == s_name && o_key.locale.empty()) {
            return &o_key;
        }
    }
    return nullptr;
}

const desktop_entry_key_o *desktop_entry_group_o::find_key(const std::string &s_name,
                                                           const std::string &s_locale) const {
    for (const desktop_entry_key_o &o_key : keys) {
        if (o_key.name == s_name && o_key.locale == s_locale) {
            return &o_key;
        }
    }
    return nullptr;
}

const desktop_entry_group_o *desktop_entry_file_o::find_group(const std::string &s_name) const {
    for (const desktop_entry_group_o &o_group : groups) {
        if (o_group.name == s_name) {
            return &o_group;
        }
    }
    return nullptr;
}

const desktop_entry_key_o *desktop_entry_file_o::find_key(const std::string &s_group,
                                                          const std::string &s_name,
                                                          const std::string &s_locale) const {
    const desktop_entry_group_o *p_group = find_group(s_group);
    if (nullptr == p_group) {
        return nullptr;
    }
    if (!s_locale.empty()) {
        for (const std::string &s_candidate : locale_match_candidates(s_locale)) {
            const desktop_entry_key_o *p_key = p_group->find_key(s_name, s_candidate);
            if (nullptr != p_key) {
                return p_key;
            }
        }
    }
    return p_group->find_key(s_name);
}

std::optional<std::string> desktop_entry_file_o::value(const std::string &s_group,
                                                       const std::string &s_name,
                                                       const std::string &s_locale) const {
    const desktop_entry_key_o *p_key = find_key(s_group, s_name, s_locale);
    if (nullptr == p_key) {
        return std::nullopt;
    }
    return p_key->value;
}

std::vector<std::string> desktop_entry_file_o::list_value(const std::string &s_group,
                                                          const std::string &s_name,
                                                          const std::string &s_locale) const {
    const desktop_entry_key_o *p_key = find_key(s_group, s_name, s_locale);
    if (nullptr == p_key) {
        return {};
    }
    return p_key->values;
}

std::optional<bool> desktop_entry_file_o::bool_value(const std::string &s_group,
                                                     const std::string &s_name) const {
    const desktop_entry_key_o *p_key = find_key(s_group, s_name);
    if (nullptr == p_key) {
        return std::nullopt;
    }
    const std::string s_lower = to_lower_ascii(trim_spaces(p_key->value));
    if ("true" == s_lower || "1" == s_lower) {
        return true;
    }
    if ("false" == s_lower || "0" == s_lower) {
        return false;
    }
    return std::nullopt;
}

std::string desktop_entry_file_o::type() const {
    const std::optional<std::string> s_value = value(GROUP_DESKTOP_ENTRY, "Type");
    if (!s_value.has_value()) {
        return {};
    }
    return *s_value;
}

const desktop_entry_group_o *desktop_entry_file_o::desktop_entry_group() const {
    return find_group(GROUP_DESKTOP_ENTRY);
}

bool desktop_entry_reader_c::parse_file(const std::string &s_path,
                                        desktop_entry_file_o &o_file,
                                        std::vector<desktop_entry_diagnostic_o> &o_diagnostics) {
    std::ifstream o_input(s_path, std::ios::binary);
    if (!o_input) {
        o_file = desktop_entry_file_o{};
        o_diagnostics = {{desktop_entry_severity_e::error, 0, "cannot open file: " + s_path}};
        return false;
    }
    std::ostringstream o_buffer;
    o_buffer << o_input.rdbuf();
    return parse_text(o_buffer.str(), o_file, o_diagnostics);
}

bool desktop_entry_reader_c::parse_text(const std::string &s_text,
                                        desktop_entry_file_o &o_file,
                                        std::vector<desktop_entry_diagnostic_o> &o_diagnostics) {
    o_file = desktop_entry_file_o{};
    o_diagnostics.clear();

    std::string s_body = s_text;
    if (3 <= s_body.size()
        && static_cast<unsigned char>(s_body[0]) == 0xef
        && static_cast<unsigned char>(s_body[1]) == 0xbb
        && static_cast<unsigned char>(s_body[2]) == 0xbf) {
        s_body.erase(0, 3);
    }

    std::size_t i_bad_offset = 0;
    if (!is_valid_utf8(s_body, i_bad_offset)) {
        o_diagnostics.push_back({desktop_entry_severity_e::error, 0,
                                 "input is not valid UTF-8 at byte offset "
                                     + std::to_string(i_bad_offset)});
        return false;
    }

    o_file.lines = split_lines(s_body);
    for (std::string &s_line : o_file.lines) {
        if (!s_line.empty() && '\r' == s_line.back()) {
            s_line.pop_back();
        }
    }

    desktop_entry_group_o *p_group = nullptr;
    for (std::size_t i_index = 0; i_index < o_file.lines.size(); i_index++) {
        const std::string &s_line = o_file.lines[i_index];
        const std::size_t i_line_number = i_index + 1;

        if (s_line.empty() || '#' == s_line[0]) {
            continue;
        }

        if ('[' == s_line[0]) {
            if (3 > s_line.size() || ']' != s_line.back()) {
                o_diagnostics.push_back({desktop_entry_severity_e::error, i_line_number,
                                         "malformed group header: " + s_line});
                o_file = desktop_entry_file_o{};
                return false;
            }
            const std::string s_name = s_line.substr(1, s_line.size() - 2);
            if (s_name.empty() || std::string::npos != s_name.find('[')
                || std::string::npos != s_name.find(']')) {
                o_diagnostics.push_back({desktop_entry_severity_e::error, i_line_number,
                                         "invalid group name: " + s_line});
                o_file = desktop_entry_file_o{};
                return false;
            }
            o_file.groups.push_back(desktop_entry_group_o{});
            o_file.groups.back().name = s_name;
            o_file.groups.back().line_number = i_line_number;
            p_group = &o_file.groups.back();
            continue;
        }

        const std::size_t i_equal = s_line.find('=');
        if (std::string::npos == i_equal) {
            o_diagnostics.push_back({desktop_entry_severity_e::error, i_line_number,
                                     "expected 'key=value' or '[group]': " + s_line});
            o_file = desktop_entry_file_o{};
            return false;
        }
        if (nullptr == p_group) {
            o_diagnostics.push_back({desktop_entry_severity_e::error, i_line_number,
                                     "key outside of any group: " + s_line});
            o_file = desktop_entry_file_o{};
            return false;
        }

        const std::string s_key_text = trim_spaces(s_line.substr(0, i_equal));
        const std::string s_value_text = trim_spaces(s_line.substr(i_equal + 1));
        std::string s_name;
        std::string s_locale;
        if (!split_localized_key(s_key_text, s_name, s_locale)) {
            o_diagnostics.push_back({desktop_entry_severity_e::error, i_line_number,
                                     "invalid key name: " + s_key_text});
            o_file = desktop_entry_file_o{};
            return false;
        }

        for (const desktop_entry_key_o &o_existing : p_group->keys) {
            if (o_existing.name == s_name && o_existing.locale == s_locale) {
                o_diagnostics.push_back({desktop_entry_severity_e::warning, i_line_number,
                                         "duplicate key in group [" + p_group->name + "]: "
                                             + s_key_text});
            }
        }

        desktop_entry_key_o o_key;
        o_key.name = s_name;
        o_key.locale = s_locale;
        o_key.line_number = i_line_number;
        decode_value(s_value_text, o_key.value, o_key.values);
        p_group->keys.push_back(std::move(o_key));
    }

    return true;
}

std::vector<std::string> locale_match_candidates(const std::string &s_locale) {
    std::vector<std::string> o_candidates;
    if (s_locale.empty()) {
        return o_candidates;
    }

    std::string s_base = s_locale;
    const std::size_t i_dot = s_base.find('.');
    if (std::string::npos != i_dot) {
        const std::size_t i_at = s_base.find('@', i_dot);
        if (std::string::npos == i_at) {
            s_base.erase(i_dot);
        } else {
            s_base.erase(i_dot, i_at - i_dot);
        }
    }

    std::string s_language_country = s_base;
    std::string s_modifier;
    const std::size_t i_at = s_base.find('@');
    if (std::string::npos != i_at) {
        s_language_country = s_base.substr(0, i_at);
        s_modifier = s_base.substr(i_at + 1);
    }

    std::string s_language = s_language_country;
    std::string s_country;
    const std::size_t i_underscore = s_language_country.find('_');
    if (std::string::npos != i_underscore) {
        s_language = s_language_country.substr(0, i_underscore);
        s_country = s_language_country.substr(i_underscore + 1);
    }

    if (!s_country.empty() && !s_modifier.empty()) {
        o_candidates.push_back(s_language + "_" + s_country + "@" + s_modifier);
        o_candidates.push_back(s_language + "_" + s_country);
        o_candidates.push_back(s_language + "@" + s_modifier);
        o_candidates.push_back(s_language);
    } else if (!s_country.empty()) {
        o_candidates.push_back(s_language + "_" + s_country);
        o_candidates.push_back(s_language);
    } else if (!s_modifier.empty()) {
        o_candidates.push_back(s_language + "@" + s_modifier);
        o_candidates.push_back(s_language);
    } else if (!s_language.empty()) {
        o_candidates.push_back(s_language);
    }
    return o_candidates;
}

std::vector<std::string> exec_field_codes(const std::string &s_exec) {
    std::vector<std::string> o_codes;
    for (std::size_t i_index = 0; i_index + 1 < s_exec.size(); i_index++) {
        if ('%' != s_exec[i_index]) {
            continue;
        }
        o_codes.push_back(std::string("%") + s_exec[i_index + 1]);
        i_index += 1;
    }
    return o_codes;
}

std::vector<std::string> exec_deprecated_field_codes(const std::string &s_exec) {
    std::vector<std::string> o_codes;
    for (const std::string &s_code : exec_field_codes(s_exec)) {
        if (2 == s_code.size() && contains_character(DEPRECATED_EXEC_FIELD_CODES, s_code[1])) {
            o_codes.push_back(s_code);
        }
    }
    return o_codes;
}

bool desktop_entry_type_is_known(const std::string &s_type) {
    return is_known_type(s_type);
}

std::vector<desktop_entry_diagnostic_o> desktop_entry_validate(const desktop_entry_file_o &o_file) {
    std::vector<desktop_entry_diagnostic_o> o_diagnostics;
    const desktop_entry_group_o *p_group = o_file.desktop_entry_group();
    if (nullptr == p_group) {
        o_diagnostics.push_back({desktop_entry_severity_e::error, 0,
                                 "missing required group: [" + std::string(GROUP_DESKTOP_ENTRY)
                                     + "]"});
        return o_diagnostics;
    }

    const std::size_t i_group_line = p_group->line_number;
    const desktop_entry_key_o *p_type = p_group->find_key("Type");
    if (nullptr == p_type) {
        o_diagnostics.push_back({desktop_entry_severity_e::error, i_group_line,
                                 "missing required key: Type"});
    }
    if (nullptr == p_group->find_key("Name")) {
        o_diagnostics.push_back({desktop_entry_severity_e::error, i_group_line,
                                 "missing required key: Name"});
    }

    const std::string s_type = nullptr == p_type ? std::string() : p_type->value;
    if (nullptr != p_type && !is_known_type(s_type)) {
        o_diagnostics.push_back({desktop_entry_severity_e::warning, p_type->line_number,
                                 "unknown entry type: " + s_type});
    }

    if (TYPE_APPLICATION == s_type) {
        const bool b_dbus_activatable = o_file.bool_value(GROUP_DESKTOP_ENTRY, "DBusActivatable")
                                            .value_or(false);
        if (!b_dbus_activatable && nullptr == p_group->find_key("Exec")) {
            o_diagnostics.push_back({desktop_entry_severity_e::error, i_group_line,
                                     "missing required key for Application: Exec"});
        }
    } else if (TYPE_LINK == s_type) {
        if (nullptr == p_group->find_key("URL")) {
            o_diagnostics.push_back({desktop_entry_severity_e::error, i_group_line,
                                     "missing required key for Link: URL"});
        }
    }

    const std::vector<std::string> o_actions =
        o_file.list_value(GROUP_DESKTOP_ENTRY, "Actions");
    for (const std::string &s_action : o_actions) {
        const std::string s_action_group = std::string(GROUP_DESKTOP_ACTION_PREFIX) + s_action;
        if (nullptr == o_file.find_group(s_action_group)) {
            o_diagnostics.push_back({desktop_entry_severity_e::warning, i_group_line,
                                     "Actions lists an undefined action: " + s_action});
        }
    }

    for (const desktop_entry_group_o &o_candidate : o_file.groups) {
        const std::string s_prefix = GROUP_DESKTOP_ACTION_PREFIX;
        if (0 != o_candidate.name.compare(0, s_prefix.size(), s_prefix)) {
            continue;
        }
        const std::string s_action = o_candidate.name.substr(s_prefix.size());
        if (std::find(o_actions.begin(), o_actions.end(), s_action) == o_actions.end()) {
            o_diagnostics.push_back({desktop_entry_severity_e::warning, o_candidate.line_number,
                                     "action group not listed in Actions: " + s_action});
        }
        if (nullptr == o_candidate.find_key("Name")) {
            o_diagnostics.push_back({desktop_entry_severity_e::error, o_candidate.line_number,
                                     "missing required key in action group: Name"});
        }
    }

    const desktop_entry_key_o *p_exec = p_group->find_key("Exec");
    if (nullptr != p_exec) {
        const std::vector<std::string> o_codes = exec_field_codes(p_exec->value);
        for (const std::string &s_code : o_codes) {
            if ("%%" == s_code) {
                continue;
            }
            if (contains_character(DEPRECATED_EXEC_FIELD_CODES, s_code[1])) {
                o_diagnostics.push_back({desktop_entry_severity_e::warning, p_exec->line_number,
                                         "deprecated Exec field code: " + s_code});
            } else if (!contains_character(KNOWN_EXEC_FIELD_CODES, s_code[1])) {
                o_diagnostics.push_back({desktop_entry_severity_e::warning, p_exec->line_number,
                                         "unknown Exec field code: " + s_code});
            }
        }
    }

    return o_diagnostics;
}

}  // namespace gnome_appimage::desktop
