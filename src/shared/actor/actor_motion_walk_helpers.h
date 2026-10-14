#ifndef SRC_SHARED_ACTOR_MOTION_WALK_HELPERS_H
#define SRC_SHARED_ACTOR_MOTION_WALK_HELPERS_H

#include "types.h"

/// Computes one coordinate-axis gap for the scripted walk's arrival test.
///
/// Borrows two signed 32-bit coordinates in the root's parent frame, with a
/// difference that fits a signed word. The full-word difference selects the
/// subtraction order, but the operands narrow to unsigned low halfwords.
/// Returns -65535..65535; the result can be negative when an input crosses a
/// multiple of 65536. Callers narrow it to a signed halfword for comparison.
static inline s32 _actorMotionWalkAxisGap(const long* targetAxis, const long* rootAxis)
{
    if (*targetAxis - *rootAxis >= 0) {
        return (u16)*targetAxis - (u16)*rootAxis;
    }
    return (u16)*rootAxis - (u16)*targetAxis;
}

#endif
