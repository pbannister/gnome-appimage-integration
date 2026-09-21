// Tool-gated tests for the SquashFS payload reader.
// Usage: squashfs-reader-test <squashfs-image> <source-dir> <expected-compression>
#include "appimage/squashfs_reader.h"

#include <cstdio>
#include <fstream>
#include <string>
#include <vector>

using gnome_appimage::appimage::squashfs_entry_o;
using gnome_appimage::appimage::squashfs_node_type_e;
using gnome_appimage::appimage::squashfs_reader_c;
using gnome_appimage::appimage::squashfs_stat_o;
using gnome_appimage::appimage::squashfs_compression_name;

namespace {

int g_count_failure = 0;

void check(bool b_condition, const char *s_expression, int i_line) {
    if (!b_condition) {
        std::fprintf(stderr, "FAIL line %d: %s\n", i_line, s_expression);
        g_count_failure++;
    }
}

#define CHECK(expression) check((expression), #expression, __LINE__)

bool read_file_bytes(const std::string &s_path, std::string &s_data) {
    std::ifstream o_input(s_path, std::ios::binary);
    if (!o_input) {
        return false;
    }
    s_data.assign(std::istreambuf_iterator<char>(o_input), std::istreambuf_iterator<char>());
    return true;
}

const squashfs_entry_o *find_entry(const std::vector<squashfs_entry_o> &o_entries,
                                   const std::string &s_name) {
    for (const squashfs_entry_o &o_entry : o_entries) {
        if (o_entry.name == s_name) {
            return &o_entry;
        }
    }
    return nullptr;
}

void compare_file(squashfs_reader_c &o_reader,
                  const std::string &s_image_path,
                  const std::string &s_source_path) {
    std::string s_expected;
    if (!read_file_bytes(s_source_path, s_expected)) {
        std::fprintf(stderr, "FAIL cannot read source file: %s\n", s_source_path.c_str());
        g_count_failure++;
        return;
    }
    std::string s_actual;
    std::string s_error;
    if (!o_reader.read_file(s_image_path, s_actual, s_error)) {
        std::fprintf(stderr, "FAIL read_file(%s): %s\n", s_image_path.c_str(), s_error.c_str());
        g_count_failure++;
        return;
    }
    check(s_actual.size() == s_expected.size(), "extracted size matches source", __LINE__);
    check(s_actual == s_expected, "extracted content matches source", __LINE__);
}

}  // namespace

int main(int i_argument_count, char **p_arguments) {
    if (4 > i_argument_count) {
        std::fprintf(stderr,
                     "usage: squashfs-reader-test <squashfs-image> <source-dir> "
                     "<expected-compression>\n");
        return 2;
    }
    const std::string s_image = p_arguments[1];
    const std::string s_source = p_arguments[2];
    const std::string s_expected_compression = p_arguments[3];

    squashfs_reader_c o_reader;
    std::string s_error;
    CHECK(o_reader.open(s_image, 0, s_error));
    CHECK(o_reader.is_open());
    CHECK(o_reader.superblock().valid);
    CHECK(0x73717368U == o_reader.superblock().magic);
    CHECK(4 == o_reader.superblock().major);
    CHECK(131072 == o_reader.superblock().block_size);
    CHECK(s_expected_compression
          == squashfs_compression_name(o_reader.superblock().compression));

    std::vector<squashfs_entry_o> o_root_entries;
    CHECK(o_reader.list_root(o_root_entries, s_error));
    CHECK(nullptr != find_entry(o_root_entries, "test.desktop"));
    CHECK(nullptr != find_entry(o_root_entries, "large.bin"));
    CHECK(nullptr != find_entry(o_root_entries, ".DirIcon"));
    CHECK(nullptr != find_entry(o_root_entries, "link.desktop"));

    const squashfs_entry_o *p_usr = find_entry(o_root_entries, "usr");
    CHECK(nullptr != p_usr);
    if (nullptr != p_usr) {
        CHECK(squashfs_node_type_e::directory == p_usr->type);
    }

    compare_file(o_reader, "/test.desktop", s_source + "/test.desktop");
    compare_file(o_reader, "/large.bin", s_source + "/large.bin");
    compare_file(o_reader, "/.DirIcon", s_source + "/.DirIcon");
    compare_file(o_reader, "/usr/share/icons/hicolor/256x256/apps/fooview.png",
                 s_source + "/usr/share/icons/hicolor/256x256/apps/fooview.png");

    std::vector<squashfs_entry_o> o_share_entries;
    CHECK(o_reader.list_directory("/usr/share", o_share_entries, s_error));
    CHECK(nullptr != find_entry(o_share_entries, "icons"));

    squashfs_stat_o o_stat;
    CHECK(o_reader.stat("/test.desktop", o_stat, s_error));
    CHECK(squashfs_node_type_e::regular_file == o_stat.type);
    CHECK(nullptr != find_entry(o_root_entries, "test.desktop"));

    squashfs_stat_o o_link_stat;
    CHECK(o_reader.stat("/link.desktop", o_link_stat, s_error));
    CHECK(squashfs_node_type_e::symlink == o_link_stat.type);

    std::vector<squashfs_entry_o> o_desktop_entries;
    CHECK(o_reader.list_root_files_with_extension(".desktop", o_desktop_entries, s_error));
    CHECK(1 == o_desktop_entries.size());
    if (1 == o_desktop_entries.size()) {
        CHECK("test.desktop" == o_desktop_entries[0].name);
    }

    std::string s_data;
    CHECK(!o_reader.read_file("/does-not-exist", s_data, s_error));

    if (0 != g_count_failure) {
        std::fprintf(stderr, "squashfs-reader-test: %d failure(s)\n", g_count_failure);
        return 1;
    }
    std::printf("squashfs-reader-test: all checks passed (%s)\n",
                s_expected_compression.c_str());
    return 0;
}
