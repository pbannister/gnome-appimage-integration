// desktop-inspect: read desktop entry files and locate them on the XDG search path.
#include "desktop/desktop_entry_locator.h"
#include "desktop/desktop_entry_reader.h"
#include "desktop/icon_theme_locator.h"
#include "desktop/mime_association_reader.h"
#include "tools/desktop_entry_output.h"
#include "version/version.h"

#include <filesystem>
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
          << "  --explain <id>     show which file wins an identifier, and what it masks\n"
          << "  --icon <name>      show every file an icon name resolves to\n"
          << "  --theme <name>     resolve the icon in this theme first\n"
          << "  --mime <type>      show which application opens a MIME type, and from where\n"
          << "  --why              also print the directories and files that were searched\n"
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

int show_icon(const std::string &s_name, const std::string &s_theme, bool b_why) {
    using gnome_appimage::desktop::icon_candidate_o;
    using gnome_appimage::desktop::icon_lookup_o;
    using gnome_appimage::desktop::icon_theme_locator_c;

    const icon_theme_locator_c o_locator;
    const icon_lookup_o o_result = o_locator.lookup(s_name, s_theme);
    if (!o_result.found) {
        std::cerr << "error: no icon named " << s_name << " was found\n";
    } else {
        std::cout << "icon: " << s_name << '\n'
                  << "best: " << o_result.best_path << '\n'
                  << "candidates:\n";
        for (const icon_candidate_o &o_candidate : o_result.candidates) {
            std::cout << "  " << (o_candidate.theme.empty() ? "-" : o_candidate.theme) << " "
                      << (o_candidate.size.empty() ? "-" : o_candidate.size) << " "
                      << o_candidate.path << '\n';
        }
    }
    if (b_why) {
        std::cout << "themes searched:\n";
        for (const std::string &s_theme : o_result.searched_themes) {
            std::cout << "  " << s_theme << '\n';
        }
        std::cout << "directories searched:\n";
        for (const std::string &s_directory : o_result.searched_directories) {
            std::cout << "  " << s_directory << '\n';
        }
    }
    return o_result.found ? EXIT_OK : EXIT_ERROR;
}

int show_mime(const std::string &s_type, bool b_why) {
    using gnome_appimage::desktop::mime_association_o;
    using gnome_appimage::desktop::mime_association_reader_c;
    using gnome_appimage::desktop::mime_lookup_o;

    const mime_association_reader_c o_reader;
    const mime_lookup_o o_result = o_reader.lookup(s_type);
    std::cout << "mime: " << s_type << '\n';
    if (o_result.found) {
        std::cout << "default: " << o_result.default_application.desktop_id << '\n'
                  << "path: " << o_result.default_application.desktop_path << '\n'
                  << "source: " << o_result.default_application.source_file << '\n'
                  << "group: " << o_result.default_application.group << '\n';
    } else {
        std::cout << "default: (none)\n";
    }
    if (!o_result.associations.empty()) {
        std::cout << "associations:\n";
        for (const mime_association_o &o_association : o_result.associations) {
            std::cout << "  " << o_association.desktop_id << "  " << o_association.source_file
                      << '\n';
        }
    }
    if (b_why) {
        std::cout << "files searched:\n";
        for (const std::string &s_file : o_result.searched_files) {
            std::cout << "  " << s_file << '\n';
        }
    }
    return o_result.found ? EXIT_OK : EXIT_ERROR;
}

int explain_identifier(const std::string &s_id) {
    using gnome_appimage::desktop::desktop_entry_candidate_o;
    using gnome_appimage::desktop::desktop_entry_locator_c;

    const desktop_entry_locator_c o_locator;
    const std::optional<desktop_entry_candidate_o> o_winner = o_locator.locate(s_id);
    if (!o_winner.has_value()) {
        std::cerr << "error: no desktop entry has the identifier " << s_id << '\n';
        return EXIT_ERROR;
    }
    std::cout << "identifier: " << s_id << '\n'
              << "winner: " << o_winner->path << '\n'
              << "priority: " << o_winner->priority << '\n'
              << "data-directory: " << o_winner->data_directory << '\n'
              << "relative-path: " << o_winner->relative_path << '\n';
    std::cout << "masked copies with the same identifier:\n";
    for (const std::string &s_directory : o_locator.application_directories()) {
        const std::filesystem::path o_candidate =
            std::filesystem::path(s_directory) / o_winner->relative_path;
        std::error_code o_error;
        if (std::filesystem::exists(o_candidate, o_error)
            && o_candidate.string() != o_winner->path) {
            std::cout << "  " << o_candidate.string() << '\n';
        }
    }
    return inspect_path(o_winner->path, std::string(), false);
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
    std::string s_explain;
    std::string s_icon;
    std::string s_mime;
    std::string s_locale;
    std::string s_theme;
    bool b_all = false;
    bool b_path = false;
    bool b_autostart_path = false;
    bool b_json = false;
    bool b_why = false;

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
        } else if ("--explain" == s_argument) {
            if (i_argument_count <= i_index + 1) {
                std::cerr << "error: --explain needs a value\n";
                return EXIT_USAGE;
            }
            s_explain = p_arguments[++i_index];
        } else if ("--icon" == s_argument) {
            if (i_argument_count <= i_index + 1) {
                std::cerr << "error: --icon needs a value\n";
                return EXIT_USAGE;
            }
            s_icon = p_arguments[++i_index];
        } else if ("--mime" == s_argument) {
            if (i_argument_count <= i_index + 1) {
                std::cerr << "error: --mime needs a value\n";
                return EXIT_USAGE;
            }
            s_mime = p_arguments[++i_index];
        } else if ("--why" == s_argument) {
            b_why = true;
        } else if ("--theme" == s_argument) {
            if (i_argument_count <= i_index + 1) {
                std::cerr << "error: --theme needs a value\n";
                return EXIT_USAGE;
            }
            s_theme = p_arguments[++i_index];
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

    if (!s_icon.empty()) {
        return show_icon(s_icon, s_theme, b_why);
    }
    if (!s_mime.empty()) {
        return show_mime(s_mime, b_why);
    }
    if (!s_explain.empty()) {
        return explain_identifier(s_explain);
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
