#ifndef ACTOR_342000_H
#define ACTOR_342000_H

#include "common.h"
#include <psyq/libgs.h>
#include "main/task.h"

/// Per-instance work block for the overlay's model actor.
///
/// `func_actor_342000_80162158` allocates it with `Mem_Malloc(0x2AC, 0)`,
/// `Mem_Set`s it to zero over the same 0x2AC bytes and stores it in the
/// `Task::idMap` slot (0x1C), so the size below is the allocation, not a
/// guess: the actor reuses that pointer field for its own work block and it is
/// *not* a `TaskIdMap` here. Reach it with `(Actor342000Work*)task->idMap`.
///
/// `field_2A4` is the coordinate node the actor's model is re-parented to:
/// `func_actor_342000_80162158` seeds it with `&Gfx_ViewCoord`, and the exit
/// callback `func_actor_342000_80163F88` writes it back into
/// `((GsCOORDINATE2*)((TmdObject*)task->extra)->field_8)->sub`.
typedef struct Actor342000Work {
    /* 0x000 */ byte           pad_0[0x2A4];
    /* 0x2A4 */ GsCOORDINATE2* field_2A4;
    /* 0x2A8 */ byte           pad_2A8[0x4];
} Actor342000Work;
STATIC_ASSERT_SIZEOF(Actor342000Work, 0x2AC);

/// Work block of the overlay's event/sequence task -- the one
/// `D_actor_342000_80165070` points at.
///
/// `func_actor_342000_8016382C` allocates it with `Mem_Calloc(0x80, 0)`,
/// `Mem_Set`s 0x80 bytes and stores it in that task's `Task::idMap` slot, so
/// the size is anchored. The same function publishes its owning task in
/// `D_actor_342000_80165070`, which is how the leaf helpers below reach it:
/// `(Actor342000EventWork*)D_actor_342000_80165070->idMap`.
///
/// `field_48` is the `Game_GetPtrSlot(3)` task every `Gp_DispatchMsg` in the
/// overlay is aimed at; `field_50` / `field_5C` / `field_60` / `field_64` are
/// spawned child tasks the teardown helpers kill. `field_7A` and `field_7C`
/// are once-only latches guarding a sound cue and the fade-out setup.
typedef struct Actor342000EventWork {
    /* 0x00 */ byte  pad_0[0x48];
    /* 0x48 */ Task* field_48;
    /* 0x4C */ s32   field_4C;
    /* 0x50 */ Task* field_50;
    /* 0x54 */ byte  pad_54[0x8];
    /* 0x5C */ Task* field_5C;
    /* 0x60 */ Task* field_60;
    /* 0x64 */ Task* field_64;
    /* 0x68 */ s16   field_68;
    /* 0x6A */ s16   field_6A;
    /* 0x6C */ byte  pad_6C[0x4];
    /* 0x70 */ s16   field_70;
    /* 0x72 */ s16   field_72;
    /* 0x74 */ byte  pad_74[0x6];
    /* 0x7A */ u16   field_7A;
    /* 0x7C */ u16   field_7C;
    /* 0x7E */ byte  pad_7E[0x2];
} Actor342000EventWork;
STATIC_ASSERT_SIZEOF(Actor342000EventWork, 0x80);

/// The task owning the `Actor342000EventWork` block, published by
/// `func_actor_342000_8016382C`.
extern Task* D_actor_342000_80165070;

#endif
