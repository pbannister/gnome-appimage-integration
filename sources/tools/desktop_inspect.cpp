// desktop-inspect: read desktop entry files and locate them on the XDG search path.
#include "desktop/desktop_entry_locator.h"
#include "desktop/desktop_entry_reader.h"
#include "tools/desktop_entry_output.h"
#include "version/version.h"

#include <iostream>
#include <optional>
#include <ostream>
#include <string>
#include <vector>

namespace {

constexpr int EXIT_OK = 0;
constexpr int EXIT_ERROR = 1;
constexpr int EXIT_USAGE = 2;

void print_usage(std::ostream &o_out) {
    o_out << "usage: desktop-inspect [options] [path]\n"
          << "\n"
          << "  path               parse and print one desktop entry file\n"
          << "  --all              list entries on the application search path\n"
          << "  --locate <id>      resolve one desktop file identifier\n"
          << "  --path             print the application search path\n"
          << "  --autostart-path   print the autostart search path\n"
          << "  --locale <locale>  select localized values, for example sr_YU\n"
          << "  --json             print JSON\n"
          << "  --help             print this help\n"
          << "  --version          print the build-time version\n";
}

void print_entry_json(const std::string &s_path,
                      const gnome_appimage::desktop::desktop_entry_file_o &o_file,
                      const std::vector<gnome_appimage::desktop::desktop_entry_diagnostic_o>
                          &o_diagnostics) {
    using gnome_appimage::tools::json_escape;
    std::cout << "{\"path\":\"" << json_escape(s_path) << "\",\"groups\":";
    gnome_appimage::tools::print_desktop_entry_groups_json(o_file, std::cout);
    std::cout << ",\"diagnostics\":";
    gnome_appimage::tools::print_diagnostics_json(o_diagnostics, std::cout);
    std::cout << "}\n";
}

int inspect_path(const std::string &s_path, const std::string &s_locale, bool b_json) {
    using gnome_appimage::desktop::desktop_entry_diagnostic_o;
    using gnome_appimage::desktop::desktop_entry_file_o;
    using gnome_appimage::desktop::desktop_entry_reader_c;

    desktop_entry_file_o o_file;
    std::vector<desktop_entry_diagnostic_o> o_parse_diagnostics;
    if (!desktop_entry_reader_c::parse_file(s_path, o_file, o_parse_diagnostics)) {
        gnome_appimage::tools::print_diagnostics(o_parse_diagnostics, std::cerr);
        return EXIT_ERROR;
    }
    std::vector<desktop_entry_diagnostic_o> o_validation = desktop_entry_validate(o_file);
    std::vector<desktop_entry_diagnostic_o> o_all = o_parse_diagnostics;
    o_all.insert(o_all.end(), o_validation.begin(), o_validation.end());

    if (b_json) {
        print_entry_json(s_path, o_file, o_all);
    } else {
        std::cout << "# " << s_path << '\n';
        gnome_appimage::tools::print_desktop_entry_text(o_file, std::cout);
        gnome_appimage::tools::print_desktop_entry_resolved(o_file, s_locale, std::cout);
    }
    gnome_appimage::tools::print_diagnostics(o_all, std::cerr);
    return gnome_appimage::tools::has_error_diagnostic(o_all) ? EXIT_ERROR : EXIT_OK;
}

int list_entries(const std::string &s_locale, bool b_json) {
    using gnome_appimage::desktop::desktop_entry_candidate_o;
    using gnome_appimage::desktop::desktop_entry_file_o;
    using gnome_appimage::desktop::desktop_entry_locator_c;
    using gnome_appimage::desktop::desktop_entry_reader_c;

    const desktop_entry_locator_c o_locator;
    const std::vector<desktop_entry_candidate_o> o_candidates = o_locator.list();
    if (b_json) {
        std::cout << '[';
    }
    bool b_first = true;
    for (const desktop_entry_candidate_o &o_candidate : o_candidates) {
        desktop_entry_file_o o_file;
        std::vector<gnome_appimage::desktop::desktop_entry_diagnostic_o> o_diagnostics;
        const bool b_parsed = desktop_entry_reader_c::parse_file(o_candidate.path, o_file,
                                                                 o_diagnostics);
        const std::optional<std::string> s_name =
            b_parsed ? o_file.value("Desktop Entry", "Name", s_locale) : std::nullopt;
        if (b_json) {
            if (!b_first) {
                std::cout << ',';
            }
            std::cout << "{\"id\":\"" << gnome_appimage::tools::json_escape(o_candidate.id)
                      << "\",\"path\":\"" << gnome_appimage::tools::json_escape(o_candidate.path)
                      << "\",\"name\":\""
                      << gnome_appimage::tools::json_escape(s_name.value_or("")) << "\"}";
        } else {
            std::cout << o_candidate.id << '\t' << s_name.value_or("") << '\t'
                      << o_candidate.path << '\n';
        }
        b_first = false;
    }
    if (b_json) {
        std::cout << "]\n";
    }
    return EXIT_OK;
}

int locate_entry(const std::string &s_id, bool b_json) {
    using gnome_appimage::desktop::desktop_entry_locator_c;
    const desktop_entry_locator_c o_locator;
    const std::optional<gnome_appimage::desktop::desktop_entry_candidate_o> o_candidate =
        o_locator.locate(s_id);
    if (!o_candidate.has_value()) {
        std::cerr << "error: no such desktop entry: " << s_id << '\n';
        return EXIT_ERROR;
    }
    if (b_json) {
        std::cout << "{\"id\":\"" << gnome_appimage::tools::json_escape(o_candidate->id)
                  << "\",\"path\":\"" << gnome_appimage::tools::json_escape(o_candidate->path)
                  << "\"}\n";
    } else {
        std::cout << o_candidate->path << '\n';
    }
    return EXIT_OK;
}

void print_directories(const std::vector<std::string> &o_directories) {
    for (const std::string &s_directory : o_directories) {
        std::cout << s_directory << '\n';
    }
}

}  // namespace

int main(int i_argument_count, char **p_arguments) {
    std::string s_path;
    std::string s_locate;
    std::string s_locale;
    bool b_all = false;
    bool b_path = false;
    bool b_autostart_path = false;
    bool b_json = false;

    for (int i_index = 1; i_index < i_argument_count; i_index++) {
        const std::string s_argument = p_arguments[i_index];
        if ("--all" == s_argument) {
            b_all = true;
        } else if ("--path" == s_argument) {
            b_path = true;
        } else if ("--autostart-path" == s_argument) {
            b_autostart_path = true;
        } else if ("--json" == s_argument) {
            b_json = true;
        } else if ("--help" == s_argument) {
            print_usage(std::cout);
            return EXIT_OK;
        } else if ("--version" == s_argument) {
            std::cout << "desktop-inspect " << gnome_appimage::version::version_string()
                      << " (build " << gnome_appimage::version::version_build_counter() << ")\n";
            return EXIT_OK;
        } else if ("--locale" == s_argument) {
            if (i_argument_count <= i_index + 1) {
                std::cerr << "error: --locale needs a value\n";
                return EXIT_USAGE;
            }
            s_locale = p_arguments[++i_index];
        } else if ("--locate" == s_argument) {
            if (i_argument_count <= i_index + 1) {
                std::cerr << "error: --locate needs a value\n";
                return EXIT_USAGE;
            }
            s_locate = p_arguments[++i_index];
        } else if (!s_argument.empty() && '-' == s_argument[0]) {
            std::cerr << "error: unknown option: " << s_argument << '\n';
            return EXIT_USAGE;
        } else if (s_path.empty()) {
            s_path = s_argument;
        } else {
            std::cerr << "error: only one path may be given\n";
            return EXIT_USAGE;
        }
    }

    if (b_path || b_autostart_path) {
        const gnome_appimage::desktop::desktop_entry_locator_c o_locator;
        print_directories(b_path ? o_locator.application_directories()
                                 : o_locator.autostart_directories());
        return EXIT_OK;
    }
    if (b_all) {
        return list_entries(s_locale, b_json);
    }
    if (!s_locate.empty()) {
        return locate_entry(s_locate, b_json);
    }
    if (!s_path.empty()) {
        return inspect_path(s_path, s_locale, b_json);
    }

    print_usage(std::cerr);
    return EXIT_USAGE;
}
