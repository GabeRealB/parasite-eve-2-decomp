#ifndef INCLUDE_ACTORS_ACTORS_SHARED_80169F74_H
#define INCLUDE_ACTORS_ACTORS_SHARED_80169F74_H

#include "types.h"

/// Work block hanging off `Task::work`. Only the prefix this body reaches is
/// described: `yaw` is the heading taken from the root coordinate's Z-axis
/// after the placement rotations are applied.
typedef struct ActorsShared80169f74Work {
    /* 0x00 */ byte pad_0[0x16];
    /* 0x16 */ s16  yaw;
} ActorsShared80169f74Work;

#endif // INCLUDE_ACTORS_ACTORS_SHARED_80169F74_H
