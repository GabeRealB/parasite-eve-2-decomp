#ifndef ACTORS_SHARED_80134318_H
#define ACTORS_SHARED_80134318_H

#include "common.h"

#include <psyq/libgte.h>

#include "gameplay/1BC.h"
#include "gameplay/3A34.h"
#include "main/task.h"

/// Work block behind the task's `work` slot (0x1C) during the death sequence
/// run by `ActorsShared80134318`. `field_100` / `field_12A` are the
/// matrix and Y scale `ActorsShared80135098` reads (`ActorShared80135098Work`);
/// `field_13E` is the death sub-state and `field_140` its frame counter.
/// `field_130` points at the `Task*` whose state is set to 4 when the sequence
/// starts.
typedef struct ActorsShared80134318Work {
    /* 0x000 */ byte   pad_0[0x40];
    /* 0x040 */ GpObj  obj_40;
    /* 0x060 */ byte   pad_60[0x60];
    /* 0x0C0 */ GpObj  obj_C0;
    /* 0x0E0 */ byte   pad_E0[0x20];
    /* 0x100 */ MATRIX field_100;
    /* 0x120 */ byte   pad_120[0xA];
    /* 0x12A */ s16    field_12A;
    /* 0x12C */ byte   pad_12C[4];
    /* 0x130 */ Task** field_130;
    /* 0x134 */ byte   pad_134[0xA];
    /* 0x13E */ s16    field_13E;
    /* 0x140 */ u16    field_140;
} ActorsShared80134318Work;

void ActorsShared80134318(GpEnemy* arg0, Task* arg1);

#endif
