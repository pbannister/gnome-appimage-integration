#include "appimage/appimage_signature.h"

#include <algorithm>
#include <cctype>
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <sstream>
#include <sys/wait.h>
#include <unistd.h>
#include <vector>

#include "crypto/sha256.h"

namespace gnome_appimage::appimage {
namespace {

constexpr std::size_t READ_CHUNK_SIZE = 1024U * 1024U;

std::string trim_whitespace(const std::string &s_text) {
    std::size_t i_begin = 0;
    std::size_t i_end = s_text.size();
    const auto is_space = [](char c_character) {
        return 0 != std::isspace(static_cast<unsigned char>(c_character));
    };
    while (i_begin < i_end && is_space(s_text[i_begin])) {
        i_begin++;
    }
    while (i_begin < i_end && is_space(s_text[i_end - 1])) {
        i_end--;
    }
    return s_text.substr(i_begin, i_end - i_begin);
}

bool is_hex_digits(const std::string &s_text) {
    if (s_text.empty()) {
        return false;
    }
    for (const char c_character : s_text) {
        if (0 == std::isxdigit(static_cast<unsigned char>(c_character))) {
            return false;
        }
    }
    return true;
}

std::string lowercase(const std::string &s_text) {
    std::string s_result;
    s_result.reserve(s_text.size());
    for (const char c_character : s_text) {
        s_result += static_cast<char>(std::tolower(static_cast<unsigned char>(c_character)));
    }
    return s_result;
}

bool command_exists(const std::string &s_command) {
    if (s_command.empty()) {
        return false;
    }
    if (std::string::npos != s_command.find('/')) {
        return 0 == access(s_command.c_str(), X_OK);
    }
    const char *s_path = std::getenv("PATH");
    if (nullptr == s_path) {
        return false;
    }
    std::string s_remaining = s_path;
    while (!s_remaining.empty()) {
        const std::size_t i_separator = s_remaining.find(':');
        const std::string s_directory = std::string::npos == i_separator
                                            ? s_remaining
                                            : s_remaining.substr(0, i_separator);
        const std::string s_candidate =
            (s_directory.empty() ? std::string(".") : s_directory) + "/" + s_command;
        if (0 == access(s_candidate.c_str(), X_OK)) {
            return true;
        }
        if (std::string::npos == i_separator) {
            break;
        }
        s_remaining = s_remaining.substr(i_separator + 1);
    }
    return false;
}

// Run a program without a shell, capturing stdout and stderr together.
bool capture_process(const std::vector<std::string> &o_arguments, std::string &s_output,
                     int &i_exit_code) {
    s_output.clear();
    i_exit_code = -1;
    if (o_arguments.empty()) {
        return false;
    }
    int i_pipe[2] = {-1, -1};
    if (0 != pipe(i_pipe)) {
        return false;
    }
    const pid_t i_child = fork();
    if (0 > i_child) {
        close(i_pipe[0]);
        close(i_pipe[1]);
        return false;
    }
    if (0 == i_child) {
        dup2(i_pipe[1], STDOUT_FILENO);
        dup2(i_pipe[1], STDERR_FILENO);
        close(i_pipe[0]);
        close(i_pipe[1]);
        std::vector<char *> o_raw;
        o_raw.reserve(o_arguments.size() + 1);
        for (const std::string &s_argument : o_arguments) {
            o_raw.push_back(const_cast<char *>(s_argument.c_str()));
        }
        o_raw.push_back(nullptr);
        execvp(o_raw[0], o_raw.data());
        _exit(127);
    }
    close(i_pipe[1]);
    char s_buffer[4096];
    ssize_t i_count = 0;
    while (0 < (i_count = read(i_pipe[0], s_buffer, sizeof(s_buffer)))) {
        s_output.append(s_buffer, static_cast<std::size_t>(i_count));
    }
    close(i_pipe[0]);
    int i_status = 0;
    while (0 > waitpid(i_child, &i_status, 0) && EINTR == errno) {
    }
    if (WIFEXITED(i_status)) {
        i_exit_code = WEXITSTATUS(i_status);
        return true;
    }
    return false;
}

// A temporary file that removes itself.
class temp_file_c {
public:
    temp_file_c() = default;
    ~temp_file_c() {
        if (!s_path_.empty()) {
            std::remove(s_path_.c_str());
        }
    }
    temp_file_c(const temp_file_c &) = delete;
    temp_file_c &operator=(const temp_file_c &) = delete;

    bool write_text(const std::string &s_content) {
        const char *s_runtime = std::getenv("XDG_RUNTIME_DIR");
        std::string s_template =
            std::string(nullptr == s_runtime ? "/tmp" : s_runtime) + "/appimage-signature-XXXXXX";
        std::vector<char> o_buffer(s_template.begin(), s_template.end());
        o_buffer.push_back('\0');
        const int i_descriptor = mkstemp(o_buffer.data());
        if (0 > i_descriptor) {
            return false;
        }
        s_path_ = o_buffer.data();
        const ssize_t i_written =
            ::write(i_descriptor, s_content.data(), s_content.size());
        close(i_descriptor);
        return i_written == static_cast<ssize_t>(s_content.size());
    }

    const std::string &path() const {
        return s_path_;
    }

private:
    std::string s_path_;
};

// The digest the section covers: the file with the signature section zeroed.
bool compute_signature_digest(const std::string &s_path, const appimage_info_o &o_info,
                              std::string &s_digest, std::string &s_error) {
    const std::uint64_t u_section_offset = o_info.signature_section.offset;
    const std::uint64_t u_section_size = o_info.signature_section.size;
    if (u_section_offset > o_info.file_size
        || u_section_size > o_info.file_size - u_section_offset) {
        s_error = "the signature section lies outside the file";
        return false;
    }
    std::ifstream o_input(s_path, std::ios::binary);
    if (!o_input) {
        s_error = "cannot open " + s_path;
        return false;
    }
    gnome_appimage::crypto::sha256_c o_hasher;
    std::vector<char> o_buffer(READ_CHUNK_SIZE);
    const auto hash_range = [&](std::uint64_t u_offset, std::uint64_t u_size) {
        o_input.clear();
        o_input.seekg(static_cast<std::streamoff>(u_offset), std::ios::beg);
        std::uint64_t u_remaining = u_size;
        while (0 < u_remaining) {
            const std::size_t u_take = static_cast<std::size_t>(
                std::min<std::uint64_t>(u_remaining, o_buffer.size()));
            o_input.read(o_buffer.data(), static_cast<std::streamsize>(u_take));
            const std::streamsize i_read = o_input.gcount();
            if (0 >= i_read) {
                return false;
            }
            o_hasher.update(reinterpret_cast<const std::uint8_t *>(o_buffer.data()),
                            static_cast<std::size_t>(i_read));
            u_remaining -= static_cast<std::uint64_t>(i_read);
        }
        return true;
    };
    if (!hash_range(0, u_section_offset)) {
        s_error = "cannot read the file before the signature section";
        return false;
    }
    o_hasher.update_zeros(u_section_size);
    if (u_section_offset + u_section_size < o_info.file_size
        && !hash_range(u_section_offset + u_section_size,
                       o_info.file_size - u_section_offset - u_section_size)) {
        s_error = "cannot read the file after the signature section";
        return false;
    }
    s_digest = o_hasher.hex_digest();
    return true;
}

}  // namespace

void classify_appimage_signature(const std::string &s_content, appimage_info_o &o_info) {
    std::string s_trimmed = s_content;
    // A fixed-size section is usually padded with NULs after the content.
    while (!s_trimmed.empty() && '\0' == s_trimmed.back()) {
        s_trimmed.pop_back();
    }
    s_trimmed = trim_whitespace(s_trimmed);

    o_info.signature = appimage_signature_e::empty_padding;
    o_info.signature_text.clear();
    o_info.signature_digest.clear();
    o_info.signature_is_empty = true;
    if (s_trimmed.empty()) {
        return;
    }
    o_info.signature_is_empty = false;

    // A bare digest, optionally prefixed the way some tools write it.
    std::string s_candidate = s_trimmed;
    for (const char *s_prefix : {"sha256:", "SHA256:", "sha256=", "SHA256="}) {
        const std::size_t u_length = std::strlen(s_prefix);
        if (s_candidate.size() > u_length
            && 0 == s_candidate.compare(0, u_length, s_prefix)) {
            s_candidate = s_candidate.substr(u_length);
            break;
        }
    }
    if (64 == s_candidate.size() && is_hex_digits(s_candidate)) {
        o_info.signature = appimage_signature_e::hex_digest;
        o_info.signature_digest = lowercase(s_candidate);
        return;
    }
    if (0 == s_trimmed.rfind("-----BEGIN PGP SIGNATURE-----", 0)) {
        o_info.signature = appimage_signature_e::pgp_signature;
        o_info.signature_text = s_trimmed;
        return;
    }
    o_info.signature = appimage_signature_e::unrecognised;
    o_info.signature_text = s_trimmed;
}

bool verify_appimage_signature(const std::string &s_path, appimage_info_o &o_info,
                               std::string &s_error) {
    s_error.clear();
    o_info.computed_digest.clear();
    o_info.signature_result = appimage_signature_result_e::unverifiable;
    o_info.signature_result_note.clear();
    if (appimage_signature_e::hex_digest != o_info.signature
        && appimage_signature_e::pgp_signature != o_info.signature) {
        o_info.signature_result = appimage_signature_result_e::not_checked;
        return true;
    }

    std::string s_computed;
    std::string s_digest_error;
    if (!compute_signature_digest(s_path, o_info, s_computed, s_digest_error)) {
        s_error = s_digest_error;
        return false;
    }
    o_info.computed_digest = s_computed;

    if (appimage_signature_e::hex_digest == o_info.signature) {
        o_info.signature_result = s_computed == o_info.signature_digest
                                      ? appimage_signature_result_e::verified
                                      : appimage_signature_result_e::mismatch;
        return true;
    }

    // A signature is over the digest text the signer had, which is the digest this
    // file has when the section is zeroed.
    if (!command_exists("gpg")) {
        o_info.signature_result_note = "gpg is not installed";
        return true;
    }
    temp_file_c o_signature_file;
    temp_file_c o_digest_file;
    if (!o_signature_file.write_text(o_info.signature_text)
        || !o_digest_file.write_text(s_computed)) {
        o_info.signature_result_note = "cannot write a temporary file for gpg";
        return true;
    }
    std::string s_output;
    int i_exit_code = -1;
    if (!capture_process({"gpg", "--verify", o_signature_file.path(), o_digest_file.path()},
                         s_output, i_exit_code)) {
        o_info.signature_result_note = "gpg could not be run";
        return true;
    }
    if (0 == i_exit_code && std::string::npos != s_output.find("Good signature")) {
        o_info.signature_result = appimage_signature_result_e::verified;
        return true;
    }
    if (std::string::npos != s_output.find("BAD signature")) {
        o_info.signature_result = appimage_signature_result_e::mismatch;
        return true;
    }
    // A missing public key, or any other refusal: say what gpg said, first line only.
    std::istringstream o_lines(s_output);
    std::string s_line;
    while (std::getline(o_lines, s_line)) {
        const std::string s_trimmed = trim_whitespace(s_line);
        if (!s_trimmed.empty()) {
            o_info.signature_result_note = s_trimmed;
            break;
        }
    }
    if (o_info.signature_result_note.empty()) {
        o_info.signature_result_note = "gpg refused the signature";
    }
    return true;
}

std::string appimage_signature_label(const appimage_info_o &o_info) {
    switch (o_info.signature) {
        case appimage_signature_e::absent:
            return {};
        case appimage_signature_e::empty_padding:
            return "present (empty padding)";
        case appimage_signature_e::unrecognised:
            return "present (not a digest or a signature)";
        case appimage_signature_e::hex_digest:
        case appimage_signature_e::pgp_signature:
            break;
    }
    const std::string s_kind =
        appimage_signature_e::hex_digest == o_info.signature ? "payload digest" : "signature";
    switch (o_info.signature_result) {
        case appimage_signature_result_e::verified:
            return "present, " + s_kind + " verified";
        case appimage_signature_result_e::mismatch:
            return "present, " + s_kind + " MISMATCH";
        case appimage_signature_result_e::unverifiable:
            return "present, " + s_kind + " not verified: " + o_info.signature_result_note;
        case appimage_signature_result_e::not_checked:
            break;
    }
    return "present, " + s_kind + " not checked";
}

}  // namespace gnome_appimage::appimage
