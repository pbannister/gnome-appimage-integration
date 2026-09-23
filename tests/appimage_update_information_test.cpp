//
// appimage_update_information_test.cpp: unit tests for reading the update-information
// value an AppImage embeds, and for turning it into the URL a check would fetch.
//
#include "appimage/appimage_update_information.h"

#include <iostream>
#include <string>

using gnome_appimage::appimage::understand_update_information;
using gnome_appimage::appimage::update_information_o;
using gnome_appimage::appimage::update_pattern_matches;
using gnome_appimage::appimage::update_transport_e;

namespace {

int i_failures = 0;

void check(bool b_condition, const std::string &s_description) {
    if (!b_condition) {
        std::cerr << "FAIL: " << s_description << '\n';
        i_failures++;
    }
}

void test_github_latest() {
    const update_information_o o_information = understand_update_information(
        "gh-releases-zsync|FreeCAD|FreeCAD|latest|FreeCAD*x86_64*.AppImage.zsync");
    check(update_transport_e::github_releases == o_information.transport,
          "gh-releases-zsync must be recognised");
    check(o_information.usable, "a complete gh-releases-zsync value must be usable");
    check("FreeCAD/FreeCAD" == o_information.repository, "the repository must be owner/repo");
    check("latest" == o_information.release, "the release must be kept as written");
    check("https://api.github.com/repos/FreeCAD/FreeCAD/releases/latest"
              == o_information.request_url,
          "latest must resolve to the latest-release endpoint");
    check(!o_information.request_is_list, "latest is not a list request");
    check("FreeCAD*x86_64*.AppImage.zsync" == o_information.zsync_pattern,
          "the zsync pattern must be kept");
    check("FreeCAD*x86_64*.AppImage" == o_information.image_pattern,
          "the image pattern must drop the .zsync suffix");
    check(!o_information.description.empty(), "a usable value must be described");
}

void test_github_release_values() {
    const update_information_o o_pre = understand_update_information(
        "gh-releases-zsync|probono|AppImages|latest-pre|Subsurface-*x86_64.AppImage.zsync");
    check(o_pre.request_is_list, "latest-pre needs the release list");
    check("https://api.github.com/repos/probono/AppImages/releases?per_page=20"
              == o_pre.request_url,
          "latest-pre must resolve to the release list");
    check(!o_pre.request_url.empty() && o_pre.usable, "latest-pre must be usable");

    const update_information_o o_all = understand_update_information(
        "gh-releases-zsync|probono|AppImages|latest-all|Subsurface-*x86_64.AppImage.zsync");
    check(o_all.request_is_list, "latest-all needs the release list");

    const update_information_o o_tag = understand_update_information(
        "gh-releases-zsync|FreeCAD|FreeCAD|1.1.3|FreeCAD*x86_64*.AppImage.zsync");
    check("https://api.github.com/repos/FreeCAD/FreeCAD/releases/tags/1.1.3"
              == o_tag.request_url,
          "a specific tag must resolve to the tag endpoint");
    check(!o_tag.request_is_list, "a specific tag is not a list request");
}

void test_zsync() {
    const update_information_o o_information = understand_update_information(
        "zsync|https://example.org/Application-latest-x86_64.AppImage.zsync");
    check(update_transport_e::zsync == o_information.transport, "zsync must be recognised");
    check(o_information.usable, "an https zsync value must be usable");
    check("https://example.org/Application-latest-x86_64.AppImage.zsync"
              == o_information.request_url,
          "the zsync URL must be kept as the request URL");
    check("https://example.org/Application-latest-x86_64.AppImage" == o_information.image_pattern,
          "the image pattern must drop the .zsync suffix");

    const update_information_o o_plain =
        understand_update_information("zsync|http://server.domain/path/App-latest-x86_64.AppImage.zsync");
    check(o_plain.usable, "an http zsync URL must be usable");

    const update_information_o o_bad_scheme =
        understand_update_information("zsync|ftp://server.domain/file.zsync");
    check(!o_bad_scheme.usable, "a non-HTTP zsync URL must not be usable");
    check(!o_bad_scheme.problem.empty(), "an unusable value must say why");
}

void test_unusable_values() {
    const update_information_o o_absent = understand_update_information("");
    check(update_transport_e::absent == o_absent.transport, "an empty value is absent");
    check(!o_absent.usable, "an empty value is not usable");

    // appimagetool's --guess option has been written into .upd_info by tools that
    // mistook it for a transport; it is not one.
    const update_information_o o_guess = understand_update_information("guess");
    check(update_transport_e::unrecognised == o_guess.transport, "guess is not a transport");
    check(!o_guess.usable, "guess must not be usable");
    check("guess" == o_guess.transport_name, "the unknown transport must be quoted back");

    const update_information_o o_bintray =
        understand_update_information("bintray-zsync|user|repo|file.zsync");
    check(update_transport_e::bintray_zsync == o_bintray.transport, "bintray must be named");
    check(!o_bintray.usable, "bintray must not be usable");
    check(o_bintray.problem.find("deprecated") != std::string::npos,
          "the bintray problem must say it is deprecated");

    const update_information_o o_pling =
        understand_update_information("pling-v1-zsync|1623134|*-stable-x86_64.AppImage");
    check(update_transport_e::pling_v1 == o_pling.transport, "pling must be recognised");
    check(!o_pling.usable, "pling must not be usable yet");
    check("*-stable-x86_64.AppImage" == o_pling.image_pattern,
          "pling must keep the file pattern");

    const update_information_o o_short = understand_update_information("zsync");
    check(!o_short.usable, "a transport with no fields must not be usable");
    check(o_short.problem.find("one field") != std::string::npos,
          "the problem must count the fields the transport needs");

    const update_information_o o_github_short =
        understand_update_information("gh-releases-zsync|FreeCAD|FreeCAD|latest");
    check(!o_github_short.usable, "a three-field gh-releases-zsync must not be usable");

    const update_information_o o_github_empty =
        understand_update_information("gh-releases-zsync|FreeCAD||latest|file.zsync");
    check(!o_github_empty.usable, "an empty GitHub field must not be usable");

    // A zsync file whose name does not end in .zsync names no image pattern, but the
    // value is still a valid transport.
    const update_information_o o_odd =
        understand_update_information("zsync|https://example.org/latest");
    check(o_odd.usable, "a zsync URL without a .zsync suffix is still usable");
    check(o_odd.image_pattern.empty(), "a name without .zsync must yield no image pattern");
}

void test_pattern_matching() {
    check(update_pattern_matches("FreeCAD*x86_64*.AppImage.zsync",
                                 "FreeCAD_1.1.3-Linux-x86_64-py311.AppImage.zsync"),
          "the FreeCAD pattern must match its own release asset");
    check(update_pattern_matches("Subsurface-*x86_64.AppImage.zsync",
                                 "Subsurface-2.4.2-x86_64.AppImage.zsync"),
          "a leading literal must match");
    check(update_pattern_matches("FreeCAD*x86_64*.AppImage.zsync",
                                 "FreeCAD_1.1.3-Linux-aarch64.AppImage.zsync")
              == false,
          "the pattern must not match another architecture");
    check(update_pattern_matches("App-?.AppImage", "App-1.AppImage"),
          "? must match one character");
    check(!update_pattern_matches("App-?.AppImage", "App-12.AppImage"),
          "? must match exactly one character");
    check(update_pattern_matches("*", "anything"), "* must match everything");
    check(!update_pattern_matches("", "anything"), "an empty pattern must match nothing");
    check(update_pattern_matches("exact.zsync", "exact.zsync"), "a literal pattern must match");
    check(!update_pattern_matches("exact.zsync", "exact.zsync.extra"),
          "a literal pattern must match the whole name");
}

}  // namespace

int main() {
    test_github_latest();
    test_github_release_values();
    test_zsync();
    test_unusable_values();
    test_pattern_matching();
    if (0 != i_failures) {
        std::cerr << "appimage-update-information-test: " << i_failures << " checks failed\n";
        return 1;
    }
    std::cout << "appimage-update-information-test: all checks passed\n";
    return 0;
}
