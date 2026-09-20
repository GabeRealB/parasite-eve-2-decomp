#ifndef ACTORS_SHARED_80132604_H
#define ACTORS_SHARED_80132604_H

#include "common.h"

#include "gameplay/1BC.h"
#include "main/task.h"

/// Animation prefix of the work blocks owned by actor_511000 and actor_210700.
/// Each overlay places its own fields after this common 0x480-byte prefix.
typedef struct ActorsShared80132604Work {
    /* 0x000 */ GpAnimCtx  anim;
    /* 0x014 */ GpAnimSlot slots[0x14];
    /* 0x334 */ byte       field_334[0x140];
    /* 0x474 */ s32        field_474;
    /* 0x478 */ s32        field_478;
    /* 0x47C */ s32        field_47C;
} ActorsShared80132604Work;
STATIC_ASSERT_SIZEOF(ActorsShared80132604Work, 0x480);

/// The three words this handler reads from the animation message payload:
/// source-bank index, animation id, and blend flag. Senders may append data.
typedef struct ActorsShared80132604Args {
    /* 0x0 */ s32 field_0;
    /* 0x4 */ s32 field_4;
    /* 0x8 */ s32 field_8;
} ActorsShared80132604Args;
STATIC_ASSERT_SIZEOF(ActorsShared80132604Args, 0xC);

/// Each carrier resolves this symbol to its own animation-source table.
extern void* ActorsShared80132604Table[];

s32 ActorsShared80132604(Task* task, s32 arg1, ActorsShared80132604Args* msg, s32 arg3);

#endif
