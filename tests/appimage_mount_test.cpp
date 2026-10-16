// Unit tests for the verdict on whether an AppImage can mount its payload here.
//
// The verdict takes the image's needs and the host's state as arguments, so every
// combination is exercised without a FUSE device, which no test can conjure and a machine
// without one cannot satisfy.
//
// Usage: appimage-mount-test
#include "integration/appimage_integrator.h"

#include <cstdio>
#include <string>
#include <vector>

using gnome_appimage::integration::appimage_mount_needs_o;
using gnome_appimage::integration::appimage_mount_problem;

namespace {

int g_count_failure = 0;

void check(bool b_condition, const char *s_expression, int i_line) {
    if (!b_condition) {
        std::fprintf(stderr, "FAIL line %d: %s\n", i_line, s_expression);
        g_count_failure++;
    }
}

#define CHECK(expression) check((expression), #expression, __LINE__)

bool mentions(const std::string &s_text, const std::string &s_needle) {
    return std::string::npos != s_text.find(s_needle);
}

appimage_mount_needs_o mounting(const std::vector<std::string> &o_libraries) {
    appimage_mount_needs_o o_needs;
    o_needs.mounts = true;
    o_needs.libraries = o_libraries;
    return o_needs;
}

}  // namespace

int main() {
    const std::vector<std::string> o_none;

    // A file that does not mount has nothing to say, however bare the machine is.
    {
        appimage_mount_needs_o o_needs;
        o_needs.mounts = false;
        CHECK(appimage_mount_problem(o_needs, o_none, false, false).empty());
    }

    // A mount with everything in place is silent.
    CHECK(appimage_mount_problem(mounting(o_none), o_none, true, true).empty());
    CHECK(appimage_mount_problem(mounting({"libfuse.so.2"}), o_none, true, true).empty());

    // The device is the one that matters most: without it nothing mounts.
    {
        const std::string s_problem =
            appimage_mount_problem(mounting(o_none), o_none, false, true);
        CHECK(!s_problem.empty());
        CHECK(mentions(s_problem, "/dev/fuse"));
        CHECK(mentions(s_problem, "APPIMAGE_EXTRACT_AND_RUN=1"));
    }

    // An older runtime's library, named.
    {
        const std::string s_problem =
            appimage_mount_problem(mounting({"libfuse.so.2"}), {"libfuse.so.2"}, true, true);
        CHECK(!s_problem.empty());
        CHECK(mentions(s_problem, "libfuse.so.2"));
        CHECK(!mentions(s_problem, "/dev/fuse"));
    }

    // No helper to do the mounting with.
    {
        const std::string s_problem =
            appimage_mount_problem(mounting(o_none), o_none, true, false);
        CHECK(!s_problem.empty());
        CHECK(mentions(s_problem, "fusermount"));
    }

    // Everything at once names everything, and reads as a list rather than a run-on.
    {
        const std::string s_problem =
            appimage_mount_problem(mounting({"libfuse.so.2"}), {"libfuse.so.2"}, false, false);
        CHECK(mentions(s_problem, "/dev/fuse"));
        CHECK(mentions(s_problem, "libfuse.so.2"));
        CHECK(mentions(s_problem, "fusermount"));
        CHECK(mentions(s_problem, " and "));
    }

    // The sentence says the launcher is still written, because integration is files and
    // works either way: this is about the click, not about the install.
    {
        const std::string s_problem =
            appimage_mount_problem(mounting(o_none), o_none, false, false);
        CHECK(mentions(s_problem, "launcher is still written"));
    }

    if (0 != g_count_failure) {
        std::fprintf(stderr, "appimage-mount-test: %d failure(s)\n", g_count_failure);
        return 1;
    }
    std::printf("appimage-mount-test: all checks passed\n");
    return 0;
}
