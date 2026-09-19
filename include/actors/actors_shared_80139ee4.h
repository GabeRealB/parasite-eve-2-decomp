#ifndef ACTORS_SHARED_80139EE4_H
#define ACTORS_SHARED_80139EE4_H

#include "common.h"

#include "gameplay/1BC.h"
#include "gameplay/3A34.h"
#include "main/task.h"
#include "main/tmd.h"

/// 0x34-byte block taken from `G_SCRATCH_HEAD`: a `MATRIX` plus the `VECTOR`
/// handed to `ScaleMatrix` and the yaw stored before `Gfx_RotMatrixY`.
typedef struct ActorsShared80139ee4Scratch {
    /* 0x00 */ MATRIX m;
    /* 0x20 */ VECTOR scale;
    /* 0x30 */ s16    angle;
    /* 0x32 */ s16    pad_32;
} ActorsShared80139ee4Scratch;
STATIC_ASSERT_SIZEOF(ActorsShared80139ee4Scratch, 0x34);

/// Work block of the enemies that share `ActorsShared80139ee4`, reached through
/// the task's `Task::work` slot. Only what this body touches is modelled here;
/// the same offsets are described in full by `Actor444000DropWork`
/// (`include/actors/actor_444000.h`).
typedef struct ActorsShared80139ee4Work {
    /* 0x000 */ byte          pad_0[0x10];
    /* 0x010 */ GsCOORDINATE2 coord; ///< kept on the floor under the model
    /* 0x060 */ byte          pad_60[0x50];
    /* 0x0B0 */ GpObj         obj;   ///< collision node; `radius` is the marker size
    /* 0x0D0 */ byte          pad_D0[0x20];
    /* 0x0F0 */ GpRec18       rec;
    /* 0x108 */ byte          pad_108[0xA4];
    /* 0x1AC */ u16           timer; ///< step counter within the state
    /* 0x1AE */ byte          pad_1AE[0x12];
} ActorsShared80139ee4Work;
STATIC_ASSERT_SIZEOF(ActorsShared80139ee4Work, 0x1C0);

/// That enemy's task, named for the slots this body reaches through it.
typedef struct ActorsShared80139ee4Task {
    /* 0x00 */ byte                      pad_0[0x1C];
    /* 0x1C */ ActorsShared80139ee4Work* field_1C;
    /* 0x20 */ byte                      pad_20[0xC];
    /* 0x2C */ TmdObject*                extra;
    /* 0x30 */ s32                       state;
} ActorsShared80139ee4Task;

void ActorsShared80139ee4(GpEnemy* enemy, ActorsShared80139ee4Task* task);

#endif
