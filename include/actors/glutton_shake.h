#ifndef INCLUDE_ACTORS_GLUTTON_SHAKE_H
#define INCLUDE_ACTORS_GLUTTON_SHAKE_H

/// Vertical screen-shake requests for the Glutton host.
///
/// Each supported nonzero level selects a fixed run of frames and pixel offsets.
/// Other values do not arm a changed request; `GLUTTON_SHAKE_NONE` does not cancel
/// an active shake.
enum {
    GLUTTON_SHAKE_NONE   = 0, // No shake
    GLUTTON_SHAKE_SHORT  = 1, // 5 frames alternating 0 and 2 pixels
    GLUTTON_SHAKE_MEDIUM = 2, // 10 frames of a four-frame 0, 2, 3, 2 pattern
    GLUTTON_SHAKE_LONG   = 3, // 22 frames of an eight-frame ramp peaking at 4 pixels
};

#endif // INCLUDE_ACTORS_GLUTTON_SHAKE_H
