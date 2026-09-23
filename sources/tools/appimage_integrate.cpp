// appimage-integrate: plan, install, uninstall, audit, and run AppImages,
// and manage the *.AppImage double-click handler.
#include "appimage/appimage_reader.h"
#include "appimage/appimage_update_information.h"
#include "appimage/squashfs_reader.h"
#include "desktop/desktop_entry_locator.h"
#include "desktop/desktop_entry_reader.h"
#include "integration/appimage_integrator.h"
#include "json/json_reader.h"
#include "tools/desktop_entry_output.h"
#include "version/version.h"
#include "version/version_compare.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <sstream>
#include <string>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>
#include <vector>

namespace {

namespace fs = std::filesystem;

using gnome_appimage::integration::appimage_integrator_c;
using gnome_appimage::integration::audit_finding_o;
using gnome_appimage::integration::installed_appimage_o;
using gnome_appimage::integration::integration_action_o;
using gnome_appimage::integration::integration_conflict_o;
using gnome_appimage::integration::integration_conflict_policy_e;
using gnome_appimage::integration::integration_icon_o;
using gnome_appimage::appimage::understand_update_information;
using gnome_appimage::integration::integration_options_o;
using gnome_appimage::integration::integration_plan_o;

constexpr int EXIT_OK = 0;
constexpr int EXIT_ERROR = 1;
constexpr int EXIT_USAGE = 2;
// The handler is the right-click "Open With" item, so its name is user-visible:
// "AppImage Activator". "AppImage Handler" is another project's name.
constexpr const char *HANDLER_NAME = "AppImage Activator";
constexpr const char *HANDLER_DESKTOP_ID = "appimage-activator.desktop";
constexpr const char *HANDLER_MANIFEST = "appimage-activator.manifest";
constexpr const char *HANDLER_ICON_NAME = "appimage-activator";
// Names this handler used before it was renamed; install migrates them away.
constexpr const char *HANDLER_LEGACY_NAME = "AppImage Handler";
constexpr const char *HANDLER_LEGACY_DESKTOP_ID = "appimage-handler.desktop";
constexpr const char *HANDLER_LEGACY_MANIFEST = "appimage-handler.manifest";
constexpr const char *HANDLER_LEGACY_ICON_NAME = "appimage-handler";
// The length of ".AppImage", used to recognise an AppImage path in a command line.
constexpr std::size_t APPIMAGE_SUFFIX_LENGTH = 9;

void print_usage(std::ostream &o_out) {
    o_out << "usage: appimage-integrate <command> [options]\n"
          << "\n"
          << "commands:\n"
          << "  explain <AppImage>        show what this AppImage is and what install would write\n"
          << "  plan <AppImage>           print the install plan; change nothing\n"
          << "  install <AppImage>        integrate the AppImage into the desktop\n"
          << "  uninstall --identifier ID [--remove-appimage]\n"
          << "  list                      list AppImages integrated by this tool\n"
          << "  refresh                   rewrite every recorded launcher from its embedded entry\n"
          << "  update --check            ask the update information whether a newer build exists\n"
          << "  windows                   list running AppImages and their window classes\n"
          << "  run <AppImage> [args...]  run once, without integrating\n"
          << "  audit [--check]           report desktop integration inconsistencies\n"
          << "  handler status            show the current *.AppImage handler\n"
          << "  handler install           make this tool the *.AppImage handler\n"
          << "  handler uninstall         restore the previous *.AppImage handler\n"
          << "  handle <AppImage>         the handler entry point (double-click)\n"
          << "\n"
          << "options:\n"
          << "  --install-dir DIR         where the AppImage is placed (default ~/Applications)\n"
          << "  --desktop-file-name NAME  override the desktop file name (the ID)\n"
          << "  --exec-args ARGUMENTS     extra arguments inserted into Exec\n"
          << "  --wm-class CLASS          set StartupWMClass (refresh: for every launcher)\n"
          << "  --wm-class-from-window    read StartupWMClass from the running application\n"
          << "  --icon-name NAME          override the installed icon name\n"
          << "  --name NAME               set Name= in the launcher, e.g. with a version\n"
          << "  --ignore-signature        integrate although .sha256_sig does not match\n"
          << "  --no-move                 copy instead of move\n"
          << "  --no-icons                do not install icons\n"
          << "  --replace                 replace an existing launcher for this application\n"
          << "  --add                     install alongside an existing launcher\n"
          << "  --extract-and-run         for run: force APPIMAGE_EXTRACT_AND_RUN=1\n"
          << "  --detached                for run: start in a new session and report the pid\n"
          << "  --json                    machine-readable output where supported\n"
          << "  --yes                     do not ask before writing\n"
          << "  --dry-run                 for refresh: show what would be rewritten\n"
          << "  --check                   for update and audit: ask the transport\n"
          << "  --all                     for update: every AppImage this tool integrated\n"
          << "  --notify                  for update: show the result in a desktop notification\n"
          << "  --version                 print the build-time version\n";
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
        if (!s_directory.empty()
            && 0 == access((s_directory + "/" + s_command).c_str(), X_OK)) {
            return true;
        }
        if (std::string::npos == i_end) {
            break;
        }
        i_start = i_end + 1;
    }
    return false;
}

// Run a command for its exit status only, with its output discarded.
// Locate the graphical activator next to the tool, or in the build tree. The
// pre-conversion names are still accepted, so an older install keeps working.
std::string handler_ui_program(const std::string &s_tool) {
    const fs::path o_tool(s_tool);
    const fs::path o_repository = o_tool.parent_path().parent_path().parent_path();
    const std::vector<fs::path> o_candidates = {
        o_tool.parent_path() / "appimage-activator",
        o_tool.parent_path().parent_path() / "appimage-activator",
        o_repository / "dataflow.out/build/appimage-activator",
        // The earlier Python dialog, if it is the only one installed.
        o_tool.parent_path() / "appimage_activator_ui.py",
        o_tool.parent_path() / "appimage_handler_ui.py",
    };
    for (const fs::path &o_candidate : o_candidates) {
        std::error_code o_error;
        if (fs::exists(o_candidate, o_error)) {
            return o_candidate.string();
        }
    }
    return {};
}

std::string capture_command(const std::vector<std::string> &o_arguments) {
    if (o_arguments.empty()) {
        return {};
    }
    std::string s_command;
    for (const std::string &s_argument : o_arguments) {
        if (!s_command.empty()) {
            s_command += " ";
        }
        s_command += "'" + s_argument + "'";
    }
    s_command += " 2>/dev/null";
    std::string s_result;
    FILE *p_pipe = popen(s_command.c_str(), "r");
    if (nullptr == p_pipe) {
        return {};
    }
    char s_buffer[512];
    while (nullptr != std::fgets(s_buffer, sizeof(s_buffer), p_pipe)) {
        s_result += s_buffer;
    }
    pclose(p_pipe);
    while (!s_result.empty() && ('\n' == s_result.back() || '\r' == s_result.back())) {
        s_result.pop_back();
    }
    return s_result;
}

// Refresh the icon theme cache after an icon is installed or removed; GTK keeps
// using a stale cache and then never sees the new icon.
void refresh_icon_cache(const std::string &s_theme_directory) {
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
    const std::string s_result = capture_command({s_tool, "-f", "-t", s_theme_directory});
    static_cast<void>(s_result);
}

std::string tool_path(const appimage_integrator_c &o_integrator) {
    const std::string s_installed = fs::path(std::getenv("HOME") ? std::getenv("HOME") : "")
                                        .append(".local/bin/appimage-integrate")
                                        .string();
    if (fs::exists(s_installed)) {
        return s_installed;
    }
    std::error_code o_error;
    const fs::path o_self = fs::read_symlink("/proc/self/exe", o_error);
    if (!o_error) {
        return o_self.string();
    }
    static_cast<void>(o_integrator);
    return "appimage-integrate";
}

std::string embedded_name(const std::string &s_appimage_path) {
    using gnome_appimage::appimage::appimage_detection_e;
    using gnome_appimage::appimage::appimage_info_o;
    using gnome_appimage::appimage::appimage_reader_c;
    using gnome_appimage::appimage::squashfs_reader_c;
    using gnome_appimage::desktop::desktop_entry_file_o;
    using gnome_appimage::desktop::desktop_entry_reader_c;

    appimage_info_o o_info;
    if (!appimage_reader_c::read(s_appimage_path, o_info)
        || appimage_detection_e::type2 != o_info.detection || !o_info.has_squashfs) {
        return {};
    }
    squashfs_reader_c o_reader;
    std::string s_error;
    if (!o_reader.open(s_appimage_path, o_info.payload_offset, s_error)) {
        return {};
    }
    std::vector<gnome_appimage::appimage::squashfs_entry_o> o_desktop_entries;
    if (!o_reader.list_root_files_with_extension(".desktop", o_desktop_entries, s_error)
        || o_desktop_entries.empty()) {
        return {};
    }
    std::string s_content;
    if (!o_reader.read_file(o_desktop_entries[0].path, s_content, s_error)) {
        return {};
    }
    desktop_entry_file_o o_entry;
    std::vector<gnome_appimage::desktop::desktop_entry_diagnostic_o> o_diagnostics;
    desktop_entry_reader_c::parse_text(s_content, o_entry, o_diagnostics);
    return o_entry.value("Desktop Entry", "Name").value_or(std::string());
}

void print_plan_text(const integration_plan_o &o_plan) {
    std::cout << "this-run: " << (o_plan.mode.empty() ? "(unknown)" : o_plan.mode) << '\n'
              << "appimage: " << o_plan.appimage_path << '\n'
              << "installed: " << o_plan.installed_path << '\n'
              << "identifier: " << o_plan.identifier << '\n'
              << "desktop-id: " << o_plan.desktop_id << '\n'
              << "desktop-entry: " << o_plan.desktop_entry_path << '\n'
              << "manifest: " << o_plan.manifest_path << '\n'
              << "icon-name: " << o_plan.icon_name << '\n'
              << "exec: " << o_plan.exec_command << '\n'
              << "field-code: " << o_plan.field_code << '\n'
              << "embedded-desktop: " << o_plan.embedded_desktop_path << '\n'
              << "startup-wm-class: "
              << (o_plan.startup_wm_class.empty() ? "(none)" : o_plan.startup_wm_class)
              << (o_plan.startup_wm_class_is_explicit ? "  (set explicitly, kept over the embedded entry)" : "")
              << '\n';
    std::cout << "signature: " << (o_plan.signature.empty() ? "(absent)" : o_plan.signature)
              << '\n';
    std::cout << "mime-types:";
    for (const std::string &s_type : o_plan.mime_types) {
        std::cout << ' ' << s_type;
    }
    std::cout << '\n';
    if (!o_plan.icons.empty()) {
        std::cout << "icons:\n";
        for (const integration_icon_o &o_icon : o_plan.icons) {
            std::cout << "  " << o_icon.size_directory << " " << o_icon.extension << "  "
                      << o_icon.source_in_payload << " -> " << o_icon.installed_name
                      << o_icon.extension << '\n';
        }
    }
    if (!o_plan.mime_package_files.empty()) {
        std::cout << "mime-packages:\n";
        for (const auto &o_package : o_plan.mime_package_files) {
            std::cout << "  " << o_package.source_in_payload << " -> "
                      << o_package.installed_name << '\n';
        }
    }
    for (const std::string &s_warning : o_plan.warnings) {
        std::cout << "warning: " << s_warning << '\n';
    }
    for (const std::string &s_note : o_plan.notes) {
        std::cout << "note: " << s_note << '\n';
    }
    std::cout << "actions:\n";
    for (const auto &o_action : o_plan.actions) {
        std::cout << "  " << o_action.description << ": "
                  << (o_action.target_path.empty() ? o_action.source_path : o_action.target_path)
                  << '\n';
    }
}

void print_plan_json(const integration_plan_o &o_plan) {
    using gnome_appimage::tools::json_escape;
    std::cout << "{\"appimage\":\"" << json_escape(o_plan.appimage_path) << "\",\"installed\":\""
              << json_escape(o_plan.installed_path) << "\",\"mode\":\""
              << json_escape(o_plan.mode) << "\",\"identifier\":\""
              << json_escape(o_plan.identifier) << "\",\"desktop_id\":\""
              << json_escape(o_plan.desktop_id) << "\",\"desktop_entry\":\""
              << json_escape(o_plan.desktop_entry_path) << "\",\"icon_name\":\""
              << json_escape(o_plan.icon_name) << "\",\"exec\":\""
              << json_escape(o_plan.exec_command) << "\",\"startup_wm_class\":\""
              << json_escape(o_plan.startup_wm_class) << "\",\"startup_wm_class_source\":\""
              << (o_plan.startup_wm_class_is_explicit ? "explicit" : "embedded")
              << "\",\"icons\":[";
    bool b_first = true;
    for (const integration_icon_o &o_icon : o_plan.icons) {
        if (!b_first) {
            std::cout << ',';
        }
        b_first = false;
        std::cout << "{\"size\":\"" << json_escape(o_icon.size_directory) << "\",\"extension\":\""
                  << json_escape(o_icon.extension) << "\",\"source\":\""
                  << json_escape(o_icon.source_in_payload) << "\"}";
    }
    std::cout << "],\"warnings\":[";
    b_first = true;
    for (const std::string &s_warning : o_plan.warnings) {
        if (!b_first) {
            std::cout << ',';
        }
        b_first = false;
        std::cout << '"' << json_escape(s_warning) << '"';
    }
    std::cout << "]}\n";
}

integration_options_o options_from(const std::string &s_install_dir,
                                  const std::string &s_desktop_file_name,
                                  const std::string &s_exec_args,
                                  const std::string &s_wm_class,
                                  const std::string &s_icon_name,
                                  const std::string &s_name,
                                  bool b_ignore_signature,
                                  bool b_move,
                                  bool b_icons,
                                  integration_conflict_policy_e e_policy) {
    integration_options_o o_options;
    o_options.install_directory = s_install_dir;
    o_options.desktop_file_name = s_desktop_file_name;
    o_options.extra_exec_arguments = s_exec_args;
    o_options.startup_wm_class_override = s_wm_class;
    o_options.icon_name_override = s_icon_name;
    o_options.name_override = s_name;
    o_options.ignore_signature = b_ignore_signature;
    o_options.move_appimage = b_move;
    o_options.write_icons = b_icons;
    o_options.conflict_policy = e_policy;
    return o_options;
}

int command_plan_or_explain(const std::string &s_path,
                            const integration_options_o &o_options,
                            bool b_json) {
    const appimage_integrator_c o_integrator;
    integration_options_o o_effective = o_options;
    if (o_effective.tool_path.empty()) {
        o_effective.tool_path = tool_path(o_integrator);
    }
    integration_plan_o o_plan;
    if (!o_integrator.plan(s_path, o_effective, o_plan)) {
        std::cerr << "error: " << o_plan.error << '\n';
        return EXIT_ERROR;
    }
    if (b_json) {
        print_plan_json(o_plan);
    } else {
        print_plan_text(o_plan);
        std::cout << "desktop-entry-contents:\n" << o_plan.desktop_entry_text;
    }
    return EXIT_OK;
}

// Machine-readable description, used by the graphical handler.
int command_explain_json(const std::string &s_path, const integration_options_o &o_options) {
    using gnome_appimage::appimage::update_information_o;
    using gnome_appimage::tools::json_escape;
    const appimage_integrator_c o_integrator;
    integration_plan_o o_plan;
    const bool b_valid = o_integrator.plan(s_path, o_options, o_plan);
    const update_information_o o_update =
        understand_update_information(o_plan.update_information);
    std::cout << "{\"path\":\"" << json_escape(s_path) << "\""
              << ",\"name\":\"" << json_escape(o_plan.name) << "\""
              << ",\"generic_name\":\"" << json_escape(o_plan.generic_name) << "\""
              << ",\"comment\":\"" << json_escape(o_plan.comment) << "\""
              << ",\"version\":\"" << json_escape(o_plan.version) << "\""
              << ",\"version_source\":\"" << json_escape(o_plan.version_source) << "\""
              << ",\"installed_version\":\"" << json_escape(o_plan.installed_version) << "\""
              << ",\"version_relation\":\"" << json_escape(o_plan.version_relation) << "\""
              << ",\"detection\":\"" << json_escape(o_plan.detection_name) << "\""
              << ",\"file_size\":" << o_plan.file_size
              << ",\"payload_size\":" << o_plan.payload_size
              << ",\"payload_offset\":" << o_plan.payload_offset
              << ",\"signature\":\"" << json_escape(o_plan.signature) << "\""
              << ",\"signature_stored\":\"" << json_escape(o_plan.signature_stored) << "\""
              << ",\"signature_computed\":\"" << json_escape(o_plan.signature_computed) << "\""
              << ",\"signature_mismatch\":" << (o_plan.signature_mismatch ? "true" : "false")
              << ",\"compression\":\"" << json_escape(o_plan.compression_name) << "\""
              << ",\"update_information\":\"" << json_escape(o_plan.update_information) << "\""
              << ",\"update_usable\":" << (o_update.usable ? "true" : "false")
              << ",\"update_description\":\"" << json_escape(o_update.description)
              << "\",\"update_problem\":\"" << json_escape(o_update.problem) << "\""
              << ",\"identifier\":\"" << json_escape(o_plan.identifier) << "\""
              << ",\"desktop_id\":\"" << json_escape(o_plan.desktop_id) << "\""
              << ",\"installed\":\"" << json_escape(o_plan.installed_path) << "\""
              << ",\"icon_name\":\"" << json_escape(o_plan.icon_name) << "\""
              << ",\"exec\":\"" << json_escape(o_plan.exec_command) << "\""
              << ",\"startup_wm_class\":\"" << json_escape(o_plan.startup_wm_class) << "\""
              << ",\"embedded_desktop\":\"" << json_escape(o_plan.embedded_desktop_path) << "\""
              << ",\"desktop_entry\":\"" << json_escape(o_plan.desktop_entry_text) << "\""
              << ",\"mode\":\"" << json_escape(o_plan.mode) << "\""
              << ",\"valid\":" << (b_valid ? "true" : "false")
              << ",\"error\":\"" << json_escape(o_plan.error) << "\""
              << ",\"conflicts\":[";
    bool b_first = true;
    for (const integration_conflict_o &o_conflict : o_plan.conflicts) {
        if (!b_first) {
            std::cout << ',';
        }
        b_first = false;
        std::cout << "{\"desktop_id\":\"" << json_escape(o_conflict.desktop_id)
                  << "\",\"path\":\"" << json_escape(o_conflict.path)
                  << "\",\"name\":\"" << json_escape(o_conflict.name)
                  << "\",\"appimage\":\"" << json_escape(o_conflict.appimage_path)
                  << "\",\"icon\":\"" << json_escape(o_conflict.icon)
                  << "\",\"wm_class\":\"" << json_escape(o_conflict.wm_class)
                  << "\",\"version\":\"" << json_escape(o_conflict.version)
                  << "\",\"origin\":\"" << json_escape(o_conflict.origin)
                  << "\",\"managed\":" << (o_conflict.managed ? "true" : "false")
                  << ",\"upgrade\":" << (o_conflict.upgrade ? "true" : "false")
                  << ",\"repair\":" << (o_conflict.repair ? "true" : "false")
                  << ",\"exec_exists\":" << (o_conflict.exec_exists ? "true" : "false") << '}';
    }
    std::cout << "]}\n";
    return b_valid ? EXIT_OK : EXIT_ERROR;
}

int command_explain(const std::string &s_path, const integration_options_o &o_options) {
    const appimage_integrator_c o_integrator;
    std::cout << o_integrator.describe(s_path, o_options);
    return EXIT_OK;
}

int command_install(const std::string &s_path,
                    const integration_options_o &o_options,
                    bool b_assume_yes) {
    const appimage_integrator_c o_integrator;
    integration_options_o o_effective = o_options;
    if (o_effective.tool_path.empty()) {
        o_effective.tool_path = tool_path(o_integrator);
    }
    integration_plan_o o_plan;
    if (!o_integrator.plan(s_path, o_effective, o_plan)) {
        std::cerr << "error: " << o_plan.error << '\n';
        return EXIT_ERROR;
    }
    print_plan_text(o_plan);
    std::cout << "desktop-entry-contents:\n" << o_plan.desktop_entry_text;

    if (!b_assume_yes) {
        if (0 == isatty(STDIN_FILENO)) {
            std::cerr << "error: refusing to write without --yes on a non-interactive input\n";
            return EXIT_ERROR;
        }
        std::cout << "Proceed with these changes? [y/N] " << std::flush;
        std::string s_answer;
        std::getline(std::cin, s_answer);
        if ("y" != s_answer && "Y" != s_answer) {
            std::cout << "cancelled\n";
            return EXIT_OK;
        }
    }

    std::string s_error;
    if (!o_integrator.install(o_plan, s_error)) {
        std::cerr << "error: " << s_error << '\n';
        return EXIT_ERROR;
    }
    std::cout << "installed: " << o_plan.desktop_entry_path << '\n'
              << "appimage: " << o_plan.installed_path << '\n'
              << "identifier: " << o_plan.identifier << '\n';
    return EXIT_OK;
}

int command_uninstall(const std::string &s_identifier, bool b_remove_appimage) {
    const appimage_integrator_c o_integrator;
    std::string s_error;
    if (!o_integrator.uninstall(s_identifier, b_remove_appimage, s_error)) {
        std::cerr << "error: " << s_error << '\n';
        return EXIT_ERROR;
    }
    std::cout << "uninstalled: " << s_identifier << '\n';
    return EXIT_OK;
}

int command_list(bool b_json) {
    const appimage_integrator_c o_integrator;
    const std::vector<installed_appimage_o> o_installed = o_integrator.list_installed();
    if (b_json) {
        std::cout << '[';
    }
    bool b_first = true;
    for (const installed_appimage_o &o_entry : o_installed) {
        std::error_code o_exists_error;
        const bool b_missing = !fs::exists(o_entry.appimage_path, o_exists_error);
        if (b_json) {
            if (!b_first) {
                std::cout << ',';
            }
            std::cout << "{\"identifier\":\""
                      << gnome_appimage::tools::json_escape(o_entry.identifier)
                      << "\",\"appimage\":\""
                      << gnome_appimage::tools::json_escape(o_entry.appimage_path)
                      << "\",\"desktop_entry\":\""
                      << gnome_appimage::tools::json_escape(o_entry.desktop_entry_path)
                      << "\",\"missing\":" << (b_missing ? "true" : "false") << "}";
        } else {
            std::cout << o_entry.identifier << '\t' << o_entry.appimage_path << '\t'
                      << o_entry.desktop_entry_path
                      << (b_missing ? "\t[MISSING: the launcher points at a file that is not "
                                      "there]"
                                    : "")
                      << '\n';
        }
        b_first = false;
    }
    if (b_json) {
        std::cout << "]\n";
    }
    return EXIT_OK;
}

// has_display() and the JSON writer are defined further down the file.
bool has_display();

std::string trim_spaces(const std::string &s_text) {
    const std::size_t i_first = s_text.find_first_not_of(" \t\r\n");
    if (std::string::npos == i_first) {
        return {};
    }
    return s_text.substr(i_first, s_text.find_last_not_of(" \t\r\n") - i_first + 1);
}

// -- running windows and AppImage processes ------------------------------------
//
// GNOME does not let other programs enumerate windows: the shell owns the window
// list and its Introspect.GetWindows method answers "GetWindows is not allowed"
// (GNOME Shell 46, js/dbusServices/shellIntrospect.js).  Two sources remain:
//
//   * X11 and XWayland clients, which xlsclients and xprop can see, with the
//     WM_CLASS the dock matches on;
//   * the processes themselves, which name the app an AppImage runs.  GTK sets a
//     Wayland window's application id from the GtkApplication id when there is one
//     and from the program name otherwise, so the executable's basename is the id
//     an AppImage whose app does not use GtkApplication will report.

// One running AppImage, with whatever the window system says about it.
struct running_appimage_o {
    std::string appimage;
    int i_pid = 0;
    std::string process;
    std::string executable;
    std::string window_id;
    std::string window_instance;
    std::string window_class;
    std::string title;
};

std::string read_proc_text(const std::string &s_path) {
    std::ifstream o_input(s_path);
    if (!o_input) {
        return {};
    }
    std::ostringstream o_buffer;
    o_buffer << o_input.rdbuf();
    return o_buffer.str();
}

std::vector<std::string> read_command_line(int i_pid) {
    std::vector<std::string> o_arguments;
    const std::string s_raw = read_proc_text("/proc/" + std::to_string(i_pid) + "/cmdline");
    std::string s_current;
    for (const char c_character : s_raw) {
        if ('\0' == c_character) {
            if (!s_current.empty()) {
                o_arguments.push_back(s_current);
                s_current.clear();
            }
            continue;
        }
        s_current += c_character;
    }
    if (!s_current.empty()) {
        o_arguments.push_back(s_current);
    }
    return o_arguments;
}

std::string process_executable(int i_pid) {
    std::error_code o_error;
    const fs::path o_target =
        fs::read_symlink("/proc/" + std::to_string(i_pid) + "/exe", o_error);
    return o_error ? std::string() : o_target.string();
}

int process_parent(int i_pid) {
    // /proc/<pid>/stat: pid (comm) state ppid ..., and comm may contain spaces.
    const std::string s_stat = read_proc_text("/proc/" + std::to_string(i_pid) + "/stat");
    const std::size_t i_close = s_stat.rfind(')');
    if (std::string::npos == i_close || i_close + 3 > s_stat.size()) {
        return 0;
    }
    std::istringstream o_fields(s_stat.substr(i_close + 1));
    std::string s_state;
    int i_parent = 0;
    o_fields >> s_state >> i_parent;
    return i_parent;
}

// An executable path inside a mounted or extracted AppImage.
bool is_appimage_internal_path(const std::string &s_path) {
    return std::string::npos != s_path.find("/.mount_")
           || std::string::npos != s_path.find("/appimage_extracted_");
}

// One KEY=value entry from a process environment, which /proc stores NUL-separated.
std::string process_environment_value(int i_pid, const std::string &s_key) {
    const std::string s_environment =
        read_proc_text("/proc/" + std::to_string(i_pid) + "/environ");
    const std::string s_prefix = s_key + "=";
    std::size_t i_start = 0;
    while (i_start < s_environment.size()) {
        const std::size_t i_end = s_environment.find('\0', i_start);
        const std::string s_entry = s_environment.substr(
            i_start, std::string::npos == i_end ? std::string::npos : i_end - i_start);
        if (0 == s_entry.compare(0, s_prefix.size(), s_prefix)) {
            return s_entry.substr(s_prefix.size());
        }
        if (std::string::npos == i_end) {
            break;
        }
        i_start = i_end + 1;
    }
    return {};
}

bool looks_like_appimage_path(const std::string &s_text) {
    return s_text.size() > APPIMAGE_SUFFIX_LENGTH
           && 0 == s_text.compare(s_text.size() - APPIMAGE_SUFFIX_LENGTH,
                                 APPIMAGE_SUFFIX_LENGTH, ".AppImage");
}

// The AppImage a process belongs to.  The AppImage runtime puts the file in the
// APPIMAGE environment variable of everything it starts, and that survives the
// runtime process exiting, which is what happens in practice: the payload is
// reparented and no ancestor names the file any more.  The command line is checked
// first because it also covers the runtime process itself and a program started
// with an AppImage path as its argv[0]; the ancestry is the last resort.
std::string appimage_of_process(int i_pid) {
    for (const std::string &s_argument : read_command_line(i_pid)) {
        if (looks_like_appimage_path(s_argument)) {
            return s_argument;
        }
    }
    const std::string s_from_environment = process_environment_value(i_pid, "APPIMAGE");
    if (!s_from_environment.empty()) {
        return s_from_environment;
    }
    int i_current = process_parent(i_pid);
    for (int i_depth = 0; i_depth < 8 && 0 < i_current; i_depth++) {
        for (const std::string &s_argument : read_command_line(i_current)) {
            if (looks_like_appimage_path(s_argument)) {
                return s_argument;
            }
        }
        const std::string s_parent_environment =
            process_environment_value(i_current, "APPIMAGE");
        if (!s_parent_environment.empty()) {
            return s_parent_environment;
        }
        const int i_parent = process_parent(i_current);
        if (i_parent == i_current) {
            break;
        }
        i_current = i_parent;
    }
    return {};
}

// A window found on the X display, whatever listed it.
struct x11_window_o {
    std::string id;
    std::string title;
    std::string instance;
    std::string window_class;
};

// The pid xprop reports for a window, and the AppImage that process belongs to.
void attach_window(std::vector<running_appimage_o> &o_running, const x11_window_o &o_window) {
    const std::string s_pid_text = capture_command({"xprop", "-id", o_window.id, "_NET_WM_PID"});
    const std::size_t i_digits = s_pid_text.find_last_not_of("0123456789");
    const int i_pid = std::string::npos == i_digits || i_digits + 1 >= s_pid_text.size()
                          ? 0
                          : std::atoi(s_pid_text.substr(i_digits + 1).c_str());
    const std::string s_appimage = 0 < i_pid ? appimage_of_process(i_pid) : std::string();
    if (s_appimage.empty()) {
        return;
    }
    running_appimage_o o_entry;
    o_entry.appimage = s_appimage;
    o_entry.i_pid = i_pid;
    o_entry.executable = process_executable(i_pid);
    o_entry.process = fs::path(o_entry.executable).filename().string();
    o_entry.window_id = o_window.id;
    o_entry.window_instance = o_window.instance;
    o_entry.window_class = o_window.window_class;
    o_entry.title = o_window.title;
    o_running.push_back(std::move(o_entry));
}

// Windows from xlsclients, which is the friendly form: it prints the window, its
// title, and the WM_CLASS the dock matches on.  It only lists windows a window
// manager has marked with WM_STATE, so it finds nothing on a bare X server.
std::size_t attach_xlsclients_windows(std::vector<running_appimage_o> &o_running,
                                      std::size_t i_before) {
    const std::string s_listing = capture_command({"xlsclients", "-l"});
    std::istringstream o_lines(s_listing);
    std::string s_line;
    x11_window_o o_window;
    const auto finish_window = [&]() {
        if (!o_window.id.empty()) {
            attach_window(o_running, o_window);
        }
        o_window = x11_window_o{};
    };
    while (std::getline(o_lines, s_line)) {
        const std::string s_trimmed = trim_spaces(s_line);
        if (0 == s_trimmed.rfind("Window ", 0)) {
            finish_window();
            o_window.id = trim_spaces(s_trimmed.substr(7));
            const std::size_t i_colon = o_window.id.find(':');
            if (std::string::npos != i_colon) {
                o_window.id = o_window.id.substr(0, i_colon);
            }
            continue;
        }
        if (0 == s_trimmed.rfind("Name:", 0)) {
            o_window.title = trim_spaces(s_trimmed.substr(5));
            continue;
        }
        if (0 == s_trimmed.rfind("Instance/Class:", 0)) {
            const std::string s_pair = trim_spaces(s_trimmed.substr(15));
            const std::size_t i_slash = s_pair.find('/');
            o_window.instance = std::string::npos == i_slash ? s_pair : s_pair.substr(0, i_slash);
            o_window.window_class =
                std::string::npos == i_slash ? s_pair : s_pair.substr(i_slash + 1);
            continue;
        }
    }
    finish_window();
    return o_running.size() - i_before;
}

// Windows from xwininfo's tree, for an X server with no window manager: every mapped
// window is listed with its title and its instance/class pair, WM_STATE or not.
std::size_t attach_xwininfo_windows(std::vector<running_appimage_o> &o_running,
                                    std::size_t i_before) {
    const std::string s_listing = capture_command({"xwininfo", "-root", "-tree"});
    std::istringstream o_lines(s_listing);
    std::string s_line;
    while (std::getline(o_lines, s_line)) {
        // 0x20000e "title": ("instance" "class")  1096x823+0+0  +0+0
        const std::string s_entry = trim_spaces(s_line);
        const std::size_t i_space = s_entry.find(' ');
        if (std::string::npos == i_space || 0 != s_entry.compare(0, 2, "0x")) {
            continue;
        }
        const std::size_t i_marker = s_entry.find(": (");
        if (std::string::npos == i_marker) {
            continue;
        }
        const std::size_t i_instance_open = s_entry.find('"', i_marker);
        const std::size_t i_instance_close = std::string::npos == i_instance_open
                                                ? std::string::npos
                                                : s_entry.find('"', i_instance_open + 1);
        const std::size_t i_class_open = std::string::npos == i_instance_close
                                             ? std::string::npos
                                             : s_entry.find('"', i_instance_close + 1);
        const std::size_t i_class_close = std::string::npos == i_class_open
                                              ? std::string::npos
                                              : s_entry.find('"', i_class_open + 1);
        if (std::string::npos == i_class_close) {
            continue;
        }
        x11_window_o o_window;
        o_window.id = s_entry.substr(0, i_space);
        const std::size_t i_title_open = s_entry.find('"');
        const std::size_t i_title_close = s_entry.rfind('"', i_marker);
        if (std::string::npos != i_title_open && i_title_close > i_title_open) {
            o_window.title =
                s_entry.substr(i_title_open + 1, i_title_close - i_title_open - 1);
        }
        o_window.instance =
            s_entry.substr(i_instance_open + 1, i_instance_close - i_instance_open - 1);
        o_window.window_class =
            s_entry.substr(i_class_open + 1, i_class_close - i_class_open - 1);
        if (o_window.window_class.empty()) {
            continue;
        }
        attach_window(o_running, o_window);
    }
    return o_running.size() - i_before;
}

// Every X11 or XWayland window that belongs to a running AppImage.
void attach_x11_windows(std::vector<running_appimage_o> &o_running) {
    if (!command_exists("xprop") || !has_display()) {
        return;
    }
    const std::size_t i_before = o_running.size();
    if (command_exists("xlsclients") && 0 < attach_xlsclients_windows(o_running, i_before)) {
        return;
    }
    // A bare X server, or a window no manager has marked: ask the tree directly.
    // Windows found this way come from the same server, so nothing is duplicated.
    if (command_exists("xwininfo")) {
        attach_xwininfo_windows(o_running, i_before);
    }
}

// The AppImages that are running now, from the processes that belong to them.
std::vector<running_appimage_o> find_running_appimages() {
    std::vector<running_appimage_o> o_running;
    std::error_code o_error;
    for (const fs::directory_entry &o_entry : fs::directory_iterator("/proc", o_error)) {
        const std::string s_name = o_entry.path().filename().string();
        if (s_name.empty() || 0 == s_name.find_first_not_of("0123456789")) {
            continue;
        }
        const int i_pid = std::atoi(s_name.c_str());
        const std::string s_executable = process_executable(i_pid);
        if (!is_appimage_internal_path(s_executable)) {
            continue;
        }
        const std::string s_appimage = appimage_of_process(i_pid);
        if (s_appimage.empty()) {
            continue;
        }
        running_appimage_o o_found;
        o_found.appimage = s_appimage;
        o_found.i_pid = i_pid;
        o_found.executable = s_executable;
        o_found.process = fs::path(s_executable).filename().string();
        o_running.push_back(std::move(o_found));
    }
    // A window is the better evidence, and it also covers AppImages whose processes
    // are not inside a mount (a synthetic AppImage, or a tool started on one).
    attach_x11_windows(o_running);
    // Group by AppImage, and inside a group put the entries that carry a window class
    // first: that is the class the dock matches on, so both the report and
    // window_class_for_appimage() must see it before a bare process name.
    std::sort(o_running.begin(), o_running.end(),
              [](const running_appimage_o &o_left, const running_appimage_o &o_right) {
                  if (o_left.appimage != o_right.appimage) {
                      return o_left.appimage < o_right.appimage;
                  }
                  if (o_left.window_class.empty() != o_right.window_class.empty()) {
                      return !o_left.window_class.empty();
                  }
                  return o_left.i_pid < o_right.i_pid;
              });
    return o_running;
}

// The class the dock would match this AppImage's window on: X11's WM_CLASS when the
// window is an X11 client, otherwise the process name GTK falls back to.
std::string window_class_for_appimage(const std::string &s_path, std::string &s_source,
                                      std::string &s_error) {
    const std::vector<running_appimage_o> o_running = find_running_appimages();
    std::error_code o_error;
    const fs::path o_wanted = fs::weakly_canonical(fs::path(s_path), o_error);
    for (const running_appimage_o &o_entry : o_running) {
        std::error_code o_entry_error;
        const fs::path o_candidate = fs::weakly_canonical(fs::path(o_entry.appimage), o_entry_error);
        if (o_candidate != o_wanted) {
            continue;
        }
        if (!o_entry.window_class.empty()) {
            s_source = "the window " + o_entry.window_id + " of process "
                       + std::to_string(o_entry.i_pid);
            return o_entry.window_class;
        }
        if (!o_entry.process.empty()) {
            s_source = "process " + std::to_string(o_entry.i_pid) + " (" + o_entry.executable + ")";
            return o_entry.process;
        }
    }
    std::ostringstream o_message;
    o_message << "no running process belongs to " << s_path;
    if (o_running.empty()) {
        o_message << ", and no AppImage is running";
    } else {
        o_message << "; running AppImages:";
        for (const running_appimage_o &o_entry : o_running) {
            o_message << "\n  " << o_entry.appimage;
        }
    }
    o_message << "\nstart the application, then try again, or pass --wm-class CLASS";
    s_error = o_message.str();
    return {};
}

int command_windows(bool b_json) {
    const std::vector<running_appimage_o> o_running = find_running_appimages();
    if (b_json) {
        std::cout << '[';
        bool b_first = true;
        for (const running_appimage_o &o_entry : o_running) {
            if (!b_first) {
                std::cout << ',';
            }
            b_first = false;
            std::cout << "{\"appimage\":\"" << gnome_appimage::tools::json_escape(o_entry.appimage)
                      << "\",\"pid\":" << o_entry.i_pid << ",\"process\":\""
                      << gnome_appimage::tools::json_escape(o_entry.process)
                      << "\",\"executable\":\""
                      << gnome_appimage::tools::json_escape(o_entry.executable)
                      << "\",\"window_id\":\"" << gnome_appimage::tools::json_escape(o_entry.window_id)
                      << "\",\"wm_instance\":\""
                      << gnome_appimage::tools::json_escape(o_entry.window_instance)
                      << "\",\"wm_class\":\"" << gnome_appimage::tools::json_escape(o_entry.window_class)
                      << "\",\"title\":\"" << gnome_appimage::tools::json_escape(o_entry.title)
                      << "\"}";
        }
        std::cout << "]\n";
        return EXIT_OK;
    }
    if (o_running.empty()) {
        std::cout << "no running AppImage was found\n";
        std::cout << "GNOME does not let other programs list windows, so a native Wayland\n"
                     "window's class can only be read in Looking Glass: press Alt+F2, run 'lg',\n"
                     "open the Windows tab and read 'wmclass' for the window, then\n"
                     "re-integrate with: appimage-integrate install --wm-class <that value> <AppImage>\n";
        return EXIT_OK;
    }
    std::string s_current_appimage;
    std::vector<std::string> o_suggested_classes;
    for (const running_appimage_o &o_entry : o_running) {
        if (o_entry.appimage != s_current_appimage) {
            s_current_appimage = o_entry.appimage;
            o_suggested_classes.clear();
            std::cout << o_entry.appimage << '\n';
        }
        if (!o_entry.window_id.empty()) {
            std::cout << "  window  " << o_entry.window_id << "  " << o_entry.window_instance << '/'
                      << o_entry.window_class;
            if (!o_entry.title.empty()) {
                std::cout << "  \"" << o_entry.title << '"';
            }
            std::cout << '\n';
        }
        if (0 < o_entry.i_pid) {
            std::cout << "  process " << o_entry.i_pid << "  " << o_entry.executable << '\n';
        }
        const std::string s_class =
            o_entry.window_class.empty() ? o_entry.process : o_entry.window_class;
        if (s_class.empty()
            || o_suggested_classes.end() != std::find(o_suggested_classes.begin(),
                                                      o_suggested_classes.end(), s_class)) {
            continue;
        }
        o_suggested_classes.push_back(s_class);
        std::cout << "  --wm-class " << s_class;
        if (o_entry.window_class.empty()) {
            std::cout << "   (the program name, which GTK reports as the window app id when "
                         "the application sets no GtkApplication id)";
        }
        std::cout << '\n';
    }
    return EXIT_OK;
}

// A JSON array of strings, for the lists a command reports alongside its results.
void print_quoted_list(const std::vector<std::string> &o_items) {
    for (std::size_t i_index = 0; i_index < o_items.size(); i_index++) {
        if (0 < i_index) {
            std::cout << ',';
        }
        std::cout << '"' << gnome_appimage::tools::json_escape(o_items[i_index]) << '"';
    }
}

// Rewrite every recorded launcher from the AppImage's embedded entry, so launchers
// written before a change to the template gain the new keys in one command.  What the
// embedded entry does not carry is preserved from the launcher it replaces: the Name=
// a user chose, the desktop id, the icon name, and the window class.
int command_refresh(bool b_json, bool b_assume_yes, bool b_dry_run, bool b_wm_class_from_window,
                    const std::string &s_wm_class_override) {
    using gnome_appimage::desktop::desktop_entry_file_o;
    using gnome_appimage::desktop::desktop_entry_reader_c;
    using gnome_appimage::tools::json_escape;

    const appimage_integrator_c o_integrator;
    const std::vector<installed_appimage_o> o_installed = o_integrator.list_installed();
    if (o_installed.empty()) {
        if (b_json) {
            std::cout << "{\"launchers\":[],\"skipped\":[],\"written\":0}\n";
        } else {
            std::cout << "nothing is recorded, so there is no launcher to refresh\n";
        }
        return EXIT_OK;
    }

    std::vector<integration_plan_o> o_plans;
    std::vector<std::string> o_class_sources;
    std::vector<std::string> o_skipped;
    for (const installed_appimage_o &o_entry : o_installed) {
        std::error_code o_exists_error;
        if (!fs::exists(o_entry.appimage_path, o_exists_error)) {
            o_skipped.push_back(o_entry.desktop_id + ": the AppImage is not at "
                                + o_entry.appimage_path);
            continue;
        }
        // Read the launcher being replaced: its Name= may have been chosen by hand,
        // and its class is what the dock currently matches on.
        std::string s_name;
        std::string s_launcher_class;
        desktop_entry_file_o o_launcher;
        std::vector<gnome_appimage::desktop::desktop_entry_diagnostic_o> o_diagnostics;
        if (desktop_entry_reader_c::parse_file(o_entry.desktop_entry_path, o_launcher,
                                               o_diagnostics)) {
            s_name = o_launcher.value("Desktop Entry", "Name").value_or(std::string());
            s_launcher_class =
                o_launcher.value("Desktop Entry", "StartupWMClass").value_or(std::string());
        }

        // The class to write: an explicit --wm-class, then the running application when
        // asked to read it, then what the launcher already has, then a class this tool
        // recorded earlier.  Nothing is invented, so a launcher keeps its class unless
        // one of the two explicit sources supplies a better one.
        std::string s_class;
        std::string s_class_source;
        if (!s_wm_class_override.empty()) {
            s_class = s_wm_class_override;
            s_class_source = "--wm-class";
        }
        if (s_class.empty() && b_wm_class_from_window) {
            std::string s_where;
            std::string s_error;
            s_class = window_class_for_appimage(o_entry.appimage_path, s_where, s_error);
            if (!s_class.empty()) {
                s_class_source = "the running application, " + s_where;
            }
        }
        if (s_class.empty() && !s_launcher_class.empty()) {
            s_class = s_launcher_class;
            s_class_source = "the launcher being replaced";
        }
        if (s_class.empty() && o_entry.startup_wm_class_is_explicit
            && !o_entry.startup_wm_class.empty()) {
            s_class = o_entry.startup_wm_class;
            s_class_source = "the record";
        }

        integration_options_o o_options;
        o_options.tool_path = tool_path(o_integrator);
        o_options.install_directory = fs::path(o_entry.appimage_path).parent_path().string();
        o_options.desktop_file_name = o_entry.desktop_id;
        o_options.icon_name_override = o_entry.icon_name;
        o_options.name_override = s_name;
        o_options.startup_wm_class_override = s_class;
        o_options.move_appimage = false;
        o_options.refresh_own_launchers = true;
        o_options.identifier_override = o_entry.identifier;

        integration_plan_o o_plan;
        if (!o_integrator.plan(o_entry.appimage_path, o_options, o_plan)) {
            o_skipped.push_back(o_entry.desktop_id + ": " + o_plan.error);
            continue;
        }
        o_plans.push_back(std::move(o_plan));
        o_class_sources.push_back(s_class_source);
    }

    if (b_json) {
        std::cout << "{\"launchers\":[";
    } else {
        std::cout << "launchers to rewrite: " << o_plans.size() << '\n';
    }
    for (std::size_t i_index = 0; i_index < o_plans.size(); i_index++) {
        const integration_plan_o &o_plan = o_plans[i_index];
        if (b_json) {
            if (0 < i_index) {
                std::cout << ',';
            }
            std::cout << "{\"identifier\":\"" << json_escape(o_plan.identifier)
                      << "\",\"desktop_entry\":\"" << json_escape(o_plan.desktop_entry_path)
                      << "\",\"name\":\"" << json_escape(o_plan.name)
                      << "\",\"startup_wm_class\":\""
                      << json_escape(o_plan.startup_wm_class)
                      << "\",\"startup_wm_class_source\":"
                      << (o_class_sources[i_index].empty()
                              ? std::string("null")
                              : "\"" + json_escape(o_class_sources[i_index]) + "\"")
                      << ",\"mode\":\"" << json_escape(o_plan.mode) << "\"}";
            continue;
        }
        std::cout << "  " << o_plan.desktop_entry_path << '\n'
                  << "    " << o_plan.mode << '\n'
                  << "    name: " << o_plan.name << '\n'
                  << "    startup-wm-class: "
                  << (o_plan.startup_wm_class.empty() ? "(none)" : o_plan.startup_wm_class);
        if (!o_class_sources[i_index].empty()) {
            std::cout << "   (from " << o_class_sources[i_index] << ')';
        }
        std::cout << '\n';
    }
    if (!b_json) {
        for (const std::string &s_skipped : o_skipped) {
            std::cout << "  skipped: " << s_skipped << '\n';
        }
    }
    if (b_dry_run) {
        if (b_json) {
            std::cout << "],\"skipped\":[";
            print_quoted_list(o_skipped);
            std::cout << "],\"failed\":[],\"dry_run\":true,\"written\":0}\n";
        } else {
            std::cout << "dry run: nothing was written\n";
        }
        return EXIT_OK;
    }
    if (!b_assume_yes) {
        if (b_json) {
            std::cerr << "error: refusing to write without --yes on a non-interactive input\n";
            return EXIT_ERROR;
        }
        if (0 == isatty(STDIN_FILENO)) {
            std::cerr << "error: refusing to write without --yes on a non-interactive input\n";
            return EXIT_ERROR;
        }
        std::cout << "Rewrite these launchers? [y/N] " << std::flush;
        std::string s_answer;
        std::getline(std::cin, s_answer);
        if ("y" != s_answer && "Y" != s_answer) {
            std::cout << "cancelled\n";
            return EXIT_OK;
        }
    }

    std::vector<std::string> o_failed;
    int i_written = 0;
    for (std::size_t i_index = 0; i_index < o_plans.size(); i_index++) {
        const integration_plan_o &o_plan = o_plans[i_index];
        std::string s_error;
        if (!o_integrator.install(o_plan, s_error)) {
            o_failed.push_back(o_plan.desktop_entry_path + ": " + s_error);
            std::cerr << "error: " << o_plan.desktop_entry_path << ": " << s_error << '\n';
            continue;
        }
        i_written++;
        if (b_json) {
            continue;
        }
        std::cout << "rewritten: " << o_plan.desktop_entry_path;
        if (!o_plan.startup_wm_class.empty()) {
            std::cout << "  StartupWMClass=" << o_plan.startup_wm_class;
            if (!o_class_sources[i_index].empty()) {
                std::cout << " (from " << o_class_sources[i_index] << ')';
            }
        }
        std::cout << '\n';
    }
    if (b_json) {
        std::cout << "],\"skipped\":[";
        print_quoted_list(o_skipped);
        std::cout << "],\"failed\":[";
        print_quoted_list(o_failed);
        std::cout << "],\"written\":" << i_written << "}\n";
    } else {
        std::cout << "rewritten launchers: " << i_written << " of " << o_plans.size();
        if (!o_skipped.empty()) {
            std::cout << ", skipped " << o_skipped.size();
        }
        if (!o_failed.empty()) {
            std::cout << ", failed " << o_failed.size();
        }
        std::cout << '\n';
    }
    return o_skipped.empty() && o_failed.empty() ? EXIT_OK : EXIT_ERROR;
}


// -- update checks --------------------------------------------------------------
//
// The update-information value names where newer builds of an AppImage live.  These
// helpers resolve that value, ask the transport what it has, and compare it with what
// is installed.  Nothing is downloaded or installed: checking is a read.

// One asset of a GitHub release.
struct update_asset_o {
    std::string name;
    long long i_size = 0;
};

// One release of a GitHub repository.
struct update_release_o {
    std::string tag;
    bool b_prerelease = false;
    std::vector<update_asset_o> o_assets;
};

// What one check found.
struct update_check_o {
    std::string appimage;
    std::string information;
    std::string installed_version;
    std::string installed_source;
    std::string latest_version;
    std::string relation;
    std::string asset_name;
    long long i_asset_size = 0;
    std::string zsync_name;
    long long i_zsync_size = 0;
    bool b_update_available = false;
    // Empty when the check was made; otherwise why it could not be.
    std::string problem;
};

// Run a command and capture its output, keeping the exit status: a network failure
// must be reportable, which popen-with-stderr-discarded cannot do.
bool run_capture(const std::vector<std::string> &o_arguments, std::string &o_output,
                 int &i_status) {
    int i_pipe[2];
    if (0 != pipe(i_pipe)) {
        return false;
    }
    const pid_t i_child = fork();
    if (0 > i_child) {
        close(i_pipe[0]);
        close(i_pipe[1]);
        return false;
    }
    if (0 == i_child) {
        dup2(i_pipe[1], STDOUT_FILENO);
        dup2(i_pipe[1], STDERR_FILENO);
        close(i_pipe[0]);
        close(i_pipe[1]);
        std::vector<char *> o_argv;
        o_argv.reserve(o_arguments.size() + 1);
        for (const std::string &s_argument : o_arguments) {
            o_argv.push_back(const_cast<char *>(s_argument.c_str()));
        }
        o_argv.push_back(nullptr);
        execvp(o_argv[0], o_argv.data());
        _exit(127);
    }
    close(i_pipe[1]);
    char s_buffer[4096];
    ssize_t i_count = 0;
    while (0 < (i_count = read(i_pipe[0], s_buffer, sizeof(s_buffer)))) {
        o_output.append(s_buffer, static_cast<std::size_t>(i_count));
    }
    close(i_pipe[0]);
    int i_wait_status = 0;
    waitpid(i_child, &i_wait_status, 0);
    i_status = WIFEXITED(i_wait_status) ? WEXITSTATUS(i_wait_status) : -1;
    return true;
}

std::string first_line(const std::string &s_text) {
    const std::size_t i_end = s_text.find('\n');
    return std::string::npos == i_end ? s_text : s_text.substr(0, i_end);
}

std::string tool_user_agent() {
    return std::string("gnome-appimage-integration/")
           + gnome_appimage::version::version_string();
}

// Fetch a URL with curl.  The Accept header selects the GitHub API media type; for a
// plain zsync file only the first few kilobytes are asked for, which is all the header
// of a .zsync file needs.
bool http_get(const std::string &s_url, const std::string &s_accept, bool b_first_block,
              std::string &o_body, std::string &s_error) {
    if (!command_exists("curl")) {
        s_error = "curl is not installed, so no update check can be made";
        return false;
    }
    std::vector<std::string> o_command = {"curl", "-sS", "-L", "--max-time", "25",
                                          "-H", "User-Agent: " + tool_user_agent()};
    if (!s_accept.empty()) {
        o_command.push_back("-H");
        o_command.push_back("Accept: " + s_accept);
    }
    if (b_first_block) {
        o_command.push_back("-r");
        o_command.push_back("0-4095");
    }
    o_command.push_back(s_url);
    std::string s_output;
    int i_status = 0;
    if (!run_capture(o_command, s_output, i_status)) {
        s_error = "cannot run curl";
        return false;
    }
    if (0 != i_status) {
        s_error = "the request failed: " + first_line(s_output);
        return false;
    }
    o_body = s_output;
    return true;
}

// Read the fields of one release object from the GitHub API.
bool release_from_object(const gnome_appimage::json::value_c &o_object,
                         update_release_o &o_release) {
    const gnome_appimage::json::value_c *p_tag = o_object.member("tag_name");
    if (nullptr == p_tag || !p_tag->is_string()) {
        return false;
    }
    o_release.tag = p_tag->as_string();
    const gnome_appimage::json::value_c *p_prerelease = o_object.member("prerelease");
    o_release.b_prerelease = nullptr != p_prerelease && p_prerelease->as_boolean();
    const gnome_appimage::json::value_c *p_assets = o_object.member("assets");
    if (nullptr == p_assets || !p_assets->is_array()) {
        return true;
    }
    for (const gnome_appimage::json::value_c &o_asset : p_assets->items()) {
        const gnome_appimage::json::value_c *p_name = o_asset.member("name");
        if (nullptr == p_name || !p_name->is_string()) {
            continue;
        }
        update_asset_o o_found;
        o_found.name = p_name->as_string();
        const gnome_appimage::json::value_c *p_size = o_asset.member("size");
        if (nullptr != p_size && p_size->is_number()) {
            o_found.i_size = static_cast<long long>(p_size->as_number());
        }
        o_release.o_assets.push_back(std::move(o_found));
    }
    return true;
}

// Pick the release a transport asked for out of an API response.  `latest` answers with
// one release; `latest-pre` and `latest-all` answer with the list, newest first.
bool release_from_response(const std::string &s_body, bool b_is_list, bool b_want_prerelease,
                           update_release_o &o_release, std::string &s_error) {
    std::string s_json_error;
    const gnome_appimage::json::value_c o_value =
        gnome_appimage::json::value_c::parse(s_body, s_json_error);
    if (!s_json_error.empty()) {
        s_error = "the answer is not JSON: " + s_json_error;
        return false;
    }
    if (!b_is_list) {
        if (!o_value.is_object() || !release_from_object(o_value, o_release)) {
            const gnome_appimage::json::value_c *p_message = o_value.member("message");
            s_error = nullptr != p_message && p_message->is_string()
                          ? "the API answered: " + p_message->as_string()
                          : "the answer describes no release";
            return false;
        }
        return true;
    }
    if (!o_value.is_array() || o_value.items().empty()) {
        s_error = "the API answered with no releases";
        return false;
    }
    for (const gnome_appimage::json::value_c &o_item : o_value.items()) {
        update_release_o o_candidate;
        if (!release_from_object(o_item, o_candidate)) {
            continue;
        }
        // latest-all takes the newest of either kind, which the API lists first.
        if (!b_want_prerelease || o_candidate.b_prerelease) {
            o_release = std::move(o_candidate);
            return true;
        }
    }
    s_error = "the API answered with no release that fits";
    return false;
}

const update_asset_o *find_asset(const update_release_o &o_release,
                                 const std::string &s_pattern) {
    for (const update_asset_o &o_asset : o_release.o_assets) {
        if (gnome_appimage::appimage::update_pattern_matches(s_pattern, o_asset.name)) {
            return &o_asset;
        }
    }
    return nullptr;
}

// The version a release tag names: a leading v or V in front of a digit is how people
// tag releases, and it is not part of the version.
std::string release_version(const std::string &s_tag) {
    if (2 <= s_tag.size() && ('v' == s_tag[0] || 'V' == s_tag[0])
        && 0 != std::isdigit(static_cast<unsigned char>(s_tag[1]))) {
        return s_tag.substr(1);
    }
    return s_tag;
}

// Ask the transport named by one update-information value what it has, and compare that
// with the installed version.  Fills `o_result.problem` rather than failing.
void check_update(const std::string &s_appimage, const std::string &s_information,
                  const std::string &s_installed_version, const std::string &s_installed_source,
                  update_check_o &o_result) {
    using gnome_appimage::appimage::update_transport_e;

    o_result.appimage = s_appimage;
    o_result.information = s_information;
    o_result.installed_version = s_installed_version;
    o_result.installed_source = s_installed_source;

    const gnome_appimage::appimage::update_information_o o_update =
        understand_update_information(s_information);
    if (update_transport_e::absent == o_update.transport) {
        o_result.problem = "the AppImage carries no update information, so there is nothing "
                           "to ask";
        return;
    }
    if (!o_update.usable) {
        o_result.problem = o_update.problem;
        return;
    }

    std::string s_body;
    std::string s_error;
    if (update_transport_e::github_releases == o_update.transport) {
        if (!http_get(o_update.request_url, "application/vnd.github+json", false, s_body,
                      s_error)) {
            o_result.problem = s_error;
            return;
        }
        update_release_o o_release;
        if (!release_from_response(s_body, o_update.request_is_list,
                                   "latest-pre" == o_update.release, o_release, s_error)) {
            o_result.problem = s_error;
            return;
        }
        o_result.latest_version = release_version(o_release.tag);
        const update_asset_o *p_zsync = find_asset(o_release, o_update.zsync_pattern);
        if (nullptr == p_zsync) {
            o_result.problem = "release " + o_release.tag + " has no asset matching "
                               + o_update.zsync_pattern;
            return;
        }
        o_result.zsync_name = p_zsync->name;
        o_result.i_zsync_size = p_zsync->i_size;
        if (!o_update.image_pattern.empty()) {
            const update_asset_o *p_image = find_asset(o_release, o_update.image_pattern);
            if (nullptr != p_image) {
                o_result.asset_name = p_image->name;
                o_result.i_asset_size = p_image->i_size;
            }
        }
    } else {
        if (!http_get(o_update.request_url, {}, true, s_body, s_error)) {
            o_result.problem = s_error;
            return;
        }
        // A .zsync file starts with "key: value" header lines; Filename names the
        // AppImage this zsync file updates.
        std::istringstream o_lines(s_body);
        std::string s_line;
        while (std::getline(o_lines, s_line)) {
            const std::string s_trimmed = trim_spaces(s_line);
            if (0 == s_trimmed.compare(0, 9, "Filename:")) {
                o_result.asset_name = trim_spaces(s_trimmed.substr(9));
            } else if (0 == s_trimmed.compare(0, 7, "Length:")) {
                o_result.i_asset_size = std::atoll(trim_spaces(s_trimmed.substr(7)).c_str());
            }
        }
        if (o_result.asset_name.empty()) {
            o_result.problem = "the zsync file names no Filename, so nothing can be compared";
            return;
        }
        // A zsync file carries no version, only the name of the file it updates, so the
        // honest answer is whether that name is the one already installed.
        o_result.relation =
            o_result.asset_name == fs::path(s_appimage).filename().string() ? "same-file"
                                                                           : "other-file";
        return;
    }

    if (o_result.installed_version.empty()) {
        o_result.relation = "unknown";
        return;
    }
    const int i_relation = gnome_appimage::version::compare_versions(o_result.latest_version,
                                                                    o_result.installed_version);
    if (0 > i_relation) {
        o_result.relation = "older";
    } else if (0 < i_relation) {
        o_result.relation = "newer";
        o_result.b_update_available = true;
    } else {
        o_result.relation = "same";
    }
}

// The sentence a person reads for one check.
std::string update_check_sentence(const update_check_o &o_check) {
    if (!o_check.problem.empty()) {
        return "cannot check: " + o_check.problem;
    }
    if ("newer" == o_check.relation) {
        return "an update is available: " + o_check.latest_version;
    }
    if ("same" == o_check.relation) {
        return "up to date (" + o_check.latest_version + ")";
    }
    if ("same-file" == o_check.relation) {
        return "up to date: the transport offers the file that is installed";
    }
    if ("other-file" == o_check.relation) {
        return "a different file is offered: " + o_check.asset_name
               + " (the transport names no version, so this tool cannot tell whether it is "
                 "newer)";
    }
    if ("older" == o_check.relation) {
        return "the release is older than the installed version: " + o_check.latest_version
               + " against " + o_check.installed_version;
    }
    return "the installed version is unknown, and the release names "
           + o_check.latest_version;
}

void print_update_check_text(const update_check_o &o_check) {
    std::cout << "checking: " << o_check.appimage << '\n';
    std::cout << "  update-information: "
              << (o_check.information.empty() ? "(absent)" : o_check.information) << '\n';
    if (!o_check.installed_version.empty()) {
        std::cout << "  installed-version: " << o_check.installed_version;
        if (!o_check.installed_source.empty()) {
            std::cout << "  (from " << o_check.installed_source << ')';
        }
        std::cout << '\n';
    }
    std::cout << "  result: " << update_check_sentence(o_check) << '\n';
    if (!o_check.asset_name.empty()) {
        std::cout << "  appimage-asset: " << o_check.asset_name;
        if (0 < o_check.i_asset_size) {
            std::cout << "  (" << o_check.i_asset_size << " bytes)";
        }
        std::cout << '\n';
    }
    if (!o_check.zsync_name.empty()) {
        std::cout << "  zsync-asset: " << o_check.zsync_name;
        if (0 < o_check.i_zsync_size) {
            std::cout << "  (" << o_check.i_zsync_size << " bytes)";
        }
        std::cout << '\n';
    }
}

// Tell the user the result without a terminal: the launcher's context action runs this
// from the shell's menu, where nothing else would be visible.
void notify_update_checks(const std::vector<update_check_o> &o_checks) {
    std::string s_text;
    for (const update_check_o &o_check : o_checks) {
        if (!s_text.empty()) {
            s_text += "\n";
        }
        s_text += fs::path(o_check.appimage).filename().string() + ": "
                  + update_check_sentence(o_check);
    }
    if (s_text.empty()) {
        return;
    }
    if (command_exists("zenity") && has_display()) {
        static_cast<void>(capture_command(
            {"zenity", "--info", "--title=AppImage updates", "--text=" + s_text}));
        return;
    }
    if (command_exists("notify-send")) {
        static_cast<void>(capture_command(
            {"notify-send", "AppImage updates", s_text}));
        return;
    }
    std::cerr << "note: neither zenity nor notify-send is installed; the result is:\n"
              << s_text << '\n';
}

// Check each of these AppImages once.  Reading the file is what an update check is
// about, not whether it is integrated, so a launcher conflict must not stop it.
std::vector<update_check_o> run_update_checks(const appimage_integrator_c &o_integrator,
                                              const std::vector<std::string> &o_paths) {
    std::vector<update_check_o> o_checks;
    for (const std::string &s_target : o_paths) {
        gnome_appimage::appimage::appimage_info_o o_info;
        update_check_o o_check;
        if (!gnome_appimage::appimage::appimage_reader_c::read(s_target, o_info)) {
            o_check.appimage = s_target;
            o_check.problem = o_info.error;
            o_checks.push_back(std::move(o_check));
            continue;
        }
        std::string s_version_source;
        const std::string s_version = o_integrator.appimage_version(s_target, s_version_source);
        check_update(s_target, o_info.update_information, s_version, s_version_source, o_check);
        o_checks.push_back(std::move(o_check));
    }
    return o_checks;
}

int command_update(const std::string &s_path, bool b_all, bool b_json, bool b_notify) {
    using gnome_appimage::tools::json_escape;

    const appimage_integrator_c o_integrator;
    std::vector<std::string> o_paths;
    if (b_all) {
        // Several records can describe launchers for one file; check the file once.
        std::vector<std::string> o_seen;
        for (const installed_appimage_o &o_entry : o_integrator.list_installed()) {
            std::error_code o_error;
            const std::string s_key =
                fs::weakly_canonical(fs::path(o_entry.appimage_path), o_error).string();
            if (o_seen.end() != std::find(o_seen.begin(), o_seen.end(), s_key)) {
                continue;
            }
            o_seen.push_back(s_key);
            o_paths.push_back(o_entry.appimage_path);
        }
        if (o_paths.empty()) {
            if (b_json) {
                std::cout << "{\"checks\":[]}\n";
            } else {
                std::cout << "nothing is recorded, so there is nothing to check\n";
            }
            return EXIT_OK;
        }
    } else {
        o_paths.push_back(s_path);
    }

    const std::vector<update_check_o> o_checks = run_update_checks(o_integrator, o_paths);
    bool b_failed = false;
    for (const update_check_o &o_check : o_checks) {
        if (!o_check.problem.empty()) {
            b_failed = true;
        }
    }

    if (b_json) {
        std::cout << "{\"checks\":[";
        for (std::size_t i_index = 0; i_index < o_checks.size(); i_index++) {
            const update_check_o &o_check = o_checks[i_index];
            if (0 < i_index) {
                std::cout << ',';
            }
            std::cout << "{\"appimage\":\"" << json_escape(o_check.appimage)
                      << "\",\"update_information\":\"" << json_escape(o_check.information)
                      << "\",\"installed_version\":\""
                      << json_escape(o_check.installed_version)
                      << "\",\"installed_version_source\":"
                      << (o_check.installed_source.empty()
                              ? std::string("null")
                              : "\"" + json_escape(o_check.installed_source) + "\"")
                      << ",\"latest_version\":\"" << json_escape(o_check.latest_version)
                      << "\",\"relation\":\"" << json_escape(o_check.relation)
                      << "\",\"update_available\":"
                      << (o_check.b_update_available ? "true" : "false")
                      << ",\"appimage_asset\":\"" << json_escape(o_check.asset_name)
                      << "\",\"appimage_asset_size\":" << o_check.i_asset_size
                      << ",\"zsync_asset\":\"" << json_escape(o_check.zsync_name)
                      << "\",\"zsync_asset_size\":" << o_check.i_zsync_size
                      << ",\"problem\":\"" << json_escape(o_check.problem) << "\"}";
        }
        std::cout << "]}\n";
    } else {
        for (const update_check_o &o_check : o_checks) {
            print_update_check_text(o_check);
        }
        if (1 < o_checks.size()) {
            std::size_t i_checked = 0;
            std::size_t i_available = 0;
            for (const update_check_o &o_check : o_checks) {
                if (o_check.problem.empty()) {
                    i_checked++;
                }
                if (o_check.b_update_available) {
                    i_available++;
                }
            }
            std::cout << "checked " << i_checked << " of " << o_checks.size()
                      << ", could not check " << (o_checks.size() - i_checked)
                      << ", updates available " << i_available << '\n';
        }
        if (0 < o_checks.size()) {
            std::cout << "note: this command only checks; it does not download or install an "
                         "update\n";
        }
    }
    if (b_notify) {
        notify_update_checks(o_checks);
    }
    return b_failed ? EXIT_ERROR : EXIT_OK;
}

int command_audit(bool b_json, bool b_check) {
    const appimage_integrator_c o_integrator;
    std::vector<audit_finding_o> o_findings = o_integrator.audit();
    if (b_check) {
        // Asking the transport is opt-in: it needs the network and the desktop's
        // AppImages, and an audit that reaches out on its own would be a surprise.
        std::vector<std::string> o_paths;
        std::vector<std::string> o_seen;
        for (const installed_appimage_o &o_entry : o_integrator.list_installed()) {
            std::error_code o_error;
            const std::string s_key =
                fs::weakly_canonical(fs::path(o_entry.appimage_path), o_error).string();
            if (o_seen.end() != std::find(o_seen.begin(), o_seen.end(), s_key)) {
                continue;
            }
            o_seen.push_back(s_key);
            o_paths.push_back(o_entry.appimage_path);
        }
        for (const update_check_o &o_check : run_update_checks(o_integrator, o_paths)) {
            const std::string s_subject = fs::path(o_check.appimage).filename().string();
            if (!o_check.problem.empty()) {
                o_findings.push_back(
                    {audit_finding_o::severity_e::info, s_subject,
                     "the update check could not be made: " + o_check.problem,
                     "run: appimage-inspect --update-url " + o_check.appimage});
            } else if (o_check.b_update_available) {
                o_findings.push_back(
                    {audit_finding_o::severity_e::warning, s_subject,
                     "an update is available: " + o_check.latest_version + ", against the "
                         "installed " + o_check.installed_version,
                     "download it, then run: appimage-integrate install --replace <the new file>"});
            } else {
                o_findings.push_back({audit_finding_o::severity_e::info, s_subject,
                                      "up to date: " + o_check.latest_version, {}});
            }
        }
    }
    bool b_has_error = false;
    if (b_json) {
        std::cout << '[';
    }
    bool b_first = true;
    for (const audit_finding_o &o_finding : o_findings) {
        const char *s_severity = audit_finding_o::severity_e::error == o_finding.severity
                                     ? "error"
                                     : (audit_finding_o::severity_e::warning == o_finding.severity
                                            ? "warning"
                                            : "info");
        if (audit_finding_o::severity_e::error == o_finding.severity) {
            b_has_error = true;
        }
        if (b_json) {
            if (!b_first) {
                std::cout << ',';
            }
            std::cout << "{\"severity\":\"" << s_severity << "\",\"subject\":\""
                      << gnome_appimage::tools::json_escape(o_finding.subject)
                      << "\",\"message\":\""
                      << gnome_appimage::tools::json_escape(o_finding.message)
                      << "\",\"remedy\":\""
                      << gnome_appimage::tools::json_escape(o_finding.remedy) << "\"}";
        } else {
            std::cout << s_severity << ": " << o_finding.subject << ": " << o_finding.message
                      << '\n';
            if (!o_finding.remedy.empty()) {
                std::cout << "    remedy: " << o_finding.remedy << '\n';
            }
        }
        b_first = false;
    }
    if (b_json) {
        std::cout << "]\n";
    }
    return b_has_error ? EXIT_ERROR : EXIT_OK;
}

int command_run(const std::string &s_path,
                const std::vector<std::string> &o_arguments,
                bool b_extract_and_run) {
    struct stat o_status;
    if (0 != stat(s_path.c_str(), &o_status)) {
        std::cerr << "error: cannot stat " << s_path << '\n';
        return EXIT_ERROR;
    }
    if (0 == (o_status.st_mode & S_IXUSR)) {
        const mode_t u_mode = static_cast<mode_t>(o_status.st_mode | S_IXUSR | S_IXGRP | S_IXOTH);
        if (0 != chmod(s_path.c_str(), u_mode)) {
            std::cerr << "error: cannot make " << s_path << " executable\n";
            return EXIT_ERROR;
        }
        std::cout << "note: made " << s_path << " executable\n";
    }

    const bool b_fuse_available = 0 == access("/dev/fuse", R_OK | W_OK)
                                  && (command_exists("fusermount3")
                                      || command_exists("fusermount"));
    if (b_extract_and_run || !b_fuse_available) {
        if (!b_extract_and_run) {
            std::cout << "note: FUSE is unavailable; using APPIMAGE_EXTRACT_AND_RUN=1\n";
        }
        setenv("APPIMAGE_EXTRACT_AND_RUN", "1", 1);
    }

    std::vector<std::string> o_command;
    o_command.push_back(s_path);
    for (const std::string &s_argument : o_arguments) {
        o_command.push_back(s_argument);
    }
    std::vector<char *> o_raw;
    for (const std::string &s_argument : o_command) {
        o_raw.push_back(const_cast<char *>(s_argument.c_str()));
    }
    o_raw.push_back(nullptr);
    execv(s_path.c_str(), o_raw.data());
    std::cerr << "error: cannot execute " << s_path << '\n';
    return EXIT_ERROR;
}

// Locate the handler icon next to the tool, or in the source tree.
std::string handler_icon_source(const std::string &s_tool) {
    const fs::path o_tool(s_tool);
    const std::vector<fs::path> o_candidates = {
        o_tool.parent_path() / "icons/appimage-activator.svg",
        o_tool.parent_path() / "appimage-activator.svg",
        o_tool.parent_path().parent_path().parent_path()
            / "sources/tools/icons/appimage-activator.svg",
    };
    for (const fs::path &o_candidate : o_candidates) {
        std::error_code o_error;
        if (fs::exists(o_candidate, o_error)) {
            return o_candidate.string();
        }
    }
    return {};
}

bool write_text_file(const std::string &s_path, const std::string &s_text) {
    std::ofstream o_output(s_path, std::ios::trunc);
    if (!o_output) {
        return false;
    }
    o_output << s_text;
    return o_output.good();
}

std::string read_text_file(const std::string &s_path) {
    std::ifstream o_input(s_path);
    if (!o_input) {
        return {};
    }
    std::ostringstream o_buffer;
    o_buffer << o_input.rdbuf();
    return o_buffer.str();
}

std::string handler_desktop_path(const appimage_integrator_c &o_integrator) {
    return (fs::path(o_integrator.application_directories()[0]) / HANDLER_DESKTOP_ID).string();
}

// The pre-rename entry, if it is still on disk.
std::string handler_legacy_desktop_path(const appimage_integrator_c &o_integrator) {
    return (fs::path(o_integrator.application_directories()[0]) / HANDLER_LEGACY_DESKTOP_ID)
        .string();
}

int command_handler_status(const appimage_integrator_c &o_integrator) {
    const std::vector<std::string> o_types = {"application/vnd.appimage",
                                              "application/x-appimage",
                                              "application/x-iso9660-appimage"};
    for (const std::string &s_type : o_types) {
        const std::string s_default =
            capture_command({"xdg-mime", "query", "default", s_type});
        std::cout << s_type << ": " << (s_default.empty() ? "(none)" : s_default);
        if (HANDLER_DESKTOP_ID == s_default) {
            std::cout << "  [" << HANDLER_NAME << "]";
        } else if (HANDLER_LEGACY_DESKTOP_ID == s_default) {
            std::cout << "  [the old " << HANDLER_LEGACY_NAME
                      << " entry; run: appimage-integrate handler install]";
        }
        std::cout << '\n';
    }
    const std::string s_desktop = handler_desktop_path(o_integrator);
    std::cout << HANDLER_NAME << " entry: " << s_desktop << " ("
              << (fs::exists(s_desktop) ? "present" : "absent") << ")\n";
    const std::string s_legacy = handler_legacy_desktop_path(o_integrator);
    if (fs::exists(s_legacy)) {
        std::cout << "leftover " << HANDLER_LEGACY_NAME << " entry: " << s_legacy
                  << "  [run: appimage-integrate handler install]\n";
    }
    return EXIT_OK;
}

int command_handler_install(const appimage_integrator_c &o_integrator) {
    if (!command_exists("xdg-mime")) {
        std::cerr << "error: xdg-mime is required to register the handler\n";
        return EXIT_ERROR;
    }
    std::error_code o_error;
    const fs::path o_state_directory(o_integrator.state_directory());
    const fs::path o_data_home = o_state_directory.parent_path();
    fs::create_directories(o_state_directory, o_error);

    const std::vector<std::string> o_types = {"application/vnd.appimage",
                                              "application/x-appimage",
                                              "application/x-iso9660-appimage"};
    const std::string s_tool = tool_path(o_integrator);

    // The launcher must carry the activator icon.
    std::ostringstream o_desktop;
    o_desktop << "[Desktop Entry]\n"
              << "Type=Application\n"
              << "Name=" << HANDLER_NAME << '\n'
              << "Comment=Run or integrate an AppImage\n"
              << "Exec=" << s_tool << " handle %f\n"
              << "Icon=" << HANDLER_ICON_NAME << '\n'
              << "StartupWMClass=" << HANDLER_ICON_NAME << '\n'
              << "Terminal=false\n"
              << "NoDisplay=true\n"
              << "MimeType=application/vnd.appimage;application/x-appimage;"
                 "application/x-iso9660-appimage;\n"
              << "X-Integrated-By=gnome-appimage-integration\n"
              << "X-Integrated-At=" << gnome_appimage::version::version_string() << '\n';
    const std::string s_desktop = handler_desktop_path(o_integrator);
    write_text_file(s_desktop, o_desktop.str());

    // Install the icon into the user's icon theme.
    std::string s_icon_installed;
    const std::string s_icon_source = handler_icon_source(s_tool);
    if (!s_icon_source.empty()) {
        const fs::path o_theme_directory = o_data_home / "icons/hicolor/scalable/apps";
        fs::create_directories(o_theme_directory, o_error);
        std::error_code o_icon_error;
        const std::string s_icon_target =
            (o_theme_directory / (std::string(HANDLER_ICON_NAME) + ".svg")).string();
        fs::copy_file(s_icon_source, s_icon_target, fs::copy_options::overwrite_existing,
                      o_icon_error);
        if (!o_icon_error) {
            s_icon_installed = s_icon_target;
            refresh_icon_cache((o_data_home / "icons/hicolor").string());
        }
    }

    // Point the AppImage MIME types at the same icon, backing up the definition.
    // The rename means an existing definition may name either the original
    // generic icon or this tool's pre-rename icon.
    std::string s_mime_package;
    std::string s_mime_backup;
    const fs::path o_mime_package = o_data_home / "mime/packages/appimage.xml";
    std::error_code o_mime_error;
    if (fs::exists(o_mime_package, o_mime_error)) {
        std::string s_content = read_text_file(o_mime_package.string());
        const std::vector<std::string> o_old_icons = {"application-x-executable",
                                                      HANDLER_LEGACY_ICON_NAME};
        bool b_rewrite = false;
        for (const std::string &s_old_icon : o_old_icons) {
            if (std::string::npos != s_content.find(s_old_icon)) {
                b_rewrite = true;
                break;
            }
        }
        if (b_rewrite) {
            const fs::path o_backup_directory = o_state_directory / "backup";
            fs::create_directories(o_backup_directory, o_mime_error);
            s_mime_backup = (o_backup_directory / "appimage.xml").string();
            std::error_code o_backup_error;
            if (!fs::exists(s_mime_backup, o_backup_error)) {
                write_text_file(s_mime_backup, s_content);
            }
            for (const std::string &s_old_icon : o_old_icons) {
                std::size_t i_position = 0;
                while ((i_position = s_content.find(s_old_icon, i_position))
                       != std::string::npos) {
                    s_content.replace(i_position, s_old_icon.size(), HANDLER_ICON_NAME);
                    i_position += std::string(HANDLER_ICON_NAME).size();
                }
            }
            if (write_text_file(o_mime_package.string(), s_content)) {
                s_mime_package = o_mime_package.string();
                if (command_exists("update-mime-database")) {
                    const std::string s_update = capture_command(
                        {"update-mime-database", (o_data_home / "mime").string()});
                    static_cast<void>(s_update);
                }
            }
        }
    }

    std::ostringstream o_manifest;
    o_manifest << "handler_desktop=" << s_desktop << '\n';
    if (!s_icon_installed.empty()) {
        o_manifest << "icon=" << s_icon_installed << '\n';
    }
    if (!s_mime_package.empty()) {
        o_manifest << "mime_package=" << s_mime_package << '\n';
        o_manifest << "mime_backup=" << s_mime_backup << '\n';
    }
    // Preserve the real previous defaults when our own handler is re-installed,
    // including across the rename, when they live in the pre-rename manifest.
    std::map<std::string, std::string> o_previous_by_type;
    {
        std::ifstream o_old_manifest((o_state_directory / HANDLER_MANIFEST).string());
        if (!o_old_manifest) {
            o_old_manifest.open((o_state_directory / HANDLER_LEGACY_MANIFEST).string());
        }
        std::string s_old_line;
        while (std::getline(o_old_manifest, s_old_line)) {
            const std::size_t i_old_tab = s_old_line.find('\t');
            if (0 != s_old_line.compare(0, 17, "previous_default=")
                || std::string::npos == i_old_tab) {
                continue;
            }
            o_previous_by_type[s_old_line.substr(17, i_old_tab - 17)] =
                s_old_line.substr(i_old_tab + 1);
        }
    }
    for (const std::string &s_type : o_types) {
        std::string s_previous = capture_command({"xdg-mime", "query", "default", s_type});
        if (HANDLER_DESKTOP_ID == s_previous || HANDLER_LEGACY_DESKTOP_ID == s_previous) {
            const auto o_old = o_previous_by_type.find(s_type);
            if (o_previous_by_type.end() != o_old && !o_old->second.empty()) {
                s_previous = o_old->second;
            }
        }
        o_manifest << "previous_default=" << s_type << '\t' << s_previous << '\n';
    }
    const std::string s_manifest = (o_state_directory / HANDLER_MANIFEST).string();
    write_text_file(s_manifest, o_manifest.str());

    std::vector<std::string> o_command = {"xdg-mime", "default", HANDLER_DESKTOP_ID};
    for (const std::string &s_type : o_types) {
        o_command.push_back(s_type);
    }
    const std::string s_result = capture_command(o_command);
    static_cast<void>(s_result);
    if (command_exists("update-desktop-database")) {
        const std::string s_update = capture_command(
            {"update-desktop-database", o_integrator.application_directories()[0]});
        static_cast<void>(s_update);
    }

    // The rename leaves two things behind that would still answer for the AppImage
    // MIME types: the pre-rename entry, its icon, and its own record.
    std::vector<std::string> o_removed_legacy;
    const std::string s_legacy_desktop = handler_legacy_desktop_path(o_integrator);
    for (const std::string &s_path :
         {s_legacy_desktop,
          (o_data_home / "icons/hicolor/scalable/apps"
           / (std::string(HANDLER_LEGACY_ICON_NAME) + ".svg"))
              .string(),
          (o_state_directory / HANDLER_LEGACY_MANIFEST).string()}) {
        std::error_code o_remove_error;
        if (fs::exists(s_path, o_remove_error) && fs::remove(s_path, o_remove_error)) {
            o_removed_legacy.push_back(s_path);
        }
    }
    if (!o_removed_legacy.empty()) {
        refresh_icon_cache((o_data_home / "icons/hicolor").string());
        std::cout << "removed the pre-rename " << HANDLER_LEGACY_NAME << " files:\n";
        for (const std::string &s_path : o_removed_legacy) {
            std::cout << "  " << s_path << '\n';
        }
    }

    std::cout << HANDLER_NAME << " installed: " << s_desktop << '\n';
    if (!s_icon_installed.empty()) {
        std::cout << "handler icon: " << s_icon_installed << '\n';
    }
    std::cout << "previous defaults recorded in " << s_manifest << '\n';
    return command_handler_status(o_integrator);
}

int command_handler_uninstall(const appimage_integrator_c &o_integrator) {
    const fs::path o_state_directory(o_integrator.state_directory());
    // Fall back to the pre-rename record, so an install from before the rename can
    // still be reversed while its entry is on disk.
    std::string s_manifest = (o_state_directory / HANDLER_MANIFEST).string();
    std::string s_legacy_manifest = (o_state_directory / HANDLER_LEGACY_MANIFEST).string();
    std::ifstream o_input(s_manifest);
    if (!o_input) {
        s_manifest = s_legacy_manifest;
        o_input.open(s_manifest);
    }
    if (!o_input) {
        std::cerr << "error: no handler manifest at "
                  << (o_state_directory / HANDLER_MANIFEST).string() << '\n';
        return EXIT_ERROR;
    }
    std::string s_line;
    std::string s_mime_package;
    std::string s_mime_backup;
    std::vector<std::string> o_icons;
    while (std::getline(o_input, s_line)) {
        if (0 == s_line.compare(0, 5, "icon=")) {
            o_icons.push_back(s_line.substr(5));
            continue;
        }
        if (0 == s_line.compare(0, 13, "mime_package=")) {
            s_mime_package = s_line.substr(13);
            continue;
        }
        if (0 == s_line.compare(0, 12, "mime_backup=")) {
            s_mime_backup = s_line.substr(12);
            continue;
        }
        const std::size_t i_tab = s_line.find('\t');
        if (0 != s_line.compare(0, 17, "previous_default=") || std::string::npos == i_tab) {
            continue;
        }
        const std::string s_type = s_line.substr(17, i_tab - 17);
        const std::string s_previous = s_line.substr(i_tab + 1);
        if (s_previous.empty()) {
            continue;
        }
        if (command_exists("xdg-mime")) {
            const std::string s_result =
                capture_command({"xdg-mime", "default", s_previous, s_type});
            static_cast<void>(s_result);
        }
    }
    o_input.close();

    std::error_code o_error;
    const std::string s_icon_directory =
        (fs::path(o_integrator.state_directory()).parent_path() / "icons/hicolor").string();
    // Remove the recorded icons, and both names this handler has used, because the
    // record being reversed may predate the rename.
    for (const std::string &s_icon : o_icons) {
        fs::remove(s_icon, o_error);
        o_error.clear();
    }
    for (const char *s_icon_name : {HANDLER_ICON_NAME, HANDLER_LEGACY_ICON_NAME}) {
        fs::remove((fs::path(s_icon_directory) / "scalable/apps"
                    / (std::string(s_icon_name) + ".svg"))
                       .string(),
                   o_error);
        o_error.clear();
    }
    refresh_icon_cache(s_icon_directory);
    if (!s_mime_package.empty() && !s_mime_backup.empty()
        && fs::exists(s_mime_backup, o_error)) {
        std::error_code o_restore_error;
        fs::copy_file(s_mime_backup, s_mime_package, fs::copy_options::overwrite_existing,
                      o_restore_error);
        if (!o_restore_error && command_exists("update-mime-database")) {
            const std::string s_mime_directory =
                fs::path(s_mime_package).parent_path().parent_path().string();
            const std::string s_result = capture_command({"update-mime-database", s_mime_directory});
            static_cast<void>(s_result);
        }
    }
    for (const std::string &s_path : {handler_desktop_path(o_integrator),
                                      handler_legacy_desktop_path(o_integrator), s_manifest,
                                      s_legacy_manifest}) {
        fs::remove(s_path, o_error);
        o_error.clear();
    }
    std::cout << HANDLER_NAME << " removed and previous defaults restored\n";
    return command_handler_status(o_integrator);
}

bool has_display() {
    return nullptr != std::getenv("DISPLAY") || nullptr != std::getenv("WAYLAND_DISPLAY");
}

// Write a report to a private temporary file for `zenity --text-info --filename`.
std::string write_temp_report(const std::string &s_text) {
    const char *s_runtime = std::getenv("XDG_RUNTIME_DIR");
    std::string s_template =
        std::string(nullptr == s_runtime ? "/tmp" : s_runtime) + "/appimage-report-XXXXXX";
    std::vector<char> o_buffer(s_template.begin(), s_template.end());
    o_buffer.push_back('\0');
    const int i_descriptor = mkstemp(o_buffer.data());
    if (0 > i_descriptor) {
        return {};
    }
    const std::string s_path = o_buffer.data();
    const ssize_t i_written = write(i_descriptor, s_text.data(), s_text.size());
    close(i_descriptor);
    if (0 > i_written || static_cast<std::size_t>(i_written) != s_text.size()) {
        unlink(s_path.c_str());
        return {};
    }
    return s_path;
}

void show_text_info(const std::string &s_title, const std::string &s_text) {
    if (!command_exists("zenity") || !has_display()) {
        std::cout << s_text;
        return;
    }
    const std::string s_path = write_temp_report(s_text);
    if (s_path.empty()) {
        std::cout << s_text;
        return;
    }
    const std::string s_result = capture_command(
        {"zenity", "--text-info", "--title=" + s_title, "--width=760", "--height=560",
         "--filename=" + s_path});
    static_cast<void>(s_result);
    unlink(s_path.c_str());
}

// A desktop notice that stays visible for a few seconds.
void show_notice(const std::string &s_title, const std::string &s_body) {
    // Always leave a trace in the log, even when a notification is shown.
    std::cout << s_title << ": " << s_body << '\n';
    if (!has_display()) {
        return;
    }
    if (command_exists("notify-send")) {
        const std::string s_result = capture_command(
            {"notify-send", "-a", HANDLER_NAME, "-u", "normal", "-t", "5000", s_title,
             s_body});
        static_cast<void>(s_result);
        return;
    }
    if (command_exists("zenity")) {
        const std::string s_result =
            capture_command({"zenity", "--info", "--timeout=5", "--title=AppImage",
                             "--text=" + s_title + "\n" + s_body});
        static_cast<void>(s_result);
    }
}

// Start the AppImage in its own session and keep the handler alive only long
// enough to show a notice with the child's process id.
int run_detached_with_notice(const std::string &s_path, const std::string &s_name) {
    struct stat o_status;
    if (0 != stat(s_path.c_str(), &o_status)) {
        show_text_info("AppImage", "Cannot stat " + s_path);
        return EXIT_ERROR;
    }
    if (0 == (o_status.st_mode & S_IXUSR)) {
        const mode_t u_mode = static_cast<mode_t>(o_status.st_mode | S_IXUSR | S_IXGRP | S_IXOTH);
        if (0 != chmod(s_path.c_str(), u_mode)) {
            show_text_info("AppImage", "Cannot make " + s_path + " executable");
            return EXIT_ERROR;
        }
    }

    const bool b_fuse_available = 0 == access("/dev/fuse", R_OK | W_OK)
                                  && (command_exists("fusermount3")
                                      || command_exists("fusermount"));
    const std::string s_label = s_name.empty() ? fs::path(s_path).filename().string() : s_name;

    const pid_t i_child = fork();
    if (0 > i_child) {
        show_text_info("AppImage", "Cannot fork to start " + s_label);
        return EXIT_ERROR;
    }
    if (0 == i_child) {
        setsid();
        if (nullptr == std::freopen("/dev/null", "w", stdout)) {
            _exit(127);
        }
        if (nullptr == std::freopen("/dev/null", "w", stderr)) {
            _exit(127);
        }
        if (!b_fuse_available) {
            setenv("APPIMAGE_EXTRACT_AND_RUN", "1", 1);
        }
        std::vector<std::string> o_command;
        o_command.push_back(s_path);
        std::vector<char *> o_raw;
        for (const std::string &s_argument : o_command) {
            o_raw.push_back(const_cast<char *>(s_argument.c_str()));
        }
        o_raw.push_back(nullptr);
        execv(s_path.c_str(), o_raw.data());
        _exit(127);
    }

    show_notice("Starting " + s_label,
                "process " + std::to_string(static_cast<long long>(i_child))
                    + (b_fuse_available ? "" : " (extract and run)"));
    return EXIT_OK;
}

int command_handle(const std::string &s_path) {
    const std::string s_name = embedded_name(s_path);
    const std::string s_label = s_name.empty() ? fs::path(s_path).filename().string() : s_name;

    // Prefer the graphical activator: it can remember its size, keep a text area
    // for Inspect, and offer the conflict choices.
    const appimage_integrator_c o_integrator;
    const std::string s_tool = tool_path(o_integrator);
    const std::string s_ui = handler_ui_program(s_tool);
    if (!s_ui.empty() && has_display()) {
        std::vector<std::string> o_command = {s_ui, "--tool", s_tool, s_path};
        std::vector<char *> o_raw;
        for (const std::string &s_argument : o_command) {
            o_raw.push_back(const_cast<char *>(s_argument.c_str()));
        }
        o_raw.push_back(nullptr);
        execvp(o_raw[0], o_raw.data());
        // Fall through to the zenity flow if execvp failed, which happens when the
        // file is a script without an interpreter.
    }

    if (!command_exists("zenity") || !has_display()) {
        std::cout << "AppImage: " << s_label << '\n'
                  << "  appimage-integrate run \"" << s_path << "\"\n"
                  << "  appimage-integrate install \"" << s_path << "\"\n"
                  << "  appimage-inspect \"" << s_path << "\"\n";
        return EXIT_OK;
    }

    const std::string s_choice = capture_command(
        {"zenity", "--list", "--radiolist", "--title=AppImage",
         "--text=What do you want to do with " + s_label + "?", "--column=",
         "--column=Action", "TRUE", "Run once", "FALSE", "Integrate", "FALSE", "Inspect",
         "FALSE", "Cancel", "--height=280", "--width=460"});

    if ("Run once" == s_choice) {
        return run_detached_with_notice(s_path, s_name);
    }

    if ("Inspect" == s_choice) {
        const appimage_integrator_c o_integrator;
        integration_options_o o_options;
        o_options.conflict_policy = integration_conflict_policy_e::fail;
        o_options.tool_path = tool_path(o_integrator);
        show_text_info(s_label, o_integrator.describe(s_path, o_options));
        return EXIT_OK;
    }

    if ("Integrate" == s_choice) {
        const appimage_integrator_c o_integrator;
        integration_options_o o_options;
        o_options.tool_path = tool_path(o_integrator);
        integration_plan_o o_plan;
        if (!o_integrator.plan(s_path, o_options, o_plan)) {
            if (o_plan.conflicts.empty()) {
                show_text_info("AppImage", "Cannot integrate " + s_label + ":\n\n" + o_plan.error);
                return EXIT_ERROR;
            }
            std::string s_prompt = s_label + " is already represented by:\n";
            for (const integration_conflict_o &o_conflict : o_plan.conflicts) {
                s_prompt += "\n  " + o_conflict.path + "\n      (" + o_conflict.origin + ")";
            }
            const std::string s_action = capture_command(
                {"zenity", "--list", "--radiolist", "--title=AppImage",
                 "--text=" + s_prompt, "--column=", "--column=Action", "TRUE",
                 "Replace existing", "FALSE", "Add alongside", "FALSE", "Cancel",
                 "--height=340", "--width=680"});
            if ("Replace existing" == s_action) {
                o_options.conflict_policy = integration_conflict_policy_e::replace;
            } else if ("Add alongside" == s_action) {
                o_options.conflict_policy = integration_conflict_policy_e::add;
            } else {
                return EXIT_OK;
            }
            if (!o_integrator.plan(s_path, o_options, o_plan)) {
                show_text_info("AppImage", "Cannot integrate " + s_label + ":\n\n" + o_plan.error);
                return EXIT_ERROR;
            }
        }

        std::string s_error;
        if (!o_integrator.install(o_plan, s_error)) {
            show_text_info("AppImage", "Cannot integrate " + s_label + ":\n\n" + s_error);
            return EXIT_ERROR;
        }

        std::string s_report = "Integrated " + s_label + "\n\n";
        s_report += o_integrator.describe_plan(o_plan);
        s_report += "\nwhat was done:\n";
        for (const integration_action_o &o_action : o_plan.actions) {
            s_report += "  " + o_action.description + ": "
                        + (o_action.target_path.empty() ? o_action.source_path
                                                        : o_action.target_path)
                        + "\n";
        }
        show_text_info("AppImage", s_report);
        return run_detached_with_notice(o_plan.installed_path, s_name);
    }

    return EXIT_OK;
}

}  // namespace

int main(int i_argument_count, char **p_arguments) {
    if (1 >= i_argument_count) {
        print_usage(std::cerr);
        return EXIT_USAGE;
    }
    const std::string s_command = p_arguments[1];
    if ("--help" == s_command || "help" == s_command) {
        print_usage(std::cout);
        return EXIT_OK;
    }
    if ("--version" == s_command) {
        std::cout << "appimage-integrate " << gnome_appimage::version::version_string()
                  << " (build " << gnome_appimage::version::version_build_counter() << ")\n";
        return EXIT_OK;
    }

    std::string s_path;
    std::string s_install_dir;
    std::string s_desktop_file_name;
    std::string s_exec_args;
    std::string s_wm_class;
    std::string s_icon_name;
    std::string s_name;
    std::string s_identifier;
    bool b_move = true;
    bool b_icons = true;
    integration_conflict_policy_e e_conflict_policy = integration_conflict_policy_e::fail;
    bool b_extract_and_run = false;
    bool b_detached = false;
    bool b_json = false;
    bool b_assume_yes = false;
    bool b_remove_appimage = false;
    bool b_ignore_signature = false;
    bool b_wm_class_from_window = false;
    bool b_dry_run = false;
    bool b_check = false;
    bool b_all = false;
    bool b_notify = false;
    std::vector<std::string> o_run_arguments;

    for (int i_index = 2; i_index < i_argument_count; i_index++) {
        const std::string s_argument = p_arguments[i_index];
        // Everything after the AppImage path belongs to the AppImage itself.
        if ("run" == s_command && !s_path.empty()) {
            o_run_arguments.push_back(s_argument);
            continue;
        }
        const bool b_needs_value =
            "--install-dir" == s_argument || "--desktop-file-name" == s_argument
            || "--exec-args" == s_argument || "--wm-class" == s_argument
            || "--icon-name" == s_argument || "--identifier" == s_argument
            || "--name" == s_argument;
        if (b_needs_value && i_argument_count > i_index + 1) {
            const std::string s_value = p_arguments[++i_index];
            if ("--install-dir" == s_argument) {
                s_install_dir = s_value;
            } else if ("--desktop-file-name" == s_argument) {
                s_desktop_file_name = s_value;
            } else if ("--exec-args" == s_argument) {
                s_exec_args = s_value;
            } else if ("--wm-class" == s_argument) {
                s_wm_class = s_value;
            } else if ("--icon-name" == s_argument) {
                s_icon_name = s_value;
            } else if ("--name" == s_argument) {
                s_name = s_value;
            } else {
                s_identifier = s_value;
            }
        } else if ("--no-move" == s_argument) {
            b_move = false;
        } else if ("--no-icons" == s_argument) {
            b_icons = false;
        } else if ("--ignore-signature" == s_argument) {
            b_ignore_signature = true;
        } else if ("--wm-class-from-window" == s_argument) {
            b_wm_class_from_window = true;
        } else if ("--dry-run" == s_argument) {
            b_dry_run = true;
        } else if ("--check" == s_argument) {
            b_check = true;
        } else if ("--all" == s_argument) {
            b_all = true;
        } else if ("--notify" == s_argument) {
            b_notify = true;
        } else if ("--replace" == s_argument) {
            e_conflict_policy = integration_conflict_policy_e::replace;
        } else if ("--add" == s_argument) {
            e_conflict_policy = integration_conflict_policy_e::add;
        } else if ("--extract-and-run" == s_argument) {
            b_extract_and_run = true;
        } else if ("--detached" == s_argument) {
            b_detached = true;
        } else if ("--json" == s_argument) {
            b_json = true;
        } else if ("--yes" == s_argument) {
            b_assume_yes = true;
        } else if ("--remove-appimage" == s_argument) {
            b_remove_appimage = true;
        } else if (!s_argument.empty() && '-' == s_argument[0]) {
            std::cerr << "error: unknown option: " << s_argument << '\n';
            return EXIT_USAGE;
        } else if (s_path.empty()) {
            s_path = s_argument;
        } else if ("run" == s_command) {
            o_run_arguments.push_back(s_argument);
        } else {
            std::cerr << "error: only one path may be given\n";
            return EXIT_USAGE;
        }
    }

    // refresh resolves a class per recorded launcher, inside the command.
    if (b_wm_class_from_window && "refresh" != s_command) {
        if (s_path.empty()) {
            std::cerr << "error: --wm-class-from-window needs an AppImage path\n";
            return EXIT_USAGE;
        }
        std::string s_source;
        std::string s_error;
        s_wm_class = window_class_for_appimage(s_path, s_source, s_error);
        if (s_wm_class.empty()) {
            std::cerr << "error: " << s_error << '\n';
            return EXIT_ERROR;
        }
        // A diagnostic, so --json output stays machine-readable.
        std::cerr << "StartupWMClass " << s_wm_class << " read from " << s_source << '\n';
    }

    const appimage_integrator_c o_integrator;

    if ("plan" == s_command) {
        if (s_path.empty()) {
            std::cerr << "error: plan needs an AppImage path\n";
            return EXIT_USAGE;
        }
        return command_plan_or_explain(
            s_path,
            options_from(s_install_dir, s_desktop_file_name, s_exec_args, s_wm_class, s_icon_name,
                          s_name, b_ignore_signature,
                          b_move, b_icons, e_conflict_policy),
            b_json);
    }
    if ("explain" == s_command) {
        if (s_path.empty()) {
            std::cerr << "error: explain needs an AppImage path\n";
            return EXIT_USAGE;
        }
        if (b_json) {
            return command_explain_json(
                s_path,
                options_from(s_install_dir, s_desktop_file_name, s_exec_args, s_wm_class,
                              s_icon_name, s_name, b_ignore_signature, b_move, b_icons,
                              e_conflict_policy));
        }
        return command_explain(
            s_path,
            options_from(s_install_dir, s_desktop_file_name, s_exec_args, s_wm_class, s_icon_name,
                          s_name, b_ignore_signature,
                          b_move, b_icons, e_conflict_policy));
    }
    if ("install" == s_command) {
        if (s_path.empty()) {
            std::cerr << "error: install needs an AppImage path\n";
            return EXIT_USAGE;
        }
        return command_install(
            s_path,
            options_from(s_install_dir, s_desktop_file_name, s_exec_args, s_wm_class, s_icon_name,
                          s_name, b_ignore_signature,
                          b_move, b_icons, e_conflict_policy),
            b_assume_yes);
    }
    if ("uninstall" == s_command) {
        if (s_identifier.empty()) {
            std::cerr << "error: uninstall needs --identifier\n";
            return EXIT_USAGE;
        }
        return command_uninstall(s_identifier, b_remove_appimage);
    }
    if ("list" == s_command) {
        return command_list(b_json);
    }
    if ("update" == s_command) {
        if (!b_check) {
            std::cerr << "error: only checking is implemented; run: appimage-integrate update "
                         "--check [--all] <AppImage>\n";
            return EXIT_USAGE;
        }
        if (!b_all && s_path.empty()) {
            std::cerr << "error: update --check needs an AppImage path, or --all\n";
            return EXIT_USAGE;
        }
        return command_update(s_path, b_all, b_json, b_notify);
    }
    if ("refresh" == s_command) {
        return command_refresh(b_json, b_assume_yes, b_dry_run, b_wm_class_from_window, s_wm_class);
    }
    if ("run" == s_command) {
        if (s_path.empty()) {
            std::cerr << "error: run needs an AppImage path\n";
            return EXIT_USAGE;
        }
        if (b_detached) {
            return run_detached_with_notice(s_path, embedded_name(s_path));
        }
        return command_run(s_path, o_run_arguments, b_extract_and_run);
    }
    if ("windows" == s_command) {
        return command_windows(b_json);
    }
    if ("audit" == s_command) {
        return command_audit(b_json, b_check);
    }
    if ("handler" == s_command) {
        const std::string s_subcommand = s_path.empty() ? std::string("status") : s_path;
        if ("install" == s_subcommand) {
            return command_handler_install(o_integrator);
        }
        if ("uninstall" == s_subcommand) {
            return command_handler_uninstall(o_integrator);
        }
        return command_handler_status(o_integrator);
    }
    if ("handle" == s_command) {
        if (s_path.empty()) {
            std::cerr << "error: handle needs an AppImage path\n";
            return EXIT_USAGE;
        }
        return command_handle(s_path);
    }

    std::cerr << "error: unknown command: " << s_command << '\n';
    print_usage(std::cerr);
    return EXIT_USAGE;
}
