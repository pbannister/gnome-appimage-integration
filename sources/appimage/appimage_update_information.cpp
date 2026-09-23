#include "appimage/appimage_update_information.h"

#include <cstddef>

namespace gnome_appimage::appimage {

namespace {

constexpr std::size_t COUNT_ZSYNC_FIELDS = 1;
constexpr std::size_t COUNT_GITHUB_FIELDS = 4;
constexpr std::size_t COUNT_PLING_FIELDS = 2;

std::vector<std::string> split_fields(const std::string &s_text) {
    std::vector<std::string> o_fields;
    std::size_t i_start = 0;
    while (i_start <= s_text.size()) {
        const std::size_t i_end = s_text.find('|', i_start);
        o_fields.push_back(std::string::npos == i_end
                               ? s_text.substr(i_start)
                               : s_text.substr(i_start, i_end - i_start));
        if (std::string::npos == i_end) {
            break;
        }
        i_start = i_end + 1;
    }
    return o_fields;
}

bool is_http_url(const std::string &s_url) {
    return 0 == s_url.compare(0, 7, "http://") || 0 == s_url.compare(0, 8, "https://");
}

// The zsync file the transport names is the AppImage's own name plus ".zsync", so the
// image pattern is the same pattern with that suffix removed.
std::string image_pattern_of(const std::string &s_zsync_pattern) {
    const std::string s_suffix = ".zsync";
    if (s_zsync_pattern.size() <= s_suffix.size()
        || 0 != s_zsync_pattern.compare(s_zsync_pattern.size() - s_suffix.size(),
                                        s_suffix.size(), s_suffix)) {
        return {};
    }
    return s_zsync_pattern.substr(0, s_zsync_pattern.size() - s_suffix.size());
}

std::string release_description(const std::string &s_release) {
    if ("latest" == s_release) {
        return "the latest release";
    }
    if ("latest-pre" == s_release) {
        return "the latest prerelease";
    }
    if ("latest-all" == s_release) {
        return "the latest release or prerelease";
    }
    return "release " + s_release;
}

void describe_github(update_information_o &o_information) {
    o_information.repository = o_information.fields[0] + "/" + o_information.fields[1];
    o_information.release = o_information.fields[2];
    o_information.zsync_pattern = o_information.fields[3];
    o_information.image_pattern = image_pattern_of(o_information.zsync_pattern);

    const std::string s_api = "https://api.github.com/repos/" + o_information.repository
                              + "/releases/";
    if ("latest" == o_information.release) {
        o_information.request_url = s_api + "latest";
    } else if ("latest-pre" == o_information.release
               || "latest-all" == o_information.release) {
        // GitHub marks prereleases on the list endpoint; there is no latest-prerelease
        // endpoint, so the caller reads the list and takes the first one that fits.
        o_information.request_url = "https://api.github.com/repos/" + o_information.repository
                                    + "/releases?per_page=20";
        o_information.request_is_list = true;
    } else {
        o_information.request_url = s_api + "tags/" + o_information.release;
    }
    o_information.description = "GitHub releases in " + o_information.repository + ", "
                                + release_description(o_information.release) + ", file "
                                + o_information.zsync_pattern;
}

}  // namespace

std::string update_transport_name(update_transport_e e_transport) {
    switch (e_transport) {
    case update_transport_e::absent:
        return {};
    case update_transport_e::zsync:
        return "zsync";
    case update_transport_e::github_releases:
        return "gh-releases-zsync";
    case update_transport_e::pling_v1:
        return "pling-v1-zsync";
    case update_transport_e::bintray_zsync:
        return "bintray-zsync";
    case update_transport_e::unrecognised:
        return {};
    }
    return {};
}

update_information_o understand_update_information(const std::string &s_text) {
    update_information_o o_information;
    o_information.text = s_text;
    if (s_text.empty()) {
        o_information.problem = "there is no update information";
        return o_information;
    }

    const std::vector<std::string> o_fields = split_fields(s_text);
    o_information.transport_name = o_fields[0];
    o_information.fields.assign(o_fields.begin() + 1, o_fields.end());

    if ("zsync" == o_information.transport_name) {
        o_information.transport = update_transport_e::zsync;
        if (COUNT_ZSYNC_FIELDS != o_information.fields.size()) {
            o_information.problem = "zsync needs one field, the .zsync URL, but the value has "
                                    + std::to_string(o_information.fields.size());
            return o_information;
        }
        o_information.request_url = o_information.fields[0];
        o_information.zsync_pattern = o_information.fields[0];
        o_information.image_pattern = image_pattern_of(o_information.zsync_pattern);
        if (!is_http_url(o_information.request_url)) {
            o_information.problem = "the zsync URL must be http or https, but it is \""
                                    + o_information.request_url + "\"";
            return o_information;
        }
        o_information.usable = true;
        o_information.description = "zsync from " + o_information.request_url;
        return o_information;
    }

    if ("gh-releases-zsync" == o_information.transport_name) {
        o_information.transport = update_transport_e::github_releases;
        if (COUNT_GITHUB_FIELDS != o_information.fields.size()) {
            o_information.problem =
                "gh-releases-zsync needs four fields (user, repository, release, filename), "
                "but the value has " + std::to_string(o_information.fields.size());
            return o_information;
        }
        for (const std::string &s_field : o_information.fields) {
            if (s_field.empty()) {
                o_information.problem =
                    "gh-releases-zsync has an empty field, so it names no release to check";
                return o_information;
            }
        }
        describe_github(o_information);
        o_information.usable = true;
        return o_information;
    }

    if ("pling-v1-zsync" == o_information.transport_name) {
        o_information.transport = update_transport_e::pling_v1;
        if (COUNT_PLING_FIELDS != o_information.fields.size()) {
            o_information.problem =
                "pling-v1-zsync needs two fields (product id, file pattern), but the value has "
                + std::to_string(o_information.fields.size());
            return o_information;
        }
        o_information.image_pattern = o_information.fields[1];
        o_information.problem = "this tool cannot check pling-v1-zsync yet";
        o_information.description = "Pling product " + o_information.fields[0] + ", file "
                                    + o_information.image_pattern;
        return o_information;
    }

    if ("bintray-zsync" == o_information.transport_name) {
        o_information.transport = update_transport_e::bintray_zsync;
        o_information.problem =
            "the bintray-zsync transport is deprecated: Bintray was shut down in 2021";
        o_information.description = "bintray-zsync (deprecated)";
        return o_information;
    }

    o_information.transport = update_transport_e::unrecognised;
    o_information.problem = "\"" + o_information.transport_name
                            + "\" is not a transport the AppImage specification defines";
    o_information.description = o_information.transport_name;
    return o_information;
}

bool update_pattern_matches(const std::string &s_pattern, const std::string &s_name) {
    if (s_pattern.empty()) {
        return false;
    }
    // '*' matches any run, '?' one character, everything else literally.
    std::size_t i_pattern = 0;
    std::size_t i_name = 0;
    std::size_t i_star = std::string::npos;
    std::size_t i_restart = 0;
    while (i_name < s_name.size()) {
        if (i_pattern < s_pattern.size()
            && ('?' == s_pattern[i_pattern] || s_pattern[i_pattern] == s_name[i_name])) {
            i_pattern++;
            i_name++;
            continue;
        }
        if (i_pattern < s_pattern.size() && '*' == s_pattern[i_pattern]) {
            i_star = i_pattern;
            i_restart = i_name;
            i_pattern++;
            continue;
        }
        if (std::string::npos != i_star) {
            i_pattern = i_star + 1;
            i_restart++;
            i_name = i_restart;
            continue;
        }
        return false;
    }
    while (i_pattern < s_pattern.size() && '*' == s_pattern[i_pattern]) {
        i_pattern++;
    }
    return i_pattern == s_pattern.size();
}

}  // namespace gnome_appimage::appimage
