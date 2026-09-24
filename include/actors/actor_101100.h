#ifndef ACTOR_101100_H
#define ACTOR_101100_H

#include "common.h"

#include "actors/actors_shared_80137fb8.h"
#include "actors/actors_shared_801385e0.h"
#include "actors/actors_shared_80138efc.h"
#include "gameplay/1BC.h"
#include "gameplay/3A34.h"
#include "gameplay/3FB8.h"
#include "gameplay/pairsrc.h"
#include "main/session.h"
#include "main/task.h"

/// Work block the spawn state allocates with `memCalloc(0x58, 0)` and parks in
/// the `Task::work` slot (0x1C), which is not a `TaskIdMap` here. Only the
/// fields the two state handlers touch are filled in: the `GpObj` at 0x08,
/// whose `flags` carry the 0xC000 pair the spawn state ORs in and the per-frame
/// state clears, and the one-entry `GpRec18` collision table at 0x40 that the
/// object's `field_C` points at (`Gp_InitRec18Table(_, 1, 0)`).
typedef struct Actor101100Work {
    /* 0x00 */ byte    pad_0[8];
    /* 0x08 */ GpObj   obj;
    /* 0x28 */ byte    pad_28[0x18];
    /* 0x40 */ GpRec18 rec[1];
} Actor101100Work;
STATIC_ASSERT_SIZEOF(Actor101100Work, 0x58);

/// Player flag byte read by the 0x801339B0 handler: when it is 1 the handler
/// takes its `GameActor::field_958` shortcut instead of measuring distance.
extern u8 D_801153F2;

/// Absolute; nonzero skips the per-frame state handler entirely.
extern u8 D_801153F4;

typedef ActorsShared80137fb8Work Actor104900Work;

/// 0xBCC-byte work block `Actor01100_Fn0097C` allocates with
/// `memCalloc` and parks in `Task::work`. The coordinate at the front is
/// linked as `coords[1].sub`; the two `GpAnimCtx` runs are what
/// `func_800B3F84` seeds. Same size as `ActorsShared80138efcWork` /
/// `ActorShared801384acWork`, which are later views of this block.
typedef struct Actor104900SpawnWork {
    /* 0x000 */ GsCOORDINATE2 coord;
    /* 0x050 */ GpAnimCtx     anim;
    /* 0x064 */ GpAnimSlot    slots[21];
    /* 0x3AC */ byte          poses[0x150];
    /* 0x4FC */ GpAnimCtx     anim2;
    /* 0x510 */ GpAnimSlot    slots2[21];
    /* 0x858 */ byte          poses2[0x150];
    /* 0x9A8 */ byte          pad_9A8[0x20];
    /* 0x9C8 */ GpObj         objs[2];
    /* 0xA08 */ byte          pad_A08[0x20];
    /// Four three-entry contact tables. `Actor01100_Fn00F58` resolves the
    /// first against the world and scans the last for a class-2 hit, then
    /// clears all four with `Gp_ClearRec18Occupied`.
    /* 0xA28 */ GpRec18 contacts[4][3];
    /* 0xB48 */ MATRIX  lightMtx;
    /* 0xB68 */ MATRIX  colorMtx;
    /* 0xB88 */ u32     actorId;
    /* 0xB8C */ byte    pad_B8C[6];
    /* 0xB92 */ s16     field_B92;
    /* 0xB94 */ byte    pad_B94[8];
    /* 0xB9C */ s16     field_B9C;
    /* 0xB9E */ byte    pad_B9E[5];
    /* 0xBA3 */ s8      field_BA3;
    /* 0xBA4 */ s8      field_BA4;
    /* 0xBA5 */ s8      field_BA5;
    /* 0xBA6 */ u8      field_BA6;
    /* 0xBA7 */ s8      state;
    /* 0xBA8 */ s8      field_BA8;
    /* 0xBA9 */ byte    pad_BA9[2];
    /* 0xBAB */ u8      field_BAB;
    /* 0xBAC */ u8      field_BAC;
    /* 0xBAD */ byte    pad_BAD;
    /* 0xBAE */ u8      field_BAE;
    /* 0xBAF */ u8      field_BAF;
    /// Placement record for the hit sparks `func_800FDB18` spawns.
    /* 0xBB0 */ GpEffArg effArg;
    /* 0xBB8 */ u8       field_BB8;
    /* 0xBB9 */ byte     pad_BB9[2];
    /* 0xBBB */ u8       field_BBB;
    /* 0xBBC */ byte     pad_BBC[2];
    /* 0xBBE */ s16      field_BBE;
    /* 0xBC0 */ s32      field_BC0;
    /* 0xBC4 */ s32      field_BC4;
    /* 0xBC8 */ u8       field_BC8;
    /* 0xBC9 */ byte     pad_BC9[3];
} Actor104900SpawnWork;
STATIC_ASSERT_SIZEOF(Actor104900SpawnWork, 0xBCC);

extern s32        D_8007216C;
extern GpPairSrcE Actor01100_D074E8;
extern GpPairSrcE Actor01100_D07510;
extern u8         Actor01100_D15604[];

/// When 1, `Actor01100_Fn00F58` skips pushing the model out of its world
/// contacts for the frame.
extern u8 D_80072729;

/// Effect ids `Actor01100_Fn02960` passes to `Gp_SpawnEff`: `D_8011574C`
/// at the model root when the low four bits of `D_80070F70` are clear, and
/// `D_80115738` for the splash.
extern s32 D_80115738;
extern s32 D_8011574C;

/// Word whose low bits `Actor01100_Fn02960` (bits 0-3) and
/// `Actor01100_Fn06F38` (bit 0) test; what sets it is outside this entry.
extern s32 D_80070F70;

/// Actor id the set-up `Actor01100_Fn0097C` stores for the secondary tasks,
/// which shift it into bits 8-15 of their sound ids.
extern u8 Actor01100_D15670;

/// Scale copied onto the stack and passed to `Actor01100_Fn067C0` when the
/// placement `entryId` is 0x31: 0x1400 on each axis. The trailing word is
/// present in the object and unread.
typedef struct Actor104900ScaleRodata {
    ActorsShared801385e0Scale scale;
    s32                       pad;
} Actor104900ScaleRodata;
STATIC_ASSERT_SIZEOF(Actor104900ScaleRodata, 0x14);

extern const Actor104900ScaleRodata Actor01100_D00010;

/// Pair table the spawn state packs into the display node's `GpObj.key`.
extern GpU16Pair Actor01100_D074F8;

/// Frame block the shot handler is passed. Same field offsets as
/// `ActorsShared80138efcArg` through `field_64`. `offset` is copied onto
/// each spawned shot; `pan` and `depth` are filled by the dispatcher.
/// `SVECTOR` raises the alignment, so this type is one byte longer than
/// the block the dispatcher actually passes.
typedef struct {
    byte    pad_0[0x10];
    SVECTOR offset; // Offset copied onto the spawned shot
    byte    pad_18[0x48];
    s8      pan;    // Horizontal pan for the shot cue
    byte    pad_61[1];
    s8      depth;  // Depth for the shot cue
    byte    pad_63[1];
    s8      field_64;
} Actor104900ShotArg;
STATIC_ASSERT_SIZEOF(Actor104900ShotArg, 0x66);

void Actor01100_Fn03BAC(GpEnemy* enemy, Task* task, ActorsShared80138efcWork* work);
void Actor01100_Fn041BC(GpEnemy* enemy, Task* task, ActorsShared80138efcWork* work);
void Actor01100_Fn06198(Task* task);
void Actor01100_Fn0638C(Task* task);
void Actor01100_Fn0668C(Task* task);
void Actor01100_Fn067C0(MATRIX* arg0, ActorsShared801385e0Scale* arg1);
s32  Actor01100_Fn06954(GsCOORDINATE2* arg0, s32 arg1);
s32  Actor01100_Fn06AC8(GsCOORDINATE2* arg0);
void Actor01100_Fn06E4C(GpEnemy* enemy, Task* task, ActorsShared80138efcWork* work);
void Actor01100_Fn06F38(GpEnemy* enemy, Task* task, ActorsShared80138efcWork* work, ActorsShared80138efcArg* arg);
void Actor01100_Fn070DC(GpEnemy* enemy, Task* task, ActorsShared80138efcWork* work);
void Actor01100_Fn072B8(GpEnemy* enemy, Task* task, ActorsShared80138efcWork* work, ActorsShared80138efcArg* arg);
void Actor01100_Fn073A8(Task* arg0);
void Actor01100_Fn073DC(Task* task);

/// One of the actor's three state handlers - spawn/setup, per-frame tick and
/// teardown. Wider than the usual two-argument `GpEnemyTaskFunc` shape: the
/// handlers also take the actor's work block (`Task::work`) and a 0x68-byte
/// scratchpad buffer the dispatcher allocates around the call.
typedef void (*Actor101100StateFunc)(GpEnemy* enemy, Task* task, void* work, void* scratch);

/// Fixed-size table of `Actor101100StateFunc` callbacks. Copied onto the stack
/// by `Actor01100_Fn06554` so the call uses a local jump table.
typedef struct {
    Actor101100StateFunc funcs[3];
} Actor101100StateFuncTable3;

#endif // ACTOR_101100_H
