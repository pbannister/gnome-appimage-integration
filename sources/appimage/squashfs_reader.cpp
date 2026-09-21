#include "appimage/squashfs_reader.h"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <map>
#include <utility>

#if defined(APPIMAGE_HAVE_ZLIB)
#include <zlib.h>
#endif
#if defined(APPIMAGE_HAVE_LZMA)
#include <lzma.h>
#endif
#if defined(APPIMAGE_HAVE_ZSTD)
#include <zstd.h>
#endif

namespace gnome_appimage::appimage {

namespace {

constexpr std::uint32_t SQUASHFS_MAGIC = 0x73717368U;
constexpr std::size_t METADATA_SIZE = 8192;
constexpr std::uint16_t COMPRESSED_BIT = 0x8000U;
constexpr std::uint32_t COMPRESSED_BIT_BLOCK = 0x01000000U;
constexpr std::uint32_t INVALID_FRAGMENT = 0xffffffffU;
constexpr std::uint64_t INVALID_TABLE = 0xffffffffffffULL;
constexpr std::uint32_t MIN_BLOCK_SIZE = 4096;
constexpr std::uint32_t MAX_BLOCK_SIZE = 1048576;

constexpr std::uint16_t COMPRESSION_ZLIB = 1;
constexpr std::uint16_t COMPRESSION_LZMA = 2;
constexpr std::uint16_t COMPRESSION_LZO = 3;
constexpr std::uint16_t COMPRESSION_XZ = 4;
constexpr std::uint16_t COMPRESSION_LZ4 = 5;
constexpr std::uint16_t COMPRESSION_ZSTD = 6;

constexpr std::uint16_t INODE_DIR = 1;
constexpr std::uint16_t INODE_REG = 2;
constexpr std::uint16_t INODE_SYMLINK = 3;
constexpr std::uint16_t INODE_BLKDEV = 4;
constexpr std::uint16_t INODE_CHRDEV = 5;
constexpr std::uint16_t INODE_FIFO = 6;
constexpr std::uint16_t INODE_SOCKET = 7;
constexpr std::uint16_t INODE_LDIR = 8;
constexpr std::uint16_t INODE_LREG = 9;
constexpr std::uint16_t INODE_LSYMLINK = 10;
constexpr std::uint16_t INODE_LBLKDEV = 11;
constexpr std::uint16_t INODE_LCHRDEV = 12;
constexpr std::uint16_t INODE_LFIFO = 13;
constexpr std::uint16_t INODE_LSOCKET = 14;

std::uint16_t read_u16(const std::uint8_t *p_data) {
    return static_cast<std::uint16_t>(static_cast<std::uint16_t>(p_data[0])
                                      | static_cast<std::uint16_t>(p_data[1] << 8));
}

std::uint32_t read_u32(const std::uint8_t *p_data) {
    return static_cast<std::uint32_t>(p_data[0])
           | (static_cast<std::uint32_t>(p_data[1]) << 8)
           | (static_cast<std::uint32_t>(p_data[2]) << 16)
           | (static_cast<std::uint32_t>(p_data[3]) << 24);
}

std::uint64_t read_u64(const std::uint8_t *p_data) {
    return static_cast<std::uint64_t>(read_u32(p_data))
           | (static_cast<std::uint64_t>(read_u32(p_data + 4)) << 32);
}

squashfs_node_type_e node_type_from_inode_type(std::uint16_t u_type) {
    switch (u_type) {
        case INODE_DIR:
        case INODE_LDIR:
            return squashfs_node_type_e::directory;
        case INODE_REG:
        case INODE_LREG:
            return squashfs_node_type_e::regular_file;
        case INODE_SYMLINK:
        case INODE_LSYMLINK:
            return squashfs_node_type_e::symlink;
        case INODE_BLKDEV:
        case INODE_LBLKDEV:
            return squashfs_node_type_e::block_device;
        case INODE_CHRDEV:
        case INODE_LCHRDEV:
            return squashfs_node_type_e::character_device;
        case INODE_FIFO:
        case INODE_LFIFO:
            return squashfs_node_type_e::fifo;
        case INODE_SOCKET:
        case INODE_LSOCKET:
            return squashfs_node_type_e::socket;
        default:
            return squashfs_node_type_e::unknown;
    }
}

struct metadata_block_o {
    std::vector<std::uint8_t> data;
    std::uint64_t on_disk_size = 0;
};

struct metadata_cursor_o {
    std::uint64_t block_offset = 0;
    std::size_t offset = 0;
};

struct inode_o {
    std::uint16_t type = 0;
    std::uint16_t mode = 0;
    std::uint16_t uid = 0;
    std::uint16_t gid = 0;
    std::uint32_t mtime = 0;
    std::uint32_t number = 0;
    std::uint32_t dir_start_block = 0;
    std::uint32_t dir_offset = 0;
    std::uint32_t dir_size = 0;
    std::uint64_t file_start_block = 0;
    std::uint64_t file_size = 0;
    std::uint32_t fragment = INVALID_FRAGMENT;
    std::uint32_t fragment_offset = 0;
    std::string symlink_target;
    metadata_cursor_o next;
};

struct fragment_entry_o {
    std::uint64_t start_block = 0;
    std::uint32_t size = 0;
};

bool is_directory_inode(const inode_o &o_inode) {
    return INODE_DIR == o_inode.type || INODE_LDIR == o_inode.type;
}

bool is_regular_inode(const inode_o &o_inode) {
    return INODE_REG == o_inode.type || INODE_LREG == o_inode.type;
}

bool parse_superblock(const std::uint8_t *p_data,
                      squashfs_superblock_o &o_superblock,
                      std::string &s_error) {
    o_superblock = squashfs_superblock_o{};
    o_superblock.magic = read_u32(p_data);
    if (SQUASHFS_MAGIC != o_superblock.magic) {
        s_error = "not a SquashFS image (bad magic)";
        return false;
    }
    o_superblock.inodes = read_u32(p_data + 4);
    o_superblock.mkfs_time = read_u32(p_data + 8);
    o_superblock.block_size = read_u32(p_data + 12);
    o_superblock.fragments = read_u32(p_data + 16);
    o_superblock.compression = read_u16(p_data + 20);
    o_superblock.block_log = read_u16(p_data + 22);
    o_superblock.flags = read_u16(p_data + 24);
    o_superblock.id_count = read_u16(p_data + 26);
    o_superblock.major = read_u16(p_data + 28);
    o_superblock.minor = read_u16(p_data + 30);
    o_superblock.root_inode = read_u64(p_data + 32);
    o_superblock.bytes_used = read_u64(p_data + 40);
    o_superblock.id_table_start = read_u64(p_data + 48);
    o_superblock.xattr_id_table_start = read_u64(p_data + 56);
    o_superblock.inode_table_start = read_u64(p_data + 64);
    o_superblock.directory_table_start = read_u64(p_data + 72);
    o_superblock.fragment_table_start = read_u64(p_data + 80);
    o_superblock.lookup_table_start = read_u64(p_data + 88);

    if (MIN_BLOCK_SIZE > o_superblock.block_size
        || MAX_BLOCK_SIZE < o_superblock.block_size
        || 0 != (o_superblock.block_size & (o_superblock.block_size - 1))) {
        s_error = "invalid SquashFS block size";
        return false;
    }
    if (4 != o_superblock.major) {
        s_error = "unsupported SquashFS major version";
        return false;
    }
    o_superblock.valid = true;
    return true;
}

}  // namespace

struct squashfs_reader_state_o {
    std::FILE *p_file = nullptr;
    std::uint64_t u_offset = 0;
    std::uint64_t u_file_size = 0;
    squashfs_superblock_o o_superblock;
    std::vector<std::uint64_t> o_fragment_table_blocks;
    std::vector<std::uint64_t> o_id_table_blocks;
    std::map<std::uint64_t, metadata_block_o> o_metadata_cache;
};

namespace {

bool read_at(squashfs_reader_state_o &o_state,
             std::uint64_t u_relative_offset,
             void *p_buffer,
             std::size_t i_size,
             std::string &s_error) {
    if (0 == i_size) {
        return true;
    }
    const std::uint64_t u_absolute = o_state.u_offset + u_relative_offset;
    if (u_absolute > o_state.u_file_size || i_size > o_state.u_file_size - u_absolute) {
        s_error = "read past end of image";
        return false;
    }
    if (0 != fseeko(o_state.p_file, static_cast<off_t>(u_absolute), SEEK_SET)) {
        s_error = "seek failed";
        return false;
    }
    if (i_size != std::fread(p_buffer, 1, i_size, o_state.p_file)) {
        s_error = "short read";
        return false;
    }
    return true;
}

bool decompress_block(std::uint16_t u_compression,
                      const std::uint8_t *p_input,
                      std::size_t i_input_size,
                      std::uint8_t *p_output,
                      std::size_t i_output_capacity,
                      std::size_t &o_output_size,
                      std::string &s_error) {
    switch (u_compression) {
#if defined(APPIMAGE_HAVE_ZLIB)
        case COMPRESSION_ZLIB: {
            uLongf u_output = i_output_capacity;
            const int i_result = uncompress(reinterpret_cast<Bytef *>(p_output), &u_output,
                                            reinterpret_cast<const Bytef *>(p_input),
                                            i_input_size);
            if (Z_OK != i_result) {
                s_error = "zlib decompression failed";
                return false;
            }
            o_output_size = u_output;
            return true;
        }
#endif
#if defined(APPIMAGE_HAVE_LZMA)
        case COMPRESSION_XZ: {
            std::uint64_t u_memory_limit = UINT64_MAX;
            std::size_t i_input_position = 0;
            std::size_t i_output_position = 0;
            const lzma_ret e_result = lzma_stream_buffer_decode(
                &u_memory_limit, 0, nullptr, p_input, &i_input_position, i_input_size,
                p_output, &i_output_position, i_output_capacity);
            if (LZMA_OK != e_result) {
                s_error = "xz decompression failed";
                return false;
            }
            o_output_size = i_output_position;
            return true;
        }
#endif
#if defined(APPIMAGE_HAVE_ZSTD)
        case COMPRESSION_ZSTD: {
            const std::size_t i_result =
                ZSTD_decompress(p_output, i_output_capacity, p_input, i_input_size);
            if (ZSTD_isError(i_result)) {
                s_error = std::string("zstd decompression failed: ")
                          + ZSTD_getErrorName(i_result);
                return false;
            }
            o_output_size = i_result;
            return true;
        }
#endif
        default:
            s_error = std::string("unsupported SquashFS compression: ")
                      + squashfs_compression_name(u_compression);
            return false;
    }
}

bool get_metadata_block(squashfs_reader_state_o &o_state,
                        std::uint64_t u_block_offset,
                        const metadata_block_o *&p_block,
                        std::string &s_error) {
    const auto o_found = o_state.o_metadata_cache.find(u_block_offset);
    if (o_state.o_metadata_cache.end() != o_found) {
        p_block = &o_found->second;
        return true;
    }

    std::uint8_t u_header[2];
    if (!read_at(o_state, u_block_offset, u_header, sizeof(u_header), s_error)) {
        return false;
    }
    const std::uint16_t u_raw_header = read_u16(u_header);
    const bool b_compressed = 0 == (u_raw_header & COMPRESSED_BIT);
    std::size_t i_stored_size = u_raw_header & ~COMPRESSED_BIT;
    if (0 == i_stored_size) {
        i_stored_size = COMPRESSED_BIT;
    }

    std::vector<std::uint8_t> o_input(i_stored_size);
    if (!read_at(o_state, u_block_offset + 2, o_input.data(), i_stored_size, s_error)) {
        return false;
    }

    metadata_block_o o_block;
    o_block.on_disk_size = 2 + i_stored_size;
    if (b_compressed) {
        o_block.data.resize(METADATA_SIZE);
        std::size_t i_output_size = 0;
        if (!decompress_block(o_state.o_superblock.compression, o_input.data(), i_stored_size,
                              o_block.data.data(), o_block.data.size(), i_output_size,
                              s_error)) {
            return false;
        }
        o_block.data.resize(i_output_size);
    } else {
        o_block.data = std::move(o_input);
    }

    const auto o_inserted = o_state.o_metadata_cache.emplace(u_block_offset, std::move(o_block));
    p_block = &o_inserted.first->second;
    return true;
}

bool metadata_read(squashfs_reader_state_o &o_state,
                   metadata_cursor_o &o_cursor,
                   void *p_buffer,
                   std::size_t i_size,
                   std::string &s_error) {
    std::uint8_t *p_output = static_cast<std::uint8_t *>(p_buffer);
    while (0 < i_size) {
        const metadata_block_o *p_block = nullptr;
        if (!get_metadata_block(o_state, o_cursor.block_offset, p_block, s_error)) {
            return false;
        }
        if (o_cursor.offset > p_block->data.size()) {
            s_error = "metadata offset out of range";
            return false;
        }
        const std::size_t i_available = p_block->data.size() - o_cursor.offset;
        if (0 == i_available) {
            s_error = "empty metadata block";
            return false;
        }
        const std::size_t i_take = std::min(i_size, i_available);
        if (nullptr != p_output) {
            std::memcpy(p_output, p_block->data.data() + o_cursor.offset, i_take);
            p_output += i_take;
        }
        i_size -= i_take;
        o_cursor.offset += i_take;
        if (o_cursor.offset == p_block->data.size()) {
            o_cursor.block_offset += p_block->on_disk_size;
            o_cursor.offset = 0;
        }
    }
    return true;
}

bool table_blocks(squashfs_reader_state_o &o_state,
                  std::uint64_t u_table_start,
                  std::size_t i_each,
                  std::size_t i_count,
                  std::vector<std::uint64_t> &o_blocks,
                  std::string &s_error) {
    o_blocks.clear();
    if (0 == i_count) {
        return true;
    }
    if (INVALID_TABLE == u_table_start) {
        s_error = "table start is invalid";
        return false;
    }
    const std::uint64_t u_total = static_cast<std::uint64_t>(i_each) * i_count;
    const std::size_t i_block_count = static_cast<std::size_t>(
        (u_total + METADATA_SIZE - 1) / METADATA_SIZE);
    std::vector<std::uint8_t> o_buffer(i_block_count * sizeof(std::uint64_t));
    if (!read_at(o_state, u_table_start, o_buffer.data(), o_buffer.size(), s_error)) {
        return false;
    }
    for (std::size_t i_index = 0; i_index < i_block_count; i_index++) {
        o_blocks.push_back(read_u64(o_buffer.data() + i_index * sizeof(std::uint64_t)));
    }
    return true;
}

bool table_get(squashfs_reader_state_o &o_state,
               const std::vector<std::uint64_t> &o_blocks,
               std::size_t i_each,
               std::size_t i_index,
               std::uint8_t *p_output,
               std::string &s_error) {
    const std::size_t i_position = i_index * i_each;
    const std::size_t i_block_number = i_position / METADATA_SIZE;
    const std::size_t i_offset = i_position % METADATA_SIZE;
    if (i_block_number >= o_blocks.size()) {
        s_error = "table index out of range";
        return false;
    }
    const metadata_block_o *p_block = nullptr;
    if (!get_metadata_block(o_state, o_blocks[i_block_number], p_block, s_error)) {
        return false;
    }
    if (i_offset + i_each > p_block->data.size()) {
        s_error = "table entry out of range";
        return false;
    }
    std::memcpy(p_output, p_block->data.data() + i_offset, i_each);
    return true;
}

bool fragment_get(squashfs_reader_state_o &o_state,
                  std::uint32_t u_index,
                  fragment_entry_o &o_fragment,
                  std::string &s_error) {
    std::uint8_t u_buffer[16];
    if (!table_get(o_state, o_state.o_fragment_table_blocks, sizeof(u_buffer), u_index,
                   u_buffer, s_error)) {
        return false;
    }
    o_fragment.start_block = read_u64(u_buffer);
    o_fragment.size = read_u32(u_buffer + 8);
    return true;
}

bool id_get(squashfs_reader_state_o &o_state,
            std::uint16_t u_index,
            std::uint32_t &o_id,
            std::string &s_error) {
    if (o_state.o_id_table_blocks.empty()) {
        o_id = u_index;
        return true;
    }
    std::uint8_t u_buffer[4];
    if (!table_get(o_state, o_state.o_id_table_blocks, sizeof(u_buffer), u_index, u_buffer,
                   s_error)) {
        return false;
    }
    o_id = read_u32(u_buffer);
    return true;
}

bool read_inode(squashfs_reader_state_o &o_state,
                std::uint64_t u_inode_ref,
                inode_o &o_inode,
                std::string &s_error) {
    metadata_cursor_o o_cursor;
    o_cursor.block_offset = o_state.o_superblock.inode_table_start + (u_inode_ref >> 16);
    o_cursor.offset = static_cast<std::size_t>(u_inode_ref & 0xffffULL);

    std::uint8_t u_base[16];
    if (!metadata_read(o_state, o_cursor, u_base, sizeof(u_base), s_error)) {
        return false;
    }
    o_inode = inode_o{};
    o_inode.type = read_u16(u_base);
    o_inode.mode = read_u16(u_base + 2);
    o_inode.uid = read_u16(u_base + 4);
    o_inode.gid = read_u16(u_base + 6);
    o_inode.mtime = read_u32(u_base + 8);
    o_inode.number = read_u32(u_base + 12);

    if (INODE_DIR == o_inode.type) {
        std::uint8_t u_body[16];
        if (!metadata_read(o_state, o_cursor, u_body, sizeof(u_body), s_error)) {
            return false;
        }
        o_inode.dir_start_block = read_u32(u_body);
        o_inode.dir_size = read_u16(u_body + 8);
        o_inode.dir_offset = read_u16(u_body + 10);
    } else if (INODE_LDIR == o_inode.type) {
        std::uint8_t u_body[24];
        if (!metadata_read(o_state, o_cursor, u_body, sizeof(u_body), s_error)) {
            return false;
        }
        o_inode.dir_size = read_u32(u_body + 4);
        o_inode.dir_start_block = read_u32(u_body + 8);
        o_inode.dir_offset = read_u16(u_body + 18);
    } else if (INODE_REG == o_inode.type) {
        std::uint8_t u_body[16];
        if (!metadata_read(o_state, o_cursor, u_body, sizeof(u_body), s_error)) {
            return false;
        }
        o_inode.file_start_block = read_u32(u_body);
        o_inode.fragment = read_u32(u_body + 4);
        o_inode.fragment_offset = read_u32(u_body + 8);
        o_inode.file_size = read_u32(u_body + 12);
    } else if (INODE_LREG == o_inode.type) {
        std::uint8_t u_body[40];
        if (!metadata_read(o_state, o_cursor, u_body, sizeof(u_body), s_error)) {
            return false;
        }
        o_inode.file_start_block = read_u64(u_body);
        o_inode.file_size = read_u64(u_body + 8);
        o_inode.fragment = read_u32(u_body + 28);
        o_inode.fragment_offset = read_u32(u_body + 32);
    } else if (INODE_SYMLINK == o_inode.type || INODE_LSYMLINK == o_inode.type) {
        std::uint8_t u_body[8];
        if (!metadata_read(o_state, o_cursor, u_body, sizeof(u_body), s_error)) {
            return false;
        }
        const std::uint32_t u_target_size = read_u32(u_body + 4);
        if (0 < u_target_size) {
            o_inode.symlink_target.resize(u_target_size);
            if (!metadata_read(o_state, o_cursor, &o_inode.symlink_target[0], u_target_size,
                               s_error)) {
                return false;
            }
        }
    }

    o_inode.next = o_cursor;
    return true;
}

bool list_entries(squashfs_reader_state_o &o_state,
                  const inode_o &o_inode,
                  const std::string &s_parent_path,
                  std::vector<squashfs_entry_o> &o_entries,
                  std::string &s_error) {
    metadata_cursor_o o_cursor;
    o_cursor.block_offset = o_state.o_superblock.directory_table_start + o_inode.dir_start_block;
    o_cursor.offset = o_inode.dir_offset;

    const std::uint64_t u_total = 3 < o_inode.dir_size ? o_inode.dir_size - 3 : 0;
    std::uint64_t u_consumed = 0;
    std::uint32_t u_remaining = 0;
    std::uint32_t u_header_start_block = 0;
    std::uint32_t u_header_inode_number = 0;

    while (u_consumed < u_total) {
        if (0 == u_remaining) {
            std::uint8_t u_header[12];
            if (!metadata_read(o_state, o_cursor, u_header, sizeof(u_header), s_error)) {
                return false;
            }
            u_consumed += sizeof(u_header);
            u_remaining = read_u32(u_header) + 1;
            u_header_start_block = read_u32(u_header + 4);
            u_header_inode_number = read_u32(u_header + 8);
        }

        std::uint8_t u_entry[8];
        if (!metadata_read(o_state, o_cursor, u_entry, sizeof(u_entry), s_error)) {
            return false;
        }
        u_consumed += sizeof(u_entry);
        const std::uint32_t u_entry_offset = read_u16(u_entry);
        const std::int16_t i_inode_delta = static_cast<std::int16_t>(read_u16(u_entry + 2));
        const std::uint16_t u_inode_type = read_u16(u_entry + 4);
        const std::size_t i_name_size = static_cast<std::size_t>(read_u16(u_entry + 6)) + 1;

        std::string s_name(i_name_size, '\0');
        if (!metadata_read(o_state, o_cursor, &s_name[0], i_name_size, s_error)) {
            return false;
        }
        u_consumed += i_name_size;
        u_remaining--;

        squashfs_entry_o o_result;
        o_result.name = s_name;
        o_result.path = "/" == s_parent_path ? "/" + s_name : s_parent_path + "/" + s_name;
        o_result.inode_ref =
            (static_cast<std::uint64_t>(u_header_start_block) << 16) | u_entry_offset;
        o_result.inode_number = static_cast<std::uint32_t>(
            static_cast<std::int64_t>(u_header_inode_number) + i_inode_delta);
        o_result.type = node_type_from_inode_type(u_inode_type);
        o_entries.push_back(std::move(o_result));
    }
    return true;
}

bool resolve_path(squashfs_reader_state_o &o_state,
                  const std::string &s_path,
                  inode_o &o_inode,
                  std::string &s_error) {
    if (!read_inode(o_state, o_state.o_superblock.root_inode, o_inode, s_error)) {
        return false;
    }

    std::vector<std::string> o_components;
    std::size_t i_start = 0;
    while (i_start < s_path.size()) {
        while (i_start < s_path.size() && '/' == s_path[i_start]) {
            i_start++;
        }
        if (i_start >= s_path.size()) {
            break;
        }
        const std::size_t i_end = s_path.find('/', i_start);
        if (std::string::npos == i_end) {
            o_components.push_back(s_path.substr(i_start));
            break;
        }
        o_components.push_back(s_path.substr(i_start, i_end - i_start));
        i_start = i_end + 1;
    }

    std::string s_current = "/";
    for (const std::string &s_component : o_components) {
        if (!is_directory_inode(o_inode)) {
            s_error = "not a directory: " + s_current;
            return false;
        }
        std::vector<squashfs_entry_o> o_entries;
        if (!list_entries(o_state, o_inode, s_current, o_entries, s_error)) {
            return false;
        }
        bool b_found = false;
        std::uint64_t u_inode_ref = 0;
        for (const squashfs_entry_o &o_entry : o_entries) {
            if (o_entry.name == s_component) {
                u_inode_ref = o_entry.inode_ref;
                b_found = true;
                break;
            }
        }
        if (!b_found) {
            s_error = "no such path: " + s_path;
            return false;
        }
        if (!read_inode(o_state, u_inode_ref, o_inode, s_error)) {
            return false;
        }
        s_current = "/" == s_current ? "/" + s_component : s_current + "/" + s_component;
    }
    return true;
}

bool decompress_data_block(squashfs_reader_state_o &o_state,
                           const std::uint8_t *p_input,
                           std::size_t i_input_size,
                           bool b_compressed,
                           std::vector<std::uint8_t> &o_output,
                           std::size_t &o_output_size,
                           std::string &s_error) {
    if (!b_compressed) {
        if (i_input_size > o_output.size()) {
            s_error = "uncompressed data block larger than the block size";
            return false;
        }
        std::memcpy(o_output.data(), p_input, i_input_size);
        o_output_size = i_input_size;
        return true;
    }
    return decompress_block(o_state.o_superblock.compression, p_input, i_input_size,
                            o_output.data(), o_output.size(), o_output_size, s_error);
}

bool read_file_data(squashfs_reader_state_o &o_state,
                    const inode_o &o_inode,
                    std::string &s_data,
                    std::string &s_error) {
    if (squashfs_reader_c::MAX_FILE_SIZE < o_inode.file_size) {
        s_error = "file exceeds the reader size limit";
        return false;
    }
    const std::uint32_t u_block_size = o_state.o_superblock.block_size;
    if (0 == u_block_size) {
        s_error = "invalid SquashFS block size";
        return false;
    }

    s_data.clear();
    s_data.reserve(static_cast<std::size_t>(o_inode.file_size));

    const bool b_has_fragment = INVALID_FRAGMENT != o_inode.fragment;
    const std::uint64_t u_full_blocks =
        b_has_fragment ? (o_inode.file_size / u_block_size)
                       : ((o_inode.file_size + u_block_size - 1) / u_block_size);

    metadata_cursor_o o_cursor = o_inode.next;
    std::uint64_t u_data_position = o_inode.file_start_block;
    std::uint64_t u_remaining = o_inode.file_size;
    std::vector<std::uint8_t> o_output(u_block_size);

    for (std::uint64_t i_block = 0; i_block < u_full_blocks; i_block++) {
        std::uint8_t u_header[4];
        if (!metadata_read(o_state, o_cursor, u_header, sizeof(u_header), s_error)) {
            return false;
        }
        const std::uint32_t u_block_header = read_u32(u_header);
        const bool b_compressed = 0 == (u_block_header & COMPRESSED_BIT_BLOCK);
        const std::size_t i_input_size = u_block_header & ~COMPRESSED_BIT_BLOCK;
        const std::size_t i_expected = static_cast<std::size_t>(
            std::min<std::uint64_t>(u_block_size, u_remaining));

        if (0 == i_input_size) {
            s_data.append(i_expected, '\0');
        } else {
            std::vector<std::uint8_t> o_input(i_input_size);
            if (!read_at(o_state, u_data_position, o_input.data(), i_input_size, s_error)) {
                return false;
            }
            std::size_t i_output_size = 0;
            if (!decompress_data_block(o_state, o_input.data(), i_input_size, b_compressed,
                                       o_output, i_output_size, s_error)) {
                return false;
            }
            if (i_output_size < i_expected) {
                s_error = "short data block";
                return false;
            }
            s_data.append(reinterpret_cast<const char *>(o_output.data()), i_expected);
        }
        u_data_position += i_input_size;
        u_remaining -= i_expected;
    }

    if (b_has_fragment && 0 < u_remaining) {
        fragment_entry_o o_fragment;
        if (!fragment_get(o_state, o_inode.fragment, o_fragment, s_error)) {
            return false;
        }
        const bool b_compressed = 0 == (o_fragment.size & COMPRESSED_BIT_BLOCK);
        const std::size_t i_input_size = o_fragment.size & ~COMPRESSED_BIT_BLOCK;
        std::vector<std::uint8_t> o_input(i_input_size);
        if (!read_at(o_state, o_fragment.start_block, o_input.data(), i_input_size, s_error)) {
            return false;
        }
        std::size_t i_output_size = 0;
        if (!decompress_data_block(o_state, o_input.data(), i_input_size, b_compressed, o_output,
                                   i_output_size, s_error)) {
            return false;
        }
        const std::size_t i_fragment_offset = o_inode.fragment_offset;
        if (i_fragment_offset > i_output_size
            || u_remaining > i_output_size - i_fragment_offset) {
            s_error = "fragment range out of bounds";
            return false;
        }
        s_data.append(reinterpret_cast<const char *>(o_output.data()) + i_fragment_offset,
                      static_cast<std::size_t>(u_remaining));
    }

    if (s_data.size() != o_inode.file_size) {
        s_error = "file size mismatch";
        return false;
    }
    return true;
}

bool name_ends_with(const std::string &s_name, const std::string &s_extension) {
    if (s_name.size() < s_extension.size()) {
        return false;
    }
    return 0 == s_name.compare(s_name.size() - s_extension.size(), s_extension.size(),
                               s_extension);
}

}  // namespace

const char *squashfs_compression_name(std::uint16_t u_compression) {
    switch (u_compression) {
        case COMPRESSION_ZLIB:
            return "gzip";
        case COMPRESSION_LZMA:
            return "lzma";
        case COMPRESSION_LZO:
            return "lzo";
        case COMPRESSION_XZ:
            return "xz";
        case COMPRESSION_LZ4:
            return "lz4";
        case COMPRESSION_ZSTD:
            return "zstd";
        default:
            return "unknown";
    }
}

bool squashfs_compression_supported(std::uint16_t u_compression) {
    switch (u_compression) {
#if defined(APPIMAGE_HAVE_ZLIB)
        case COMPRESSION_ZLIB:
            return true;
#endif
#if defined(APPIMAGE_HAVE_LZMA)
        case COMPRESSION_XZ:
            return true;
#endif
#if defined(APPIMAGE_HAVE_ZSTD)
        case COMPRESSION_ZSTD:
            return true;
#endif
        default:
            return false;
    }
}

const char *squashfs_node_type_name(squashfs_node_type_e e_type) {
    switch (e_type) {
        case squashfs_node_type_e::directory:
            return "directory";
        case squashfs_node_type_e::regular_file:
            return "file";
        case squashfs_node_type_e::symlink:
            return "symlink";
        case squashfs_node_type_e::block_device:
            return "block-device";
        case squashfs_node_type_e::character_device:
            return "character-device";
        case squashfs_node_type_e::fifo:
            return "fifo";
        case squashfs_node_type_e::socket:
            return "socket";
        default:
            return "unknown";
    }
}

bool squashfs_superblock_read(const std::string &s_image_path,
                              std::uint64_t u_payload_offset,
                              squashfs_superblock_o &o_superblock,
                              std::string &s_error) {
    std::FILE *p_file = std::fopen(s_image_path.c_str(), "rb");
    if (nullptr == p_file) {
        s_error = "cannot open image: " + s_image_path;
        return false;
    }
    if (0 != fseeko(p_file, static_cast<off_t>(u_payload_offset), SEEK_SET)) {
        std::fclose(p_file);
        s_error = "cannot seek to the payload offset";
        return false;
    }
    std::uint8_t u_buffer[96];
    const std::size_t i_read = std::fread(u_buffer, 1, sizeof(u_buffer), p_file);
    std::fclose(p_file);
    if (sizeof(u_buffer) != i_read) {
        s_error = "cannot read the SquashFS superblock";
        return false;
    }
    return parse_superblock(u_buffer, o_superblock, s_error);
}

squashfs_reader_c::squashfs_reader_c() = default;

squashfs_reader_c::~squashfs_reader_c() {
    close();
}

bool squashfs_reader_c::open(const std::string &s_image_path,
                             std::uint64_t u_payload_offset,
                             std::string &s_error) {
    close();
    std::FILE *p_file = std::fopen(s_image_path.c_str(), "rb");
    if (nullptr == p_file) {
        s_error = "cannot open image: " + s_image_path;
        return false;
    }

    if (0 != fseeko(p_file, 0, SEEK_END)) {
        std::fclose(p_file);
        s_error = "cannot determine the image size";
        return false;
    }
    const off_t i_file_size = ftello(p_file);
    if (0 > i_file_size) {
        std::fclose(p_file);
        s_error = "cannot determine the image size";
        return false;
    }

    if (0 != fseeko(p_file, static_cast<off_t>(u_payload_offset), SEEK_SET)) {
        std::fclose(p_file);
        s_error = "cannot seek to the payload offset";
        return false;
    }
    std::uint8_t u_buffer[96];
    if (sizeof(u_buffer) != std::fread(u_buffer, 1, sizeof(u_buffer), p_file)) {
        std::fclose(p_file);
        s_error = "cannot read the SquashFS superblock";
        return false;
    }

    squashfs_superblock_o o_superblock;
    if (!parse_superblock(u_buffer, o_superblock, s_error)) {
        std::fclose(p_file);
        return false;
    }
    if (!squashfs_compression_supported(o_superblock.compression)) {
        std::fclose(p_file);
        s_error = std::string("unsupported SquashFS compression: ")
                  + squashfs_compression_name(o_superblock.compression);
        return false;
    }

    auto *p_state = new squashfs_reader_state_o();
    p_state->p_file = p_file;
    p_state->u_offset = u_payload_offset;
    p_state->u_file_size = static_cast<std::uint64_t>(i_file_size);
    p_state->o_superblock = o_superblock;

    if (0 < o_superblock.fragments && INVALID_TABLE != o_superblock.fragment_table_start) {
        if (!table_blocks(*p_state, o_superblock.fragment_table_start, 16,
                          o_superblock.fragments, p_state->o_fragment_table_blocks,
                          s_error)) {
            std::fclose(p_file);
            delete p_state;
            return false;
        }
    }
    if (0 < o_superblock.id_count && INVALID_TABLE != o_superblock.id_table_start) {
        if (!table_blocks(*p_state, o_superblock.id_table_start, 4, o_superblock.id_count,
                          p_state->o_id_table_blocks, s_error)) {
            std::fclose(p_file);
            delete p_state;
            return false;
        }
    }

    p_state_ = p_state;
    return true;
}

void squashfs_reader_c::close() {
    if (nullptr == p_state_) {
        return;
    }
    if (nullptr != p_state_->p_file) {
        std::fclose(p_state_->p_file);
    }
    delete p_state_;
    p_state_ = nullptr;
}

const squashfs_superblock_o &squashfs_reader_c::superblock() const {
    static const squashfs_superblock_o o_empty;
    if (nullptr == p_state_) {
        return o_empty;
    }
    return p_state_->o_superblock;
}

bool squashfs_reader_c::list_root(std::vector<squashfs_entry_o> &o_entries,
                                  std::string &s_error) {
    o_entries.clear();
    if (nullptr == p_state_) {
        s_error = "reader is not open";
        return false;
    }
    inode_o o_inode;
    if (!read_inode(*p_state_, p_state_->o_superblock.root_inode, o_inode, s_error)) {
        return false;
    }
    return list_entries(*p_state_, o_inode, "/", o_entries, s_error);
}

bool squashfs_reader_c::list_directory(const std::string &s_path,
                                       std::vector<squashfs_entry_o> &o_entries,
                                       std::string &s_error) {
    o_entries.clear();
    if (nullptr == p_state_) {
        s_error = "reader is not open";
        return false;
    }
    inode_o o_inode;
    if (!resolve_path(*p_state_, s_path, o_inode, s_error)) {
        return false;
    }
    if (!is_directory_inode(o_inode)) {
        s_error = "not a directory: " + s_path;
        return false;
    }
    const std::string s_normalized = "/" == s_path ? "/" : s_path;
    return list_entries(*p_state_, o_inode, s_normalized, o_entries, s_error);
}

bool squashfs_reader_c::stat(const std::string &s_path,
                             squashfs_stat_o &o_stat,
                             std::string &s_error) {
    o_stat = squashfs_stat_o{};
    if (nullptr == p_state_) {
        s_error = "reader is not open";
        return false;
    }
    inode_o o_inode;
    if (!resolve_path(*p_state_, s_path, o_inode, s_error)) {
        return false;
    }
    o_stat.inode_number = o_inode.number;
    o_stat.mode = o_inode.mode;
    o_stat.mtime = o_inode.mtime;
    o_stat.type = node_type_from_inode_type(o_inode.type);
    o_stat.size = is_regular_inode(o_inode) ? o_inode.file_size : 0;
    if (!id_get(*p_state_, o_inode.uid, o_stat.uid, s_error)) {
        return false;
    }
    if (!id_get(*p_state_, o_inode.gid, o_stat.gid, s_error)) {
        return false;
    }
    return true;
}

bool squashfs_reader_c::read_file(const std::string &s_path,
                                  std::string &s_data,
                                  std::string &s_error) {
    s_data.clear();
    if (nullptr == p_state_) {
        s_error = "reader is not open";
        return false;
    }
    inode_o o_inode;
    if (!resolve_path(*p_state_, s_path, o_inode, s_error)) {
        return false;
    }
    if (!is_regular_inode(o_inode)) {
        s_error = "not a regular file: " + s_path;
        return false;
    }
    return read_file_data(*p_state_, o_inode, s_data, s_error);
}

bool squashfs_reader_c::list_root_files_with_extension(
    const std::string &s_extension,
    std::vector<squashfs_entry_o> &o_entries,
    std::string &s_error) {
    std::vector<squashfs_entry_o> o_root_entries;
    if (!list_root(o_root_entries, s_error)) {
        return false;
    }
    o_entries.clear();
    for (squashfs_entry_o &o_entry : o_root_entries) {
        if (squashfs_node_type_e::regular_file == o_entry.type
            && name_ends_with(o_entry.name, s_extension)) {
            o_entries.push_back(std::move(o_entry));
        }
    }
    return true;
}

}  // namespace gnome_appimage::appimage
