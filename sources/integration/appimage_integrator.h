#pragma once

#include "desktop/desktop_entry_reader.h"

#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace gnome_appimage::integration {

// One file the integration will write or move.
struct integration_action_o {
    enum class kind_e {
        move_appimage,
        copy_appimage,
        install_icon,
        write_desktop_entry,
        write_mime_package,
        write_manifest,
        update_desktop_database,
        update_mime_database,
        remove_path,
        restore_mime_default,
    };

    kind_e kind = kind_e::write_desktop_entry;
    std::string source_path;
    std::string target_path;
    std::string description;
};

// One icon copied out of the payload.
struct integration_icon_o {
    std::string source_in_payload;
    std::string installed_name;
    std::string size_directory;
    std::string extension;
    std::string content;
    int pixel_size = 0;
};

// One MIME definition file copied out of the payload.
struct integration_mime_package_o {
    std::string source_in_payload;
    std::string installed_name;
    std::string content;
};

// An existing launcher that already represents the same application.
struct integration_conflict_o {
    std::string desktop_id;
    std::string path;
    std::string name;
    std::string appimage_path;
    std::string icon;
    std::string wm_class;
    std::string version;
    std::string origin;
    bool managed = false;
    bool upgrade = false;
    bool exec_exists = false;
};

// What to do when the application is already represented by another launcher.
enum class integration_conflict_policy_e {
    fail,
    replace,
    add,
};

// A complete, printable plan. Building a plan never writes to the filesystem.
struct integration_plan_o {
    bool valid = false;
    std::string error;

    std::string appimage_path;
    std::string installed_path;
    std::string identifier;
    std::string desktop_id;
    std::string desktop_entry_path;
    std::string desktop_entry_text;
    std::string manifest_path;
    std::string icon_name;
    std::string embedded_desktop_path;
    std::string startup_wm_class;
    std::string exec_command;
    std::string extra_exec_arguments;
    std::string field_code;
    std::string name;
    std::string generic_name;
    std::string comment;
    std::string version;
    std::string version_source;
    std::string detection_name;
    std::string compression_name;
    std::string update_information;
    std::uint64_t file_size = 0;
    std::uint64_t payload_size = 0;
    bool replace_conflicts = false;
    bool move_appimage = true;

    std::vector<integration_icon_o> icons;
    std::vector<std::string> mime_types;
    std::vector<integration_mime_package_o> mime_package_files;
    std::vector<integration_conflict_o> conflicts;
    std::vector<std::string> warnings;
    std::vector<std::string> notes;
    std::vector<integration_action_o> actions;
};

// User-tunable integration choices.
struct integration_options_o {
    std::string install_directory;
    std::string desktop_file_name;
    std::string extra_exec_arguments;
    std::string startup_wm_class_override;
    std::string icon_name_override;
    // Replaces the Name= written to the launcher, so several launchers for one
    // application can be told apart in the menu.
    std::string name_override;
    std::string tool_path;
    integration_conflict_policy_e conflict_policy = integration_conflict_policy_e::fail;
    bool move_appimage = true;
    bool write_icons = true;
    bool make_executable = true;
    bool update_caches = true;
};

// One installed AppImage, read back from its manifest.
struct installed_appimage_o {
    std::string identifier;
    std::string appimage_path;
    std::string desktop_entry_path;
    std::string desktop_id;
    std::string icon_name;
    std::string manifest_path;
};

// One audit finding about the real desktop.
struct audit_finding_o {
    enum class severity_e { info, warning, error };

    severity_e severity = severity_e::info;
    std::string subject;
    std::string message;
    std::string remedy;
};

// Plan, install, uninstall, and audit AppImage desktop integration.
class appimage_integrator_c {
public:
    appimage_integrator_c();
    explicit appimage_integrator_c(const std::map<std::string, std::string> &o_environment);

    // Build a plan by reading the AppImage. Never writes to the filesystem.
    bool plan(const std::string &s_appimage_path,
              const integration_options_o &o_options,
              integration_plan_o &o_plan) const;

    // Execute a plan and write the manifest that makes it reversible.
    bool install(const integration_plan_o &o_plan, std::string &s_error) const;

    // Reverse an install. Optionally delete the AppImage itself.
    bool uninstall(const std::string &s_identifier,
                   bool b_remove_appimage,
                   std::string &s_error) const;

    std::vector<installed_appimage_o> list_installed() const;

    // Inspect the real desktop and report everything that is inconsistent.
    std::vector<audit_finding_o> audit() const;

    // A human-readable report of what this AppImage is and what install would write.
    std::string describe(const std::string &s_appimage_path,
                         const integration_options_o &o_options) const;

    // A human-readable report of what an install did, or would do.
    std::string describe_plan(const integration_plan_o &o_plan) const;

    const std::vector<std::string> &application_directories() const {
        return o_application_directories_;
    }
    const std::string &state_directory() const { return s_state_directory_; }
    const std::string &icon_directory() const { return s_icon_directory_; }

private:
    void build();

    std::map<std::string, std::string> o_environment_;
    std::vector<std::string> o_application_directories_;
    std::string s_data_home_;
    std::string s_applications_directory_;
    std::string s_icon_directory_;
    std::string s_mime_packages_directory_;
    std::string s_state_directory_;
    std::string s_install_directory_;
    std::string s_tool_path_;
};

// Compute the stable identifier used to name the manifest and the icon.
std::string integration_identifier(const std::string &s_content_probe);

// Sanitize a string into a desktop-entry-safe token.
std::string integration_sanitize_token(const std::string &s_text);

}  // namespace gnome_appimage::integration
