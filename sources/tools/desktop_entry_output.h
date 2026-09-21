#pragma once

#include "desktop/desktop_entry_reader.h"

#include <cstdio>
#include <ostream>
#include <set>
#include <string>
#include <vector>

namespace gnome_appimage::tools {

// Escape a string for inclusion in a JSON document.
inline std::string json_escape(const std::string &s_text) {
    std::string s_result;
    for (const char c_character : s_text) {
        switch (c_character) {
            case '"':
                s_result += "\\\"";
                break;
            case '\\':
                s_result += "\\\\";
                break;
            case '\n':
                s_result += "\\n";
                break;
            case '\r':
                s_result += "\\r";
                break;
            case '\t':
                s_result += "\\t";
                break;
            default:
                if (static_cast<unsigned char>(c_character) < 0x20) {
                    char s_buffer[8];
                    std::snprintf(s_buffer, sizeof(s_buffer), "\\u%04x",
                                  static_cast<unsigned int>(
                                      static_cast<unsigned char>(c_character)));
                    s_result += s_buffer;
                } else {
                    s_result += c_character;
                }
                break;
        }
    }
    return s_result;
}

// Print the raw groups, keys, and values in source order.
inline void print_desktop_entry_text(const desktop::desktop_entry_file_o &o_file,
                                     std::ostream &o_out) {
    for (const desktop::desktop_entry_group_o &o_group : o_file.groups) {
        o_out << '[' << o_group.name << "]\n";
        for (const desktop::desktop_entry_key_o &o_key : o_group.keys) {
            o_out << o_key.name;
            if (!o_key.locale.empty()) {
                o_out << '[' << o_key.locale << ']';
            }
            o_out << '=' << o_key.value << '\n';
        }
    }
}

// Print one effective value per key of the [Desktop Entry] group.
inline void print_desktop_entry_resolved(const desktop::desktop_entry_file_o &o_file,
                                         const std::string &s_locale,
                                         std::ostream &o_out) {
    const desktop::desktop_entry_group_o *p_group = o_file.desktop_entry_group();
    if (nullptr == p_group) {
        return;
    }
    o_out << "resolved[" << (s_locale.empty() ? "C" : s_locale) << "]\n";
    std::set<std::string> o_seen;
    for (const desktop::desktop_entry_key_o &o_key : p_group->keys) {
        if (0 != o_seen.count(o_key.name)) {
            continue;
        }
        o_seen.insert(o_key.name);
        const std::optional<std::string> s_value = o_file.value(p_group->name, o_key.name,
                                                                s_locale);
        if (s_value.has_value()) {
            o_out << "  " << o_key.name << '=' << *s_value << '\n';
        }
    }
}

inline const char *severity_name(desktop::desktop_entry_severity_e e_severity) {
    return desktop::desktop_entry_severity_e::error == e_severity ? "error" : "warning";
}

// Serialize the groups array of a desktop entry as JSON.
inline void print_desktop_entry_groups_json(const desktop::desktop_entry_file_o &o_file,
                                            std::ostream &o_out) {
    o_out << '[';
    bool b_first_group = true;
    for (const desktop::desktop_entry_group_o &o_group : o_file.groups) {
        if (!b_first_group) {
            o_out << ',';
        }
        b_first_group = false;
        o_out << "{\"name\":\"" << json_escape(o_group.name) << "\",\"line\":"
              << o_group.line_number << ",\"keys\":[";
        bool b_first_key = true;
        for (const desktop::desktop_entry_key_o &o_key : o_group.keys) {
            if (!b_first_key) {
                o_out << ',';
            }
            b_first_key = false;
            o_out << "{\"name\":\"" << json_escape(o_key.name) << "\",\"locale\":\""
                  << json_escape(o_key.locale) << "\",\"value\":\"" << json_escape(o_key.value)
                  << "\",\"line\":" << o_key.line_number << '}';
        }
        o_out << "]}";
    }
    o_out << ']';
}

// Serialize a diagnostics array as JSON.
inline void print_diagnostics_json(
    const std::vector<desktop::desktop_entry_diagnostic_o> &o_diagnostics,
    std::ostream &o_out) {
    o_out << '[';
    bool b_first = true;
    for (const desktop::desktop_entry_diagnostic_o &o_diagnostic : o_diagnostics) {
        if (!b_first) {
            o_out << ',';
        }
        b_first = false;
        o_out << "{\"severity\":\"" << severity_name(o_diagnostic.severity) << "\",\"line\":"
              << o_diagnostic.line_number << ",\"message\":\""
              << json_escape(o_diagnostic.message) << "\"}";
    }
    o_out << ']';
}

inline void print_diagnostics(
    const std::vector<desktop::desktop_entry_diagnostic_o> &o_diagnostics,
    std::ostream &o_out) {
    for (const desktop::desktop_entry_diagnostic_o &o_diagnostic : o_diagnostics) {
        o_out << severity_name(o_diagnostic.severity) << ": ";
        if (0 < o_diagnostic.line_number) {
            o_out << "line " << o_diagnostic.line_number << ": ";
        }
        o_out << o_diagnostic.message << '\n';
    }
}

inline bool has_error_diagnostic(
    const std::vector<desktop::desktop_entry_diagnostic_o> &o_diagnostics) {
    for (const desktop::desktop_entry_diagnostic_o &o_diagnostic : o_diagnostics) {
        if (desktop::desktop_entry_severity_e::error == o_diagnostic.severity) {
            return true;
        }
    }
    return false;
}

}  // namespace gnome_appimage::tools
