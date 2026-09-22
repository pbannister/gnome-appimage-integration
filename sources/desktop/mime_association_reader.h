#pragma once

#include <cstddef>
#include <map>
#include <string>
#include <vector>

namespace gnome_appimage::desktop {

// One MIME-to-desktop association and where it came from.
struct mime_association_o {
    std::string mime_type;
    std::string desktop_id;
    std::string desktop_path;
    std::string source_file;
    std::string group;
    std::size_t priority = 0;
};

// The complete result of one MIME lookup.
struct mime_lookup_o {
    std::string mime_type;
    bool found = false;
    mime_association_o default_application;
    std::vector<mime_association_o> associations;
    std::vector<std::string> searched_files;
};

// Read default-application and association information following the MIME
// Applications Specification.
class mime_association_reader_c {
public:
    mime_association_reader_c();
    explicit mime_association_reader_c(const std::map<std::string, std::string> &o_environment);

    const std::vector<std::string> &configuration_files() const;

    // Resolve the desktop file for a desktop ID through the application search path.
    std::string resolve_desktop_id(const std::string &s_desktop_id) const;

    mime_lookup_o lookup(const std::string &s_mime_type) const;

private:
    std::map<std::string, std::string> o_environment_;
    std::vector<std::string> o_application_directories_;
    std::vector<std::string> o_configuration_files_;
    void build();
};

}  // namespace gnome_appimage::desktop
