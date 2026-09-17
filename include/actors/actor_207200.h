#ifndef ACTOR_207200_H
#define ACTOR_207200_H

#include "common.h"
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>

#include "gameplay/3A34.h"
#include "main/tmd.h"

/// `GpObj` list node with 0x18 bytes of trailing state; five of them live in
/// `Actor207200Work` and are unlinked one by one when the actor tears down.
/// `obj.flags` is the display flag word the transform handler clears the top
/// bit of; `field_20` is the sub-object the context points its `field_54` at.
typedef struct Actor207200Obj {
    /* 0x00 */ GpObj obj;
    /* 0x20 */ byte  field_20[0x18];
} Actor207200Obj;
STATIC_ASSERT_SIZEOF(Actor207200Obj, 0x38);

typedef struct Actor207200Work {
    /* 0x000 */ byte           pad_0[0xFC];
    /* 0x0FC */ Actor207200Obj field_FC;
    /* 0x134 */ byte           pad_134[0x18];
    /* 0x14C */ Actor207200Obj field_14C;
    /* 0x184 */ Actor207200Obj field_184;
    /* 0x1BC */ byte           pad_1BC[0x20];
    /* 0x1DC */ Actor207200Obj field_1DC;
    /* 0x214 */ Actor207200Obj field_214;
    /* 0x24C */ byte           pad_24C[0x18];
    /* 0x264 */ MATRIX         field_264; // transform folded onto the model part
    /* 0x284 */ byte           pad_284[2];
    /* 0x286 */ s16            field_286; // state the enemy work is running
    /* 0x288 */ s16            field_288;
    /* 0x28A */ s16            field_28A; // frames spent in the current state
    /* 0x28C */ s16            field_28C; // animation id the work is playing
    /* 0x28E */ u16            field_28E; // id the three helper slots last saw
    /* 0x290 */ u16            field_290; // frames spent on the current id
    /* 0x292 */ byte           pad_292[0xE];
    /* 0x2A0 */ s16            field_2A0; // angle the transform is scaled by
    /* 0x2A2 */ byte           pad_2A2[0x22];
    /* 0x2C4 */ Actor207200Obj field_2C4;
    /* 0x2FC */ byte           pad_2FC[0x78];
    /* 0x374 */ Actor207200Obj field_374;
    /* 0x3AC */ Actor207200Obj field_3AC;
    /* 0x3E4 */ byte           pad_3E4[0x10];
    /* 0x3F4 */ GpEffArg       field_3F4; // `func_800FDB18` argument record
    /* 0x3FC */ byte           pad_3FC[0x58];
    /* 0x454 */ s32            field_454; // model position while the actor idles
    /* 0x458 */ s32            field_458;
    /* 0x45C */ s32            field_45C;
    /* 0x460 */ byte           pad_460[4];
    /* 0x464 */ MATRIX         field_464; // last transform folded onto the model
    /* 0x484 */ byte           pad_484[2];
    /* 0x486 */ s16            field_486; // state the six helper slots are running
    /* 0x488 */ s16            field_488; // frames spent in `field_486`
    /* 0x48A */ u16            field_48A; // frames the current helper stage has run
    /* 0x48C */ s16            field_48C;
    /* 0x48E */ u16            field_48E; // id the six helper slots last saw
    /* 0x490 */ u16            field_490; // frames spent on the current id
    /* 0x492 */ s16            field_492;
    /* 0x494 */ byte           pad_494[4];
    /* 0x498 */ s16            field_498;
    /* 0x49A */ s16            field_49A;
    /* 0x49C */ s16            field_49C;
    /* 0x49E */ byte           pad_49E[4];
    /* 0x4A2 */ s16            field_4A2;
    /* 0x4A4 */ s16            field_4A4;
    /* 0x4A6 */ s16            field_4A6;
    /* 0x4A8 */ byte           pad_4A8[2];
    /* 0x4AA */ s16            field_4AA;
} Actor207200Work;

/// The 0x4AC-byte allocation `func_actor_207200_8014B278` makes with
/// `Mem_Calloc` and stores in `Task::idMap`: the animation context
/// `func_800B3F84` fills in, five `GpObj` render nodes with their `GpRec18`
/// tables (`Gp_LinkObj` shapes 3/2/2/3/3) and three `func_800FDB18` argument
/// records. `Actor207200Work` is the tick handlers' view of the same block and
/// does not yet agree with this layout over 0x234..0x2C4.
typedef struct Actor207200SpawnWork {
    /* 0x000 */ byte     pad_0[0x14];
    /* 0x014 */ byte     field_14[0x118];
    /* 0x12C */ byte     field_12C[0x70];
    /* 0x19C */ MATRIX   field_19C;
    /* 0x1BC */ MATRIX   field_1BC;
    /* 0x1DC */ GpObj    obj1;
    /* 0x1FC */ GpRec18  rec1[1];
    /* 0x214 */ GpObj    obj2;
    /* 0x234 */ GpRec18  rec2[6];
    /* 0x2C4 */ GpObj    obj3;
    /* 0x2E4 */ GpRec18  rec3[6];
    /* 0x374 */ GpObj    obj4;
    /* 0x394 */ GpRec18  rec4[1];
    /* 0x3AC */ GpObj    obj5;
    /* 0x3CC */ GpRec18  rec5[1];
    /* 0x3E4 */ GpEffArg eff0;
    /* 0x3EC */ GpEffArg eff1;
    /* 0x3F4 */ GpEffArg eff2;
    /* 0x3FC */ byte     pad_3FC[0x52];
    /* 0x44E */ u16      field_44E;
    /* 0x450 */ byte     pad_450[0x38];
    /* 0x488 */ s16      field_488;
    /* 0x48A */ byte     pad_48A[2];
    /* 0x48C */ s16      field_48C;
    /* 0x48E */ s16      field_48E;
    /* 0x490 */ byte     pad_490[4];
    /* 0x494 */ s16      field_494;
    /* 0x496 */ byte     pad_496[8];
    /* 0x49E */ s16      field_49E;
    /* 0x4A0 */ byte     pad_4A0[4];
    /* 0x4A4 */ s16      field_4A4;
    /* 0x4A6 */ s16      field_4A6;
    /* 0x4A8 */ s16      field_4A8;
} Actor207200SpawnWork;
STATIC_ASSERT_SIZEOF(Actor207200SpawnWork, 0x4AC);

/// Owning context. The leading part holds the `Gp_UnlinkNode` list entry at
/// +0x10, as for the gameplay `GpEnemy`, so `field_40` is its HP and
/// `field_54` a model pointer. `field_18` is the model part the context is
/// driven from, re-picked every frame by `func_actor_207200_8014D8DC`.
typedef struct Actor207200Ctx {
    /* 0x00 */ byte           pad_0[8];
    /* 0x08 */ u16            field_8; // room/area id byte the sound id embeds
    /* 0x0A */ byte           pad_A[6];
    /* 0x10 */ GpLinkNode     node;
    /* 0x18 */ GsCOORDINATE2* field_18;
    /* 0x1C */ byte           pad_1C[0x24];
    /* 0x40 */ s16            field_40;
    /* 0x42 */ byte           pad_42[0x12];
    /* 0x54 */ s32            field_54;
} Actor207200Ctx;

typedef struct Actor207200 {
    /* 0x00 */ byte             pad_0[0x1C];
    /* 0x1C */ Actor207200Work* field_1C;
    /* 0x20 */ Actor207200Ctx*  field_20;
    /* 0x24 */ byte             pad_24[6];
    /* 0x2A */ s16              field_2A;
    /* 0x2C */ void*            field_2C; // `Task::extra`: the actor's TmdObject
} Actor207200;

/// Effect-setup record handed to the spawned task through `D_80062730`.
extern u8      D_actor_207200_801517F8[];
extern SVECTOR D_actor_207200_80153F18;
/// Spawned task's setup argument (`D_800626EC[5].setupArg`).
extern s32 D_80062730;

void func_actor_207200_8014DB4C(Actor207200* arg0);
void func_actor_207200_8014CFEC(Actor207200* arg0);
/// Angle from `coord` to the player, plus the horizontal distance between them
/// written through `dist`; the result is a 4096-unit circle angle.

#endif
