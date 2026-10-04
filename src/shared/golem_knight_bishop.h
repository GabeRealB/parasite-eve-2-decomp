/* The code the Knight GOLEM (actor_402200) and the Bishop GOLEM (actor_403900)
 * share. It fades its model's translucency in and out to appear and vanish,
 * and queues a frame-capture pass at its depth so the translucent body
 * refracts the scene. It reappears
 * behind the player (0x5AA back along the player's heading) or when the player
 * enters one of the room's trigger boxes. It strikes, or grabs the player in a
 * hold that can end in an instant kill. While it aims, it draws a red beam
 * from its fourth part. Hits of kinds 1/2 make the translucency flicker and spark.
 * Low HP drops it to its knees, where it writhes. Death either collapses it or
 * shrinks it away. A dispatcher runs one of twelve sequences chosen by
 * field_6CC. A step-forward helper, per-animation sound cues and a hold cue
 * timer complete the frame. The dead state uses inlined copies of the reseed,
 * tint and shadow, which move into the shared header.
 *
 * Each package builds the library for its own type: it defines
 * GOLEM_KNIGHT_BISHOP_KIND as GOLEM_KNIGHT (actor_402200) or GOLEM_BISHOP
 * (actor_403900) before including this header, and the parameters below
 * follow from it.
 *
 * Include this header in the prologue and each fragment at its function's
 * position.
 */

#ifndef SRC_SHARED_GOLEM_KNIGHT_BISHOP_H
#define SRC_SHARED_GOLEM_KNIGHT_BISHOP_H

#define GOLEM_KNIGHT 1
#define GOLEM_BISHOP 2
#ifndef GOLEM_KNIGHT_BISHOP_KIND
#error "define GOLEM_KNIGHT_BISHOP_KIND (GOLEM_KNIGHT or GOLEM_BISHOP) before including golem_knight_bishop.h"
#endif

/* Per type: its id (also in its collision keys, 0x30000 | id); how often a
 * hold re-decides whether it ends; what one hit adds to the hit load and the
 * load at which the box approach breaks off; the aim countdown, whose last
 * frames project the beam; the idle sequence's cap on its approach counter;
 * and the base of the random recovery delay. */
#if GOLEM_KNIGHT_BISHOP_KIND == GOLEM_KNIGHT
#define GOLEM_KNIGHT_BISHOP_ID            0x16
#define GOLEM_KNIGHT_BISHOP_GRAB_RECHECK  0x1E
#define GOLEM_KNIGHT_BISHOP_HIT_WEIGHT    0xA0
#define GOLEM_KNIGHT_BISHOP_AIM_TIME      0x14
#define GOLEM_KNIGHT_BISHOP_IDLE_LIMIT    12
#define GOLEM_KNIGHT_BISHOP_RECOVER_DELAY 0x4B
#else
#define GOLEM_KNIGHT_BISHOP_ID            0x27
#define GOLEM_KNIGHT_BISHOP_GRAB_RECHECK  0x14
#define GOLEM_KNIGHT_BISHOP_HIT_WEIGHT    0xFA
#define GOLEM_KNIGHT_BISHOP_AIM_TIME      0xA
#define GOLEM_KNIGHT_BISHOP_IDLE_LIMIT    8
#define GOLEM_KNIGHT_BISHOP_RECOVER_DELAY 0x2D
#endif

#include "types.h"

#include "actors/actor.h"

#include "main/coord.h"
#include "main/task_types.h"

/// What a `GolemKnightBishopRegion` covers, and how the golem answers the
/// player standing inside it.
enum {
    GOLEM_KNIGHT_BISHOP_REGION_CIRCLE = 0, // A circle: the golem takes the spot behind the player and grabs them
    GOLEM_KNIGHT_BISHOP_REGION_BOX    = 1, // A box: the golem appears at the entry's post and attacks from there
};

/// One region of a room that draws the golem onto the player.
///
/// A room's regions form one table, chosen at spawn by stage and room. The
/// golem tests the player's world x / z against each entry in order and acts
/// on the first one that holds them. A circle uses only its centre and radius
/// and leaves the box edges zero. A box names the area watched and the post,
/// outside that area, the golem attacks from: it faces `param.heading` there
/// and breaks off once the player's bearing leaves that heading by more than
/// 0x180.
typedef struct {
    s16 kind;        // shape and response (`GOLEM_KNIGHT_BISHOP_REGION_CIRCLE`, `GOLEM_KNIGHT_BISHOP_REGION_BOX`)
    union {
        s16 radius;  // circle: planar distance from the centre the player has to be inside
        s16 heading; // box: y rotation the golem takes at its post, 0x1000 to the turn
    } param;
    s16 x;           // world x of the circle's centre, or of a box's post
    s16 z;           // world z of the same point
    s16 minX;        // box: edges of the watched area, all four exclusive
    s16 maxZ;
    s16 maxX;
    s16 minZ;
} GolemKnightBishopRegion;
STATIC_ASSERT_SIZEOF(GolemKnightBishopRegion, 0x10);

/// One 8-byte entry of the spawn's placement run `D_actor_402200_80153C78`,
/// terminated by a zero `field_0`: when the session's stage (`field_2`) and
/// room (`field_4`) match, `field_0` indexes the box tables and `field_6` is
/// the box count stored to `GolemKnightBishopWork::field_6FA`.
typedef struct GolemKnightBishopSpot {
    s16 field_0;
    s16 field_2;
    s16 field_4;
    u16 field_6;
} GolemKnightBishopSpot;
STATIC_ASSERT_SIZEOF(GolemKnightBishopSpot, 0x8);

/// Per-instance work block the overlay's setup `func_actor_402200_80137444`
/// allocates with `memCalloc(0x71C)` and parks in the 0x1C slot below (the
/// task's `Task::work`).
///
/// `field_6E2` is the ground-shadow shade the ground-quad body
/// `func_actor_402200_8013806C` hands to `Gp_DrawEffGroundQuad`, which draws
/// nothing for a negative value: the body turns the calloc'd zero into -1 the
/// first time it runs, so an actor that never raises the shade casts no
/// shadow.
///
/// `field_6F4` is the actor's phase, written and read as a signed halfword:
/// the frame handler clears it on entry, `func_actor_402200_801381E0` raises
/// it to 1 while the remaining-enemy count is positive, and the handler
/// branches on 0 / 1 thereafter.
///
/// `field_6EA` is a pending request that `golemKnightBishopUpdateTintInline` applies and
/// clears; see there for the values.
///
/// `field_718` arms a one-shot vocal cue and `field_71A` is its frame counter.
/// While the flag is clear the body does nothing; once it is set the counter
/// runs up, plays the actor's cue at 0x14, and at 0x5F asks the scene for
/// message 0x3ED - clearing the flag and sending 0x3F1 instead if the scene
/// refuses it.
/// `rig.slots[1]` is the animation slot the cue body `func_actor_402200_80135BE0`
/// hands to `Gp_AnimGetRec`: the second of the rig's slots, the same one the
/// other actor overlays' cue bodies play from. `field_6CA` latches the record's
/// two cue bits (`0x30`) for the next frame, and `field_712` is the running entry index into the overlay's
/// cue-id table `D_actor_402200_80138420` - zero disarms the body, and while it
/// is set the two adjacent words `[field_712 * 2 - 1]` and `[field_712 * 2]`
/// are the cue ids it plays.
typedef struct GolemKnightBishopWork {
    ActorAnimRig19         rig;
    MATRIX                 field_43C;
    MATRIX                 field_45C;
    byte                   field_47C[8];
    void*                  field_484;
    WorldCollisionContact* field_488;
    s16                    field_48C;
    s16                    field_48E;
    /// Halfword the attack sequences park alongside the timers: state 0 stores
    /// -0xA7 when `field_6D2` is clear and 0x109 when it is set. The branch
    /// sequence `func_actor_402200_80135630` stores the same pair, so it is the
    /// same slot set's vertical placement.
    s16  field_490;
    byte pad_492[2];
    /// Hit descriptor the flinch handler `func_actor_402200_80131F54` and the
    /// hurt states `func_actor_402200_80133AEC` / `func_actor_402200_80134194`
    /// store on the frame a hit lands: the damage amount `field_716` with the
    /// tag bits 0x30000 OR'd in.
    s32 field_494;
    /// Halfword the attack sequences arm to 0x15E next to `field_490`.
    s16 field_498;
    /// Hit-pending flags, raised together with `field_494`: bit 0x8000 is the
    /// flag the hit handler clears when it consumes the descriptor. The frame
    /// handler `func_actor_402200_80137A1C` makes the enemy lockable only while
    /// it is set, and `func_actor_402200_80131F54` clears `field_6C6` as it
    /// raises it.
    u16                    field_49A;
    WorldCollisionContact  field_49C[3];
    byte                   field_4E4[8];
    void*                  field_4EC;
    WorldCollisionContact* field_4F0;
    s16                    field_4F4;
    s16                    field_4F6;
    s16                    field_4F8;
    byte                   pad_4FA[2];
    s32                    field_4FC;
    s16                    field_500;
    /// Flag word the attack sequences raise: bit 0x4000 is set by state 0 of
    /// both `func_actor_402200_80135630` and `func_actor_402200_80135A24`,
    /// alongside clearing bit 0x4000 of the 0x502 word below.
    u16                    field_502;
    WorldCollisionContact  field_504[4];
    byte                   field_564[8];
    void*                  field_56C;
    WorldCollisionContact* field_570;
    s16                    field_574;
    s16                    field_576;
    s16                    field_578;
    byte                   pad_57A[2];
    s32                    field_57C;
    s16                    field_580;
    u16                    field_582;
    WorldCollisionContact  field_584;
    byte                   field_59C[8];
    void*                  field_5A4;
    void*                  field_5A8;
    s16                    field_5AC;
    s16                    field_5AE;
    s16                    field_5B0;
    byte                   pad_5B2[2];
    s32                    field_5B4;
    s16                    field_5B8;
    u16                    field_5BA;
    byte                   field_5BC[8];
    void*                  field_5C4;
    WorldCollisionContact* field_5C8;
    s16                    field_5CC;
    s16                    field_5CE;
    s16                    field_5D0;
    byte                   pad_5D2[2];
    s32                    field_5D4;
    s16                    field_5D8;
    u16                    field_5DA;
    s16                    field_5DC;
    s16                    field_5DE;
    s16                    field_5E0;
    byte                   pad_5E2[2];
    s16                    field_5E4;
    s16                    field_5E6;
    s16                    field_5E8;
    byte                   pad_5EA[2];
    s16                    field_5EC;
    s16                    field_5EE;
    WorldCollisionContact* field_5F0;
    /// Head of the actor's first `WorldCollisionContact` table; `func_actor_402200_801329A4`
    /// branches on its `key` before clearing it.
    WorldCollisionContact  field_5F4;
    byte                   field_60C[8];
    void*                  field_614;
    void*                  field_618;
    s16                    field_61C;
    s16                    field_61E;
    s16                    field_620;
    byte                   pad_622[2];
    s32                    field_624;
    s16                    field_628;
    u16                    field_62A;
    s16                    field_62C;
    s16                    field_62E;
    s16                    field_630;
    byte                   pad_632[2];
    s16                    field_634;
    s16                    field_636;
    s16                    field_638;
    byte                   pad_63A[2];
    s16                    field_63C;
    s16                    field_63E;
    WorldCollisionContact* field_640;
    /// Head of a second `WorldCollisionContact` table, cleared by state 5 of
    /// `func_actor_402200_801329A4`.
    WorldCollisionContact field_644;
    /// `func_800FDB18` argument record for the hit spark: the fourth part's
    /// coordinate, 0x500, 2.
    EffectSpawnArg field_65C;
    s32            field_664;
    s32            field_668;
    s32            field_66C;
    byte           pad_670[4];
    /// Copy of the root coordinate's matrix `func_actor_402200_80134968`
    /// takes when its fade-out finishes, with `scale` reset to 0x1000 beside
    /// it.
    MATRIX field_674;
    /// Per-axis scale applied to the root's rotation.
    VECTOR scale;
    s32    field_6A4;
    s32    field_6A8;
    s32    field_6AC;
    byte   pad_6B0[4];
    /// Box table the scans `func_actor_402200_80132D78` and
    /// `func_actor_402200_80132688` walk, `field_6FA` entries of 0x10 bytes
    /// each.
    GolemKnightBishopRegion* field_6B4;
    s32                      field_6B8;
    /// Sound event id the sequence body `func_actor_402200_8013539C` queues: the
    /// overlay's cue word `D_actor_402200_80138468` with the `Enemy` work id's
    /// high nibble in bits 8-11, the same construction the cue body
    /// `func_actor_402200_80135BE0` uses on `D_actor_402200_80138420`. Stored
    /// back to the block and re-read from there as the first argument of
    /// `SndEvt_EnqueueType6`.
    s32 field_6BC;
    /// Animation id the frame code reseeds slots 1..0x12 with; the reseed body
    /// `func_actor_402200_80137EEC` also indexes the blend table
    /// `D_actor_402200_801383AC` with it.
    s16 field_6C0;
    /// Animation id the slots were last reseeded with, so the reseed runs once
    /// per change rather than every frame.
    s16 field_6C2;
    /// Frames the current animation has been ticking; the reseed clears it and
    /// the tick path walks it up by one a frame.
    s16 field_6C4;
    /// Flinch countdown: `func_actor_402200_80131F54` arms it from
    /// `Gp_GetIdParam2` when a hit lands and ticks it down a frame at a time,
    /// raising `field_494`/`field_49A` on the frame it runs out. While it is
    /// non-zero a hit is already being flinched, so the sequence bodies arm
    /// the pair immediately only when it is zero.
    s16 field_6C6;
    /// Cleared on the frame the sequence body `func_actor_402200_8013539C`
    /// reseeds the animation.
    s16 field_6C8;
    u16 field_6CA;
    /// Set to 4 when the sequence restarts in mode 2, cleared otherwise.
    s16 field_6CC;
    /// State `func_actor_402200_8013539C` advances: 0 reseeds the animation at
    /// `field_6C0` and arms the cue, 1 waits for `field_6C4` to reach 0x37 and
    /// then drops the state back to 0 so the reseed runs again.
    /// The attack sequence `func_actor_402200_801354B0` runs the same shape
    /// over three states: its state 0 picks between slot sets 9 and 0xA on
    /// `field_6D2` and parks the state on the matching one, and states 1 / 2
    /// each wait out their own `field_6C4` threshold (0x50 and 0x3B) before
    /// dropping back to 0.
    s16 field_6CE;
    s16 field_6D0;
    /// Which-side flag the target body `func_actor_402200_80131F54` raises from
    /// a dot product of the offset to the actor it is tracking: 1 when the
    /// product comes out zero, 0 otherwise. The sequence bodies branch on it -
    /// `func_actor_402200_801354B0` picks between slot sets 9 and 0xA, and
    /// `func_actor_402200_80135630` / `func_actor_402200_80135A24` between 0xD
    /// and the set at `field_6C0`.
    s16 field_6D2;
    /// Countdown `func_actor_402200_801347F4` rolls from the `gRandomLcgState` LCG
    /// (0x4B..0x6A) when it reseeds the animation, and ticks down a frame at a
    /// time until it runs out and the cue fires.
    u16 field_6D4;
    s16 field_6D6;
    s16 field_6D8;
    /// Timer pair the reseed arms alongside `field_6DE`.
    s16 field_6DA;
    s16 field_6DC;
    /// Third timer the reseed arms; written last of the three.
    s16 field_6DE;
    /// Fourth timer `func_actor_402200_801347F4` clears alongside the trio
    /// above when its countdown runs out.
    s16 field_6E0;
    s16 field_6E2;
    s16 field_6E4;
    s16 field_6E6;
    s16 field_6E8;
    s16 field_6EA;
    /// Sequence mode `func_actor_402200_8013539C` tests: the reseed arms the
    /// cue unless it is already 1, and a restart that finds it 1 flips it to 2.
    s16 field_6EC;
    s16 field_6EE;
    /// Latch the attack sequences park the slot set in: state 0 stores 1 or 2
    /// next to `field_6C0`, and state 1 reads it back to pick the frame count
    /// it waits for (0x2C for the 0x11 animation, 0x19 otherwise).
    s16 field_6F0;
    /// Pair `func_actor_402200_80135A24` parks at 2 while it runs, cleared when
    /// its countdown runs out.
    s16 field_6F2;
    s16 field_6F4;
    s16 field_6F6;
    s16 field_6F8;
    /// Entry count of the box table at `field_6B4`, read as a signed halfword;
    /// a non-positive count disarms the scan.
    s16 field_6FA;
    /// Screen x / y and quartered depth of the two points
    /// `func_actor_402200_80135D5C` projects.
    s16 field_6FC[2];
    s16 field_700[2];
    s16 field_704[2];
    /// Index of the box the scan last reported a hit on.
    s16 field_708;
    s16 field_70A;
    s16 field_70C;
    s16 field_70E;
    s16 field_710;
    s16 field_712;
    /// Second per-state latch, read and written as a signed halfword: the
    /// attack sequences raise it to 1 in state 0 and state 1 bumps it to 2 on
    /// the frame it still equals the state.
    s16 field_714;
    /// Damage amount the hit handlers OR into `field_494`; read as a signed
    /// halfword on the frame the hit lands.
    s16 field_716;
    s16 field_718;
    s16 field_71A;
} GolemKnightBishopWork;
STATIC_ASSERT_SIZEOF(GolemKnightBishopWork, 0x71C);

/// 0x18-byte block `func_actor_402200_80132E34` takes from the scratch stack
/// to place the actor relative to the player: `in` is the offset rotated
/// through the player's root coordinate into `out`.
typedef struct GolemKnightBishopOffsetScratch {
    VECTOR  out;
    SVECTOR in;
} GolemKnightBishopOffsetScratch;
STATIC_ASSERT_SIZEOF(GolemKnightBishopOffsetScratch, 0x18);

/// 0x48-byte block `func_actor_402200_80135D5C` takes from the scratch stack
/// to aim the actor: `m` is the root's world matrix brought local to the
/// fourth part, `out` the GTE's rotated offset, and `pts` the two world points
/// (root-based aim point, fourth-part offset) projected through `GsWSMATRIX`
/// into `sxy` and the quartered screen z `otz`.
typedef struct GolemKnightBishopAimScratch {
    MATRIX  m;
    VECTOR  out;
    SVECTOR pts[2];
    s32     sxy;
    s32     otz;
} GolemKnightBishopAimScratch;
STATIC_ASSERT_SIZEOF(GolemKnightBishopAimScratch, 0x48);

/// One 4-byte entry of `D_actor_402200_801383D8`: the first entry whose
/// `frame` is not below the animation frame `GolemKnightBishopWork::field_6C4`
/// supplies `value` for `field_6C8`.
typedef struct GolemKnightBishopFrameStep {
    s16 frame;
    u16 value;
} GolemKnightBishopFrameStep;
STATIC_ASSERT_SIZEOF(GolemKnightBishopFrameStep, 4);

/// 0x30-byte block `func_actor_402200_80131F54` takes from the scratch stack:
/// `delta` receives the `func_800E0C10` push-back and is then reused for the
/// offset to the player, and `ofs` is the spark offset handed to
/// `func_800FDB18`.
typedef struct GolemKnightBishopHitScratch {
    WorldCollisionDelta delta;
    byte                pad_10[0x10];
    SVECTOR             ofs;
    byte                pad_28[8];
} GolemKnightBishopHitScratch;
STATIC_ASSERT_SIZEOF(GolemKnightBishopHitScratch, 0x30);

/// The scratch-pad block of the box scan: `out` first holds the player's
/// planar offset from a box's centre, then the offset `in` behind the player
/// rotated through the player's root coordinate.
typedef struct GolemKnightBishopBoxScratch {
    VECTOR  out;
    byte    pad_10[0x10];
    SVECTOR in;
} GolemKnightBishopBoxScratch;
STATIC_ASSERT_SIZEOF(GolemKnightBishopBoxScratch, 0x28);

/// The scratch-pad block of the red trail drawer: the normalised screen
/// direction of the trail, the depth and its per-segment step, the six
/// vertex pairs of the current segments and the endpoint increments.
typedef struct GolemKnightBishopTrailScratch {
    VECTOR  dir;
    SVECTOR norm;
    s32     z;
    s32     dz;
    u16     x[6];
    u16     y[6];
    s16     dx;
    s16     dy;
} GolemKnightBishopTrailScratch;
STATIC_ASSERT_SIZEOF(GolemKnightBishopTrailScratch, 0x3C);

/// The scratch-pad block of the grab: the query sent with message 0x3F8, the
/// animation sent with message 0x3FF, the placement sent with message 0x3E9,
/// and the offset `in` rotated through the actor's root into `out`; `in` is
/// also the rotation the grab's matrix is built from.
typedef struct GolemKnightBishopGrabScratch {
    GameActorButtonPressHold query;
    AnimationPlayRequest     anim;
    ActorTransform           place;
    VECTOR                   out;
    SVECTOR                  in;
} GolemKnightBishopGrabScratch;
STATIC_ASSERT_SIZEOF(GolemKnightBishopGrabScratch, 0x5C);

void golemKnightBishopPickHitReaction(Task* arg0, s32 arg1);
void golemKnightBishopBoxScanSeq(Task* arg0);
s32  golemKnightBishopPlayerInBox(Task* arg0);
void golemKnightBishopPlaceTarget(Task* arg0);
void golemKnightBishopStrikeSeq(Task* arg0);
void golemKnightBishopTranslucencyFade(Task* arg0);
void golemKnightBishopLightFlinchSeq(Task* arg0);
void golemKnightBishopHeavyFlinchSeq(Task* arg0);
void golemKnightBishopKneelSeq(Task* arg0);
void golemKnightBishopKneelHitSeq(Task* arg0);
void golemKnightBishopCollapseDeathSeq(Task* arg0);
void golemKnightBishopPlayAnimCues(Task* arg0);
void golemKnightBishopDrawAimBeam(Task* arg0);
void golemKnightBishopDeadState(Enemy* arg0, Task* arg1);
void golemKnightBishopFrameState(Enemy* arg0, Task* arg1);
void golemKnightBishopRunSequence(Task* arg0);
void golemKnightBishopApplyScale(Task* arg0);
void golemKnightBishopKneelDeathSeq(Task* arg0);
void golemKnightBishopStepForward(Task* arg0);
void golemKnightBishopDrawShadow(Task* arg0);
void golemKnightBishopHoldCueTimer(Task* arg0);
void golemKnightBishopQueueFrameCapture(GfxCoord* arg0, s32 arg1);

/* Defined by each package. */
void golemKnightBishopTakeHits(Task* arg0);
void golemKnightBishopUpdateTint(Task* arg0);
void golemKnightBishopTickAnim(Task* arg0);
void golemKnightBishopSpawn(Enemy* arg0, Task* arg1);
void golemKnightBishopAimFromPart(Task* arg0);
void golemKnightBishopIdleSeq(Task* arg0);
void golemKnightBishopGrabSeq(Task* arg0);
void golemKnightBishopBoxApproachSeq(Task* arg0);
void golemKnightBishopRecoverSeq(Task* arg0);

static inline void golemKnightBishopTickAnimInline(Task* arg0);
static inline void golemKnightBishopDrawShadowInline(Task* arg0);

/// Relights the actor from its root coordinate, then applies and clears a
/// pending request in `field_6EA`: 1 and 2 set the translation of the model's
/// colour matrix to (0, 0, 0x400) and (0xFFF, 0xFFF, 0xFFF) respectively.
static __inline__ void golemKnightBishopUpdateTintInline(Task* task)
{
    GolemKnightBishopWork* work;
    GfxCoord*              obj;
    VECTOR                 vec;
    s16                    r;
    s16                    g;
    s16                    b;

    obj    = task->extra.tmd->coords;
    work   = task->work;
    vec.vx = obj->workm.t[0];
    vec.vy = obj->workm.t[1];
    vec.vz = obj->workm.t[2];
    Gp_UpdateActorColor(task->spawnArg2.pointer, &vec, 0, 0);
    switch (work->field_6EA) {
        case 1:
            r = 0;
            g = 0;
            b = 0x400;
            Gp_SetObjTrans(task->extra.tmd, r, g, b);
            work->field_6EA = 0;
            break;
        case 2:
            r = 0xFFF;
            g = 0xFFF;
            b = 0xFFF;
            Gp_SetObjTrans(task->extra.tmd, r, g, b);
            work->field_6EA = 0;
            break;
        case 0:
        default:
            return;
    }
}

#endif /* SRC_SHARED_GOLEM_KNIGHT_BISHOP_H */
