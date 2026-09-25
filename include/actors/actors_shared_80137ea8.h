#ifndef ACTORS_SHARED_80137EA8_H
#define ACTORS_SHARED_80137EA8_H

#include "common.h"

#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>
#include "main/coord.h"

#include "main/task.h"

/// Work block the carriers hang off `Task::work`. `coord` is the extra
/// `GpCoord` `ActorsShared80137e18` wires as `sub` of the model's
/// second part; `field_34E` is that node's Y scale in 4096-per-unit fixed
/// point, the middle of the X/Y/Z trio that spawn path arms to 0x1000.
typedef struct ActorShared80137ea8Work {
    /* 0x000 */ byte    pad_0[0x2EC];
    /* 0x2EC */ GpCoord coord;
    /* 0x33C */ byte    pad_33C[0x12];
    /* 0x34E */ u16     field_34E; // Y scale; 4096 = one unit
} ActorShared80137ea8Work;
STATIC_ASSERT_SIZEOF(ActorShared80137ea8Work, 0x350);

/// Drops the extra coordinate's Y scale by 2, applies that scale to the Y
/// column of its matrix, and clears `coord.flg` so the next `GsGetLw` rebuilds
/// it. Shared verbatim by `actor_107000` and `actor_207000`.
void ActorsShared80137ea8(Task* task);

#endif
