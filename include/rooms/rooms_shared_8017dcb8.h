#ifndef INCLUDE_ROOMS_ROOMS_SHARED_8017DCB8_H
#define INCLUDE_ROOMS_ROOMS_SHARED_8017DCB8_H

#include "common.h"

/// Packed view of Task::spawnArg1 for the drifting room effect. The low
/// halfword supplies mode and drawing flags; the upper bytes supply vertical
/// speed and the signed lifetime used to start the fade.
typedef struct RoomMoteArg {
    /* 0x0 */ u16 flags;
    /* 0x2 */ u8  speed;
    /* 0x3 */ s8  lifetime;
} RoomMoteArg;
STATIC_ASSERT_SIZEOF(RoomMoteArg, 0x4);

#endif // INCLUDE_ROOMS_ROOMS_SHARED_8017DCB8_H
