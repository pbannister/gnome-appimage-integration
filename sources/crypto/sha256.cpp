#include "crypto/sha256.h"

#include <algorithm>
#include <cstdio>
#include <cstring>

namespace gnome_appimage::crypto {
namespace {

constexpr std::uint32_t K[64] = {
    0x428a2f98U, 0x71374491U, 0xb5c0fbcfU, 0xe9b5dba5U, 0x3956c25bU, 0x59f111f1U, 0x923f82a4U,
    0xab1c5ed5U, 0xd807aa98U, 0x12835b01U, 0x243185beU, 0x550c7dc3U, 0x72be5d74U, 0x80deb1feU,
    0x9bdc06a7U, 0xc19bf174U, 0xe49b69c1U, 0xefbe4786U, 0x0fc19dc6U, 0x240ca1ccU, 0x2de92c6fU,
    0x4a7484aaU, 0x5cb0a9dcU, 0x76f988daU, 0x983e5152U, 0xa831c66dU, 0xb00327c8U, 0xbf597fc7U,
    0xc6e00bf3U, 0xd5a79147U, 0x06ca6351U, 0x14292967U, 0x27b70a85U, 0x2e1b2138U, 0x4d2c6dfcU,
    0x53380d13U, 0x650a7354U, 0x766a0abbU, 0x81c2c92eU, 0x92722c85U, 0xa2bfe8a1U, 0xa81a664bU,
    0xc24b8b70U, 0xc76c51a3U, 0xd192e819U, 0xd6990624U, 0xf40e3585U, 0x106aa070U, 0x19a4c116U,
    0x1e376c08U, 0x2748774cU, 0x34b0bcb5U, 0x391c0cb3U, 0x4ed8aa4aU, 0x5b9cca4fU, 0x682e6ff3U,
    0x748f82eeU, 0x78a5636fU, 0x84c87814U, 0x8cc70208U, 0x90befffaU, 0xa4506cebU, 0xbef9a3f7U,
    0xc67178f2U};

inline std::uint32_t rotate_right(std::uint32_t u_value, unsigned int u_bits) {
    return (u_value >> u_bits) | (u_value << (32U - u_bits));
}

}  // namespace

void sha256_c::transform(const std::uint8_t *p_block) {
    std::uint32_t o_words[64];
    for (int i_index = 0; i_index < 16; i_index++) {
        o_words[i_index] = (static_cast<std::uint32_t>(p_block[i_index * 4]) << 24U)
                           | (static_cast<std::uint32_t>(p_block[i_index * 4 + 1]) << 16U)
                           | (static_cast<std::uint32_t>(p_block[i_index * 4 + 2]) << 8U)
                           | static_cast<std::uint32_t>(p_block[i_index * 4 + 3]);
    }
    for (int i_index = 16; i_index < 64; i_index++) {
        const std::uint32_t u_s0 = rotate_right(o_words[i_index - 15], 7)
                                   ^ rotate_right(o_words[i_index - 15], 18)
                                   ^ (o_words[i_index - 15] >> 3U);
        const std::uint32_t u_s1 = rotate_right(o_words[i_index - 2], 17)
                                   ^ rotate_right(o_words[i_index - 2], 19)
                                   ^ (o_words[i_index - 2] >> 10U);
        o_words[i_index] = o_words[i_index - 16] + u_s0 + o_words[i_index - 7] + u_s1;
    }

    std::uint32_t a = o_state_[0];
    std::uint32_t b = o_state_[1];
    std::uint32_t c = o_state_[2];
    std::uint32_t d = o_state_[3];
    std::uint32_t e = o_state_[4];
    std::uint32_t f = o_state_[5];
    std::uint32_t g = o_state_[6];
    std::uint32_t h = o_state_[7];
    for (int i_index = 0; i_index < 64; i_index++) {
        const std::uint32_t u_s1 =
            rotate_right(e, 6) ^ rotate_right(e, 11) ^ rotate_right(e, 25);
        const std::uint32_t u_choice = (e & f) ^ ((~e) & g);
        const std::uint32_t u_temp1 = h + u_s1 + u_choice + K[i_index] + o_words[i_index];
        const std::uint32_t u_s0 =
            rotate_right(a, 2) ^ rotate_right(a, 13) ^ rotate_right(a, 22);
        const std::uint32_t u_majority = (a & b) ^ (a & c) ^ (b & c);
        const std::uint32_t u_temp2 = u_s0 + u_majority;
        h = g;
        g = f;
        f = e;
        e = d + u_temp1;
        d = c;
        c = b;
        b = a;
        a = u_temp1 + u_temp2;
    }
    o_state_[0] += a;
    o_state_[1] += b;
    o_state_[2] += c;
    o_state_[3] += d;
    o_state_[4] += e;
    o_state_[5] += f;
    o_state_[6] += g;
    o_state_[7] += h;
}

void sha256_c::update(const std::uint8_t *p_data, std::size_t u_size) {
    if (nullptr == p_data || b_final_) {
        return;
    }
    u_length_ += u_size;
    std::size_t i_offset = 0;
    while (i_offset < u_size) {
        const std::size_t u_space = sizeof(o_buffer_) - u_buffered_;
        const std::size_t u_take = std::min(u_space, u_size - i_offset);
        std::memcpy(o_buffer_ + u_buffered_, p_data + i_offset, u_take);
        u_buffered_ += u_take;
        i_offset += u_take;
        if (sizeof(o_buffer_) == u_buffered_) {
            transform(o_buffer_);
            u_buffered_ = 0;
        }
    }
}

void sha256_c::update(const std::string &s_text) {
    update(reinterpret_cast<const std::uint8_t *>(s_text.data()), s_text.size());
}

void sha256_c::update_zeros(std::uint64_t u_size) {
    static const std::uint8_t o_zeros[64] = {};
    while (0 < u_size) {
        const std::size_t u_take =
            static_cast<std::size_t>(std::min<std::uint64_t>(u_size, sizeof(o_zeros)));
        update(o_zeros, u_take);
        u_size -= u_take;
    }
}

std::string sha256_c::hex_digest() {
    if (b_final_) {
        return s_digest_;
    }
    const std::uint64_t u_bits = u_length_ * 8U;
    const std::uint8_t u_padding = 0x80U;
    update(&u_padding, 1);
    // Pad to 56 bytes modulo 64, then the length in bits as a big-endian word.
    std::uint8_t o_zero = 0;
    while (56 != (u_buffered_ % 64)) {
        update(&o_zero, 1);
    }
    std::uint8_t o_length[8];
    for (int i_index = 0; i_index < 8; i_index++) {
        o_length[i_index] = static_cast<std::uint8_t>(u_bits >> (56U - 8U * i_index));
    }
    update(o_length, sizeof(o_length));

    std::string s_result;
    s_result.reserve(64);
    char s_byte[3];
    for (const std::uint32_t u_word : o_state_) {
        for (int i_shift = 3; i_shift >= 0; i_shift--) {
            std::snprintf(s_byte, sizeof(s_byte), "%02x",
                          static_cast<unsigned int>((u_word >> (8U * i_shift)) & 0xffU));
            s_result += s_byte;
        }
    }
    b_final_ = true;
    s_digest_ = s_result;
    return s_digest_;
}

std::string sha256_hex(const std::string &s_text) {
    sha256_c o_hasher;
    o_hasher.update(s_text);
    return o_hasher.hex_digest();
}

}  // namespace gnome_appimage::crypto
