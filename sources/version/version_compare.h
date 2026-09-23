#ifndef GNOME_APPIMAGE_VERSION_COMPARE_H
#define GNOME_APPIMAGE_VERSION_COMPARE_H

#include <string>

namespace gnome_appimage::version {

// Compare two version strings, oldest first.
//
// A version string is split into the runs a person reads: digits stay together and
// compare as numbers, letters stay together and compare case-insensitively, and
// separators are not tokens.  So 1.1.3 < 1.1.10 and 26.3.0 > 5.13.0.  A position with
// no token counts as zero against a number and as a release against letters, so
// 1.0 < 1.0.1 and 1.0rc1 < 1.0.  Versions that are not comparable (empty, or no shared
// shape) still get a stable answer rather than an exception.
//
// Returns a negative number when the left version is older, zero when the two are the
// same version, and a positive number when the left version is newer.
int compare_versions(const std::string &s_left, const std::string &s_right);

}  // namespace gnome_appimage::version

#endif
