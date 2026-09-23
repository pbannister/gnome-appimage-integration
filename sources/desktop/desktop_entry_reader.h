#pragma once

#include <cstddef>
#include <optional>
#include <string>
#include <vector>

namespace gnome_appimage::desktop {

// Severity of a parse or validation diagnostic.
enum class desktop_entry_severity_e {
    warning,
    error,
};

// A single key/value pair from a desktop entry group.
// The name excludes any locale postfix, which is held separately.
struct desktop_entry_key_o {
    std::string name;
    std::string locale;
    std::string value;
    std::vector<std::string> values;
    std::size_t line_number = 0;
};

// A [group] header and the keys that follow it, in file order.
struct desktop_entry_group_o {
    std::string name;
    std::size_t line_number = 0;
    std::vector<desktop_entry_key_o> keys;

    // Return the key with the given base name and no locale postfix.
    const desktop_entry_key_o *find_key(const std::string &s_name) const;

    // Return the key with the given base name and locale postfix.
    const desktop_entry_key_o *find_key(const std::string &s_name,
                                        const std::string &s_locale) const;
};

// A parse or validation message tied to a one-based source line.
struct desktop_entry_diagnostic_o {
    desktop_entry_severity_e severity = desktop_entry_severity_e::error;
    std::size_t line_number = 0;
    std::string message;
};

// A parsed desktop entry file, preserving source order.
struct desktop_entry_file_o {
    std::vector<std::string> lines;
    std::vector<desktop_entry_group_o> groups;

    const desktop_entry_group_o *find_group(const std::string &s_name) const;

    const desktop_entry_key_o *find_key(const std::string &s_group,
                                        const std::string &s_name,
                                        const std::string &s_locale = std::string()) const;

    // Return the value selected for the locale, or the unlocalized value.
    std::optional<std::string> value(const std::string &s_group,
                                     const std::string &s_name,
                                     const std::string &s_locale = std::string()) const;

    // Return the list elements selected for the locale.
    std::vector<std::string> list_value(const std::string &s_group,
                                        const std::string &s_name,
                                        const std::string &s_locale = std::string()) const;

    std::optional<bool> bool_value(const std::string &s_group,
                                   const std::string &s_name) const;

    // Return the Type value of the [Desktop Entry] group, or an empty string.
    std::string type() const;

    const desktop_entry_group_o *desktop_entry_group() const;
};

// Reader for desktop entry files. The public interface does not throw.
class desktop_entry_reader_c {
public:
    static bool parse_text(const std::string &s_text,
                           desktop_entry_file_o &o_file,
                           std::vector<desktop_entry_diagnostic_o> &o_diagnostics);

    static bool parse_file(const std::string &s_path,
                           desktop_entry_file_o &o_file,
                           std::vector<desktop_entry_diagnostic_o> &o_diagnostics);
};

// Validate required keys and report deprecated Exec field codes.
std::vector<desktop_entry_diagnostic_o> desktop_entry_validate(const desktop_entry_file_o &o_file);

// Return the Exec field codes in order of appearance, including literal percent signs
// reported as "%%". Unknown codes are returned unchanged.
std::vector<std::string> exec_field_codes(const std::string &s_exec);

// The program an Exec line runs: the first token, with quoting and escapes resolved.
// Returns an empty string when the line names no program.
std::string desktop_exec_program(const std::string &s_exec);

// Return only the deprecated Exec field codes found in the value.
std::vector<std::string> exec_deprecated_field_codes(const std::string &s_exec);

// Return locale postfixes to try for the given LC_MESSAGES value, best match first.
std::vector<std::string> locale_match_candidates(const std::string &s_locale);

// Return true when the value is one of the three defined entry types.
bool desktop_entry_type_is_known(const std::string &s_type);

}  // namespace gnome_appimage::desktop
