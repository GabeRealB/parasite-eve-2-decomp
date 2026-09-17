#ifndef ACTOR_342000_H
#define ACTOR_342000_H

#include "common.h"
#include <psyq/libgs.h>
#include "main/task.h"

/// Position + rotation argument for the overlay's message handlers -- the
/// `arg2` of the `GpMsgEntry` table `D_actor_342000_801648E8` (id 0x7D4,
/// `func_actor_342000_801640C0`). `field_0` / `field_4` / `field_8` are copied
/// onto the actor coordinate's `coord.t`; `field_10` / `field_12` / `field_14`
/// are the euler angles its matrix is rebuilt from. Same shape as gameplay's
/// `GpXformArg`.
typedef struct _Actor342000Move {
    /* 0x00 */ s32  field_0;
    /* 0x04 */ s32  field_4;
    /* 0x08 */ s32  field_8;
    /* 0x0C */ byte pad_C[4];
    /* 0x10 */ s16  field_10;
    /* 0x12 */ s16  field_12;
    /* 0x14 */ s16  field_14;
} Actor342000Move;
STATIC_ASSERT_SIZEOF(Actor342000Move, 0x18);

/// Script command payload of the overlay's message handlers -- the `arg2` of
/// the id 0x7DB handler `func_actor_342000_80164110`, which reads `field_2`
/// and latches it in `Actor342000Work::field_2AA`. Same shape as the
/// neighbouring overlays' command payloads.
typedef struct _Actor342000Cmd {
    /* 0x0 */ byte pad_0[2];
    /* 0x2 */ u16  field_2;
} Actor342000Cmd;
STATIC_ASSERT_SIZEOF(Actor342000Cmd, 0x4);

/// A `MATRIX` plus the word-wise view `func_actor_342000_801628C8` splats the
/// actor coordinate's identity rotation through: five aligned stores rather
/// than nine halfword ones (the same idiom as `Actor311900MatWords`).
typedef union Actor342000MatWords {
    MATRIX mat;
    struct {
        /* 0x00 */ s32 m00_m01;
        /* 0x04 */ s32 m02_m10;
        /* 0x08 */ s32 m11_m12;
        /* 0x0C */ s32 m20_m21;
        /* 0x10 */ s16 m22;
    } ident;
} Actor342000MatWords;
STATIC_ASSERT_SIZEOF(Actor342000MatWords, 0x20);

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
///
/// `coord` is the actor's own rotation node. `func_actor_342000_801628C8`
/// builds `coord.coord` from the euler angles below it (`Gfx_RotMatrixY` of
/// `field_278`, then `X` of `field_274`, then `Z` of `field_27C`, word loads),
/// scales each of its columns by the matching `field_264` component through
/// `gpf 12` and clears `coord.flg`; `func_actor_342000_801640C0` writes all of
/// it from an `Actor342000Move`.
///
/// `field_264` holds that per-axis scale, 1.12 fixed point like the matrix it
/// multiplies: each column `j` is gathered into a scratchpad `SVECTOR`, run
/// through `GPF` against `field_264[j]` and written back.
///
/// `field_29C` / `field_2A0` are the actor's two child tasks; the per-frame tail
/// of `func_actor_342000_801628C8` ticks them with `func_actor_342000_80161EA4`.
///
/// `field_2AA` latches the `Actor342000Cmd::field_2` the id 0x7DB handler was
/// last called with; command 0xA additionally refills `field_264` from the
/// handler's second payload.
typedef struct Actor342000Work {
    /* 0x000 */ byte           pad_0[0x214];
    /* 0x214 */ GsCOORDINATE2  coord;
    /* 0x264 */ VECTOR         field_264;
    /* 0x274 */ s32            field_274;
    /* 0x278 */ s32            field_278;
    /* 0x27C */ s32            field_27C;
    /* 0x280 */ byte           pad_280[0x1C];
    /* 0x29C */ Task*          field_29C;
    /* 0x2A0 */ Task*          field_2A0;
    /* 0x2A4 */ GsCOORDINATE2* field_2A4;
    /* 0x2A8 */ byte           pad_2A8[0x2];
    /* 0x2AA */ u16            field_2AA;
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

/// Colour-matrix work block of the overlay's model actor:
/// `func_actor_342000_8016201C` `Mem_Malloc`s 0x44 bytes for it and parks it in
/// the task's `Task::idMap` slot (0x1C), which is *not* a `TaskIdMap` here.
///
/// The two matrices are the light/colour pair `Tmd_SetupDraw` loads: the same
/// function republishes them onto `((TmdObject*)task->extra)->field_1C` and
/// `field_20`, which otherwise point at `Gp_DefaultMtx` / `Gp_DefaultMtx2` via
/// `Gp_BindDefaultMtx`. `field_40` is the `Task::spawnArg2` spawner, reparented
/// to the actor on the spawn tick.
typedef struct Actor342000ColorMtx {
    /* 0x00 */ MATRIX light;
    /* 0x20 */ MATRIX color;
    /* 0x40 */ Task*  field_40;
} Actor342000ColorMtx;
STATIC_ASSERT_SIZEOF(Actor342000ColorMtx, 0x44);

/// The task owning the `Actor342000EventWork` block, published by
/// `func_actor_342000_8016382C`.
extern Task* D_actor_342000_80165070;

/// Message 0x7D4's static payload, handed to `Gp_DispatchMsg` by the actor's
/// spawn tick. Same shape as the `Actor342000Move` the handler takes.
extern Actor342000Move D_actor_342000_801648B8;

/// Spawns the actor's work block (`Mem_Malloc(0x2AC, 0)`, zeroed over the same
/// size), parks it in `Task::idMap`, seeds `field_2A4` and the model's part
/// coordinate, then bumps the task's state.
void func_actor_342000_80162158(Task* arg0);

/// Advances the animation banks hanging off `arg0`'s work block by `arg1`
/// ticks, one `Gp_AnimTickIndex` per bank, and reports whether a bank needed a
/// re-scan. `arg1` is the overlay's tick count -- 8 for the actor's own model
/// and 4 for the two child tasks at `Actor342000Work::field_29C`/`field_2A0`.
s32 func_actor_342000_80161EA4(Task* arg0, s32 arg1);

#endif
