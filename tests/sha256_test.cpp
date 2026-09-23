//
// sha256_test.cpp: unit tests for the digest the AppImage signature covers, using
// the FIPS 180-4 examples.
//
#include "crypto/sha256.h"

#include <iostream>
#include <string>

using gnome_appimage::crypto::sha256_c;
using gnome_appimage::crypto::sha256_hex;

namespace {

int i_failures = 0;

void check(bool b_condition, const std::string &s_description) {
    if (!b_condition) {
        std::cerr << "FAIL: " << s_description << '\n';
        i_failures++;
    }
}

void check_digest(const std::string &s_input, const std::string &s_expected,
                  const std::string &s_description) {
    const std::string s_actual = sha256_hex(s_input);
    if (s_actual != s_expected) {
        std::cerr << "FAIL: " << s_description << ": expected " << s_expected << ", got "
                  << s_actual << '\n';
        i_failures++;
    }
}

void test_known_digests() {
    check_digest("", "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855",
                 "the empty string");
    check_digest("abc", "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad",
                 "abc");
    check_digest("abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq",
                 "248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1",
                 "the 56-byte example, which pads with a second block");
    check_digest(
        "abcdefghbcdefghicdefghijdefghijkefghijklfghijklmghijklmnhijklmnoijklmnopjklmnopqklmnop"
        "qrlmnopqrsmnopqrstnopqrstu",
        "cf5b16a778af8380036ce59e7b0492370b249b11e8f07a51afac45037afee9d1",
        "the 112-byte example");
}

void test_streaming_matches_one_shot() {
    const std::string s_text = "abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq";
    sha256_c o_hasher;
    for (const char c_character : s_text) {
        o_hasher.update(std::string(1, c_character));
    }
    check(o_hasher.hex_digest() == sha256_hex(s_text), "byte-at-a-time equals one shot");
    check(o_hasher.hex_digest() == sha256_hex(s_text), "finalising twice gives the same answer");
    o_hasher.update("ignored");
    check(o_hasher.hex_digest() == sha256_hex(s_text), "updating after finalising changes nothing");

    // And in awkward chunk sizes, including one that straddles the block boundary.
    for (const std::size_t u_chunk : {std::size_t(1), std::size_t(7), std::size_t(63),
                                      std::size_t(64), std::size_t(65), std::size_t(1000)}) {
        sha256_c o_chunked;
        for (std::size_t i_offset = 0; i_offset < s_text.size(); i_offset += u_chunk) {
            o_chunked.update(s_text.substr(i_offset, u_chunk));
        }
        check(o_chunked.hex_digest() == sha256_hex(s_text),
              "chunk size " + std::to_string(u_chunk) + " equals one shot");
    }
}

void test_million_a() {
    sha256_c o_hasher;
    o_hasher.update_zeros(0);
    const std::string s_block(1000, 'a');
    for (int i_index = 0; i_index < 1000; i_index++) {
        o_hasher.update(s_block);
    }
    check(o_hasher.hex_digest()
              == "cdc76e5c9914fb9281a1c7e284d73e67f1809a48a497200e046d39ccc7112cd0",
          "one million a");
}

void test_zero_padding() {
    // The signature covers the file with its section zeroed, so feeding zeros has
    // to hash exactly like the same number of zero bytes.
    const std::string s_zeros(100, '\0');
    sha256_c o_padded;
    o_padded.update("head");
    o_padded.update_zeros(100);
    o_padded.update("tail");

    sha256_c o_explicit;
    o_explicit.update("head");
    o_explicit.update(s_zeros);
    o_explicit.update("tail");

    check(o_padded.hex_digest() == o_explicit.hex_digest(), "zero padding equals literal zeros");
    check(o_padded.hex_digest() != sha256_hex("head" "tail"), "the zeros are not skipped");
}

}  // namespace

int main() {
    test_known_digests();
    test_streaming_matches_one_shot();
    test_million_a();
    test_zero_padding();
    if (0 != i_failures) {
        std::cerr << "sha256-test: " << i_failures << " checks failed\n";
        return 1;
    }
    std::cout << "sha256-test: all checks passed\n";
    return 0;
}
