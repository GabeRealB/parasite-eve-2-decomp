#ifndef ACTOR_450800_H
#define ACTOR_450800_H

#include "common.h"

#include <psyq/libgte.h>

#include "gameplay/1BC.h"
#include "main/task.h"

/// Spawn offset `func_actor_450800_80132108` copies into a local and hands to
/// `Gp_SpawnEff` as the effect's position. Stays in assembly (the symbol's
/// rodata run reaches past it), so it is listed `force_not_migration` in the
/// overlay's symbol map and pulled in by its own `INCLUDE_RODATA`.
extern SVECTOR D_actor_450800_80131E24;

/// Work block this overlay hangs off the task's `Task::idMap` slot (0x1C),
/// which is not a `TaskIdMap` here. `func_actor_450800_80132160` allocates it
/// with `Mem_Calloc(0x504, 0)` and stores it straight into that field, so the
/// size below is the allocation and not a guess. Reach it with
/// `(Actor450800Work*)task->idMap`.
///
/// The three tasks at +0x4F0 .. +0x4F8 are the helper tasks this actor spawns;
/// the exit callback `func_actor_450800_80132868` kills all three on teardown.
///
/// `yaw` and `travel` are the cache the "walk to" placement opcode writes:
/// the heading it applied to the root coordinate and the distance left to
/// cover, in twelfths. That opcode is the shared body `ActorsShared80133678`,
/// which reaches the same two fields through `ActorsShared80133678Work`.
///
/// `anim` is the animation context `func_800B4114` walks. `field_4B8` is the
/// current animation id; `func_actor_450800_80132AE0` starts slots 1..0x13 of
/// `anim` from it, forwarding `field_4FC` as the reset argument, and latches the
/// id into `field_4B6` as the copy kept for change detection.
typedef struct Actor450800Work {
    /* 0x000 */ byte      pad_0[0x40];
    /* 0x040 */ GpAnimCtx anim;
    /* 0x054 */ byte      pad_54[0x45A];
    /* 0x4AE */ u16       yaw;
    /* 0x4B0 */ byte      pad_4B0[0x2];
    /* 0x4B2 */ s16       travel;
    /* 0x4B4 */ byte      pad_4B4[0x2];
    /* 0x4B6 */ s16       field_4B6;
    /* 0x4B8 */ s16       field_4B8;
    /* 0x4BA */ byte      pad_4BA[0x36];
    /* 0x4F0 */ Task*     field_4F0;
    /* 0x4F4 */ Task*     field_4F4;
    /* 0x4F8 */ Task*     field_4F8;
    /* 0x4FC */ s16       field_4FC;
    /* 0x4FE */ byte      pad_4FE[0x6];
} Actor450800Work;
STATIC_ASSERT_SIZEOF(Actor450800Work, 0x504);

#endif
