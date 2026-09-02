#ifndef ACTOR_335800_H
#define ACTOR_335800_H

#include "common.h"

#include <psyq/libgte.h>

#include "main/task.h"
#include "main/tmd.h"

/// Work block allocated by `func_actor_335800_80163AA0` (`Mem_Calloc(0x4C8)`)
/// and parked in that task's `Task::idMap` slot -- that slot is not a
/// `TaskIdMap` here. `func_actor_335800_80163B54` republishes the two matrices
/// onto `TmdObject::field_1C` / `field_20`, the light/colour pair
/// `Gp_BindDefaultMtx` otherwise points at `Gp_DefaultMtx` / `Gp_DefaultMtx2`,
/// exactly as `func_actor_213100_8014A23C` does for `Actor213100Work`.
///
/// The size is the allocation, and the fields below are the ones the init
/// seeds: the two `sb` bytes at 0x43D/0x43E and the `sh` at 0x4C4 are set to
/// -1, and the three words at 0x4A0..0x4A8 are cleared.
typedef struct Actor335800Work {
    /* 0x000 */ byte   pad_0[0x43D];
    /* 0x43D */ s8     field_43D;
    /* 0x43E */ s8     field_43E;
    /* 0x43F */ byte   pad_43F[0x1];
    /* 0x440 */ MATRIX light;
    /* 0x460 */ MATRIX color;
    /* 0x480 */ byte   pad_480[0x20];
    /* 0x4A0 */ s32    field_4A0;
    /* 0x4A4 */ s32    field_4A4;
    /* 0x4A8 */ s32    field_4A8;
    /* 0x4AC */ byte   pad_4AC[0x18];
    /* 0x4C4 */ s16    field_4C4;
    /* 0x4C6 */ byte   pad_4C6[0x2];
} Actor335800Work;
STATIC_ASSERT_SIZEOF(Actor335800Work, 0x4C8);

/// Work block allocated by `func_actor_335800_80162640` (`Mem_Calloc(0x50C)`)
/// and parked in that task's `Task::idMap` slot -- that slot is not a
/// `TaskIdMap` here, just as with `Actor335800Work`. This is the parent
/// actor's block: the init seeds the two `sb` bytes at 0x475/0x476 and the
/// `sh` at 0x506 to -1, clears the three words at 0x4D8..0x4E0, and stores the
/// two child tasks it spawns from `D_actor_335800_8016EADC` at 0x4FC/0x500.
/// `func_actor_335800_80162F9C` republishes the light/colour matrix pair onto
/// the parent's `TmdObject::field_1C` / `field_20`, exactly as
/// `func_actor_335800_80163B54` does for `Actor335800Work`.
///
/// The size is the allocation; the fields below are the ones the init and the
/// message handlers touch.
typedef struct Actor335800MainWork {
    /* 0x000 */ byte   pad_0[0x475];
    /* 0x475 */ s8     field_475;
    /* 0x476 */ s8     field_476;
    /* 0x477 */ byte   pad_477[0x1];
    /* 0x478 */ MATRIX light;
    /* 0x498 */ MATRIX color;
    /* 0x4B8 */ byte   pad_4B8[0x20];
    /* 0x4D8 */ s32    field_4D8;
    /* 0x4DC */ s32    field_4DC;
    /* 0x4E0 */ s32    field_4E0;
    /* 0x4E4 */ byte   pad_4E4[0x18];
    /* 0x4FC */ Task*  field_4FC;
    /* 0x500 */ Task*  field_500;
    /* 0x504 */ s16    field_504;
    /* 0x506 */ s16    field_506;
    /* 0x508 */ s32    field_508;
} Actor335800MainWork;
STATIC_ASSERT_SIZEOF(Actor335800MainWork, 0x50C);

/// Payload the sender of message 0x7DB passes as `Gp_DispatchMsg`'s `arg2`.
/// `func_dryfield_night_garage_801809A4` points it at a 4-byte record
/// (`{ 0x1803, 2 }` then `{ 0x1803, 3 }`); `func_actor_104600_80133D74`
/// likewise switches on the halfword at 0x2.
typedef struct Actor335800Msg {
    /* 0x0 */ u16 field_0;
    /* 0x2 */ u16 field_2;
} Actor335800Msg;
STATIC_ASSERT_SIZEOF(Actor335800Msg, 0x4);

void func_actor_335800_80163B54(Task* arg0);

#endif
