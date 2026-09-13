#ifndef ACTOR_460200_H
#define ACTOR_460200_H

#include "common.h"

#include "gameplay/1BC.h"
#include "main/task.h"

/// Work block this overlay's actors hang off their task's `Task::idMap` slot
/// (0x1C), which is not a `TaskIdMap` here. `ActorsShared80132514Work`
/// describes the same block from the animation-reset path's side.
///
/// `state` drives `func_actor_460200_801325FC`: 1 starts the animation through
/// `func_actor_460200_80132AC8` and 2 through `func_actor_460200_80132A50`,
/// both of which then advance it to 3. `animId` is the clip those slots are
/// seeded with and `animArg` the extra argument the first path carries; they
/// are the same two fields `ActorsShared80132514Work` names `field_4B8` /
/// `field_4EC`, which is also where `func_actor_460200_80132A50` latches
/// `animId` onto `field_4B6`.
typedef struct Actor460200Work {
    /* 0x000 */ byte      pad_0[0x40];
    /* 0x040 */ GpAnimCtx anim;
    /* 0x054 */ byte      pad_54[0x460];
    /* 0x4B4 */ s16       state;
    /* 0x4B6 */ s16       field_4B6;
    /* 0x4B8 */ u16       animId;
    /* 0x4BA */ s16       field_4BA;
    /* 0x4BC */ byte      pad_4BC[0x30];
    /* 0x4EC */ u16       animArg;
} Actor460200Work;

/// Argument block of the script opcode `func_actor_460200_80132B2C`
/// implements: which animation to play, and how. Same layout as the
/// `Actor150400AnimArgs` that opcode's twin takes.
typedef struct Actor460200AnimArgs {
    /* 0x0 */ byte pad_0[4];
    /* 0x4 */ s32  animId;
    /* 0x8 */ s32  withArg;
    /* 0xC */ u16  animArg;
} Actor460200AnimArgs;

void func_actor_460200_801325FC(Task* task);

s32 func_actor_460200_80132B2C(Task* task, s32 arg1, Actor460200AnimArgs* args);

s32 func_actor_460200_80133C64(Task* task, s32 arg1, Actor460200AnimArgs* args);

#endif
