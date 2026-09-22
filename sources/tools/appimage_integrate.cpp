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
#include <sstream>
#include <string>
#include <sys/stat.h>
#include <unistd.h>
#include <vector>

namespace {

namespace fs = std::filesystem;

using gnome_appimage::integration::appimage_integrator_c;
using gnome_appimage::integration::audit_finding_o;
using gnome_appimage::integration::installed_appimage_o;
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
          << "  --extract-and-run         for run: force APPIMAGE_EXTRACT_AND_RUN=1\n"
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
                                  bool b_icons) {
    integration_options_o o_options;
    o_options.install_directory = s_install_dir;
    o_options.desktop_file_name = s_desktop_file_name;
    o_options.extra_exec_arguments = s_exec_args;
    o_options.startup_wm_class_override = s_wm_class;
    o_options.icon_name_override = s_icon_name;
    o_options.move_appimage = b_move;
    o_options.write_icons = b_icons;
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
    fs::create_directories(o_integrator.state_directory(), o_error);

    const std::vector<std::string> o_types = {"application/vnd.appimage",
                                              "application/x-appimage",
                                              "application/x-iso9660-appimage"};

    std::ostringstream o_manifest;
    o_manifest << "handler_desktop=" << handler_desktop_path(o_integrator) << '\n';
    for (const std::string &s_type : o_types) {
        const std::string s_previous = capture_command({"xdg-mime", "query", "default", s_type});
        o_manifest << "previous_default=" << s_type << '\t' << s_previous << '\n';
    }
    const std::string s_manifest =
        (fs::path(o_integrator.state_directory()) / HANDLER_MANIFEST).string();
    std::ofstream o_output(s_manifest, std::ios::trunc);
    o_output << o_manifest.str();
    o_output.close();

    std::ostringstream o_desktop;
    o_desktop << "[Desktop Entry]\n"
              << "Type=Application\n"
              << "Name=AppImage Handler\n"
              << "Comment=Run or integrate an AppImage\n"
              << "Exec=" << tool_path(o_integrator) << " handle %f\n"
              << "Icon=application-x-executable\n"
              << "Terminal=false\n"
              << "NoDisplay=true\n"
              << "MimeType=application/vnd.appimage;application/x-appimage;"
                 "application/x-iso9660-appimage;\n"
              << "X-Integrated-By=gnome-appimage-integration\n"
              << "X-Integrated-At=" << gnome_appimage::version::version_string() << '\n';
    const std::string s_desktop = handler_desktop_path(o_integrator);
    std::ofstream o_writer(s_desktop, std::ios::trunc);
    o_writer << o_desktop.str();
    o_writer.close();

    std::vector<std::string> o_command = {"xdg-mime", "default", HANDLER_DESKTOP_ID};
    for (const std::string &s_type : o_types) {
        o_command.push_back(s_type);
    }
    const std::string s_result = capture_command(o_command);
    static_cast<void>(s_result);
    if (command_exists("update-desktop-database")) {
        std::vector<std::string> o_update = {"update-desktop-database",
                                             o_integrator.application_directories()[0]};
        const std::string s_update = capture_command(o_update);
        static_cast<void>(s_update);
    }
    std::cout << "handler installed: " << s_desktop << '\n'
              << "previous defaults recorded in " << s_manifest << '\n';
    return command_handler_status(o_integrator);
}

int command_handler_uninstall(const appimage_integrator_c &o_integrator) {
    const std::string s_manifest =
        (fs::path(o_integrator.state_directory()) / HANDLER_MANIFEST).string();
    std::ifstream o_input(s_manifest);
    if (!o_input) {
        std::cerr << "error: no handler manifest at " << s_manifest << '\n';
        return EXIT_ERROR;
    }
    std::string s_line;
    while (std::getline(o_input, s_line)) {
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
    std::error_code o_error;
    fs::remove(handler_desktop_path(o_integrator), o_error);
    fs::remove(s_manifest, o_error);
    std::cout << "handler removed and previous defaults restored\n";
    return command_handler_status(o_integrator);
}

int command_handle(const std::string &s_path) {
    const std::string s_name = embedded_name(s_path);
    const std::string s_label = s_name.empty() ? fs::path(s_path).filename().string() : s_name;

    if (command_exists("zenity")
        && (nullptr != std::getenv("DISPLAY") || nullptr != std::getenv("WAYLAND_DISPLAY"))) {
        const std::string s_choice = capture_command(
            {"zenity", "--list", "--radiolist", "--title=AppImage",
             "--text=What do you want to do with " + s_label + "?", "--column=",
             "--column=Action", "TRUE", "Run once", "FALSE", "Integrate", "FALSE",
             "Inspect", "FALSE", "Cancel", "--height=260", "--width=420"});
        if ("Integrate" == s_choice) {
            const appimage_integrator_c o_integrator;
            integration_options_o o_options;
            o_options.tool_path = tool_path(o_integrator);
            integration_plan_o o_plan;
            if (!o_integrator.plan(s_path, o_options, o_plan)) {
                capture_command({"zenity", "--error", "--text=" + o_plan.error});
                return EXIT_ERROR;
            }
            std::string s_error;
            if (!o_integrator.install(o_plan, s_error)) {
                capture_command({"zenity", "--error", "--text=" + s_error});
                return EXIT_ERROR;
            }
            capture_command({"zenity", "--info",
                             "--text=Integrated " + s_label + " as " + o_plan.desktop_id});
            return command_run(o_plan.installed_path, {}, false);
        }
        if ("Inspect" == s_choice) {
            const std::string s_report =
                capture_command({"appimage-inspect", "--desktop", s_path});
            capture_command({"zenity", "--text-info", "--title=" + s_label, "--width=700",
                             "--height=500", "--text=" + s_report});
            return EXIT_OK;
        }
        if ("Cancel" == s_choice || s_choice.empty()) {
            return EXIT_OK;
        }
        return command_run(s_path, {}, false);
    }

    std::cout << "AppImage: " << s_label << '\n'
              << "  appimage-integrate run \"" << s_path << "\"\n"
              << "  appimage-integrate install \"" << s_path << "\"\n"
              << "  appimage-inspect \"" << s_path << "\"\n";
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
    bool b_extract_and_run = false;
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
        } else if ("--extract-and-run" == s_argument) {
            b_extract_and_run = true;
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

    if ("plan" == s_command || "explain" == s_command) {
        if (s_path.empty()) {
            std::cerr << "error: " << s_command << " needs an AppImage path\n";
            return EXIT_USAGE;
        }
        return command_plan_or_explain(
            s_path,
            options_from(s_install_dir, s_desktop_file_name, s_exec_args, s_wm_class, s_icon_name,
                          b_move, b_icons),
            b_json);
    }
    if ("install" == s_command) {
        if (s_path.empty()) {
            std::cerr << "error: install needs an AppImage path\n";
            return EXIT_USAGE;
        }
        return command_install(
            s_path,
            options_from(s_install_dir, s_desktop_file_name, s_exec_args, s_wm_class, s_icon_name,
                          b_move, b_icons),
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
