#ifndef ACTORS_SHARED_80137EA8_H
#define ACTORS_SHARED_80137EA8_H

#include "common.h"

#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>

/// Work block the carriers hang off their context's 0x1C slot (the task's
/// `Task::idMap`, which is not a `TaskIdMap` here). `coord` is the extra
/// `GsCOORDINATE2` `ActorsShared80137e18` wires as `sub` of the model's
/// second part; `field_34E` is that node's Y scale in 4096-per-unit fixed
/// point, the middle of the X/Y/Z trio that spawn path arms to 0x1000.
typedef struct ActorShared80137ea8Work {
    /* 0x000 */ byte          pad_0[0x2EC];
    /* 0x2EC */ GsCOORDINATE2 coord;
    /* 0x33C */ byte          pad_33C[0x12];
    /* 0x34E */ u16           field_34E; // Y scale; 4096 = one unit
} ActorShared80137ea8Work;
STATIC_ASSERT_SIZEOF(ActorShared80137ea8Work, 0x350);

/// Actor context handed to this body: `field_1C` is the work block above.
/// Same shape as the other actor overlays' contexts.
typedef struct ActorShared80137ea8 {
    /* 0x00 */ byte                     pad_0[0x1C];
    /* 0x1C */ ActorShared80137ea8Work* field_1C;
} ActorShared80137ea8;

/// Drops the extra coordinate's Y scale by 2, applies that scale to the Y
/// column of its matrix, and clears `coord.flg` so the next `GsGetLw` rebuilds
/// it. Shared verbatim by `actor_107000` and `actor_207000`.
void ActorsShared80137ea8(ActorShared80137ea8* arg0);

#endif
