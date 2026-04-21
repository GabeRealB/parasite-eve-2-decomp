#ifndef VERSION_H
#define VERSION_H

// Defines region and revision-specific version macros based on the `VER_XXX`
// Makefile target.
//
// Two styles of version check macros are defined:
//
// - `VERSION_IS(USA)` — matches a single release exactly. Use for things that
//   differ between every build (e.g. random padding bytes or raw addresses)
//
// - `VERSION_REGION_IS(NTSC)` — matches all releases in a region. Use for
//   region-specific behavior (e.g. Japanese text rendering)

/// Checks if the build target is a specific release.
/// @param release The release identifier (JAP, USA)
#define VERSION_IS(release) defined(VER_##release)

/// Checks if the build target matches a given region.
/// @param region The region identifier (e.g. NTSC, NTSCJ, PAL)
#define VERSION_REGION_IS(region) defined(VERSION_##region)

#if defined(VER_JAP)
#define VERSION_NTSCJ         1
#define VERSION_SERIAL_DISK_1 "SLPS-02480"
#define VERSION_SERIAL_DISK_2 "SLPS-02481"
#elif defined(VER_USA)
#define VERSION_NTSC          1
#define VERSION_SERIAL_DISK_1 "SLUS-01042"
#define VERSION_SERIAL_DISK_2 "SLUS-01055"
#endif

#endif // VERSION_H
