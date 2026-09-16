#ifndef ACTOR_421600_H
#define ACTOR_421600_H

#include "common.h"

#include <psyq/libgte.h>

#include "gameplay/1BC.h"
#include "gameplay/3A34.h"
#include "main/task.h"
#include "main/tmd.h"

/// Per-actor state block for the `actor_421600` overlay's enemy.
///
/// `func_actor_421600_80134AD4` allocates it with `Mem_Calloc(0xEB0, 0)` and
/// stores it in the `Task::idMap` slot (0x1C), so the size below is the
/// allocation rather than a guess: this actor reuses that pointer field for its
/// own work block and it is *not* a `TaskIdMap` here. Reach it with
/// `(Actor421600Work*)task->idMap`.
///
/// Only the fields the decompiled code touches are named so far: the three
/// `GpObj` display nodes `func_actor_421600_8013E668` hands back to
/// `Gp_UnlinkObj`, the two child tasks it kills, and the halfword
/// `func_actor_421600_8013E654` writes. The display nodes sit 0x24 later than
/// the 0x8C8/0xA08/0xB48 triple on actor 01900/401800, with the same 0x140
/// stride. `field_4` is the live-actor flag `func_actor_421600_8013E858`
/// tests, and `field_B6C.flags` is the mask it writes. `field_0` / `field_68`
/// and the 0x828 motion halfwords are the same cluster `Actor00100_Fn0B730`
/// uses; `field_8EC.field_1C` is the 0x908 store. `field_B8C` is the
/// `GpRec18` table `func_actor_421600_8013285C` walks after the 0x20-byte
/// `field_B6C` node, matching `Actor00100Obj.field_20` after `objs[2]`.
/// `field_E90` is read as a word (not the `s16` actor 444000 keeps at the same
/// offset); `func_actor_421600_8013E9D8` masks it to 24 bits and compares that
/// with 0x11402 to pick the state it writes to `field_0`.
typedef struct Actor421600Work {
    /* 0x000 */ s16  field_0;
    /* 0x002 */ byte pad_2[2];
    /* 0x004 */ s16  field_4;
    /* 0x006 */ byte pad_6[0x54];
    /// Animation step counter masked to 0x3FF by the state handlers; the
    /// model-shrink tails wait for it to read 0xC.
    /* 0x05A */ u16     field_5A;
    /* 0x05C */ byte    pad_5C[0xC];
    /* 0x068 */ u16     field_68;
    /* 0x06A */ byte    pad_6A[0x7BE];
    /* 0x828 */ u16     field_828;
    /* 0x82A */ u16     field_82A;
    /* 0x82C */ byte    pad_82C[2];
    /* 0x82E */ s16     field_82E;
    /* 0x830 */ byte    pad_830[2];
    /* 0x832 */ u16     field_832;
    /* 0x834 */ byte    pad_834[0xA];
    /* 0x83E */ u16     field_83E;
    /* 0x840 */ u16     field_840;
    /* 0x842 */ byte    pad_842[2];
    /* 0x844 */ s16     field_844;
    /* 0x846 */ byte    pad_846[0xA6];
    /* 0x8EC */ GpObj   field_8EC;
    /* 0x90C */ byte    pad_90C[0x120];
    /* 0xA2C */ GpObj   field_A2C;
    /* 0xA4C */ byte    pad_A4C[0x120];
    /* 0xB6C */ GpObj   field_B6C;
    /* 0xB8C */ GpRec18 field_B8C;
    /* 0xBA4 */ byte    pad_BA4[0x2EC];
    /* 0xE90 */ s32     field_E90;
    /* 0xE94 */ Task*   field_E94;
    /* 0xE98 */ Task*   field_E98;
    /* 0xE9C */ byte    pad_E9C[0x10];
    /* 0xEAC */ s16     field_EAC;
    /* 0xEAE */ byte    pad_EAE[2];
} Actor421600Work;
STATIC_ASSERT_SIZEOF(Actor421600Work, 0xEB0);

/// Per-task actor context: `field_1C` is the work block above (the same
/// pointer `Task::idMap` holds), `field_20` the `GpEnemy` in
/// `Task::spawnArg2`, and `field_2C` the actor's `TmdObject`. Same shape as
/// `Actor403000` / `Actor401800`.
typedef struct Actor421600 {
    /* 0x00 */ byte             pad_0[0x1C];
    /* 0x1C */ Actor421600Work* field_1C;
    /* 0x20 */ GpEnemy*         field_20;
    /* 0x24 */ byte             pad_24[8];
    /* 0x2C */ TmdObject*       field_2C;
} Actor421600;

/// The overlay's pose table: 8-byte records of three halfwords at 0x0/0x2/0x4
/// plus padding, i.e. `SVECTOR`s. Indexed by the low signed halfword of the
/// caller's id. `actor_403000` keeps a table of the same shape at 0x80158CE0
/// and reaches it with a body the shared-body index groups with this one; a
/// body that reads its own overlay's data cannot be promoted, so each carrier
/// keeps a plain-C copy -- `src/actors/actor_403000/actor_403000.c` for the
/// other.
extern SVECTOR D_actor_421600_80151158[];

/// 4-byte table indexed by `(arg0 > 0) + ((arg1 < 1) << 1)`.
extern s8 D_actor_421600_801511D0[];

/// `Task::exitCallback` teardown: kill the two helper tasks, unlink the three
/// display nodes, drop the enemy's `field_54` slot, then `Gp_DestroyEnemy`.
void func_actor_421600_8013E668(Task* task);

/// Copy the `vx`/`vy`/`vz` of record `arg1` of the pose table into `arg0`.
void func_actor_421600_8013E7F8(SVECTOR* arg0, s32 arg1);

s8 func_actor_421600_8013E830(s32 arg0, s32 arg1);

void func_actor_421600_8013E858(Actor421600* arg0);

/// Write the actor's start state: `field_0` becomes 2, or 5 when the id word
/// `field_E90` masks down to 0x11402. Also runs the shared spawn/teardown tail
/// that `func_actor_421600_8013EAAC` / `_8013EB7C` share.
void func_actor_421600_8013E9D8(Actor421600* arg0);

void func_actor_421600_80134604(Actor421600* arg0);

s32  func_actor_421600_8013285C(GsCOORDINATE2* coord, GpRec18* movement, s16 arg2);
void func_actor_421600_8013EAAC(Actor421600* arg0);
void func_actor_421600_8013EB7C(Actor421600* arg0);

#endif
