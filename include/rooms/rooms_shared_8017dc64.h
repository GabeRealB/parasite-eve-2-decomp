#ifndef ROOMS_SHARED_8017DC64_H
#define ROOMS_SHARED_8017DC64_H

#include "common.h"

/// The water tower's script-event hook: cues the room's stage sound 6 for the
/// two water-tower events and reports "handled nothing" for every other one.
/// The day and night water tower both carry this body.
s32 RoomsShared8017dc64(s32 arg0, s32 arg1, s32 arg2);

#endif // ROOMS_SHARED_8017DC64_H
