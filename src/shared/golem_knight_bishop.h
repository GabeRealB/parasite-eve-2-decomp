/* The code the Knight GOLEM (actor_402200) and the Bishop GOLEM (actor_403900)
 * share. It fades its model's translucency in and out to appear and vanish,
 * and queues a frame-capture pass at its depth so the translucent body
 * refracts the scene. It reappears behind the player (0x5AA back along the
 * player's heading), beside them, or at a post when the player enters one of
 * the room's trigger boxes. It strikes, or grabs the player in a hold that
 * can end in an instant kill; an appearance for a strike can also be a feint
 * that shrinks away again. While it aims, it draws a red beam from its fourth
 * part. Hits of kinds 1/2 make the translucency flicker and spark. Low HP
 * drops it to the ground, where it writhes; it dies there or collapses from
 * standing. A dispatcher runs one of twelve sequences chosen by
 * `GolemKnightBishopWork::sequence`. A step-forward helper, per-animation
 * sound cues and the grab release update complete the frame. The dead state uses
 * inline animation, tint and shadow helpers from the inline fragment.
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

/// Binds one room to the regions the golem watches there.
///
/// Each package lists the rooms its golem is placed in as a run of these,
/// ended by an entry whose `regionTable` is 0. The spawn takes the regions of
/// every entry that names the session's stage and area, so the last such
/// entry decides. A room with no entry leaves the golem without regions.
typedef struct {
    s16 regionTable; // index of the room's region table in the package's list of them; 0 ends the run, and slot 0 of the list is empty
    s16 stage;       // stage the room is in, a `GAME_STAGE_` value
    s16 area;        // the room's area within that stage, a `GAME_AREA_` value
    u16 regionCount; // entries in that region table
} GolemKnightBishopRoomRegions;
STATIC_ASSERT_SIZEOF(GolemKnightBishopRoomRegions, 0x8);

/// Values of `GolemKnightBishopWork::sequence`, the behaviour the per-frame
/// dispatcher runs. `GolemKnightBishopWork::lastAttack` holds one of the three
/// attacks.
enum {
    GOLEM_KNIGHT_BISHOP_SEQUENCE_IDLE           = 0,  // between attacks: waits, then picks the next attack and tests its spot
    GOLEM_KNIGHT_BISHOP_SEQUENCE_GRAB           = 1,  // appears behind the player and holds them
    GOLEM_KNIGHT_BISHOP_SEQUENCE_STRIKE         = 2,  // appears by the player, as a feint or to strike
    GOLEM_KNIGHT_BISHOP_SEQUENCE_BOX_APPROACH   = 3,  // appears at a box region's post, aims and charges
    GOLEM_KNIGHT_BISHOP_SEQUENCE_RECOVER        = 4,  // stands exposed for a moment, then vanishes
    GOLEM_KNIGHT_BISHOP_SEQUENCE_LIGHT_FLINCH   = 5,  // reels from a hit of less than 0x50 damage
    GOLEM_KNIGHT_BISHOP_SEQUENCE_HEAVY_FLINCH   = 6,  // reels from a heavier hit, animated by the side it came from
    GOLEM_KNIGHT_BISHOP_SEQUENCE_KNEEL          = 7,  // falls at low hit points and writhes there
    GOLEM_KNIGHT_BISHOP_SEQUENCE_KNEEL_HIT      = 8,  // flinches from a hit taken on the ground
    GOLEM_KNIGHT_BISHOP_SEQUENCE_COLLAPSE_DEATH = 9,  // falls dead from standing
    GOLEM_KNIGHT_BISHOP_SEQUENCE_KNEEL_DEATH    = 10, // dies on the ground
    GOLEM_KNIGHT_BISHOP_SEQUENCE_REGION_SCAN    = 11, // from spawn: waits for the player to enter one of the room's regions
};

/// Values of `GolemKnightBishopWork::fadeState`, the change of appearance in
/// progress.
enum {
    GOLEM_KNIGHT_BISHOP_FADE_HIDDEN         = 0, // model not drawn, no shadow, hurt body off
    GOLEM_KNIGHT_BISHOP_FADE_APPEAR         = 1, // colour blends in, then the translucency clears; the shadow darkens
    GOLEM_KNIGHT_BISHOP_FADE_SHOWN          = 2, // fully visible
    GOLEM_KNIGHT_BISHOP_FADE_VANISH         = 3, // translucency rises, then the colour blends out; ends hidden
    GOLEM_KNIGHT_BISHOP_FADE_FEINT_APPEAR   = 4, // as the appearance, but to 11/16 of the colour and with no shadow
    GOLEM_KNIGHT_BISHOP_FADE_FEINT_SHOWN    = 5, // the feint stands, shadowless
    GOLEM_KNIGHT_BISHOP_FADE_FEINT_SHRINK   = 6, // the feint squashes and narrows as it vanishes; ends hidden
    GOLEM_KNIGHT_BISHOP_FADE_FLICKER_START  = 7, // rolls the first flicker period
    GOLEM_KNIGHT_BISHOP_FADE_FLICKER_DIM    = 8, // flickering: fades half out, tinted, shedding sparks
    GOLEM_KNIGHT_BISHOP_FADE_FLICKER_BRIGHT = 9, // flickering: fades back in, shedding sparks
};

/// Work block of a Knight or Bishop GOLEM, allocated at this size by the spawn
/// state and kept at `Task::work`.
///
/// It holds the animation rig, the matrices the model is lit by, the six
/// collision bodies with their contact tables, and the halfwords the per-frame
/// handlers and the sequences drive.
///
/// `sequence` selects the behaviour and `step` is the position inside it. A
/// sequence asks for an animation in `anim`, for movement in `forwardSpeed`
/// and for a change of appearance in `fadeState`, and the frame carries them
/// out.
///
/// The golem is hidden between attacks. It picks a spot by the player
/// (`targetPos`, `targetYaw`), tests it against the room with the two probe
/// bodies, which ride the player's root, and appears there only when nothing
/// is in the way. An appearance for a strike can be a feint: a partly faded,
/// shadowless figure that takes no damage and shrinks away again, unless the
/// player touches or shoots it, which brings the next attack at once.
///
/// Both fade lengths are divisors, so a writer sets them before selecting a
/// `fadeState` that fades.
typedef struct {
    ActorAnimRig19           rig;                    // animation context, slots and pose buffers of the body's nineteen parts
    MATRIX                   colorMtx;               // colour matrix of the model; `tintRequest` changes its translation
    MATRIX                   lightMtx;               // light matrix of the model
    WorldCollisionBody       hurtBody;               // sphere on part 3 that takes weapon hits; off while hidden, moved and grid-enabled once the golem is down
    WorldCollisionContact    hurtContacts[3];        // its hits and overlaps, also published as `Enemy::recs`
    WorldCollisionBody       groundBody;             // sphere resting on the root that keeps the standing golem on the floor and out of walls
    WorldCollisionContact    groundContacts[4];      // its grid contacts
    WorldCollisionBody       strikeBody;             // sphere with an attack key, pair-enabled while a blow is live: on parts 8 and 12 for the strike, on the root for the charge
    WorldCollisionContact    strikeContacts[1];      // what the blow landed on
    WorldCollisionBody       pathProbeBody;          // capsule on the player's root, grid-tested for a frame to see whether a wall stands between the player and the spot
    WorldCollisionBody       spotProbeBody;          // sphere on the player's root, 0x5AA behind them where a grab appears; grid-tested with the capsule for that spot
    WorldCollisionCapsule    pathProbeCapsule;       // the capsule's shape in the player's frame: `ends[1]` over the player, `ends[0]` 2000 out toward the spot
    WorldCollisionContact    probeContacts[1];       // shared by both probes; a key left here means the spot is blocked
    WorldCollisionBody       aimBeamBody;            // capsule on the root the aim beam probes with; enabled only while the golem aims
    WorldCollisionCapsule    aimBeamCapsule;         // its shape in root space: `ends[1]` the point on part 4 the beam leaves, `ends[0]` 10000 ahead
    WorldCollisionContact    aimBeamContacts[1];     // where the beam stopped
    EffectSpawnArg           hitEffectArg;           // argument record of the hit effect spawned on part 3
    VECTOR                   prevRootPos;            // root translation before this frame's step, put back when the collision resolve rejects the move
    MATRIX                   unscaledRootMtx;        // root matrix saved once a feint has appeared; the shrink rebuilds the root from it and `scale`
    VECTOR                   scale;                  // per-axis scale of the shrinking feint, 0x1000 for 1
    VECTOR                   targetPos;              // world position the golem appears at next
    GolemKnightBishopRegion* regions;                // the room's regions; NULL in a room with none, where the golem does nothing
    s32                      appearSound;            // sound event started with an appearance and stopped when it completes; 0 when none is playing
    s32                      vanishSound;            // sound event started with a vanish and stopped when it completes; 0 when none is playing
    s16                      anim;                   // animation the sequence asks for
    s16                      playingAnim;            // animation the slots were last started on; a difference from `anim` restarts them with a blend
    s16                      animFrame;              // frames since `playingAnim` started
    s16                      hitCooldown;            // frames during which further weapon hits are ignored, set by the weapon that hit; the hurt body is re-armed when it ends
    s16                      forwardSpeed;           // distance the root moves along its facing each frame; negative backs away
    u16                      prevCueFlags;           // cue bits of the previous frame's animation record, so a cue plays once on its falling edge
    s16                      sequence;               // running behaviour, a `GOLEM_KNIGHT_BISHOP_SEQUENCE_` value
    s16                      step;                   // step inside that sequence; each sequence numbers its own
    s16                      shrinkStep;             // stage of the feint's shrink (0 squashing, 1 narrowing as it stretches, 2 held)
    s16                      hitFromFront;           // side the last weapon hit came from (1 in front of the golem, 0 behind)
    s16                      timer;                  // countdown of the running sequence, in frames; the end of a fatal grab counts its steps here instead
    s16                      auxTimer;               // second counter of the running sequence: frames before a hold starts to hurt, frames of aiming left, or the strike's request to arm the hurt body
    s16                      translucency;           // how see-through the model is drawn (0 solid, 0xFF invisible)
    s16                      fadeState;              // change of appearance in progress, a `GOLEM_KNIGHT_BISHOP_FADE_` value
    s16                      translucencyFadeFrames; // frames `translucency` and `shadowShade` take to cross their range in a fade
    s16                      colorBlendFadeFrames;   // frames the model's colour blend takes to cross its range in a fade
    s16                      flickerTimer;           // frames left in the current half of the flicker
    s16                      shadowShade;            // shade of the ground shadow, up to 0x80; negative draws none
    s16                      feinting;               // set while the appearance for a strike is a feint, which takes no damage
    s16                      targetYaw;              // angle `targetPos` was placed by (0..0xFFF): the player's heading for a grab, the bearing from the player for a strike
    s16                      feintBroken;            // set when the player touches or shoots the feint: it shrinks faster and the next attack follows at once
    s16                      tintRequest;            // pending tint of the model, cleared once applied (0 none, 1 dim blue, 2 full white)
    s16                      flickerStage;           // 0 not flickering, 1 a hit that makes the translucency flicker has landed, 2 the flinch from it is over
    s16                      counterattacking;       // set while the attack under way answers a broken feint
    s16                      downedPose;             // 0 standing, 1 fallen from a hit from behind, 2 from one in front; filed as the saved pose on death
    s16                      reactionLock;           // 0 none, 1 attacking: only a flicker hit makes it flinch, 2 falling: a hit starts no reaction on the ground
    s16                      grabBreak;              // why the hold is to end (0 no reason, 1 the player struggled free, 2 a weapon hit the golem)
    s16                      grabDamageTicks;        // times the hold has hurt the player; each raises the chance of the kill
    s16                      grabKillRollArmed;      // 0 until the first hold check that finds the player above the HP limit, which never rolls the kill
    s16                      regionCount;            // entries at `regions`
    s16                      beamScreenX[2];         // screen x of the aim beam's ends: [0] the far end, [1] the point on part 4
    s16                      beamScreenY[2];         // screen y of the same two points
    s16                      beamDepth[2];           // their ordering depths, a quarter of the screen z
    s16                      boxRegion;              // index at `regions` of the box the player was last found in
    s16                      interruptDamage;        // damage taken since an attack or hold opened; enough of it breaks that off
    s16                      attackCount;            // attacks started so far, up to `GOLEM_KNIGHT_BISHOP_IDLE_LIMIT`; each shortens the idle wait by a sixteenth
    s16                      lastAttack;             // `GOLEM_KNIGHT_BISHOP_SEQUENCE_` value of the last attack started (grab, strike or box approach; 0 before the first)
    s16                      repeatCount;            // times running the idle pick has repeated `lastAttack`; each makes the other attack likelier
    s16                      soundSet;               // the room's sound variant (0 none): picks the animation cue sounds and the sound file loaded at spawn
    s16                      knockdownStage;         // 0 upright, 1 on the frame a fall starts, 2 after it; from 2 the root is no longer pressed to the floor
    s16                      actorId;                // number of the actor package (0x16 Knight, 0x27 Bishop), as used in its collision keys
    s16                      grabStage;              // 0 not holding, 1 holding the player, 2 let go: `grabReleaseTimer` runs until the player is handed back
    s16                      grabReleaseTimer;       // frames since the hold ended: the cue plays at 0x14, and from 0x5F the player is freed once their animation is over
} GolemKnightBishopWork;
STATIC_ASSERT_SIZEOF(GolemKnightBishopWork, 0x71C);

/// Scratch-stack block the sequences that position the golem work in.
///
/// Target placement, the box approach and the strike each reserve one for the
/// length of the call. It pairs a full-width offset with a short vector whose
/// meaning is the caller's; nothing in it carries from one use to the next.
typedef struct {
    VECTOR  offset;  // offset along the world axes: `operand` after a GTE rotation, or the player's position less the golem's, of which only x and z are filled
    SVECTOR operand; // short vector being worked on: the local offset to rotate, a planar offset used as it stands, or the Euler angles (0x1000 to the turn) the root's rotation is built from
} GolemKnightBishopOffsetScratch;
STATIC_ASSERT_SIZEOF(GolemKnightBishopOffsetScratch, 0x18);

/// One span of the forward-speed curve of the charge that ends a box
/// approach.
///
/// The spans are in ascending `lastFrame` order. Each frame of the charge
/// moves at the `forwardSpeed` of the first span whose `lastFrame` is not
/// below `GolemKnightBishopWork::animFrame`. The search has no terminator, so
/// the final span has to reach past the frame the charge ends on.
typedef struct {
    s16 lastFrame;    // last animation frame the span covers
    u16 forwardSpeed; // `GolemKnightBishopWork::forwardSpeed` over the span: a signed distance per frame held as its raw halfword, which the charge reads without sign extension
} GolemKnightBishopChargeSpeedSpan;
STATIC_ASSERT_SIZEOF(GolemKnightBishopChargeSpeedSpan, 4);

/// Shared pose, clip, tint and placement values of the included GOLEM helpers.
enum {
    GOLEM_KNIGHT_BISHOP_DOWNED_BEHIND            = 1,
    GOLEM_KNIGHT_BISHOP_ANIM_DOWNED_HIT_BEHIND   = 0xE,
    GOLEM_KNIGHT_BISHOP_ANIM_DOWNED_HIT_FRONT    = 0x12,
    GOLEM_KNIGHT_BISHOP_DOWNED_HIT_BEHIND_FRAMES = 16,
    GOLEM_KNIGHT_BISHOP_DOWNED_HIT_FRONT_FRAMES  = 22,
    GOLEM_KNIGHT_BISHOP_TINT_WHITE               = 2,
    GOLEM_KNIGHT_BISHOP_GRAB_TARGET_DISTANCE     = 1450, // world units behind the player's root
};

/// Yaw units and the two stages of settling onto a downed collision body.
enum {
    GOLEM_KNIGHT_BISHOP_YAW_FULL_TURN = 4096,
    GOLEM_KNIGHT_BISHOP_YAW_HALF_TURN = 2048,
    GOLEM_KNIGHT_BISHOP_YAW_MASK      = 4095,
    GOLEM_KNIGHT_BISHOP_FALL_STARTED  = 1,
    GOLEM_KNIGHT_BISHOP_FALL_SETTLED  = 2,
};

/// Fall timings and collision placement shared by knockdown and standing death.
enum {
    GOLEM_KNIGHT_BISHOP_FALL_BEHIND_FRAMES       = 66,
    GOLEM_KNIGHT_BISHOP_FALL_FRONT_FRAMES        = 49,
    GOLEM_KNIGHT_BISHOP_FALL_BEHIND_IMPACT_FRAME = 44,
    GOLEM_KNIGHT_BISHOP_FALL_FRONT_IMPACT_FRAME  = 25,
    GOLEM_KNIGHT_BISHOP_HURT_RADIUS              = 350,
    GOLEM_KNIGHT_BISHOP_DOWNED_BEHIND_OFFSET_Z   = -167,
    GOLEM_KNIGHT_BISHOP_DOWNED_FRONT_OFFSET_Z    = 265,
};

/// Clip IDs and state values shared by the GOLEM sequences and appearance update.
enum {
    GOLEM_KNIGHT_BISHOP_ANIM_GRAB_START              = 1,
    GOLEM_KNIGHT_BISHOP_ANIM_GRAB_HOLD               = 2,
    GOLEM_KNIGHT_BISHOP_ANIM_GRAB_KILL               = 3,
    GOLEM_KNIGHT_BISHOP_ANIM_STAND                   = 4,
    GOLEM_KNIGHT_BISHOP_ANIM_STRIKE                  = 5,
    GOLEM_KNIGHT_BISHOP_ANIM_ADVANCE                 = 6,
    GOLEM_KNIGHT_BISHOP_ANIM_CHARGE                  = 7,
    GOLEM_KNIGHT_BISHOP_ANIM_LIGHT_FLINCH            = 8,
    GOLEM_KNIGHT_BISHOP_ANIM_HEAVY_FLINCH_FRONT      = 9,
    GOLEM_KNIGHT_BISHOP_ANIM_HEAVY_FLINCH_BEHIND     = 10,
    GOLEM_KNIGHT_BISHOP_ANIM_RECOVER                 = 11,
    GOLEM_KNIGHT_BISHOP_ANIM_GRAB_RELEASE            = 12,
    GOLEM_KNIGHT_BISHOP_ANIM_FALL_BEHIND             = 13,
    GOLEM_KNIGHT_BISHOP_ANIM_WRITHE_BEHIND           = 15,
    GOLEM_KNIGHT_BISHOP_ANIM_LIE_BEHIND              = 16,
    GOLEM_KNIGHT_BISHOP_ANIM_FALL_FRONT              = 17,
    GOLEM_KNIGHT_BISHOP_ANIM_WRITHE_FRONT            = 19,
    GOLEM_KNIGHT_BISHOP_ANIM_LIE_FRONT               = 20,
    GOLEM_KNIGHT_BISHOP_ANIM_GRAB_EARLY_RELEASE      = 21,
    GOLEM_KNIGHT_BISHOP_DOWNED_FRONT                 = 2,
    GOLEM_KNIGHT_BISHOP_REACTION_UNLOCKED            = 0,
    GOLEM_KNIGHT_BISHOP_REACTION_ATTACKING           = 1,
    GOLEM_KNIGHT_BISHOP_REACTION_FALLING             = 2,
    GOLEM_KNIGHT_BISHOP_FLICKER_NONE                 = 0,
    GOLEM_KNIGHT_BISHOP_FLICKER_HIT                  = 1,
    GOLEM_KNIGHT_BISHOP_FLICKER_RECOVERED            = 2,
    GOLEM_KNIGHT_BISHOP_GRAB_NONE                    = 0,
    GOLEM_KNIGHT_BISHOP_GRAB_HOLDING                 = 1,
    GOLEM_KNIGHT_BISHOP_GRAB_RELEASED                = 2,
    GOLEM_KNIGHT_BISHOP_GRAB_BREAK_NONE              = 0,
    GOLEM_KNIGHT_BISHOP_GRAB_BREAK_STRUGGLE          = 1,
    GOLEM_KNIGHT_BISHOP_TASK_ACTIVE                  = 1,
    GOLEM_KNIGHT_BISHOP_TASK_DEAD                    = 2,
    GOLEM_KNIGHT_BISHOP_STANDARD_TRANSLUCENCY_FRAMES = 20,
    GOLEM_KNIGHT_BISHOP_STANDARD_COLOR_BLEND_FRAMES  = 10,
    GOLEM_KNIGHT_BISHOP_TRANSLUCENCY_INVISIBLE       = 255,
    GOLEM_KNIGHT_BISHOP_SHADOW_FULL_SHADE            = 128,
    GOLEM_KNIGHT_BISHOP_SHADOW_HIDDEN                = -1,
    GOLEM_KNIGHT_BISHOP_SEQUENCE_START               = 0,
    GOLEM_KNIGHT_BISHOP_FRAME_CAPTURE_BIAS           = 12,
    GOLEM_KNIGHT_BISHOP_AIM_ROOT_Y                   = -1300,
    GOLEM_KNIGHT_BISHOP_AIM_RANGE                    = 10000,
    GOLEM_KNIGHT_BISHOP_SCALE_ONE                    = 4096,
};

/// Tests a player's world X/Z against a box region's four exclusive edges.
///
/// The caller checks the region kind. `region` is a GolemKnightBishopRegion
/// pointer and `playerWorld` a MATRIX pointer, both live for this expression.
/// Each argument may be evaluated up to four times in short-circuit order;
/// neither may have side effects. No surrounding identifiers are captured.
#define GOLEM_KNIGHT_BISHOP_PLAYER_INSIDE_BOX(region, playerWorld)                   \
    ((region)->minX < (playerWorld)->t[0] && (playerWorld)->t[0] < (region)->maxX && \
     (playerWorld)->t[2] < (region)->maxZ && (region)->minZ < (playerWorld)->t[2])

static void _golemKnightBishopPickHitReaction(Task* task, s32 damage);
static void _golemKnightBishopRegionScanSeq(Task* task);
static s32  _golemKnightBishopPlayerInBox(Task* task);
static void _golemKnightBishopPlaceTarget(Task* task);
static void _golemKnightBishopStrikeSeq(Task* task);
static void _golemKnightBishopUpdateAppearance(Task* task);
static void _golemKnightBishopLightFlinchSeq(Task* task);
static void _golemKnightBishopHeavyFlinchSeq(Task* task);
static void _golemKnightBishopKnockdownSeq(Task* task);
static void _golemKnightBishopDownedHitSeq(Task* task);
static void _golemKnightBishopCollapseDeathSeq(Task* task);
static void _golemKnightBishopPlayAnimCues(Task* task);
static void _golemKnightBishopDrawAimBeam(Task* task);
static void _golemKnightBishopDeadState(Enemy* enemy, Task* task);
static void _golemKnightBishopFrameState(Enemy* enemy, Task* task);
static void _golemKnightBishopRunSequence(Task* task);
static void _golemKnightBishopApplyScale(Task* task);
static void _golemKnightBishopDownedDeathSeq(Task* task);
static void _golemKnightBishopStepForward(Task* task);
static void _golemKnightBishopDrawShadow(Task* task);
static void _golemKnightBishopTickGrabRelease(Task* task);
static void _golemKnightBishopQueueFrameCapture(GfxCoord* coord, s32 depthBias);

/* Defined by each package. */
static void _golemKnightBishopTakeHits(Task* task);
static void _golemKnightBishopUpdateTint(Task* task);
static void _golemKnightBishopTickAnim(Task* task);
static void _golemKnightBishopSpawn(Enemy* enemy, Task* task);
static void _golemKnightBishopAimFromPart(Task* task);
static void _golemKnightBishopIdleSeq(Task* task);
static void _golemKnightBishopGrabSeq(Task* task);
static void _golemKnightBishopBoxApproachSeq(Task* task);
static void _golemKnightBishopRecoverSeq(Task* task);

static inline void _golemKnightBishopTickAnimInline(Task* task);
static inline void _golemKnightBishopDrawShadowInline(Task* task);

static inline void _golemKnightBishopUpdateTintInline(Task* task);

#endif /* SRC_SHARED_GOLEM_KNIGHT_BISHOP_H */
