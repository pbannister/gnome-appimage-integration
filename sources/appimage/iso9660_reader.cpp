#include "appimage/iso9660_reader.h"

#include "appimage/squashfs_reader.h"

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

namespace gnome_appimage::appimage {

// One directory record, with its Rock Ridge name and symlink target resolved.
struct iso_dir_record_o {
    std::uint32_t extent = 0;
    std::uint32_t data_length = 0;
    std::string name;
    std::string symlink_target;
    bool is_directory = false;
    bool is_symlink = false;
};

namespace {

constexpr std::size_t SECTOR_SIZE = 2048;
constexpr std::uint64_t SYSTEM_AREA_SECTORS = 16;
constexpr std::uint8_t PVD_TYPE = 1;
constexpr std::uint8_t DIRECTORY_FLAG = 0x02;
constexpr int MAX_SYMLINK_DEPTH = 8;

std::uint32_t read_u32_le(const std::uint8_t *p_data) {
    return static_cast<std::uint32_t>(p_data[0]) | (static_cast<std::uint32_t>(p_data[1]) << 8)
           | (static_cast<std::uint32_t>(p_data[2]) << 16)
           | (static_cast<std::uint32_t>(p_data[3]) << 24);
}

// Read a fixed-size region at an absolute byte offset.
bool read_at(FILE *p_file, std::uint64_t u_offset, void *p_buffer, std::size_t i_size,
             std::string &s_error) {
    if (0 != fseeko(p_file, static_cast<off_t>(u_offset), SEEK_SET)) {
        s_error = "cannot seek in image";
        return false;
    }
    if (i_size != fread(p_buffer, 1, i_size, p_file)) {
        s_error = "short read from image";
        return false;
    }
    return true;
}

// Parse the Rock Ridge System Use area, extracting the POSIX name (NM) and the
// symlink target (SL).  A CE continuation is read from the image by the caller;
// here it is reported so the name can be marked incomplete rather than wrong.
void parse_system_use(const std::uint8_t *p_data, std::size_t i_size, std::string &o_name,
                      std::string &o_symlink) {
    std::size_t i_index = 0;
    while (i_index + 4 <= i_size) {
        const char c_signature_0 = static_cast<char>(p_data[i_index]);
        const char c_signature_1 = static_cast<char>(p_data[i_index + 1]);
        const std::size_t i_entry_length = p_data[i_index + 2];
        if (4 > i_entry_length || i_index + i_entry_length > i_size) {
            break;
        }
        const std::uint8_t *p_entry_data = p_data + i_index + 4;
        const std::size_t i_entry_data = i_entry_length - 4;

        if ('N' == c_signature_0 && 'M' == c_signature_1) {
            if (0 < i_entry_data) {
                o_name.append(reinterpret_cast<const char *>(p_entry_data + 1), i_entry_data - 1);
            }
        } else if ('S' == c_signature_0 && 'L' == c_signature_1) {
            // Component records: one flag byte, one length byte, then the bytes.
            std::size_t i_component = 1;
            while (i_component + 2 <= i_entry_data) {
                const std::uint8_t u_flags = p_entry_data[i_component];
                const std::uint8_t u_length = p_entry_data[i_component + 1];
                if (i_component + 2 + u_length > i_entry_data) {
                    break;
                }
                const std::string s_component(
                    reinterpret_cast<const char *>(p_entry_data + i_component + 2), u_length);
                if (0 != (u_flags & 0x04)) {          // parent directory
                    o_symlink += o_symlink.empty() ? ".." : "/..";
                } else if (0 != (u_flags & 0x02)) {   // current directory
                    o_symlink += o_symlink.empty() ? "." : "/.";
                } else if (0 != (u_flags & 0x08)) {   // absolute (root)
                    o_symlink = "/" + s_component;
                } else {
                    o_symlink += o_symlink.empty() ? s_component : "/" + s_component;
                }
                i_component += 2 + u_length;
            }
        }
        i_index += i_entry_length;
    }
}

// Parse one directory record.  A zero length byte means padding to the next
// sector boundary, and signals the caller to stop.
bool parse_directory_record(const std::uint8_t *p_data, std::size_t i_remaining,
                            iso_dir_record_o &o_record, bool &o_padding) {
    o_padding = false;
    if (1 > i_remaining) {
        return false;
    }
    const std::uint8_t u_length = p_data[0];
    if (0 == u_length) {
        o_padding = true;
        return false;
    }
    if (34 > u_length || u_length > i_remaining) {
        return false;
    }
    o_record.extent = read_u32_le(p_data + 2);
    o_record.data_length = read_u32_le(p_data + 10);
    o_record.is_directory = 0 != (p_data[25] & DIRECTORY_FLAG);
    const std::uint8_t u_name_length = p_data[32];
    if (33 + u_name_length > u_length) {
        return false;
    }
    const std::string s_iso_name(reinterpret_cast<const char *>(p_data + 33), u_name_length);
    const std::size_t i_system_use = 33 + u_name_length + (u_name_length % 2 == 0 ? 1 : 0);
    std::string s_rock_name;
    std::string s_symlink;
    if (i_system_use < u_length) {
        parse_system_use(p_data + i_system_use, u_length - i_system_use, s_rock_name, s_symlink);
    }
    o_record.name = s_rock_name.empty() ? s_iso_name : s_rock_name;
    o_record.symlink_target = s_symlink;
    o_record.is_symlink = !s_symlink.empty();
    return true;
}

squashfs_node_type_e node_type_of(const iso_dir_record_o &o_record) {
    if (o_record.is_directory) {
        return squashfs_node_type_e::directory;
    }
    if (o_record.is_symlink) {
        return squashfs_node_type_e::symlink;
    }
    return squashfs_node_type_e::regular_file;
}

std::vector<std::string> split_path(const std::string &s_path) {
    std::vector<std::string> o_parts;
    std::size_t i_start = 0;
    while (i_start < s_path.size()) {
        const std::size_t i_slash = s_path.find('/', i_start);
        const std::string s_part =
            s_path.substr(i_start, std::string::npos == i_slash ? std::string::npos : i_slash - i_start);
        if (!s_part.empty()) {
            o_parts.push_back(s_part);
        }
        if (std::string::npos == i_slash) {
            break;
        }
        i_start = i_slash + 1;
    }
    return o_parts;
}

std::string join_path(const std::string &s_base, const std::string &s_child) {
    if (s_base.empty() || "/" == s_base) {
        return "/" + s_child;
    }
    return s_base + "/" + s_child;
}

}  // namespace

// The state the reader keeps while it is open.
struct iso9660_reader_state_o {
    std::string image_path;
    std::uint64_t payload_offset = 0;
    iso_dir_record_o root;
};

iso9660_reader_c::iso9660_reader_c() = default;

iso9660_reader_c::~iso9660_reader_c() {
    close();
}

bool iso9660_reader_c::open(const std::string &s_image_path, std::uint64_t u_payload_offset,
                            std::string &s_error) {
    close();
    FILE *p_file = std::fopen(s_image_path.c_str(), "rb");
    if (nullptr == p_file) {
        s_error = "cannot open image: " + s_image_path;
        return false;
    }

    std::uint8_t u_pvd[SECTOR_SIZE];
    const std::uint64_t u_pvd_offset = u_payload_offset + SYSTEM_AREA_SECTORS * SECTOR_SIZE;
    if (!read_at(p_file, u_pvd_offset, u_pvd, sizeof(u_pvd), s_error)) {
        std::fclose(p_file);
        return false;
    }
    std::fclose(p_file);

    if (PVD_TYPE != u_pvd[0] || 0 != std::memcmp(u_pvd + 1, "CD001", 5)) {
        s_error = "no ISO 9660 primary volume descriptor";
        return false;
    }

    iso_dir_record_o o_root;
    bool o_padding = false;
    if (!parse_directory_record(u_pvd + 156, sizeof(u_pvd) - 156, o_root, o_padding)
        || !o_root.is_directory) {
        s_error = "no ISO 9660 root directory record";
        return false;
    }

    auto *p_state = new iso9660_reader_state_o();
    p_state->image_path = s_image_path;
    p_state->payload_offset = u_payload_offset;
    p_state->root = o_root;
    p_state_ = p_state;
    return true;
}

void iso9660_reader_c::close() {
    delete p_state_;
    p_state_ = nullptr;
}

namespace {

// Read the data of a directory or file record into a buffer.
bool read_extent(const iso9660_reader_state_o &o_state, const iso_dir_record_o &o_record,
                 std::string &o_data, std::string &s_error) {
    if (o_record.data_length > iso9660_reader_c::MAX_FILE_SIZE) {
        s_error = "file too large";
        return false;
    }
    o_data.resize(o_record.data_length);
    if (0 == o_record.data_length) {
        return true;
    }
    FILE *p_file = std::fopen(o_state.image_path.c_str(), "rb");
    if (nullptr == p_file) {
        s_error = "cannot open image";
        return false;
    }
    const std::uint64_t u_offset =
        o_state.payload_offset + static_cast<std::uint64_t>(o_record.extent) * SECTOR_SIZE;
    const bool b_ok =
        read_at(p_file, u_offset, &o_data[0], o_record.data_length, s_error);
    std::fclose(p_file);
    return b_ok;
}

// List the children of a directory record.
bool list_children(const iso9660_reader_state_o &o_state, const iso_dir_record_o &o_directory,
                   std::vector<iso_dir_record_o> &o_children, std::string &s_error) {
    std::string s_data;
    if (!read_extent(o_state, o_directory, s_data, s_error)) {
        return false;
    }
    std::size_t i_offset = 0;
    while (i_offset < s_data.size()) {
        bool o_padding = false;
        iso_dir_record_o o_child;
        if (!parse_directory_record(reinterpret_cast<const std::uint8_t *>(s_data.data()) + i_offset,
                                    s_data.size() - i_offset, o_child, o_padding)) {
            if (o_padding) {
                i_offset = ((i_offset / SECTOR_SIZE) + 1) * SECTOR_SIZE;
                continue;
            }
            break;
        }
        if ("." != o_child.name && ".." != o_child.name) {
            o_children.push_back(o_child);
        }
        i_offset += reinterpret_cast<const std::uint8_t *>(s_data.data())[i_offset];
    }
    return true;
}

// Resolve an absolute path to a directory record, following symlinks.  The
// parent directory is tracked so a relative symlink target resolves correctly.
bool resolve_path(const iso9660_reader_state_o &o_state, const std::string &s_path,
                  iso_dir_record_o &o_result, std::string &s_error, int i_depth = 0) {
    if (MAX_SYMLINK_DEPTH < i_depth) {
        s_error = "symlink depth exceeded";
        return false;
    }
    iso_dir_record_o o_current = o_state.root;
    std::string s_current_path = "/";
    for (const std::string &s_part : split_path(s_path)) {
        if (!o_current.is_directory) {
            s_error = "not a directory: " + s_current_path;
            return false;
        }
        std::vector<iso_dir_record_o> o_children;
        if (!list_children(o_state, o_current, o_children, s_error)) {
            return false;
        }
        const iso_dir_record_o *p_child = nullptr;
        for (const iso_dir_record_o &o_child : o_children) {
            if (o_child.name == s_part) {
                p_child = &o_child;
                break;
            }
        }
        if (nullptr == p_child) {
            s_error = "not found: " + join_path(s_current_path, s_part);
            return false;
        }
        o_current = *p_child;
        if (o_current.is_symlink) {
            std::string s_target = o_current.symlink_target;
            const std::string s_resolve_from =
                s_target.empty() || '/' != s_target[0] ? s_current_path : std::string();
            const std::string s_full = s_resolve_from.empty()
                                           ? s_target
                                           : join_path(s_resolve_from, s_target);
            if (!resolve_path(o_state, s_full, o_current, s_error, i_depth + 1)) {
                return false;
            }
        }
        s_current_path = join_path(s_current_path, s_part);
    }
    o_result = o_current;
    return true;
}

void to_entry(const iso_dir_record_o &o_record, const std::string &s_path, squashfs_entry_o &o_entry) {
    o_entry.path = s_path;
    o_entry.name = o_record.name;
    o_entry.type = node_type_of(o_record);
    o_entry.size = o_record.is_symlink ? 0 : o_record.data_length;
}

}  // namespace

bool iso9660_reader_c::list_root(std::vector<squashfs_entry_o> &o_entries, std::string &s_error) {
    if (!is_open()) {
        s_error = "reader is not open";
        return false;
    }
    std::vector<iso_dir_record_o> o_children;
    if (!list_children(*p_state_, p_state_->root, o_children, s_error)) {
        return false;
    }
    o_entries.clear();
    for (const iso_dir_record_o &o_child : o_children) {
        squashfs_entry_o o_entry;
        to_entry(o_child, "/" + o_child.name, o_entry);
        o_entries.push_back(std::move(o_entry));
    }
    return true;
}

bool iso9660_reader_c::list_directory(const std::string &s_path,
                                      std::vector<squashfs_entry_o> &o_entries,
                                      std::string &s_error) {
    if (!is_open()) {
        s_error = "reader is not open";
        return false;
    }
    iso_dir_record_o o_directory;
    if (!resolve_path(*p_state_, s_path, o_directory, s_error)) {
        return false;
    }
    if (!o_directory.is_directory) {
        s_error = "not a directory: " + s_path;
        return false;
    }
    std::vector<iso_dir_record_o> o_children;
    if (!list_children(*p_state_, o_directory, o_children, s_error)) {
        return false;
    }
    o_entries.clear();
    for (const iso_dir_record_o &o_child : o_children) {
        squashfs_entry_o o_entry;
        to_entry(o_child, join_path(s_path, o_child.name), o_entry);
        o_entries.push_back(std::move(o_entry));
    }
    return true;
}

bool iso9660_reader_c::stat(const std::string &s_path, squashfs_stat_o &o_stat,
                            std::string &s_error) {
    if (!is_open()) {
        s_error = "reader is not open";
        return false;
    }
    iso_dir_record_o o_record;
    if (!resolve_path(*p_state_, s_path, o_record, s_error)) {
        return false;
    }
    o_stat.type = node_type_of(o_record);
    o_stat.size = o_record.is_symlink ? 0 : o_record.data_length;
    return true;
}

bool iso9660_reader_c::read_file(const std::string &s_path, std::string &s_data,
                                 std::string &s_error) {
    if (!is_open()) {
        s_error = "reader is not open";
        return false;
    }
    iso_dir_record_o o_record;
    if (!resolve_path(*p_state_, s_path, o_record, s_error)) {
        return false;
    }
    if (o_record.is_directory) {
        s_error = "is a directory: " + s_path;
        return false;
    }
    return read_extent(*p_state_, o_record, s_data, s_error);
}

bool iso9660_reader_c::list_root_files_with_extension(const std::string &s_extension,
                                                      std::vector<squashfs_entry_o> &o_entries,
                                                      std::string &s_error) {
    if (!list_root(o_entries, s_error)) {
        return false;
    }
    std::vector<squashfs_entry_o> o_filtered;
    for (const squashfs_entry_o &o_entry : o_entries) {
        if (o_entry.name.size() >= s_extension.size()
            && 0 == o_entry.name.compare(o_entry.name.size() - s_extension.size(),
                                        s_extension.size(), s_extension)) {
            o_filtered.push_back(o_entry);
        }
    }
    o_entries = std::move(o_filtered);
    return true;
}

}  // namespace gnome_appimage::appimage
