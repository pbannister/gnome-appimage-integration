// appimage-integrate: plan, install, uninstall, audit, and run AppImages,
// and manage the *.AppImage double-click handler.
#include "appimage/appimage_reader.h"
#include "appimage/squashfs_reader.h"
#include "desktop/desktop_entry_locator.h"
#include "desktop/desktop_entry_reader.h"
#include "integration/appimage_integrator.h"
#include "tools/desktop_entry_output.h"
#include "version/version.h"

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
using gnome_appimage::integration::integration_options_o;
using gnome_appimage::integration::integration_plan_o;

constexpr int EXIT_OK = 0;
constexpr int EXIT_ERROR = 1;
constexpr int EXIT_USAGE = 2;
constexpr const char *HANDLER_DESKTOP_ID = "appimage-handler.desktop";
constexpr const char *HANDLER_MANIFEST = "appimage-handler.manifest";

void print_usage(std::ostream &o_out) {
    o_out << "usage: appimage-integrate <command> [options]\n"
          << "\n"
          << "commands:\n"
          << "  explain <AppImage>        show what this AppImage is and what install would write\n"
          << "  plan <AppImage>           print the install plan; change nothing\n"
          << "  install <AppImage>        integrate the AppImage into the desktop\n"
          << "  uninstall --identifier ID [--remove-appimage]\n"
          << "  list                      list AppImages integrated by this tool\n"
          << "  run <AppImage> [args...]  run once, without integrating\n"
          << "  audit                     report desktop integration inconsistencies\n"
          << "  handler status            show the current *.AppImage handler\n"
          << "  handler install           make this tool the *.AppImage handler\n"
          << "  handler uninstall         restore the previous *.AppImage handler\n"
          << "  handle <AppImage>         the handler entry point (double-click)\n"
          << "\n"
          << "options:\n"
          << "  --install-dir DIR         where the AppImage is placed (default ~/Applications)\n"
          << "  --desktop-file-name NAME  override the desktop file name (the ID)\n"
          << "  --exec-args ARGUMENTS     extra arguments inserted into Exec\n"
          << "  --wm-class CLASS          set StartupWMClass explicitly\n"
          << "  --icon-name NAME          override the installed icon name\n"
          << "  --no-move                 copy instead of move\n"
          << "  --no-icons                do not install icons\n"
          << "  --replace                 replace an existing launcher for this application\n"
          << "  --add                     install alongside an existing launcher\n"
          << "  --extract-and-run         for run: force APPIMAGE_EXTRACT_AND_RUN=1\n"
          << "  --detached                for run: start in a new session and report the pid\n"
          << "  --json                    machine-readable output where supported\n"
          << "  --yes                     do not ask before writing\n"
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
bool command_succeeds(const std::vector<std::string> &o_arguments) {
    if (o_arguments.empty()) {
        return false;
    }
    std::vector<char *> o_raw;
    for (const std::string &s_argument : o_arguments) {
        o_raw.push_back(const_cast<char *>(s_argument.c_str()));
    }
    o_raw.push_back(nullptr);
    const pid_t i_child = fork();
    if (0 > i_child) {
        return false;
    }
    if (0 == i_child) {
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
    return WIFEXITED(i_status) && 0 == WEXITSTATUS(i_status);
}

// Locate the graphical handler next to the tool, or in the source tree.
std::string handler_ui_script(const std::string &s_tool) {
    const fs::path o_tool(s_tool);
    const std::string s_installed = (o_tool.parent_path() / "appimage_handler_ui.py").string();
    if (fs::exists(s_installed)) {
        return s_installed;
    }
    const std::string s_source =
        (o_tool.parent_path().parent_path().parent_path() / "sources/tools/appimage_handler_ui.py")
            .string();
    if (fs::exists(s_source)) {
        return s_source;
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
    std::cout << "appimage: " << o_plan.appimage_path << '\n'
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
              << (o_plan.startup_wm_class.empty() ? "(none)" : o_plan.startup_wm_class) << '\n';
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
              << json_escape(o_plan.installed_path) << "\",\"identifier\":\""
              << json_escape(o_plan.identifier) << "\",\"desktop_id\":\""
              << json_escape(o_plan.desktop_id) << "\",\"desktop_entry\":\""
              << json_escape(o_plan.desktop_entry_path) << "\",\"icon_name\":\""
              << json_escape(o_plan.icon_name) << "\",\"exec\":\""
              << json_escape(o_plan.exec_command) << "\",\"startup_wm_class\":\""
              << json_escape(o_plan.startup_wm_class) << "\",\"icons\":[";
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
                                  bool b_move,
                                  bool b_icons,
                                  integration_conflict_policy_e e_policy) {
    integration_options_o o_options;
    o_options.install_directory = s_install_dir;
    o_options.desktop_file_name = s_desktop_file_name;
    o_options.extra_exec_arguments = s_exec_args;
    o_options.startup_wm_class_override = s_wm_class;
    o_options.icon_name_override = s_icon_name;
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
    using gnome_appimage::tools::json_escape;
    const appimage_integrator_c o_integrator;
    integration_plan_o o_plan;
    const bool b_valid = o_integrator.plan(s_path, o_options, o_plan);
    std::cout << "{\"path\":\"" << json_escape(s_path) << "\""
              << ",\"name\":\"" << json_escape(o_plan.name) << "\""
              << ",\"generic_name\":\"" << json_escape(o_plan.generic_name) << "\""
              << ",\"comment\":\"" << json_escape(o_plan.comment) << "\""
              << ",\"version\":\"" << json_escape(o_plan.version) << "\""
              << ",\"version_source\":\"" << json_escape(o_plan.version_source) << "\""
              << ",\"detection\":\"" << json_escape(o_plan.detection_name) << "\""
              << ",\"file_size\":" << o_plan.file_size
              << ",\"payload_size\":" << o_plan.payload_size
              << ",\"compression\":\"" << json_escape(o_plan.compression_name) << "\""
              << ",\"update_information\":\"" << json_escape(o_plan.update_information) << "\""
              << ",\"identifier\":\"" << json_escape(o_plan.identifier) << "\""
              << ",\"desktop_id\":\"" << json_escape(o_plan.desktop_id) << "\""
              << ",\"icon_name\":\"" << json_escape(o_plan.icon_name) << "\""
              << ",\"exec\":\"" << json_escape(o_plan.exec_command) << "\""
              << ",\"startup_wm_class\":\"" << json_escape(o_plan.startup_wm_class) << "\""
              << ",\"embedded_desktop\":\"" << json_escape(o_plan.embedded_desktop_path) << "\""
              << ",\"desktop_entry\":\"" << json_escape(o_plan.desktop_entry_text) << "\""
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
        if (b_json) {
            if (!b_first) {
                std::cout << ',';
            }
            std::cout << "{\"identifier\":\""
                      << gnome_appimage::tools::json_escape(o_entry.identifier)
                      << "\",\"appimage\":\""
                      << gnome_appimage::tools::json_escape(o_entry.appimage_path)
                      << "\",\"desktop_entry\":\""
                      << gnome_appimage::tools::json_escape(o_entry.desktop_entry_path) << "\"}";
        } else {
            std::cout << o_entry.identifier << '\t' << o_entry.appimage_path << '\t'
                      << o_entry.desktop_entry_path << '\n';
        }
        b_first = false;
    }
    if (b_json) {
        std::cout << "]\n";
    }
    return EXIT_OK;
}

int command_audit(bool b_json) {
    const appimage_integrator_c o_integrator;
    const std::vector<audit_finding_o> o_findings = o_integrator.audit();
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
        o_tool.parent_path() / "icons/appimage-handler.svg",
        o_tool.parent_path() / "appimage-handler.svg",
        o_tool.parent_path().parent_path().parent_path()
            / "sources/tools/icons/appimage-handler.svg",
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

int command_handler_status(const appimage_integrator_c &o_integrator) {
    const std::vector<std::string> o_types = {"application/vnd.appimage",
                                              "application/x-appimage",
                                              "application/x-iso9660-appimage"};
    for (const std::string &s_type : o_types) {
        const std::string s_default =
            capture_command({"xdg-mime", "query", "default", s_type});
        std::cout << s_type << ": " << (s_default.empty() ? "(none)" : s_default);
        if (HANDLER_DESKTOP_ID == s_default) {
            std::cout << "  [this tool]";
        }
        std::cout << '\n';
    }
    const std::string s_desktop = handler_desktop_path(o_integrator);
    std::cout << "handler desktop entry: " << s_desktop << " ("
              << (fs::exists(s_desktop) ? "present" : "absent") << ")\n";
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

    // The launcher must carry the handler icon.
    std::ostringstream o_desktop;
    o_desktop << "[Desktop Entry]\n"
              << "Type=Application\n"
              << "Name=AppImage Handler\n"
              << "Comment=Run or integrate an AppImage\n"
              << "Exec=" << s_tool << " handle %f\n"
              << "Icon=appimage-handler\n"
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
        const std::string s_icon_target = (o_theme_directory / "appimage-handler.svg").string();
        fs::copy_file(s_icon_source, s_icon_target, fs::copy_options::overwrite_existing,
                      o_icon_error);
        if (!o_icon_error) {
            s_icon_installed = s_icon_target;
        }
    }

    // Point the AppImage MIME types at the same icon, backing up the definition.
    std::string s_mime_package;
    std::string s_mime_backup;
    const fs::path o_mime_package = o_data_home / "mime/packages/appimage.xml";
    std::error_code o_mime_error;
    if (fs::exists(o_mime_package, o_mime_error)) {
        std::string s_content = read_text_file(o_mime_package.string());
        const std::string s_old_icon = "application-x-executable";
        const std::string s_new_icon = "appimage-handler";
        if (std::string::npos != s_content.find(s_old_icon)) {
            const fs::path o_backup_directory = o_state_directory / "backup";
            fs::create_directories(o_backup_directory, o_mime_error);
            s_mime_backup = (o_backup_directory / "appimage.xml").string();
            std::error_code o_backup_error;
            if (!fs::exists(s_mime_backup, o_backup_error)) {
                write_text_file(s_mime_backup, s_content);
            }
            std::size_t i_position = 0;
            while ((i_position = s_content.find(s_old_icon, i_position)) != std::string::npos) {
                s_content.replace(i_position, s_old_icon.size(), s_new_icon);
                i_position += s_new_icon.size();
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
    // Preserve the real previous defaults when our own handler is re-installed.
    std::map<std::string, std::string> o_previous_by_type;
    {
        std::ifstream o_old_manifest((o_state_directory / HANDLER_MANIFEST).string());
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
        if (HANDLER_DESKTOP_ID == s_previous) {
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
    std::cout << "handler installed: " << s_desktop << '\n';
    if (!s_icon_installed.empty()) {
        std::cout << "handler icon: " << s_icon_installed << '\n';
    }
    std::cout << "previous defaults recorded in " << s_manifest << '\n';
    return command_handler_status(o_integrator);
}

int command_handler_uninstall(const appimage_integrator_c &o_integrator) {
    const fs::path o_state_directory(o_integrator.state_directory());
    const std::string s_manifest = (o_state_directory / HANDLER_MANIFEST).string();
    std::ifstream o_input(s_manifest);
    if (!o_input) {
        std::cerr << "error: no handler manifest at " << s_manifest << '\n';
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
    for (const std::string &s_icon : o_icons) {
        fs::remove(s_icon, o_error);
        o_error.clear();
    }
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
    fs::remove(handler_desktop_path(o_integrator), o_error);
    fs::remove(s_manifest, o_error);
    std::cout << "handler removed and previous defaults restored\n";
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
            {"notify-send", "-a", "AppImage Handler", "-u", "normal", "-t", "5000", s_title,
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

    // Prefer the GTK handler: it can remember its size and return from Inspect.
    const appimage_integrator_c o_integrator;
    const std::string s_tool = tool_path(o_integrator);
    const std::string s_ui = handler_ui_script(s_tool);
    if (!s_ui.empty() && has_display() && command_exists("python3")
        && command_succeeds({"python3", "-c",
                             "import gi; gi.require_version('Gtk','4.0'); "
                             "from gi.repository import Gtk"})) {
        std::vector<std::string> o_command = {"python3", s_ui, "--tool", s_tool, s_path};
        std::vector<char *> o_raw;
        for (const std::string &s_argument : o_command) {
            o_raw.push_back(const_cast<char *>(s_argument.c_str()));
        }
        o_raw.push_back(nullptr);
        execvp("python3", o_raw.data());
        // Fall through to the zenity flow if execvp failed.
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
    std::string s_identifier;
    bool b_move = true;
    bool b_icons = true;
    integration_conflict_policy_e e_conflict_policy = integration_conflict_policy_e::fail;
    bool b_extract_and_run = false;
    bool b_detached = false;
    bool b_json = false;
    bool b_assume_yes = false;
    bool b_remove_appimage = false;
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
            || "--icon-name" == s_argument || "--identifier" == s_argument;
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
            } else {
                s_identifier = s_value;
            }
        } else if ("--no-move" == s_argument) {
            b_move = false;
        } else if ("--no-icons" == s_argument) {
            b_icons = false;
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

    const appimage_integrator_c o_integrator;

    if ("plan" == s_command) {
        if (s_path.empty()) {
            std::cerr << "error: plan needs an AppImage path\n";
            return EXIT_USAGE;
        }
        return command_plan_or_explain(
            s_path,
            options_from(s_install_dir, s_desktop_file_name, s_exec_args, s_wm_class, s_icon_name,
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
                              s_icon_name, b_move, b_icons, e_conflict_policy));
        }
        return command_explain(
            s_path,
            options_from(s_install_dir, s_desktop_file_name, s_exec_args, s_wm_class, s_icon_name,
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
    if ("audit" == s_command) {
        return command_audit(b_json);
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
