// Portable unit tests for the AppImage container reader.
// Usage: appimage-reader-test <elf-path> <temporary-dir> [squashfs-path]
#include "appimage/appimage_reader.h"

#include <cstdio>
#include <fstream>
#include <string>

using gnome_appimage::appimage::appimage_cpu_check;
using gnome_appimage::appimage::appimage_cpu_check_o;
using gnome_appimage::appimage::appimage_cpu_compatibility_e;
using gnome_appimage::appimage::appimage_detection_e;
using gnome_appimage::appimage::appimage_elf_info_o;
using gnome_appimage::appimage::appimage_info_o;
using gnome_appimage::appimage::host_machine_name;
using gnome_appimage::appimage::appimage_reader_c;

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

bool write_file_bytes(const std::string &s_path, const std::string &s_data) {
    std::ofstream o_output(s_path, std::ios::binary | std::ios::trunc);
    if (!o_output) {
        return false;
    }
    o_output.write(s_data.data(), static_cast<std::streamsize>(s_data.size()));
    return o_output.good();
}

void patch_appimage_magic(std::string &s_data) {
    if (11 <= s_data.size()) {
        s_data[8] = static_cast<char>(0x41);
        s_data[9] = static_cast<char>(0x49);
        s_data[10] = static_cast<char>(0x02);
    }
}

}  // namespace

int main(int i_argument_count, char **p_arguments) {
    if (3 > i_argument_count) {
        std::fprintf(stderr,
                     "usage: appimage-reader-test <elf-path> <temporary-dir> [squashfs-path]\n");
        return 2;
    }
    const std::string s_elf_path = p_arguments[1];
    const std::string s_temporary = p_arguments[2];

    appimage_info_o o_info;

    // A text file is not an ELF and is rejected.
    const std::string s_text_path = s_temporary + "/plain.txt";
    CHECK(write_file_bytes(s_text_path, "not an elf\n"));
    CHECK(!appimage_reader_c::read(s_text_path, o_info));
    CHECK(appimage_detection_e::not_elf == o_info.detection);
    CHECK(!o_info.error.empty());

    // A plain ELF is read and reported as an ELF without the AppImage magic.
    std::string s_elf;
    if (!read_file_bytes(s_elf_path, s_elf)) {
        std::fprintf(stderr, "FAIL cannot read ELF source: %s\n", s_elf_path.c_str());
        return 2;
    }
    const std::string s_plain_path = s_temporary + "/plain.elf";
    CHECK(write_file_bytes(s_plain_path, s_elf));
    CHECK(appimage_reader_c::read(s_plain_path, o_info));
    CHECK(appimage_detection_e::elf_without_appimage_magic == o_info.detection);
    CHECK(0 < o_info.elf.elf_size);
    CHECK(32 == o_info.elf.elf_class || 64 == o_info.elf.elf_class);
    CHECK(o_info.payload_offset == o_info.elf.elf_size);
    CHECK(!o_info.elf.machine_name.empty());

    // The host's own machine is compatible with itself.
    const std::string s_host_machine = host_machine_name();
    const appimage_cpu_check_o o_host_cpu = appimage_cpu_check(o_info.elf);
    CHECK(appimage_cpu_compatibility_e::compatible == o_host_cpu.compatibility);

    // A foreign machine is incompatible and names both sides.
    appimage_elf_info_o o_foreign = o_info.elf;
    if ("x86-64" == s_host_machine) {
        o_foreign.machine = 0xb7;
        o_foreign.machine_name = "aarch64";
    } else if ("aarch64" == s_host_machine) {
        o_foreign.machine = 0x3e;
        o_foreign.machine_name = "x86-64";
    } else {
        o_foreign.machine_name = "unknown";
    }
    const appimage_cpu_check_o o_foreign_cpu = appimage_cpu_check(o_foreign);
    if ("unknown" == s_host_machine || "unknown" == o_foreign.machine_name) {
        CHECK(appimage_cpu_compatibility_e::unknown == o_foreign_cpu.compatibility);
    } else {
        CHECK(appimage_cpu_compatibility_e::incompatible == o_foreign_cpu.compatibility);
        CHECK(!o_foreign_cpu.problem.empty());
    }

    // Byte order is part of the machine, even when the machine name matches.
    appimage_elf_info_o o_endian = o_info.elf;
    o_endian.little_endian = !o_info.elf.little_endian;
    const appimage_cpu_check_o o_endian_cpu = appimage_cpu_check(o_endian);
    CHECK(appimage_cpu_compatibility_e::incompatible == o_endian_cpu.compatibility);
    CHECK(std::string::npos != o_endian_cpu.problem.find("endian"));

    // A 32-bit build on a 64-bit host of the same family is allowed with a note.
    appimage_elf_info_o o_compat = o_info.elf;
    if ("x86-64" == s_host_machine) {
        o_compat.machine = 0x03;
        o_compat.machine_name = "x86";
        o_compat.elf_class = 32;
    } else if ("aarch64" == s_host_machine) {
        o_compat.machine = 0x28;
        o_compat.machine_name = "arm";
        o_compat.elf_class = 32;
    } else {
        o_compat.elf_class = 0;
    }
    if (0 != o_compat.elf_class) {
        const appimage_cpu_check_o o_compat_cpu = appimage_cpu_check(o_compat);
        CHECK(appimage_cpu_compatibility_e::compatible_with_compat == o_compat_cpu.compatibility);
        CHECK(!o_compat_cpu.note.empty());
    }

    const std::uint64_t u_payload_offset = o_info.payload_offset;

    // A patched ELF with appended zero padding is detected as type 2 but has no SquashFS.
    std::string s_fake = s_elf;
    patch_appimage_magic(s_fake);
    if (s_fake.size() < u_payload_offset) {
        s_fake.append(static_cast<std::size_t>(u_payload_offset - s_fake.size()), '\0');
    }
    s_fake.append(4096, '\0');
    const std::string s_fake_path = s_temporary + "/fake.AppImage";
    CHECK(write_file_bytes(s_fake_path, s_fake));
    CHECK(appimage_reader_c::read(s_fake_path, o_info));
    CHECK(appimage_detection_e::type2 == o_info.detection);
    CHECK(u_payload_offset == o_info.payload_offset);
    CHECK(4096 == o_info.payload_size);
    CHECK(!o_info.has_squashfs);
    CHECK(!o_info.payload_error.empty());

    // With a real SquashFS payload the container reports the payload superblock.
    if (4 <= i_argument_count) {
        std::string s_squashfs;
        if (!read_file_bytes(p_arguments[3], s_squashfs)) {
            std::fprintf(stderr, "FAIL cannot read squashfs source\n");
            g_count_failure++;
        } else {
            std::string s_full = s_elf;
            patch_appimage_magic(s_full);
            if (s_full.size() < u_payload_offset) {
                s_full.append(static_cast<std::size_t>(u_payload_offset - s_full.size()), '\0');
            }
            s_full.append(s_squashfs);
            const std::string s_appimage_path = s_temporary + "/real.AppImage";
            CHECK(write_file_bytes(s_appimage_path, s_full));
            CHECK(appimage_reader_c::read(s_appimage_path, o_info));
            CHECK(appimage_detection_e::type2 == o_info.detection);
            CHECK(u_payload_offset == o_info.payload_offset);
            CHECK(s_squashfs.size() == o_info.payload_size);
            CHECK(o_info.has_squashfs);
            CHECK(0x73717368U == o_info.squashfs.magic);
            CHECK(4 == o_info.squashfs.major);
            CHECK(0 < o_info.squashfs.block_size);
        }
    }

    if (0 != g_count_failure) {
        std::fprintf(stderr, "appimage-reader-test: %d failure(s)\n", g_count_failure);
        return 1;
    }
    std::printf("appimage-reader-test: all checks passed\n");
    return 0;
}
