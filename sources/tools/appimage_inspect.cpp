// appimage-inspect: read AppImage containers, their payloads, and the embedded desktop entry.
#include "appimage/appimage_reader.h"
#include "appimage/squashfs_reader.h"
#include "desktop/desktop_entry_reader.h"
#include "tools/desktop_entry_output.h"
#include "version/version.h"

#include <cstdio>
#include <iomanip>
#include <iostream>
#include <optional>
#include <ostream>
#include <string>
#include <vector>

namespace {

using gnome_appimage::appimage::appimage_detection_e;
using gnome_appimage::appimage::appimage_info_o;
using gnome_appimage::appimage::appimage_reader_c;
using gnome_appimage::appimage::squashfs_entry_o;
using gnome_appimage::appimage::squashfs_reader_c;
using gnome_appimage::desktop::desktop_entry_diagnostic_o;
using gnome_appimage::desktop::desktop_entry_file_o;
using gnome_appimage::desktop::desktop_entry_reader_c;

constexpr int EXIT_OK = 0;
constexpr int EXIT_ERROR = 1;
constexpr int EXIT_USAGE = 2;

void print_usage(std::ostream &o_out) {
    o_out << "usage: appimage-inspect [options] <path>\n"
          << "\n"
          << "  --desktop          print only the embedded desktop entry\n"
          << "  --list             print only the payload root listing\n"
          << "  --locale <locale>  select localized desktop entry values\n"
          << "  --json             print JSON\n"
          << "  --help             print this help\n"
          << "  --version          print the build-time version\n";
}

const char *elf_data_name(const appimage_info_o &o_info) {
    return o_info.elf.little_endian ? "little-endian" : "big-endian";
}

std::string magic_string(const appimage_info_o &o_info) {
    char s_buffer[16];
    std::snprintf(s_buffer, sizeof(s_buffer), "%02x %02x %02x", o_info.magic[0],
                  o_info.magic[1], o_info.magic[2]);
    return s_buffer;
}

void print_summary(const appimage_info_o &o_info,
                   const std::vector<squashfs_entry_o> &o_entries,
                   const std::string &s_payload_error,
                   const std::optional<std::string> &s_desktop_path,
                   const desktop_entry_file_o &o_desktop_file) {
    std::cout << "path: " << o_info.path << '\n';
    std::cout << "detection: " << gnome_appimage::appimage::appimage_detection_name(o_info.detection)
              << '\n';
    std::cout << "file-size: " << o_info.file_size << '\n';
    std::cout << "magic: " << magic_string(o_info) << '\n';
    std::cout << "elf-class: " << o_info.elf.elf_class << '\n';
    std::cout << "elf-data: " << elf_data_name(o_info) << '\n';
    std::cout << "machine: " << o_info.elf.machine_name << " (0x" << std::hex
              << o_info.elf.machine << std::dec << ")\n";
    std::cout << "entry-point: 0x" << std::hex << o_info.elf.entry_point << std::dec << '\n';
    std::cout << "payload-offset: " << o_info.payload_offset << '\n';
    std::cout << "payload-size: " << o_info.payload_size << '\n';

    if (o_info.update_information_section.present) {
        std::cout << "update-information: " << o_info.update_information << '\n';
    } else {
        std::cout << "update-information: (absent)\n";
    }
    if (o_info.signature_section.present) {
        std::cout << "signature: present size=" << o_info.signature_section.size
                  << " empty=" << (o_info.signature_is_empty ? "true" : "false") << '\n';
    } else {
        std::cout << "signature: (absent)\n";
    }

    if (o_info.has_squashfs) {
        std::cout << "squashfs: present\n";
        std::cout << "squashfs-compression: "
                  << gnome_appimage::appimage::squashfs_compression_name(
                         o_info.squashfs.compression)
                  << '\n';
        std::cout << "squashfs-block-size: " << o_info.squashfs.block_size << '\n';
        std::cout << "squashfs-inodes: " << o_info.squashfs.inodes << '\n';
        std::cout << "squashfs-fragments: " << o_info.squashfs.fragments << '\n';
    } else if (!s_payload_error.empty()) {
        std::cout << "squashfs: unavailable (" << s_payload_error << ")\n";
    } else {
        std::cout << "squashfs: (absent)\n";
    }

    if (!o_entries.empty()) {
        std::cout << "payload:\n";
        for (const squashfs_entry_o &o_entry : o_entries) {
            std::cout << "  " << gnome_appimage::appimage::squashfs_node_type_name(o_entry.type)
                      << ' ' << o_entry.size << ' ' << o_entry.path << '\n';
        }
    }

    if (s_desktop_path.has_value()) {
        std::cout << "embedded-desktop: " << *s_desktop_path << '\n';
        gnome_appimage::tools::print_desktop_entry_text(o_desktop_file, std::cout);
    } else {
        std::cout << "embedded-desktop: (none)\n";
    }
}

void print_json(const appimage_info_o &o_info,
                const std::vector<squashfs_entry_o> &o_entries,
                const std::string &s_payload_error,
                const std::optional<std::string> &s_desktop_path,
                const desktop_entry_file_o &o_desktop_file,
                const std::vector<desktop_entry_diagnostic_o> &o_diagnostics) {
    using gnome_appimage::tools::json_escape;
    std::cout << "{\"path\":\"" << json_escape(o_info.path) << "\"";
    std::cout << ",\"detection\":\""
              << gnome_appimage::appimage::appimage_detection_name(o_info.detection) << "\"";
    std::cout << ",\"file_size\":" << o_info.file_size;
    std::cout << ",\"magic\":\"" << magic_string(o_info) << "\"";
    std::cout << ",\"elf_class\":" << o_info.elf.elf_class;
    std::cout << ",\"elf_data\":\"" << elf_data_name(o_info) << "\"";
    std::cout << ",\"machine\":\"" << json_escape(o_info.elf.machine_name) << "\"";
    std::cout << ",\"entry_point\":" << o_info.elf.entry_point;
    std::cout << ",\"payload_offset\":" << o_info.payload_offset;
    std::cout << ",\"payload_size\":" << o_info.payload_size;
    std::cout << ",\"update_information\":\"" << json_escape(o_info.update_information) << "\"";
    std::cout << ",\"signature_present\":"
              << (o_info.signature_section.present ? "true" : "false");
    std::cout << ",\"signature_empty\":"
              << (o_info.signature_is_empty ? "true" : "false");
    if (o_info.has_squashfs) {
        std::cout << ",\"squashfs\":{\"present\":true,\"compression\":\""
                  << gnome_appimage::appimage::squashfs_compression_name(
                         o_info.squashfs.compression)
                  << "\",\"block_size\":" << o_info.squashfs.block_size
                  << ",\"inodes\":" << o_info.squashfs.inodes
                  << ",\"fragments\":" << o_info.squashfs.fragments << "}";
    } else {
        std::cout << ",\"squashfs\":{\"present\":false,\"error\":\""
                  << json_escape(s_payload_error) << "\"}";
    }
    std::cout << ",\"entries\":[";
    bool b_first = true;
    for (const squashfs_entry_o &o_entry : o_entries) {
        if (!b_first) {
            std::cout << ',';
        }
        b_first = false;
        std::cout << "{\"path\":\"" << json_escape(o_entry.path) << "\",\"name\":\""
                  << json_escape(o_entry.name) << "\",\"type\":\""
                  << gnome_appimage::appimage::squashfs_node_type_name(o_entry.type)
                  << "\",\"size\":" << o_entry.size << '}';
    }
    std::cout << ']';
    if (s_desktop_path.has_value()) {
        std::cout << ",\"desktop\":{\"path\":\"" << json_escape(*s_desktop_path)
                  << "\",\"groups\":";
        gnome_appimage::tools::print_desktop_entry_groups_json(o_desktop_file, std::cout);
        std::cout << ",\"diagnostics\":";
        gnome_appimage::tools::print_diagnostics_json(o_diagnostics, std::cout);
        std::cout << '}';
    }
    std::cout << "}\n";
}

}  // namespace

int main(int i_argument_count, char **p_arguments) {
    std::string s_path;
    std::string s_locale;
    bool b_desktop = false;
    bool b_list = false;
    bool b_json = false;

    for (int i_index = 1; i_index < i_argument_count; i_index++) {
        const std::string s_argument = p_arguments[i_index];
        if ("--desktop" == s_argument) {
            b_desktop = true;
        } else if ("--list" == s_argument) {
            b_list = true;
        } else if ("--json" == s_argument) {
            b_json = true;
        } else if ("--help" == s_argument) {
            print_usage(std::cout);
            return EXIT_OK;
        } else if ("--version" == s_argument) {
            std::cout << "appimage-inspect " << gnome_appimage::version::version_string()
                      << " (build " << gnome_appimage::version::version_build_counter() << ")\n";
            return EXIT_OK;
        } else if ("--locale" == s_argument) {
            if (i_argument_count <= i_index + 1) {
                std::cerr << "error: --locale needs a value\n";
                return EXIT_USAGE;
            }
            s_locale = p_arguments[++i_index];
        } else if (!s_argument.empty() && '-' == s_argument[0]) {
            std::cerr << "error: unknown option: " << s_argument << '\n';
            return EXIT_USAGE;
        } else if (s_path.empty()) {
            s_path = s_argument;
        } else {
            std::cerr << "error: only one path may be given\n";
            return EXIT_USAGE;
        }
    }

    if (s_path.empty()) {
        print_usage(std::cerr);
        return EXIT_USAGE;
    }

    appimage_info_o o_info;
    if (!appimage_reader_c::read(s_path, o_info)) {
        std::cerr << "error: " << o_info.error << '\n';
        return EXIT_ERROR;
    }

    std::vector<squashfs_entry_o> o_entries;
    std::optional<std::string> s_desktop_path;
    desktop_entry_file_o o_desktop_file;
    std::vector<desktop_entry_diagnostic_o> o_desktop_diagnostics;
    std::string s_payload_error = o_info.payload_error;

    if (appimage_detection_e::type2 == o_info.detection && o_info.has_squashfs) {
        squashfs_reader_c o_reader;
        std::string s_error;
        if (!o_reader.open(s_path, o_info.payload_offset, s_error)) {
            s_payload_error = s_error;
        } else {
            if (!o_reader.list_root(o_entries, s_error)) {
                s_payload_error = s_error;
            }
            std::vector<squashfs_entry_o> o_desktop_entries;
            if (o_reader.list_root_files_with_extension(".desktop", o_desktop_entries, s_error)
                && !o_desktop_entries.empty()) {
                s_desktop_path = o_desktop_entries[0].path;
                std::string s_content;
                if (o_reader.read_file(o_desktop_entries[0].path, s_content, s_error)) {
                    desktop_entry_reader_c::parse_text(s_content, o_desktop_file,
                                                       o_desktop_diagnostics);
                } else {
                    s_payload_error = s_error;
                }
            }
        }
    }

    if (b_desktop) {
        if (!s_desktop_path.has_value()) {
            std::cerr << "error: no embedded desktop entry found\n";
            return EXIT_ERROR;
        }
        gnome_appimage::tools::print_desktop_entry_text(o_desktop_file, std::cout);
        gnome_appimage::tools::print_desktop_entry_resolved(o_desktop_file, s_locale, std::cout);
        gnome_appimage::tools::print_diagnostics(o_desktop_diagnostics, std::cerr);
        return gnome_appimage::tools::has_error_diagnostic(o_desktop_diagnostics) ? EXIT_ERROR
                                                                                 : EXIT_OK;
    }
    if (b_list) {
        for (const squashfs_entry_o &o_entry : o_entries) {
            std::cout << gnome_appimage::appimage::squashfs_node_type_name(o_entry.type) << '\t'
                      << o_entry.size << '\t' << o_entry.path << '\n';
        }
        return EXIT_OK;
    }
    if (b_json) {
        print_json(o_info, o_entries, s_payload_error, s_desktop_path, o_desktop_file,
                   o_desktop_diagnostics);
        return EXIT_OK;
    }

    print_summary(o_info, o_entries, s_payload_error, s_desktop_path, o_desktop_file);
    return EXIT_OK;
}
