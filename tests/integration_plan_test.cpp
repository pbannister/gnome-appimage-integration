// Sandboxed integration test: plan, install, and uninstall inside a temporary XDG home.
// Usage: integration-plan-test <appimage> <temporary-root>
#include "integration/appimage_integrator.h"

#include "desktop/desktop_entry_reader.h"

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <map>
#include <string>
#include <unistd.h>
#include <vector>

using gnome_appimage::desktop::desktop_entry_file_o;
using gnome_appimage::desktop::desktop_entry_reader_c;
using gnome_appimage::desktop::desktop_entry_validate;
using gnome_appimage::integration::appimage_integrator_c;
using gnome_appimage::integration::installed_appimage_o;
using gnome_appimage::integration::integration_icon_o;
using gnome_appimage::integration::integration_options_o;
using gnome_appimage::integration::integration_plan_o;

namespace {

namespace fs = std::filesystem;

int g_count_failure = 0;

void check(bool b_condition, const char *s_expression, int i_line) {
    if (!b_condition) {
        std::fprintf(stderr, "FAIL line %d: %s\n", i_line, s_expression);
        g_count_failure++;
    }
}

#define CHECK(expression) check((expression), #expression, __LINE__)

bool contains(const std::string &s_text, const std::string &s_needle) {
    return std::string::npos != s_text.find(s_needle);
}

bool is_executable(const std::string &s_path) {
    return 0 == access(s_path.c_str(), X_OK);
}

}  // namespace

int main(int i_argument_count, char **p_arguments) {
    if (3 > i_argument_count) {
        std::fprintf(stderr, "usage: integration-plan-test <appimage> <temporary-root>\n");
        return 2;
    }
    const std::string s_appimage = p_arguments[1];
    const fs::path o_root = p_arguments[2];
    const fs::path o_home = o_root / "home";
    const fs::path o_install = o_home / "Applications";

    std::error_code o_error;
    fs::create_directories(o_home / ".local/share", o_error);
    fs::create_directories(o_root / "system/share", o_error);

    std::map<std::string, std::string> o_environment;
    o_environment["HOME"] = o_home.string();
    o_environment["XDG_DATA_HOME"] = (o_home / ".local/share").string();
    o_environment["XDG_DATA_DIRS"] = (o_root / "system/share").string();

    const appimage_integrator_c o_integrator(o_environment);

    integration_options_o o_options;
    o_options.install_directory = o_install.string();

    integration_plan_o o_plan;
    CHECK(o_integrator.plan(s_appimage, o_options, o_plan));
    if (!o_plan.valid) {
        std::fprintf(stderr, "FAIL plan: %s\n", o_plan.error.c_str());
        return 1;
    }

    CHECK("org.example.Test.desktop" == o_plan.desktop_id);
    CHECK((o_install / fs::path(s_appimage).filename()).string() == o_plan.installed_path);
    CHECK(contains(o_plan.exec_command, o_plan.installed_path));
    CHECK(contains(o_plan.exec_command, "%U") || contains(o_plan.exec_command, "%F"));
    if (0 != o_plan.icon_name.compare(0, 9, "appimage_")) {
        std::fprintf(stderr, "FAIL icon-name=[%s] identifier=[%s] desktop-id=[%s]\n",
                     o_plan.icon_name.c_str(), o_plan.identifier.c_str(),
                     o_plan.desktop_id.c_str());
        g_count_failure++;
    }
    CHECK("TestApp" == o_plan.startup_wm_class);
    CHECK(1 == o_plan.mime_types.size());

    bool b_has_48 = false;
    for (const integration_icon_o &o_icon : o_plan.icons) {
        if ("48x48" == o_icon.size_directory && ".png" == o_icon.extension) {
            b_has_48 = true;
        }
    }
    CHECK(b_has_48);

    CHECK(contains(o_plan.desktop_entry_text, "[Desktop Entry]"));
    CHECK(contains(o_plan.desktop_entry_text, "Type=Application"));
    CHECK(contains(o_plan.desktop_entry_text, "Name=Test Application"));
    CHECK(contains(o_plan.desktop_entry_text, "Icon=" + o_plan.icon_name));
    CHECK(contains(o_plan.desktop_entry_text, "TryExec=" + o_plan.installed_path));
    CHECK(contains(o_plan.desktop_entry_text, "Terminal=false"));
    CHECK(contains(o_plan.desktop_entry_text, "StartupNotify=true"));
    CHECK(contains(o_plan.desktop_entry_text, "StartupWMClass=TestApp"));
    CHECK(contains(o_plan.desktop_entry_text, "Actions=Remove-AppImage;"));
    CHECK(contains(o_plan.desktop_entry_text, "[Desktop Action Remove-AppImage]"));
    CHECK(contains(o_plan.desktop_entry_text, "X-AppImage-Identifier=" + o_plan.identifier));

    std::string s_error;
    CHECK(o_integrator.install(o_plan, s_error));
    if (!s_error.empty()) {
        std::fprintf(stderr, "FAIL install: %s\n", s_error.c_str());
    }
    CHECK(fs::exists(o_plan.desktop_entry_path));
    CHECK(fs::exists(o_plan.installed_path));
    CHECK(is_executable(o_plan.installed_path));
    CHECK(fs::exists(o_plan.manifest_path));
    for (const integration_icon_o &o_icon : o_plan.icons) {
        const fs::path o_target =
            fs::path(o_integrator.icon_directory()) / o_icon.size_directory / "apps"
            / (o_plan.icon_name + o_icon.extension);
        CHECK(fs::exists(o_target));
    }

    desktop_entry_file_o o_installed;
    std::vector<gnome_appimage::desktop::desktop_entry_diagnostic_o> o_diagnostics;
    CHECK(desktop_entry_reader_c::parse_file(o_plan.desktop_entry_path, o_installed,
                                             o_diagnostics));
    CHECK(o_plan.exec_command == o_installed.value("Desktop Entry", "Exec").value_or(""));
    CHECK(o_plan.installed_path == o_installed.value("Desktop Entry", "TryExec").value_or(""));
    CHECK(o_plan.icon_name == o_installed.value("Desktop Entry", "Icon").value_or(""));
    CHECK(!o_installed.bool_value("Desktop Entry", "Terminal").value_or(true));

    const std::vector<installed_appimage_o> o_installed_list = o_integrator.list_installed();
    CHECK(1 == o_installed_list.size());
    if (1 == o_installed_list.size()) {
        CHECK(o_plan.identifier == o_installed_list[0].identifier);
    }

    CHECK(o_integrator.uninstall(o_plan.identifier, false, s_error));
    CHECK(!fs::exists(o_plan.desktop_entry_path));
    CHECK(!fs::exists(o_plan.manifest_path));
    CHECK(fs::exists(o_plan.installed_path));
    CHECK(o_integrator.list_installed().empty());

    // A second round, this time removing the AppImage itself.
    const std::string s_second = (o_root / "second.AppImage").string();
    fs::copy_file(o_plan.installed_path, s_second, o_error);
    integration_plan_o o_second_plan;
    CHECK(o_integrator.plan(s_second, o_options, o_second_plan));
    CHECK(o_integrator.install(o_second_plan, s_error));
    CHECK(o_integrator.uninstall(o_second_plan.identifier, true, s_error));
    CHECK(!fs::exists(o_second_plan.installed_path));

    if (0 != g_count_failure) {
        std::fprintf(stderr, "integration-plan-test: %d failure(s)\n", g_count_failure);
        return 1;
    }
    std::printf("integration-plan-test: all checks passed\n");
    return 0;
}
