#ifndef ROOMS_SHARED_8017DCB8_H
#define ROOMS_SHARED_8017DCB8_H

#include "common.h"

#include "main/task.h"

/// Packed view of Task::spawnArg1 for the drifting room effect. The low
/// halfword supplies mode and drawing flags; the upper bytes supply vertical
/// speed and the signed lifetime used to start the fade.
typedef struct RoomMoteArg {
    /* 0x0 */ u16 flags;
    /* 0x2 */ u8  speed;
    /* 0x3 */ s8  lifetime;
} RoomMoteArg;
STATIC_ASSERT_SIZEOF(RoomMoteArg, 0x4);

/// Moves the effect's coordinate vertically and advances its drawing phase
/// every other tick. State 1 ramps brightness up before fading; state 2 holds
/// its initial brightness until the fade. Releases the work block when dark
/// or when the room transition reaches state 4.
void RoomsShared8017dcb8(Task* task);

#endif
