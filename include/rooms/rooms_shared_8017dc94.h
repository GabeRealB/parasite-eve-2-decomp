#ifndef ROOMS_SHARED_8017DC94_H
#define ROOMS_SHARED_8017DC94_H

#include "common.h"

/// The driveway's script-event hook: cues the room's stage sound 6 for the two
/// driveway events and reports "handled nothing" for every other one.
/// The day and night driveway both carry this body.
s32 RoomsShared8017dc94(s32 arg0, s32 arg1, s32 arg2);

#endif // ROOMS_SHARED_8017DC94_H
