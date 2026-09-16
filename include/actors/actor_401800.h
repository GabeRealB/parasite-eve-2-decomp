#ifndef ACTOR_401800_H
#define ACTOR_401800_H

#include "common.h"

#include "gameplay/1BC.h"
#include "gameplay/3A34.h"
#include "gameplay/3FB8.h"
#include "main/task.h"
#include "main/tmd.h"

/// Private work block of the actor 401800 task, hanging off `Task::idMap`.
///
/// Only the fields the decompiled code touches are named, so the struct is
/// deliberately open-ended: the three `GpObj` display nodes the teardown hands
/// back to `Gp_UnlinkObj`, and the two child tasks it kills. The offsets match
/// the same-shaped teardown of actor 01900 (`Actor01900Work`), whose display
/// nodes sit at the same three addresses; only the child-task pair differs.
/// `field_4` is the live-actor flag `func_actor_401800_8013E138` tests, and
/// `field_A08.flags` / `field_B48.flags` are the two masks it writes. The
/// halfwords at 0x898..0x8A2 are the same animation-state slots
/// `Actor01900_Fn0A7C0` writes; `func_actor_401800_8013E194` is that body.
typedef struct Actor401800Work {
    /* 0x000 */ s16 field_0;
    /* 0x002 */ s16 field_2;
    /* 0x004 */ s16 field_4;
    /// Step counter `func_actor_401800_8013E4F0` resets to 0 and bumps once a
    /// frame; the same slot `Actor104000Work.field_6` counts in.
    /* 0x006 */ s16  field_6;
    /* 0x008 */ byte pad_8[0x52];
    /* 0x05A */ u16  field_5A;
    /* 0x05C */ byte pad_5C[0xC];
    /* 0x068 */ u16  field_68;
    /* 0x06A */ byte pad_6A[0x82A];
    /* 0x894 */ s32  field_894;
    /* 0x898 */ s16  field_898;
    /* 0x89A */ s16  field_89A;
    /* 0x89C */ byte pad_89C[2];
    /* 0x89E */ s16  field_89E;
    /* 0x8A0 */ byte pad_8A0[2];
    /* 0x8A2 */ s16  field_8A2;
    /* 0x8A4 */ s16  field_8A4;
    /* 0x8A6 */ byte pad_8A6[8];
    /* 0x8AE */ s16  field_8AE;
    /* 0x8B0 */ s16  field_8B0;
    /* 0x8B2 */ byte pad_8B2[2];
    /// State the `0x3FF` handler last ran for: `func_actor_401800_8013A034`
    /// sends the actor's 0x200 effect when `field_5A & 0x3FF` is 4 and differs
    /// from this, then stores the mask back. Same slot `Actor01900Work.field_894`
    /// compares against.
    /* 0x8B4 */ s32 field_8B4;
    /// `func_800FDB18` argument record: the coordinate handed to it plus the
    /// effect scale / count pair. Same slot `Actor01900Work.field_8B8` keeps.
    /* 0x8B8 */ GpEffArg field_8B8;
    /* 0x8C0 */ byte     pad_8C0[8];
    /* 0x8C8 */ GpObj    field_8C8;
    /* 0x8E8 */ GpRec18  field_8E8;
    /* 0x900 */ byte     pad_900[0x108];
    /* 0xA08 */ GpObj    field_A08;
    /* 0xA28 */ GpRec18  field_A28;
    /* 0xA40 */ byte     pad_A40[0x108];
    /* 0xB48 */ GpObj    field_B48;
    /* 0xB68 */ byte     pad_B68[0x9C];
    /// Step the actor walks along its local Z axis: `func_actor_401800_80139118`
    /// seeds it with -0x78, hands it to the step helper while the 0x10 clip is
    /// playing, and halves it each time the `field_A28` contact test fires.
    /// Same role `Actor401300Work.field_C98` plays.
    /* 0xC04 */ s16  field_C04;
    /* 0xC06 */ byte pad_C06[2];
    /// Per-variant reload the LCG spreads over the idle step countdown: the
    /// high half of a fresh `Gp_LcgState` draw masked to 3 bits (`& 7`) is
    /// added to it and stored into `field_6`. Same slot `Actor401300Work`
    /// keeps as `field_CA0`, whose counterpart loads it with `& 0xF`.
    /* 0xC08 */ u16  field_C08;
    /* 0xC0A */ byte pad_C0A[4];
    /// Radius `func_actor_401800_8013A034` hands its scratch distance test:
    /// the player is close enough to arm the actor once the squared XZ offset
    /// fits inside it. Same role `Actor01900Work.field_C32` plays.
    /* 0xC0E */ u16 field_C0E;
    /// The three bytes `func_actor_401800_8013DF80` copies out of the room
    /// request record it is handed; same slot as `Actor01900Work.field_C34`.
    /* 0xC10 */ u8   field_C10[3];
    /* 0xC13 */ byte pad_C13[1];
    /// The two helper tasks killed before the nodes are unlinked; the same
    /// pair `Actor01900Work` keeps at +0xC38 / +0xC3C.
    /* 0xC14 */ Task* field_C14;
    /* 0xC18 */ Task* field_C18;
    /* 0xC1C */ byte  pad_C1C[4];
    /// Set to 1 by the actors that own the `0x3F1` message and cleared once
    /// `func_actor_401800_80139118` has sent it. Same slot `Actor01900Work`
    /// keeps at 0xC20.
    /* 0xC20 */ s16 field_C20;
} Actor401800Work;

/// 0xC-byte scratch `func_actor_401800_8013A034` takes from `G_SCRATCH_HEAD`
/// for its player-in-radius test: the X/Z offset to the camera target and the
/// radius, each squared in place before `dx + dz < r`. Same shape as
/// `Actor01900RangeScratch` / `Actor401300RangeScratch`.
typedef struct Actor401800RangeScratch {
    /* 0x0 */ s32 dx;
    /* 0x4 */ s32 dz;
    /* 0x8 */ s32 r;
} Actor401800RangeScratch;
STATIC_ASSERT_SIZEOF(Actor401800RangeScratch, 0xC);

/// Animation view of the same task work block: the pose context at 0x1C and
/// its slot array, then the blend context the actor keeps beside it. The
/// arrays cover the slot indices `func_actor_401800_801337EC` walks, which is
/// the identical body of `Actor01900_Fn01950`; the offsets all match
/// `Actor01900AnimWork`, and the tail overlays the work block's
/// `field_8A2` / `field_8A4` (the state the slot writes step down by 3).
typedef struct Actor401800AnimWork {
    /* 0x000 */ byte       pad_0[0x1C];
    /* 0x01C */ GpAnimCtx  anim;
    /* 0x030 */ GpAnimSlot slots[19];
    /* 0x328 */ byte       pad_328[0x130];
    /* 0x458 */ GpAnimCtx  blendAnim;
    /* 0x46C */ GpAnimSlot blendSlots[19];
    /* 0x764 */ byte       pad_764[0x13E];
    /* 0x8A2 */ s16        field_8A2;
    /* 0x8A4 */ s16        field_8A4;
    /* 0x8A6 */ byte       pad_8A6[4];
    /* 0x8AA */ s16        field_8AA;
    /* 0x8AC */ s16        field_8AC;
} Actor401800AnimWork;

/// 0x10-byte scratch `func_actor_401800_80135F58` takes from `G_SCRATCH_HEAD`:
/// the offset from the actor to the player, then the clamped turn applied to
/// the root coordinate. Same shape as `Actor01900AimScratch`.
typedef struct Actor401800AimScratch {
    /* 0x0 */ SVECTOR delta;
    /* 0x8 */ s16     pad_8;
    /* 0xA */ s16     pad_A;
    /* 0xC */ s16     angle;
    /* 0xE */ s16     pad_E;
} Actor401800AimScratch;
STATIC_ASSERT_SIZEOF(Actor401800AimScratch, 0x10);

/// 0x34-byte scratch the yaw rebuild takes from `G_SCRATCH_HEAD`: a `MATRIX`
/// plus the `VECTOR` handed to `ScaleMatrix` and the yaw stored before
/// `Gfx_RotMatrixY`. Same layout as `Actor01900RotScratch`.
typedef struct Actor401800RotScratch {
    /* 0x00 */ MATRIX m;
    /* 0x20 */ VECTOR scale;
    /* 0x30 */ s16    angle;
    /* 0x32 */ s16    pad_32;
} Actor401800RotScratch;
STATIC_ASSERT_SIZEOF(Actor401800RotScratch, 0x34);

/// Per-task actor context: `field_1C` is the work block above (the same
/// pointer `Task::idMap` holds), `field_20` the `GpEnemy` in
/// `Task::spawnArg2`, and `field_2C` the actor's `TmdObject`. Same shape as
/// `Actor01900`.
typedef struct Actor401800 {
    /* 0x00 */ byte             pad_0[0x1C];
    /* 0x1C */ Actor401800Work* field_1C;
    /* 0x20 */ GpEnemy*         field_20;
    /* 0x24 */ byte             pad_24[8];
    /* 0x2C */ TmdObject*       field_2C;
} Actor401800;

/// Payload of the `0x3FF` message `func_actor_401800_80138F5C` sends: the same
/// 0x14-byte animation record other actors keep as `GpAnimArg` data
/// (`D_actor_356100_80173244` and friends); `field_4` is the animation id.
extern GpAnimArg D_actor_401800_80155A0C;

/// Payload of the `0x3E9` message `func_actor_401800_80138C28` sends: the
/// slot-3 task's root position followed by the heading `ratan2` derives from
/// the direction to the actor. Same 0x18-byte shape the `Actor401300Work`
/// fields `field_CD4` / `field_CE4` form.
typedef struct Actor401800Msg3E9 {
    /* 0x00 */ VECTOR  field_0;
    /* 0x10 */ SVECTOR field_10;
} Actor401800Msg3E9;
STATIC_ASSERT_SIZEOF(Actor401800Msg3E9, 0x18);

/// Payload `func_actor_401800_80138C28` fills and sends with message 0x3E9.
extern Actor401800Msg3E9 D_actor_401800_80155AD8;

/// Camera-target matrix `func_actor_401800_8013A034` measures the actor's root
/// coordinate against for its proximity test. Same global `Actor401300` reads.
extern MATRIX* D_80073B8C;

/// LCG seed the actor handlers `Gp_LcgState = Gp_LcgState * 5 + 0x71357911`
/// step and read the high half of; `func_actor_401800_8013E5A4` takes 3 bits
/// of it as the extra idle steps it adds to `Actor401800Work.field_6`.
extern u32 Gp_LcgState;

/// The block `func_actor_401800_8013A034` posts into `D_actor_401800_80155978`
/// when the actor's live flag is set, taking over the animation the actor had
/// been running. Same pair `Actor401300` keeps as `D_actor_401300_80158878` /
/// `D_actor_401300_80152BB8`.
extern s32  D_actor_401800_80155124;
extern s32* D_actor_401800_80155978;

/// Gameplay slot `Gp_SpawnEff` effects read their model data from; set before
/// each spawn in `func_actor_401800_8013BB10`.
extern void* D_80114B78[1];

/// Overlay effect model data `func_actor_401800_8013BB10` points `D_80114B78`
/// at before spawning: the 0x60030 debris burst, then the 0xA0005 fan the step
/// counter trips at 3 and 5 and the two 0xA0005 bursts at 7 and 9.
extern char D_actor_401800_80143E9C;
extern char D_actor_401800_80144434;
extern char D_actor_401800_80144F24;

/// Payload of message `0x7D3`, the "set animation state" request the handler
/// table `D_actor_401800_80155A80` routes to `func_actor_401800_8013DCBC`:
/// `field_4` is the requested state, 0..4. The 01900 actor's table entry for
/// the same message id carries the identical record as `Actor01900Msg7D3`.
typedef struct Actor401800Msg7D3 {
    /* 0x0 */ s32 field_0;
    /* 0x4 */ s32 field_4;
} Actor401800Msg7D3;

/// Movement is frozen while this is 1. Same flag `Actor401800_MoveForwardNonzero`
/// and the other families' step helpers test.
extern u8 D_80072729;

s32 func_actor_401800_80132C68(GsCOORDINATE2* coord, GpRec18* rec, s32 arg2);
/// Returns non-zero while `coord` may still travel `arg1` units of its local Z
/// path; the result is read as a signed halfword (`func_actor_401800_80139118`),
/// the way `func_actor_401300_8013267C` is.
s32  func_actor_401800_80133558(GsCOORDINATE2* coord, s16 arg1, s16 arg2);
s32  func_actor_401800_80133918(Actor401800* arg0);
s32  func_actor_401800_8013DCBC(Actor401800* arg0, s32 arg1, Actor401800Msg7D3* arg2);
void func_actor_401800_80133EB8(Actor401800* arg0);
void func_actor_401800_8013E0A0(Task* task);
void func_actor_401800_8013E138(Actor401800* arg0);
void func_actor_401800_8013E194(Actor401800* arg0);
void func_actor_401800_8013E23C(Actor401800* arg0);
void func_actor_401800_8013E2E8(Actor401800* arg0);
void func_actor_401800_8013E394(Actor401800* arg0);
void func_actor_401800_8013E44C(Actor401800* arg0);
void func_actor_401800_8013E4F0(Actor401800* arg0);

#endif // ACTOR_401800_H
