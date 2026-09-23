#ifndef GNOME_APPIMAGE_APPIMAGE_UPDATE_INFORMATION_H
#define GNOME_APPIMAGE_APPIMAGE_UPDATE_INFORMATION_H

#include <string>
#include <vector>

namespace gnome_appimage::appimage {

// The transports the AppImage specification defines, plus the two ways a value can
// fail to be one: an unknown transport name, or no value at all.
enum class update_transport_e {
    absent,
    zsync,
    github_releases,
    pling_v1,
    bintray_zsync,
    unrecognised,
};

// An update-information string, understood.  Everything here is derived from the
// string alone: no network, and no reading of the AppImage.
//
// The string is a transport name followed by '|'-separated fields, for example
//   gh-releases-zsync|FreeCAD|FreeCAD|latest|FreeCAD*x86_64*.AppImage.zsync
//   zsync|https://example.org/Application-latest-x86_64.AppImage.zsync
struct update_information_o {
    // What was read from the .upd_info section, exactly as it was printed.
    std::string text;
    update_transport_e transport = update_transport_e::absent;
    // The transport name as it appears, so an unknown one can be quoted back.
    std::string transport_name;
    // The fields after the transport name, empty fields kept.
    std::vector<std::string> fields;

    // Whether this tool can act on the value, and if not, why.
    bool usable = false;
    std::string problem;

    // One sentence a person can read, for the activator and for explain.
    std::string description;

    // GitHub releases
    std::string repository;    // owner/repository
    std::string release;       // the tag as written, including latest/latest-pre/latest-all
    // The URL a check fetches.  For latest-pre and latest-all this is the release list,
    // which the caller reads as a list and takes the first usable entry from.
    std::string request_url;
    bool request_is_list = false;

    // The zsync file the transport names, and the AppImage beside it: the same name
    // without the .zsync suffix, wildcards kept.  Empty when the value does not name
    // a zsync file at all, as pling-v1-zsync does not.
    std::string zsync_pattern;
    std::string image_pattern;
};

// Understand one update-information value.  Never reads anything but the string.
update_information_o understand_update_information(const std::string &s_text);

// The name the specification uses for a transport, for messages and documents.
std::string update_transport_name(update_transport_e e_transport);

// Match a file name against a pattern with '*' and '?' wildcards, the way the
// specification's file-name patterns are meant.  An empty pattern matches nothing.
bool update_pattern_matches(const std::string &s_pattern, const std::string &s_name);

}  // namespace gnome_appimage::appimage

#endif
