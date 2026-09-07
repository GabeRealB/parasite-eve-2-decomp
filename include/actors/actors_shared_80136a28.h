#ifndef ACTORS_SHARED_80136A28_H
#define ACTORS_SHARED_80136A28_H

#include "common.h"

#include "gameplay/1BC.h"
#include "gameplay/3A34.h"
#include "main/task.h"

/// Work block the four actor_105600-family overlays hang off `Task::idMap`
/// (not a `TaskIdMap` here). The three display nodes at +0x40, +0x78 and
/// +0x98 are the ones `ActorsShared80136a28` hands back to `Gp_UnlinkObj`
/// on state 0. `field_E8` / `field_EC` are the death-timer / sub-state pair
/// that same function writes: state 0 clears the timer and advances to 1;
/// state 1 counts frames and calls `Gp_DestroyEnemy` once the timer
/// reaches 0x3D.
typedef struct ActorsShared80136a28Work {
    /* 0x000 */ byte  pad_0[0x40];
    /* 0x040 */ GpObj obj40;
    /* 0x060 */ byte  pad_60[0x18];
    /* 0x078 */ GpObj obj78;
    /* 0x098 */ GpObj obj98;
    /* 0x0B8 */ byte  pad_B8[0x30];
    /* 0x0E8 */ u16   field_E8;
    /* 0x0EA */ byte  pad_EA[2];
    /* 0x0EC */ s16   field_EC;
} ActorsShared80136a28Work;

void ActorsShared80136a28(GpEnemy* arg0, Task* arg1);

#endif
