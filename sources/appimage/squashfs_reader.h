#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace gnome_appimage::appimage {

// Types used by a SquashFS inode.
enum class squashfs_node_type_e {
    directory,
    regular_file,
    symlink,
    block_device,
    character_device,
    fifo,
    socket,
    unknown,
};

// The SquashFS superblock that begins a type 2 payload.
struct squashfs_superblock_o {
    std::uint32_t magic = 0;
    std::uint32_t inodes = 0;
    std::uint32_t mkfs_time = 0;
    std::uint32_t block_size = 0;
    std::uint32_t fragments = 0;
    std::uint16_t compression = 0;
    std::uint16_t block_log = 0;
    std::uint16_t flags = 0;
    std::uint16_t id_count = 0;
    std::uint16_t major = 0;
    std::uint16_t minor = 0;
    std::uint64_t root_inode = 0;
    std::uint64_t bytes_used = 0;
    std::uint64_t id_table_start = 0;
    std::uint64_t xattr_id_table_start = 0;
    std::uint64_t inode_table_start = 0;
    std::uint64_t directory_table_start = 0;
    std::uint64_t fragment_table_start = 0;
    std::uint64_t lookup_table_start = 0;
    bool valid = false;
};

// One directory entry.
struct squashfs_entry_o {
    std::string path;
    std::string name;
    std::uint64_t inode_ref = 0;
    std::uint32_t inode_number = 0;
    squashfs_node_type_e type = squashfs_node_type_e::unknown;
    std::uint64_t size = 0;
};

// Metadata for one inode.
struct squashfs_stat_o {
    std::uint32_t inode_number = 0;
    std::uint16_t mode = 0;
    std::uint32_t uid = 0;
    std::uint32_t gid = 0;
    std::uint64_t size = 0;
    std::uint32_t mtime = 0;
    squashfs_node_type_e type = squashfs_node_type_e::unknown;
};

// Return the name of a SquashFS compression identifier.
const char *squashfs_compression_name(std::uint16_t u_compression);

// Return true when this build can decompress the identifier.
bool squashfs_compression_supported(std::uint16_t u_compression);

// Read and validate a SquashFS superblock at a byte offset in a file.
bool squashfs_superblock_read(const std::string &s_image_path,
                              std::uint64_t u_payload_offset,
                              squashfs_superblock_o &o_superblock,
                              std::string &s_error);

const char *squashfs_node_type_name(squashfs_node_type_e e_type);

// Read files and directories from the SquashFS payload of an AppImage.
// The reader never writes to the image file.
struct squashfs_reader_state_o;

class squashfs_reader_c {
public:
    static constexpr std::uint64_t MAX_FILE_SIZE = 64ULL * 1024ULL * 1024ULL;

    squashfs_reader_c();
    ~squashfs_reader_c();
    squashfs_reader_c(const squashfs_reader_c &) = delete;
    squashfs_reader_c &operator=(const squashfs_reader_c &) = delete;

    // Open the payload that begins at the given offset in the image file.
    bool open(const std::string &s_image_path,
              std::uint64_t u_payload_offset,
              std::string &s_error);
    void close();
    bool is_open() const { return nullptr != p_state_; }

    const squashfs_superblock_o &superblock() const;

    // List the root directory.
    bool list_root(std::vector<squashfs_entry_o> &o_entries, std::string &s_error);

    // List a directory by absolute path, for example "/usr/share".
    bool list_directory(const std::string &s_path,
                        std::vector<squashfs_entry_o> &o_entries,
                        std::string &s_error);

    // Return metadata for one path.
    bool stat(const std::string &s_path, squashfs_stat_o &o_stat, std::string &s_error);

    // Read a regular file into memory.
    bool read_file(const std::string &s_path, std::string &s_data, std::string &s_error);

    // List the root files whose name ends with the given extension, for example ".desktop".
    bool list_root_files_with_extension(const std::string &s_extension,
                                        std::vector<squashfs_entry_o> &o_entries,
                                        std::string &s_error);

private:
    squashfs_reader_state_o *p_state_ = nullptr;
};

}  // namespace gnome_appimage::appimage
