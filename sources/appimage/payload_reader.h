#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace gnome_appimage::appimage {

// Forward declarations: the concrete readers define these shared result types.
struct squashfs_entry_o;
struct squashfs_stat_o;

// The interface every payload reader (SquashFS for a type 2 AppImage, ISO 9660
// for a type 1 AppImage) exposes to the integration engine.  It reads files and
// directories from the filesystem image appended to the runtime, without
// executing or mounting anything.
class payload_reader_c {
public:
    virtual ~payload_reader_c() = default;

    // Open the payload that begins at the given offset in the image file.
    virtual bool open(const std::string &s_image_path,
                      std::uint64_t u_payload_offset,
                      std::string &s_error) = 0;
    virtual void close() = 0;
    virtual bool is_open() const = 0;

    // List the root directory.
    virtual bool list_root(std::vector<squashfs_entry_o> &o_entries,
                           std::string &s_error) = 0;

    // List a directory by absolute path, for example "/usr/share".
    virtual bool list_directory(const std::string &s_path,
                                std::vector<squashfs_entry_o> &o_entries,
                                std::string &s_error) = 0;

    // Return metadata for one path.
    virtual bool stat(const std::string &s_path, squashfs_stat_o &o_stat,
                      std::string &s_error) = 0;

    // Read a regular file into memory.
    virtual bool read_file(const std::string &s_path, std::string &s_data,
                           std::string &s_error) = 0;

    // List the root files whose name ends with the given extension, for example
    // ".desktop".
    virtual bool list_root_files_with_extension(const std::string &s_extension,
                                                std::vector<squashfs_entry_o> &o_entries,
                                                std::string &s_error) = 0;
};

}  // namespace gnome_appimage::appimage
