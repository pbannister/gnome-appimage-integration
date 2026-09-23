#ifndef GNOME_APPIMAGE_SHA256_H
#define GNOME_APPIMAGE_SHA256_H

#include <cstddef>
#include <cstdint>
#include <string>

namespace gnome_appimage::crypto {

// SHA-256 (FIPS 180-4).  Streaming, so a large AppImage is hashed in chunks, and
// with a way to feed zero padding without holding it in memory: the AppImage
// signature covers the file with the `.sha256_sig` section zeroed.
class sha256_c {
public:
    void update(const std::uint8_t *p_data, std::size_t u_size);
    void update(const std::string &s_text);
    void update_zeros(std::uint64_t u_size);

    // Finish and return the digest as lowercase hexadecimal.  Calling this twice
    // returns the same text; updating after it has no effect.
    std::string hex_digest();

private:
    void transform(const std::uint8_t *p_block);

    std::uint32_t o_state_[8] = {0x6a09e667U, 0xbb67ae85U, 0x3c6ef372U, 0xa54ff53aU,
                                 0x510e527fU, 0x9b05688cU, 0x1f83d9abU, 0x5be0cd19U};
    std::uint64_t u_length_ = 0;
    std::uint8_t o_buffer_[64] = {};
    std::size_t u_buffered_ = 0;
    bool b_final_ = false;
    std::string s_digest_;
};

// The digest of a string, for tests and short inputs.
std::string sha256_hex(const std::string &s_text);

}  // namespace gnome_appimage::crypto

#endif
