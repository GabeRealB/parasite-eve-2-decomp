#ifndef ROOMS_DRYFIELD_DILAPIDATED_HOUSE_H
#define ROOMS_DRYFIELD_DILAPIDATED_HOUSE_H

#include "common.h"

#include <psyq/libgs.h>
#include <psyq/libgte.h>

#include "main/session.h"
#include "main/task.h"
#include "main/tmd.h"

/// Work block of the task family whose state-0 init is
/// `func_dryfield_dilapidated_house_80180B84`, which allocates it with
/// `Mem_Malloc(0x6C, 0)` and parks it in the `Task::work` slot (0x1C) -- that
/// slot is *not* a `TaskIdMap` here. Reach it with
/// `(DdhCoordWork*)task->work`.
///
/// `func_dryfield_dilapidated_house_80180F5C` writes the same ramp value (from
/// `func_dryfield_dilapidated_house_80180FD8`, 0..0x1000) into all three of
/// `field_0` / `field_4` / `field_8`; `func_dryfield_dilapidated_house_80181028`
/// rebuilds `mtx` as the identity and then composes it against the parent's
/// `GsCOORDINATE2` chain, and `func_dryfield_dilapidated_house_80180B84` copies
/// `mtx` verbatim into a spawned child's `GsCOORDINATE2::coord`.
typedef struct DdhCoordWork {
    /* 0x00 */ s32    field_0;
    /* 0x04 */ s32    field_4;
    /* 0x08 */ s32    field_8;
    /* 0x0C */ MATRIX mtx;
    /* 0x2C */ byte   pad_2C[0x40];
} DdhCoordWork;
STATIC_ASSERT_SIZEOF(DdhCoordWork, 0x6C);

/// Record at `D_dryfield_dilapidated_house_8018669C` handed to
/// `func_dryfield_dilapidated_house_80180A0C` together with the complement of a
/// 0..0x1000 ramp. Both pairs are a pointer and the entry count of the array it
/// points at: `field_0` / `field_16` and `field_8` / `field_12`; `field_14` is
/// the index the caller is currently at, and the callee walks from it.
typedef struct DdhRoomRec {
    /* 0x00 */ void*    field_0;
    /* 0x04 */ s32      field_4;
    /* 0x08 */ SVECTOR* field_8;
    /* 0x0C */ s32      field_C;
    /* 0x10 */ s16      field_10;
    /* 0x12 */ s16      field_12;
    /* 0x14 */ s16      field_14;
    /* 0x16 */ s16      field_16;
} DdhRoomRec;
STATIC_ASSERT_SIZEOF(DdhRoomRec, 0x18);

/// Work block of the handler table at `D_dryfield_dilapidated_house_8017D61C`,
/// whose state 0 is `func_dryfield_dilapidated_house_8018118C`: allocated with
/// `Mem_Malloc(0x24, 0)` and parked in the `Task::work` slot. It holds a
/// snapshot of the placed model coordinate's matrix (`mtx`, copied from
/// `RoomCoord::coord`) plus one 0x1000 word.
typedef struct DdhModelWork {
    /* 0x00 */ MATRIX mtx;
    /* 0x20 */ s32    field_20;
} DdhModelWork;
STATIC_ASSERT_SIZEOF(DdhModelWork, 0x24);

/// Work block of the state family at `D_dryfield_dilapidated_house_8017D634`,
/// whose state 0 is `func_dryfield_dilapidated_house_801814B4`: allocated with
/// `Mem_Malloc(0x40, 0)` and parked in the `Task::work` slot. One angle step per
/// model part, each the matching entry of `D_dryfield_dilapidated_house_80186804`
/// scaled by the task's spawn arg 1 and wrapped into the 0x4000 angle period.
/// `func_dryfield_dilapidated_house_80180738` advances the same table against a
/// running per-part angle.
typedef struct DdhAngleStep {
    /* 0x00 */ s32 step[16];
} DdhAngleStep;
STATIC_ASSERT_SIZEOF(DdhAngleStep, 0x40);

/// Work block of the three effect handlers `func_dryfield_dilapidated_house_80182744`,
/// `func_dryfield_dilapidated_house_80183C8C` and
/// `func_dryfield_dilapidated_house_80183D5C`, reached as `task->spawnArg2`
/// and handed to `Gp_ReleaseState1CMem` when their ramp runs out. `field_24` is a
/// scale and `field_26` an angle in the 0x100-step rotation space: the pair starts
/// at 0x80 / 0x100, steps by -8 and +0x80 per frame and drives one
/// `Gfx_RotMatrixZ` + `Gp_UpdateCoord` + draw call per frame. `field_22` is the
/// per-frame tick the task rolls back while the `Gp_State1C` fade is armed;
/// `field_20` and `field_28` are a third ramp value the two `80182744` states
/// seed from one `Gp_LcgState` draw and hand to the same draw routine.
typedef struct DdhEffWork {
    /* 0x00 */ byte pad_00[0x20];
    /* 0x20 */ u16  field_20;
    /* 0x22 */ u16  field_22;
    /* 0x24 */ s16  field_24;
    /* 0x26 */ s16  field_26;
    /* 0x28 */ s16  field_28;
} DdhEffWork;
STATIC_ASSERT_SIZEOF(DdhEffWork, 0x2A);

/// 0x28-byte scratch `func_dryfield_dilapidated_house_801823B8` carves off
/// `G_SCRATCH_HEAD` for one beam segment. `v` is the quad's four corners, taken
/// from `workm.t` of two adjacent slots on each trail. `otz` is `gte_stszotz`
/// of that projection: closer than 0x11 drops the quad, otherwise it picks the
/// OT bucket the `POLY_G4` is linked into.
typedef struct DdhBeamScratch {
    /* 0x00 */ s32     otz;
    /* 0x04 */ s32     unused;
    /* 0x08 */ SVECTOR v[4];
} DdhBeamScratch;
STATIC_ASSERT_SIZEOF(DdhBeamScratch, 0x28);

/// Argument block `func_dryfield_dilapidated_house_8017E9A4` hands its task as
/// `Task::spawnArg2`: the address of `D_dryfield_dilapidated_house_80189B80`,
/// whose first halfword it has just set to that call's argument and whose second
/// is the flag the same function's cancel path raises. So the task spawned from
/// entry 0 of `D_dryfield_dilapidated_house_80183E64` runs for `duration` frames
/// and ends early once `func_dryfield_dilapidated_house_8017E9A4` is called with
/// 0. Same layout the actor family's `Actor460200CaptureArgs` describes.
typedef struct DdhCaptureArgs {
    /* 0x0 */ u16 duration;
    /* 0x2 */ s16 done;
} DdhCaptureArgs;
STATIC_ASSERT_SIZEOF(DdhCaptureArgs, 0x4);

// Cross-unit prototypes. Each function lives in the unit its address falls in;
// these are the ones a *different* unit calls.
void func_dryfield_dilapidated_house_8017E9A4(s32 arg0);
void func_dryfield_dilapidated_house_8017EBB8(Task* task);
void func_dryfield_dilapidated_house_8017EE58(Task* task);
void func_dryfield_dilapidated_house_8017F568(Task* task, SVECTOR* verts, s32 arg2);
void func_dryfield_dilapidated_house_8017FAD4(Task* task, SVECTOR* verts, s32* arg2, s32* arg3);
void func_dryfield_dilapidated_house_80180A0C(Task* task, DdhRoomRec* rec, s32 arg2);
s32  func_dryfield_dilapidated_house_80180FD8(Task* task);
void func_dryfield_dilapidated_house_80181028(Task* task);
void func_dryfield_dilapidated_house_801810F8(TmdObject* dst, TmdObject* src);
void func_dryfield_dilapidated_house_80181290(s32 p0, s32 p1, s32 p2, s32 p3, SVECTOR* coeff);

#endif // ROOMS_DRYFIELD_DILAPIDATED_HOUSE_H
