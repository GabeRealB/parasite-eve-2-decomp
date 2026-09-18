#ifndef ACTORS_SHARED_80163148_H
#define ACTORS_SHARED_80163148_H

#include "common.h"

#include "main/task.h"

/// Fade work block `ActorsShared80163148` allocates with `Mem_Calloc(8, 0)` and
/// stores in its own task's `Task::work` slot -- an 8-byte block, not the
/// carrier overlay's own work struct. The three channels start at 0xFF and decay
/// by the task's `Task::spawnArg1` every frame, drawn by `Fade_DrawOverlay`; the
/// task kills itself once the red channel has gone negative.
typedef struct ActorsShared80163148Fade {
    /* 0x0 */ byte pad_0[0x2];
    /* 0x2 */ s16  r;
    /* 0x4 */ s16  g;
    /* 0x6 */ s16  b;
} ActorsShared80163148Fade;
STATIC_ASSERT_SIZEOF(ActorsShared80163148Fade, 0x8);

void ActorsShared80163148(Task* arg0);

#endif
