#ifndef GNOME_APPIMAGE_APPIMAGE_SIGNATURE_H
#define GNOME_APPIMAGE_APPIMAGE_SIGNATURE_H

#include <string>

#include "appimage/appimage_reader.h"

namespace gnome_appimage::appimage {

// The two enums this uses, appimage_signature_e and appimage_signature_result_e, are
// declared with the reader's facts in appimage_reader.h.

// Read the section content and classify it.  Fills the signature fields of o_info;
// o_info.signature_is_empty is kept in step.
void classify_appimage_signature(const std::string &s_content, appimage_info_o &o_info);

// Compute the digest the section covers and check it: the whole file, with the
// signature section treated as zero padding.  A hex digest is compared directly; a
// signature is checked with `gpg --verify` against the digest computed here.  A
// missing gpg, or a signature whose key is not in the keyring, is reported as
// unverifiable rather than as a mismatch.
//
// Hashing reads the whole file, so callers decide when to pay for it.
bool verify_appimage_signature(const std::string &s_path, appimage_info_o &o_info,
                               std::string &s_error);

// A label for the plan and the logs: empty when there is no section, then
// "present (empty padding)", "present, payload digest verified", and so on.
std::string appimage_signature_label(const appimage_info_o &o_info);

}  // namespace gnome_appimage::appimage

#endif
