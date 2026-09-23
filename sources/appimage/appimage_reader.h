#pragma once

#include "appimage/squashfs_reader.h"

#include <cstdint>
#include <string>
#include <vector>

namespace gnome_appimage::appimage {

// How an inspected file was classified.
enum class appimage_detection_e {
    not_elf,
    elf_without_appimage_magic,
    type1,
    type2,
    unrecognized_type,
};

// What the `.sha256_sig` section holds, and how checking it went.  The reader
// classifies the section cheaply; the verification is in appimage_signature.h,
// because it hashes the whole file.
enum class appimage_signature_e {
    absent,
    empty_padding,
    hex_digest,
    pgp_signature,
    unrecognised,
};

enum class appimage_signature_result_e {
    not_checked,
    verified,
    mismatch,
    unverifiable,
};

// Selected facts from the ELF header.
struct appimage_elf_info_o {
    int elf_class = 0;
    bool little_endian = true;
    std::uint16_t machine = 0;
    std::string machine_name;
    std::uint64_t entry_point = 0;
    std::uint64_t elf_size = 0;
    std::uint64_t section_header_offset = 0;
    std::uint16_t section_header_entry_size = 0;
    std::uint32_t section_header_count = 0;
};

// One named ELF section.
struct appimage_section_o {
    bool present = false;
    std::string name;
    std::uint64_t offset = 0;
    std::uint64_t size = 0;
};

// Everything the container reader reports about one file.
struct appimage_info_o {
    std::string path;
    appimage_detection_e detection = appimage_detection_e::not_elf;
    std::uint64_t file_size = 0;
    std::uint8_t magic[3] = {0, 0, 0};
    appimage_elf_info_o elf;
    std::uint64_t payload_offset = 0;
    std::uint64_t payload_size = 0;
    appimage_section_o update_information_section;
    std::string update_information;
    appimage_section_o signature_section;
    bool signature_is_empty = false;
    // What the signature section holds, the digest it states, what this file hashes
    // to, and whether the two were checked.  Filled by classify_appimage_signature()
    // and verify_appimage_signature(); reading alone never hashes the file.
    appimage_signature_e signature = appimage_signature_e::absent;
    std::string signature_text;
    std::string signature_digest;
    std::string computed_digest;
    appimage_signature_result_e signature_result = appimage_signature_result_e::not_checked;
    std::string signature_result_note;
    bool has_squashfs = false;
    squashfs_superblock_o squashfs;
    std::string payload_error;
    std::string error;
};

// Return a stable name for a detection outcome.
const char *appimage_detection_name(appimage_detection_e e_detection);

// Return a readable name for an ELF machine identifier.
std::string elf_machine_name(std::uint16_t u_machine);

// Read the container structure of an AppImage without executing or mounting it.
// Returns false when the file cannot be read or is not a usable ELF.
class appimage_reader_c {
public:
    static constexpr std::uint32_t MAX_UPDATE_INFORMATION_SIZE = 64U * 1024U;

    static bool read(const std::string &s_path, appimage_info_o &o_info);
};

}  // namespace gnome_appimage::appimage
