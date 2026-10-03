/* The animation driver of the Odd Stranger (actor_401000, actor_401800): a
 * 19-slot rig plus a second blend context. State handlers request a clip change (cross-fade through a 45x45
 * transition table, or a hard restart) and can overlay a secondary clip on
 * slots 1-10, mixed by weight. Each frame the driver also eases a head yaw
 * toward its target by up to 0x100 and turns joints 5 and 2 by 2/3 and 1/2 of
 * it, then plays the state's animation sound event. A 0x7D3 message puts it
 * into a scripted pose.
 *
 * Include this header in the prologue and each fragment at its function's
 * position.
 */

#ifndef SRC_SHARED_ODD_STRANGER_H
#define SRC_SHARED_ODD_STRANGER_H

/* The Odd Stranger ships in two builds of the same source: actor_401000
 * (variant 1) and actor_401800 (variant 2). Each package defines
 * ODD_STRANGER_VARIANT before including this header; the differences between
 * the builds follow from it. */
#if ODD_STRANGER_VARIANT == 1
#define ODD_STRANGER_HIT_FX_OFFSET     1                        /* the work block keeps the hit effect offset */
#define ODD_STRANGER_CLIP2_STEP_A      0x11                     /* first footstep cue frame of clip 2 */
#define ODD_STRANGER_CLIP2_STEP_B      0x1B                     /* second footstep cue frame of clip 2 */
#define ODD_STRANGER_BODY_RADIUS       0x1AE                    /* body collision sphere radius */
#define ODD_STRANGER_SWING_RADIUS      0xD7                     /* body sphere radius in the chase and sidestep states */
#define ODD_STRANGER_BODY2_GRID        1                        /* whether some states enable (1) or disable (0) the second body's grid collision */
#define ODD_STRANGER_SIGHT_TEST        0                        /* whether turning and chasing test the line of sight */
#define ODD_STRANGER_SIGHT_COOLDOWN    field_C1B                /* work member holding the sight cooldown */
#define ODD_STRANGER_WALK_STEP         0xA                      /* forward step while walking */
#define ODD_STRANGER_PATROL_TURN_CLAMP 0x20                     /* per-frame turn clamp toward the patrol waypoint */
#define ODD_STRANGER_STALK_RATE        0x24                     /* body slot rate the stalk state starts with */
#define ODD_STRANGER_STALK_TURN_BIAS   0x300                    /* turn bias of the stalk state */
#define ODD_STRANGER_STALK_STEP        0x16                     /* forward step of the stalk state */
#define ODD_STRANGER_HOLD_AIM_CLIP     0x13                     /* clip of the hold-aim state */
#define ODD_STRANGER_PART1_FX_SCALE    0x100                    /* scale of the effect at part 1 when dormant */
#define ODD_STRANGER_GRAB_FX_PART      5                        /* model part of the effect at the end of the grab */
#define ODD_STRANGER_IDLE_JITTER_MASK  0xF                      /* mask on the random idle countdown */
#define ODD_STRANGER_STAGGER_DAMAGE    0x4C                     /* accumulated damage that forces a stagger */
#define ODD_STRANGER_DEATH_RELEASE_ARG 2                        /* argument of the 0x3F1 message on death */
#define ODD_STRANGER_BURST_MODEL_5     &gOddStrangerBurstModelB /* model of the frame-5 burst of the walking death */
#elif ODD_STRANGER_VARIANT == 2
#define ODD_STRANGER_HIT_FX_OFFSET     0
#define ODD_STRANGER_CLIP2_STEP_A      0x10
#define ODD_STRANGER_CLIP2_STEP_B      0x16
#define ODD_STRANGER_BODY_RADIUS       0x12C
#define ODD_STRANGER_SWING_RADIUS      0x96
#define ODD_STRANGER_BODY2_GRID        0
#define ODD_STRANGER_SIGHT_TEST        1
#define ODD_STRANGER_SIGHT_COOLDOWN    field_8CA
#define ODD_STRANGER_WALK_STEP         7
#define ODD_STRANGER_PATROL_TURN_CLAMP 0x18
#define ODD_STRANGER_STALK_RATE        0x30
#define ODD_STRANGER_STALK_TURN_BIAS   0x400
#define ODD_STRANGER_STALK_STEP        0x15
#define ODD_STRANGER_HOLD_AIM_CLIP     9
#define ODD_STRANGER_PART1_FX_SCALE    0x200
#define ODD_STRANGER_GRAB_FX_PART      1
#define ODD_STRANGER_IDLE_JITTER_MASK  7
#define ODD_STRANGER_STAGGER_DAMAGE    0x38
#define ODD_STRANGER_DEATH_RELEASE_ARG 0
#define ODD_STRANGER_BURST_MODEL_5     &gOddStrangerBurstModelA
#else
#error "ODD_STRANGER_VARIANT must be 1 or 2"
#endif

#include "common.h"

#include "actors/actor.h"
#include "overlay.h"

/* The Odd Stranger's work block. The two builds differ by one member: the hit
 * effect offset at 0x8C0, which only actor_401000's build has
 * (ODD_STRANGER_HIT_FX_OFFSET, defined by each package). The offsets in the
 * comments are that build's; actor_401800's members from 0x8C8 on sit 8 bytes
 * lower. */

/// The Odd Stranger's work block, hanging off `Task::work` in both packages.
///
/// Only the fields the decompiled code touches are named: among them the three
/// `WorldCollisionBody` collision bodies the teardown hands back to
/// `Gp_UnlinkObj`, and the two child tasks it kills. Actor 01900 (the Grinning
/// Stranger) uses the same layout as actor_401800's build. `field_4` is the
/// live-actor flag, and `field_B50.flags` / `field_A10.flags` are the two masks
/// the teardown writes. The halfwords at 0x898..0x8A2 are the animation-state
/// slots `Actor01900_Fn0A7C0` writes too.
typedef struct OddStrangerWork {
    /* 0x000 */ s16 field_0;
    /* 0x002 */ s16 field_2;
    /* 0x004 */ s16 field_4;
    /// One-shot latch `oddStrangerDormant` raises once the actor's
    /// spawn sound has been queued; the same slot `_Actor401300Work` keeps at
    /// +0x6.
    /* 0x006 */ s16 field_6;
    /// Frame counter `oddStrangerStalk` runs the 0x5B re-arm and the
    /// 0xF1 turn flip off; the same slot `_Actor401300Work` keeps at +0x8.
    /* 0x008 */ s16  field_8;
    /* 0x00A */ byte pad_A[2];
    /// Spawn square and one step along the facing, both narrowed to 16 bits by
    /// `func_actor_401000_80133274`'s normalised heading.
    /* 0x00C */ ActorPatrolPoint field_C[2];
    /* 0x014 */ s16              field_14;
    /// Heading `actorMsgPlaceRecordYaw` takes from the root coordinate's
    /// Z axis once a placement record has been applied to it.
    /* 0x016 */ s16  yaw;
    /* 0x018 */ byte pad_18[0x4];
    /// The animation rig and its blend twin. Guards test the second slot's
    /// status (`rig.slots[1].status`), and its record index is the keyframe
    /// the stun and grab states compare against.
    /* 0x01C */ ActorAnimRig19 rig;
    /* 0x458 */ ActorAnimRig19 blend;
    /* 0x894 */ s32            field_894;
    /* 0x898 */ s16            field_898;
    /* 0x89A */ s16            field_89A;
    /// Clip the body slots are playing; `oddStrangerDrive` moves it
    /// to the requested `field_89E` when it applies a clip change.
    /* 0x89C */ s16 field_89C;
    /* 0x89E */ s16 field_89E;
    /// Frames since the last clip change: counted up every
    /// `oddStrangerDrive` tick and cleared when a change is applied.
    /* 0x8A0 */ u16 field_8A0;
    /* 0x8A2 */ s16 field_8A2;
    /* 0x8A4 */ s16 field_8A4;
    /* 0x8A6 */ s16 field_8A6;
    /* 0x8A8 */ s16 field_8A8;
    /// Playback rate of the blend slots, and the weight (out of 0x1000) the
    /// blend pose gets when `oddStrangerTickBlended` mixes it into the
    /// body pose; both are seeded (0x30, 0x800) when a blend clip starts.
    /* 0x8AA */ u16  field_8AA;
    /* 0x8AC */ s16  field_8AC;
    /* 0x8AE */ s16  field_8AE;
    /* 0x8B0 */ s16  field_8B0;
    /* 0x8B2 */ byte pad_8B2[2];
    /// Last animation state `oddStrangerDormant` acted on; the same
    /// de-duplication slot `_Actor401300Work` keeps at +0x8BC.
    /* 0x8B4 */ s32            field_8B4;
    /* 0x8B8 */ EffectSpawnArg field_8B8;
    /// Offset the actor's state-3/5/7/8 effects spawn at, passed as the
    /// `Gp_SpawnEff` position: the same local `SVECTOR` `Actor401300` keeps on
    /// the stack for the 3013B6E8 triple, materialised into the work block
    /// here because every one of the four spawns reads it.
#if ODD_STRANGER_HIT_FX_OFFSET
    /* 0x8C0 */ SVECTOR field_8C0; // offset of the hit effect from its part (actor_401000's build)
#endif
    /* 0x8C8 */ byte               pad_8C8[2];
    /* 0x8CA */ u8                 field_8CA; // sight cooldown, in actor_401800's build (actor_401000's keeps it in field_C1B)
    /* 0x8CB */ byte               pad_8CB[5];
    /* 0x8D0 */ WorldCollisionBody field_8D0;
    /// Contact records of the `field_8D0` node, also the enemy's `recs`;
    /// the movement helpers walk them twelve at a time.
    /* 0x8F0 */ WorldCollisionContact field_8F0[12];
    /* 0xA10 */ WorldCollisionBody    field_A10;
    /// Contact records of the `field_A10` node.
    /* 0xA30 */ WorldCollisionContact field_A30[12];
    /* 0xB50 */ WorldCollisionBody    field_B50;
    /// The single obstacle record the `field_B50` node is registered against.
    /* 0xB70 */ WorldCollisionContact field_B70;
    /// Light matrix `func_actor_401000_80133274` binds to the model's
    /// `TmdObject::lightMtx` (the color matrix is `field_BA8`, which is the
    /// same pair `_Actor401300Work` keeps at +0xC28 / +0xC48).
    /* 0xB88 */ MATRIX field_B88;
    /// Saved at 0xBA8 and copied over 0xBC8 when
    /// `func_actor_401000_80138F50` enters its state; the same pair
    /// `_Actor401300Work` keeps at +0xC48 / +0xC68.
    /* 0xBA8 */ MATRIX field_BA8;
    /* 0xBC8 */ MATRIX field_BC8;
    /// Cleared by `func_actor_401000_80133274` right after the `field_A10`
    /// node is linked; the same slot `_Actor401300Work` keeps at +0xC88.
    /* 0xBE8 */ s16  field_BE8;
    /* 0xBEA */ s16  field_BEA;
    /* 0xBEC */ s16  field_BEC;
    /* 0xBEE */ byte pad_BEE[2];
    /// Forward direction `oddStrangerSidestep` rebuilds from the wrapped
    /// turn toward the player: `gfxRotMatrixY` on the turn then its second
    /// column, normalised, and finally scaled by the `field_C0A` draw. The same
    /// slot `_Actor401300Work` keeps at +0xC8C.
    /* 0xBF0 */ SVECTOR field_BF0;
    /// Position `oddStrangerTick` snaps the root coordinate to when
    /// a 0xB/0xD state transition arrives: written by the transition handler and
    /// loaded into `coord.t` with `composeStamp` cleared so the local matrix is rebuilt.
    /* 0xBF8 */ SVECTOR field_BF8;
    /// Turn angle `oddStrangerTurnAround` rebuilds the facing from, and
    /// the yaw it is driven to: each entry nudges `field_C00` by 0x89 toward
    /// `field_C02` and stops once they meet, and `gfxRotMatrixY` /
    /// `actorRescaleYaw` turn that angle into the root rotation. The
    /// same pair `_Actor401300Work` keeps at +0xC94 / +0xC96.
    /* 0xC00 */ s16 field_C00;
    /* 0xC02 */ s16 field_C02;
    /// Turn countdown `oddStrangerAdvance` runs while it walks the
    /// actor at the player: the `detectPlayerOutOfReach` probe reads it
    /// signed, the step helper and the countdown itself through a `(u16)`.
    /* 0xC04 */ s16 field_C04;
    /// Clip-phase latch `oddStrangerChase` runs the 8 / -1 / 0 march
    /// off: 8 flips to -1 once `field_8A2` reaches 0x18, -1 flips to 0 at 0x12,
    /// and 0 keys the 5-frame exit window. The same slot `Actor01900Work` keeps
    /// at +0xC26.
    /* 0xC06 */ s16 field_C06;
    /// Turn direction `oddStrangerSidestep` toggles as it enters: 0 (the
    /// unseeded state) draws a sign from `gRandomLcgState`, and each entry flips it
    /// to the other side. Selects the `field_89E` clip and the `field_C12` sign.
    /// The same slot `_Actor401300Work` keeps at +0xC9C.
    /* 0xC08 */ s16 field_C08;
    /// Turn length `oddStrangerSidestep` rebuilds the forward direction
    /// with: seeded to 0xDE, taken signed by the `gte_lddp` draw and halved
    /// while the actor overlaps an obstacle record. The same slot
    /// `_Actor401300Work` keeps at +0xC9E.
    /* 0xC0A */ s16 field_C0A;
    /// Forward step `oddStrangerGrabRelease` walks the root by, feeding the
    /// same `MoveForwardNonzero` helper `_Actor401300Work` keeps at +0xC98.
    /// Set to -0x78 when the live-actor flag goes up, halved while the actor
    /// overlaps an obstacle record. The three reads widen it differently: the
    /// `detectPlayerOutOfReach` probe takes the signed value, while the
    /// step helper and the halving read it back through a `(u16)`.
    /* 0xC0C */ s16  field_C0C;
    /* 0xC0E */ byte pad_C0E[2];
    /// Frame-length bias `oddStrangerIdle` reseeds the `field_6`
    /// countdown from, plus a 0-15 `gRandomLcgState` draw. The 401300 sibling keeps
    /// the same bias at +0xCA0, and the countdown `Actor01900` runs off +0xC10
    /// is the same slot.
    /* 0xC10 */ u16 field_C10;
    /// Turn step `oddStrangerSidestep` adds to (or subtracts from) the
    /// wrapped facing each entry; the same slot `_Actor401300Work` keeps at
    /// +0xCA2 and `Actor01900Work` at +0xC14.
    /* 0xC12 */ s16 field_C12;
    /// Third of the four halfwords `func_actor_401000_80133274` copies out of
    /// the `spawnArg1`-selected record; not read anywhere yet.
    /* 0xC14 */ s16 field_C14;
    /// Radius `oddStrangerDormant` and `func_actor_401000_80138F50`
    /// test the actor's distance from `gPlayerStatus.coordMtx` against.
    /* 0xC16 */ u16 field_C16;
    /// The three bytes `oddStrangerApplyCommand` copies out of the front of
    /// the message payload; the same triple `Actor01900Work` keeps at +0xC34.
    /* 0xC18 */ u8 field_C18[3];
    /// Turn cooldown `oddStrangerTurnAround` spends an entry on: while it
    /// is up the actor keeps the state-8 arm instead of the 0xB one, and each
    /// entry it is up it counts down by one. The same slot `_Actor401300Work`
    /// keeps at +0xC1F.
    /* 0xC1B */ u8 field_C1B;
    /// The two helper tasks killed before the nodes are unlinked; the same
    /// pair `Actor01900Work` keeps at +0xC38 / +0xC3C.
    /* 0xC1C */ Task* field_C1C;
    /* 0xC20 */ Task* field_C20;
    /// Counter `oddStrangerTurnAround` gates the turn-entry obstacle
    /// probe on: below 2 the actor keeps the state-8 arm whatever the range
    /// check says. The same slot `_Actor401300Work` keeps at +0xD1C.
    /* 0xC24 */ s16 field_C24;
    /// Wraps counter `oddStrangerSidestep` counts the turn entries with:
    /// nonzero picks the un-biased `field_C12` arm, and each entry increments
    /// it. The same slot `_Actor401300Work` keeps at +0xD1E.
    /* 0xC26 */ s16 field_C26;
    /// Latch `oddStrangerGrabRelease` clears after sending the closing
    /// 0x3F1 message, gating on it being 1 the same way the 0x3ED probe does.
    /// The same slot `DesertChaserWork` keeps at +0xC28.
    /* 0xC28 */ s16  field_C28;
    /* 0xC2A */ byte pad_C2A[2];
    /// Ring of the last seven view-space positions `oddStrangerTick`
    /// records, one per step; `field_C7C` is the write cursor.
    /* 0xC2C */ SVECTOR field_C2C[7];
    /* 0xC64 */ byte    pad_C64[0x18];
    /// Cleared by `func_actor_401000_80133274` once both obstacle tables have
    /// been dropped; the write cursor `_Actor401300Work` keeps at +0xD78.
    /* 0xC7C */ s16  field_C7C;
    /* 0xC7E */ byte pad_C7E[2];
} OddStrangerWork;
STATIC_ASSERT_SIZEOF(OddStrangerWork, ODD_STRANGER_HIT_FX_OFFSET ? 0xC80 : 0xC78);

/// The actor's state handlers, indexed by `OddStrangerWork::field_0`.
/// `oddStrangerTick` copies the table to its frame before
/// dispatching. Same shape as `Actor01900StateTable` / `Actor401300StateTable`.
typedef struct OddStrangerStateTable {
    TaskFunc fn[34];
} OddStrangerStateTable;
STATIC_ASSERT_SIZEOF(OddStrangerStateTable, 0x88);

typedef struct {
    ActorTransform value;
    u8             retained[8];
} OddStrangerTransformStorage;
STATIC_ASSERT_SIZEOF(OddStrangerTransformStorage, 32);

/// Animation-state view shared by actor_401000 and actor_401800. Each actor
/// owns a larger work block; these are the fields their animation driver uses.
typedef struct OddStrangerRigWork {
    /* 0x000 */ s16            field_0; // Actor state
    /* 0x002 */ s16            field_2; // State step, -1 on entry
    /* 0x004 */ byte           pad_4[0x18];
    /* 0x01C */ ActorAnimRig19 rig;
    /* 0x458 */ ActorAnimRig19 blend;
    /* 0x894 */ byte           pad_894[4];
    /* 0x898 */ s16            field_898; // Pending clip change: cross-fade, reset, running
    /* 0x89A */ s16            field_89A; // Nonzero while pose blending is active
    /* 0x89C */ s16            field_89C; // Previous clip
    /* 0x89E */ s16            field_89E; // Requested clip
    /* 0x8A0 */ u16            field_8A0; // Frames since the clip change
    /* 0x8A2 */ s16            field_8A2; // Body slot rate
    /* 0x8A4 */ byte           pad_8A4[2];
    /* 0x8A6 */ s16            field_8A6; // Blend clip change request
    /* 0x8A8 */ s16            field_8A8; // Blend clip
    /* 0x8AA */ u16            field_8AA; // Blend slot rate
    /* 0x8AC */ s16            field_8AC; // Blend weight
    /* 0x8AE */ s16            field_8AE; // Target head yaw
    /* 0x8B0 */ s16            field_8B0; // Current head yaw
    /* 0x8B2 */ byte           pad_8B2[2];
    /* 0x8B4 */ s32            field_8B4; // Last animation event index
} OddStrangerRigWork;
STATIC_ASSERT_SIZEOF(OddStrangerRigWork, 0x8B8);

#include "main/task_types.h"

void oddStrangerTickBlended(Task* arg0);
void oddStrangerDrive(Task* arg0);
s32  oddStrangerPlayMessage(Task* arg0, s32 arg1, AnimationPlayRequest* arg2, s32 arg3);

s32 oddStrangerAnimEvent(OddStrangerRigWork* work);

void oddStrangerExit(Task* task);
s32  oddStrangerApplyCommand(Task* arg0, s32 arg1, u16* arg2, s32 arg3);
void oddStrangerGrabRelease(Task* arg0);
void oddStrangerBackOff(Task* arg0);
void oddStrangerSidestep(Task* arg0);
void oddStrangerAdvance(Task* arg0);
void oddStrangerFacePlayer(Task* arg0);
void oddStrangerScriptPose8(Task* arg0);
void oddStrangerScriptPoseD(Task* arg0);
void oddStrangerScriptPose3(Task* arg0);
void oddStrangerScriptPoseB(Task* arg0);
void oddStrangerScriptPose2(Task* arg0);
void oddStrangerDie(Task* arg0);
void oddStrangerHoldAim(Task* arg0);

void oddStrangerSpawnHitEffect(Task* arg0, s16 arg1, s32 arg2);
void oddStrangerIdle(Task* arg0);
void oddStrangerDormant(Task* arg0);
void oddStrangerGrab(Task* arg0);
void oddStrangerGrabHold(Task* arg0);
void oddStrangerStunned(Task* arg0);
void oddStrangerTurnAround(Task* arg0);

void oddStrangerChase(Task* arg0);
void oddStrangerPatrol(Task* arg0);

/// Whether the XZ offset `d` reaches at least `r`: `overlayOutOfRange` with
/// the scratch cursor published before the squares.
static __inline__ s32 oddStrangerOutOfRange(SVECTOR* d, s16 r)
{
    u8*                  head;
    OverlayRangeScratch* blk;
    s32                  ret;

    head                                      = SCRATCH_STACK_CURSOR(u8);
    blk                                       = (OverlayRangeScratch*)(head - 0xC);
    ((OverlayRangeScratch*)(head - 0xC))->dx  = d->vx;
    SCRATCH_STACK_CURSOR(OverlayRangeScratch) = blk;
    blk->dz                                   = d->vz;
    blk->r                                    = r;
    ((OverlayRangeScratch*)(head - 0xC))->dx *= ((OverlayRangeScratch*)(head - 0xC))->dx;
    blk->dz                                  *= blk->dz;
    blk->r                                   *= blk->r;
    SCRATCH_STACK_CURSOR(u8)                  = head;
    ret                                       = ((OverlayRangeScratch*)(head - 0xC))->dx + blk->dz >= blk->r;
    return ret;
}

void oddStrangerStalk(Task* arg0);
void oddStrangerTick(Enemy* enemy, Task* actor);

void oddStrangerWalkingDeath(Task* arg0);

void oddStrangerTakeHit(Task* arg0);

/* Defined by each package. */
s32 oddStrangerPushContacts(Task* arg0, WorldCollisionContact* recs, s16 count);

#endif /* SRC_SHARED_ODD_STRANGER_H */
