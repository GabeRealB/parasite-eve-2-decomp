#ifndef ACTOR_361100_H
#define ACTOR_361100_H

#include "common.h"

#include <psyq/libgte.h>

#include "main/task.h"
#include "main/tmd.h"

/// Work block allocated by `func_actor_361100_80162D28` and
/// `func_actor_361100_80163410` (`Mem_Calloc(0x4A4)`)
/// and parked in that task's `Task::idMap` slot -- that slot is not a
/// `TaskIdMap` here. `func_actor_361100_80162E04` and
/// `func_actor_361100_801634B4` republish the two matrices
/// onto `TmdObject::field_1C` / `field_20`, the light/colour pair
/// `Gp_BindDefaultMtx` otherwise points at `Gp_DefaultMtx` / `Gp_DefaultMtx2`,
/// exactly as `func_actor_350700_801624B4` does for `Actor350700Work`.
///
/// The size is the allocation, and the fields below are the ones the inits
/// seed: the three `sb` bytes at 0x43D/0x43E/0x4A2 are set to -1, and
/// `func_actor_361100_80162D28` also clears the three words at 0x480..0x488.
///
/// The six words at 0x480..0x498 are two groups of three, four bytes apart
/// within a group and twelve between them: `func_actor_361100_801630D4` writes
/// only the 0x490 group, `func_actor_361100_80162F58` clears both, and the two
/// words that fall between them (0x48C, 0x49C) are never touched by anything in
/// this overlay, which is the `pad` slot of a `VECTOR` apiece.
///
/// `field_4A0` is the halfword the 0x7DB handler `func_actor_361100_80163750`
/// arms alongside the first group, next door to the byte
/// `func_actor_361100_80163670` writes.
typedef struct Actor361100Work {
    /* 0x000 */ byte   pad_0[0x43D];
    /* 0x43D */ s8     field_43D;
    /* 0x43E */ s8     field_43E;
    /* 0x43F */ byte   pad_43F[0x1];
    /* 0x440 */ MATRIX light;
    /* 0x460 */ MATRIX color;
    /* 0x480 */ s32    field_480;
    /* 0x484 */ s32    field_484;
    /* 0x488 */ s32    field_488;
    /* 0x48C */ byte   pad_48C[0x4];
    /* 0x490 */ s32    field_490;
    /* 0x494 */ s32    field_494;
    /* 0x498 */ s32    field_498;
    /* 0x49C */ byte   pad_49C[0x4];
    /* 0x4A0 */ s16    field_4A0;
    /* 0x4A2 */ s8     field_4A2;
    /* 0x4A3 */ byte   pad_4A3[0x1];
} Actor361100Work;
STATIC_ASSERT_SIZEOF(Actor361100Work, 0x4A4);

/// Placement argument block of the script opcode `func_actor_361100_80162F58`
/// handles: a world translation followed by the Euler angles handed to
/// `RotMatrixZYX`, the same block `ActorsShared8013231cArgs` and
/// `Actor503500PlaceArgs` take.
typedef struct Actor361100Placement {
    /* 0x00 */ VECTOR  pos;
    /* 0x10 */ SVECTOR rot;
} Actor361100Placement;
STATIC_ASSERT_SIZEOF(Actor361100Placement, 0x18);

/// Payload the sender of message 0x7DB passes as `Gp_DispatchMsg`'s `arg2`; the
/// same 4-byte record as `Actor335800Msg` and `Actor342400Msg`. The overlay's
/// 0x7DB handler, `func_actor_361100_80163750`, switches on the halfword at
/// 0x2, as `func_actor_104600_80133D74` and `func_actor_342400_801626AC` do
/// for theirs.
typedef struct Actor361100Msg {
    /* 0x0 */ u16 field_0;
    /* 0x2 */ u16 field_2;
} Actor361100Msg;
STATIC_ASSERT_SIZEOF(Actor361100Msg, 0x4);

/// Overlay of `GsCOORDINATE2` at `TmdObject::field_8`. Offset 0x44 (libgs's
/// `param`, with `super` at 0x48) holds the Euler angles the code writes and
/// then hands straight to `RotMatrixZYX`, the same reuse `RoomCoord` and
/// `ActorsShared8013231cCoord` document.
typedef struct Actor361100Coord {
    /* 0x00 */ s32     flg;
    /* 0x04 */ MATRIX  coord;
    /* 0x24 */ MATRIX  workm;
    /* 0x44 */ SVECTOR rot;
} Actor361100Coord;
STATIC_ASSERT_SIZEOF(Actor361100Coord, 0x4C);

/// Places the actor at `placement`: drops the opcode's translation straight
/// into the root part's local matrix, stores its Euler angles in the
/// coordinate's own `rot` slot and rebuilds the rotation from them with
/// `RotMatrixZYX`. Clearing `flg` makes `Gp_UpdateCoordTree` recompute the
/// world matrix from it, and the six words the body then clears are the work
/// block's two vector accumulators.
s32 func_actor_361100_80162F58(Task* task, s32 arg1, Actor361100Placement* placement);

/// Message 0x7DB handler, listed in `D_actor_361100_80171BB8`, the table the
/// task installs at `Task::field_24`. 0 parks the actor, clearing the work
/// block's first vector accumulator; 1 arms it, dropping 0x2D000 into the
/// accumulator's middle word and 0xA0 into `field_4A0`; every other sub-command
/// exits the task through its own `Task::exitCallback`.
s32  func_actor_361100_80163750(Task* task, s32 msgId, Actor361100Msg* msg);
void func_actor_361100_80162E04(Task* arg0);
void func_actor_361100_801634B4(Task* arg0);
void func_actor_361100_80163494(Task* arg0);

#endif
