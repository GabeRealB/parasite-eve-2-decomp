#ifndef ACTOR_105300_H
#define ACTOR_105300_H

#include "common.h"
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>

#include "main/session.h"
#include "main/task.h"

#include "gameplay/3FB8.h"

/// The model object in `Task::extra`, seen through this overlay: `field_8` is
/// the object's trailing `GsCOORDINATE2` array. The spawn seeds one of those
/// coordinates from `D_actor_105300_80133A20` and hands it to the enemy as
/// `GpEnemy::field_18`.
typedef struct Actor05300Obj2C {
    /* 0x00 */ byte           pad_0[8];
    /* 0x08 */ GsCOORDINATE2* field_8;
} Actor05300Obj2C;

/// Work block of the task this enemy hangs off -- the spawn reaches it as
/// `task->parent->idMap`, the same slot `ActorsShared80133838` (the shared
/// teardown of `actor_105300` / `actor_105400`) reads as the sound id
/// `field_31C`. `field_334` is the enemy's sub-state index: it selects the
/// spawn position in `D_actor_105300_80133A20` and also which of the two
/// per-enemy death flags the spawn raises.
typedef struct Actor05300Work {
    /* 0x000 */ byte pad_0[0x29C];
    /* 0x29C */ s32  field_29C;
    /* 0x2A0 */ byte pad_2A0[0x94];
    /* 0x334 */ s16  field_334;
    /* 0x336 */ byte pad_336[2];
    /* 0x338 */ s16  field_338;
} Actor05300Work;

/// 0x48-byte part object the spawn allocates with `Mem_Calloc` and parks in
/// `Task::idMap`. It leads with the `GpObj` list node linked into
/// `Gp_ObjLists[2]` -- and the one the shared teardown hands back to
/// `Gp_UnlinkObj` -- so `obj.field_C` is the single-entry `GpRec18` collision
/// table at 0x20. `field_38` is the same coordinate `obj.field_8` points at,
/// and `field_46` is the `ActorsShared80133838Work::field_46` the teardown
/// reads back to pick its death flag.
typedef struct Actor05300Part {
    /* 0x00 */ GpObj          obj;
    /* 0x20 */ GpRec18        rec18[1];
    /* 0x38 */ GsCOORDINATE2* field_38;
    /* 0x3C */ s16            field_3C;
    /* 0x3E */ s16            field_3E;
    /* 0x40 */ byte           pad_40[2];
    /* 0x42 */ u16            field_42;
    /* 0x44 */ byte           pad_44[2];
    /* 0x46 */ u16            field_46;
} Actor05300Part;
STATIC_ASSERT_SIZEOF(Actor05300Part, 0x48);

/// The task whose work block is `Actor05300Work`, reached as `task->field_1C`
/// (the `Task::idMap` slot).
typedef struct Actor05300 {
    /* 0x00 */ byte            pad_0[0x1C];
    /* 0x1C */ Actor05300Work* field_1C;
} Actor05300;

/// Spawn position copied into a coordinate's translation, one entry per
/// `Actor05300Work::field_334` sub-state.
typedef struct Actor05300SpawnPos {
    /* 0x0 */ s16 x;
    /* 0x2 */ s16 y;
    /* 0x4 */ s16 z;
} Actor05300SpawnPos;
STATIC_ASSERT_SIZEOF(Actor05300SpawnPos, 0x6);

s16 Actor05300_Fn01B70(Actor05300* arg0);

#endif
