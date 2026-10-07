#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/abs.h>
#include <psyq/inline_c.h>
#include <psyq/memory.h>

#include "common.h"
#include "gte.h"

#include "actors/actor.h"

#include "gameplay/actor.h"
#include "gameplay/actor_render.h"
#include "gameplay/animation.h"
#include "gameplay/area_entry.h"
#include "gameplay/areaplace.h"
#include "gameplay/attachment_state.h"
#include "gameplay/attachments.h"
#include "gameplay/collision.h"
#include "gameplay/damage.h"
#include "gameplay/effect_tasks.h"
#include "gameplay/effects.h"
#include "gameplay/enemy.h"
#include "gameplay/message.h"
#include "gameplay/pad_script.h"
#include "gameplay/enemy_params.h"
#include "gameplay/player_actor.h"
#include "gameplay/room_effects.h"
#include "gameplay/scene.h"
#include "gameplay/scene_combat.h"
#include "gameplay/scene_runtime.h"
#include "gameplay/world_collision.h"
#include "gameplay/world_coords.h"
#include "gameplay/world_targets.h"

#include "main/coord.h"
#include "main/display.h"
#include "main/display_types.h"
#include "main/random.h"
#include "main/gfx.h"
#include "main/gfx_types.h"
#include "main/gfxgte.h"
#include "main/mc.h"
#include "main/mc_types.h"
#include "main/mem.h"
#include "main/scratch.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/task_types.h"
#include "main/tmd.h"
#include "main/tmd_types.h"
#include "main/wipsys.h"
#include "main/wipsys_types.h"

#include "overlay.h"

/// Navigation sweep steps, in yaw units, degrees and world-coordinate reach.
enum {
    ACTOR_01600_PROBE_YAW_STEP    = 0x71,
    ACTOR_01600_PROBE_DEGREE_STEP = 10,
    ACTOR_01600_PROBE_LAST_DEGREE = 359,
    ACTOR_01600_PROBE_REACH       = 2000,
    ACTOR_01600_MIN_TURN_STEP     = 0x20,
};

/// Packed collision identity of this scavenger body.
enum { ACTOR_01600_BODY_KEY = WORLD_COLLISION_CONTACT_ENEMY_BODY | 0x10 };

/// Results of the clear-heading search; the direction bit selects a turn animation.
enum {
    ACTOR_01600_HEADING_PENDING    = 0,
    ACTOR_01600_HEADING_DIRECT     = 1,
    ACTOR_01600_HEADING_ARC        = 2,
    ACTOR_01600_HEADING_DIRECTION  = 0x80,
    ACTOR_01600_HEADING_BLOCKED    = 0xFF,
    ACTOR_01600_HEADING_BEST_UNSET = 0xFFFF,
    ACTOR_01600_WIDE_ARC           = ACTOR_TRANSFORM_ANGLE_HALF_TURN / 2,
    ACTOR_01600_REAR_ARC_MIN       = ACTOR_TRANSFORM_ANGLE_HALF_TURN - 0x80,
    ACTOR_01600_REAR_ARC_MAX       = ACTOR_TRANSFORM_ANGLE_HALF_TURN + 0x80,
};

/// Playback requests used when no clip should advance, or when a resting actor is hit.
enum {
    ACTOR_01600_ANIMATION_NONE     = 0xFF,
    ACTOR_01600_ANIMATION_REST     = 1,
    ACTOR_01600_ANIMATION_HIT_WAKE = 0xA,
    ACTOR_01600_ANIMATION_STAGGER  = 0xE,
};

/// Placement modes interpreted during setup; other modes wait for their numbered wave.
enum {
    ACTOR_01600_PLACEMENT_ACTIVE     = 0,
    ACTOR_01600_PLACEMENT_FIRST_WAVE = 1,
    ACTOR_01600_PLACEMENT_SCRIPTED   = 3,
};

/// Yaw stored in an end of a `_Actor01600ClearArc` that the sweep has not recorded yet.
///
/// No swept yaw can equal it: the sweep runs from 0 to 0xFE4.
#define ACTOR_01600_CLEAR_ARC_UNSET 0xFFFF

/// One arc of headings in which the scavenger's probe capsule stayed clear of the room's collision grid.
///
/// When the straight line to its target is blocked, the scavenger turns the
/// probe through a full circle in steps of 0x71 (ten degrees) and records each
/// unbroken run of steps at which the probe made no grid contact, then turns
/// toward the recorded arc nearest the target's bearing. Yaws are 4096 to the
/// turn, measured from the scavenger's own facing, and the sweep only counts
/// upward, so `startYaw <= endYaw` and no arc wraps past the full turn.
///
/// An arc is open while the probe stays clear and closes at the first blocked
/// step, or when the sweep completes. One that closes after a single clear
/// step has both ends equal. An arc opened on the sweep's final step is never
/// closed and is not counted.
typedef struct {
    s32 startYaw; // Yaw of the first clear step; `ACTOR_01600_CLEAR_ARC_UNSET` until the arc opens
    s32 endYaw;   // Yaw of the latest clear step; `ACTOR_01600_CLEAR_ARC_UNSET` until a second clear step or the arc's closing
} _Actor01600ClearArc;
STATIC_ASSERT_SIZEOF(_Actor01600ClearArc, 0x8);

/// The scavenger's own body in the world's collision lists: one sphere and the contact table it fills.
///
/// The sphere rides the model's root transform, its centre one radius above
/// the root, and carries a key of the category every enemy body uses. Its
/// table is also the enemy record's hit records. Each frame the table's
/// push-back moves the root, a contact of category 2 is a hit the scavenger
/// takes, a contact of category 3 is another enemy's body that pushes it away
/// in some animations, and the occupied entries are then cleared.
typedef struct {
    WorldCollisionBody    body;        // Sphere linked on list 2; floor-query, grid and pair tests are enabled after the link. Radius 0x190, widened to 0x258 over the first frames of an attack animation
    WorldCollisionContact contacts[8]; // Table `body` borrows, also installed as the enemy record's hit records; the last entry is marked LAST
} _Actor01600BodySphere;
STATIC_ASSERT_SIZEOF(_Actor01600BodySphere, 0xE0);

/// A capsule the scavenger senses with: the linked body, the shape that body borrows and the one-entry contact table that shape fills.
///
/// The body carries no key, so what it touches records no contact with it and
/// only its own table reports the touch. The scavenger keeps two: the cone it
/// notices a target with and the thin segment it tests headings with. Both
/// ride the model's root transform from a point 0x190 above the root and
/// reach from that point to `shape.ends[0]`; `shape.ends[1]` stays at the
/// origin. Positions and radii are world coordinate units.
///
/// A sensor stays linked from setup until dying begins or the task ends, and
/// is switched on and off through its body's grid and pair test flags. Its
/// reader empties the occupied entry once it has looked at it, so a result
/// lasts one read.
typedef struct {
    WorldCollisionBody    body;        // Capsule body whose context is `shape`; key and radius stay 0
    WorldCollisionCapsule shape;       // Segment and end radii, with `contacts` as its table
    WorldCollisionContact contacts[1]; // Single result, marked LAST; a key of 0 means nothing was touched
} _Actor01600CapsuleSensor;
STATIC_ASSERT_SIZEOF(_Actor01600CapsuleSensor, 0x50);

/// The sphere the scavenger's leaping bite hits with, and the one-entry contact table it fills.
///
/// It rides the model's root transform, centred 0x186 above the root, on a
/// list that is paired against the player's bodies. Its key is packed from
/// entry 1 of the scavenger's attack table, so a body it touches receives
/// that attack. The bites of a grab use entry 0 and are sent to the held
/// actor directly; they do not go through this sphere.
///
/// The body stays linked from setup until dying begins, and its pair test is
/// what arms it: enabled at frame 9 and disabled at frame 0x17 of a lunge
/// that is not an entrance, and off at every other time. An occupied entry
/// means the bite connected. Finding one disarms the sphere and empties the
/// table, as does a hit taken while airborne outside the roam behaviour;
/// running out of hit points disarms it.
typedef struct {
    WorldCollisionBody    body;        // Sphere of radius 0x12C linked on list 3; only its pair test is ever enabled, never its grid test
    WorldCollisionContact contacts[1]; // Table `body` borrows; the single entry is marked LAST
} _Actor01600BiteSphere;
STATIC_ASSERT_SIZEOF(_Actor01600BiteSphere, 0x38);

/// What the scavenger is doing, in `_Actor01600Work::behavior`.
enum {
    ACTOR_01600_BEHAVIOR_ROAM         = 0, // Unaware: rests, wakes to a noise, searches for a clear heading and hops along it
    ACTOR_01600_BEHAVIOR_ATTACK       = 1, // A target was noticed: runs `_Actor01600Work::attackAction`
    ACTOR_01600_BEHAVIOR_STAGGER      = 2, // Stagger reaction: thrown back, up again, and roaming after 0x5B frames
    ACTOR_01600_BEHAVIOR_BUILDUP      = 3, // Buildup reaction held in one animation until the enemy record's buildup ends
    ACTOR_01600_BEHAVIOR_POSED        = 4, // Holds one animation; forced on roaming and attacking scavengers by some rooms and placements
    ACTOR_01600_BEHAVIOR_BUILDUP_DOWN = 5  // Buildup reaction from an attack that knocks the scavenger down
};

/// The attack behaviour's current action, in `_Actor01600Work::attackAction`.
enum {
    ACTOR_01600_ACTION_ADVANCE       = 0, // Hop toward the target, then choose a turn, a grab or a lunge from its bearing and distance
    ACTOR_01600_ACTION_ZIGZAG        = 1, // The same approach, sidestepping one way and then the other
    ACTOR_01600_ACTION_TURN          = 2, // Turn by `turnRequest`
    ACTOR_01600_ACTION_TURN_WRAPPED  = 3, // The same turn, chosen when the way round the other side is shorter
    ACTOR_01600_ACTION_SETTLE        = 4, // One animation after an entrance leap, then advance
    ACTOR_01600_ACTION_LUNGE         = 5, // Leap at the target with the bite sphere armed
    ACTOR_01600_ACTION_LUNGE_RECOVER = 6, // The same leap followed by its landing animations; also the leap a waiting scavenger emerges with
    ACTOR_01600_ACTION_GRAB          = 7, // Close on the target, hold it and bite until it is released
    ACTOR_01600_ACTION_KNOCKBACK     = 8  // Thrown away from the target after a bite lands or a hit is taken mid-leap, then up again
};

/// How the facing yaw is steered, in `_Actor01600Work::turnMode`.
enum {
    ACTOR_01600_TURN_TRACK_TARGET  = 0, // Toward the nearer player actor, 0x20 a step
    ACTOR_01600_TURN_RANDOM_BEGIN  = 1, // Draw a random turn of 0x400..0x7FF either way; nothing selects this mode
    ACTOR_01600_TURN_RANDOM        = 2, // Spend that turn in random steps, then track the target
    ACTOR_01600_TURN_REQUEST_BEGIN = 5, // Take `turnRequest` as the turn to make
    ACTOR_01600_TURN_REQUEST       = 6, // Spend it at `turnRate` a step
    ACTOR_01600_TURN_HOLD          = 7  // The turn is finished and the yaw is left alone
};

/// Steps of the search for a heading the path probe finds clear, in `_Actor01600Work::pathSearchPhase`.
///
/// A phase of `ACTOR_01600_PATH_SEARCH_BEGIN_SWEEP` or later means the straight
/// line to the target was blocked.
enum {
    ACTOR_01600_PATH_SEARCH_PROBE_TARGET = 0, // Aim the probe at the target
    ACTOR_01600_PATH_SEARCH_CHECK_TARGET = 1, // Read that probe; a clear line ends the search with the turn to the target
    ACTOR_01600_PATH_SEARCH_BEGIN_SWEEP  = 2, // Reset the clear arcs and start turning the probe through a full circle
    ACTOR_01600_PATH_SEARCH_SWEEP        = 3, // One sweep step a frame
    ACTOR_01600_PATH_SEARCH_CHOOSE_ARC   = 4  // Take the turn to the recorded arc nearest the target's bearing
};

/// Phases of the lock-on anchor during a hop, in `_Actor01600Work::targetAnchorPhase`.
enum {
    ACTOR_01600_TARGET_ANCHOR_FOLLOW   = 0, // The enemy record publishes the root; the anchor copies the root's X and Z
    ACTOR_01600_TARGET_ANCHOR_DETACH   = 1, // The enemy record is pointed at the anchor
    ACTOR_01600_TARGET_ANCHOR_HOLD     = 2, // The anchor stays where it is for 8 frames
    ACTOR_01600_TARGET_ANCHOR_AIM      = 3, // The gap to the root is split into 5 steps
    ACTOR_01600_TARGET_ANCHOR_CATCH_UP = 4  // The anchor takes those steps, then follows again
};

/// Phases of dying, in `_Actor01600Work::deathPhase`.
enum {
    ACTOR_01600_DEATH_BEGIN    = 0,   // Unlink the bodies and the target entry and choose the falling animation
    ACTOR_01600_DEATH_COLLAPSE = 1,   // Slide and flatten for 0x3C frames
    ACTOR_01600_DEATH_REMOVE   = 2,   // Hide the model; the last scavenger of the final wave reports to the room task instead
    ACTOR_01600_DEATH_EXIT     = 3,   // Count `exitTimer` down, then end the task
    ACTOR_01600_DEATH_REPORTED = 0xFF // Reported to the room task; nothing further runs here
};

/// Work block of one scavenger: its animation, its collision bodies and the state its behaviours run on.
///
/// The task has three states: setup, the per-frame tick and dying. The tick
/// first holds a scavenger that was placed waiting (`active` clear) until its
/// wave's cue, then takes the hits and push-back `bodySphere` collected, runs
/// `behavior`, plays the requested animation and steps the root.
///
/// Yaws and turns use 4096 to the turn. Positions, radii and speeds are world
/// coordinate units and units per frame. Flags are 0 or 1 unless a comment
/// says otherwise. Animation ids index the package's animation-set table.
typedef struct {
    AnimationContext         anim;                                  // Playback context of the model's joints
    AnimationSlot            slots[9];                              // One playback slot per model part; only slots 1..8 are played
    u8                       poses[9][ANIMATION_POSE_BUFFER_BYTES]; // Encoded transition pose for the slot at the same index
    MATRIX                   colorMatrix;                           // Light-colour matrix the model borrows
    MATRIX                   lightMatrix;                           // Light-direction matrix the model borrows
    GfxCoord                 targetAnchor;                          // Unrotated world-space coordinate the enemy record publishes instead of the root during a hop; see `targetAnchorPhase`
    _Actor01600CapsuleSensor sight;                                 // Cone on list 3, radius 0x384 at its far end and 0x64 at the body, reaching 0x3E8 while resting and 0xFA0 once woken. A contact with a player body alerts the scavenger
    _Actor01600BodySphere    bodySphere;                            // The body that takes hits and push-back
    _Actor01600BiteSphere    bite;                                  // Attack sphere of the lunge
    EffectSpawnArg           hitEffect;                             // Argument record of the hit effects spawned on the model
    _Actor01600CapsuleSensor pathProbe;                             // Segment of radius 1 on list 2, turned to `probeYaw`; a room-grid contact means that heading is blocked. Its tests are enabled only while a path search runs
    _Actor01600ClearArc      clearArcs[8];                          // Arcs the path sweep found clear; the first `clearArcCount` are closed
    MATRIX                   deathMatrix;                           // Root transform when dying began; the collapse rebuilds the root from it each frame
    VECTOR3                  previousPosition;                      // Root position before the latest step, restored when contacts push the body in opposing directions
    byte                     field_4C8[4];                          // Never accessed; role unproven
    SVECTOR                  recoilRotation;                        // Rotation applied to model part 1 each frame. A hit sets `vx`, which then decays by 0x20 a frame; the other components stay 0
    Task*                    grabTarget;                            // Player-actor task being grabbed; set when a grab begins and left in place afterwards
    SVECTOR                  scriptedRotation;                      // Rotation a staged scavenger's animations turn the root or model part 1 with; `vy` starts at the placement's yaw
    s32                      turnRequest;                           // Signed turn chosen toward the target or toward a clear arc
    s32                      grabEndHeight;                         // Root height stored when a grab ends; nothing reads it
    s16                      clearArcCount;                         // Index of the arc the sweep is recording, and the number of arcs once it ends (0: no heading is clear)
    s16                      pathSearchPhase;                       // `ACTOR_01600_PATH_SEARCH_*`
    s16                      probeYaw;                              // Yaw of `pathProbe` from the scavenger's facing
    s16                      sweepDegrees;                          // Sweep progress in degrees: -10 at reset, complete past 359
    s16                      turnRate;                              // Yaw per step of a requested turn: a sixteenth of the turn, at least 0x20
    s16                      targetAnchorPhase;                     // `ACTOR_01600_TARGET_ANCHOR_*`
    u16                      targetAnchorTimer;                     // Frames spent in the anchor's hold or catch-up phase
    s16                      targetAnchorStepX;                     // X the anchor moves in each catch-up frame
    s16                      targetAnchorStepZ;                     // Z the anchor moves in each catch-up frame
    s16                      animBlendFrames;                       // Blend length given to the next animation change
    s16                      yaw;                                   // Facing yaw the root's rotation is rebuilt from whenever the scavenger turns
    s16                      behavior;                              // `ACTOR_01600_BEHAVIOR_*`
    s16                      dead;                                  // Hit points ran out; dying starts once the scavenger is not airborne
    s16                      deathPhase;                            // `ACTOR_01600_DEATH_*`
    s16                      stateTimer;                            // Frames counted by the current reaction or death phase; the roam behaviour wraps it at 0x1F without reading it
    s16                      animRequest;                           // Animation wanted on slots 1..8; 0xFF plays nothing
    s16                      animPlaying;                           // Animation last started. A request that differs restarts playback, so clearing this replays the request
    s16                      animFrame;                             // Frames since the animation started; behaviours also clear it to loop their own timing
    s16                      hopFrameOffset;                        // Added to the frame thresholds of a repeated roam hop: 0 for the first repeat, -3 after it
    s16                      forwardSpeed;                          // Distance the root moves along its facing each frame; negative backs away
    s16                      turnMode;                              // `ACTOR_01600_TURN_*`
    s16                      turnRemaining;                         // Signed turn left in the random and requested modes
    s16                      field_514;                             // Written only, with 0 or 1; role unproven
    s16                      attackAction;                          // `ACTOR_01600_ACTION_*`
    s16                      deathScaleY;                           // Vertical scale of the collapsing body, 4096 for 1.0, shrinking by 0x50 a frame until it is 0x200 or less
    s16                      alerted;                               // A target was noticed; the roam behaviour hands over to the attack behaviour
    s16                      hitCooldown;                           // Frames left in which further hits are ignored, taken from the attack that landed
    s16                      verticalSpeed;                         // Height the root moves each frame while airborne; negative rises
    s16                      jumpHeight;                            // Sum of `verticalSpeed` since take-off; the jump ends when it is no longer negative
    s16                      recoilActive;                          // `recoilRotation` is decaying
    s16                      scriptedDelay;                         // Frames before a staged scavenger starts `scriptedAnim`
    s16                      shadowUnderBody;                       // The ground shadow is drawn on the floor under model part 1 instead of under the root; set during a grab
    s16                      airborne;                              // A jump is in progress
    s16                      biteLanded;                            // The lunge's bite connected; the rest of that lunge no longer sets its own speed
    s16                      field_52C;                             // Written only, with 0; role unproven
    s16                      active;                                // The bodies are linked and the behaviours run; clear while a placed scavenger waits for its cue
    s16                      suspended;                             // The step leaves the root's height alone; set for a waiting scavenger and cleared when its leap takes off
    s16                      shadowHidden;                          // No ground shadow is drawn
    s16                      holdingTarget;                         // The grabbed actor is in its scripted state and has to be released from it
    s16                      reactionDelay;                         // Frames before a resting scavenger wakes to a noise, or a waiting one answers its cue
    s16                      animRate;                              // Playback rate set on each slot, in sixteenths: 0x10 normally, 0x14 for reactions and leaps
    s16                      noiseHeard;                            // A resting scavenger heard the player; `reactionDelay` is running
    s16                      entranceLeap;                          // The next lunge is an entrance: the bite stays unarmed and the scavenger settles afterwards
    s16                      grabTargetIndex;                       // Player-actor slot of `grabTarget` (0 player, 1 companion)
    s16                      burstState;                            // (0 intact, 2 blown apart by the killing hit, which skips the collapse)
    u16                      idleSoundTimer;                        // Frames until the next random idle sound; restarts at 0x14
    s16                      longLeap;                              // The emerging leap rises faster and travels further
    s16                      scriptedAnim;                          // Animation a staged scavenger runs once `scriptedDelay` is spent (7, then 9; 0 none)
    u16                      hopCount;                              // Roam hops made since the last path search; the third ends the run
    s16                      colorRefresh;                          // A staged scavenger's lighting colour needs updating
    u16                      grabBiteTimer;                         // Frames since the last bite of a grab; every 0x14th is a bite
    s16                      exitTimer;                             // Frames a dead scavenger stays hidden before its task ends
    s16                      colorTimer;                            // Frames since the lighting colour was last updated; it is refreshed every fifth
    byte                     field_552[2];                          // Never accessed; role unproven
    s16                      grabBiteCount;                         // 2 when a grab takes hold, one more per bite; 5 or more lets go. The notice that a player actor's hit points ran out sets 6
    s16                      buildupKnocksDown;                     // The attack that last started a buildup knocks the scavenger down
} _Actor01600Work;
STATIC_ASSERT_SIZEOF(_Actor01600Work, 0x558);

/// Scratch-stack block of the scavenger's contact pass: the correction that
/// pushes it out of the room's collision, then the offsets its hits and
/// overlaps are measured along.
///
/// The pass reserves one block and has the push-back of the body sphere's
/// contact records resolved into `delta`; the whole units of that correction
/// move the root. It then walks the same records, reusing `delta` for each. A
/// damaging contact takes the offset from the root to the attacking player,
/// whose length is the range the damage is worked out for. A contact with
/// another enemy's body takes the offset from that body's contact point to the
/// root; `normal` holds its direction, and `delta` then holds that direction
/// in the frame of the collision grid's coordinate, which the root is pushed
/// out along. Nothing clears the block when it is reserved and nothing in it
/// carries over to the next frame. The pass releases the block before it
/// returns, except where a hit leaves the scavenger without hit points: that
/// path returns with the block still reserved.
///
/// The pass touches neither run of bytes around the vectors, so what else the
/// block was laid out to hold is unproven.
typedef struct {
    byte                field_0[0x20];   // Reserved with the block and never accessed; role unproven
    WorldCollisionDelta delta;           // Correction resolved from the contact records, in signed 16.16 units; then a whole-unit offset to the attacker or from an overlapped body; last `normal` in the collision grid's frame
    VECTOR              normal;          // `delta` of an overlapped body, normalised: away from that body, 4096 = 1.0
    byte                field_40[0x8];   // Reserved with the block and never accessed; role unproven
    s32                 contributorMask; // Bitmask of the contact records that contributed to the correction, as the resolver reports it; never read
} _Actor01600ContactScratch;
STATIC_ASSERT_SIZEOF(_Actor01600ContactScratch, 0x4C);

/// The scratch-stack block the scavenger aims its path probe in: the probe's
/// length laid along the scavenger's facing, the turn about Y to the probe's
/// yaw, and the length after that turn.
///
/// One block serves one aiming and nothing carries over to the next: it is
/// reserved, the turned length is copied to the far end of the probe's
/// capsule, and it is released. Both vectors are offsets in the frame of the
/// model's root, which the probe rides, in world coordinate units.
typedef struct {
    SVECTOR reach;    // (0, 0, length) of the probe before the turn; `pad` is never written
    SVECTOR farEnd;   // `reach` turned by `rotation`. Its `vx` and `vz` become the capsule's far end; `vy` stays 0 and is not read, and `pad` is never written
    MATRIX  rotation; // Identity, written word-wise, then turned about Y by the probe's yaw, 4096 to the turn; its translation is never set or read
} _Actor01600PathProbeAimScratch;
STATIC_ASSERT_SIZEOF(_Actor01600PathProbeAimScratch, 0x30);

static s32 _actor01600SelectNearestPlayer(Task* actor);

extern GameActorButtonPressHold Actor01600_D12878;
extern ActorTransform           Actor01600_D12890;

extern s32 Actor01600_D12874;
extern s32 Actor01600_D12870;

/// Models effect 0x80005 spawns, set in `D_800626EC[5].data.model`.
static TmdSource _gActor01600ScavengerBurstHead;
static TmdSource _gActor01600ScavengerBurstLeg;
static TmdSource _gActor01600ScavengerBurstEar;
extern SVECTOR   Actor01600_D12868;

static void Actor01600_Fn070AC(Task* arg0, Task* arg1);

static void Actor01600_Fn01420(Task* arg0);
static void Actor01600_Fn04054(Enemy* arg0, Task* arg1);
void        Actor01600_Fn066E8(Task* arg0);

static s32 _actor01600MeasureTarget(Task* actor, s32* horizontalDistance);
static s32 _actor01600StepPathProbe(Task* actor, s32 reachDistance, s32 relativeYaw);

static u8 _actor01600SearchClearHeading(Task* actor);

/// Scratch-stack block the scavenger's ground shadow is placed in when it is
/// drawn under the root.
///
/// A scavenger on the ground has its shadow centred on the root's world
/// position. One in the air has its root above the floor, so the shadow is
/// centred on the root moved back down its own Y axis by the height of the
/// jump. One block serves one draw: it is reserved, `centre` is handed to the
/// ground quad's drawer, and it is released. The shadow of a scavenger that
/// has hold of its target is placed under the body instead, without a block.
typedef struct {
    VECTOR3 centre;     // World position the shadow quad is centred on, world coordinate units; for an airborne scavenger first `drop` along world axes, before the root's position is added
    byte    field_C[4]; // Reserved with the block and never accessed; role unproven
    SVECTOR drop;       // Offset from an airborne root down to the floor, in the root's frame: Y alone, the height risen plus 0x80; `pad` is never written
} _Actor01600GroundShadowScratch;
STATIC_ASSERT_SIZEOF(_Actor01600GroundShadowScratch, 0x18);

/// Scratch-stack block of the scavenger's sidestep, which moves the root
/// across its facing.
///
/// The step is laid along X and turned about Y by the yaw of the root's
/// facing, so it runs level and square to that heading whatever pitch or roll
/// the root carries, and the result is added to the root's translation. One
/// block serves one step and nothing carries over to the next: it is
/// reserved, used and released. The step is written into the block before
/// the cursor is moved down to cover it.
typedef struct {
    VECTOR  step;     // (distance, 0, 0) with the distance cut to 16 bits, then turned by `rotation`: the offset added to the root's translation, world units; `pad` is never written
    SVECTOR facing;   // The root's local Z axis, 4096 = 1.0; `vy` is not read and `pad` is never written
    MATRIX  rotation; // Identity, written word-wise, then turned about Y by `yaw`; its translation is never set or read
    s16     yaw;      // Yaw of `facing`, 4096 to the turn
} _Actor01600SidestepScratch;
STATIC_ASSERT_SIZEOF(_Actor01600SidestepScratch, 0x3C);

static void _actor01600StepTurn(Task* actor);

extern AnimationPlayRequest Actor01600_D127D8;

static s32  Actor01600_Fn047A0(Task* actor);
static s32  Actor01600_Fn04974(Task* actor, s32 angle, s32 distance, s32 flags);
static void Actor01600_Fn06974(Task* actor, s32 distance);
static s32  Actor01600_Fn06C1C(Task* actor);
static s32  Actor01600_Fn06C94(Task* actor, s32 angle, s32 distance);
static s32  Actor01600_Fn06D74(Task* actor, s32 angle, s32 distance);

extern EnemyParams   Actor01600_D09F0C;
extern AnimationSet* Actor01600_D127EC[31];
// Typed callback views for the task message dispatcher.

extern TaskMessageEntry Actor01600_D127A4[3];
static void             _actor01600InitPlacement(Task* actor);
static void             _actor01600Exit(Task* actor);

extern DamageAttack Actor01600_D09F04[2];

extern SVECTOR Actor01600_D09F1C[];
extern SVECTOR Actor01600_D09F3C[];

MATRIX* ScaleMatrix(MATRIX* m, VECTOR* v);
MATRIX* MulMatrix(MATRIX* m0, MATRIX* m1);

static void _actor01600Init(Enemy* enemy, Task* actor);
static void _actor01600LinkCollisionBodies(Task* actor);
static void Actor01600_Fn00674(Enemy* arg0, Task* arg1);
static void _actor01600ConsumeReactions(Task* actor);
static void Actor01600_Fn00BAC(Task* arg0);
static void _actor01600ApplyDamage(Task* actor, s32 damage);
static void Actor01600_Fn017BC(Task* arg0);
static void Actor01600_Fn020F8(Task* arg0);
static void _actor01600StepAnimation(Task* actor);
static void Actor01600_Fn03EEC(Task* arg0);
static void Actor01600_Fn04AD8(Task* arg0);
static s32  Actor01600_Fn05558(Task* arg0);
static void Actor01600_Fn05F80(Task* arg0);
static void Actor01600_Fn0646C(Task* arg0);
static void Actor01600_Fn06744(Task* arg0);
static void Actor01600_Fn06810(Enemy* arg0, Task* arg1);
static void Actor01600_Fn06880(Task* arg0);
static void Actor01600_Fn06A84(Task* arg0);
static void _actor01600ReleaseGrab(Task* actor);
static u8   Actor01600_Fn06F78(void);
static void _actor01600Remove(Task* actor, s32 skipBodyUnlink);

static AnimationSet _gActor01600Actor101600Animation0A1A8;
static AnimationSet _gActor01600Actor101600Animation0A4C0;
static AnimationSet _gActor01600Actor101600Animation0A610;
static AnimationSet _gActor01600Actor101600Animation0AA20;
static AnimationSet _gActor01600Actor101600Animation0AC58;
static AnimationSet _gActor01600Actor101600Animation0B03C;
static AnimationSet _gActor01600Actor101600Animation0B3DC;
static AnimationSet _gActor01600Actor101600Animation0B7C4;
static AnimationSet _gActor01600Actor101600Animation0BE80;
static AnimationSet _gActor01600Actor101600Animation0C370;
static AnimationSet _gActor01600Actor101600Animation0C6D8;
static AnimationSet _gActor01600Actor101600Animation0C898;
static AnimationSet _gActor01600Actor101600Animation0CA18;
static AnimationSet _gActor01600Actor101600Animation0CD10;
static AnimationSet _gActor01600Actor101600Animation0CFB0;
static AnimationSet _gActor01600Actor101600Animation0D0EC;
static AnimationSet _gActor01600Actor101600Animation0D584;
static AnimationSet _gActor01600Actor101600Animation0D7B4;
static AnimationSet _gActor01600Actor101600Animation0DCA4;
static AnimationSet _gActor01600Actor101600Animation0E168;
static AnimationSet _gActor01600Actor101600Animation0E5C8;
static AnimationSet _gActor01600Actor101600Animation0EA60;
static AnimationSet _gActor01600Actor101600Animation0EE58;
static AnimationSet _gActor01600Actor101600Animation0EFD4;
static AnimationSet _gActor01600Actor101600Animation0F394;
static AnimationSet _gActor01600Actor101600Animation0FB10;
static AnimationSet _gActor01600Actor101600Animation0FED4;
static AnimationSet _gActor01600Actor101600Animation10114;
static AnimationSet _gActor01600Actor101600Animation106BC;
static AnimationSet _gActor01600Actor101600Animation10B6C;
static AnimationSet _gActor01600Actor101600Animation11820;
static AnimationSet _gActor01600Actor101600Animation12074;
static AnimationSet _gActor01600Actor101600Animation1277C;
static s32          _actor01600HandleCommand(Task* actor, s32 messageId, const ActorCommand* request, s32 unusedArg);
static s32          _actor01600HandleReleaseHold(Task* actor, s32 messageId, s32 unusedFirstArg, s32 unusedSecondArg);
void                Actor01600_Fn066E8(Task*);

extern AnimationSet* Actor01600_D127C8[4];

static TmdBone _gActor01600ScavengerBodySkeleton[9] = {
#include "assets/scavenger_body_skeleton.inc"
};

static u32 _gActor01600ScavengerBodyPartVerts[9] = {
#include "assets/scavenger_body_partVerts.inc"
};

static SVECTOR _gActor01600ScavengerBodyVerts[132] = {
#include "assets/scavenger_body_verts.inc"
};

static SVECTOR _gActor01600ScavengerBodyNormals[133] = {
#include "assets/scavenger_body_normals.inc"
};

static u32 _gActor01600ScavengerBodyStream[1324] = {
#include "assets/scavenger_body_stream.inc"
};

static TmdSource _gActor01600ScavengerBody = {
    0,
    7240,
    1956,
    9,
    _gActor01600ScavengerBodyPartVerts,
    _gActor01600ScavengerBodyVerts,
    _gActor01600ScavengerBodyNormals,
    _gActor01600ScavengerBodySkeleton,
    _gActor01600ScavengerBodyStream,
};

static TmdBone _gActor01600ScavengerBurstHeadSkeleton[1] = {
#include "assets/scavenger_burst_head_skeleton.inc"
};

static u32 _gActor01600ScavengerBurstHeadPartVerts[1] = {
#include "assets/scavenger_burst_head_partVerts.inc"
};

static SVECTOR _gActor01600ScavengerBurstHeadVerts[41] = {
#include "assets/scavenger_burst_head_verts.inc"
};

static SVECTOR _gActor01600ScavengerBurstHeadNormals[41] = {
#include "assets/scavenger_burst_head_normals.inc"
};

static u32 _gActor01600ScavengerBurstHeadStream[315] = {
#include "assets/scavenger_burst_head_stream.inc"
};

static TmdSource _gActor01600ScavengerBurstHead = {
    0,
    2200,
    0,
    1,
    _gActor01600ScavengerBurstHeadPartVerts,
    _gActor01600ScavengerBurstHeadVerts,
    _gActor01600ScavengerBurstHeadNormals,
    _gActor01600ScavengerBurstHeadSkeleton,
    _gActor01600ScavengerBurstHeadStream,
};

static TmdBone _gActor01600ScavengerBurstLegSkeleton[1] = {
#include "assets/scavenger_burst_leg_skeleton.inc"
};

static u32 _gActor01600ScavengerBurstLegPartVerts[1] = {
#include "assets/scavenger_burst_leg_partVerts.inc"
};

static SVECTOR _gActor01600ScavengerBurstLegVerts[27] = {
#include "assets/scavenger_burst_leg_verts.inc"
};

static SVECTOR _gActor01600ScavengerBurstLegNormals[27] = {
#include "assets/scavenger_burst_leg_normals.inc"
};

static u32 _gActor01600ScavengerBurstLegStream[241] = {
#include "assets/scavenger_burst_leg_stream.inc"
};

static TmdSource _gActor01600ScavengerBurstLeg = {
    0,
    1580,
    0,
    1,
    _gActor01600ScavengerBurstLegPartVerts,
    _gActor01600ScavengerBurstLegVerts,
    _gActor01600ScavengerBurstLegNormals,
    _gActor01600ScavengerBurstLegSkeleton,
    _gActor01600ScavengerBurstLegStream,
};

static TmdBone _gActor01600ScavengerBurstEarSkeleton[1] = {
#include "assets/scavenger_burst_ear_skeleton.inc"
};

static u32 _gActor01600ScavengerBurstEarPartVerts[1] = {
#include "assets/scavenger_burst_ear_partVerts.inc"
};

static SVECTOR _gActor01600ScavengerBurstEarVerts[8] = {
#include "assets/scavenger_burst_ear_verts.inc"
};

static SVECTOR _gActor01600ScavengerBurstEarNormals[8] = {
#include "assets/scavenger_burst_ear_normals.inc"
};

static u32 _gActor01600ScavengerBurstEarStream[70] = {
#include "assets/scavenger_burst_ear_stream.inc"
};

static TmdSource _gActor01600ScavengerBurstEar = {
    0,
    396,
    0,
    1,
    _gActor01600ScavengerBurstEarPartVerts,
    _gActor01600ScavengerBurstEarVerts,
    _gActor01600ScavengerBurstEarNormals,
    _gActor01600ScavengerBurstEarSkeleton,
    _gActor01600ScavengerBurstEarStream,
};

DamageAttack Actor01600_D09F04[2] = {
    { 12, 7 },
    { 14, 7 },
};

EnemyParams Actor01600_D09F0C = { Actor01600_D09F04, 85, 10, 62, 2, 100, 20, 100, 0 };

SVECTOR Actor01600_D09F1C[4] = {
    { 5632, 0, 3040, 0 },
    { 3168, 0, 1408, 0 },
    { 992, 0, 2880, 0 },
    { -2688, 0, 1408, 0 },
};

SVECTOR Actor01600_D09F3C[5] = {
    { 0x32E0, 0, 832, 0 },
    { 9632, 0, 2432, 0 },
    { 6592, 0, 864, 0 },
    { 3616, 0, 2523, 0 },
    { 2176, 0, 861, 0 },
};

static AnimationPackedPose _gActor01600Actor101600Animation0A1A8Bank1[6] = {
#include "assets/actor_101600_animation_0A1A8_bank1.inc"
};

static AnimationPackedRotation _gActor01600Actor101600Animation0A1A8Bank4[46] = {
#include "assets/actor_101600_animation_0A1A8_bank4.inc"
};

static AnimationRecord _gActor01600Actor101600Animation0A1A8Records[76] = {
#include "assets/actor_101600_animation_0A1A8_records.inc"
};

static u16 _gActor01600Actor101600Animation0A1A8Indices[10] = {
#include "assets/actor_101600_animation_0A1A8_indices.inc"
};

static AnimationSet _gActor01600Actor101600Animation0A1A8 = {
    _gActor01600Actor101600Animation0A1A8Records,
    _gActor01600Actor101600Animation0A1A8Indices,
    { NULL, _gActor01600Actor101600Animation0A1A8Bank1, NULL, NULL, _gActor01600Actor101600Animation0A1A8Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor01600Actor101600Animation0A4C0Bank1[11] = {
#include "assets/actor_101600_animation_0A4C0_bank1.inc"
};

static AnimationPackedRotation _gActor01600Actor101600Animation0A4C0Bank4[61] = {
#include "assets/actor_101600_animation_0A4C0_bank4.inc"
};

static AnimationRecord _gActor01600Actor101600Animation0A4C0Records[89] = {
#include "assets/actor_101600_animation_0A4C0_records.inc"
};

static u16 _gActor01600Actor101600Animation0A4C0Indices[10] = {
#include "assets/actor_101600_animation_0A4C0_indices.inc"
};

static AnimationSet _gActor01600Actor101600Animation0A4C0 = {
    _gActor01600Actor101600Animation0A4C0Records,
    _gActor01600Actor101600Animation0A4C0Indices,
    { NULL, _gActor01600Actor101600Animation0A4C0Bank1, NULL, NULL, _gActor01600Actor101600Animation0A4C0Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor01600Actor101600Animation0A610Bank1[4] = {
#include "assets/actor_101600_animation_0A610_bank1.inc"
};

static AnimationPackedRotation _gActor01600Actor101600Animation0A610Bank4[14] = {
#include "assets/actor_101600_animation_0A610_bank4.inc"
};

static AnimationRecord _gActor01600Actor101600Animation0A610Records[43] = {
#include "assets/actor_101600_animation_0A610_records.inc"
};

static u16 _gActor01600Actor101600Animation0A610Indices[10] = {
#include "assets/actor_101600_animation_0A610_indices.inc"
};

static AnimationSet _gActor01600Actor101600Animation0A610 = {
    _gActor01600Actor101600Animation0A610Records,
    _gActor01600Actor101600Animation0A610Indices,
    { NULL, _gActor01600Actor101600Animation0A610Bank1, NULL, NULL, _gActor01600Actor101600Animation0A610Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor01600Actor101600Animation0AA20Bank1[23] = {
#include "assets/actor_101600_animation_0AA20_bank1.inc"
};

static AnimationPackedRotation _gActor01600Actor101600Animation0AA20Bank4[70] = {
#include "assets/actor_101600_animation_0AA20_bank4.inc"
};

static AnimationRecord _gActor01600Actor101600Animation0AA20Records[106] = {
#include "assets/actor_101600_animation_0AA20_records.inc"
};

static u16 _gActor01600Actor101600Animation0AA20Indices[10] = {
#include "assets/actor_101600_animation_0AA20_indices.inc"
};

static AnimationSet _gActor01600Actor101600Animation0AA20 = {
    _gActor01600Actor101600Animation0AA20Records,
    _gActor01600Actor101600Animation0AA20Indices,
    { NULL, _gActor01600Actor101600Animation0AA20Bank1, NULL, NULL, _gActor01600Actor101600Animation0AA20Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor01600Actor101600Animation0AC58Bank1[8] = {
#include "assets/actor_101600_animation_0AC58_bank1.inc"
};

static AnimationPackedRotation _gActor01600Actor101600Animation0AC58Bank4[41] = {
#include "assets/actor_101600_animation_0AC58_bank4.inc"
};

static AnimationRecord _gActor01600Actor101600Animation0AC58Records[62] = {
#include "assets/actor_101600_animation_0AC58_records.inc"
};

static u16 _gActor01600Actor101600Animation0AC58Indices[10] = {
#include "assets/actor_101600_animation_0AC58_indices.inc"
};

static AnimationSet _gActor01600Actor101600Animation0AC58 = {
    _gActor01600Actor101600Animation0AC58Records,
    _gActor01600Actor101600Animation0AC58Indices,
    { NULL, _gActor01600Actor101600Animation0AC58Bank1, NULL, NULL, _gActor01600Actor101600Animation0AC58Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor01600Actor101600Animation0B03CBank1[14] = {
#include "assets/actor_101600_animation_0B03C_bank1.inc"
};

static AnimationPackedRotation _gActor01600Actor101600Animation0B03CBank4[79] = {
#include "assets/actor_101600_animation_0B03C_bank4.inc"
};

static AnimationRecord _gActor01600Actor101600Animation0B03CRecords[113] = {
#include "assets/actor_101600_animation_0B03C_records.inc"
};

static u16 _gActor01600Actor101600Animation0B03CIndices[10] = {
#include "assets/actor_101600_animation_0B03C_indices.inc"
};

static AnimationSet _gActor01600Actor101600Animation0B03C = {
    _gActor01600Actor101600Animation0B03CRecords,
    _gActor01600Actor101600Animation0B03CIndices,
    { NULL, _gActor01600Actor101600Animation0B03CBank1, NULL, NULL, _gActor01600Actor101600Animation0B03CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor01600Actor101600Animation0B3DCBank1[13] = {
#include "assets/actor_101600_animation_0B3DC_bank1.inc"
};

static AnimationPackedRotation _gActor01600Actor101600Animation0B3DCBank4[72] = {
#include "assets/actor_101600_animation_0B3DC_bank4.inc"
};

static AnimationRecord _gActor01600Actor101600Animation0B3DCRecords[106] = {
#include "assets/actor_101600_animation_0B3DC_records.inc"
};

static u16 _gActor01600Actor101600Animation0B3DCIndices[10] = {
#include "assets/actor_101600_animation_0B3DC_indices.inc"
};

static AnimationSet _gActor01600Actor101600Animation0B3DC = {
    _gActor01600Actor101600Animation0B3DCRecords,
    _gActor01600Actor101600Animation0B3DCIndices,
    { NULL, _gActor01600Actor101600Animation0B3DCBank1, NULL, NULL, _gActor01600Actor101600Animation0B3DCBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor01600Actor101600Animation0B7C4Bank1[14] = {
#include "assets/actor_101600_animation_0B7C4_bank1.inc"
};

static AnimationPackedRotation _gActor01600Actor101600Animation0B7C4Bank4[79] = {
#include "assets/actor_101600_animation_0B7C4_bank4.inc"
};

static AnimationRecord _gActor01600Actor101600Animation0B7C4Records[114] = {
#include "assets/actor_101600_animation_0B7C4_records.inc"
};

static u16 _gActor01600Actor101600Animation0B7C4Indices[10] = {
#include "assets/actor_101600_animation_0B7C4_indices.inc"
};

static AnimationSet _gActor01600Actor101600Animation0B7C4 = {
    _gActor01600Actor101600Animation0B7C4Records,
    _gActor01600Actor101600Animation0B7C4Indices,
    { NULL, _gActor01600Actor101600Animation0B7C4Bank1, NULL, NULL, _gActor01600Actor101600Animation0B7C4Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor01600Actor101600Animation0BE80Bank1[24] = {
#include "assets/actor_101600_animation_0BE80_bank1.inc"
};

static AnimationPackedRotation _gActor01600Actor101600Animation0BE80Bank4[139] = {
#include "assets/actor_101600_animation_0BE80_bank4.inc"
};

static AnimationRecord _gActor01600Actor101600Animation0BE80Records[205] = {
#include "assets/actor_101600_animation_0BE80_records.inc"
};

static u16 _gActor01600Actor101600Animation0BE80Indices[10] = {
#include "assets/actor_101600_animation_0BE80_indices.inc"
};

static AnimationSet _gActor01600Actor101600Animation0BE80 = {
    _gActor01600Actor101600Animation0BE80Records,
    _gActor01600Actor101600Animation0BE80Indices,
    { NULL, _gActor01600Actor101600Animation0BE80Bank1, NULL, NULL, _gActor01600Actor101600Animation0BE80Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor01600Actor101600Animation0C370Bank1[19] = {
#include "assets/actor_101600_animation_0C370_bank1.inc"
};

static AnimationPackedRotation _gActor01600Actor101600Animation0C370Bank4[100] = {
#include "assets/actor_101600_animation_0C370_bank4.inc"
};

static AnimationRecord _gActor01600Actor101600Animation0C370Records[144] = {
#include "assets/actor_101600_animation_0C370_records.inc"
};

static u16 _gActor01600Actor101600Animation0C370Indices[10] = {
#include "assets/actor_101600_animation_0C370_indices.inc"
};

static AnimationSet _gActor01600Actor101600Animation0C370 = {
    _gActor01600Actor101600Animation0C370Records,
    _gActor01600Actor101600Animation0C370Indices,
    { NULL, _gActor01600Actor101600Animation0C370Bank1, NULL, NULL, _gActor01600Actor101600Animation0C370Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor01600Actor101600Animation0C6D8Bank1[13] = {
#include "assets/actor_101600_animation_0C6D8_bank1.inc"
};

static AnimationPackedRotation _gActor01600Actor101600Animation0C6D8Bank4[68] = {
#include "assets/actor_101600_animation_0C6D8_bank4.inc"
};

static AnimationRecord _gActor01600Actor101600Animation0C6D8Records[96] = {
#include "assets/actor_101600_animation_0C6D8_records.inc"
};

static u16 _gActor01600Actor101600Animation0C6D8Indices[10] = {
#include "assets/actor_101600_animation_0C6D8_indices.inc"
};

static AnimationSet _gActor01600Actor101600Animation0C6D8 = {
    _gActor01600Actor101600Animation0C6D8Records,
    _gActor01600Actor101600Animation0C6D8Indices,
    { NULL, _gActor01600Actor101600Animation0C6D8Bank1, NULL, NULL, _gActor01600Actor101600Animation0C6D8Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor01600Actor101600Animation0C898Bank1[5] = {
#include "assets/actor_101600_animation_0C898_bank1.inc"
};

static AnimationPackedRotation _gActor01600Actor101600Animation0C898Bank4[25] = {
#include "assets/actor_101600_animation_0C898_bank4.inc"
};

static AnimationRecord _gActor01600Actor101600Animation0C898Records[57] = {
#include "assets/actor_101600_animation_0C898_records.inc"
};

static u16 _gActor01600Actor101600Animation0C898Indices[10] = {
#include "assets/actor_101600_animation_0C898_indices.inc"
};

static AnimationSet _gActor01600Actor101600Animation0C898 = {
    _gActor01600Actor101600Animation0C898Records,
    _gActor01600Actor101600Animation0C898Indices,
    { NULL, _gActor01600Actor101600Animation0C898Bank1, NULL, NULL, _gActor01600Actor101600Animation0C898Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor01600Actor101600Animation0CA18Bank1[5] = {
#include "assets/actor_101600_animation_0CA18_bank1.inc"
};

static AnimationPackedRotation _gActor01600Actor101600Animation0CA18Bank4[25] = {
#include "assets/actor_101600_animation_0CA18_bank4.inc"
};

static AnimationRecord _gActor01600Actor101600Animation0CA18Records[41] = {
#include "assets/actor_101600_animation_0CA18_records.inc"
};

static u16 _gActor01600Actor101600Animation0CA18Indices[10] = {
#include "assets/actor_101600_animation_0CA18_indices.inc"
};

static AnimationSet _gActor01600Actor101600Animation0CA18 = {
    _gActor01600Actor101600Animation0CA18Records,
    _gActor01600Actor101600Animation0CA18Indices,
    { NULL, _gActor01600Actor101600Animation0CA18Bank1, NULL, NULL, _gActor01600Actor101600Animation0CA18Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor01600Actor101600Animation0CD10Bank1[11] = {
#include "assets/actor_101600_animation_0CD10_bank1.inc"
};

static AnimationPackedRotation _gActor01600Actor101600Animation0CD10Bank4[59] = {
#include "assets/actor_101600_animation_0CD10_bank4.inc"
};

static AnimationRecord _gActor01600Actor101600Animation0CD10Records[83] = {
#include "assets/actor_101600_animation_0CD10_records.inc"
};

static u16 _gActor01600Actor101600Animation0CD10Indices[10] = {
#include "assets/actor_101600_animation_0CD10_indices.inc"
};

static AnimationSet _gActor01600Actor101600Animation0CD10 = {
    _gActor01600Actor101600Animation0CD10Records,
    _gActor01600Actor101600Animation0CD10Indices,
    { NULL, _gActor01600Actor101600Animation0CD10Bank1, NULL, NULL, _gActor01600Actor101600Animation0CD10Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor01600Actor101600Animation0CFB0Bank1[5] = {
#include "assets/actor_101600_animation_0CFB0_bank1.inc"
};

static AnimationPackedRotation _gActor01600Actor101600Animation0CFB0Bank4[28] = {
#include "assets/actor_101600_animation_0CFB0_bank4.inc"
};

static AnimationRecord _gActor01600Actor101600Animation0CFB0Records[110] = {
#include "assets/actor_101600_animation_0CFB0_records.inc"
};

static u16 _gActor01600Actor101600Animation0CFB0Indices[10] = {
#include "assets/actor_101600_animation_0CFB0_indices.inc"
};

static AnimationSet _gActor01600Actor101600Animation0CFB0 = {
    _gActor01600Actor101600Animation0CFB0Records,
    _gActor01600Actor101600Animation0CFB0Indices,
    { NULL, _gActor01600Actor101600Animation0CFB0Bank1, NULL, NULL, _gActor01600Actor101600Animation0CFB0Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor01600Actor101600Animation0D0ECBank1[4] = {
#include "assets/actor_101600_animation_0D0EC_bank1.inc"
};

static AnimationPackedRotation _gActor01600Actor101600Animation0D0ECBank4[18] = {
#include "assets/actor_101600_animation_0D0EC_bank4.inc"
};

static AnimationRecord _gActor01600Actor101600Animation0D0ECRecords[34] = {
#include "assets/actor_101600_animation_0D0EC_records.inc"
};

static u16 _gActor01600Actor101600Animation0D0ECIndices[10] = {
#include "assets/actor_101600_animation_0D0EC_indices.inc"
};

static AnimationSet _gActor01600Actor101600Animation0D0EC = {
    _gActor01600Actor101600Animation0D0ECRecords,
    _gActor01600Actor101600Animation0D0ECIndices,
    { NULL, _gActor01600Actor101600Animation0D0ECBank1, NULL, NULL, _gActor01600Actor101600Animation0D0ECBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor01600Actor101600Animation0D584Bank1[18] = {
#include "assets/actor_101600_animation_0D584_bank1.inc"
};

static AnimationPackedRotation _gActor01600Actor101600Animation0D584Bank4[90] = {
#include "assets/actor_101600_animation_0D584_bank4.inc"
};

static AnimationRecord _gActor01600Actor101600Animation0D584Records[135] = {
#include "assets/actor_101600_animation_0D584_records.inc"
};

static u16 _gActor01600Actor101600Animation0D584Indices[10] = {
#include "assets/actor_101600_animation_0D584_indices.inc"
};

static AnimationSet _gActor01600Actor101600Animation0D584 = {
    _gActor01600Actor101600Animation0D584Records,
    _gActor01600Actor101600Animation0D584Indices,
    { NULL, _gActor01600Actor101600Animation0D584Bank1, NULL, NULL, _gActor01600Actor101600Animation0D584Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor01600Actor101600Animation0D7B4Bank1[8] = {
#include "assets/actor_101600_animation_0D7B4_bank1.inc"
};

static AnimationPackedRotation _gActor01600Actor101600Animation0D7B4Bank4[40] = {
#include "assets/actor_101600_animation_0D7B4_bank4.inc"
};

static AnimationRecord _gActor01600Actor101600Animation0D7B4Records[61] = {
#include "assets/actor_101600_animation_0D7B4_records.inc"
};

static u16 _gActor01600Actor101600Animation0D7B4Indices[10] = {
#include "assets/actor_101600_animation_0D7B4_indices.inc"
};

static AnimationSet _gActor01600Actor101600Animation0D7B4 = {
    _gActor01600Actor101600Animation0D7B4Records,
    _gActor01600Actor101600Animation0D7B4Indices,
    { NULL, _gActor01600Actor101600Animation0D7B4Bank1, NULL, NULL, _gActor01600Actor101600Animation0D7B4Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor01600Actor101600Animation0DCA4Bank1[18] = {
#include "assets/actor_101600_animation_0DCA4_bank1.inc"
};

static AnimationPackedRotation _gActor01600Actor101600Animation0DCA4Bank4[98] = {
#include "assets/actor_101600_animation_0DCA4_bank4.inc"
};

static AnimationRecord _gActor01600Actor101600Animation0DCA4Records[149] = {
#include "assets/actor_101600_animation_0DCA4_records.inc"
};

static u16 _gActor01600Actor101600Animation0DCA4Indices[10] = {
#include "assets/actor_101600_animation_0DCA4_indices.inc"
};

static AnimationSet _gActor01600Actor101600Animation0DCA4 = {
    _gActor01600Actor101600Animation0DCA4Records,
    _gActor01600Actor101600Animation0DCA4Indices,
    { NULL, _gActor01600Actor101600Animation0DCA4Bank1, NULL, NULL, _gActor01600Actor101600Animation0DCA4Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor01600Actor101600Animation0E168Bank1[17] = {
#include "assets/actor_101600_animation_0E168_bank1.inc"
};

static AnimationPackedRotation _gActor01600Actor101600Animation0E168Bank4[96] = {
#include "assets/actor_101600_animation_0E168_bank4.inc"
};

static AnimationRecord _gActor01600Actor101600Animation0E168Records[143] = {
#include "assets/actor_101600_animation_0E168_records.inc"
};

static u16 _gActor01600Actor101600Animation0E168Indices[10] = {
#include "assets/actor_101600_animation_0E168_indices.inc"
};

static AnimationSet _gActor01600Actor101600Animation0E168 = {
    _gActor01600Actor101600Animation0E168Records,
    _gActor01600Actor101600Animation0E168Indices,
    { NULL, _gActor01600Actor101600Animation0E168Bank1, NULL, NULL, _gActor01600Actor101600Animation0E168Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor01600Actor101600Animation0E5C8Bank1[17] = {
#include "assets/actor_101600_animation_0E5C8_bank1.inc"
};

static AnimationPackedRotation _gActor01600Actor101600Animation0E5C8Bank4[90] = {
#include "assets/actor_101600_animation_0E5C8_bank4.inc"
};

static AnimationRecord _gActor01600Actor101600Animation0E5C8Records[124] = {
#include "assets/actor_101600_animation_0E5C8_records.inc"
};

static u16 _gActor01600Actor101600Animation0E5C8Indices[10] = {
#include "assets/actor_101600_animation_0E5C8_indices.inc"
};

static AnimationSet _gActor01600Actor101600Animation0E5C8 = {
    _gActor01600Actor101600Animation0E5C8Records,
    _gActor01600Actor101600Animation0E5C8Indices,
    { NULL, _gActor01600Actor101600Animation0E5C8Bank1, NULL, NULL, _gActor01600Actor101600Animation0E5C8Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor01600Actor101600Animation0EA60Bank1[18] = {
#include "assets/actor_101600_animation_0EA60_bank1.inc"
};

static AnimationPackedRotation _gActor01600Actor101600Animation0EA60Bank4[95] = {
#include "assets/actor_101600_animation_0EA60_bank4.inc"
};

static AnimationRecord _gActor01600Actor101600Animation0EA60Records[130] = {
#include "assets/actor_101600_animation_0EA60_records.inc"
};

static u16 _gActor01600Actor101600Animation0EA60Indices[10] = {
#include "assets/actor_101600_animation_0EA60_indices.inc"
};

static AnimationSet _gActor01600Actor101600Animation0EA60 = {
    _gActor01600Actor101600Animation0EA60Records,
    _gActor01600Actor101600Animation0EA60Indices,
    { NULL, _gActor01600Actor101600Animation0EA60Bank1, NULL, NULL, _gActor01600Actor101600Animation0EA60Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor01600Actor101600Animation0EE58Bank1[22] = {
#include "assets/actor_101600_animation_0EE58_bank1.inc"
};

static AnimationPackedRotation _gActor01600Actor101600Animation0EE58Bank4[66] = {
#include "assets/actor_101600_animation_0EE58_bank4.inc"
};

static AnimationRecord _gActor01600Actor101600Animation0EE58Records[107] = {
#include "assets/actor_101600_animation_0EE58_records.inc"
};

static u16 _gActor01600Actor101600Animation0EE58Indices[10] = {
#include "assets/actor_101600_animation_0EE58_indices.inc"
};

static AnimationSet _gActor01600Actor101600Animation0EE58 = {
    _gActor01600Actor101600Animation0EE58Records,
    _gActor01600Actor101600Animation0EE58Indices,
    { NULL, _gActor01600Actor101600Animation0EE58Bank1, NULL, NULL, _gActor01600Actor101600Animation0EE58Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor01600Actor101600Animation0EFD4Bank1[5] = {
#include "assets/actor_101600_animation_0EFD4_bank1.inc"
};

static AnimationPackedRotation _gActor01600Actor101600Animation0EFD4Bank4[24] = {
#include "assets/actor_101600_animation_0EFD4_bank4.inc"
};

static AnimationRecord _gActor01600Actor101600Animation0EFD4Records[41] = {
#include "assets/actor_101600_animation_0EFD4_records.inc"
};

static u16 _gActor01600Actor101600Animation0EFD4Indices[10] = {
#include "assets/actor_101600_animation_0EFD4_indices.inc"
};

static AnimationSet _gActor01600Actor101600Animation0EFD4 = {
    _gActor01600Actor101600Animation0EFD4Records,
    _gActor01600Actor101600Animation0EFD4Indices,
    { NULL, _gActor01600Actor101600Animation0EFD4Bank1, NULL, NULL, _gActor01600Actor101600Animation0EFD4Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor01600Actor101600Animation0F394Bank1[13] = {
#include "assets/actor_101600_animation_0F394_bank1.inc"
};

static AnimationPackedRotation _gActor01600Actor101600Animation0F394Bank4[70] = {
#include "assets/actor_101600_animation_0F394_bank4.inc"
};

static AnimationRecord _gActor01600Actor101600Animation0F394Records[116] = {
#include "assets/actor_101600_animation_0F394_records.inc"
};

static u16 _gActor01600Actor101600Animation0F394Indices[10] = {
#include "assets/actor_101600_animation_0F394_indices.inc"
};

static AnimationSet _gActor01600Actor101600Animation0F394 = {
    _gActor01600Actor101600Animation0F394Records,
    _gActor01600Actor101600Animation0F394Indices,
    { NULL, _gActor01600Actor101600Animation0F394Bank1, NULL, NULL, _gActor01600Actor101600Animation0F394Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor01600Actor101600Animation0FB10Bank1[42] = {
#include "assets/actor_101600_animation_0FB10_bank1.inc"
};

static AnimationPackedRotation _gActor01600Actor101600Animation0FB10Bank4[139] = {
#include "assets/actor_101600_animation_0FB10_bank4.inc"
};

static AnimationRecord _gActor01600Actor101600Animation0FB10Records[199] = {
#include "assets/actor_101600_animation_0FB10_records.inc"
};

static u16 _gActor01600Actor101600Animation0FB10Indices[10] = {
#include "assets/actor_101600_animation_0FB10_indices.inc"
};

static AnimationSet _gActor01600Actor101600Animation0FB10 = {
    _gActor01600Actor101600Animation0FB10Records,
    _gActor01600Actor101600Animation0FB10Indices,
    { NULL, _gActor01600Actor101600Animation0FB10Bank1, NULL, NULL, _gActor01600Actor101600Animation0FB10Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor01600Actor101600Animation0FED4Bank1[16] = {
#include "assets/actor_101600_animation_0FED4_bank1.inc"
};

static AnimationPackedRotation _gActor01600Actor101600Animation0FED4Bank4[75] = {
#include "assets/actor_101600_animation_0FED4_bank4.inc"
};

static AnimationRecord _gActor01600Actor101600Animation0FED4Records[103] = {
#include "assets/actor_101600_animation_0FED4_records.inc"
};

static u16 _gActor01600Actor101600Animation0FED4Indices[10] = {
#include "assets/actor_101600_animation_0FED4_indices.inc"
};

static AnimationSet _gActor01600Actor101600Animation0FED4 = {
    _gActor01600Actor101600Animation0FED4Records,
    _gActor01600Actor101600Animation0FED4Indices,
    { NULL, _gActor01600Actor101600Animation0FED4Bank1, NULL, NULL, _gActor01600Actor101600Animation0FED4Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor01600Actor101600Animation10114Bank1[8] = {
#include "assets/actor_101600_animation_10114_bank1.inc"
};

static AnimationPackedRotation _gActor01600Actor101600Animation10114Bank4[41] = {
#include "assets/actor_101600_animation_10114_bank4.inc"
};

static AnimationRecord _gActor01600Actor101600Animation10114Records[64] = {
#include "assets/actor_101600_animation_10114_records.inc"
};

static u16 _gActor01600Actor101600Animation10114Indices[10] = {
#include "assets/actor_101600_animation_10114_indices.inc"
};

static AnimationSet _gActor01600Actor101600Animation10114 = {
    _gActor01600Actor101600Animation10114Records,
    _gActor01600Actor101600Animation10114Indices,
    { NULL, _gActor01600Actor101600Animation10114Bank1, NULL, NULL, _gActor01600Actor101600Animation10114Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor01600Actor101600Animation106BCBank1[21] = {
#include "assets/actor_101600_animation_106BC_bank1.inc"
};

static AnimationPackedRotation _gActor01600Actor101600Animation106BCBank4[120] = {
#include "assets/actor_101600_animation_106BC_bank4.inc"
};

static AnimationRecord _gActor01600Actor101600Animation106BCRecords[164] = {
#include "assets/actor_101600_animation_106BC_records.inc"
};

static u16 _gActor01600Actor101600Animation106BCIndices[10] = {
#include "assets/actor_101600_animation_106BC_indices.inc"
};

static AnimationSet _gActor01600Actor101600Animation106BC = {
    _gActor01600Actor101600Animation106BCRecords,
    _gActor01600Actor101600Animation106BCIndices,
    { NULL, _gActor01600Actor101600Animation106BCBank1, NULL, NULL, _gActor01600Actor101600Animation106BCBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor01600Actor101600Animation10B6CBank1[24] = {
#include "assets/actor_101600_animation_10B6C_bank1.inc"
};

static AnimationPackedRotation _gActor01600Actor101600Animation10B6CBank4[87] = {
#include "assets/actor_101600_animation_10B6C_bank4.inc"
};

static AnimationRecord _gActor01600Actor101600Animation10B6CRecords[126] = {
#include "assets/actor_101600_animation_10B6C_records.inc"
};

static u16 _gActor01600Actor101600Animation10B6CIndices[10] = {
#include "assets/actor_101600_animation_10B6C_indices.inc"
};

static AnimationSet _gActor01600Actor101600Animation10B6C = {
    _gActor01600Actor101600Animation10B6CRecords,
    _gActor01600Actor101600Animation10B6CIndices,
    { NULL, _gActor01600Actor101600Animation10B6CBank1, NULL, NULL, _gActor01600Actor101600Animation10B6CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor01600Actor101600Animation11820Bank1[24] = {
#include "assets/actor_101600_animation_11820_bank1.inc"
};

static AnimationPackedRotation _gActor01600Actor101600Animation11820Bank4[299] = {
#include "assets/actor_101600_animation_11820_bank4.inc"
};

static AnimationRecord _gActor01600Actor101600Animation11820Records[422] = {
#include "assets/actor_101600_animation_11820_records.inc"
};

static u16 _gActor01600Actor101600Animation11820Indices[20] = {
#include "assets/actor_101600_animation_11820_indices.inc"
};

static AnimationSet _gActor01600Actor101600Animation11820 = {
    _gActor01600Actor101600Animation11820Records,
    _gActor01600Actor101600Animation11820Indices,
    { NULL, _gActor01600Actor101600Animation11820Bank1, NULL, NULL, _gActor01600Actor101600Animation11820Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor01600Actor101600Animation12074Bank1[14] = {
#include "assets/actor_101600_animation_12074_bank1.inc"
};

static AnimationPackedRotation _gActor01600Actor101600Animation12074Bank4[198] = {
#include "assets/actor_101600_animation_12074_bank4.inc"
};

static AnimationRecord _gActor01600Actor101600Animation12074Records[273] = {
#include "assets/actor_101600_animation_12074_records.inc"
};

static u16 _gActor01600Actor101600Animation12074Indices[20] = {
#include "assets/actor_101600_animation_12074_indices.inc"
};

static AnimationSet _gActor01600Actor101600Animation12074 = {
    _gActor01600Actor101600Animation12074Records,
    _gActor01600Actor101600Animation12074Indices,
    { NULL, _gActor01600Actor101600Animation12074Bank1, NULL, NULL, _gActor01600Actor101600Animation12074Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor01600Actor101600Animation1277CBank1[12] = {
#include "assets/actor_101600_animation_1277C_bank1.inc"
};

static AnimationPackedRotation _gActor01600Actor101600Animation1277CBank4[178] = {
#include "assets/actor_101600_animation_1277C_bank4.inc"
};

static AnimationRecord _gActor01600Actor101600Animation1277CRecords[216] = {
#include "assets/actor_101600_animation_1277C_records.inc"
};

static u16 _gActor01600Actor101600Animation1277CIndices[20] = {
#include "assets/actor_101600_animation_1277C_indices.inc"
};

static AnimationSet _gActor01600Actor101600Animation1277C = {
    _gActor01600Actor101600Animation1277CRecords,
    _gActor01600Actor101600Animation1277CIndices,
    { NULL, _gActor01600Actor101600Animation1277CBank1, NULL, NULL, _gActor01600Actor101600Animation1277CBank4, NULL, NULL, NULL },
};

TaskMessageEntry Actor01600_D127A4[3] = {
    { ACTOR_COMMAND_MESSAGE_APPLY, _actor01600HandleCommand },
    { ACTOR_MESSAGE_RELEASE_HOLD, _actor01600HandleReleaseHold },
    { TASK_MESSAGE_TABLE_END, NULL },
};

TaskDesc Actor01600_D127BC = { { { (TASK_BODY_TMD | TASK_DESC_SKIP_AUTO_MODEL_BUFFER), 96 } }, Actor01600_Fn066E8, { .model = &_gActor01600ScavengerBody } };

AnimationSet* Actor01600_D127C8[4] = {
    NULL,
    &_gActor01600Actor101600Animation11820,
    &_gActor01600Actor101600Animation12074,
    &_gActor01600Actor101600Animation1277C,
};

AnimationPlayRequest Actor01600_D127D8 = { { .sets = Actor01600_D127C8 }, 0, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationSet* Actor01600_D127EC[31] = {
    NULL,
    &_gActor01600Actor101600Animation0A1A8,
    &_gActor01600Actor101600Animation0A4C0,
    &_gActor01600Actor101600Animation0A610,
    &_gActor01600Actor101600Animation0AA20,
    &_gActor01600Actor101600Animation0AC58,
    &_gActor01600Actor101600Animation0B03C,
    &_gActor01600Actor101600Animation0B3DC,
    &_gActor01600Actor101600Animation0B7C4,
    &_gActor01600Actor101600Animation0BE80,
    &_gActor01600Actor101600Animation0C370,
    &_gActor01600Actor101600Animation0C6D8,
    &_gActor01600Actor101600Animation0C898,
    &_gActor01600Actor101600Animation0CA18,
    &_gActor01600Actor101600Animation0CD10,
    &_gActor01600Actor101600Animation0CFB0,
    &_gActor01600Actor101600Animation0D0EC,
    &_gActor01600Actor101600Animation0D584,
    &_gActor01600Actor101600Animation0D7B4,
    &_gActor01600Actor101600Animation0DCA4,
    &_gActor01600Actor101600Animation0E168,
    &_gActor01600Actor101600Animation0E5C8,
    &_gActor01600Actor101600Animation0EA60,
    &_gActor01600Actor101600Animation0EE58,
    &_gActor01600Actor101600Animation0EFD4,
    &_gActor01600Actor101600Animation0F394,
    &_gActor01600Actor101600Animation0FB10,
    &_gActor01600Actor101600Animation0FED4,
    &_gActor01600Actor101600Animation10114,
    &_gActor01600Actor101600Animation106BC,
    &_gActor01600Actor101600Animation10B6C,
};

SVECTOR Actor01600_D12868 = { 0, -100, 0, 0 };

s32 Actor01600_D12870 = 0;

s32 Actor01600_D12874 = 0;

/// Hold sent as `GAME_ACTOR_MESSAGE_AWAIT_BUTTON_PRESSES`. The static press
/// count is 1; the grab overwrites it with 5 and selects the animation the
/// fallback companion handler plays.
GameActorButtonPressHold Actor01600_D12878 = { { { .sets = Actor01600_D127C8 }, 0, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE }, 1 };

ActorTransform Actor01600_D12890;

static __inline__ void update_actor_color(Enemy* ctx, GfxCoord* attach);

/// Takes a 0x10-byte `VECTOR` from the scratch stack, fills it with `attach`'s
/// world position and hands it to `worldCoordUpdateActorColor`. Inlined so the
/// scratch-head address is rematerialised on every access.
static __inline__ void update_actor_color(Enemy* ctx, GfxCoord* attach)
{
    u8*     head;
    VECTOR* block;

    head  = SCRATCH_STACK_CURSOR(u8);
    block = (VECTOR*)(head - 0x10);

    SCRATCH_STACK_CURSOR(VECTOR) = block;

    block->vx = attach->workm.t[0];
    block->vy = attach->workm.t[1];
    block->vz = attach->workm.t[2];
    worldCoordUpdateActorColor(ctx, block, 0, 0);

    SCRATCH_STACK_CURSOR(u8) = SCRATCH_STACK_CURSOR(u8) + 0x10;
}

static const EnemyTaskFuncTable3 Actor01600_D00004 = {
    { _actor01600Init, Actor01600_Fn00674, Actor01600_Fn04054 },
};

/// Creates the scavenger's work, animation and target entry, then applies its placement mode.
///
/// The task owns the primary-heap work block and borrows this overlay's assets.
/// Allocation failure destroys the enemy; success advances to the live task state.
static void _actor01600Init(Enemy* enemy, Task* actor)
{
    SVECTOR          facingAxis;
    GfxCoord*        bodyCoord;
    s32              partIndex;
    u32              randomState;
    GfxCoord*        rootCoord;
    TmdObject*       model;
    _Actor01600Work* work;

    model     = actor->extra.tmd;
    rootCoord = model->coords;
    work      = memCalloc(sizeof(_Actor01600Work), false);
    bodyCoord = rootCoord + 1;
    if (work == NULL) {
        enemyDestroy(enemy, actor);
        return;
    }
    // Publish the owned work and the borrowed lighting and targeting storage.
    actor->work               = work;
    model->flags              = 0;
    rootCoord->composeStamp   = GRAPHICS_COORD_DIRTY;
    model->lightMtx           = &work->lightMatrix;
    model->colorMtx           = &work->colorMatrix;
    work->targetAnchor.parent = &gGfxViewCoord;
    gfxSetRotIdentity(&work->targetAnchor.coord);
    work->targetAnchor.coord.t[0]   = rootCoord->coord.t[0];
    work->targetAnchor.coord.t[1]   = rootCoord->coord.t[1];
    work->targetAnchor.coord.t[2]   = rootCoord->coord.t[2];
    work->targetAnchor.composeStamp = GRAPHICS_COORD_DIRTY;
    work->targetAnchorPhase         = ACTOR_01600_TARGET_ANCHOR_FOLLOW;
    enemy->field_4                  = &rootCoord->coord;
    enemy->field_48                 = 0;
    worldTargetLinkNode(&enemy->node);
    enemy->bodyPos.vy             = -0x190;
    enemy->node.state.parts.flags = 0;
    enemy->coord                  = rootCoord;
    enemy->bodyPos.vx             = 0;
    enemy->bodyPos.vz             = 0;
    enemy->param                  = &Actor01600_D09F0C;
    enemy->recs                   = work->bodySphere.contacts;
    enemy->hp                     = (u16)Actor01600_D09F0C.hpMax;
    work->hitEffect.spawnArgLo    = 0x280;
    work->hitEffect.spawnArgHi    = 2;
    work->hitEffect.coord         = bodyCoord;
    animationInitContext(&work->anim, Actor01600_D127EC, model, work->poses, work->slots);
    for (partIndex = 1; partIndex < ARRAY_SIZE(work->slots); partIndex++) {
        animationResetSlot(&work->anim, partIndex, 1);
    }
    sceneAcquireBattleRef(0);
    work->animRequest     = 1;
    work->animPlaying     = 1;
    work->idleSoundTimer  = 0x14;
    work->pathSearchPhase = ACTOR_01600_PATH_SEARCH_PROBE_TARGET;
    work->biteLanded      = 0;
    work->field_52C       = 0;
    work->animBlendFrames = 0;
    work->hopFrameOffset  = 0;
    work->shadowUnderBody = 0;
    work->airborne        = 0;
    work->verticalSpeed   = 0;
    work->jumpHeight      = 0;
    work->turnMode        = ACTOR_01600_TURN_TRACK_TARGET;
    work->suspended       = 0;
    work->shadowHidden    = 0;
    work->holdingTarget   = 0;
    work->noiseHeard      = 0;
    work->entranceLeap    = 0;
    work->burstState      = 0;
    work->grabTargetIndex = 0;
    work->scriptedDelay   = 0;
    work->longLeap        = 0;
    work->animRate        = ANIMATION_RATE_ONE;
    randomState           = (gRandomLcgState * RANDOM_LCG_MULTIPLIER) + RANDOM_LCG_INCREMENT;
    work->reactionDelay   = (s16)(((randomState >> 0x10) & 0x1F) + 1);
    gRandomLcgState       = randomState;
    gfxReadMatrixZAxis(&actor->extra.tmd->coords->coord, &facingAxis);
    work->yaw = ratan2(facingAxis.vx, facingAxis.vz);
    _actor01600InitPlacement(actor);
    actor->exitCallback = &_actor01600Exit;
    actor->msgTable     = Actor01600_D127A4;
    actor->state        = actor->state + 1;
}

/// Links the sight capsule with the work's single owned contact.
///
/// Requires an unlinked sensor and live root; tests are enabled by the next body link.
static __inline__ void _actor01600LinkSight(_Actor01600Work* work, GfxCoord* rootCoord, WorldCollisionContact* sightContacts)
{
    work->sight.shape.ends[0].vz     = 0xFA0;
    work->sight.shape.end0Radius     = 0x384;
    work->sight.shape.end1Radius     = 0x64;
    work->sight.shape.contacts       = sightContacts;
    work->sight.body.context.capsule = &work->sight.shape;
    work->sight.body.pos.vx          = 0;
    work->sight.body.pos.vy          = -0x190;
    work->sight.body.pos.vz          = 0;
    work->sight.body.key             = 0;
    work->sight.body.radius          = 0;
    work->sight.body.flags           = WORLD_COLLISION_BODY_CAPSULE;
    work->sight.body.coord           = rootCoord;
    worldCollisionLinkBody(WORLD_COLLISION_LIST_ENEMY_ATTACKS, &work->sight.body);
    worldCollisionInitContacts(sightContacts, ARRAY_SIZE(work->sight.contacts), 0);
}

/// Links the body sphere and enables the sight sensor prepared immediately before it.
///
/// The work owns the eight contacts; `bodyContacts` points to their first entry.
/// The body must be unlinked and the root coordinate live throughout its link.
static __inline__ void _actor01600LinkBodySphere(_Actor01600Work* work, GfxCoord* rootCoord, WorldCollisionContact* bodyContacts)
{
    work->bodySphere.body.coord            = rootCoord;
    work->bodySphere.body.context.contacts = bodyContacts;
    work->bodySphere.body.key              = ACTOR_01600_BODY_KEY;
    work->bodySphere.body.radius           = 0x190;
    work->bodySphere.body.pos.vx           = 0;
    work->bodySphere.body.pos.vy           = -0x190;
    work->bodySphere.body.pos.vz           = 0;
    work->bodySphere.body.flags            = WORLD_COLLISION_BODY_SPHERE;
    work->sight.body.flags                |= (WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED);
    worldCollisionLinkBody(WORLD_COLLISION_LIST_ENEMY_BODIES, &work->bodySphere.body);
    worldCollisionInitContacts(bodyContacts, ARRAY_SIZE(work->bodySphere.contacts), 0);
}

/// Links the path-probe capsule and enables the previously linked body sphere.
///
/// Requires an unlinked sensor and its single owned contact. The next bite link
/// disables this probe until a heading search arms it.
static __inline__ void _actor01600LinkPathProbe(_Actor01600Work* work, GfxCoord* rootCoord, WorldCollisionContact* pathProbeContacts)
{
    work->pathProbe.shape.ends[0].vz     = 0x1F4;
    work->pathProbe.shape.ends[0].vx     = 0x1F4;
    work->pathProbe.shape.end0Radius     = 1;
    work->pathProbe.shape.end1Radius     = 1;
    work->pathProbe.shape.contacts       = pathProbeContacts;
    work->pathProbe.body.coord           = rootCoord;
    work->pathProbe.body.context.capsule = &work->pathProbe.shape;
    work->pathProbe.body.pos.vx          = 0;
    work->pathProbe.body.pos.vy          = -0x190;
    work->pathProbe.body.pos.vz          = 0;
    work->pathProbe.body.key             = 0;
    work->pathProbe.body.radius          = 0;
    work->pathProbe.body.flags           = WORLD_COLLISION_BODY_CAPSULE;
    work->bodySphere.body.flags         |= (WORLD_COLLISION_BODY_FLOOR_QUERY | WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED);
    worldCollisionLinkBody(WORLD_COLLISION_LIST_ENEMY_BODIES, &work->pathProbe.body);
    worldCollisionInitContacts(pathProbeContacts, ARRAY_SIZE(work->pathProbe.contacts), 0);
}

/// Links the bite sphere with its pair test off and disarms the prepared path probe.
///
/// The work owns the single contact addressed by `biteContacts`. The body must
/// be unlinked and its root coordinate live throughout the link.
static __inline__ void _actor01600LinkBiteSphere(_Actor01600Work* work, GfxCoord* rootCoord, WorldCollisionContact* biteContacts)
{
    work->bite.body.coord            = rootCoord;
    work->bite.body.context.contacts = biteContacts;
    work->bite.body.pos.vx           = 0;
    work->bite.body.pos.vy           = -0x186;
    work->bite.body.pos.vz           = 0;
    work->pathProbe.body.flags      &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ (WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED));
    work->bite.body.key              = damagePackAttackKey(Actor01600_D09F04, 1);
    work->bite.body.radius           = 0x12C;
    work->bite.body.flags            = WORLD_COLLISION_BODY_SPHERE;
    worldCollisionLinkBody(WORLD_COLLISION_LIST_ENEMY_ATTACKS, &work->bite.body);
    worldCollisionInitContacts(biteContacts, ARRAY_SIZE(work->bite.contacts), 0);
    work->bite.body.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
}

/// Links the scavenger's sight, body, path probe and bite with their owned contact tables.
///
/// The four bodies must be unlinked. Sight and body tests start enabled; the path
/// probe and bite start disabled. Their coordinate and work storage must remain
/// live until unlinking. Centres and radii use root-local world coordinate units.
static void _actor01600LinkCollisionBodies(Task* actor)
{
    _Actor01600Work* work;
    GfxCoord*        rootCoord;

    work      = actor->work;
    rootCoord = actor->extra.tmd->coords;
    // Configure the broad sight capsule before its tests are enabled by the body link.
    _actor01600LinkSight(work, rootCoord, work->sight.contacts);
    _actor01600LinkBodySphere(work, rootCoord, work->bodySphere.contacts);
    // The thin probe starts disabled; a heading search arms it between frames.
    _actor01600LinkPathProbe(work, rootCoord, work->pathProbe.contacts);
    _actor01600LinkBiteSphere(work, rootCoord, work->bite.contacts);
}

static void Actor01600_Fn00674(Enemy* arg0, Task* arg1)
{
    _Actor01600Work* work;
    GfxCoord*        coord;
    TmdObject*       obj;
    s32              id;
    s32              stageAreaKey;
    u16              count;

    work  = arg1->work;
    coord = arg1->extra.tmd->coords;
    if (!(Actor01600_Fn05558(arg1) & 0xFF)) {
        switch (gSceneCombatState.actorControl) {
            case SCENE_COMBAT_ACTORS_RUNNING:
                arg1->extra.tmd->flags       = 0;
                arg0->node.state.parts.flags = 0;
                break;
            case SCENE_COMBAT_ACTORS_PAUSED:
                Actor01600_Fn06810(arg0, arg1);
                Actor01600_Fn03EEC(arg1);
                return;
            case SCENE_COMBAT_ACTORS_HIDDEN:
                obj                          = arg1->extra.tmd;
                obj->flags                  |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
                arg0->node.state.parts.flags = WORLD_TARGET_NOT_LOCKABLE;
                return;
            default:
                break;
        }
        Actor01600_Fn04AD8(arg1);
        if (arg0->reactionFlags != 0) {
            _actor01600ConsumeReactions(arg1);
        }
        Actor01600_Fn00BAC(arg1);
        if (work->airborne == 0) {
            if (work->dead != 0) {
                arg1->state = 2;
            }
        }
        stageAreaKey = GAME_LOCATION_WORD(gGameSession->location.loc) & GAME_LOCATION_STAGE_AREA_MASK;
        if (stageAreaKey != GAME_LOCATION_KEY(3, 38, 0, 0) && stageAreaKey != GAME_LOCATION_KEY(4, 7, 0, 0) && stageAreaKey != GAME_LOCATION_KEY(4, 1, 0, 0)) {
            if (coord->coord.t[1] >= 0x65) {
                coord->coord.t[1] = -0xA;
            }
        }
        if (((GAME_LOCATION_WORD(gGameSession->location.loc) & GAME_LOCATION_STAGE_AREA_MASK) == GAME_LOCATION_KEY(3, 29, 0, 0)) && (coord->coord.t[1] >= -0x3E7)) {
            id = (((u16)((Enemy*)arg1->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x40100005;
            sndEvtRequestScriptStart(id, (s8)worldCoordGetOriginAudioPan(coord), (s8)worldCoordGetOriginAudioDepth(coord));
            id = (((u16)((Enemy*)arg1->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x4010000A;
            sndEvtRequestScriptStart(id, (s8)worldCoordGetOriginAudioPan(coord), (s8)worldCoordGetOriginAudioDepth(coord));
            _actor01600ReleaseGrab(arg1);
            _actor01600Remove(arg1, 0);
        }
        if ((GAME_LOCATION_WORD(gGameSession->location.loc) & GAME_LOCATION_STAGE_AREA_MASK) == GAME_LOCATION_KEY(4, 1, 0, 0)) {
            if (coord->coord.t[1] > 0) {
                work->shadowHidden = 1;
            }
            if (coord->coord.t[1] >= 0x3E9) {
                id = (((u16)((Enemy*)arg1->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x40100005;
                sndEvtRequestScriptStart(id, (s8)worldCoordGetOriginAudioPan(coord), (s8)worldCoordGetOriginAudioDepth(coord));
                id = (((u16)((Enemy*)arg1->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x4010000A;
                sndEvtRequestScriptStart(id, (s8)worldCoordGetOriginAudioPan(coord), (s8)worldCoordGetOriginAudioDepth(coord));
                _actor01600ReleaseGrab(arg1);
                _actor01600Remove(arg1, 0);
            }
        }
        Actor01600_Fn01420(arg1);
        _actor01600StepAnimation(arg1);
        Actor01600_Fn06A84(arg1);
        coord->composeStamp = GRAPHICS_COORD_DIRTY;
        actorRenderComposeCoord(coord);
        count            = work->colorTimer + 1;
        work->colorTimer = count;
        if (((s16)count >= 5) || (gGameSession->viewDirty == 1)) {
            work->colorTimer = 0;
            Actor01600_Fn06810(arg0, arg1);
        }
        if (work->shadowHidden == 0) {
            Actor01600_Fn03EEC(arg1);
        }
    }
}

/// Consumes grounded stagger, buildup and damage-over-time reactions.
///
/// Airborne reactions remain pending. A grounded damage-over-time reaction
/// releases a held player before its damage pulse and clears only on expiry.
static void _actor01600ConsumeReactions(Task* actor)
{
    Enemy*           enemy;
    _Actor01600Work* work;
    s16              behavior;
    s32              damage;
    u8               remainingReactions;
    u8               pendingReactions;

    enemy            = actor->spawnArg2.pointer;
    pendingReactions = enemy->reactionFlags;
    work             = actor->work;
    if ((pendingReactions & ENEMY_REACTION_STAGGER) && (work->airborne == 0)) {
        enemy->reactionFlags = pendingReactions & ENEMY_REACTION_STAGGER_CLEAR;
        work->behavior       = ACTOR_01600_BEHAVIOR_STAGGER;
        work->animRate       = 0x14;
        work->stateTimer     = 0;
        work->animRequest    = 0xE;
    }
    remainingReactions = enemy->reactionFlags;
    if ((remainingReactions & ENEMY_REACTION_BUILDUP) && (work->behavior != ACTOR_01600_BEHAVIOR_STAGGER) && (work->airborne == 0)) {
        enemy->reactionFlags = remainingReactions & ENEMY_REACTION_BUILDUP_CLEAR;
        work->behavior       = ACTOR_01600_BEHAVIOR_BUILDUP;
        if (work->buildupKnocksDown != 0) {
            work->animRequest = ACTOR_01600_ANIMATION_STAGGER;
            work->behavior    = ACTOR_01600_BEHAVIOR_BUILDUP_DOWN;
        }
        work->stateTimer = 0;
    }
    if ((enemy->reactionFlags & ENEMY_REACTION_DAMAGE_OVER_TIME_BITS) && (work->airborne == 0)) {
        _actor01600ReleaseGrab(actor);
        damage = damageTickEnemyDamageOverTime(enemy);
        if (damage != 0) {
            behavior = work->behavior;
            if ((behavior != ACTOR_01600_BEHAVIOR_STAGGER) && (behavior != ACTOR_01600_BEHAVIOR_BUILDUP_DOWN)) {
                work->behavior    = ACTOR_01600_BEHAVIOR_ROAM;
                work->animRequest = 0xA;
            }
            _actor01600ApplyDamage(actor, damage);
        }
        if (damageIsEnemyDamageOverTimeExpired(enemy) != 0) {
            enemy->reactionFlags &= ENEMY_REACTION_DAMAGE_OVER_TIME_CLEAR;
        }
    }
}

static void Actor01600_Fn00BAC(Task* actor)
{
    s32                        distance;
    Task**                     slots;
    void*                      world;
    _Actor01600Work*           work;
    Enemy*                     ctx;
    GfxCoord*                  coord;
    s32                        contactIndex;
    _Actor01600ContactScratch* scratch;
    GfxCoord*                  other;
    s32                        x, y, z;
    s32                        damage;
    s32                        amount;
    s32                        product;
    s32                        push;
    s32                        clamped;
    s32                        cx, cz;
    s16                        count;
    s32                        mode;
    /* Keep the comparison state local to each reaction branch (GCC 2.8.1). */
    s32 ignoredState;
    work    = actor->work;
    scratch = SCRATCH_STACK_RESERVE_BLOCK(_Actor01600ContactScratch);
    ctx     = actor->spawnArg2.pointer;
    coord   = actor->extra.tmd->coords;
    mode    = worldCollisionResolvePushback(work->bodySphere.contacts, &scratch->delta, ARRAY_SIZE(work->bodySphere.contacts), &scratch->contributorMask);
    world   = coord + 1;
    switch (mode) {
        case 1:
            coord->coord.t[0] += scratch->delta.fixed.vx.halves.integer;
            coord->coord.t[1] += scratch->delta.fixed.vy.halves.integer;
            coord->coord.t[2] += scratch->delta.fixed.vz.halves.integer;
            break;
        case 2:
            coord->coord.t[0] = work->previousPosition.vx;
            coord->coord.t[1] = work->previousPosition.vy;
            coord->coord.t[2] = work->previousPosition.vz;
            break;
        case 0:
        default:
            break;
    }
    slots = gPlayerActorTasks;
    if (work->hitCooldown != 0) {
        if (--work->hitCooldown <= 0)
            work->hitCooldown = 0;
    }
    /* Take the hits and the enemy push-outs the body sphere collected this frame. */
    for (contactIndex = 0; contactIndex < ARRAY_SIZE(work->bodySphere.contacts); contactIndex++) {
        switch (work->bodySphere.contacts[contactIndex].key.parts.kind) {
            case 2:
                if (work->hitCooldown == 0) {
                    other                    = slots[(u8)work->bodySphere.contacts[contactIndex].key.parts.id >> 7]->extra.tmd->coords;
                    x                        = other->coord.t[0] - coord->coord.t[0];
                    scratch->delta.vector.vx = x;
                    y                        = other->coord.t[1] - coord->coord.t[1];
                    scratch->delta.vector.vy = y;
                    z                        = other->coord.t[2] - coord->coord.t[2];
                    scratch->delta.vector.vz = z;
                    damage                   = damageComputePlayerAttack(work->bodySphere.contacts[contactIndex].key.value, SquareRoot0(x * x + y * y + z * z), 0, 0);
                    if (damageRollCriticalHit(actor->spawnArg2.pointer, work->bodySphere.contacts[contactIndex].key.value, 0)) {
                        damage *= 4;
                        effectSpawn(EFFECT_CRITICAL_HIT, actor->extra.tmd->coords + 1, 0, 0);
                    }
                    if (work->behavior == ACTOR_01600_BEHAVIOR_ATTACK && work->airborne != 0 && work->verticalSpeed < 0) {
                        damage *= 2;
                        effectSpawn(EFFECT_CRITICAL_HIT, actor->extra.tmd->coords + 1, 3, 0);
                    }
                    damageAccumulateLifeDrainHp(ctx, work->bodySphere.contacts[contactIndex].key.value, damage, 0);
                    _actor01600ApplyDamage(actor, damage);
                    count = damageGetPlayerAttackHitCooldown(work->bodySphere.contacts[contactIndex].key.value);
                    if (count > 0)
                        work->hitCooldown = count;
                    switch (damageGetPlayerAttackReaction(work->bodySphere.contacts[contactIndex].key.value) & 0xFFFF) {
                        case 4:
                        case DAMAGE_PLAYER_REACTION_EXPLOSION:
                            if ((s16)ctx->hp <= 0) {
                                work->burstState = 0;
                                Actor01600_Fn0646C(actor);
                                work->burstState = 2;
                                work->airborne   = 0;
                                return;
                            }
                            damageStartEnemyStagger(actor->spawnArg2.pointer);
                            break;
                        case DAMAGE_PLAYER_REACTION_BUILDUP:
                        case 9:
                            if (work->behavior != ACTOR_01600_BEHAVIOR_BUILDUP) {
                                ignoredState = ACTOR_01600_BEHAVIOR_BUILDUP_DOWN;
                                if (work->behavior != ignoredState) {
                                    damageStartEnemyBuildup(actor->spawnArg2.pointer, work->bodySphere.contacts[contactIndex].key.value, 0);
                                    work->buildupKnocksDown = 1;
                                }
                            }
                            break;
                        case 8:
                            if (work->behavior != ACTOR_01600_BEHAVIOR_BUILDUP) {
                                ignoredState = ACTOR_01600_BEHAVIOR_BUILDUP_DOWN;
                                if (work->behavior != ignoredState) {
                                    damageStartEnemyBuildup(actor->spawnArg2.pointer, work->bodySphere.contacts[contactIndex].key.value, 0);
                                    work->buildupKnocksDown = 0;
                                }
                            }
                            break;
                        case DAMAGE_PLAYER_REACTION_STAGGER:
                        case 5:
                            if (work->behavior != ACTOR_01600_BEHAVIOR_BUILDUP) {
                                ignoredState = ACTOR_01600_BEHAVIOR_BUILDUP_DOWN;
                                if (work->behavior != ignoredState) {
                                    damageStartEnemyStagger(actor->spawnArg2.pointer);
                                }
                            }
                            break;
                        case DAMAGE_PLAYER_REACTION_NONE:
                            break;
                        case DAMAGE_PLAYER_REACTION_POISON:
                            damageTryStartEnemyDamageOverTime(actor->spawnArg2.pointer, work->bodySphere.contacts[contactIndex].key.value, 0);
                            break;
                    }
                    if (damage >= 40 && work->buildupKnocksDown == 0) {
                        damageStartEnemyStagger(ctx);
                        if (work->behavior == ACTOR_01600_BEHAVIOR_ROAM && work->airborne != 0) {
                            work->airborne      = 0;
                            work->verticalSpeed = 0;
                            work->jumpHeight    = 0;
                        }
                    } else {
                        work->recoilActive      = 1;
                        gRandomLcgState         = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                        work->recoilRotation.vx = ((gRandomLcgState >> 11) & 0x60) + 0x100;
                    }
                    if (work->behavior != ACTOR_01600_BEHAVIOR_ROAM && work->airborne != 0) {
                        work->bite.body.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
                        worldCollisionClearContacts(work->bite.contacts);
                        work->animBlendFrames = 0;
                        work->verticalSpeed  += 20;
                        amount                = _actor01600MeasureTarget(actor, &distance);
                        if (amount < 0)
                            amount = -amount;
                        if (amount < 0x400) {
                            work->animRequest  = 14;
                            work->forwardSpeed = -40;
                        } else {
                            work->animRequest  = 11;
                            work->forwardSpeed = 40;
                        }
                        work->attackAction = ACTOR_01600_ACTION_KNOCKBACK;
                    }
                    effectSpawnHit(damageGetPlayerAttackEffectId(work->bodySphere.contacts[contactIndex].key.value), world, 0, &work->hitEffect);
                }
                break;
            case 3:
                cx                       = coord->workm.t[0] - work->bodySphere.contacts[contactIndex].point.vx;
                scratch->delta.vector.vy = 0;
                scratch->delta.vector.vx = cx;
                cz                       = coord->workm.t[2] - work->bodySphere.contacts[contactIndex].point.vz;
                scratch->delta.vector.vz = cz;
                push                     = cx * cx + cz * cz;
                push                     = SquareRoot0(push);
                push                     = -push;
                push                    += work->bodySphere.contacts[contactIndex].distance;
                clamped                  = push;
                if (push <= 0)
                    clamped = 0;
                push                     = clamped;
                scratch->delta.vector.vx = coord->workm.t[0] - work->bodySphere.contacts[contactIndex].point.vx;
                scratch->delta.vector.vy = coord->workm.t[1] - work->bodySphere.contacts[contactIndex].point.vy;
                scratch->delta.vector.vz = coord->workm.t[2] - work->bodySphere.contacts[contactIndex].point.vz;
                VectorNormal(&scratch->delta.vector, &scratch->normal);
                ApplyTransposeMatrixLV(&Gp_GridParams->viewCoord->workm, &scratch->normal, &scratch->delta.vector);
                if (work->animRequest == 23 || work->animRequest == 5 || work->animRequest == 6) {
                    coord->coord.t[0] += (push * scratch->delta.vector.vx) >> 12;
                    product            = push * scratch->delta.vector.vy;
                    if (product < 0)
                        coord->coord.t[1] += product >> 12;
                    coord->coord.t[2] += (push * scratch->delta.vector.vz) >> 12;
                }
                break;
            case 0:
            case 1:
            case 4:
            case 5:
            case 6:
            case 7:
            case 8:
            case 9:
            case 10:
            case 11:
            case 12:
            case 13:
            case 14:
            case 15:
            case 16:
                break;
        }
    }
    worldCollisionClearContacts(work->bodySphere.contacts);
    if (work->attackAction && worldCollisionFindContactIndex(work->bite.contacts, WORLD_COLLISION_FIND_ANY_KEY)) {
        work->bodySphere.body.pos.vy = -400;
        work->bodySphere.body.radius = 400;
        work->biteLanded             = 1;
        work->bite.body.flags       &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        worldCollisionClearContacts(work->bite.contacts);
        if (work->animFrame < 15) {
            work->attackAction    = ACTOR_01600_ACTION_KNOCKBACK;
            work->animBlendFrames = 0;
            work->verticalSpeed  += 20;
            amount                = _actor01600MeasureTarget(actor, &distance);
            if (amount < 0)
                amount = -amount;
            if (amount < 0x400) {
                work->animRequest  = 14;
                work->forwardSpeed = -40;
            } else {
                work->animRequest  = 11;
                work->forwardSpeed = 40;
            }
        }
    }
    SCRATCH_STACK_RELEASE_BLOCK(_Actor01600ContactScratch);
    return;
}

/// Subtracts damage from the scavenger and reports it to the target readout.
///
/// Hit points retain halfword subtraction. A lethal hit begins dying and disarms
/// the bite; a surviving hit stops forward motion and clears the alert.
static void _actor01600ApplyDamage(Task* actor, s32 damage)
{
    s32              soundScriptId;
    s32              audioPan;
    Enemy*           enemy;
    _Actor01600Work* work;
    GfxCoord*        rootCoord;

    enemy     = actor->spawnArg2.pointer;
    work      = actor->work;
    rootCoord = actor->extra.tmd->coords;
    enemy->hp = enemy->hp - damage;
    worldTargetAddReadoutAmount(&enemy->node, damage, 0);
    work->reactionDelay = 1;
    if (enemy->hp <= 0) {
        work->dead             = 1;
        work->deathPhase       = ACTOR_01600_DEATH_BEGIN;
        work->bite.body.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        soundScriptId          = (((u16)((Enemy*)actor->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x4010000A;
        sndEvtRequestScriptStart(soundScriptId, (s8)worldCoordGetOriginAudioPan(rootCoord), (s8)worldCoordGetOriginAudioDepth(rootCoord));
    } else {
        if (work->animRequest == ACTOR_01600_ANIMATION_REST) {
            work->animRequest = ACTOR_01600_ANIMATION_HIT_WAKE;
            work->animFrame   = 0;
        }
        work->forwardSpeed = 0;
        work->alerted      = 0;
        soundScriptId      = (((u16)((Enemy*)actor->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x40100009;
        audioPan           = (s8)worldCoordGetOriginAudioPan(rootCoord);
        sndEvtRequestScriptStart(soundScriptId, audioPan, (s8)worldCoordGetOriginAudioDepth(rootCoord));
    }
}

/// Per-frame tick of the actor's behaviours, dispatched on `behavior`. Roam
/// and attack hand the frame to `Actor01600_Fn017BC` / `Actor01600_Fn020F8`
/// and then run the shared post-step `Actor01600_Fn06744`. Stagger advances
/// `stateTimer`, sets `forwardSpeed` to -0x3C while the animation is still
/// 0xE and under 0x11 frames in, requests animation 0x16 at frame 0x28 and,
/// past frame 0x5B, returns to roam on animation 0x19 with the pair test of
/// `sight` enabled again. Buildup plays animation 0x13 until
/// `damageTickEnemyBuildup` fires. Posed only selects animation 0x11. The
/// knocked-down buildup sets `recoilRotation.vx` once per fall (animation
/// 0xE, past frame 0x2C, `recoilActive` still clear and bit 1 of `animFrame`
/// set) and, on animation 0x16 past frame 0x32, returns to roam the same way
/// stagger does.
///
/// Whatever the behaviour, animations 1/9/0x10/0x13/0x15/0x16/0x1B..0x1E are
/// silent; the rest count `idleSoundTimer` down and, on expiry, play one of three
/// growls (`0x4010_0006..8`) picked by a `gRandomLcgState` draw modulo 5 - two of
/// the five outcomes stay quiet - panned and attenuated for the actor's
/// coordinate, then rearm the counter at 0x14.
static void Actor01600_Fn01420(Task* arg0)
{
    _Actor01600Work* work;
    GfxCoord*        coord;
    s32              id;
    s32              state;
    u16              sel;
    s16              count;

    work  = arg0->work;
    coord = arg0->extra.tmd->coords;

    switch (work->behavior) {
        case ACTOR_01600_BEHAVIOR_ROAM:
            Actor01600_Fn017BC(arg0);
            Actor01600_Fn06744(arg0);
            break;
        case ACTOR_01600_BEHAVIOR_ATTACK:
            Actor01600_Fn020F8(arg0);
            Actor01600_Fn06744(arg0);
            break;
        case ACTOR_01600_BEHAVIOR_STAGGER:
            _actor01600ReleaseGrab(arg0);
            work->stateTimer = work->stateTimer + 1;
            if (work->animRequest == 0xE && work->animFrame < 0x11) {
                work->forwardSpeed = -0x3C;
                Actor01600_Fn06744(arg0);
            } else {
                work->forwardSpeed = 0;
            }
            if (work->stateTimer == 0x28) {
                work->animRate    = 0x10;
                work->animRequest = 0x16;
            }
            if (work->stateTimer >= 0x5B) {
                work->animRequest       = 0x19;
                work->behavior          = ACTOR_01600_BEHAVIOR_ROAM;
                work->stateTimer        = 0;
                work->sight.body.flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;
            }
            work->forwardSpeed = 0;
            Actor01600_Fn06744(arg0);
            break;
        case ACTOR_01600_BEHAVIOR_BUILDUP:
            _actor01600ReleaseGrab(arg0);
            work->animRequest = 0x13;
            if (damageTickEnemyBuildup(arg0->spawnArg2.pointer) != 0) {
                work->behavior          = ACTOR_01600_BEHAVIOR_ROAM;
                work->animRequest       = 0x19;
                work->sight.body.flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;
            }
            work->forwardSpeed = 0;
            Actor01600_Fn06744(arg0);
            break;
        case ACTOR_01600_BEHAVIOR_POSED:
            work->animRequest = 0x11;
            break;
        case ACTOR_01600_BEHAVIOR_BUILDUP_DOWN:
            _actor01600ReleaseGrab(arg0);
            if (work->animRequest == 0xE) {
                if (work->animFrame >= 0x2C && work->recoilActive == 0 &&
                    ((u16)work->animFrame & 2)) {
                    work->recoilActive      = 1;
                    gRandomLcgState         = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                    work->recoilRotation.vx = ((gRandomLcgState >> 11) & 0x60) + 0x20;
                }
            } else if (work->animRequest == 0x16) {
                if (work->animFrame >= 0x32) {
                    work->behavior          = ACTOR_01600_BEHAVIOR_ROAM;
                    work->animRequest       = 0x19;
                    work->sight.body.flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;
                }
            }
            if (damageTickEnemyBuildup(arg0->spawnArg2.pointer) != 0) {
                work->animRequest = 0x16;
            }
            work->forwardSpeed = 0;
            Actor01600_Fn06744(arg0);
            break;
    }

    state = work->animRequest;
    if (state == 1 || state == 0x16 || state == 0x15 || state == 0x10 ||
        state == 0x13 || state == 0x1C || state == 0x1D || state == 0x1E ||
        state == 0x1B || state == 9) {
        return;
    }
    count                = work->idleSoundTimer - 1;
    work->idleSoundTimer = count;
    if (count != 0) {
        return;
    }
    gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
    sel             = (gRandomLcgState >> 16) % 5;
    switch (sel) {
        case 0:
            id = ((((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x40100006;
            sndEvtRequestScriptStart(id, (s8)worldCoordGetOriginAudioPan(coord), (s8)worldCoordGetOriginAudioDepth(coord));
            break;
        case 1:
            id = ((((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x40100007;
            sndEvtRequestScriptStart(id, (s8)worldCoordGetOriginAudioPan(coord), (s8)worldCoordGetOriginAudioDepth(coord));
            break;
        case 2:
            id = ((((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x40100008;
            sndEvtRequestScriptStart(id, (s8)worldCoordGetOriginAudioPan(coord), (s8)worldCoordGetOriginAudioDepth(coord));
            break;
    }
    work->idleSoundTimer = 0x14;
}

static void Actor01600_Fn017BC(Task* actor)
{
    Enemy*                 ctx;
    _Actor01600Work*       work;
    GfxCoord*              coord;
    TmdObject*             model;
    s16                    frameOffset;
    s16                    count;
    s16                    tick;
    s16                    state;
    s16                    height;
    s16                    repeatHeight;
    s16                    frame;
    s32                    contact;
    s32                    id;
    void*                  old;
    s32                    pan8;
    s32                    distance;
    WorldCollisionContact* rec;
    s32                    pan1;
    s32                    pan2;
    s32                    pan3;
    s32                    pan4;
    s32                    pan5;
    s32                    pan6;
    s32                    pan7;
    u16                    flags;
    u16                    attackFrame;

    work                       = actor->work;
    old                        = SCRATCH_STACK_CURSOR(void);
    rec                        = work->sight.contacts;
    SCRATCH_STACK_CURSOR(void) = old - 8;
    model                      = actor->extra.tmd;
    coord                      = model->coords;
    ctx                        = actor->spawnArg2.pointer;
    if (worldCollisionCountContactsByKind(rec, WORLD_COLLISION_CONTACT_PLAYER_BODY) != 0) {
        work->alerted = 1;
    }
    if (work->alerted != 0) {
        work->behavior              = ACTOR_01600_BEHAVIOR_ATTACK;
        work->attackAction          = ACTOR_01600_ACTION_ADVANCE;
        work->turnMode              = ACTOR_01600_TURN_TRACK_TARGET;
        work->animBlendFrames       = 0;
        work->airborne              = 0;
        work->verticalSpeed         = 0;
        work->jumpHeight            = 0;
        work->animFrame             = 0;
        work->pathSearchPhase       = ACTOR_01600_PATH_SEARCH_PROBE_TARGET;
        work->sight.body.flags     &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ (WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED));
        work->pathProbe.body.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ (WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED));
        sceneEngageBattle(1);
    }
    worldCollisionClearContacts(rec);
    if (work->noiseHeard == 1) {
        count               = (u16)work->reactionDelay - 1;
        work->reactionDelay = count;
        if (count == 0) {
            work->animFrame   = 0;
            work->animRequest = 2;
            work->noiseHeard  = 0;
        }
    }
    tick             = (u16)work->stateTimer + 1;
    work->stateTimer = tick;
    if (tick >= 0x1F) {
        work->stateTimer = 0;
    }
    state = (u16)work->animRequest - 1;
    switch (state) {
        case 0:
            work->sight.shape.ends[0].vz = 0x3E8;
            work->forwardSpeed           = 0;
            work->animRate               = 0x10;
            if (work->animFrame >= 0x3E) {
                work->animFrame = 0;
            }
            if (ctx->place->mode == 0) {
                if ((gSceneCombatState.signals.bytes.actionFlags & SCENE_COMBAT_ACTION_NOISE) || (gSceneCombatState.signals.packed & SCENE_COMBAT_SIGNAL_CAST_FOOTSTEP_OR_ALERT)) {
                    work->noiseHeard = 1;
                }
            }
            break;
        case 1:
            work->sight.shape.ends[0].vz = 0xFA0;
            work->forwardSpeed           = 0;
            work->animRate               = 0x10;
            work->animBlendFrames        = 0;
            if (work->animFrame >= 0x36) {
                work->animRequest           = 0x19;
                work->animFrame             = 0;
                work->pathSearchPhase       = ACTOR_01600_PATH_SEARCH_PROBE_TARGET;
                work->field_514             = 1;
                work->pathProbe.body.flags |= (WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED);
            }
            break;
        case 24:
            work->forwardSpeed    = 0;
            work->animRate        = 0x10;
            work->animBlendFrames = 0;
            contact               = _actor01600SearchClearHeading(actor) & 0xFF;
            if (contact != 0) {
                flags                      = work->pathProbe.body.flags & (WORLD_COLLISION_BODY_FLAGS_MASK ^ (WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED));
                work->pathProbe.body.flags = flags;
                if (contact != 0xFF) {
                    distance        = work->turnRequest;
                    work->hopCount  = 0U;
                    distance        = abs(distance);
                    work->animFrame = 0;
                    if ((distance >= 0x201) || ((contact & 0xF) == 2)) {
                        work->turnMode = ACTOR_01600_TURN_REQUEST_BEGIN;
                        if ((contact & 0xF0) == 0x80) {
                            work->animRequest = 7;
                        } else {
                            work->animRequest = 8;
                        }
                    } else {
                        work->turnMode    = ACTOR_01600_TURN_TRACK_TARGET;
                        work->animRequest = 4;
                    }
                } else {
                    work->pathProbe.body.flags = flags | (WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED);
                    work->pathSearchPhase      = ACTOR_01600_PATH_SEARCH_PROBE_TARGET;
                }
            }
            if (work->animFrame >= 0x57) {
                work->animFrame   = 0;
                work->animPlaying = 0;
                work->animRequest = 0x19;
            }
            break;
        case 2:
            work->animBlendFrames = 0;
            work->animRate        = 0x10;
            work->forwardSpeed    = 0;
            if (work->animFrame >= 0x3D) {
                work->animFrame   = 0;
                work->animRequest = 3;
            }
            break;
        case 3:
            work->animRate        = 0x10;
            work->animBlendFrames = 4;
            if (work->airborne != 0) {
                if (work->animFrame >= 0xC) {
                    work->verticalSpeed = (u16)work->verticalSpeed + 0xF;
                } else {
                    work->verticalSpeed = (u16)work->verticalSpeed + 0xA;
                }
                height           = (u16)work->jumpHeight + (u16)work->verticalSpeed;
                work->jumpHeight = height;
                if (height >= 0) {
                    work->airborne      = 0;
                    work->verticalSpeed = 0;
                    work->jumpHeight    = 0;
                }
            }
            if (work->animFrame == 6) {
                work->airborne      = 1;
                work->verticalSpeed = -0x50;
                work->jumpHeight    = (u16)work->jumpHeight - 0x50;
            }
            attackFrame = (u16)work->animFrame;
            if ((u32)(attackFrame - 5) < 0x10U) {
                if ((s16)attackFrame >= 0xC) {
                    work->forwardSpeed = 0x5A;
                } else {
                    work->forwardSpeed = 0x3C;
                }
                _actor01600StepTurn(actor);
            } else {
                work->forwardSpeed = 0;
            }
            if (work->animFrame == 0x14) {
                id   = (((u16)((Enemy*)actor->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x40100002;
                pan1 = (s8)worldCoordGetOriginAudioPan(coord);
                sndEvtRequestScriptStart(id, (s32)pan1, (s8)worldCoordGetOriginAudioDepth(coord));
            }
            if (work->animFrame >= 0x15) {
                id   = (((u16)((Enemy*)actor->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x40100001;
                pan2 = (s8)worldCoordGetOriginAudioPan(coord);
                sndEvtRequestScriptStart(id, (s32)pan2, (s8)worldCoordGetOriginAudioDepth(coord));
                work->animRequest      = 0x17;
                work->airborne         = 0;
                work->verticalSpeed    = 0;
                work->jumpHeight       = 0;
                work->hopFrameOffset   = 0;
                work->animFrame        = 0;
                work->hopCount         = (u16)(work->hopCount + 1);
                work->sight.body.flags = (work->sight.body.flags | WORLD_COLLISION_BODY_PAIR_ENABLED) & (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_GRID_ENABLED);
            }
            break;
        case 22:
            work->animRate = 0x10;
            if (work->airborne != 0) {
                if (work->animFrame >= (work->hopFrameOffset + 0xC)) {
                    work->verticalSpeed = (u16)work->verticalSpeed + 0xF;
                } else {
                    work->verticalSpeed = (u16)work->verticalSpeed + 0xA;
                }
                repeatHeight     = (u16)work->jumpHeight + (u16)work->verticalSpeed;
                work->jumpHeight = repeatHeight;
                if (repeatHeight >= 0) {
                    work->airborne      = 0;
                    work->verticalSpeed = 0;
                    work->jumpHeight    = 0;
                }
            }
            if (work->animFrame == (work->hopFrameOffset + 6)) {
                work->airborne      = 1;
                work->verticalSpeed = -0x50;
                work->jumpHeight    = (u16)work->jumpHeight - 0x50;
            }
            frameOffset = work->hopFrameOffset;
            frame       = work->animFrame;
            if ((frame >= (frameOffset + 6)) && ((frameOffset + 0x15) >= frame)) {
                if ((frameOffset + 0xA) >= frame) {
                    work->forwardSpeed = 0x5A;
                } else {
                    work->forwardSpeed = 0x3C;
                }
                _actor01600StepTurn(actor);
            } else {
                work->forwardSpeed = 0;
            }
            if (work->animFrame == (work->hopFrameOffset + 0x14)) {
                id   = (((u16)((Enemy*)actor->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x40100002;
                pan3 = (s8)worldCoordGetOriginAudioPan(coord);
                sndEvtRequestScriptStart(id, (s32)pan3, (s8)worldCoordGetOriginAudioDepth(coord));
            }
            if (work->animFrame >= (work->hopFrameOffset + 0x17)) {
                id   = (((u16)((Enemy*)actor->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x40100001;
                pan4 = (s8)worldCoordGetOriginAudioPan(coord);
                sndEvtRequestScriptStart(id, (s32)pan4, (s8)worldCoordGetOriginAudioDepth(coord));
                work->hopCount        = (u16)(work->hopCount + 1);
                work->animBlendFrames = 0;
                work->airborne        = 0;
                work->verticalSpeed   = 0;
                work->jumpHeight      = 0;
                work->field_514       = 0;
                work->animFrame       = 0;
                work->hopFrameOffset  = -3;
                if ((s16)work->hopCount >= 3) {
                    work->animRequest = 0x18;
                } else {
                    work->animRequest = 0x17;
                }
                work->sight.body.flags = (work->sight.body.flags | WORLD_COLLISION_BODY_PAIR_ENABLED) & (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_GRID_ENABLED);
            }
            break;
        case 23:
            work->animRate        = 0x10;
            work->animBlendFrames = 4;
            work->forwardSpeed    = 0;
            if (work->animFrame >= 0xA) {
                work->animRequest           = 0x19;
                work->field_514             = 1;
                work->alerted               = 0;
                work->pathSearchPhase       = ACTOR_01600_PATH_SEARCH_PROBE_TARGET;
                work->pathProbe.body.flags |= (WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED);
                work->sight.body.flags      = (work->sight.body.flags | WORLD_COLLISION_BODY_PAIR_ENABLED) & (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_GRID_ENABLED);
            }
            break;
        case 6:
        case 7:
            work->animRate        = 0x10;
            work->animBlendFrames = 0;
            if (work->animRequest == 7) {
                if (work->animFrame == 0xF) {
                    id   = (((u16)((Enemy*)actor->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x40100003;
                    pan5 = (s8)worldCoordGetOriginAudioPan(coord);
                    sndEvtRequestScriptStart(id, (s32)pan5, (s8)worldCoordGetOriginAudioDepth(coord));
                }
                if (work->animFrame == 0x11) {
                    id   = (((u16)((Enemy*)actor->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x40100004;
                    pan8 = (s8)worldCoordGetOriginAudioPan(coord);
                    sndEvtRequestScriptStart(id, (s32)pan8, (s8)worldCoordGetOriginAudioDepth(coord));
                }
            } else {
                if (work->animFrame == 0xF) {
                    id   = (((u16)((Enemy*)actor->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x40100004;
                    pan6 = (s8)worldCoordGetOriginAudioPan(coord);
                    sndEvtRequestScriptStart(id, (s32)pan6, (s8)worldCoordGetOriginAudioDepth(coord));
                }
                if (work->animFrame == 0x12) {
                    id   = (((u16)((Enemy*)actor->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x40100003;
                    pan7 = (s8)worldCoordGetOriginAudioPan(coord);
                    sndEvtRequestScriptStart(id, (s32)pan7, (s8)worldCoordGetOriginAudioDepth(coord));
                }
            }
            work->forwardSpeed = 0;
            if ((u32)((u16)work->animFrame - 4) < 0x11U) {
                _actor01600StepTurn(actor);
            }
            if (work->animFrame >= 0x1C) {
                work->animPlaying       = 0;
                work->animFrame         = 0;
                work->sight.body.flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;
                if (work->turnMode == ACTOR_01600_TURN_HOLD) {
                    work->field_514   = 0;
                    work->animFrame   = 0;
                    work->animRequest = 4;
                }
            }
            break;
        case 9:
            work->animBlendFrames = 0;
            work->animRate        = 0x10;
            work->forwardSpeed    = 0;
            if (work->animFrame >= 0x28) {
                work->alerted     = 1;
                work->animFrame   = 0;
                work->animRequest = 4;
            }
            break;
        default:
            break;
    }
    SCRATCH_STACK_RELEASE_BYTES(8);
}

static void Actor01600_Fn020F8(Task* actor)
{
    PlayerStatus*    config = &gPlayerStatus;
    s32              neg_velocity;
    s32              reset_y;
    s32              reset_y2;
    SVECTOR          offset;
    s32              distance;
    Enemy*           ctx;
    _Actor01600Work* work;
    GfxCoord*        effectCoord;
    GfxCoord*        coord;
    s16              targetKind;
    s16              temp_v0_3;
    s16              temp_v0_4;
    s16              temp_v0_7;
    s16              temp_v1;
    s16              temp_v1_5;
    s16              temp_v1_6;
    s16              temp_v1_7;
    s32              id;
    s32              flags;
    s32              angle;
    s32              var_v1;
    s32              pan10;
    s32              pan11;
    s32              pan12;
    s32              pan13;
    s32              pan14;
    s32              pan_case3;
    s32              pan15;
    s32              pan16;
    s32              pan17;
    s32              pan18;
    s32              pan19;
    s32              pan20;
    s32              pan21;
    s32              pan23;
    s32              pan24;
    s32              pan_msg_zero;
    s32              pan25;
    s32              pan26;
    s32              pan27;
    s32              pan28;
    s32              pan29;
    s32              pan2;
    s32              pan30;
    s32              pan31;
    s32              pan32;
    s32              pan33;
    s32              pan34;
    s32              pan35;
    s32              pan36;
    s32              pan3;
    s32              pan4;
    s32              pan5;
    s32              pan6;
    s32              pan7;
    s32              pan8;
    s32              pan9;
    u16              temp_v0_6;
    u16              temp_v1_3;
    u16              temp_v1_8;
    GfxCoord*        attachedCoord;
    GfxCoord*        attachedCoord2;
    GfxCoord*        attachedOffset;
    GfxCoord*        attachedOffset2;

    ctx         = actor->spawnArg2.pointer;
    work        = actor->work;
    coord       = actor->extra.tmd->coords;
    flags       = _actor01600SelectNearestPlayer(actor) & 0xFF;
    effectCoord = &actor->extra.tmd->coords[2];
    memset(&offset, 0, 8);
    offset.vy = 0x32;
    temp_v1   = work->attackAction;
    switch (temp_v1) {
        case ACTOR_01600_ACTION_ADVANCE:
            work->animBlendFrames = 4;
            work->animRequest     = 5;
            work->forwardSpeed    = 0;
            work->animRate        = 0x10;
            if ((u32)((u16)work->animFrame - 9) < 9U) {
                work->forwardSpeed = 0x3C;
                _actor01600StepTurn(actor);
                Actor01600_Fn06974(actor, 0x46);
                if (work->targetAnchorPhase == ACTOR_01600_TARGET_ANCHOR_FOLLOW) {
                    work->targetAnchorPhase = ACTOR_01600_TARGET_ANCHOR_DETACH;
                }
            }
            if (work->animFrame >= 0x12) {
                id   = (((u16)((Enemy*)actor->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x40100004;
                pan2 = (s8)worldCoordGetOriginAudioPan(coord);
                sndEvtRequestScriptStart(id, (s32)pan2, (s8)worldCoordGetOriginAudioDepth(coord));
                if (Actor01600_Fn06C1C(actor) & 0xFF) {
                    break;
                }
                work->attackAction = ACTOR_01600_ACTION_ZIGZAG;
                work->animFrame    = 0;
                work->forwardSpeed = 0;
                work->animRequest  = 0x17;
            }
            angle = _actor01600MeasureTarget(actor, &distance);
            if (!(Actor01600_Fn06C94(actor, angle, distance) & 0xFF)) {
                if (Actor01600_Fn04974(actor, angle, distance, flags) & 0xFF) {
                    if (work->animFrame >= 0xA) {
                        id   = (((u16)((Enemy*)actor->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x40100003;
                        pan3 = (s8)worldCoordGetOriginAudioPan(coord);
                        sndEvtRequestScriptStart(id, (s32)pan3, (s8)worldCoordGetOriginAudioDepth(coord));
                        id   = (((u16)((Enemy*)actor->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x40100004;
                        pan4 = (s8)worldCoordGetOriginAudioPan(coord);
                        sndEvtRequestScriptStart(id, (s32)pan4, (s8)worldCoordGetOriginAudioDepth(coord));
                        work->animFrame = 0;
                        return;
                    }
                } else if ((Actor01600_Fn06D74(actor, angle, distance) & 0xFF) && (work->animFrame >= 0xA)) {
                    id   = (((u16)((Enemy*)actor->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x40100003;
                    pan5 = (s8)worldCoordGetOriginAudioPan(coord);
                    sndEvtRequestScriptStart(id, (s32)pan5, (s8)worldCoordGetOriginAudioDepth(coord));
                    id   = (((u16)((Enemy*)actor->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x40100004;
                    pan6 = (s8)worldCoordGetOriginAudioPan(coord);
                    sndEvtRequestScriptStart(id, (s32)pan6, (s8)worldCoordGetOriginAudioDepth(coord));
                    work->animFrame = 0;
                    return;
                }
            } else {
                default:
                    return;
            }
            break;
        case ACTOR_01600_ACTION_ZIGZAG:
            work->animBlendFrames = 4;
            work->animRate        = 0x10;
            work->forwardSpeed    = 0x1E;
            work->animRequest     = 6;
            if ((u32)((u16)work->animFrame - 8) < 8U) {
                work->forwardSpeed = 0x3C;
                _actor01600StepTurn(actor);
                Actor01600_Fn06974(actor, -0x5A);
                if (work->targetAnchorPhase == ACTOR_01600_TARGET_ANCHOR_FOLLOW) {
                    work->targetAnchorPhase = ACTOR_01600_TARGET_ANCHOR_DETACH;
                }
            }
            if (work->animFrame == 0x12) {
                id   = (((u16)((Enemy*)actor->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x40100003;
                pan7 = (s8)worldCoordGetOriginAudioPan(coord);
                sndEvtRequestScriptStart(id, (s32)pan7, (s8)worldCoordGetOriginAudioDepth(coord));
            }
            if ((u32)((u16)work->animFrame - 0x1A) < 9U) {
                work->forwardSpeed = 0x3C;
                _actor01600StepTurn(actor);
                Actor01600_Fn06974(actor, 0x46);
                if (work->targetAnchorPhase == ACTOR_01600_TARGET_ANCHOR_FOLLOW) {
                    work->targetAnchorPhase = ACTOR_01600_TARGET_ANCHOR_DETACH;
                }
            }
            if (work->animFrame >= 0x22) {
                id   = (((u16)((Enemy*)actor->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x40100004;
                pan8 = (s8)worldCoordGetOriginAudioPan(coord);
                sndEvtRequestScriptStart(id, (s32)pan8, (s8)worldCoordGetOriginAudioDepth(coord));
                work->animFrame = 0;
                if (Actor01600_Fn06C1C(actor) & 0xFF) {
                    return;
                }
            }
            angle = _actor01600MeasureTarget(actor, &distance);
            if (!(Actor01600_Fn06C94(actor, angle, distance) & 0xFF)) {
                if (Actor01600_Fn04974(actor, angle, distance, flags) & 0xFF) {
                    if (work->animFrame >= 8) {
                        id   = (((u16)((Enemy*)actor->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x40100003;
                        pan9 = (s8)worldCoordGetOriginAudioPan(coord);
                        sndEvtRequestScriptStart(id, (s32)pan9, (s8)worldCoordGetOriginAudioDepth(coord));
                        id    = (((u16)((Enemy*)actor->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x40100004;
                        pan10 = (s8)worldCoordGetOriginAudioPan(coord);
                        sndEvtRequestScriptStart(id, (s32)pan10, (s8)worldCoordGetOriginAudioDepth(coord));
                        work->animFrame = 0;
                        return;
                    }
                } else if ((Actor01600_Fn06D74(actor, angle, distance) & 0xFF) && (work->animFrame >= 8)) {
                    id    = (((u16)((Enemy*)actor->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x40100003;
                    pan11 = (s8)worldCoordGetOriginAudioPan(coord);
                    sndEvtRequestScriptStart(id, (s32)pan11, (s8)worldCoordGetOriginAudioDepth(coord));
                    id    = (((u16)((Enemy*)actor->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x40100004;
                    pan12 = (s8)worldCoordGetOriginAudioPan(coord);
                    sndEvtRequestScriptStart(id, (s32)pan12, (s8)worldCoordGetOriginAudioDepth(coord));
                    work->animFrame = 0;
                    return;
                }
            }
            break;
        case ACTOR_01600_ACTION_TURN:
            work->animRequest     = 7;
            work->animRate        = 0x10;
            work->animBlendFrames = 0;
            work->forwardSpeed    = 0;
            if (work->animFrame == 0xF) {
                id    = (((u16)((Enemy*)actor->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x40100003;
                pan13 = (s8)worldCoordGetOriginAudioPan(coord);
                sndEvtRequestScriptStart(id, (s32)pan13, (s8)worldCoordGetOriginAudioDepth(coord));
            }
            if (work->animFrame == 0x11) {
                var_v1 = 0x40100004;
                id     = (((u16)((Enemy*)actor->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | var_v1;
                pan14  = (s8)worldCoordGetOriginAudioPan(coord);
                sndEvtRequestScriptStart(id, (s32)pan14, (s8)worldCoordGetOriginAudioDepth(coord));
            }
            if ((u32)((u16)work->animFrame - 4) < 0x11U) {
                _actor01600StepTurn(actor);
            }
            if (work->animFrame >= 0x1C) {
                work->animPlaying = 0;
                work->animFrame   = 0;
                if (work->turnMode == ACTOR_01600_TURN_HOLD) {
                    work->animFrame    = 0;
                    work->attackAction = ACTOR_01600_ACTION_ADVANCE;
                    return;
                }
            }
            break;
        case ACTOR_01600_ACTION_TURN_WRAPPED:
            work->animRequest     = 8;
            work->animRate        = 0x10;
            work->animBlendFrames = 0;
            work->forwardSpeed    = 0;
            if (work->animFrame == 0xF) {
                id    = (((u16)((Enemy*)actor->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x40100004;
                pan15 = (s8)worldCoordGetOriginAudioPan(coord);
                sndEvtRequestScriptStart(id, (s32)pan15, (s8)worldCoordGetOriginAudioDepth(coord));
            }
            if (work->animFrame == 0x12) {
                var_v1    = 0x40100003;
                id        = (((u16)((Enemy*)actor->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | var_v1;
                pan_case3 = (s8)worldCoordGetOriginAudioPan(coord);
                sndEvtRequestScriptStart(id, (s32)pan_case3, (s8)worldCoordGetOriginAudioDepth(coord));
            }
            if ((u32)((u16)work->animFrame - 4) < 0x11U) {
                _actor01600StepTurn(actor);
            }
            if (work->animFrame >= 0x1C) {
                work->animPlaying = 0;
                work->animFrame   = 0;
                if (work->turnMode == ACTOR_01600_TURN_HOLD) {
                    work->animFrame    = 0;
                    work->attackAction = ACTOR_01600_ACTION_ADVANCE;
                    return;
                }
            }
            break;
        case ACTOR_01600_ACTION_SETTLE:
            work->animRate        = 0x10;
            work->animRequest     = 9;
            work->forwardSpeed    = 0;
            work->animBlendFrames = 0;
            if (work->animFrame == 1) {
                id    = (((u16)((Enemy*)actor->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x4010000E;
                pan16 = (s8)worldCoordGetOriginAudioPan(coord);
                sndEvtRequestScriptStart(id, (s32)pan16, (s8)worldCoordGetOriginAudioDepth(coord));
            }
            if (work->animFrame >= 0x25) {
                work->attackAction    = ACTOR_01600_ACTION_ADVANCE;
                work->animFrame       = 0;
                work->shadowUnderBody = 0;
                return;
            }
            break;
        case ACTOR_01600_ACTION_LUNGE:
            work->animRate        = 0x10;
            work->forwardSpeed    = 0;
            work->animBlendFrames = 0;
            work->animRequest     = 0x1A;
            if (work->airborne != 0) {
                work->animRate = 0x14;
                if (work->verticalSpeed < 0) {
                    work->verticalSpeed = (u16)work->verticalSpeed + 0xF;
                } else {
                    work->verticalSpeed = (u16)work->verticalSpeed + 0x14;
                }
                temp_v0_3        = (u16)work->jumpHeight + (u16)work->verticalSpeed;
                work->jumpHeight = temp_v0_3;
                if (temp_v0_3 >= 0) {
                    work->airborne      = 0;
                    work->verticalSpeed = 0;
                    work->jumpHeight    = 0;
                }
            }
            if (work->animFrame == 8) {
                work->airborne      = 1;
                work->suspended     = 0;
                work->verticalSpeed = -0x96;
                work->jumpHeight    = (u16)work->jumpHeight - 0x96;
            }
            if (work->animFrame == 0x1E) {
                id    = (((u16)((Enemy*)actor->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x40100001;
                pan17 = (s8)worldCoordGetOriginAudioPan(coord);
                sndEvtRequestScriptStart(id, (s32)pan17, (s8)worldCoordGetOriginAudioDepth(coord));
                id    = (((u16)((Enemy*)actor->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x40100002;
                pan18 = (s8)worldCoordGetOriginAudioPan(coord);
                sndEvtRequestScriptStart(id, (s32)pan18, (s8)worldCoordGetOriginAudioDepth(coord));
                work->airborne      = 0;
                work->verticalSpeed = 0;
                work->jumpHeight    = 0;
            }
            temp_v1_3 = (u16)work->animFrame;
            if ((u32)(temp_v1_3 - 8) < 0x17U) {
                if (work->biteLanded == 0) {
                    if ((s16)temp_v1_3 < 0xC) {
                        work->bodySphere.body.pos.vy = -0x258;
                        work->bodySphere.body.radius = 0x258;
                        work->forwardSpeed           = 0x12C;
                    } else if ((s16)temp_v1_3 < 0x18) {
                        work->bodySphere.body.pos.vy = -0x190;
                        work->bodySphere.body.radius = 0x190;
                        work->forwardSpeed           = 0x32;
                    } else {
                        work->forwardSpeed = 0x19;
                    }
                }
                _actor01600StepTurn(actor);
            }
            if ((work->animFrame == 9) && (work->entranceLeap == 0)) {
                work->bite.body.flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;
            }
            if (work->animFrame == 0x17) {
                work->bodySphere.body.flags = (u16)(work->bodySphere.body.flags | WORLD_COLLISION_BODY_GRID_ENABLED);
                work->bite.body.flags      &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
            }
            if (work->animFrame >= 0x34) {
                if (work->entranceLeap != 0) {
                    work->attackAction = ACTOR_01600_ACTION_SETTLE;
                } else {
                    work->attackAction = ACTOR_01600_ACTION_ADVANCE;
                }
                work->entranceLeap = 0;
                work->animFrame    = 0;
                work->shadowHidden = 0;
                return;
            }
            break;
        case ACTOR_01600_ACTION_LUNGE_RECOVER:
            work->animRate        = 0x10;
            work->animBlendFrames = 0;
            if (work->airborne != 0) {
                work->animRate = 0x14;
                if (work->verticalSpeed < 0) {
                    work->verticalSpeed = (u16)work->verticalSpeed + 0xF;
                } else {
                    work->verticalSpeed = (u16)work->verticalSpeed + 0x14;
                }
                temp_v0_4        = (u16)work->jumpHeight + (u16)work->verticalSpeed;
                work->jumpHeight = temp_v0_4;
                if (temp_v0_4 >= 0) {
                    work->airborne      = 0;
                    work->verticalSpeed = 0;
                    work->jumpHeight    = 0;
                }
            }
            temp_v1_5 = work->animRequest;
            switch (temp_v1_5) {
                case 26:
                    temp_v1_6 = work->animFrame;
                    if (temp_v1_6 == 8) {
                        work->suspended = 0;
                        work->airborne  = 1;
                        if (work->longLeap == 0) {
                            work->verticalSpeed = -0x96;
                        } else {
                            work->verticalSpeed = -0xC8;
                        }
                        work->jumpHeight = (u16)work->jumpHeight + (u16)work->verticalSpeed;
                    }
                    work->forwardSpeed = 0;
                    if (work->animFrame >= 8) {
                        if (work->longLeap == 0) {
                            if (work->animFrame < 0xC) {
                                work->bodySphere.body.pos.vy = -0x258;
                                work->bodySphere.body.radius = 0x258;
                                work->forwardSpeed           = 0x12C;
                            } else {
                                if (work->animFrame < 0x18) {
                                    work->bodySphere.body.pos.vy = -0x190;
                                    work->bodySphere.body.radius = 0x190;
                                    work->forwardSpeed           = 0x32;
                                } else {
                                    work->forwardSpeed = 0x19;
                                }
                            }
                        } else {
                            if (work->animFrame < 0xC) {
                                work->forwardSpeed = 0x1F4;
                            } else if (work->animFrame < 0x18) {
                                work->forwardSpeed = 0x46;
                            } else {
                                work->forwardSpeed = 0x32;
                            }
                        }
                    }
                    if ((work->animFrame == 9) && (work->entranceLeap == 0)) {
                        work->bite.body.flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;
                    }
                    if (work->animFrame == 0x17) {
                        work->bodySphere.body.flags = (u16)(work->bodySphere.body.flags | WORLD_COLLISION_BODY_GRID_ENABLED);
                        work->bite.body.flags      &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
                    }
                    if (work->animFrame >= 0x19) {
                        work->animRequest  = 0x1B;
                        work->animFrame    = 0;
                        work->longLeap     = 0;
                        work->entranceLeap = 0;
                        return;
                    }
                    break;
                case 27:
                    work->forwardSpeed = 0x3C;
                    if (work->animFrame == 0xB) {
                        id    = (((u16)((Enemy*)actor->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x40100005;
                        pan19 = (s8)worldCoordGetOriginAudioPan(coord);
                        sndEvtRequestScriptStart(id, (s32)pan19, (s8)worldCoordGetOriginAudioDepth(coord));
                    }
                    if (work->animFrame >= 0xD) {
                        work->animRate     = 0x10;
                        work->forwardSpeed = 0;
                    }
                    if (work->animFrame >= 0x28) {
                        work->animRequest = 0x15;
                        work->animFrame   = 0;
                    }
                    break;
                case 21:
                    if (work->animFrame == 0x1F) {
                        id    = (((u16)((Enemy*)actor->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x40100001;
                        pan20 = (s8)worldCoordGetOriginAudioPan(coord);
                        sndEvtRequestScriptStart(id, (s32)pan20, (s8)worldCoordGetOriginAudioDepth(coord));
                        id    = (((u16)((Enemy*)actor->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x40100002;
                        pan21 = (s8)worldCoordGetOriginAudioPan(coord);
                        sndEvtRequestScriptStart(id, (s32)pan21, (s8)worldCoordGetOriginAudioDepth(coord));
                    }
                    if (work->animFrame >= 0x2F) {
                        work->attackAction = ACTOR_01600_ACTION_ADVANCE;
                        work->animFrame    = 0;
                        work->shadowHidden = 0;
                        return;
                    }
                    break;
            }
            break;
        case ACTOR_01600_ACTION_GRAB:
            work->shadowUnderBody = 1;
            work->forwardSpeed    = 0;
            work->animBlendFrames = 0;
            work->animRate        = 0x10;
            work->airborne        = 0;
            work->verticalSpeed   = 0;
            temp_v1_7             = (u16)work->animRequest - 9;
            work->jumpHeight      = 0;
            switch (temp_v1_7) {
                case 19:
                    _actor01600StepTurn(actor);
                    Actor01600_Fn06974(actor, 0xA);
                    if (work->animFrame >= 0xD) {
                        work->forwardSpeed = 0x5A;
                    }
                    if (work->animFrame >= 0x12) {
                        if (Actor01600_Fn047A0(actor) & 0xFF) {
                            targetKind = work->grabTargetIndex;
                            if (targetKind == 1) {
                                if (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.companionHp > 0) {
                                    if (taskMessageDispatch(work->grabTarget, GAME_ACTOR_MESSAGE_APPLY_DAMAGE, damagePackEnemyAttackKey(ctx, 0), 0) == targetKind) {
                                        work->animRequest   = 9;
                                        work->forwardSpeed  = 0;
                                        work->animFrame     = 0;
                                        work->grabEndHeight = (s32)coord->coord.t[1];
                                        return;
                                    }
                                } else {
                                    work->animRequest   = 9;
                                    work->forwardSpeed  = 0;
                                    work->animFrame     = 0;
                                    work->grabEndHeight = (s32)coord->coord.t[1];
                                    return;
                                }
                            } else if (config->hp <= 0) {
                                work->animRequest   = 9;
                                work->forwardSpeed  = 0;
                                work->animFrame     = 0;
                                work->grabEndHeight = (s32)coord->coord.t[1];
                                return;
                            }
                            work->holdingTarget   = 1;
                            work->animBlendFrames = 0;
                            work->animRequest     = 0x1D;
                            id                    = (((u16)((Enemy*)actor->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x4010000D;
                            pan23                 = (s8)worldCoordGetOriginAudioPan(coord);
                            sndEvtRequestScriptStart(id, (s32)pan23, (s8)worldCoordGetOriginAudioDepth(coord));
                            if (work->grabTargetIndex != 0) {
                                Actor01600_D127D8.animationId = 1;
                                TASK_MESSAGE_DISPATCH_POINTER(work->grabTarget, ANIMATION_MESSAGE_INSTALL_AND_PLAY, &Actor01600_D127D8, 0);
                                id    = (((u16)((Enemy*)actor->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x4065000A;
                                pan24 = (s8)worldCoordGetOriginAudioPan(coord);
                                sndEvtRequestScriptStart(id, (s32)pan24, (s8)worldCoordGetOriginAudioDepth(coord));
                            } else {
                                Actor01600_D127D8.animationId = 2;
                                TASK_MESSAGE_DISPATCH_POINTER(work->grabTarget, ANIMATION_MESSAGE_REPLACE_AND_PLAY, &Actor01600_D127D8, 0);
                                id           = (((u16)((Enemy*)actor->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 6;
                                pan_msg_zero = (s8)worldCoordGetOriginAudioPan(coord);
                                sndEvtRequestScriptStart(id, (s32)pan_msg_zero, (s8)worldCoordGetOriginAudioDepth(coord));
                            }
                            work->grabBiteCount = 2;
                        } else {
                            work->animBlendFrames = 4;
                            work->shadowUnderBody = 0;
                            work->animRequest     = 0x1B;
                        }
                        work->forwardSpeed  = 0;
                        work->animFrame     = 0;
                        work->grabBiteTimer = 0U;
                        return;
                    }
                    break;
                case 20:
                    attachedCoord       = work->grabTarget->extra.tmd->coords;
                    attachedOffset      = &attachedCoord[17];
                    coord->coord.t[0]   = attachedCoord->coord.t[0] + attachedOffset->coord.t[0];
                    coord->coord.t[2]   = attachedCoord->coord.t[2] + attachedOffset->coord.t[2];
                    temp_v0_6           = work->grabBiteTimer + 1;
                    work->grabBiteTimer = temp_v0_6;
                    if ((s16)temp_v0_6 == 0x14) {
                        work->grabBiteTimer = 0U;
                        work->grabBiteCount = (u16)work->grabBiteCount + 1;
                        effectSpawnHit(damageGetPlayerAttackEffectId(0x1001), effectCoord, &offset, &work->hitEffect);
                        if (work->grabTargetIndex == 0) {
                            padScriptSpawnVariableMotorRamp(0xA, 0x80U, 0x80U);
                        }
                        id    = (((u16)((Enemy*)actor->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x4010000D;
                        pan25 = (s8)worldCoordGetOriginAudioPan(coord);
                        sndEvtRequestScriptStart(id, (s32)pan25, (s8)worldCoordGetOriginAudioDepth(coord));
                        if (work->grabTargetIndex == 0) {
                            taskMessageDispatch(work->grabTarget, GAME_ACTOR_MESSAGE_APPLY_DAMAGE, damagePackEnemyAttackKey(ctx, 0), 0);
                        }
                        if (config->hp <= 0) {
                            if (work->grabTargetIndex == 0) {
                                work->animRequest             = 9;
                                work->forwardSpeed            = 0;
                                work->animFrame               = 0;
                                reset_y                       = coord->coord.t[1];
                                work->holdingTarget           = 0;
                                work->grabEndHeight           = reset_y;
                                Actor01600_D127D8.animationId = 0;
                                Actor01600_D127D8.blend       = ANIMATION_BLEND_RESET;
                                Actor01600_D127D8.blendFrames = 0;
                                taskMessageDispatch(work->grabTarget, GAME_ACTOR_MESSAGE_END_SCRIPTED, 0, 0);
                                return;
                            }
                        }
                    }
                    if (work->grabTargetIndex == 0) {
                        if (work->animFrame >= 0x31) {
                            if (work->grabBiteCount >= 5) {
                                Actor01600_D127D8.animationId = 3;
                                Actor01600_D127D8.blend       = ANIMATION_BLEND_INTERPOLATE;
                                Actor01600_D127D8.blendFrames = 1;
                                TASK_MESSAGE_DISPATCH_POINTER(work->grabTarget, ANIMATION_MESSAGE_REPLACE_AND_PLAY, &Actor01600_D127D8, 0);
                                work->animPlaying = 0;
                                work->animFrame   = 0;
                                work->animRequest = 0x1E;
                            }
                            if (work->animFrame >= 0x31) {
                                work->animFrame = 0;
                                return;
                            }
                        }
                    } else {
                        if (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.companionHp <= 0) {
                            work->animRequest             = 9;
                            work->forwardSpeed            = 0;
                            work->animFrame               = 0;
                            reset_y2                      = coord->coord.t[1];
                            work->holdingTarget           = 0;
                            work->grabEndHeight           = reset_y2;
                            Actor01600_D127D8.animationId = 0;
                            Actor01600_D127D8.blend       = ANIMATION_BLEND_RESET;
                            Actor01600_D127D8.blendFrames = 0;
                            taskMessageDispatch(work->grabTarget, GAME_ACTOR_MESSAGE_END_SCRIPTED, 0, 0);
                            return;
                        }
                        if (work->animFrame >= 0x31) {
                            work->animFrame   = 0;
                            work->animRequest = 0x1E;
                            return;
                        }
                    }
                    break;
                case 21:
                    if (work->animFrame >= 0x10) {
                        work->forwardSpeed = 0;
                        temp_v1_8          = (u16)work->animFrame;
                        if ((u32)(temp_v1_8 - 0x10) < 2U) {
                            work->forwardSpeed = -0x12C;
                        } else if ((s16)temp_v1_8 < 0x1D) {
                            work->forwardSpeed = -0x64;
                        } else if ((s16)temp_v1_8 < 0x24) {
                            work->forwardSpeed = -0x28;
                        }
                    } else {
                        attachedCoord2    = work->grabTarget->extra.tmd->coords;
                        attachedOffset2   = &attachedCoord2[17];
                        coord->coord.t[0] = attachedCoord2->coord.t[0] + attachedOffset2->coord.t[0];
                        coord->coord.t[2] = attachedCoord2->coord.t[2] + attachedOffset2->coord.t[2];
                    }
                    if (work->animFrame >= 0x2D) {
                        work->holdingTarget           = 0;
                        Actor01600_D127D8.animationId = 0;
                        Actor01600_D127D8.blend       = ANIMATION_BLEND_RESET;
                        Actor01600_D127D8.blendFrames = 0;
                        taskMessageDispatch(work->grabTarget, GAME_ACTOR_MESSAGE_END_SCRIPTED, 0, 0);
                    }
                    if (work->animFrame >= 0x38) {
                        work->animRequest  = 9;
                        work->forwardSpeed = 0;
                        Actor01600_D12870  = 0;
                    }
                    if (work->animFrame == 0x13) {
                        id    = (((u16)((Enemy*)actor->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x4010000B;
                        pan26 = (s8)worldCoordGetOriginAudioPan(coord);
                        sndEvtRequestScriptStart(id, (s32)pan26, (s8)worldCoordGetOriginAudioDepth(coord));
                        if (work->grabTargetIndex == 0) {
                            padScriptSpawnVariableMotorRamp(0xA, 0xD0U, 0xD0U);
                        }
                    }
                    if (work->animFrame == 0x1F) {
                        id    = (((u16)((Enemy*)actor->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x4010000C;
                        pan27 = (s8)worldCoordGetOriginAudioPan(coord);
                        sndEvtRequestScriptStart(id, (s32)pan27, (s8)worldCoordGetOriginAudioDepth(coord));
                        id    = (((u16)((Enemy*)actor->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x40100001;
                        pan28 = (s8)worldCoordGetOriginAudioPan(coord);
                        sndEvtRequestScriptStart(id, (s32)pan28, (s8)worldCoordGetOriginAudioDepth(coord));
                    }
                    if (work->animFrame == 0x21) {
                        id    = (((u16)((Enemy*)actor->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x40100002;
                        pan29 = (s8)worldCoordGetOriginAudioPan(coord);
                        sndEvtRequestScriptStart(id, (s32)pan29, (s8)worldCoordGetOriginAudioDepth(coord));
                        return;
                    }
                    break;
                case 0:
                    if (work->animFrame == 1) {
                        id    = (((u16)((Enemy*)actor->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x4010000E;
                        pan30 = (s8)worldCoordGetOriginAudioPan(coord);
                        sndEvtRequestScriptStart(id, (s32)pan30, (s8)worldCoordGetOriginAudioDepth(coord));
                    }
                    if (work->animFrame >= 0x25) {
                        work->attackAction    = ACTOR_01600_ACTION_ADVANCE;
                        work->animFrame       = 0;
                        work->shadowUnderBody = 0;
                        return;
                    }
                    break;
                case 18:
                    work->forwardSpeed    = 0x28;
                    work->shadowUnderBody = 0;
                    work->animBlendFrames = 4;
                    if (work->animFrame >= 0xD) {
                        work->forwardSpeed = 0;
                    }
                    if (work->animFrame == 0xD) {
                        id    = (((u16)((Enemy*)actor->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x40100005;
                        pan31 = (s8)worldCoordGetOriginAudioPan(coord);
                        sndEvtRequestScriptStart(id, (s32)pan31, (s8)worldCoordGetOriginAudioDepth(coord));
                    }
                    if (work->animFrame >= 0x30) {
                        work->animRequest  = 0x15;
                        work->forwardSpeed = 0;
                        work->animFrame    = 0;
                    }
                    break;
                case 12:
                    work->shadowUnderBody = 0;
                    if (work->animFrame == 0x1F) {
                        id    = (((u16)((Enemy*)actor->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x40100001;
                        pan32 = (s8)worldCoordGetOriginAudioPan(coord);
                        sndEvtRequestScriptStart(id, (s32)pan32, (s8)worldCoordGetOriginAudioDepth(coord));
                        id    = (((u16)((Enemy*)actor->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x40100002;
                        pan33 = (s8)worldCoordGetOriginAudioPan(coord);
                        sndEvtRequestScriptStart(id, (s32)pan33, (s8)worldCoordGetOriginAudioDepth(coord));
                    }
                    if (work->animFrame >= 0x2F) {
                        work->forwardSpeed    = 0;
                        work->attackAction    = ACTOR_01600_ACTION_ADVANCE;
                        work->animFrame       = 0;
                        work->shadowUnderBody = 0;
                        return;
                    }
                    break;
            }
            break;
        case ACTOR_01600_ACTION_KNOCKBACK:
            work->animRate        = 0x14;
            work->animBlendFrames = 0;
            switch (work->animRequest) {
                case 11:
                case 14:
                    temp_v0_7 = work->animFrame;
                    if (temp_v0_7 <= 0) {
                        work->bodySphere.body.pos.vy = -0x258;
                        work->bodySphere.body.radius = 0x258;
                        work->forwardSpeed           = 0x1F4;
                    } else if (temp_v0_7 < 0x13) {
                        work->bodySphere.body.pos.vy = -0x190;
                        work->bodySphere.body.radius = 0x190;
                        work->forwardSpeed           = 0x4B;
                    } else {
                        work->forwardSpeed = 0;
                    }
                    if (work->animRequest == 0xE) {
                        neg_velocity       = -work->forwardSpeed;
                        work->forwardSpeed = neg_velocity;
                    }
                    if (work->airborne != 0) {
                        work->verticalSpeed += 0xF;
                        work->jumpHeight    += work->verticalSpeed;
                        if (work->jumpHeight >= 0) {
                            id    = (((u16)((Enemy*)actor->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x40100005;
                            pan36 = (s8)worldCoordGetOriginAudioPan(coord);
                            sndEvtRequestScriptStart(id, (s32)pan36, (s8)worldCoordGetOriginAudioDepth(coord));
                            work->airborne   = 0;
                            work->jumpHeight = 0;
                        }
                    }
                    if (work->animFrame >= 0x28) {
                        if (work->animRequest == 0xE) {
                            work->animRequest = 0x16;
                        } else {
                            work->animRequest = 0x15;
                        }
                        work->animFrame     = 0;
                        work->forwardSpeed  = 0;
                        work->airborne      = 0;
                        work->verticalSpeed = 0;
                        work->jumpHeight    = 0;
                        work->shadowHidden  = 0;
                        return;
                    }
                    break;
                case 21:
                case 22:
                    work->animRate     = 0x10;
                    work->forwardSpeed = 0;
                    if (work->animFrame == 0x19) {
                        id    = (((u16)((Enemy*)actor->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x40100002;
                        pan34 = (s8)worldCoordGetOriginAudioPan(coord);
                        sndEvtRequestScriptStart(id, (s32)pan34, (s8)worldCoordGetOriginAudioDepth(coord));
                    }
                    if (work->animFrame == 0x1B) {
                        id    = (((u16)((Enemy*)actor->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x40100001;
                        pan35 = (s8)worldCoordGetOriginAudioPan(coord);
                        sndEvtRequestScriptStart(id, (s32)pan35, (s8)worldCoordGetOriginAudioDepth(coord));
                    }
                    if (work->animFrame >= 0x2F) {
                        work->attackAction = ACTOR_01600_ACTION_ADVANCE;
                        work->animFrame    = 0;
                    }
                    break;
                default:
                    break;
            }
            break;
    }
}

/// Steps the scavenger's facing according to its target, random or requested turn mode.
///
/// Yaws use 4096 units per turn. Rebuilds the root's rotation from yaw alone,
/// leaving translation intact. The selected player task must exist even in hold mode.
static void _actor01600StepTurn(Task* actor)
{
    Task**            targetSlot;
    Task**            playerTasks;
    s16               turnMode;
    s16               yawDifference;
    s32               currentYaw;
    s16               steppedYaw;
    s32               yawDistance;
    s32               randomTurn;
    s16               turnStep;
    s32               randomStep;
    s16               wrappedDifference;
    s32               requestedStep;
    s32               randomRemaining;
    s32               requestedRemaining;
    u16               targetYaw;
    u32               randomState;
    u32               stepRandomState;
    u32               randomBits;
    GfxCoord*         targetCoord;
    _Actor01600Work*  work;
    GfxCoord*         rootCoord;
    ActorFaceScratch* scratch;

    work        = actor->work;
    rootCoord   = actor->extra.tmd->coords;
    playerTasks = gPlayerActorTasks;
    targetSlot  = &playerTasks[_actor01600SelectNearestPlayer(actor) & 0xFF];
    scratch     = SCRATCH_STACK_RESERVE_BLOCK(ActorFaceScratch);
    turnMode    = work->turnMode;
    targetCoord = (*targetSlot)->extra.tmd->coords;
    switch (turnMode) {
        case ACTOR_01600_TURN_TRACK_TARGET:
            scratch->delta.vx = targetCoord->coord.t[0] - rootCoord->coord.t[0];
            scratch->delta.vy = 0;
            scratch->delta.vz = targetCoord->coord.t[2] - rootCoord->coord.t[2];
            targetYaw         = ratan2((s16)scratch->delta.vx, (s16)scratch->delta.vz) & ACTOR_TRANSFORM_ANGLE_MASK;
            yawDifference     = targetYaw - (work->yaw & ACTOR_TRANSFORM_ANGLE_MASK);
            yawDistance       = yawDifference >= 0 ? yawDifference : -yawDifference;
            turnStep          = yawDifference;
            if (yawDistance < (ACTOR_01600_MIN_TURN_STEP + 1)) {
                work->yaw = targetYaw;
            } else {
                if (yawDistance >= (ACTOR_TRANSFORM_ANGLE_HALF_TURN + 1)) {
                    wrappedDifference = yawDifference - ACTOR_TRANSFORM_ANGLE_TURN;
                    if (yawDifference <= 0) {
                        wrappedDifference = ACTOR_TRANSFORM_ANGLE_TURN - yawDifference;
                    }
                    turnStep = wrappedDifference;
                }
                currentYaw = work->yaw;
                if (turnStep > 0) {
                    work->yaw = (u16)(currentYaw + ACTOR_01600_MIN_TURN_STEP);
                } else {
                    work->yaw = (u16)(currentYaw - ACTOR_01600_MIN_TURN_STEP);
                }
            }
            break;
        case ACTOR_01600_TURN_RANDOM_BEGIN:
            randomState     = (gRandomLcgState * RANDOM_LCG_MULTIPLIER) + RANDOM_LCG_INCREMENT;
            randomBits      = randomState >> 0x10;
            randomTurn      = (randomBits & 0x3FF) + 0x400;
            gRandomLcgState = randomState;
            if (randomBits & 0x400) {
                randomTurn = -randomTurn;
            }
            work->turnRemaining = randomTurn;
            work->turnMode      = ACTOR_01600_TURN_RANDOM;
            break;
        case ACTOR_01600_TURN_RANDOM:
            stepRandomState = (gRandomLcgState * RANDOM_LCG_MULTIPLIER) + RANDOM_LCG_INCREMENT;
            randomStep      = (stepRandomState >> 0x10) & 0x17;
            gRandomLcgState = stepRandomState;
            if (work->turnRemaining <= 0) {
                randomStep = -randomStep;
            }
            turnStep        = randomStep;
            work->yaw       = (u16)(work->yaw + turnStep);
            randomRemaining = work->turnRemaining - turnStep;
            if ((randomRemaining >= 0 ? randomRemaining : -randomRemaining) < ACTOR_01600_MIN_TURN_STEP) {
                work->turnMode      = ACTOR_01600_TURN_TRACK_TARGET;
                work->turnRemaining = 0;
            } else {
                work->turnRemaining -= turnStep;
            }
            break;
        case ACTOR_01600_TURN_HOLD:
            break;
        case ACTOR_01600_TURN_REQUEST_BEGIN:
            work->turnMode      = ACTOR_01600_TURN_REQUEST;
            work->turnRemaining = (s16)work->turnRequest;
            break;
        case ACTOR_01600_TURN_REQUEST:
            requestedStep = work->turnRate;
            if (work->turnRemaining <= 0) {
                requestedStep = -requestedStep;
            }
            turnStep   = requestedStep;
            steppedYaw = work->yaw + requestedStep;
            work->yaw  = (u16)steppedYaw;
            if (steppedYaw >= (ACTOR_TRANSFORM_ANGLE_HALF_TURN + 1)) {
                work->yaw = steppedYaw - ACTOR_TRANSFORM_ANGLE_TURN;
            } else if (steppedYaw < -ACTOR_TRANSFORM_ANGLE_HALF_TURN) {
                work->yaw = steppedYaw + ACTOR_TRANSFORM_ANGLE_TURN;
            }
            requestedRemaining = work->turnRemaining - turnStep;
            if ((requestedRemaining >= 0 ? requestedRemaining : -requestedRemaining) < work->turnRate) {
                work->turnMode      = ACTOR_01600_TURN_HOLD;
                work->turnRemaining = 0;
            } else {
                work->turnRemaining -= turnStep;
            }
            break;
    }
    // Rebuild only the facing rotation; pitch and roll do not carry through this turn.
    scratch->rot.vx = 0;
    scratch->rot.vy = (u16)work->yaw;
    scratch->rot.vz = 0;
    RotMatrix(&scratch->rot, &rootCoord->coord);
    SCRATCH_STACK_RELEASE_BLOCK(ActorFaceScratch);
}

/// Starts a changed animation request or ticks the eight model-part slots already playing.
///
/// Rates use sixteenths and blend lengths use frames. The no-animation sentinel
/// leaves playback untouched. Selected clips have their body-part XZ translation
/// suppressed or offset after playback; the root is not moved here.
static void _actor01600StepAnimation(Task* actor)
{
    _Actor01600Work* work;
    GfxCoord*        rootCoord;
    s16              animationId;
    s32              partIndex;

    work = actor->work;

    if (work->animRequest != ACTOR_01600_ANIMATION_NONE) {
        if (work->animRequest != work->animPlaying) {
            work->animPlaying = work->animRequest;
            work->animFrame   = 0;
            for (partIndex = 1; partIndex < ARRAY_SIZE(work->slots); partIndex++) {
                animationSeekSlotWithBlend(&work->anim, partIndex, work->animRequest, 0, work->animBlendFrames);
            }
        } else {
            work->animFrame = (u16)work->animFrame + 1;
            for (partIndex = 1; partIndex < ARRAY_SIZE(work->slots); partIndex++) {
                work->slots[partIndex].rate = work->animRate;
                animationTickSlot(&work->anim, partIndex);
            }
        }
        // Keep locomotion in the root instead of the selected clips' body-part translation.
        animationId = work->animRequest;
        if (animationId == 28 || animationId == 30 || animationId == 9 || animationId == 21 || animationId == 22 || animationId == 5 || animationId == 6 || animationId == 27) {
            actor->extra.tmd->coords[1].coord.t[0] = 0;
            actor->extra.tmd->coords[1].coord.t[2] = 0;
            actor->extra.tmd->coords[1].coord.t[0] = 0;
            actor->extra.tmd->coords[1].coord.t[2] = 0;
            return;
        } else if (animationId == 29) {
            actor->extra.tmd->coords[1].coord.t[0] = 0;
            rootCoord                              = actor->extra.tmd->coords;
            rootCoord[1].coord.t[2]                = rootCoord[1].coord.t[2] - 0x2BC;
        }
    }
}

static void Actor01600_Fn03EEC(Task* arg0)
{
    VECTOR3                         pos;
    _Actor01600GroundShadowScratch* scratch;
    _Actor01600Work*                work;
    GfxCoord*                       coord;
    s32                             height;

    work  = arg0->work;
    coord = arg0->extra.tmd->coords;
    if (work->shadowUnderBody == 0) {
        scratch = SCRATCH_STACK_RESERVE_BLOCK(_Actor01600GroundShadowScratch);
        if (work->airborne != 0) {
            gte_SetRotMatrix(&coord->workm);
            scratch->drop.vx = 0;
            height           = work->jumpHeight - 0x80;
            scratch->drop.vy = -height;
            scratch->drop.vz = 0;
            gte_ldv0(&scratch->drop);
            gte_rtv0();
            gte_stlvnl(&scratch->centre);
            scratch->centre.vx += coord->workm.t[0];
            scratch->centre.vy += coord->workm.t[1];
            scratch->centre.vz += coord->workm.t[2];
        } else {
            scratch->centre.vx = coord->workm.t[0];
            scratch->centre.vy = coord->workm.t[1];
            scratch->centre.vz = coord->workm.t[2];
        }
        effectDrawGroundShadow(&scratch->centre, 0x1C0, 0);
        SCRATCH_STACK_RELEASE_BLOCK(_Actor01600GroundShadowScratch);
        return;
    }
    if (worldCollisionProjectGroundPoint(MATRIX_TRANS(&coord[1].workm), &pos) != 0) {
        effectDrawGroundShadow(&pos, 0x1C0, gRoomEffectState->groundShadowShade);
    }
}

static void Actor01600_Fn04054(Enemy* arg0, Task* arg1)
{
    _Actor01600Work*  work;
    TmdObject*        obj;
    GfxCoord*         coords;
    GfxCoord*         attach;
    GfxCoord*         body;
    _Actor01600Work*  w;
    SceneCombatState* state;
    AttachmentState*  attachment;
    s32               unusedDistance;
    s32               dist;
    s32               anim;
    s16               phase;
    s16               timer;
    s16               count;
    s32               mode;

    obj    = arg1->extra.tmd;
    work   = arg1->work;
    coords = obj->coords;
    mode   = gSceneCombatState.actorControl;
    if (mode == 1) {
        return;
    }
    if (mode > 1) {
        if (mode == 2) {
            obj->flags                  |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
            arg0->node.state.parts.flags = WORLD_TARGET_NOT_LOCKABLE;
            return;
        }
    }
    switch (work->deathPhase) {
        case ACTOR_01600_DEATH_BEGIN:
            _actor01600ReleaseGrab(arg1);
            if (work->burstState != 2) {
                anim                  = work->behavior;
                work->animBlendFrames = 0;
                work->animRate        = 0x14;
                if (anim != ACTOR_01600_BEHAVIOR_STAGGER && anim != ACTOR_01600_BEHAVIOR_BUILDUP_DOWN) {
                    dist = _actor01600MeasureTarget(arg1, &unusedDistance);
                    if (dist < 0) {
                        dist = -dist;
                    }
                    work->animRequest = (dist < 0x400) ? 0xE : 0xB;
                }
                work->stateTimer  = 0;
                work->deathScaleY = ONE;
                work->deathMatrix = coords[0].coord;
                worldCoordSetActorColorMode(arg0, ENEMY_COLOR_WEIGHTED);
            }
            arg0->node.state.parts.flags = WORLD_TARGET_NOT_LOCKABLE;
            arg0->recs                   = 0;
            worldTargetUnlinkNode(&arg0->node);
            worldCollisionUnlinkBody(&work->pathProbe.body);
            worldCollisionUnlinkBody(&work->sight.body);
            worldCollisionUnlinkBody(&work->bodySphere.body);
            worldCollisionUnlinkBody(&work->bite.body);
            state = &gSceneCombatState;
            if (state->actor01600Wave >= 3) {
                if (Actor01600_Fn06F78() == 1) {
                    state->actor01600Wave = state->actor01600Wave + 1;
                }
            } else {
                sceneReleaseBattleRefWithRewards(arg1, 0x10);
            }
            work->deathPhase = ACTOR_01600_DEATH_COLLAPSE;
            break;
        case ACTOR_01600_DEATH_COLLAPSE:
            Actor01600_Fn06880(arg1);
            phase            = work->stateTimer + 1;
            work->stateTimer = phase;
            if (work->burstState != 2) {
                if (phase == 0xA) {
                    obj->flags = TMD_OBJECT_SEMI_TRANS;
                }
                if (work->stateTimer == 0xF) {
                    effectSpawn(EFFECT_CORPSE_BURN, coords, 1, NULL);
                }
                if (work->stateTimer < 0x10) {
                    body = arg1->extra.tmd->coords;
                    w    = arg1->work;

                    w->previousPosition.vx = body->coord.t[0];
                    w->previousPosition.vy = body->coord.t[1];
                    w->previousPosition.vz = body->coord.t[2];

                    body->coord.t[0] += (body->coord.m[0][2] * w->forwardSpeed) >> 12;
                    body->coord.t[2] += (body->coord.m[2][2] * w->forwardSpeed) >> 12;
                    if (w->suspended == 0) {
                        if (w->airborne != 0) {
                            body->coord.t[1] += w->verticalSpeed;
                        } else {
                            body->coord.t[1] += 0x80;
                        }
                    }
                }
            }
            if (work->stateTimer >= 0x3C) {
                work->deathPhase = ACTOR_01600_DEATH_REMOVE;
            }
            break;
        case ACTOR_01600_DEATH_REMOVE:
            if (gSceneCombatState.actor01600Wave >= 3) {
                if (Actor01600_D12874 == 1) {
                    attachment = &Gp_StateC08;
                    if (attachment->mode == ATTACHMENT_MODE_WHEEL) {
                        break;
                    }
                    if (gDisplayState.pendingMode != DISPLAY_MODE_NONE) {
                        break;
                    }
                    attachment->flags |= ATTACHMENT_FLAG_EVENT_LOCK;
                    roomEffectRequestCancelAll();
                    TASK_MESSAGE_DISPATCH_POINTER(gameGetTaskSlot(GAME_TASK_SLOT_ROOM), 0x13F4, arg1, 0);
                    work->deathPhase        = ACTOR_01600_DEATH_REPORTED;
                    arg1->extra.tmd->flags |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
                    arg1->extra.tmd->flags |= TMD_OBJECT_SKIP_AUTO_BUFFER;
                    break;
                }
                sceneReleaseBattleRefWithRewards(arg1, 0x10);
            }
            work->exitTimer         = 0x3C;
            arg1->extra.tmd->flags |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
            Actor01600_D12874--;
            arg1->extra.tmd->flags |= TMD_OBJECT_SKIP_AUTO_BUFFER;
            work->deathPhase        = ACTOR_01600_DEATH_EXIT;
            break;
        case ACTOR_01600_DEATH_EXIT:
            timer           = work->exitTimer - 1;
            work->exitTimer = timer;
            if (timer == 0) {
                _actor01600Exit(arg1);
            }
            return;
        default:
            return;
    }

    _actor01600StepAnimation(arg1);
    coords->composeStamp = GRAPHICS_COORD_DIRTY;
    actorRenderComposeCoord(coords);
    count            = work->colorTimer + 1;
    work->colorTimer = count;
    if (count < 5 && gGameSession->viewDirty != 1) {
        return;
    }
    work->colorTimer = 0;

    attach = &arg1->extra.tmd->coords[1];
    update_actor_color(arg0, attach);
}

/// Returns the nearer player actor's relative yaw and writes its horizontal distance.
///
/// Requires a live selected player task and a writable `horizontalDistance` word.
/// Yaw uses composed world matrices, is wrapped to -2048..2048, and uses 4096
/// units per turn with signed-halfword offsets. Distance uses full-width root
/// translations in their common parent frame, in world coordinate units.
static s32 _actor01600MeasureTarget(Task* actor, s32* horizontalDistance)
{
    GfxCoord*                 rootCoord;
    GfxCoord*                 targetCoord;
    ActorRangeBearingScratch* scratch;
    s32                       relativeYaw;

    targetCoord         = gPlayerActorTasks[_actor01600SelectNearestPlayer(actor) & 0xFF]->extra.tmd->coords;
    rootCoord           = actor->extra.tmd->coords;
    scratch             = SCRATCH_STACK_RESERVE_BLOCK(ActorRangeBearingScratch);
    relativeYaw         = _actorAngleBearingInFrame(&scratch->bearing, rootCoord, targetCoord);
    scratch->offset.vx  = targetCoord->coord.t[0] - rootCoord->coord.t[0];
    scratch->offset.vy  = targetCoord->coord.t[1] - rootCoord->coord.t[1];
    scratch->offset.vz  = targetCoord->coord.t[2] - rootCoord->coord.t[2];
    *horizontalDistance = SquareRoot0(scratch->offset.vx * scratch->offset.vx + scratch->offset.vz * scratch->offset.vz);
    SCRATCH_STACK_RELEASE_BLOCK(ActorRangeBearingScratch);
    return relativeYaw;
}

static s32 Actor01600_Fn047A0(Task* arg0)
{
    SVECTOR3         delta;
    s32              distance;
    _Actor01600Work* work;
    GfxCoord*        coord;
    Task*            task;
    s16              angle;
    s16              heading;
    s32              difference;
    GfxCoord*        other;

    work  = arg0->work;
    task  = work->grabTarget;
    other = task->extra.tmd->coords;
    coord = arg0->extra.tmd->coords;
    if (((GameActor*)gPlayerActorTasks[work->grabTargetIndex]->work)->mode != GAME_ACTOR_MODE_SCRIPTED) {
        if (Actor01600_D12870 != 1) {
            difference = _actor01600MeasureTarget(arg0, &distance);
            if (difference < 0) {
                difference = -difference;
            }
            if (difference < 0x401) {
                if (distance < 0x3E8) {
                    Actor01600_D12878.pressCount = 5;
                    if (work->grabTargetIndex != 0) {
                        Actor01600_D12878.animation.animationId = 1;
                    } else {
                        Actor01600_D12878.animation.animationId = 2;
                    }
                    if (TASK_MESSAGE_DISPATCH_POINTER(task, GAME_ACTOR_MESSAGE_AWAIT_BUTTON_PRESSES, &Actor01600_D12878, 0) == 0) {
                        other->composeStamp = GRAPHICS_COORD_DIRTY;
                        delta.vx            = coord->coord.t[0] - other->coord.t[0];
                        delta.vy            = 0;
                        delta.vz            = coord->coord.t[2] - other->coord.t[2];
                        angle               = ratan2(delta.vx, delta.vz);
                        heading             = angle;
                        if (angle >= 0x801) {
                            heading = angle - 0x1000;
                        } else if (angle < -0x800) {
                            heading = angle + 0x1000;
                        }
                        Actor01600_D12890.rot.vx = 0;
                        Actor01600_D12890.rot.vy = heading;
                        Actor01600_D12890.rot.vz = 0;
                        Actor01600_D12890.pos.vx = (s32)other->coord.t[0];
                        Actor01600_D12890.pos.vy = (s32)other->coord.t[1];
                        Actor01600_D12890.pos.vz = (s32)other->coord.t[2];
                        TASK_MESSAGE_DISPATCH_POINTER(task, 0x3E9, &Actor01600_D12890, 0);
                        Actor01600_D12870 = 1;
                        return 1;
                    }
                    return 0;
                }
                return 0;
            }
            return 0;
        }
        return 0;
    }
    return 0;
}

static s32 Actor01600_Fn04974(Task* actor, s32 angle, s32 distance, s32 flags)
{
    _Actor01600Work* work;
    GfxCoord*        coord;
    GfxCoord*        other;
    s32              difference;
    s32              angleAbs;
    s32              done;
    s32              otherY;
    s32              tmp;

    work  = actor->work;
    coord = actor->extra.tmd->coords;
    other = (*gPlayerActorTasks)->extra.tmd->coords;
    if (((GameActor*)gPlayerActorTasks[flags]->work)->mode != GAME_ACTOR_MODE_SCRIPTED) {
        if (Actor01600_D12870 == 0) {
            otherY     = other->coord.t[1];
            tmp        = coord->coord.t[1];
            difference = otherY - tmp;
            if (difference < 0) {
                difference = -difference;
            }
            if (difference < 0x191) {
                if (distance < 0x3E9) {
                    tmp      = angle >= 0;
                    angleAbs = tmp ? angle : -angle;
                    if (angleAbs < 0x101) {
                        work->pathProbe.body.flags |= (WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED);
                        _actor01600SearchClearHeading(actor);
                        if (work->pathSearchPhase >= ACTOR_01600_PATH_SEARCH_BEGIN_SWEEP) {
                            work->animRequest       = 0x19;
                            work->alerted           = 0;
                            work->behavior          = ACTOR_01600_BEHAVIOR_ROAM;
                            work->pathSearchPhase   = ACTOR_01600_PATH_SEARCH_PROBE_TARGET;
                            work->field_514         = 1;
                            work->sight.body.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ (WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED));
                            done                    = 1;
                        } else {
                            done = 0;
                        }
                        if ((u8)done) {
                            return 1;
                        }
                        work->attackAction    = ACTOR_01600_ACTION_GRAB;
                        work->animRequest     = 0x1C;
                        work->field_52C       = 0;
                        work->grabTargetIndex = flags;
                        work->grabTarget      = gPlayerActorTasks[flags];
                        return 1;
                    }
                    return 0;
                }
                return 0;
            }
            return 0;
        }
        return 0;
    }
    return 0;
}

static void Actor01600_Fn04AD8(Task* arg0)
{
    Enemy*           ctx;
    _Actor01600Work* work;
    GfxCoord*        body;
    s16              state;
    u16              count;
    u16              count2;

    work  = arg0->work;
    ctx   = arg0->spawnArg2.pointer;
    state = work->targetAnchorPhase;
    body  = arg0->extra.tmd->coords;
    switch (state) {
        case ACTOR_01600_TARGET_ANCHOR_FOLLOW:
            work->targetAnchor.coord.t[0] = body->coord.t[0];
            work->targetAnchor.coord.t[2] = body->coord.t[2];
            ctx->coord                    = body;
            break;
        case ACTOR_01600_TARGET_ANCHOR_DETACH:
            work->targetAnchor.coord.t[0] = body->coord.t[0];
            work->targetAnchor.coord.t[2] = body->coord.t[2];
            ctx->coord                    = &work->targetAnchor;
            work->targetAnchorTimer       = 0U;
            work->targetAnchorPhase       = (u16)work->targetAnchorPhase + 1;
            break;
        case ACTOR_01600_TARGET_ANCHOR_HOLD:
            count                   = work->targetAnchorTimer + 1;
            work->targetAnchorTimer = count;
            if ((s16)count < 8) {
                break;
            }
            work->targetAnchorTimer = 0U;
            work->targetAnchorPhase = (u16)work->targetAnchorPhase + 1;
            break;
        case ACTOR_01600_TARGET_ANCHOR_AIM:
            work->targetAnchorStepX = (s16)((body->coord.t[0] - work->targetAnchor.coord.t[0]) / 5);
            work->targetAnchorStepZ = (s16)((body->coord.t[2] - work->targetAnchor.coord.t[2]) / 5);
            work->targetAnchorPhase = (u16)work->targetAnchorPhase + 1;
            break;
        case ACTOR_01600_TARGET_ANCHOR_CATCH_UP:
            work->targetAnchor.coord.t[0] += work->targetAnchorStepX;
            work->targetAnchor.coord.t[2] += work->targetAnchorStepZ;
            count2                         = work->targetAnchorTimer + 1;
            work->targetAnchorTimer        = count2;
            if ((s16)count2 >= 5) {
                work->targetAnchorTimer = 0U;
                work->targetAnchorPhase = ACTOR_01600_TARGET_ANCHOR_FOLLOW;
            }
            break;
    }
    work->targetAnchor.coord.t[1]   = body->coord.t[1];
    work->targetAnchor.composeStamp = GRAPHICS_COORD_DIRTY;
    actorRenderComposeCoord(&work->targetAnchor);
}

/// Turns a root-local probe reach about Y using the current stored probe yaw.
///
/// `scratch->reach` must be initialized; `yaw` borrows a readable signed
/// halfword outside the block, in 4096-unit turns. Writes the far end and
/// rotation basis, leaving translation untouched, and overwrites GTE rotation registers.
static __inline__ void _actor01600RotateProbeReach(_Actor01600PathProbeAimScratch* scratch, const s16* yaw)
{
    MATRIX*  rotation;
    SVECTOR* farEnd;

    rotation = &scratch->rotation;
    gfxSetRotIdentity(rotation);
    RotMatrixY(*yaw, rotation);
    farEnd = &scratch->farEnd;
    gte_SetRotMatrix(rotation);
    gte_ldv0(&scratch->reach);
    gte_rtv0();
    gte_stsv(farEnd);
}

/// Reads the previous probe contact and aims the capsule for the next collision pass.
///
/// `reachDistance` is in world units, narrowed to a signed halfword. Nonzero
/// `relativeYaw` selects a directed probe in 4096-unit turns; zero instead advances
/// the ten-degree sweep and records its clear arcs. Even a target dead ahead
/// takes that sweep branch. Returns 1 for a clear directed probe or a finished
/// sweep, then clears the consumed contact. Calls require a valid clear-arc index.
static s32 _actor01600StepPathProbe(Task* actor, s32 reachDistance, s32 relativeYaw)
{
    _Actor01600PathProbeAimScratch* scratch;
    _Actor01600Work*                work;
    s16                             openArcIndex;
    s16                             blockedArcIndex;
    s16                             nextArcIndex;
    s16                             sweepDegrees;
    s32                             scaledSweepDegrees;
    s32                             startYaw;
    s32                             probeComplete;

    probeComplete     = 0;
    work              = actor->work;
    scratch           = SCRATCH_STACK_RESERVE_BLOCK(_Actor01600PathProbeAimScratch);
    scratch->reach.vx = 0;
    scratch->reach.vy = 0;
    scratch->reach.vz = reachDistance;
    if (relativeYaw == 0) {
        sweepDegrees       = (u16)work->sweepDegrees + ACTOR_01600_PROBE_DEGREE_STEP;
        scaledSweepDegrees = sweepDegrees << 0x10;
        probeComplete      = scaledSweepDegrees > (ACTOR_01600_PROBE_LAST_DEGREE << 16);
        work->probeYaw     = (u16)work->probeYaw + ACTOR_01600_PROBE_YAW_STEP;
        work->sweepDegrees = sweepDegrees;
    } else {
        work->probeYaw = (s16)relativeYaw;
        if ((s16)relativeYaw >= (ACTOR_TRANSFORM_ANGLE_HALF_TURN + 1)) {
            work->probeYaw = relativeYaw - ACTOR_TRANSFORM_ANGLE_TURN;
        } else if ((s16)relativeYaw < -ACTOR_TRANSFORM_ANGLE_HALF_TURN) {
            work->probeYaw = relativeYaw + ACTOR_TRANSFORM_ANGLE_TURN;
        }
    }
    // Turn the probe's length about Y to its yaw; the turned length is the capsule's far end.
    _actor01600RotateProbeReach(scratch, &work->probeYaw);
    work->pathProbe.shape.ends[0].vx = scratch->farEnd.vx;
    work->pathProbe.shape.ends[0].vz = scratch->farEnd.vz;
    // This contact belongs to the previous aim: the collision pass has not tested the new one yet.
    if (work->pathProbe.contacts[0].key.parts.kind != (WORLD_COLLISION_CONTACT_GRID >> 16)) {
        if (relativeYaw == 0) {
            openArcIndex = work->clearArcCount;
            if (work->clearArcs[openArcIndex].startYaw == ACTOR_01600_CLEAR_ARC_UNSET) {
                work->clearArcs[openArcIndex].startYaw = work->probeYaw;
            } else {
                work->clearArcs[openArcIndex].endYaw = work->probeYaw;
                if (probeComplete == 1) {
                    work->clearArcCount = (u16)work->clearArcCount + 1;
                }
            }
        } else {
            probeComplete = 1;
        }
    } else if (relativeYaw == 0) {
        blockedArcIndex = work->clearArcCount;
        startYaw        = work->clearArcs[blockedArcIndex].startYaw;
        if (startYaw != ACTOR_01600_CLEAR_ARC_UNSET) {
            // The probe is blocked: close the open arc, one step wide if it never got a second clear step.
            if (work->clearArcs[blockedArcIndex].endYaw == ACTOR_01600_CLEAR_ARC_UNSET) {
                work->clearArcs[blockedArcIndex].endYaw = startYaw;
            }
            nextArcIndex        = (u16)work->clearArcCount + 1;
            work->clearArcCount = nextArcIndex;
            if (nextArcIndex >= ARRAY_SIZE(work->clearArcs) - 1) {
                work->clearArcCount = ARRAY_SIZE(work->clearArcs) - 1;
            }
        }
    }
    worldCollisionClearContacts(work->pathProbe.contacts);
    SCRATCH_STACK_RELEASE_BLOCK(_Actor01600PathProbeAimScratch);
    return probeComplete;
}

/// Advances the frame-delayed search for a clear heading toward the nearer player actor.
///
/// The path probe's grid tests must run between calls. Returns 0 while pending,
/// 1 for a direct heading, 2 for a swept arc, or 0xFF when the sweep found no arc.
/// Bit 0x80 accompanies the direct/arc result as the turn-animation direction.
/// Stores the signed turn and its per-frame rate in the work block.
static u8 _actor01600SearchClearHeading(Task* actor)
{
    SVECTOR          facingAxis;
    s32              targetDistance;
    _Actor01600Work* work;
    s32              directionFlags;
    s32              targetYaw;
    s32              turnMagnitude;
    s32              clearArcCount;
    s32              wrappedDistance;
    s32              arcSpanOrEndDistance;
    s32              arcMidpointOrStartDistance;
    s32              bestDistance;
    s32              candidateDirection;
    s32              arcIndex;
    s32              arcCount;
    s32              result; // Shared bearing, probe verdict and turn-direction result
    s32              fullTurn;
    s32              candidateDistance;

    work           = actor->work;
    directionFlags = 0;
    switch (work->pathSearchPhase) {
        case ACTOR_01600_PATH_SEARCH_PROBE_TARGET:
            result = _actor01600MeasureTarget(actor, &targetDistance);
            _actor01600StepPathProbe(actor, targetDistance, result);
            work->turnRequest = result;
            work->pathSearchPhase++;
            break;
        case ACTOR_01600_PATH_SEARCH_CHECK_TARGET:
            work->turnRequest = _actor01600MeasureTarget(actor, &targetDistance);
            result            = _actor01600StepPathProbe(actor, targetDistance, work->turnRequest);
            if (result & 0xFF) {
                gfxReadMatrixZAxis(&actor->extra.tmd->coords->coord, &facingAxis);
                ratan2(facingAxis.vx, facingAxis.vz);
                turnMagnitude   = __builtin_abs(work->turnRequest);
                wrappedDistance = ACTOR_TRANSFORM_ANGLE_TURN - turnMagnitude;
                if (wrappedDistance < turnMagnitude) {
                    result    = 0;
                    targetYaw = wrappedDistance;
                } else {
                    result    = ACTOR_01600_HEADING_DIRECTION;
                    targetYaw = turnMagnitude;
                }
                work->turnRate = targetYaw / 16;
                if (work->turnRate < ACTOR_01600_MIN_TURN_STEP)
                    work->turnRate = ACTOR_01600_MIN_TURN_STEP;
                return result | ACTOR_01600_HEADING_DIRECT;
            }
            work->pathSearchPhase++;
            break;
        case ACTOR_01600_PATH_SEARCH_BEGIN_SWEEP:
            // Seed the first sample at zero yaw and empty the full eight-arc table.
            work->probeYaw      = -ACTOR_01600_PROBE_YAW_STEP;
            work->clearArcCount = 0;
            work->sweepDegrees  = -ACTOR_01600_PROBE_DEGREE_STEP;
            for (arcIndex = 0; arcIndex < ARRAY_SIZE(work->clearArcs); arcIndex++) {
                work->clearArcs[arcIndex].startYaw = ACTOR_01600_CLEAR_ARC_UNSET;
                work->clearArcs[arcIndex].endYaw   = ACTOR_01600_CLEAR_ARC_UNSET;
            }
            _actor01600StepPathProbe(actor, ACTOR_01600_PROBE_REACH, 0);
            work->pathSearchPhase++;
            break;
        case ACTOR_01600_PATH_SEARCH_SWEEP:
            result = _actor01600StepPathProbe(actor, ACTOR_01600_PROBE_REACH, 0);
            if (result & 0xFF) {
                if (!work->clearArcCount)
                    return ACTOR_01600_HEADING_BLOCKED;
                work->pathSearchPhase++;
            } else
                return ACTOR_01600_HEADING_PENDING;
            break;
        case ACTOR_01600_PATH_SEARCH_CHOOSE_ARC:
            // Wide arcs choose their nearer edge; narrow arcs choose the midpoint.
            targetYaw = _actor01600MeasureTarget(actor, &targetDistance);
            if (targetYaw < 0)
                targetYaw += ACTOR_TRANSFORM_ANGLE_TURN;
            clearArcCount     = work->clearArcCount;
            work->turnRequest = 0;
            if (clearArcCount == 1) {
                s32 endYaw   = work->clearArcs[0].endYaw;
                s32 startYaw = work->clearArcs[0].startYaw;

                arcSpanOrEndDistance = endYaw - startYaw;
                if (arcSpanOrEndDistance >= ACTOR_01600_WIDE_ARC) {
                    arcSpanOrEndDistance       = __builtin_abs(targetYaw - endYaw);
                    arcMidpointOrStartDistance = __builtin_abs(targetYaw - startYaw);
                    if (arcSpanOrEndDistance < arcMidpointOrStartDistance)
                        work->turnRequest = endYaw;
                    else
                        work->turnRequest = startYaw;
                } else
                    work->turnRequest = startYaw + arcSpanOrEndDistance / 2;
            } else {
                bestDistance = ACTOR_01600_HEADING_BEST_UNSET;
                arcIndex     = 0;
                if (clearArcCount > 0) {
                    fullTurn = ACTOR_TRANSFORM_ANGLE_TURN;
                    arcCount = clearArcCount;
                    do {
                        s32 endYaw   = work->clearArcs[arcIndex].endYaw;
                        s32 startYaw = work->clearArcs[arcIndex].startYaw;

                        arcSpanOrEndDistance       = endYaw - startYaw;
                        arcMidpointOrStartDistance = startYaw + arcSpanOrEndDistance / 2;
                        // Exclude arc midpoints close to directly behind the scavenger.
                        if (arcMidpointOrStartDistance < ACTOR_01600_REAR_ARC_MIN || arcMidpointOrStartDistance > ACTOR_01600_REAR_ARC_MAX) {
                            candidateDistance  = __builtin_abs(targetYaw - arcMidpointOrStartDistance);
                            wrappedDistance    = fullTurn - candidateDistance;
                            candidateDirection = 0;
                            if (wrappedDistance < candidateDistance)
                                candidateDistance = wrappedDistance;
                            else
                                candidateDirection = ACTOR_01600_HEADING_DIRECTION;
                            if (candidateDistance < bestDistance) {
                                directionFlags = candidateDirection;
                                bestDistance   = candidateDistance;
                                if (arcSpanOrEndDistance >= ACTOR_01600_WIDE_ARC) {
                                    s32 endYaw   = work->clearArcs[arcIndex].endYaw;
                                    s32 startYaw = work->clearArcs[arcIndex].startYaw;

                                    arcSpanOrEndDistance       = __builtin_abs(targetYaw - endYaw);
                                    arcMidpointOrStartDistance = __builtin_abs(targetYaw - startYaw);
                                    if (arcSpanOrEndDistance < arcMidpointOrStartDistance)
                                        arcMidpointOrStartDistance = endYaw;
                                    else
                                        arcMidpointOrStartDistance = startYaw;
                                }
                                work->turnRequest = arcMidpointOrStartDistance;
                            }
                        }
                        arcIndex++;
                    } while (arcIndex < arcCount);
                }
            }
            if (work->turnRequest > ACTOR_TRANSFORM_ANGLE_HALF_TURN)
                work->turnRequest -= ACTOR_TRANSFORM_ANGLE_TURN;
            else if (work->turnRequest < -ACTOR_TRANSFORM_ANGLE_HALF_TURN)
                work->turnRequest += ACTOR_TRANSFORM_ANGLE_TURN;
            gfxReadMatrixZAxis(&actor->extra.tmd->coords->coord, &facingAxis);
            ratan2(facingAxis.vx, facingAxis.vz);
            targetYaw = __builtin_abs(work->turnRequest);
            if (targetYaw > ACTOR_TRANSFORM_ANGLE_HALF_TURN)
                targetYaw = ACTOR_TRANSFORM_ANGLE_TURN - targetYaw;
            work->turnRate = targetYaw / 16;
            if (work->turnRate < ACTOR_01600_MIN_TURN_STEP)
                work->turnRate = ACTOR_01600_MIN_TURN_STEP;
            return directionFlags | ACTOR_01600_HEADING_ARC;
    }
    return ACTOR_01600_HEADING_PENDING;
}

/// Selects the nearer player-actor slot by horizontal distance, with ties going to the player.
///
/// Returns the player slot when either task is absent, including when only the
/// companion exists. Distances use signed-halfword position differences; Y is
/// stored in the temporary but excluded from the comparison. No task is retained.
static s32 _actor01600SelectNearestPlayer(Task* actor)
{
    GfxCoord* rootCoord;
    GfxCoord* playerCoord;
    SVECTOR   offset;
    s32       playerDistance;

    rootCoord = actor->extra.tmd->coords;
    if (gPlayerActorTasks[PLAYER_ACTOR_TASK_PLAYER] != NULL) {
        playerCoord    = gPlayerActorTasks[PLAYER_ACTOR_TASK_PLAYER]->extra.tmd->coords;
        offset.vx      = (u16)playerCoord->coord.t[0] - (u16)rootCoord->coord.t[0];
        offset.vy      = (u16)playerCoord->coord.t[1] - (u16)rootCoord->coord.t[1];
        offset.vz      = (u16)playerCoord->coord.t[2] - (u16)rootCoord->coord.t[2];
        playerDistance = SquareRoot0((offset.vx * offset.vx) + (offset.vz * offset.vz));
        if (gPlayerActorTasks[PLAYER_ACTOR_TASK_COMPANION] != NULL) {
            playerCoord = gPlayerActorTasks[PLAYER_ACTOR_TASK_COMPANION]->extra.tmd->coords;
            offset.vx   = (u16)playerCoord->coord.t[0] - (u16)rootCoord->coord.t[0];
            offset.vy   = (u16)playerCoord->coord.t[1] - (u16)rootCoord->coord.t[1];
            offset.vz   = (u16)playerCoord->coord.t[2] - (u16)rootCoord->coord.t[2];
            return SquareRoot0((offset.vx * offset.vx) + (offset.vz * offset.vz)) < playerDistance;
        }
    }
    return 0;
}

/// Activates an ordinary placement or stages a scavenger waiting for its wave cue.
///
/// Mode 0 links collision bodies and allocates primitive buffers. Mode 3 keeps
/// the staged model visible; other waiting modes hide it and defer buffer allocation.
/// Waiting placements are suspended, not lockable, and counted in the wave population.
static void _actor01600InitPlacement(Task* actor)
{
    TmdObject*       model;
    TmdObject*       drawModel;
    TmdObject*       hiddenModel;
    TmdObject*       bufferModel;
    s32              placementMode;
    Enemy*           enemy;
    _Actor01600Work* work;

    enemy         = actor->spawnArg2.pointer;
    placementMode = enemy->place->mode;
    work          = actor->work;
    switch (placementMode) {
        case ACTOR_01600_PLACEMENT_ACTIVE:
            _actor01600LinkCollisionBodies(actor);
            tmdAllocPrimitiveBuffer(actor->extra.tmd);
            model             = actor->extra.tmd;
            model->flags     &= (u16)~TMD_OBJECT_SKIP_AUTO_BUFFER;
            drawModel         = actor->extra.tmd;
            drawModel->flags &= (u16)~TMD_OBJECT_SKIP_ACTIVE_DRAW;
            work->active      = 1;
            return;
        case ACTOR_01600_PLACEMENT_SCRIPTED:
            work->entranceLeap            = 1;
            enemy->node.state.parts.flags = WORLD_TARGET_NOT_LOCKABLE;
            work->active                  = 0;
            work->suspended               = 1;
            work->scriptedRotation.vx     = 0;
            work->scriptedRotation.vy     = (u16)enemy->place->yaw;
            work->scriptedRotation.vz     = 0;
            Actor01600_D12874            += 1;
            return;
        case ACTOR_01600_PLACEMENT_FIRST_WAVE:
            work->reactionDelay = placementMode;

        // The first wave also waits, but starts its cue delay at one frame.
        default:
            hiddenModel                   = actor->extra.tmd;
            hiddenModel->flags           |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
            bufferModel                   = actor->extra.tmd;
            bufferModel->flags           |= TMD_OBJECT_SKIP_AUTO_BUFFER;
            work->entranceLeap            = 1;
            enemy->node.state.parts.flags = WORLD_TARGET_NOT_LOCKABLE;
            work->shadowHidden            = 1;
            work->active                  = 0;
            work->suspended               = 1;
            Actor01600_D12874            += 1;
            return;
    }
}
static s32 Actor01600_Fn05558(Task* arg0)
{
    SVECTOR          rot;
    Enemy*           ctx;
    AreaPlacement*   params;
    _Actor01600Work* work;
    GfxCoord*        rootCoord;
    GfxCoord*        coord;
    TmdObject*       obj;
    TmdObject*       obj2;
    TmdObject*       obj3;
    TmdObject*       obj4;
    SVECTOR*         pos;
    SVECTOR*         tableA;
    SVECTOR*         tableB;
    SVECTOR*         pos2;
    s16              countdown;
    s16              countdown2;
    s16              countdown3;
    s32              event;
    s32              pan;
    s32              scriptArg;
    u16              kind;
    u8               mode;

    coord = arg0->extra.tmd->coords;
    ctx   = arg0->spawnArg2.pointer;
    work  = arg0->work;
    if ((u32)(gGameSession->location.loc.stage - 2) < 2U) {
        if ((GAME_LOCATION_WORD(gGameSession->location.loc) & GAME_LOCATION_KEY(0, 255, 255, 0)) == GAME_LOCATION_KEY(0, 34, 1, 0)) {
            if ((u16)work->behavior < (u32)ACTOR_01600_BEHAVIOR_STAGGER) {
                work->behavior = ACTOR_01600_BEHAVIOR_POSED;
            }
        }
        if (((u32)(gGameSession->location.loc.stage - 2) < 2U) && (gGameSession->location.loc.area == 0x26) &&
            ((mode = gGameSession->location.loc.room, (mode == 1)) || (mode == 3)) && ((u16)work->behavior < (u32)ACTOR_01600_BEHAVIOR_STAGGER)) {
            work->behavior = ACTOR_01600_BEHAVIOR_POSED;
        }
    }
    if (((u8)ctx->place->variant & 0x80) && ((u16)work->behavior < (u32)ACTOR_01600_BEHAVIOR_STAGGER)) {
        work->behavior = ACTOR_01600_BEHAVIOR_POSED;
    }
    if (work->active != 0) {
        return 0;
    }

    params = ctx->place;
    kind   = params->mode;
    switch (kind) {
        case 1:
        case 2:
            _actor01600StepTurn(arg0);
            if ((u32)((u8)gSceneCombatState.actor01600Wave - 1) >= 2U) {
                goto running;
            }
            scriptArg = (s8)(u8)gSceneCombatState.actor01600Wave;
            if (scriptArg == 1) {
                event = (((u16)((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x4010000E;
                pan   = (s8)worldCoordGetOriginAudioPan(coord);
                sndEvtRequestScriptStart(event, pan, (s8)worldCoordGetOriginAudioDepth(coord));
                countdown           = (u16)work->reactionDelay - 1;
                work->reactionDelay = countdown;
                if ((countdown << 0x10) != 0) {
                    goto running;
                }
                if ((u8)ctx->place->variant == 0) {
                    work->attackAction = ACTOR_01600_ACTION_LUNGE_RECOVER;
                    work->animRequest  = 0x1A;
                } else {
                    work->entranceLeap = (s16)scriptArg;
                    work->attackAction = ACTOR_01600_ACTION_LUNGE;
                }
                work->shadowHidden = 1;
            } else if (scriptArg == 2) {
                if ((GAME_LOCATION_WORD(gGameSession->location.loc) & GAME_LOCATION_STAGE_AREA_MASK) == GAME_LOCATION_KEY(3, 15, 0, 0)) {
                    tableA            = Actor01600_D09F1C;
                    pos               = &Actor01600_D09F1C[(u16)ctx->placeKey >> ENEMY_PLACE_INDEX_SHIFT];
                    coord->coord.t[0] = pos->vx;
                    coord->coord.t[1] = pos->vy;
                    coord->coord.t[2] = pos->vz;
                }
                if ((GAME_LOCATION_WORD(gGameSession->location.loc) & GAME_LOCATION_STAGE_AREA_MASK) == GAME_LOCATION_KEY(4, 4, 0, 0)) {
                    tableB            = Actor01600_D09F3C;
                    pos2              = &Actor01600_D09F3C[(u16)ctx->placeKey >> ENEMY_PLACE_INDEX_SHIFT];
                    coord->coord.t[0] = pos2->vx;
                    coord->coord.t[1] = pos2->vy;
                    coord->coord.t[2] = pos2->vz;
                }
                sceneEngageBattle(1);
            }
            ctx->node.state.parts.flags = 0;
            work->active                = 1;
            _actor01600LinkCollisionBodies(arg0);
            if (gSceneCombatState.actor01600Wave == 1) {
                work->bodySphere.body.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_GRID_ENABLED);
            }
            tmdAllocPrimitiveBuffer(arg0->extra.tmd);
            obj                   = arg0->extra.tmd;
            obj->flags           &= (u16)~TMD_OBJECT_SKIP_AUTO_BUFFER;
            obj2                  = arg0->extra.tmd;
            obj2->flags          &= (u16)~TMD_OBJECT_SKIP_ACTIVE_DRAW;
            work->behavior        = ACTOR_01600_BEHAVIOR_ATTACK;
            work->animBlendFrames = 0;
            work->animPlaying     = 0;
            work->animFrame       = 0;
            work->biteLanded      = 0;
            return 0;
        case 3:
            if (gSceneCombatState.actor01600Wave == kind) {
                if ((u8)params->variant == 1) {
                    _actor01600Remove(arg0, 1);
                running:
                    return 1;
                }
                countdown2          = (u16)work->reactionDelay - 1;
                work->reactionDelay = countdown2;
                if ((countdown2 << 0x10) == 0) {
                    ctx->node.state.parts.flags = 0;
                    _actor01600LinkCollisionBodies(arg0);
                    work->attackAction    = ACTOR_01600_ACTION_LUNGE_RECOVER;
                    work->active          = 1;
                    work->behavior        = ACTOR_01600_BEHAVIOR_ATTACK;
                    work->animBlendFrames = 0;
                    work->animPlaying     = 0;
                    work->animFrame       = 0;
                    work->biteLanded      = 0;
                    work->animRequest     = 0x1A;
                    return 0;
                }
                goto running;
            }
            if (arg0->extra.tmd->flags & TMD_OBJECT_SKIP_ACTIVE_DRAW) {
                goto running;
            }
            _actor01600StepAnimation(arg0);
            Actor01600_Fn05F80(arg0);
            if (work->colorRefresh != 0) {
                update_actor_color(ctx, arg0->extra.tmd->coords + 1);
                work->colorRefresh = 0;
            }
            coord->composeStamp = GRAPHICS_COORD_DIRTY;
            goto running;
        default:
            if (gSceneCombatState.actor01600Wave < (s32)ctx->place->mode) {
                goto running;
            }
            countdown3          = (u16)work->reactionDelay - 1;
            work->reactionDelay = countdown3;
            if ((countdown3 << 0x10) != 0) {
                goto running;
            }
            ctx->node.state.parts.flags = 0;
            work->active                = 1;
            _actor01600LinkCollisionBodies(arg0);
            tmdAllocPrimitiveBuffer(arg0->extra.tmd);
            obj3                  = arg0->extra.tmd;
            obj3->flags          &= (u16)~TMD_OBJECT_SKIP_AUTO_BUFFER;
            obj4                  = arg0->extra.tmd;
            obj4->flags          &= (u16)~TMD_OBJECT_SKIP_ACTIVE_DRAW;
            work->attackAction    = ACTOR_01600_ACTION_LUNGE_RECOVER;
            work->behavior        = ACTOR_01600_BEHAVIOR_ATTACK;
            work->animBlendFrames = 0;
            work->animPlaying     = 0;
            work->animFrame       = 0;
            work->biteLanded      = 0;
            work->longLeap        = 1;
            work->animRequest     = 0x1A;
            rootCoord             = arg0->extra.tmd->coords;
            memset(&rot, 0, 8);
            rot.vy = -0x400;
            RotMatrix(&rot, &rootCoord->coord);
            work->shadowHidden = 1;
            return 0;
    }
}

/// Applies a borrowed scene command to this placement's staged animation and visibility.
///
/// Commands 1..7 cue variant-specific poses, visibility or the final wave;
/// command 8 releases rewards and destroys the task. The command context is
/// ignored. Returns 0 for recognized commands and -1 otherwise. Message ID
/// and the second payload are unused; the request need only live through dispatch.
static s32 _actor01600HandleCommand(Task* actor, s32 messageId, const ActorCommand* request, s32 unusedArg)
{
    // The room cues have variant-specific animation responses.
    enum {
        ACTOR_01600_COMMAND_CUE_1            = 1,
        ACTOR_01600_COMMAND_CUE_2            = 2,
        ACTOR_01600_COMMAND_CUE_3            = 3,
        ACTOR_01600_COMMAND_CUE_4            = 4,
        ACTOR_01600_COMMAND_REFRESH_COLOR    = 5,
        ACTOR_01600_COMMAND_BEGIN_FINAL_WAVE = 6,
        ACTOR_01600_COMMAND_HIDE             = 7,
        ACTOR_01600_COMMAND_DESTROY          = 8,
        ACTOR_01600_WAVE_FINAL               = 3,
    };

    SVECTOR          rotation;
    Enemy*           enemy;
    _Actor01600Work* work;
    GfxCoord*        rootCoord;
    TmdObject*       model;
    u32              placementVariant;

    enemy            = actor->spawnArg2.pointer;
    rootCoord        = actor->extra.tmd->coords;
    work             = actor->work;
    placementVariant = enemy->place->variant;

    switch (request->command) {
        case ACTOR_01600_COMMAND_CUE_1:
            work->colorRefresh = 1;
            if (placementVariant == 2) {
                gRandomLcgState     = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                work->animFrame     = 0;
                work->animPlaying   = 0;
                work->animRequest   = 3;
                work->scriptedAnim  = 9;
                work->scriptedDelay = (gRandomLcgState >> 16) % 20;
            }
            if (placementVariant == 4) {
                work->animRequest = 0x11;
                work->animPlaying = 0;
            }
            if (placementVariant == 3) {
                work->animRequest     = 0x11;
                work->animPlaying     = 0;
                rootCoord->coord.t[0] = 0x4021;
                rootCoord->coord.t[1] = -0x190;
                rootCoord->coord.t[2] = -0x6C0;
                rotation.vx           = 0;
                rotation.vy           = 0x600;
                rotation.vz           = 0;
                RotMatrix(&rotation, &rootCoord->coord);
                model         = actor->extra.tmd;
                model->flags |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
                model         = actor->extra.tmd;
                model->flags |= TMD_OBJECT_SKIP_AUTO_BUFFER;
            }
            break;
        case ACTOR_01600_COMMAND_CUE_2:
            work->colorRefresh = 1;
            if (placementVariant == 1) {
                work->animRequest = 0x1A;
                work->animPlaying = 0;
            }
            if (placementVariant == 2) {
                gRandomLcgState     = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                work->animFrame     = 0;
                work->animPlaying   = 0;
                work->animRequest   = 3;
                work->scriptedAnim  = 9;
                work->scriptedDelay = (gRandomLcgState >> 16) % 20;
            }
            if (placementVariant == 3) {
                model         = actor->extra.tmd;
                model->flags &= (u16)~TMD_OBJECT_SKIP_AUTO_BUFFER;
                model         = actor->extra.tmd;
                model->flags &= (u16)~TMD_OBJECT_SKIP_ACTIVE_DRAW;
            }
            break;
        case ACTOR_01600_COMMAND_CUE_3:
            work->colorRefresh = 1;
            if ((u32)(placementVariant - 1) < 4) {
                model         = actor->extra.tmd;
                model->flags &= (u16)~TMD_OBJECT_SKIP_AUTO_BUFFER;
                model         = actor->extra.tmd;
                model->flags &= (u16)~TMD_OBJECT_SKIP_ACTIVE_DRAW;
            }
            if (placementVariant == 1) {
                work->animRequest = 9;
                work->animPlaying = 0;
            }
            if (placementVariant == 2) {
                gRandomLcgState     = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                work->animFrame     = 0;
                work->animPlaying   = 0;
                work->animRequest   = 3;
                work->scriptedAnim  = 9;
                work->scriptedDelay = (gRandomLcgState >> 16) % 20;
            }
            break;
        case ACTOR_01600_COMMAND_CUE_4:
            work->colorRefresh = 1;
            if (placementVariant == 1) {
                work->animRequest = ACTOR_01600_ANIMATION_NONE;
                work->animPlaying = 0;
                work->animFrame   = 0;
            }
            if (placementVariant == 2) {
                work->animRequest   = 3;
                work->scriptedDelay = 0xA;
                work->animFrame     = 0;
                work->animPlaying   = 0;
                work->scriptedAnim  = 7;
            }
            if (placementVariant == 4) {
                work->animFrame     = 0;
                work->scriptedDelay = 7;
                work->scriptedAnim  = 7;
            }
            break;
        case ACTOR_01600_COMMAND_REFRESH_COLOR:
            work->colorRefresh = 1;
            break;
        case ACTOR_01600_COMMAND_BEGIN_FINAL_WAVE:
            work->colorRefresh = 1;
            if (placementVariant == 3) {
                _actor01600Remove(actor, 0);
            }
            if (placementVariant == 2 || placementVariant == 4) {
                model         = actor->extra.tmd;
                model->flags &= (u16)~TMD_OBJECT_SKIP_AUTO_BUFFER;
                model         = actor->extra.tmd;
                model->flags &= (u16)~TMD_OBJECT_SKIP_ACTIVE_DRAW;
            }
            gSceneCombatState.actor01600Wave = ACTOR_01600_WAVE_FINAL;
            break;
        case ACTOR_01600_COMMAND_HIDE:
            work->colorRefresh = 1;
            if ((u32)(placementVariant - 1) < 4) {
                model         = actor->extra.tmd;
                model->flags |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
                model         = actor->extra.tmd;
                model->flags |= TMD_OBJECT_SKIP_AUTO_BUFFER;
            }
            break;
        case ACTOR_01600_COMMAND_DESTROY:
            sceneReleaseBattleRefWithRewards(actor, 0x10);
            enemyDestroy(enemy, actor);
            Actor01600_D12874 -= 1;
            break;
        default:
            return -1;
    }
    return 0;
}

static void Actor01600_Fn05F80(Task* arg0)
{
    SVECTOR          rot;
    GfxCoord*        coord;
    _Actor01600Work* work;
    GfxCoord*        part1;
    u32              variant;
    s32              anim;
    u16              timer;
    s32              value;

    coord   = arg0->extra.tmd->coords;
    work    = arg0->work;
    variant = ((Enemy*)arg0->spawnArg2.pointer)->place->variant;
    part1   = &arg0->extra.tmd->coords[1];

    if (variant == 1) {
        switch (work->animRequest) {
            case 0x1A:
                work->colorRefresh = variant;
                if ((u32)((u16)work->animFrame - 6) < 0xEU) {
                    coord->coord.t[1] -= 0xC8;
                }
                if ((u32)((u16)work->animFrame - 0x14) < 0xFU) {
                    value             = coord->coord.t[1] + 0x96;
                    coord->coord.t[1] = value;
                    if (value >= -0x497) {
                        coord->coord.t[1] = -0x498;
                    }
                }
                if ((u32)((u16)work->animFrame - 0xB) < 0x14U) {
                    value             = coord->coord.t[0] + ((coord->coord.m[0][2] * 0x4B) >> 0xB);
                    coord->coord.t[0] = value;
                    if (value < 0x3B23) {
                        coord->coord.t[0] = 0x3B23;
                    }
                    value             = coord->coord.t[2] + ((coord->coord.m[2][2] * 0x4B) >> 0xB);
                    coord->coord.t[2] = value;
                    if (value >= -0xD11) {
                        coord->coord.t[2] = -0xD12;
                    }
                }
                if ((s16)work->animFrame >= 0x31) {
                    work->animRequest = 8;
                    work->animPlaying = 0;
                    work->animFrame   = 0;
                }
                break;
            case 8:
                if ((u32)((u16)work->animFrame - 4) < 0x11U) {
                    work->scriptedRotation.vx  = 0;
                    work->scriptedRotation.vz  = 0;
                    work->scriptedRotation.vy += 0x32;
                    RotMatrix(&work->scriptedRotation, &coord->coord);
                }
                if ((s16)work->animFrame >= 0x1C) {
                    work->animRequest = 9;
                    work->animPlaying = 0;
                    work->animFrame   = 0;
                }
                break;
            case 0xFF:
                anim = work->animFrame;
                if (anim == 2) {
                    work->hitEffect.coord      = part1;
                    work->hitEffect.spawnArgLo = 0x100;
                    work->hitEffect.spawnArgHi = anim;
                    effectSpawnHit(EFFECT_HIT_KIND_WEAPON_PUFF, part1, 0, &work->hitEffect);
                    effectSpawn(EFFECT_CRITICAL_HIT, &arg0->extra.tmd->coords[1], 0, NULL);
                }
                if ((u32)((u16)work->animFrame - 0xF) < 8U) {
                    coord->coord.t[1] += 0x80;
                }
                if ((s16)work->animFrame >= 3) {
                    coord->coord.t[0] += 0x10E;
                    coord->coord.t[2] -= 0xC8;
                    coord              = &arg0->extra.tmd->coords[6];
                    rot.vx             = -0x400;
                    rot.vy             = 0;
                    rot.vz             = 0;
                    RotMatrix(&rot, &coord->coord);
                    coord  = &arg0->extra.tmd->coords[8];
                    rot.vx = -0x400;
                    rot.vy = 0;
                    rot.vz = 0;
                    RotMatrix(&rot, &coord->coord);
                    if ((s16)work->animFrame >= 3) {
                        work->scriptedRotation.vx  = 0x384;
                        work->scriptedRotation.vy += 0x96;
                        timer                      = work->scriptedRotation.vz + 0x64;
                        work->scriptedRotation.vz  = timer;
                        if ((s16)timer >= 0x384) {
                            work->scriptedRotation.vz = 0x384;
                        }
                        coord = &arg0->extra.tmd->coords[1];
                        RotMatrix(&work->scriptedRotation, &coord->coord);
                    }
                }
                if ((s16)work->animFrame >= 0x12) {
                    _actor01600Remove(arg0, 0);
                }
                work->animFrame += 1;
                break;
        }
    }
    if (variant == 2) {
        timer               = work->scriptedDelay - 1;
        work->scriptedDelay = timer;
        if ((s16)timer < 0) {
            anim                = work->scriptedAnim;
            work->scriptedDelay = 1;
            switch (anim) {
                case 7:
                    work->animRequest = anim;
                    if ((u32)((u16)work->animFrame - 4) < 0x11U) {
                        work->scriptedRotation.vx  = 0;
                        work->scriptedRotation.vz  = 0;
                        work->scriptedRotation.vy += 0x32;
                        RotMatrix(&work->scriptedRotation, &coord->coord);
                    }
                    if ((s16)work->animFrame >= 0x1C) {
                        work->scriptedAnim = 9;
                        work->animRequest  = 9;
                        work->animPlaying  = 0;
                        work->animFrame    = 0;
                    }
                    break;
                case 9:
                    work->animRequest = anim;
                    if ((s16)work->animFrame >= 0x32) {
                        work->animFrame    = 0;
                        work->animPlaying  = 0;
                        work->animRequest  = 3;
                        work->scriptedAnim = 0;
                    }
                    break;
            }
        }
    }
    if (variant == 4) {
        timer               = work->scriptedDelay - 1;
        work->scriptedDelay = timer;
        if ((s16)timer < 0) {
            anim                = work->scriptedAnim;
            work->scriptedDelay = 1;
            if (anim == 7) {
                work->animRequest = anim;
                if ((u32)((u16)work->animFrame - 4) < 0x11U) {
                    work->scriptedRotation.vx  = 0;
                    work->scriptedRotation.vz  = 0;
                    work->scriptedRotation.vy -= 0xFA;
                    RotMatrix(&work->scriptedRotation, &coord->coord);
                }
                if ((s16)work->animFrame >= 0x1C) {
                    work->scriptedAnim = 9;
                    work->animRequest  = 9;
                    work->animPlaying  = 0;
                    work->animFrame    = 0;
                }
            }
        }
    }
}

static void Actor01600_Fn0646C(Task* arg0)
{
    EffectWork* effect;
    TmdObject*  obj;
    TmdObject*  obj2;
    s32         randomState;
    s32         choice;

    if (((_Actor01600Work*)arg0->work)->burstState != 0) {
        D_800626EC[5].data.model = &_gActor01600ScavengerBurstHead;
        effect                   = effectSpawn(EFFECT_BURST_BODY_PART_BANK8, arg0->extra.tmd->coords + 1, 0, NULL);
        if (effect != NULL) {
            Actor01600_Fn070AC(effect->task, arg0);
        }
        D_800626EC[5].data.model = &_gActor01600ScavengerBurstEar;
        effect                   = effectSpawn(EFFECT_BURST_BODY_PART_BANK8, arg0->extra.tmd->coords + 2, 0, NULL);
        if (effect != NULL) {
            Actor01600_Fn070AC(effect->task, arg0);
        }
        D_800626EC[5].data.model = &_gActor01600ScavengerBurstEar;
        effect                   = effectSpawn(EFFECT_BURST_BODY_PART_BANK8, arg0->extra.tmd->coords + 3, 0, NULL);
        if (effect != NULL) {
            Actor01600_Fn070AC(effect->task, arg0);
        }
        effectSpawn(EFFECT_030, arg0->extra.tmd->coords + 1, 0x300, &Actor01600_D12868);
        return;
    }
    randomState     = (gRandomLcgState * RANDOM_LCG_MULTIPLIER) + RANDOM_LCG_INCREMENT;
    gRandomLcgState = randomState;
    choice          = ((u32)randomState >> 0x10) & 3;
    switch (choice) {
        case 0:
        case 1:
            D_800626EC[5].data.model = &_gActor01600ScavengerBurstHead;
            effect                   = effectSpawn(EFFECT_BURST_BODY_PART_BANK8, arg0->extra.tmd->coords + 1, 0, NULL);
            if (effect != NULL) {
                Actor01600_Fn070AC(effect->task, arg0);
            }
            break;
        case 2:
            D_800626EC[5].data.model = &_gActor01600ScavengerBurstEar;
            effect                   = effectSpawn(EFFECT_BURST_BODY_PART_BANK8, arg0->extra.tmd->coords + 2, 0, NULL);
            if (effect != NULL) {
                Actor01600_Fn070AC(effect->task, arg0);
            }
            break;
        case 3:
            D_800626EC[5].data.model = &_gActor01600ScavengerBurstLeg;
            effect                   = effectSpawn(EFFECT_BURST_BODY_PART_BANK8, arg0->extra.tmd->coords + 6, 0, NULL);
            if (effect != NULL) {
                Actor01600_Fn070AC(effect->task, arg0);
            }
            break;
    }
    effectSpawn(EFFECT_030, arg0->extra.tmd->coords + 1, 0x50, &Actor01600_D12868);
    obj          = arg0->extra.tmd;
    obj->flags  |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
    obj2         = arg0->extra.tmd;
    obj2->flags |= TMD_OBJECT_SKIP_AUTO_BUFFER;
}

void Actor01600_Fn066E8(Task* arg0)
{
    EnemyTaskFuncTable3 sp;

    sp = Actor01600_D00004;
    sp.funcs[arg0->state](((Enemy*)arg0->spawnArg2.pointer), arg0);
}

/// Snapshots the root coordinate's translation into the work block's
/// `previousPosition`, then steps that coordinate along the model's own
/// facing: `forwardSpeed` units of the coordinate's column 2 added to its X/Z
/// translation. Vertical follow-up only runs while `suspended` is clear: with
/// `airborne` set `verticalSpeed` is added to `t[1]`, otherwise the
/// coordinate is moved down by 0x80.
static void Actor01600_Fn06744(Task* arg0)
{
    _Actor01600Work* work;
    GfxCoord*        coord;

    coord                     = arg0->extra.tmd->coords;
    work                      = arg0->work;
    work->previousPosition.vx = coord->coord.t[0];
    work->previousPosition.vy = coord->coord.t[1];
    work->previousPosition.vz = coord->coord.t[2];
    coord->coord.t[0]        += (s32)(coord->coord.m[0][2] * work->forwardSpeed) >> 0xC;
    coord->coord.t[2]        += (s32)(coord->coord.m[2][2] * work->forwardSpeed) >> 0xC;
    if (work->suspended == 0) {
        if (work->airborne != 0) {
            coord->coord.t[1] += work->verticalSpeed;
            return;
        }
        coord->coord.t[1] += 0x80;
    }
}

/// Colours the actor from the *second* attach coordinate of its model: takes a
/// 0x10-byte `VECTOR` off the scratch stack, fills it with that coordinate's
/// world position and hands it to `worldCoordUpdateActorColor` with zero for the unused
/// arguments.
static void Actor01600_Fn06810(Enemy* arg0, Task* arg1)
{
    GfxCoord* coord;
    void**    scratch;
    u8*       head;
    VECTOR*   block;

    coord                          = &arg1->extra.tmd->coords[1];
    scratch                        = SCRATCH_HEAD_ADDR;
    head                           = SCRATCH_HEAD_AT(scratch, void);
    block                          = (VECTOR*)(head - 0x10);
    block->vx                      = coord->workm.t[0];
    block->vy                      = coord->workm.t[1];
    block->vz                      = coord->workm.t[2];
    SCRATCH_HEAD_AT(scratch, void) = block;
    worldCoordUpdateActorColor(arg0, block, 0, 0);
    SCRATCH_POP_BYTES_AT(scratch, 0x10);
}

/// Squashes the actor's root coordinate: the work block's `deathMatrix`
/// is copied into the coordinate, an identity is splatted into an
/// `ActorScaleScratch` block and scaled per axis by 1.0 / the decaying
/// `deathScaleY` / 1.0, and the product replaces the coordinate's rotation.
/// `composeStamp` is cleared so its own work matrix is rebuilt from `coord` next frame.
static void Actor01600_Fn06880(Task* arg0)
{
    GfxCoord*          coord;
    ActorScaleScratch* head;
    ActorScaleScratch* scratch;
    _Actor01600Work*   work;

    head                                    = SCRATCH_STACK_CURSOR(ActorScaleScratch);
    work                                    = arg0->work;
    scratch                                 = head - 1;
    SCRATCH_STACK_CURSOR(ActorScaleScratch) = scratch;
    coord                                   = arg0->extra.tmd->coords;
    if (work->deathScaleY >= 0x201) {
        work->deathScaleY = (u16)work->deathScaleY - 0x50;
    }
    scratch->scale.vx = ONE;
    scratch->scale.vy = (s32)work->deathScaleY;
    scratch->scale.vz = ONE;
    coord->coord      = work->deathMatrix;
    gfxSetRotIdentity(&scratch->matrix);
    ScaleMatrix(&scratch->matrix, &scratch->scale);
    MulMatrix(&coord->coord, &scratch->matrix);
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
    SCRATCH_STACK_RELEASE_BLOCK(ActorScaleScratch);
}

/// Steps the attachment coordinate `distance` units along the model's facing:
/// `gfxReadMatrixZAxis` reads that coordinate's column into `facing`, `ratan2` turns it
/// into a yaw, `RotMatrixY` builds the rotation for the yaw and
/// `ApplyMatrixLV` rotates the step vector `(distance, 0, 0)` by it before the
/// result is added to `coord.t`.
static void Actor01600_Fn06974(Task* actor, s32 distance)
{
    MATRIX*                     rotation;
    _Actor01600SidestepScratch* block;
    GfxCoord*                   coord;

    coord                                            = actor->extra.tmd->coords;
    block                                            = SCRATCH_STACK_CURSOR(_Actor01600SidestepScratch) - 1;
    block->step.vx                                   = (s16)distance;
    block->step.vy                                   = 0;
    block->step.vz                                   = 0;
    SCRATCH_STACK_CURSOR(_Actor01600SidestepScratch) = block;
    gfxReadMatrixZAxis(&actor->extra.tmd->coords->coord, &block->facing);
    rotation   = &block->rotation;
    block->yaw = ratan2(block->facing.vx, block->facing.vz);
    gfxSetRotIdentity(rotation);
    RotMatrixY(block->yaw, rotation);
    ApplyMatrixLV(rotation, &block->step, &block->step);
    coord->coord.t[0] += block->step.vx;
    coord->coord.t[1] += block->step.vy;
    coord->coord.t[2] += block->step.vz;
    SCRATCH_STACK_RELEASE_BLOCK(_Actor01600SidestepScratch);
}

static void Actor01600_Fn06A84(Task* arg0)
{
    _Actor01600Work* work;
    GfxCoord*        coord;
    MATRIX*          scratch;
    u8*              head;
    s16              value;

    head                         = SCRATCH_STACK_CURSOR(u8);
    SCRATCH_STACK_CURSOR(MATRIX) = (MATRIX*)(head - 0x20);
    scratch                      = (MATRIX*)(head - 0x20);
    work                         = arg0->work;
    coord                        = arg0->extra.tmd->coords;
    RotMatrix(&work->recoilRotation, scratch);
    gte_SetRotMatrix(&coord[1].coord);
    gte_ldclmv(scratch);
    gte_rtir();
    gte_stclmv(&coord[1].coord);
    gte_ldclmv(&scratch->m[0][1]);
    gte_rtir();
    gte_stclmv(&coord[1].coord.m[0][1]);
    gte_ldclmv(&scratch->m[0][2]);
    gte_rtir();
    gte_stclmv(&coord[1].coord.m[0][2]);
    value = work->recoilRotation.vx;
    if (value != 0) {
        if (value < 0x21) {
            work->recoilRotation.vx = 0;
            work->recoilActive      = 0;
        } else {
            work->recoilRotation.vx = (u16)work->recoilRotation.vx - 0x20;
        }
    }
    SCRATCH_STACK_RELEASE_BYTES(0x20);
}

static s32 Actor01600_Fn06C1C(Task* arg0)
{
    _Actor01600Work* work;

    work                        = arg0->work;
    work->pathProbe.body.flags |= (WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED);
    _actor01600SearchClearHeading(arg0);
    if (work->pathSearchPhase >= ACTOR_01600_PATH_SEARCH_BEGIN_SWEEP) {
        work->animRequest       = 0x19;
        work->alerted           = 0;
        work->behavior          = ACTOR_01600_BEHAVIOR_ROAM;
        work->pathSearchPhase   = ACTOR_01600_PATH_SEARCH_PROBE_TARGET;
        work->field_514         = 1;
        work->sight.body.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ (WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED));
        return 1;
    }
    return 0;
}

/// Column 2 of the attachment matrix goes to `dir`; the trailing `SVECTOR`
/// is never read but owns the second half of the stack local block.
static s32 Actor01600_Fn06C94(Task* arg0, s32 arg1, s32 unusedDistance)
{
    SVECTOR          dir;
    SVECTOR          unused;
    _Actor01600Work* work;
    s32              ang;
    s32              half;
    s32              res;

    work = arg0->work;
    if (ABS(arg1) < 0x301) {
        return 0;
    }
    work->turnRequest = arg1;
    gfxReadMatrixZAxis(&arg0->extra.tmd->coords->coord, &dir);
    ratan2(dir.vx, dir.vz);
    ang  = ABS(work->turnRequest);
    half = 0x1000 - ang;
    if (half < ang) {
        work->attackAction = ACTOR_01600_ACTION_TURN_WRAPPED;
        res                = half;
    } else {
        work->attackAction = ACTOR_01600_ACTION_TURN;
        res                = ang;
    }
    work->turnRate = res / 16;
    if (work->turnRate < 0x20) {
        work->turnRate = 0x20;
    }
    work->animFrame = 0;
    work->turnMode  = ACTOR_01600_TURN_REQUEST_BEGIN;
    return 1;
}

static s32 Actor01600_Fn06D74(Task* arg0, s32 arg1, s32 arg2)
{
    _Actor01600Work* work;
    s32              scaledState;
    s32              handled;
    s32              angle;
    u32              state;

    work = arg0->work;
    if (arg2 < 0x7D1) {
        angle = (arg1 >= 0 ? arg1 : -arg1);
        if (angle < 0x201) {
            work->pathProbe.body.flags |= (WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED);
            _actor01600SearchClearHeading(arg0);
            handled = 0;
            if (work->pathSearchPhase >= ACTOR_01600_PATH_SEARCH_BEGIN_SWEEP) {
                work->animRequest       = 0x19;
                work->alerted           = 0;
                work->behavior          = ACTOR_01600_BEHAVIOR_ROAM;
                work->pathSearchPhase   = ACTOR_01600_PATH_SEARCH_PROBE_TARGET;
                work->field_514         = 1;
                work->sight.body.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ (WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED));
                handled                 = 1;
            }
            if ((u8)handled) {
                return 1;
            } else {
                scaledState           = gRandomLcgState * RANDOM_LCG_MULTIPLIER;
                state                 = scaledState + RANDOM_LCG_INCREMENT;
                work->animBlendFrames = 0;
                work->animPlaying     = 0;
                work->biteLanded      = 0;
                gRandomLcgState       = state;
                if ((u32)(((state >> 16) % 100) & 0xFFFF) < 0x14U) {
                    work->attackAction = ACTOR_01600_ACTION_LUNGE_RECOVER;
                    work->animRequest  = 0x1A;
                } else {
                    work->attackAction = ACTOR_01600_ACTION_LUNGE;
                }
            }
            return 1;
        }
        return 0;
    }
    return 0;
}

/// Unlinks the scavenger's target and collision bodies, then releases its task and enemy work.
///
/// The task's exit callback; also used after the dying state's exit delay.
/// Clears the enemy's borrowed hit table before teardown. The task and enemy
/// must not be accessed afterwards.
static void _actor01600Exit(Task* actor)
{
    Enemy*           enemy;
    _Actor01600Work* work;

    enemy = actor->spawnArg2.pointer;
    work  = actor->work;

    enemy->node.state.parts.flags = WORLD_TARGET_NOT_LOCKABLE;
    enemy->recs                   = 0;
    worldTargetUnlinkNode(&enemy->node);
    worldCollisionUnlinkBody(&work->pathProbe.body);
    worldCollisionUnlinkBody(&work->sight.body);
    worldCollisionUnlinkBody(&work->bodySphere.body);
    worldCollisionUnlinkBody(&work->bite.body);
    enemyTaskExit(actor);
}

/// Releases a held player from scripted control and resets the scavenger's grab motion.
///
/// Has no effect without a held target. Dispatch is synchronous; the grab target
/// remains stored but is no longer owned by this hold.
static void _actor01600ReleaseGrab(Task* actor)
{
    _Actor01600Work* work;

    work = actor->work;
    if (work->holdingTarget != 0) {
        taskMessageDispatch(work->grabTarget, GAME_ACTOR_MESSAGE_END_SCRIPTED, 0, 0);
        Actor01600_D127D8.animationId = 0;
        work->holdingTarget           = 0;
        Actor01600_D12870             = 0;
        work->forwardSpeed            = 0;
        work->attackAction            = ACTOR_01600_ACTION_ADVANCE;
        work->animFrame               = 0;
        work->shadowUnderBody         = 0;
    }
}

/// Walks the sibling ring of task slot 4's children and reports whether any of
/// them has already been flagged `0x80` in its `field_2C` object. Returns 0xFF
/// when the slot has no children at all, 1 on the first flagged sibling and 0
/// when the whole ring is clean.
static u8 Actor01600_Fn06F78(void)
{
    Task* head;
    Task* iter;

    head = gameGetTaskSlot(GAME_TASK_SLOT_SCENE)->firstChild;
    if (head == NULL) {
        return 0xFF;
    }
    iter = head;
    do {
        if (iter->extra.tmd->flags & TMD_OBJECT_SKIP_ACTIVE_DRAW) {
            return 1;
        }
        iter = iter->nextSibling;
    } while (iter != head);
    return 0;
}

/// Hides and removes a staged scavenger, releasing its battle rewards and wave count.
///
/// Only the low byte of `skipBodyUnlink` is tested: nonzero skips collision
/// unlinking for a placement whose bodies were never linked. The task and enemy
/// are destroyed before return and must not be used afterwards.
static void _actor01600Remove(Task* actor, s32 skipBodyUnlink)
{
    _Actor01600Work* work;
    Enemy*           enemy;
    TmdObject*       model;
    TmdObject*       bufferModel;

    model              = actor->extra.tmd;
    enemy              = actor->spawnArg2.pointer;
    work               = actor->work;
    model->flags       = (u16)(model->flags | TMD_OBJECT_SKIP_ACTIVE_DRAW);
    bufferModel        = actor->extra.tmd;
    bufferModel->flags = (u16)(bufferModel->flags | TMD_OBJECT_SKIP_AUTO_BUFFER);
    enemy->recs        = 0;
    worldTargetUnlinkNode(&enemy->node);
    if (!(skipBodyUnlink & 0xFF)) {
        worldCollisionUnlinkBody(&work->pathProbe.body);
        worldCollisionUnlinkBody(&work->sight.body);
        worldCollisionUnlinkBody(&work->bodySphere.body);
        worldCollisionUnlinkBody(&work->bite.body);
    }
    worldCoordSetActorColorMode(enemy, ENEMY_COLOR_WEIGHTED);
    sceneReleaseBattleRefWithRewards(actor, 0x10);
    enemyDestroy(enemy, actor);
    Actor01600_D12874 -= 1;
}

/// Copies the source actor's texture page and CLUT row (`field_24` /
/// `field_25`) onto this actor's model object, then re-runs the model stream
/// twice so the new page/CLUT is baked into both of the object's primitive
/// buffers. Objects without an aux buffer (`field_18` NULL) have nothing to
/// rebuild and are left alone.
static void Actor01600_Fn070AC(Task* arg0, Task* arg1)
{
    TmdObject* src;
    TmdObject* dst;

    src                    = arg1->extra.tmd;
    dst                    = arg0->extra.tmd;
    dst->texturePageOffset = src->texturePageOffset;
    dst->clutRowOffset     = src->clutRowOffset;
    if (dst->buffer != NULL) {
        tmdBuildBufferHalf(dst);
        tmdBuildBufferHalf(dst);
    }
}

/// Makes the current grab end on its next bite-count check.
///
/// Handles `ACTOR_MESSAGE_RELEASE_HOLD` without payloads and always returns 0.
/// The count is set even when no grab is active; this does not release a target immediately.
static s32 _actor01600HandleReleaseHold(Task* actor, s32 messageId, s32 unusedFirstArg, s32 unusedSecondArg)
{
    enum { ACTOR_01600_GRAB_RELEASE_COUNT = 6 };

    _Actor01600Work* work = actor->work;

    work->grabBiteCount = ACTOR_01600_GRAB_RELEASE_COUNT;
    return 0;
}
