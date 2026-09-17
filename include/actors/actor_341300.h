#ifndef ACTOR_341300_H
#define ACTOR_341300_H

#include "common.h"

#include <psyq/inline_c.h>
#include <psyq/libgte.h>

/// `gte_rtps` as the retail build emits it: the full `rtps` word, where the
/// `inline_c.h` macro of that name assembles to `0x7F`.
#define gte_rtps_real() __asm__ volatile("nop; nop; .word 0x4A180001")

/// One step of gameplay's LCG, `state = state * 5 + 0x71357911`, as its high half.
#define ACTOR_341300_RAND() ((Gp_LcgState = Gp_LcgState * 5 + 0x71357911) >> 16)

/// 0x30 block `func_actor_341300_80162878` allocates into `Task::idMap`: a
/// tumbling Gouraud triangle shard with its own spin and velocity.
typedef struct {
    /* 0x00 */ SVECTOR rot;
    /* 0x08 */ SVECTOR rotSpeed;
    /* 0x10 */ SVECTOR vel;
    /* 0x18 */ SVECTOR verts[3];
} Actor341300Shard;
STATIC_ASSERT_SIZEOF(Actor341300Shard, 0x30);

/// Spawn positions the shards start from, indexed by `Task::spawnArg1`.
extern SVECTOR D_actor_341300_80165A38[];

extern u32 Gp_LcgState;

#endif
