#ifndef ROOMS_SHARED_8017DAB4_H
#define ROOMS_SHARED_8017DAB4_H

#include "common.h"

/// The parking lot's script-event hook: cues one of the room's two stage
/// sounds for events 9 and 10 and reports "handled nothing" for every other
/// one. The day and night parking lot both carry this body.
s32 RoomsShared8017dab4(s32 arg0, s32 arg1, s32 arg2);

#endif // ROOMS_SHARED_8017DAB4_H
