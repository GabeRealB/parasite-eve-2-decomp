#ifndef SRC_ACTORS_ACTOR_503500_ACTOR_503500_PRIVATE_H
#define SRC_ACTORS_ACTOR_503500_ACTOR_503500_PRIVATE_H

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>

#include "common.h"

#include "gameplay/actor.h"
#include "gameplay/animation.h"
#include "gameplay/collision.h"
#include "gameplay/effects.h"
#include "gameplay/enemy.h"
#include "gameplay/geometry.h"
#include "gameplay/message.h"
#include "gameplay/pad_script.h"
#include "gameplay/enemy_params.h"

#include "main/coord.h"
#include "main/gfx.h"
#include "main/session_types.h"
#include "main/task_types.h"

struct Actor503500Work;
struct Task;

/// Bytes 0x40..0x60 of an `actor_503500` work block, which the two enemies
/// that reach them disagree about. The 0x160 block parks its collision body
/// there -- `func_actor_503500_801372C8` links `obj` and
/// `func_actor_503500_80138288` hands it back to `Gp_UnlinkObj`. The boss
/// links its node at `field_5D4` instead and uses the run for state flags;
/// `func_actor_503500_80136014` tests bits 0x102 of the halfword at 0x4C.
typedef union Actor503500Slot40 {
    WorldCollisionBody obj;
    struct {
        /* 0x00 */ byte pad_0[0xC];
        /* 0x0C */ u16  flags_4C;
    } boss;
} Actor503500Slot40;
STATIC_ASSERT_SIZEOF(Actor503500Slot40, 0x20);

/// One weighted entry of a boss attack list: `func_actor_503500_801338E8`
/// walks the list summing `weight` until it passes a random byte, then runs
/// `fn` every frame until it returns non-zero. A NULL `fn` ends the list.
typedef struct Actor503500Step {
    s32 (*fn)(struct Task*, struct Actor503500Work*);
    u32 weight;
} Actor503500Step;

/// A 16.16 world position: `func_actor_503500_80144520` reads each
/// component's high half (`lh` at +2) back into `GfxCoord.coord.t[]`.
typedef struct Actor503500FixVec {
    /* 0x0 */ GpFixed16 vx;
    /* 0x4 */ GpFixed16 vy;
    /* 0x8 */ GpFixed16 vz;
    /* 0xC */ s32       pad;
} Actor503500FixVec;
STATIC_ASSERT_SIZEOF(Actor503500FixVec, 0x10);

typedef struct Actor503500Work {
    /* 0x000 */ WorldCollisionBody    obj; // the collision body Gp_UnlinkObj takes
    /* 0x020 */ WorldCollisionContact rec; // collision table; 0xF0 enemies pass count 8
    /* 0x038 */ byte                  pad_38[0x8];
    /// Collision body of the 0x160 block, which puts its own node here rather
    /// than at 0x0: `func_actor_503500_801372C8` links it (and runs
    /// `Gp_InitRec18Table` over the record area right behind it, at 0x60) and
    /// `func_actor_503500_80138288` hands it back to `Gp_UnlinkObj`.
    /* 0x040 */ Actor503500Slot40 slot40;
    /// Record table of `slot40`'s node, passed with count 8 by
    /// `func_actor_503500_801382FC`, so the 0x160 block's table really runs to
    /// 0x120. Only the first record is named here; the 0x224 block already
    /// names `field_7E` inside that run.
    /* 0x060 */ WorldCollisionContact rec60[1];
    /* 0x078 */ byte                  pad_78[0x6];
    /* 0x07E */ u16                   field_7E; // WorldCollisionBody::flags of obj[3] in the 0x224 block
    /* 0x080 */ byte                  pad_80[0x60];
    /// Coordinate the 0xF4 block at `D_actor_503500_80177A6C` republishes
    /// alongside its collision body: `func_actor_503500_8013ECBC` stores the
    /// task's own `TmdObject::coords` in it and seeds the record's 0x600 / 3
    /// argument pair.
    /* 0x0E0 */ EffectSpawnArg field_E0; // record the block's effects are spawned with
    /* 0x0E8 */ s16            field_E8; // per-frame countdown, clamped at 0
    /* 0x0EA */ s16            field_EA;
    /* 0x0EC */ s8             field_EC; // sub-state index
    /* 0x0ED */ s8             field_ED; // sub-state index
    /* 0x0EE */ s8             field_EE; // sub-state phase, cleared with field_ED
    /* 0x0EF */ byte           pad_EF[0x1];
    /* 0x0F0 */ s8             field_F0; // sub-state index
    /* 0x0F1 */ s8             field_F1; // sub-state phase, cleared with field_F0
    /* 0x0F2 */ byte           pad_F2[0x2E];
    /// The 0x160 block's own spawn record, seeded the way `field_E0` is:
    /// `func_actor_503500_801372C8` republishes the task's `TmdObject::coords`
    /// here alongside the 0x400 / 3 pair it seeds.
    /* 0x120 */ EffectSpawnArg field_120; // record the block's effects are spawned with
                                          /// 16.16 Euler angles, velocity and position of the 0x160 block's death
                                          /// fall, stepped by `func_actor_503500_80137678`: the angles' high halves
                                          /// build the rotation and the position's high halves are added onto the
                                          /// coordinate's translation each frame.
    /* 0x128 */ Actor503500FixVec rot;
    /* 0x138 */ Actor503500FixVec vel;
    /* 0x148 */ Actor503500FixVec pos;
    /// Per-frame countdown of the 0x160 block, stepped down and floored at 0 by
    /// `func_actor_503500_801382FC`.
    /* 0x158 */ s16 field_158;
    /// Frame counter of the 0x160 block's sub-state, stepped by
    /// `func_actor_503500_801374BC` / `_80137678` and reset whenever
    /// `field_15C` changes.
    /* 0x15A */ u16 field_15A;
    /* 0x15C */ s8  field_15C; // sub-state index
    /* 0x15D */ s8  field_15D; // sub-state phase, cleared with field_15C
    /* 0x15E */ s8  field_15E; // seeded to -1 by func_actor_503500_801372C8
    /* 0x15F */ s8  field_15F; // effect-offset index, taken mod 3
                               /// The two collision bodies of the 0x3D8 block, linked by
                               /// `func_actor_503500_8013FA74` and both handed back to `Gp_UnlinkObj` by
                               /// `func_actor_503500_80141D04`. The 0x224 block puts a node at 0x160 too
                               /// (`Actor503500Work224::obj2`); the shorter blocks stop before it.
    /* 0x160 */ WorldCollisionBody obj160;
    /// Record table of `obj160`, passed with count 8 by
    /// `func_actor_503500_801420C4`, so the 0x3D8 block's table really runs to
    /// 0x240. Only the part before the 0x224 block's `field_221` is named here;
    /// the two blocks disagree about the bytes from 0x210 on.
    /* 0x180 */ WorldCollisionContact rec180[6];
    /* 0x210 */ byte                  pad_210[0x11];
    /* 0x221 */ s8                    field_221; // sub-state index
    /* 0x222 */ byte                  pad_222[0x1E];
    /* 0x240 */ WorldCollisionBody    obj240;
    /* 0x260 */ WorldCollisionContact rec260[4]; // obj240's table, count 4
    /* 0x2C0 */ byte                  pad_2C0[0x14];
    /* 0x2D4 */ s16                   field_2D4; // sub-state index of the 0x2EC block
    /* 0x2D6 */ s16                   field_2D6;
    /// Per-frame countdown of the 0x2EC block, stepped down and floored at 0 by
    /// `func_actor_503500_8013AA44`; cleared outright by
    /// `func_actor_503500_801390C4` / `_801395BC` and gated on by
    /// `func_actor_503500_80139A20`.
    /* 0x2D8 */ s16  field_2D8;
    /* 0x2DA */ s16  field_2DA;
    /* 0x2DC */ byte pad_2DC[0x2];
    /* 0x2DE */ s16  field_2DE; // sub-state frame counter, 0x2EC block
    /* 0x2E0 */ byte pad_2E0[0x4];
    /* 0x2E4 */ s8   field_2E4; // sub-state phase, cleared with field_2D4
    /* 0x2E5 */ s8   field_2E5;
    /* 0x2E6 */ byte pad_2E6[0x4];
    /* 0x2EA */ s8   field_2EA;
    /* 0x2EB */ s8   field_2EB; // TMD buffer countdown, 0x2EC block
    /* 0x2EC */ byte pad_2EC[0x48];
    /// Pose buffer `func_actor_503500_80135950` hands `func_800B3F84` when it
    /// re-seeds the animation slots.
    /* 0x334 */ byte field_334[0x34];
    /// Local offset copied from `D_actor_503500_8016F414[spawnArg1 - 0xD]`
    /// (the same address as `D_actor_503500_8016F3AC[spawnArg1]`).
    /* 0x368 */ SVECTOR field_368;
    /* 0x370 */ SVECTOR field_370; // player position latched by func_actor_503500_801400A4
                                   /// Part 0's `coord` matrix, saved by `func_actor_503500_80140654` once the
                                   /// body has risen and restored every frame before scaling.
    /* 0x378 */ MATRIX field_378;
    /* 0x398 */ byte   pad_398[0x4];
    /* 0x39C */ s32    field_39C;
    /* 0x3A0 */ byte   pad_3A0[0x4];
    /* 0x3A4 */ s16    field_3A4;
    /* 0x3A6 */ s16    field_3A6;
    /* 0x3A8 */ s16    field_3A8; // countdown, floored at 0 by func_actor_503500_801420C4
    /* 0x3AA */ s16    field_3AA;
    /* 0x3AC */ s16    field_3AC; // vertical scale, 0x1000 down to 0x200
    /* 0x3AE */ s16    field_3AE; // sub-state frame counter
    /* 0x3B0 */ u16    field_3B0;
    /* 0x3B2 */ u16    field_3B2; // fade level, stepped by 0x10 up to 0x1000
    /* 0x3B4 */ byte   pad_3B4[0x2];
    /* 0x3B6 */ u16    field_3B6; // stepped by 0x20 up to 0x1000
    /* 0x3B8 */ byte   pad_3B8[0x10];
    /* 0x3C8 */ s16    field_3C8;
    /* 0x3CA */ byte   pad_3CA[0x2];
    /* 0x3CC */ u16    field_3CC; // stepped down by 0x111, floored at 0
    /* 0x3CE */ byte   pad_3CE[0x2];
    /* 0x3D0 */ s8     field_3D0; // sub-state index
    /* 0x3D1 */ s8     field_3D1; // sub-state phase, cleared with field_3D0
    /* 0x3D2 */ byte   pad_3D2[0x2];
    /* 0x3D4 */ s8     field_3D4; // set while field_358 sits on field_368
    /* 0x3D5 */ byte   pad_3D5[0x1];
    /* 0x3D6 */ s8     field_3D6;
    /* 0x3D7 */ s8     field_3D7; // TMD buffer countdown, 0x3D8 block
    /* 0x3D8 */ byte   pad_3D8[0x9C];
    /// The boss's own light / colour matrix pair: `func_actor_503500_80132F64`
    /// points the model's `TmdObject::lightMtx` / `colorMtx` at these.
    /* 0x474 */ MATRIX lightMtx;
    /* 0x494 */ MATRIX colorMtx;
    /// Saved copy of model part 0's coordinate: `func_actor_503500_80135B74`
    /// stores it on message 4 and restores it on message 5.
    /* 0x4B4 */ GfxCoord coord4B4;
    /// Private copies of two of the boss model's part coordinates (parts 4 and
    /// 10), refreshed by `func_actor_503500_80136DDC` when bits 0x20 / 0x800 of
    /// `field_7AC` are set and then scaled by `field_5A4` / `field_5B4`.
    /* 0x504 */ GfxCoord coord504;
    /* 0x554 */ GfxCoord coord554;
    /* 0x5A4 */ VECTOR   field_5A4; // scale of coord504
    /* 0x5B4 */ VECTOR   field_5B4; // scale of coord554
    /* 0x5C4 */ VECTOR   field_5C4; // scale of model part 16, bit 0x10000
                                    /// Collision body + collision record of the boss's second body part:
                                    /// `func_actor_503500_80132F64` links it and seeds `field_5F4`,
                                    /// `func_actor_503500_80136A88` re-places the pair and
                                    /// `func_actor_503500_80136228` hands `field_5D4` back to
                                    /// `Gp_UnlinkObj` on teardown.
    /* 0x5D4 */ WorldCollisionBody field_5D4;
    /// Record table of `field_5D4`'s node -- `func_actor_503500_80132F64`
    /// parks this address in that `WorldCollisionBody` and `func_actor_503500_80136A88`
    /// re-places the pair with count 8.
    /* 0x5F4 */ WorldCollisionContact rec5F4[8];
    /// Per-frame X / Z step of `field_6C4`, written by
    /// `func_actor_503500_801353F0` from the yaw matrix scaled by `field_7A0`.
    /* 0x6B4 */ s32  field_6B4;
    /* 0x6B8 */ byte pad_6B8[0x4];
    /* 0x6BC */ s32  field_6BC;
    /* 0x6C0 */ byte pad_6C0[0x4];
    /// World position of the boss's placed part in 16.16 fixed point:
    /// `func_actor_503500_80137088` stores the placement argument's
    /// integer translation here shifted left by 16, alongside dropping the
    /// same translation into the coordinate's own matrix.
    /* 0x6C4 */ VECTOR field_6C4;
    /* 0x6D4 */ VECTOR field_6D4; // previous frame's coordinate translation
                                  /// The boss's own spawn record, seeded the way `field_E0` is:
                                  /// `func_actor_503500_80132F64` stores model part 3 here and
                                  /// `func_actor_503500_80134EAC` hands the record to `func_800FDB18` as its
                                  /// hit-effect argument.
    /* 0x6E4 */ EffectSpawnArg field_6E4;
    /* 0x6EC */ Enemy*         enemies[0x11];
    /// Two parallel per-slot halfword arrays covering the same 0x11 slots as
    /// `enemies`: `func_actor_503500_80136F40` writes both when it asks a slot
    /// to die, `func_actor_503500_80136FDC` reads `field_752` as a gate on
    /// `field_730`.
    /* 0x730 */ s16 field_730[0x11];
    /* 0x752 */ s16 field_752[0x11];
    /* 0x774 */ u32 field_774;                                           // one "already asked to die" bit per slot
    /* 0x778 */ s32 (*field_778)(struct Task*, struct Actor503500Work*); // running step, see Actor503500Step
                                                                         /// Saved rotation of the task's coordinate: the first 16 bytes of
                                                                         /// `coord.m` as words plus `m[2][2]`, restored every frame by
                                                                         /// `func_actor_503500_80134A24` before it rescales the matrix.
    /* 0x77C */ s32   field_77C[4];
    /* 0x78C */ s16   field_78C;
    /* 0x78E */ byte  pad_78E[0xE];
    /* 0x79C */ Task* field_79C; // spawned effect task, killed on step 2
    /* 0x7A0 */ s32   field_7A0; // walk speed, ramped by field_7A4 / 32
    /* 0x7A4 */ s32   field_7A4; // seeded to 0x80000
    /* 0x7A8 */ s32   field_7A8;
    /* 0x7AC */ s32   field_7AC; // part-scale enable bits, see coord504
    /* 0x7B0 */ s16   field_7B0; // boss state index
    /* 0x7B2 */ u16   field_7B2;
    /* 0x7B4 */ s16   field_7B4; // per-frame countdown, clamped at 0
                                 /// Yaw of the placed part, recovered by `func_actor_503500_80137088`
                                 /// from the rotation matrix it just built (`ratan2` of `m[0][2]` over
                                 /// `m[2][2]`).
    /* 0x7B6 */ s16  field_7B6;
    /* 0x7B8 */ s16  field_7B8;
    /* 0x7BA */ s16  field_7BA;
    /* 0x7BC */ s16  field_7BC;
    /* 0x7BE */ s16  field_7BE;
    /* 0x7C0 */ s16  field_7C0;
    /* 0x7C2 */ s16  field_7C2; // slot index being asked to die
    /* 0x7C4 */ byte pad_7C4[0x4];
    /* 0x7C8 */ u16  field_7C8;
    /* 0x7CA */ s16  field_7CA;
    /* 0x7CC */ s16  field_7CC;
    /* 0x7CE */ s16  field_7CE; // Y scale, 0x1000 stepped down by 0x10 to 0x200
    /* 0x7D0 */ s16  field_7D0; // saved field_7B6, see coord4B4
    /* 0x7D2 */ s16  field_7D2;
    /// 1 once `func_actor_503500_80135950` has seeded the animation slots and
    /// ticked them; that helper clears it again whenever it loads a new bank,
    /// and `func_actor_503500_80136D30` uses it as the gate on its own tick.
    /* 0x7D4 */ s8   field_7D4;
    /* 0x7D5 */ s8   field_7D5; // boss sub-state index, seeded with field_7D6/7D9
    /* 0x7D6 */ s8   field_7D6; // seeded to -1 with field_7D5
    /* 0x7D7 */ byte pad_7D7[0x1];
    /* 0x7D8 */ s8   field_7D8; // seeded to 1
    /* 0x7D9 */ s8   field_7D9; // mode last set by func_actor_503500_80137158
    /* 0x7DA */ u8   field_7DA; // per-state step counter
    /* 0x7DB */ u8   field_7DB; // cleared alongside field_7DA
    /* 0x7DC */ s8   field_7DC; // height band of the camera target, 0..2
    /* 0x7DD */ s8   field_7DD; // previous field_7DC
    /* 0x7DE */ s8   field_7DE;
    /* 0x7DF */ s8   field_7DF; // previous field_7DE
    /* 0x7E0 */ s8   field_7E0;
    /* 0x7E1 */ s8   field_7E1;
    /* 0x7E2 */ s8   field_7E2;
    /* 0x7E3 */ s8   field_7E3; // set when coord4B4 is restored
    /* 0x7E4 */ s8   field_7E4; // 1 while the case-1 sound/buffer state is active
    /* 0x7E5 */ s8   field_7E5; // 1 while the case-2 sound state is active
    /* 0x7E6 */ s8   field_7E6; // set to 1 when a hit takes the boss's HP to 0
    /* 0x7E7 */ s8   field_7E7; // 1 while the sound started by `gDisplayState.pendingMode` plays
} Actor503500Work;
STATIC_ASSERT_SIZEOF(Actor503500Work, 0x7E8);

/// Work block shape of the `actor_503500` effect tasks -- the ones whose
/// state-0 init `memCalloc`s the block instead of pointing `Task::work` at a
/// static global: `func_actor_503500_80144300` (0xC0),
/// `func_actor_503500_801448E8` (0xB4), `func_actor_503500_80144E8C` (0xD0),
/// `func_actor_503500_801455A4` (0x44), `func_actor_503500_80145A2C` (0xAC)
/// and `func_actor_503500_8014642C` (0x4CC). Each starts with the `WorldCollisionBody`
/// collision body their exit callback hands to `Gp_UnlinkObj`, and the ones that
/// place geometry follow it with the `WorldCollisionContact` collision record
/// (`func_actor_503500_801448E8` derives its `WorldCollisionContact` base as `block + 0x20`).
/// Only that head is shared; the payload after it differs per task, which is
/// why this type stops at 0x38.
typedef struct Actor503500ObjWork {
    /* 0x00 */ WorldCollisionBody    obj;
    /* 0x20 */ WorldCollisionContact rec;
} Actor503500ObjWork;
STATIC_ASSERT_SIZEOF(Actor503500ObjWork, 0x38);

/// `SVECTOR`-shaped offset read through unsigned halfwords: every consumer
/// copies the component straight into an `s16` field, so the sign of the load
/// never reaches the result and the original emits `lhu`.
typedef struct Actor503500UVec {
    /* 0x0 */ u16 vx;
    /* 0x2 */ u16 vy;
    /* 0x4 */ u16 vz;
    /* 0x6 */ u16 pad_6;
} Actor503500UVec;
STATIC_ASSERT_SIZEOF(Actor503500UVec, 8);

/// Scratchpad frame (`0x90` bytes carved off the scratchpad stack) used by
/// `func_actor_503500_8014176C` and `func_actor_503500_8013A470` while they
/// re-aim a chain of coordinates.
typedef struct Actor503500ChainScratch {
    /* 0x00 */ SVECTOR diff;  // `pts[i + 1] - pts[i]`
    /* 0x08 */ SVECTOR up;    // (0, 0x1000, 0) hint for `Gfx_OrthonormalBasis`
    /* 0x10 */ SVECTOR dir;   // normalised `pos`
    /* 0x18 */ SVECTOR rot;   // `Gp_ComposeParentWorld` output
    /* 0x20 */ VECTOR  pos;   // `diff` in the link's local frame
    /* 0x30 */ MATRIX  basis; // `Gfx_OrthonormalBasis` output, before `MatrixNormal`
    /* 0x50 */ MATRIX  inv;   // transpose of `world`
    /* 0x70 */ MATRIX  world; // accumulated rotation down the chain
} Actor503500ChainScratch;
STATIC_ASSERT_SIZEOF(Actor503500ChainScratch, 0x90);

typedef struct {
    s32 id;
    union {
        s32 (*call0)(Task*, s32, AnimationPlayRequest*, s32);
        s32 (*call1)(Task*, s32, ActorCommand* request);
        s32 (*call2)(Task*, s32, ActorTransform*);
        s32 (*call3)(Task*, s32, s32);
    } handler;
} Actor5035003MsgEntry;
STATIC_ASSERT_SIZEOF(Actor5035003MsgEntry, 8);

extern AnimationSet D_actor_503500_8015F934;

extern AnimationSet D_actor_503500_801602FC;

extern AnimationSet D_actor_503500_80160DA8;

extern AnimationSet D_actor_503500_80161A90;

extern AnimationSet D_actor_503500_80162500;

extern AnimationSet D_actor_503500_80162D3C;

extern AnimationSet D_actor_503500_801635A8;

extern AnimationSet D_actor_503500_80163BAC;

extern AnimationSet D_actor_503500_80164C44;

extern AnimationSet D_actor_503500_801651FC;

extern AnimationSet D_actor_503500_80165A34;

extern AnimationSet D_actor_503500_80165CDC;

extern AnimationSet D_actor_503500_80165FF8;

extern AnimationSet D_actor_503500_80166EE8;

extern AnimationSet D_actor_503500_801671B0;

extern AnimationSet D_actor_503500_801674FC;

extern AnimationSet D_actor_503500_8016A900;

extern AnimationSet D_actor_503500_8016BFB0;

extern AnimationSet D_actor_503500_8016E788;

extern DamageAttack* D_actor_503500_8016E7CC[1];

extern DamageAttack* D_actor_503500_8016E7D0[1];

extern DamageAttack* D_actor_503500_8016E7D4[2];

extern DamageAttack* D_actor_503500_8016E7DC[1];

extern EnemyParams D_actor_503500_8016E7EC[17];

extern s8 D_actor_503500_8016E8FC[20];

extern u8 D_actor_503500_8016E910[20];

extern TaskDesc D_actor_503500_8016E924[17];

extern Task* D_actor_503500_80176558;

extern ActorTransform D_actor_503500_8017655C;

extern AnimationSet** D_actor_503500_8016EAB8[2];

extern AnimationPlayRequest D_actor_503500_8016EAC0[1];

extern AnimationPlayRequest D_actor_503500_8016EAD4;

extern SVECTOR D_actor_503500_8016EC50;

extern Actor503500Step** D_actor_503500_8016EF10[2][3];

extern s16* D_actor_503500_8016EF28[2][3];

extern s16 D_actor_503500_8016EF40[4];

extern s16 D_actor_503500_8016EF48[4];

extern s16 D_actor_503500_8016EF50[4];

extern SVECTOR D_actor_503500_8016EF58[7];

extern GpGridParams D_actor_503500_8016F03C;

extern SVECTOR D_actor_503500_8016F060;

extern SVECTOR D_actor_503500_8016F068;

extern SVECTOR D_actor_503500_8016F070;

extern SVECTOR D_actor_503500_8016F078[3];

extern SVECTOR D_actor_503500_8016F090[2];

extern SVECTOR D_actor_503500_8016F0A0[1];

extern Actor503500UVec D_actor_503500_8016F0A8[1];

extern SVECTOR D_actor_503500_8016F0B0;

extern Actor503500UVec D_actor_503500_8016F0B8[2];

extern SVECTOR D_actor_503500_8016F0C8;

extern SVECTOR D_actor_503500_8016F0D0[3];

extern s32 D_actor_503500_8016F0E8[2];

extern SVECTOR D_actor_503500_8016F0F0[2];

extern RECT D_actor_503500_8016F100;

extern Actor503500UVec D_actor_503500_8016F108[4];

extern Actor503500UVec D_actor_503500_8016F128[1][3];

extern RECT D_actor_503500_8016F148[2][2];

extern SVECTOR D_actor_503500_8016F168[9];

extern SVECTOR D_actor_503500_8016F1B0;

extern SVECTOR D_actor_503500_8016F1B8[18];

extern SVECTOR D_actor_503500_8016F248[2];

extern SVECTOR D_actor_503500_8016F258;

extern SVECTOR D_actor_503500_8016F260;

extern SVECTOR D_actor_503500_8016F278[3];

extern SVECTOR D_actor_503500_8016F290[9];

extern SVECTOR D_actor_503500_8016F2D8;

extern s16 D_actor_503500_8016F2E0[6];

extern SVECTOR D_actor_503500_8016F2EC[6];

extern SVECTOR D_actor_503500_8016F31C[9];

extern RECT D_actor_503500_8016F364;

extern SVECTOR D_actor_503500_8016F36C;

extern SVECTOR D_actor_503500_8016F374[6];

extern RECT D_actor_503500_8016F3A4;

extern SVECTOR D_actor_503500_8016F3AC[4];

extern SVECTOR D_actor_503500_8016F3CC[4];

extern SVECTOR D_actor_503500_8016F3EC;

extern Actor503500UVec D_actor_503500_8016F3F4[4];

extern SVECTOR D_actor_503500_8016F414[4];

extern s16 D_actor_503500_8016F434[10];

extern SVECTOR D_actor_503500_8016F448[3];

extern s32 D_actor_503500_80171464[2];

extern TaskDesc D_actor_503500_8017146C;

extern SVECTOR D_actor_503500_80171478;

extern SVECTOR D_actor_503500_80171480[2];

extern s8 D_actor_503500_80171490[56];

extern s32 D_actor_503500_801714DC;

extern AnimationPlayRequest D_actor_503500_801714E0[2];

extern AnimationPlayRequest D_actor_503500_80171508[2];

extern AnimationPlayRequest D_actor_503500_80171530;

extern GpDelayArg D_actor_503500_80171544;

extern RECT D_actor_503500_8017155C;

extern SVECTOR D_actor_503500_80171564[5];

extern SVECTOR D_actor_503500_8017158C;

extern SVECTOR D_actor_503500_80171594;

extern PadScriptCmd D_actor_503500_8017159C[2];

extern GpScriptRec D_actor_503500_801715A4[2];

extern Actor503500UVec D_actor_503500_801715AC;

extern Actor503500UVec D_actor_503500_801715B4;

extern s32 D_actor_503500_801715BC[2];

extern TaskDesc D_actor_503500_8016E9F0[5];

extern Actor5035003MsgEntry D_actor_503500_8016EA2C[];

/// Identity rotation, written two halfwords per word store. Being inline is
/// what matches: the argument is expanded as an address sum, so the caller's
/// `&mats[i]` / `&coord[i].coord` is recomputed each iteration instead of strength-reduced.
static inline void func_actor_503500_SetRotIdentity(MATRIX* m)
{
    MATRIX_PAIR(m, 0, 0) = 0x1000;
    MATRIX_PAIR(m, 0, 2) = 0;
    MATRIX_PAIR(m, 1, 1) = 0x1000;
    MATRIX_PAIR(m, 2, 0) = 0;
    m->m[2][2]           = 0x1000;
}

void func_actor_503500_80135828(Task* arg0, s8* arg1);

void func_actor_503500_80135CE8(Task* arg0, s32 arg1);

/// Spawns slot enemy `arg1` as a child of `arg0`; returns it, or NULL.
Enemy* func_actor_503500_80135D00(Task* arg0, s32 arg1);

/// Reports whether slot `arg1` of the boss work block's `enemies` array is
/// empty. `arg0` is loaded by every caller but the body ignores it.
s32 func_actor_503500_80135E04(Task* arg0, s32 arg1);

void func_actor_503500_80135E20(Task* arg0, s32 arg1, SVECTOR* arg2);

/// Records the per-slot halfword for slot `arg1`; `arg0` is ignored the same
/// way `func_actor_503500_80135E04` ignores it.
void func_actor_503500_80135F9C(Task* arg0, s32 arg1, s16 arg2);

void func_actor_503500_80135FB4(Task* arg0, s32 arg1, s32 arg2);

s32 func_actor_503500_80136014(Task* arg0, s32 arg1);

void func_actor_503500_80136048(Task* arg0);

/// Reports whether the boss-wide gate is open; the body ignores its
/// argument, and callers pass unrelated pointers they already hold.
s32 func_actor_503500_8013608C(void* arg0);

s32 func_actor_503500_801360BC(s32 arg0, s32 arg1);

void func_actor_503500_8013611C(s32 arg0);

s16 func_actor_503500_80136134(Task* arg0);

s32 func_actor_503500_80136208(void);

s16 func_actor_503500_80136218(void);

void func_actor_503500_80137290(s32 arg0);

void func_actor_503500_801372AC(s32 arg0);

void func_actor_503500_8013BE8C(Task* task);

void func_actor_503500_8013CA8C(Task* task);

void func_actor_503500_8013DBF4(Task* task);

void func_actor_503500_8013EC64(Task* task);

void func_actor_503500_8013FA1C(Task* task);

void func_actor_503500_80142370(Task* task);

void func_actor_503500_801442A8(Task* task);

void func_actor_503500_80144890(Task* task);

void func_actor_503500_80144E34(Task* task);

void func_actor_503500_8014554C(Task* task);

void func_actor_503500_801459D4(Task* task);

void func_actor_503500_80145F84(Task* task);

// Callbacks referenced by the overlay's shared data tables.
s32 func_actor_503500_80133BF4(Task*, Actor503500Work*);

s32 func_actor_503500_80134284(Task*, Actor503500Work*);

s32 func_actor_503500_801364D0(Task*, Actor503500Work*);

s32 func_actor_503500_8013656C(Task*, Actor503500Work*);

s32 func_actor_503500_8013667C(Task*, Actor503500Work*);

s32 func_actor_503500_80136770(Task*, Actor503500Work*);

s32 func_actor_503500_8013680C(Task*, Actor503500Work*);

s32 func_actor_503500_80136948(Task*, Actor503500Work*);

void func_actor_503500_80137238(Task*);

void func_actor_503500_801384D4(Task*);

void func_actor_503500_8013AD0C(Task*);

void func_actor_503500_80143AC0(Task*);

// Callbacks referenced by the overlay's shared data tables.
s32 func_actor_503500_80135950(Task*, s32, AnimationPlayRequest*, s32);

s32 func_actor_503500_80135B74(Task*, s32, ActorCommand* msg);

s32 func_actor_503500_80137088(Task*, s32, ActorTransform* args);

s32 func_actor_503500_80137158(Task*, s32, s32);

#endif // SRC_ACTORS_ACTOR_503500_ACTOR_503500_PRIVATE_H
