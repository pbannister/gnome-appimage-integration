#include "appimage/appimage_reader.h"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <vector>

namespace gnome_appimage::appimage {

namespace {

constexpr std::uint8_t ELF_MAGIC_0 = 0x7f;
constexpr std::uint8_t ELF_CLASS_32 = 1;
constexpr std::uint8_t ELF_CLASS_64 = 2;
constexpr std::uint8_t ELF_DATA_LITTLE = 1;
constexpr std::uint8_t ELF_DATA_BIG = 2;
constexpr std::size_t ELF_HEADER_SIZE = 64;
constexpr std::uint16_t SECTION_INDEX_XINDEX = 0xffff;
constexpr std::uint32_t MAX_SECTION_COUNT = 100000;
constexpr std::uint32_t MAX_SIGNATURE_CHECK_SIZE = 1024U * 1024U;

constexpr std::uint8_t APPIMAGE_MAGIC_0 = 0x41;
constexpr std::uint8_t APPIMAGE_MAGIC_1 = 0x49;
constexpr std::uint8_t APPIMAGE_TYPE_1 = 0x01;
constexpr std::uint8_t APPIMAGE_TYPE_2 = 0x02;

struct file_reader_o {
    std::FILE *p_file = nullptr;
    std::uint64_t u_size = 0;

    ~file_reader_o() {
        if (nullptr != p_file) {
            std::fclose(p_file);
        }
    }
};

std::uint64_t read_uint(const std::uint8_t *p_data, std::size_t i_width, bool b_little_endian) {
    std::uint64_t u_value = 0;
    if (b_little_endian) {
        for (std::size_t i_index = 0; i_index < i_width; i_index++) {
            u_value |= static_cast<std::uint64_t>(p_data[i_index]) << (8 * i_index);
        }
    } else {
        for (std::size_t i_index = 0; i_index < i_width; i_index++) {
            u_value = (u_value << 8) | p_data[i_index];
        }
    }
    return u_value;
}

bool read_at(file_reader_o &o_reader,
             std::uint64_t u_offset,
             void *p_buffer,
             std::size_t i_size) {
    if (0 == i_size) {
        return true;
    }
    if (u_offset > o_reader.u_size || i_size > o_reader.u_size - u_offset) {
        return false;
    }
    if (0 != fseeko(o_reader.p_file, static_cast<off_t>(u_offset), SEEK_SET)) {
        return false;
    }
    return i_size == std::fread(p_buffer, 1, i_size, o_reader.p_file);
}

std::string read_string_table_entry(const std::vector<std::uint8_t> &o_table,
                                    std::uint64_t u_offset) {
    if (u_offset >= o_table.size()) {
        return {};
    }
    std::size_t i_end = static_cast<std::size_t>(u_offset);
    while (i_end < o_table.size() && 0 != o_table[i_end]) {
        i_end++;
    }
    return std::string(reinterpret_cast<const char *>(o_table.data() + u_offset),
                       i_end - static_cast<std::size_t>(u_offset));
}

struct section_header_o {
    std::uint32_t name_offset = 0;
    std::uint64_t offset = 0;
    std::uint64_t size = 0;
    std::uint32_t link = 0;
};

bool read_section_header(file_reader_o &o_reader,
                         bool b_little_endian,
                         int i_elf_class,
                         std::uint64_t u_section_header_offset,
                         std::uint16_t u_entry_size,
                         std::uint32_t u_index,
                         section_header_o &o_section) {
    std::vector<std::uint8_t> o_buffer(u_entry_size);
    const std::uint64_t u_offset =
        u_section_header_offset + static_cast<std::uint64_t>(u_entry_size) * u_index;
    if (!read_at(o_reader, u_offset, o_buffer.data(), o_buffer.size())) {
        return false;
    }
    o_section = section_header_o{};
    o_section.name_offset = static_cast<std::uint32_t>(read_uint(o_buffer.data(), 4, b_little_endian));
    if (ELF_CLASS_64 == i_elf_class) {
        o_section.offset = read_uint(o_buffer.data() + 24, 8, b_little_endian);
        o_section.size = read_uint(o_buffer.data() + 32, 8, b_little_endian);
        o_section.link = static_cast<std::uint32_t>(read_uint(o_buffer.data() + 40, 4, b_little_endian));
    } else {
        o_section.offset = read_uint(o_buffer.data() + 16, 4, b_little_endian);
        o_section.size = read_uint(o_buffer.data() + 20, 4, b_little_endian);
        o_section.link = static_cast<std::uint32_t>(read_uint(o_buffer.data() + 24, 4, b_little_endian));
    }
    return true;
}

}  // namespace

const char *appimage_detection_name(appimage_detection_e e_detection) {
    switch (e_detection) {
        case appimage_detection_e::not_elf:
            return "not-an-elf";
        case appimage_detection_e::elf_without_appimage_magic:
            return "elf-without-appimage-magic";
        case appimage_detection_e::type1:
            return "type-1";
        case appimage_detection_e::type2:
            return "type-2";
        case appimage_detection_e::unrecognized_type:
            return "unrecognized-type";
        default:
            return "unknown";
    }
}

std::string elf_machine_name(std::uint16_t u_machine) {
    switch (u_machine) {
        case 0x03:
            return "x86";
        case 0x08:
            return "mips";
        case 0x14:
            return "powerpc";
        case 0x15:
            return "powerpc64";
        case 0x16:
            return "s390";
        case 0x28:
            return "arm";
        case 0x2b:
            return "sparcv9";
        case 0x3e:
            return "x86-64";
        case 0xb7:
            return "aarch64";
        case 0xf3:
            return "riscv";
        default:
            return "unknown";
    }
}

bool appimage_reader_c::read(const std::string &s_path, appimage_info_o &o_info) {
    o_info = appimage_info_o{};
    o_info.path = s_path;

    file_reader_o o_reader;
    o_reader.p_file = std::fopen(s_path.c_str(), "rb");
    if (nullptr == o_reader.p_file) {
        o_info.error = "cannot open file: " + s_path;
        return false;
    }
    if (0 != fseeko(o_reader.p_file, 0, SEEK_END)) {
        o_info.error = "cannot determine the file size";
        return false;
    }
    const off_t i_file_size = ftello(o_reader.p_file);
    if (0 > i_file_size) {
        o_info.error = "cannot determine the file size";
        return false;
    }
    o_reader.u_size = static_cast<std::uint64_t>(i_file_size);
    o_info.file_size = o_reader.u_size;

    std::vector<std::uint8_t> o_header(ELF_HEADER_SIZE, 0);
    const std::size_t i_header_size =
        static_cast<std::size_t>(std::min<std::uint64_t>(ELF_HEADER_SIZE, o_reader.u_size));
    if (16 > i_header_size || !read_at(o_reader, 0, o_header.data(), i_header_size)) {
        o_info.detection = appimage_detection_e::not_elf;
        o_info.error = "file is too small to be an ELF";
        return false;
    }
    if (ELF_MAGIC_0 != o_header[0] || 'E' != o_header[1] || 'L' != o_header[2]
        || 'F' != o_header[3]) {
        o_info.detection = appimage_detection_e::not_elf;
        o_info.error = "not an ELF file";
        return false;
    }

    o_info.magic[0] = o_header[8];
    o_info.magic[1] = o_header[9];
    o_info.magic[2] = o_header[10];

    if (APPIMAGE_MAGIC_0 == o_info.magic[0] && APPIMAGE_MAGIC_1 == o_info.magic[1]) {
        if (APPIMAGE_TYPE_1 == o_info.magic[2]) {
            o_info.detection = appimage_detection_e::type1;
        } else if (APPIMAGE_TYPE_2 == o_info.magic[2]) {
            o_info.detection = appimage_detection_e::type2;
        } else {
            o_info.detection = appimage_detection_e::unrecognized_type;
        }
    } else {
        o_info.detection = appimage_detection_e::elf_without_appimage_magic;
    }

    const std::uint8_t u_class = o_header[4];
    const std::uint8_t u_data = o_header[5];
    if (ELF_CLASS_32 != u_class && ELF_CLASS_64 != u_class) {
        o_info.error = "unsupported ELF class";
        return false;
    }
    if (ELF_DATA_LITTLE != u_data && ELF_DATA_BIG != u_data) {
        o_info.error = "unsupported ELF data encoding";
        return false;
    }
    const bool b_little_endian = ELF_DATA_LITTLE == u_data;
    const int i_elf_class = ELF_CLASS_64 == u_class ? 64 : 32;

    o_info.elf.elf_class = i_elf_class;
    o_info.elf.little_endian = b_little_endian;
    o_info.elf.machine =
        static_cast<std::uint16_t>(read_uint(o_header.data() + 0x12, 2, b_little_endian));
    o_info.elf.machine_name = elf_machine_name(o_info.elf.machine);

    std::uint64_t u_section_header_offset = 0;
    std::uint16_t u_section_header_entry_size = 0;
    std::uint32_t u_section_header_count = 0;
    std::uint16_t u_section_name_index = 0;

    if (64 == i_elf_class) {
        o_info.elf.entry_point = read_uint(o_header.data() + 0x18, 8, b_little_endian);
        u_section_header_offset = read_uint(o_header.data() + 0x28, 8, b_little_endian);
        u_section_header_entry_size =
            static_cast<std::uint16_t>(read_uint(o_header.data() + 0x3a, 2, b_little_endian));
        u_section_header_count =
            static_cast<std::uint32_t>(read_uint(o_header.data() + 0x3c, 2, b_little_endian));
        u_section_name_index =
            static_cast<std::uint16_t>(read_uint(o_header.data() + 0x3e, 2, b_little_endian));
    } else {
        o_info.elf.entry_point = read_uint(o_header.data() + 0x18, 4, b_little_endian);
        u_section_header_offset = read_uint(o_header.data() + 0x20, 4, b_little_endian);
        u_section_header_entry_size =
            static_cast<std::uint16_t>(read_uint(o_header.data() + 0x2e, 2, b_little_endian));
        u_section_header_count =
            static_cast<std::uint32_t>(read_uint(o_header.data() + 0x30, 2, b_little_endian));
        u_section_name_index =
            static_cast<std::uint16_t>(read_uint(o_header.data() + 0x32, 2, b_little_endian));
    }

    if (0 != u_section_header_offset && 0 != u_section_header_entry_size) {
        if (0 == u_section_header_count) {
            section_header_o o_first;
            if (!read_section_header(o_reader, b_little_endian, i_elf_class,
                                     u_section_header_offset, u_section_header_entry_size, 0,
                                     o_first)) {
                o_info.error = "cannot read the ELF section header table";
                return false;
            }
            u_section_header_count = static_cast<std::uint32_t>(o_first.size);
            if (SECTION_INDEX_XINDEX == u_section_name_index) {
                u_section_name_index = static_cast<std::uint16_t>(o_first.link);
            }
        }
        if (MAX_SECTION_COUNT < u_section_header_count) {
            o_info.error = "ELF section header count is implausible";
            return false;
        }

        const std::uint64_t u_table_end =
            u_section_header_offset
            + static_cast<std::uint64_t>(u_section_header_entry_size) * u_section_header_count;
        if (u_table_end > o_reader.u_size) {
            o_info.error = "ELF section header table extends beyond the file";
            return false;
        }

        section_header_o o_last;
        if (!read_section_header(o_reader, b_little_endian, i_elf_class, u_section_header_offset,
                                 u_section_header_entry_size, u_section_header_count - 1,
                                 o_last)) {
            o_info.error = "cannot read the last ELF section header";
            return false;
        }
        const std::uint64_t u_last_section_end = o_last.offset + o_last.size;
        o_info.elf.elf_size = std::max(u_table_end, u_last_section_end);
        o_info.elf.section_header_offset = u_section_header_offset;
        o_info.elf.section_header_entry_size = u_section_header_entry_size;
        o_info.elf.section_header_count = u_section_header_count;

        if (u_section_name_index < u_section_header_count) {
            section_header_o o_names;
            if (read_section_header(o_reader, b_little_endian, i_elf_class,
                                    u_section_header_offset, u_section_header_entry_size,
                                    u_section_name_index, o_names)) {
                std::vector<std::uint8_t> o_name_table(static_cast<std::size_t>(o_names.size));
                if (0 == o_names.size
                    || read_at(o_reader, o_names.offset, o_name_table.data(),
                               o_name_table.size())) {
                    for (std::uint32_t u_index = 0; u_index < u_section_header_count; u_index++) {
                        section_header_o o_section;
                        if (!read_section_header(o_reader, b_little_endian, i_elf_class,
                                                 u_section_header_offset,
                                                 u_section_header_entry_size, u_index,
                                                 o_section)) {
                            break;
                        }
                        const std::string s_name =
                            read_string_table_entry(o_name_table, o_section.name_offset);
                        if (".upd_info" == s_name) {
                            o_info.update_information_section.present = true;
                            o_info.update_information_section.name = s_name;
                            o_info.update_information_section.offset = o_section.offset;
                            o_info.update_information_section.size = o_section.size;
                        } else if (".sha256_sig" == s_name) {
                            o_info.signature_section.present = true;
                            o_info.signature_section.name = s_name;
                            o_info.signature_section.offset = o_section.offset;
                            o_info.signature_section.size = o_section.size;
                        }
                    }
                }
            }
        }
    }

    o_info.payload_offset = o_info.elf.elf_size;
    if (o_reader.u_size > o_info.payload_offset) {
        o_info.payload_size = o_reader.u_size - o_info.payload_offset;
    } else {
        o_info.payload_size = 0;
    }

    if (o_info.update_information_section.present
        && 0 < o_info.update_information_section.size) {
        const std::size_t i_read_size = static_cast<std::size_t>(
            std::min<std::uint64_t>(o_info.update_information_section.size,
                                    MAX_UPDATE_INFORMATION_SIZE));
        std::vector<std::uint8_t> o_update(i_read_size);
        if (read_at(o_reader, o_info.update_information_section.offset, o_update.data(),
                    i_read_size)) {
            o_info.update_information.assign(
                reinterpret_cast<const char *>(o_update.data()), o_update.size());
        } else {
            o_info.update_information_section.present = false;
        }
    }

    if (o_info.signature_section.present && 0 < o_info.signature_section.size) {
        const std::size_t i_read_size = static_cast<std::size_t>(
            std::min<std::uint64_t>(o_info.signature_section.size, MAX_SIGNATURE_CHECK_SIZE));
        std::vector<std::uint8_t> o_signature(i_read_size);
        if (read_at(o_reader, o_info.signature_section.offset, o_signature.data(), i_read_size)) {
            bool b_all_zero = true;
            for (const std::uint8_t u_byte : o_signature) {
                if (0 != u_byte) {
                    b_all_zero = false;
                    break;
                }
            }
            o_info.signature_is_empty = b_all_zero;
        }
    }

    if (appimage_detection_e::type2 == o_info.detection
        && o_info.payload_size >= 96) {
        squashfs_superblock_o o_superblock;
        std::string s_squashfs_error;
        if (squashfs_superblock_read(s_path, o_info.payload_offset, o_superblock,
                                     s_squashfs_error)) {
            o_info.has_squashfs = true;
            o_info.squashfs = o_superblock;
        } else {
            o_info.payload_error = s_squashfs_error;
        }
    } else if (appimage_detection_e::type2 == o_info.detection) {
        o_info.payload_error = "payload is too small to hold a SquashFS superblock";
    }

    return true;
}

}  // namespace gnome_appimage::appimage
