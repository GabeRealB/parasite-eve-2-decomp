#ifndef ACTOR_800100_H
#define ACTOR_800100_H

#include "common.h"

#include "gameplay/3A34.h"
#include "gameplay/3CD8.h"
#include "gameplay/3FB8.h"

/// `gte_rtv0` as the retail build emits it: the full `mvmva 1,0,0,3,0` word.
#define gte_rtv0_real() __asm__ volatile("nop; nop; .word 0x4A486012")

/// 0x38 block `func_actor_800100_801624F0` allocates with `Mem_Calloc` when
/// its task enters state 0 and stores at `Task::idMap`: the launched
/// projectile's object plus its one-entry collision table, whose `field_0` is
/// armed with 2. `obj.field_C` points at `rec`, `obj.field_8` at the task's
/// own coordinate, and `obj.field_18` is the hit payload `0x21C9E`. The
/// projectile flies out along `work->field_26` while `field_24` opens, then
/// drops; `func_actor_800100_801631C8` hands the block back to `Gp_UnlinkObj`
/// on teardown. Same shape as the m4a1_pyke dart's `M4a1PykeBeam`.
typedef struct _Actor800100Beam {
    /* 0x00 */ GpObj   obj;
    /* 0x20 */ GpRec18 rec[1];
} Actor800100Beam;
STATIC_ASSERT_SIZEOF(Actor800100Beam, 0x38);

/// 0x5C-byte block from `G_SCRATCH_HEAD` used by
/// `func_actor_800100_80166514`: the `GsCOORDINATE2` it hands to
/// `Gp_PlaceCoordOffset` / `func_actor_800100_801668C0`, the `rot` offset
/// applied to it, and the angle `func_actor_800100_8016709C` returns.
typedef struct _Actor800100PlaceScratch {
    /* 0x00 */ GsCOORDINATE2 coord;
    /* 0x50 */ SVECTOR       rot;
    /* 0x58 */ u16           angle;
    /* 0x5A */ byte          pad_5A[2];
} Actor800100PlaceScratch;
STATIC_ASSERT_SIZEOF(Actor800100PlaceScratch, 0x5C);

/// 0x20-byte block from `G_SCRATCH_HEAD` used by
/// `func_actor_800100_80164710` and `func_actor_800100_80164B9C`: the lock
/// position `Gp_GetLockPos` fills (also the `VECTOR3` handed to
/// `func_80103C74`), and the `rot` vector above it whose `vx`/`vz`
/// `func_80103D8C` measures. `80164B9C` also treats it as the `VECTOR3`
/// handed to `func_8010BD88` / `func_8010BE5C`.
typedef struct _Actor800100LockScratch {
    /* 0x00 */ VECTOR3 lock;
    /* 0x0C */ byte    pad_C[4];
    /* 0x10 */ VECTOR3 rot;
    /* 0x1C */ byte    pad_1C[4];
} Actor800100LockScratch;
STATIC_ASSERT_SIZEOF(Actor800100LockScratch, 0x20);

/// 0x1C-byte block from `G_SCRATCH_HEAD` used by
/// `func_actor_800100_8016666C` to draw the vertical `LINE_G2` that
/// `func_actor_800100_80166514` puts on the placed coordinate. `origin` is the
/// vector pushed through the coordinate's `workm` first — always (0, 0, 0), so
/// `sxy0` is the origin's screen point — and `tip` the second, `angle` units
/// straight up, so `sxy1` is the screen point of the far end. `otz` is the
/// `gte_stszotz` of that second projection, already shifted, and doubles as
/// the `Gp_AddTpageShift` bucket.
typedef struct _Actor800100LineScratch {
    /* 0x00 */ DVECTOR sxy0;
    /* 0x04 */ DVECTOR sxy1;
    /* 0x08 */ s32     otz;
    /* 0x0C */ SVECTOR origin;
    /* 0x14 */ SVECTOR tip;
} Actor800100LineScratch;
STATIC_ASSERT_SIZEOF(Actor800100LineScratch, 0x1C);

/// One corner of the beam quad `func_actor_800100_801668C0` draws, as an
/// offset in the placed coordinate's own frame: `vy` straight up, `vz` along
/// the face. `D_actor_800100_80161F10` is the four of them.
typedef struct _Actor800100QuadCorner {
    /* 0x00 */ s16 vy;
    /* 0x02 */ s16 vz;
} Actor800100QuadCorner;
STATIC_ASSERT_SIZEOF(Actor800100QuadCorner, 4);

/// 0x44-byte block from `G_SCRATCH_HEAD` used by `func_actor_800100_801668C0`
/// to draw the textured sheet `func_actor_800100_80166514` places: the four
/// `v` corners are the `D_actor_800100_80161F10` (y, z) pairs offset by the
/// coordinate's world `t`, projected through `GsWSMATRIX` into `sxy`, and
/// `otz` is the `gte_stszotz` of the last of them.
typedef struct _Actor800100QuadScratch {
    /* 0x00 */ DVECTOR sxy[4];
    /* 0x10 */ s32     otz;
    /* 0x14 */ VECTOR  work;
    /* 0x24 */ SVECTOR v[4];
} Actor800100QuadScratch;
STATIC_ASSERT_SIZEOF(Actor800100QuadScratch, 0x44);

/// NULL-terminated `GpImgRec*` frame lists for `func_actor_800100_80163A58`,
/// indexed `table[field_987 - 1][field_989]`; `D_actor_800100_80167210` is
/// the `field_98A` sequence.
extern GpImgRec** D_actor_800100_80167200[];
extern GpImgRec** D_actor_800100_80167210[];

/// Room-light slot 3 (`&Gp_RoomCoords[3]`) that `func_actor_800100_80161F20`
/// claims as the coordinate its flare is lit from: `field_0` is the claim
/// refcount, `coord` the world coordinate driven from the actor's own, and the
/// tail the rotation and falloff recomputed from `Gp_LcgState` each frame.
extern GpCoord64 D_8011505C;

extern u32 Gp_LcgState;

/// Translation the flare's own coordinate starts at, `(0, 0x200, 0x40)`.
extern SVECTOR D_actor_800100_80167128;

void func_actor_800100_80162264(VECTOR3* arg0, u16 arg1, s32 arg2);
void func_actor_800100_80162A14(VECTOR3* arg0, u16 arg1, u16 arg2, s16 arg3);
void func_actor_800100_80162E90(VECTOR3* arg0, s32 arg1);
void func_actor_800100_801631C8(Task* arg0);
void func_actor_800100_80163C04(GpActorWork* arg0);
void func_actor_800100_80163D54(GpActorWork* arg0);
void func_actor_800100_801655C0(GpActorWork* arg0);
void func_actor_800100_80165630(GpActorWork* arg0);
void func_actor_800100_80165664(GpActorWork* arg0);
void func_actor_800100_801656C8(GpActorWork* arg0);
void func_actor_800100_801656F4(GpActorWork* arg0);
void func_actor_800100_80165720(GpActorWork* arg0);
void func_actor_800100_80165748(GpActorWork* arg0);
void func_actor_800100_801657D8(GpActorWork* arg0);
void func_actor_800100_80165930(GpActorWork* arg0);
void func_actor_800100_801659EC(GpActorWork* arg0);
void func_actor_800100_80165DE8(GpActorWork* arg0);
void func_actor_800100_80166DD0(GpActorWork* arg0);
void func_actor_800100_80166DF0(GpActorWork* arg0);
void func_actor_800100_80166E14(GpActorWork* arg0);
void func_actor_800100_80166E94(GpActorWork* arg0, s32 arg1);
void func_actor_800100_80166EE8(GpActorWork* arg0);
void func_actor_800100_8016666C(GsCOORDINATE2* arg0, s16 arg1);
void func_actor_800100_801668C0(GsCOORDINATE2* arg0);
s32  func_actor_800100_80166B40(GpRec18* arg0, GsCOORDINATE2* arg1, GsCOORDINATE2* arg2);
s32  func_actor_800100_8016709C(GsCOORDINATE2* arg0, GpRec18* arg1, GpRec18* arg2);

#endif
