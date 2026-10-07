#ifndef SRC_SHARED_ACTOR_MOTION_WALK_HELPERS_H
#define SRC_SHARED_ACTOR_MOTION_WALK_HELPERS_H

#include "types.h"

/// Measures one arrival gap before its signed-halfword narrowing.
///
/// Borrows two coordinates in the root's parent frame. Their signed full-word
/// difference must fit a word; its sign selects which unsigned low halfwords
/// to subtract. This preserves wrap at 65536 instead of taking a full-word
/// absolute value. The arrival test then narrows the result to a signed halfword.
static inline s32 _actorMotionWalkAxisGap(const long* targetAxis, const long* rootAxis)
{
    s32 gap;

    if (*targetAxis - *rootAxis >= 0) {
        gap = (u16)*targetAxis - (u16)*rootAxis;
    } else {
        gap = (u16)*rootAxis - (u16)*targetAxis;
    }
    return gap;
}

#endif
