#pragma once

#include "appimage/payload_reader.h"

#include <cstdint>
#include <string>
#include <vector>

namespace gnome_appimage::appimage {

struct iso9660_reader_state_o;

// Read files and directories from the ISO 9660 (with Rock Ridge) payload of a
// type 1 AppImage.  The reader never writes to the image file.
class iso9660_reader_c : public payload_reader_c {
public:
    static constexpr std::uint64_t MAX_FILE_SIZE = 64ULL * 1024ULL * 1024ULL;

    iso9660_reader_c();
    ~iso9660_reader_c() override;
    iso9660_reader_c(const iso9660_reader_c &) = delete;
    iso9660_reader_c &operator=(const iso9660_reader_c &) = delete;

    bool open(const std::string &s_image_path,
              std::uint64_t u_payload_offset,
              std::string &s_error) override;
    void close() override;
    bool is_open() const override { return nullptr != p_state_; }

    bool list_root(std::vector<squashfs_entry_o> &o_entries, std::string &s_error) override;
    bool list_directory(const std::string &s_path,
                        std::vector<squashfs_entry_o> &o_entries,
                        std::string &s_error) override;
    bool stat(const std::string &s_path, squashfs_stat_o &o_stat, std::string &s_error) override;
    bool read_file(const std::string &s_path, std::string &s_data, std::string &s_error) override;
    bool list_root_files_with_extension(const std::string &s_extension,
                                        std::vector<squashfs_entry_o> &o_entries,
                                        std::string &s_error) override;

private:
    iso9660_reader_state_o *p_state_ = nullptr;
};

}  // namespace gnome_appimage::appimage
