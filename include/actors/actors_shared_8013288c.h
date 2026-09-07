#ifndef ACTORS_SHARED_8013288C_H
#define ACTORS_SHARED_8013288C_H

#include "common.h"

#include "main/task.h"

/// The part of the `ActorsShared80131f9c` work block this body touches. Each
/// carrier's block is its own type (see `include/actors/actor_151000.h` for
/// the full 0x4C0 layout); the shared unit only names the two fields the
/// handler writes.
typedef struct ActorsShared8013288cWork {
    /* 0x000 */ byte pad_0[0x4B4];
    /* 0x4B4 */ s16  field_4B4;
    /* 0x4B6 */ byte pad_4B6[0x6];
    /* 0x4BC */ u8   field_4BC;
    /* 0x4BD */ byte pad_4BD[0x3];
} ActorsShared8013288cWork;
STATIC_ASSERT_SIZEOF(ActorsShared8013288cWork, 0x4C0);

extern ActorsShared8013288cWork* ActorsShared80131f9cWork;

/// Payload the message handlers take as `Gp_DispatchMsg`'s `arg2`.
typedef struct ActorsShared8013288cMsg {
    /* 0x0 */ u16 field_0;
    /* 0x2 */ u16 field_2;
} ActorsShared8013288cMsg;
STATIC_ASSERT_SIZEOF(ActorsShared8013288cMsg, 0x4);

s32 ActorsShared8013288c(Task* task, s32 arg1, ActorsShared8013288cMsg* msg);

#endif
