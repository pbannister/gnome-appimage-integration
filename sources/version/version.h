#pragma once

namespace gnome_appimage::version {

// The build-time version string, for example 2026-09-21-main-a1b2c3d.
const char *version_string();

// The build counter, incremented once per build; not checked in.
int version_build_counter();

}  // namespace gnome_appimage::version
