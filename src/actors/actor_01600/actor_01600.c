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

/// Restores an unscaled corpse transform and applies Q12 Y scale in borrowed scratch.
///
/// Arguments must be stable, side-effect-free expressions: coordinate and scratch
/// are evaluated repeatedly. The matrix is copied before scaling, translation is
/// retained, and composition becomes dirty. Expands to a standalone statement block.
#define ACTOR_01600_APPLY_CORPSE_SCALE(rootCoord, unscaled, scaleY, scratch) \
    {                                                                        \
        (scratch)->scale.vx = ONE;                                           \
        (scratch)->scale.vy = (scaleY);                                      \
        (scratch)->scale.vz = ONE;                                           \
        (rootCoord)->coord  = *(unscaled);                                   \
        gfxSetRotIdentity(&(scratch)->matrix);                               \
        ScaleMatrix(&(scratch)->matrix, &(scratch)->scale);                  \
        MulMatrix(&(rootCoord)->coord, &(scratch)->matrix);                  \
        (rootCoord)->composeStamp = GRAPHICS_COORD_DIRTY;                    \
    }

/// Spawns a detached part and applies its source actor's texture offsets.
///
/// Actor, model source and coordinate index must be stable, side-effect-free
/// expressions; actor and the writable EffectWork* result local are used
/// repeatedly. Rebinds the bank-8 descriptor synchronously before spawning.
#define ACTOR_01600_SPAWN_BURST_PART(actor, modelSource, coordIndex, effectResult)                                                \
    {                                                                                                                             \
        D_800626EC[5].data.model = (modelSource);                                                                                 \
        (effectResult)           = effectSpawn(EFFECT_BURST_BODY_PART_BANK8, (actor)->extra.tmd->coords + (coordIndex), 0, NULL); \
        if ((effectResult) != NULL) {                                                                                             \
            _actor01600CopyBurstTextures((effectResult)->task, (actor));                                                          \
        }                                                                                                                         \
    }

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

/// Animation requests interpreted by roaming and attack selection.
///
/// Values are indices of the package's animation table; the names describe
/// the behavior that requests them, rather than the contents of every clip.
enum {
    ACTOR_01600_ANIMATION_WAKE          = 2,
    ACTOR_01600_ANIMATION_ROAM_IDLE     = 3,
    ACTOR_01600_ANIMATION_ROAM_HOP      = 4,
    ACTOR_01600_ANIMATION_TURN          = 7,
    ACTOR_01600_ANIMATION_TURN_WRAPPED  = 8,
    ACTOR_01600_ANIMATION_ROAM_REPEAT   = 0x17,
    ACTOR_01600_ANIMATION_ROAM_LAND     = 0x18,
    ACTOR_01600_ANIMATION_LOOK_AROUND   = 0x19,
    ACTOR_01600_ANIMATION_LUNGE         = 0x1A,
    ACTOR_01600_ANIMATION_GRAB_APPROACH = 0x1C,
};

/// Animation requests interpreted by reaction, attack and staged motion.
///
/// Names describe the requesting behavior; request 16 is only proven silent.
enum {
    ACTOR_01600_ANIMATION_ADVANCE        = 5,
    ACTOR_01600_ANIMATION_ZIGZAG         = 6,
    ACTOR_01600_ANIMATION_SETTLE         = 9,
    ACTOR_01600_ANIMATION_THROWN_FORWARD = 0xB,
    ACTOR_01600_ANIMATION_QUIET_16       = 0x10,
    ACTOR_01600_ANIMATION_POSED          = 0x11,
    ACTOR_01600_ANIMATION_BUILDUP        = 0x13,
    ACTOR_01600_ANIMATION_RISE_FORWARD   = 0x15,
    ACTOR_01600_ANIMATION_RISE_BACK      = 0x16,
    ACTOR_01600_ANIMATION_LUNGE_LAND     = 0x1B,
    ACTOR_01600_ANIMATION_GRAB_HOLD      = 0x1D,
    ACTOR_01600_ANIMATION_GRAB_RELEASE   = 0x1E,
};

/// Sound-script keys before the enemy placement index is packed into bits 8..15.
/// The numbered keys identify cues; their audible contents are not established here.
enum {
    ACTOR_01600_SOUND_SCRIPT_1       = 0x40100001,
    ACTOR_01600_SOUND_SCRIPT_2       = 0x40100002,
    ACTOR_01600_SOUND_SCRIPT_3       = 0x40100003,
    ACTOR_01600_SOUND_SCRIPT_4       = 0x40100004,
    ACTOR_01600_SOUND_SCRIPT_5       = 0x40100005,
    ACTOR_01600_SOUND_IDLE_1         = 0x40100006,
    ACTOR_01600_SOUND_IDLE_2         = 0x40100007,
    ACTOR_01600_SOUND_IDLE_3         = 0x40100008,
    ACTOR_01600_SOUND_SCRIPT_B       = 0x4010000B,
    ACTOR_01600_SOUND_SCRIPT_C       = 0x4010000C,
    ACTOR_01600_SOUND_SCRIPT_D       = 0x4010000D,
    ACTOR_01600_SOUND_SCRIPT_E       = 0x4010000E,
    ACTOR_01600_SOUND_COMPANION_GRAB = 0x4065000A,
    ACTOR_01600_SOUND_PLAYER_GRAB    = 6,
};

/// Package-wide wave, burst and presentation timing values used by these handlers.
enum {
    ACTOR_01600_WAVE_FIRST          = 1,
    ACTOR_01600_WAVE_SECOND         = 2,
    ACTOR_01600_WAVE_FINAL          = 3,
    ACTOR_01600_BURST_INTACT        = 0,
    ACTOR_01600_BURST_COMPLETE      = 2,
    ACTOR_01600_COLOR_INTERVAL      = 5,
    ACTOR_01600_IDLE_SOUND_PERIOD   = 20,
    ACTOR_01600_FAST_ANIMATION_RATE = 20, // Animation ticks per update in sixteenths
};

/// Root-local collision dimensions and sight reaches, in world-coordinate units.
enum {
    ACTOR_01600_BODY_RADIUS            = 400,
    ACTOR_01600_BITE_RADIUS            = 300,
    ACTOR_01600_BITE_CENTER_Y          = -390,
    ACTOR_01600_SIGHT_REST_REACH       = 1000,
    ACTOR_01600_SIGHT_AWAKE_REACH      = 4000,
    ACTOR_01600_SIGHT_FAR_RADIUS       = 900,
    ACTOR_01600_SIGHT_NEAR_RADIUS      = 100,
    ACTOR_01600_PATH_INITIAL_COMPONENT = 500,
};

/// Attack admission limits: yaw uses 4096-unit turns; lengths use world units.
enum {
    ACTOR_01600_GRAB_BEGIN_YAW  = 256,
    ACTOR_01600_GRAB_TAKE_YAW   = 1024,
    ACTOR_01600_GRAB_REACH      = 1000,
    ACTOR_01600_GRAB_HEIGHT_GAP = 400,
    ACTOR_01600_ATTACK_TURN_YAW = 768,
    ACTOR_01600_LUNGE_YAW       = 512,
    ACTOR_01600_LUNGE_REACH     = 2000,
};

/// Placement modes interpreted during setup; other modes wait for their numbered wave.
enum {
    ACTOR_01600_PLACEMENT_ACTIVE      = 0,
    ACTOR_01600_PLACEMENT_FIRST_WAVE  = 1,
    ACTOR_01600_PLACEMENT_SECOND_WAVE = 2,
    ACTOR_01600_PLACEMENT_SCRIPTED    = 3,
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

static void _actor01600CopyBurstTextures(Task* burstPart, Task* actor);

static void _actor01600StepBehavior(Task* actor);
static void _actor01600StepDeath(Enemy* enemy, Task* actor);
static void _actor01600Task(Task* actor);

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

static s32  _actor01600TryAcquireGrab(Task* actor);
static s32  _actor01600TryBeginGrab(Task* actor, s32 relativeYaw, s32 horizontalDistance, s32 playerIndex);
static void _actor01600Sidestep(Task* actor, s32 lateralDistance);
static s32  _actor01600ResumeRoamIfBlocked(Task* actor);
static s32  _actor01600TryBeginAttackTurn(Task* actor, s32 relativeYaw, s32 unusedDistance);
static s32  _actor01600TryBeginLunge(Task* actor, s32 relativeYaw, s32 horizontalDistance);

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
static void _actor01600Tick(Enemy* enemy, Task* task);
static void _actor01600ConsumeReactions(Task* actor);
static void _actor01600ProcessContacts(Task* actor);
static void _actor01600ApplyDamage(Task* actor, s32 damage);
static void _actor01600StepRoam(Task* actor);
static void _actor01600StepAttack(Task* actor);
static void _actor01600StepAnimation(Task* actor);
static void _actor01600DrawGroundShadow(Task* actor);
static void _actor01600StepTargetAnchor(Task* actor);
static s32  _actor01600StepActivation(Task* actor);
static void _actor01600StepStagedMotion(Task* actor);
static void _actor01600SpawnBurstParts(Task* actor);
static void _actor01600StepRootMotion(Task* actor);
static void _actor01600RefreshBodyColor(Enemy* enemy, Task* actor);
static void _actor01600SquashCorpse(Task* actor);
static void _actor01600StepRecoil(Task* actor);
static void _actor01600ReleaseGrab(Task* actor);
static u8   _actor01600CheckSceneChildrenHidden(void);
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

TaskDesc Actor01600_D127BC = { { { (TASK_BODY_TMD | TASK_DESC_SKIP_AUTO_MODEL_BUFFER), 96 } }, _actor01600Task, { .model = &_gActor01600ScavengerBody } };

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

/// Samples the scavenger's lighting color at a coordinate's cached view-space position.
///
/// Requires a live enemy and an initialized composed cache in `sampleCoord`;
/// this helper does not refresh that cache.
/// Borrows and releases one VECTOR scratch block; the position is not retained.
/// The cursor is published before the position is filled.
static __inline__ void _actor01600SampleColorAtCoord(Enemy* enemy, GfxCoord* sampleCoord)
{
    VECTOR* scratchCursor;
    VECTOR* position;

    scratchCursor = SCRATCH_STACK_CURSOR(VECTOR);
    position      = scratchCursor - 1;

    SCRATCH_STACK_CURSOR(VECTOR) = position;

    position->vx = sampleCoord->workm.t[0];
    position->vy = sampleCoord->workm.t[1];
    position->vz = sampleCoord->workm.t[2];
    worldCoordUpdateActorColor(enemy, position, 0, 0);

    SCRATCH_STACK_CURSOR(u8) = SCRATCH_STACK_CURSOR(u8) + sizeof(*position);
}

static const EnemyTaskFuncTable3 Actor01600_D00004 = {
    { _actor01600Init, _actor01600Tick, _actor01600StepDeath },
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

/// Links the scavenger's widening sight capsule and initializes its one contact.
///
/// Requires zeroed capsule endpoints, an unlinked body, and `sightContacts`
/// equal to `work->sight.contacts`. The root and work outlive the link.
/// Starts with tests off; the subsequent body-sphere setup enables them.
static __inline__ void _actor01600LinkSight(_Actor01600Work* work, GfxCoord* rootCoord, WorldCollisionContact* sightContacts)
{
    work->sight.shape.ends[0].vz     = ACTOR_01600_SIGHT_AWAKE_REACH;
    work->sight.shape.end0Radius     = ACTOR_01600_SIGHT_FAR_RADIUS;
    work->sight.shape.end1Radius     = ACTOR_01600_SIGHT_NEAR_RADIUS;
    work->sight.shape.contacts       = sightContacts;
    work->sight.body.context.capsule = &work->sight.shape;
    work->sight.body.pos.vx          = 0;
    work->sight.body.pos.vy          = -ACTOR_01600_BODY_RADIUS;
    work->sight.body.pos.vz          = 0;
    work->sight.body.key             = 0;
    work->sight.body.radius          = 0;
    work->sight.body.flags           = WORLD_COLLISION_BODY_CAPSULE;
    work->sight.body.coord           = rootCoord;
    worldCollisionLinkBody(WORLD_COLLISION_LIST_ENEMY_ATTACKS, &work->sight.body);
    worldCollisionInitContacts(sightContacts, ARRAY_SIZE(work->sight.contacts), 0);
}

/// Links the scavenger's hit-taking sphere and enables its sight tests.
///
/// `bodyContacts` is the work's complete eight-entry table, also borrowed by
/// the enemy record. Requires an unlinked body and a prepared sight capsule;
/// the subsequent path-probe setup enables the sphere's collision tests.
/// The work and root coordinate remain live until unlinking.
static __inline__ void _actor01600LinkBodySphere(_Actor01600Work* work, GfxCoord* rootCoord, WorldCollisionContact* bodyContacts)
{
    work->bodySphere.body.coord            = rootCoord;
    work->bodySphere.body.context.contacts = bodyContacts;
    work->bodySphere.body.key              = ACTOR_01600_BODY_KEY;
    work->bodySphere.body.radius           = ACTOR_01600_BODY_RADIUS;
    work->bodySphere.body.pos.vx           = 0;
    work->bodySphere.body.pos.vy           = -ACTOR_01600_BODY_RADIUS;
    work->bodySphere.body.pos.vz           = 0;
    work->bodySphere.body.flags            = WORLD_COLLISION_BODY_SPHERE;
    work->sight.body.flags                |= (WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED);
    worldCollisionLinkBody(WORLD_COLLISION_LIST_ENEMY_BODIES, &work->bodySphere.body);
    worldCollisionInitContacts(bodyContacts, ARRAY_SIZE(work->bodySphere.contacts), 0);
}

/// Links the thin heading probe and enables the hit-taking sphere's tests.
///
/// Requires zeroed capsule endpoints, an unlinked probe, a prepared body
/// sphere, and `pathProbeContacts` equal to `work->pathProbe.contacts`.
/// The initial far end is (500, 0, 500) in root-local world units.
/// The subsequent bite setup disables the probe until a heading search;
/// the work and root remain live until unlinking.
static __inline__ void _actor01600LinkPathProbe(_Actor01600Work* work, GfxCoord* rootCoord, WorldCollisionContact* pathProbeContacts)
{
    work->pathProbe.shape.ends[0].vz     = ACTOR_01600_PATH_INITIAL_COMPONENT;
    work->pathProbe.shape.ends[0].vx     = ACTOR_01600_PATH_INITIAL_COMPONENT;
    work->pathProbe.shape.end0Radius     = 1;
    work->pathProbe.shape.end1Radius     = 1;
    work->pathProbe.shape.contacts       = pathProbeContacts;
    work->pathProbe.body.coord           = rootCoord;
    work->pathProbe.body.context.capsule = &work->pathProbe.shape;
    work->pathProbe.body.pos.vx          = 0;
    work->pathProbe.body.pos.vy          = -ACTOR_01600_BODY_RADIUS;
    work->pathProbe.body.pos.vz          = 0;
    work->pathProbe.body.key             = 0;
    work->pathProbe.body.radius          = 0;
    work->pathProbe.body.flags           = WORLD_COLLISION_BODY_CAPSULE;
    work->bodySphere.body.flags         |= (WORLD_COLLISION_BODY_FLOOR_QUERY | WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED);
    worldCollisionLinkBody(WORLD_COLLISION_LIST_ENEMY_BODIES, &work->pathProbe.body);
    worldCollisionInitContacts(pathProbeContacts, ARRAY_SIZE(work->pathProbe.contacts), 0);
}

/// Links the lunge's damaging sphere with tests off and disables the path probe.
///
/// Requires an unlinked bite body, a prepared probe, and `biteContacts` equal
/// to `work->bite.contacts`. The key delivers attack-table entry 1 to a hit
/// player body; the grab's direct bites use entry 0. The work and root
/// coordinate remain live until unlinking.
static __inline__ void _actor01600LinkBiteSphere(_Actor01600Work* work, GfxCoord* rootCoord, WorldCollisionContact* biteContacts)
{
    work->bite.body.coord            = rootCoord;
    work->bite.body.context.contacts = biteContacts;
    work->bite.body.pos.vx           = 0;
    work->bite.body.pos.vy           = ACTOR_01600_BITE_CENTER_Y;
    work->bite.body.pos.vz           = 0;
    work->pathProbe.body.flags      &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ (WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED));
    work->bite.body.key              = damagePackAttackKey(Actor01600_D09F04, 1);
    work->bite.body.radius           = ACTOR_01600_BITE_RADIUS;
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

/// Requests a tick sound at the root's signed-byte pan and depth, in that order.
///
/// Requires a caller-packed script key and a live composed root coordinate.
/// Borrows the coordinate for both queries and retains neither argument.
static __inline__ void _actor01600PlayTickSoundAtRoot(s32 soundKey, const GfxCoord* rootCoord)
{
    s32 soundPan;

    soundPan = (s8)worldCoordGetOriginAudioPan(rootCoord);
    sndEvtRequestScriptStart(soundKey, soundPan, (s8)worldCoordGetOriginAudioDepth(rootCoord));
}

/// Advances an activated scavenger's combat behavior, animation and presentation.
///
/// Activation may consume the tick. Paused actors only refresh color and shadow;
/// hidden actors become unshown and not lockable. Running actors process target
/// tracking, reactions and contacts, then request grounded death or room-specific
/// fall removal before behavior, animation and recoil. Color refreshes every five
/// active ticks or on a dirty view; the root cache supplies shadow and audio.
/// Requires initialized work, its enemy/model and current scene/room state.
static void _actor01600Tick(Enemy* enemy, Task* task)
{
    enum {
        ACTOR_01600_TASK_DEATH          = 2,
        ACTOR_01600_GROUND_RESET_HEIGHT = 100,
        ACTOR_01600_GROUND_RESET_Y      = -10,
        ACTOR_01600_STAGE_3_FALL_Y      = -1000,
        ACTOR_01600_STAGE_4_FALL_Y      = 1000,
        ACTOR_01600_SOUND_SCRIPT_A      = 0x4010000A
    };

    _Actor01600Work* work;
    GfxCoord*        rootCoord;
    TmdObject*       hiddenModel;
    s32              soundKey;
    s32              stageAreaKey;
    u16              colorFrame;

    work      = task->work;
    rootCoord = task->extra.tmd->coords;
    if (!((u8)_actor01600StepActivation(task))) {
        switch (gSceneCombatState.actorControl) {
            case SCENE_COMBAT_ACTORS_RUNNING:
                task->extra.tmd->flags        = 0;
                enemy->node.state.parts.flags = 0;
                break;
            case SCENE_COMBAT_ACTORS_PAUSED:
                _actor01600RefreshBodyColor(enemy, task);
                _actor01600DrawGroundShadow(task);
                return;
            case SCENE_COMBAT_ACTORS_HIDDEN:
                hiddenModel                   = task->extra.tmd;
                hiddenModel->flags           |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
                enemy->node.state.parts.flags = WORLD_TARGET_NOT_LOCKABLE;
                return;
            default:
                break;
        }
        _actor01600StepTargetAnchor(task);
        if (enemy->reactionFlags != 0) {
            _actor01600ConsumeReactions(task);
        }
        _actor01600ProcessContacts(task);
        if (work->airborne == 0) {
            if (work->dead != 0) {
                task->state = ACTOR_01600_TASK_DEATH;
            }
        }
        stageAreaKey = GAME_LOCATION_WORD(gGameSession->location.loc) & GAME_LOCATION_STAGE_AREA_MASK;
        if (stageAreaKey != GAME_LOCATION_KEY(3, 38, 0, 0) && stageAreaKey != GAME_LOCATION_KEY(4, 7, 0, 0) && stageAreaKey != GAME_LOCATION_KEY(4, 1, 0, 0)) {
            if (rootCoord->coord.t[1] >= ACTOR_01600_GROUND_RESET_HEIGHT + 1) {
                rootCoord->coord.t[1] = ACTOR_01600_GROUND_RESET_Y;
            }
        }
        if (((GAME_LOCATION_WORD(gGameSession->location.loc) & GAME_LOCATION_STAGE_AREA_MASK) == GAME_LOCATION_KEY(3, 29, 0, 0)) && (rootCoord->coord.t[1] >= ACTOR_01600_STAGE_3_FALL_Y + 1)) {
            soundKey = (((u16)((Enemy*)task->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | ACTOR_01600_SOUND_SCRIPT_5;
            _actor01600PlayTickSoundAtRoot(soundKey, rootCoord);
            soundKey = (((u16)((Enemy*)task->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | ACTOR_01600_SOUND_SCRIPT_A;
            _actor01600PlayTickSoundAtRoot(soundKey, rootCoord);
            _actor01600ReleaseGrab(task);
            _actor01600Remove(task, 0);
        }
        if ((GAME_LOCATION_WORD(gGameSession->location.loc) & GAME_LOCATION_STAGE_AREA_MASK) == GAME_LOCATION_KEY(4, 1, 0, 0)) {
            if (rootCoord->coord.t[1] > 0) {
                work->shadowHidden = 1;
            }
            if (rootCoord->coord.t[1] >= ACTOR_01600_STAGE_4_FALL_Y + 1) {
                soundKey = (((u16)((Enemy*)task->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | ACTOR_01600_SOUND_SCRIPT_5;
                _actor01600PlayTickSoundAtRoot(soundKey, rootCoord);
                soundKey = (((u16)((Enemy*)task->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | ACTOR_01600_SOUND_SCRIPT_A;
                _actor01600PlayTickSoundAtRoot(soundKey, rootCoord);
                _actor01600ReleaseGrab(task);
                _actor01600Remove(task, 0);
            }
        }
        _actor01600StepBehavior(task);
        _actor01600StepAnimation(task);
        _actor01600StepRecoil(task);
        rootCoord->composeStamp = GRAPHICS_COORD_DIRTY;
        actorRenderComposeCoord(rootCoord);
        colorFrame       = work->colorTimer + 1;
        work->colorTimer = colorFrame;
        if (((s16)colorFrame >= ACTOR_01600_COLOR_INTERVAL) || (gGameSession->viewDirty == 1)) {
            work->colorTimer = 0;
            _actor01600RefreshBodyColor(enemy, task);
        }
        if (work->shadowHidden == 0) {
            _actor01600DrawGroundShadow(task);
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

/// Resolves body contacts, takes player attacks and consumes successful lunge bites.
///
/// Requires linked bodies, composed view-space caches and live player tasks for
/// attack keys. Room pushback uses signed 16.16 corrections; overlap directions
/// use Q12 coefficients. Nonlethal processing consumes and clears body contacts.
/// A lethal explosive reaction retains its scratch reservation until the main
/// loop resets the cursor at the next frame; other exits release the block.
static void _actor01600ProcessContacts(Task* actor)
{
    enum {
        ACTOR_01600_REACTION_BURST_OR_STAGGER  = 4,
        ACTOR_01600_REACTION_STAGGER_ALT       = 5,
        ACTOR_01600_REACTION_BUILDUP_STANDING  = 8,
        ACTOR_01600_REACTION_BUILDUP_KNOCKDOWN = 9,
        ACTOR_01600_HEAVY_HIT_DAMAGE           = 40,
    };

    s32                        horizontalDistance;
    Task**                     playerTasks;
    GfxCoord*                  hitCoord;
    _Actor01600Work*           work;
    Enemy*                     enemy;
    GfxCoord*                  rootCoord;
    s32                        contactIndex;
    _Actor01600ContactScratch* scratch;
    GfxCoord*                  attackerCoord;
    s32                        attackDeltaX, attackDeltaY, attackDeltaZ;
    s32                        damage;
    s32                        yawMagnitude;
    s32                        verticalPush;
    s32                        overlapDepth;
    s32                        positiveDepth;
    s32                        contactDeltaX, contactDeltaZ;
    s16                        hitCooldown;
    s32                        pushbackResult;
    // Keep the buildup-down comparison separate in each reaction branch.
    s32 buildupDownBehavior;
    work           = actor->work;
    scratch        = SCRATCH_STACK_RESERVE_BLOCK(_Actor01600ContactScratch);
    enemy          = actor->spawnArg2.pointer;
    rootCoord      = actor->extra.tmd->coords;
    pushbackResult = worldCollisionResolvePushback(work->bodySphere.contacts, &scratch->delta, ARRAY_SIZE(work->bodySphere.contacts), &scratch->contributorMask);
    hitCoord       = rootCoord + 1;
    switch (pushbackResult) {
        case WORLD_COLLISION_PUSHBACK_GRID_HIT:
            rootCoord->coord.t[0] += scratch->delta.fixed.vx.halves.integer;
            rootCoord->coord.t[1] += scratch->delta.fixed.vy.halves.integer;
            rootCoord->coord.t[2] += scratch->delta.fixed.vz.halves.integer;
            break;
        case WORLD_COLLISION_PUSHBACK_OPPOSED:
            rootCoord->coord.t[0] = work->previousPosition.vx;
            rootCoord->coord.t[1] = work->previousPosition.vy;
            rootCoord->coord.t[2] = work->previousPosition.vz;
            break;
        case WORLD_COLLISION_PUSHBACK_NO_GRID_HIT:
        default:
            break;
    }
    playerTasks = gPlayerActorTasks;
    if (work->hitCooldown != 0) {
        if (--work->hitCooldown <= 0)
            work->hitCooldown = 0;
    }
    // Consume attacks and separate overlapping enemy bodies.
    for (contactIndex = 0; contactIndex < ARRAY_SIZE(work->bodySphere.contacts); contactIndex++) {
        switch (work->bodySphere.contacts[contactIndex].key.parts.kind) {
            case WORLD_COLLISION_CONTACT_ATTACK >> 16:
                if (work->hitCooldown == 0) {
                    attackerCoord            = playerTasks[(u8)work->bodySphere.contacts[contactIndex].key.parts.id >> 7]->extra.tmd->coords;
                    attackDeltaX             = attackerCoord->coord.t[0] - rootCoord->coord.t[0];
                    scratch->delta.vector.vx = attackDeltaX;
                    attackDeltaY             = attackerCoord->coord.t[1] - rootCoord->coord.t[1];
                    scratch->delta.vector.vy = attackDeltaY;
                    attackDeltaZ             = attackerCoord->coord.t[2] - rootCoord->coord.t[2];
                    scratch->delta.vector.vz = attackDeltaZ;
                    damage                   = damageComputePlayerAttack(work->bodySphere.contacts[contactIndex].key.value, SquareRoot0(attackDeltaX * attackDeltaX + attackDeltaY * attackDeltaY + attackDeltaZ * attackDeltaZ), 0, 0);
                    if (damageRollCriticalHit(actor->spawnArg2.pointer, work->bodySphere.contacts[contactIndex].key.value, 0)) {
                        damage *= 4;
                        effectSpawn(EFFECT_CRITICAL_HIT, actor->extra.tmd->coords + 1, 0, 0);
                    }
                    if (work->behavior == ACTOR_01600_BEHAVIOR_ATTACK && work->airborne != 0 && work->verticalSpeed < 0) {
                        damage *= 2;
                        effectSpawn(EFFECT_CRITICAL_HIT, actor->extra.tmd->coords + 1, 3, 0);
                    }
                    damageAccumulateLifeDrainHp(enemy, work->bodySphere.contacts[contactIndex].key.value, damage, 0);
                    _actor01600ApplyDamage(actor, damage);
                    hitCooldown = damageGetPlayerAttackHitCooldown(work->bodySphere.contacts[contactIndex].key.value);
                    if (hitCooldown > 0)
                        work->hitCooldown = hitCooldown;
                    switch (damageGetPlayerAttackReaction(work->bodySphere.contacts[contactIndex].key.value) & 0xFFFF) {
                        case ACTOR_01600_REACTION_BURST_OR_STAGGER:
                        case DAMAGE_PLAYER_REACTION_EXPLOSION:
                            if (enemy->hp <= 0) {
                                work->burstState = ACTOR_01600_BURST_INTACT;
                                _actor01600SpawnBurstParts(actor);
                                work->burstState = ACTOR_01600_BURST_COMPLETE;
                                work->airborne   = 0;
                                // The lethal burst leaves scratch reserved until the next frame reset.
                                return;
                            }
                            damageStartEnemyStagger(actor->spawnArg2.pointer);
                            break;
                        case DAMAGE_PLAYER_REACTION_BUILDUP:
                        case ACTOR_01600_REACTION_BUILDUP_KNOCKDOWN:
                            if (work->behavior != ACTOR_01600_BEHAVIOR_BUILDUP) {
                                buildupDownBehavior = ACTOR_01600_BEHAVIOR_BUILDUP_DOWN;
                                if (work->behavior != buildupDownBehavior) {
                                    damageStartEnemyBuildup(actor->spawnArg2.pointer, work->bodySphere.contacts[contactIndex].key.value, 0);
                                    work->buildupKnocksDown = 1;
                                }
                            }
                            break;
                        case ACTOR_01600_REACTION_BUILDUP_STANDING:
                            if (work->behavior != ACTOR_01600_BEHAVIOR_BUILDUP) {
                                buildupDownBehavior = ACTOR_01600_BEHAVIOR_BUILDUP_DOWN;
                                if (work->behavior != buildupDownBehavior) {
                                    damageStartEnemyBuildup(actor->spawnArg2.pointer, work->bodySphere.contacts[contactIndex].key.value, 0);
                                    work->buildupKnocksDown = 0;
                                }
                            }
                            break;
                        case DAMAGE_PLAYER_REACTION_STAGGER:
                        case ACTOR_01600_REACTION_STAGGER_ALT:
                            if (work->behavior != ACTOR_01600_BEHAVIOR_BUILDUP) {
                                buildupDownBehavior = ACTOR_01600_BEHAVIOR_BUILDUP_DOWN;
                                if (work->behavior != buildupDownBehavior) {
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
                    if (damage >= ACTOR_01600_HEAVY_HIT_DAMAGE && work->buildupKnocksDown == 0) {
                        damageStartEnemyStagger(enemy);
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
                        yawMagnitude          = _actor01600MeasureTarget(actor, &horizontalDistance);
                        if (yawMagnitude < 0)
                            yawMagnitude = -yawMagnitude;
                        if (yawMagnitude < ACTOR_TRANSFORM_ANGLE_HALF_TURN / 2) {
                            work->animRequest  = ACTOR_01600_ANIMATION_STAGGER;
                            work->forwardSpeed = -40;
                        } else {
                            work->animRequest  = ACTOR_01600_ANIMATION_THROWN_FORWARD;
                            work->forwardSpeed = 40;
                        }
                        work->attackAction = ACTOR_01600_ACTION_KNOCKBACK;
                    }
                    effectSpawnHit(damageGetPlayerAttackEffectId(work->bodySphere.contacts[contactIndex].key.value), hitCoord, 0, &work->hitEffect);
                }
                break;
            case WORLD_COLLISION_CONTACT_ENEMY_BODY >> 16:
                contactDeltaX            = rootCoord->workm.t[0] - work->bodySphere.contacts[contactIndex].point.vx;
                scratch->delta.vector.vy = 0;
                scratch->delta.vector.vx = contactDeltaX;
                contactDeltaZ            = rootCoord->workm.t[2] - work->bodySphere.contacts[contactIndex].point.vz;
                scratch->delta.vector.vz = contactDeltaZ;
                overlapDepth             = contactDeltaX * contactDeltaX + contactDeltaZ * contactDeltaZ;
                overlapDepth             = SquareRoot0(overlapDepth);
                overlapDepth             = -overlapDepth;
                overlapDepth            += work->bodySphere.contacts[contactIndex].distance;
                positiveDepth            = overlapDepth;
                if (overlapDepth <= 0)
                    positiveDepth = 0;
                overlapDepth             = positiveDepth;
                scratch->delta.vector.vx = rootCoord->workm.t[0] - work->bodySphere.contacts[contactIndex].point.vx;
                scratch->delta.vector.vy = rootCoord->workm.t[1] - work->bodySphere.contacts[contactIndex].point.vy;
                scratch->delta.vector.vz = rootCoord->workm.t[2] - work->bodySphere.contacts[contactIndex].point.vz;
                VectorNormal(&scratch->delta.vector, &scratch->normal);
                ApplyTransposeMatrixLV(&Gp_GridParams->viewCoord->workm, &scratch->normal, &scratch->delta.vector);
                if (work->animRequest == ACTOR_01600_ANIMATION_ROAM_REPEAT || work->animRequest == ACTOR_01600_ANIMATION_ADVANCE || work->animRequest == ACTOR_01600_ANIMATION_ZIGZAG) {
                    rootCoord->coord.t[0] += (overlapDepth * scratch->delta.vector.vx) >> 12;
                    verticalPush           = overlapDepth * scratch->delta.vector.vy;
                    if (verticalPush < 0)
                        rootCoord->coord.t[1] += verticalPush >> 12;
                    rootCoord->coord.t[2] += (overlapDepth * scratch->delta.vector.vz) >> 12;
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
        work->bodySphere.body.pos.vy = -ACTOR_01600_BODY_RADIUS;
        work->bodySphere.body.radius = ACTOR_01600_BODY_RADIUS;
        work->biteLanded             = 1;
        work->bite.body.flags       &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        worldCollisionClearContacts(work->bite.contacts);
        if (work->animFrame < 15) {
            work->attackAction    = ACTOR_01600_ACTION_KNOCKBACK;
            work->animBlendFrames = 0;
            work->verticalSpeed  += 20;
            yawMagnitude          = _actor01600MeasureTarget(actor, &horizontalDistance);
            if (yawMagnitude < 0)
                yawMagnitude = -yawMagnitude;
            if (yawMagnitude < ACTOR_TRANSFORM_ANGLE_HALF_TURN / 2) {
                work->animRequest  = ACTOR_01600_ANIMATION_STAGGER;
                work->forwardSpeed = -40;
            } else {
                work->animRequest  = ACTOR_01600_ANIMATION_THROWN_FORWARD;
                work->forwardSpeed = 40;
            }
        }
    }
    SCRATCH_STACK_RELEASE_BLOCK(_Actor01600ContactScratch);
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

/// Requests a packed sound-script key at the root's current signed-byte pan and depth.
///
/// Requires a fully packed key and an initialized root cache. Reads its composed
/// view-space position without refreshing it. Samples pan before depth,
/// narrowing each to a signed byte, and retains neither argument.
static __inline__ void _actor01600PlaySoundAtRoot(s32 soundKey, GfxCoord* rootCoord)
{
    s32 soundPan;

    soundPan = (s8)worldCoordGetOriginAudioPan(rootCoord);
    sndEvtRequestScriptStart(soundKey, soundPan, (s8)worldCoordGetOriginAudioDepth(rootCoord));
}

/// Advances the scavenger's behavior, root motion and periodic idle sound cues.
///
/// Roam and attack update their actions before root motion. Stagger recovers
/// after 91 ticks; buildup waits for its damage timer. Posed behavior selects its
/// pose without stepping the root. Certain animation requests suppress idle
/// cues; other requests draw one of three sounds or silence every 20 ticks.
static void _actor01600StepBehavior(Task* actor)
{
    _Actor01600Work* work;
    GfxCoord*        rootCoord;
    s32              soundKey;
    s32              animationRequest;
    u16              soundChoice;
    s16              soundCountdown;

    work      = actor->work;
    rootCoord = actor->extra.tmd->coords;

    switch (work->behavior) {
        case ACTOR_01600_BEHAVIOR_ROAM:
            _actor01600StepRoam(actor);
            _actor01600StepRootMotion(actor);
            break;
        case ACTOR_01600_BEHAVIOR_ATTACK:
            _actor01600StepAttack(actor);
            _actor01600StepRootMotion(actor);
            break;
        case ACTOR_01600_BEHAVIOR_STAGGER:
            _actor01600ReleaseGrab(actor);
            work->stateTimer = work->stateTimer + 1;
            if (work->animRequest == ACTOR_01600_ANIMATION_STAGGER && work->animFrame < 0x11) {
                work->forwardSpeed = -0x3C;
                _actor01600StepRootMotion(actor);
            } else {
                work->forwardSpeed = 0;
            }
            if (work->stateTimer == 0x28) {
                work->animRate    = ANIMATION_RATE_ONE;
                work->animRequest = ACTOR_01600_ANIMATION_RISE_BACK;
            }
            if (work->stateTimer >= 0x5B) {
                work->animRequest       = ACTOR_01600_ANIMATION_LOOK_AROUND;
                work->behavior          = ACTOR_01600_BEHAVIOR_ROAM;
                work->stateTimer        = 0;
                work->sight.body.flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;
            }
            work->forwardSpeed = 0;
            _actor01600StepRootMotion(actor);
            break;
        case ACTOR_01600_BEHAVIOR_BUILDUP:
            _actor01600ReleaseGrab(actor);
            work->animRequest = ACTOR_01600_ANIMATION_BUILDUP;
            if (damageTickEnemyBuildup(actor->spawnArg2.pointer) != 0) {
                work->behavior          = ACTOR_01600_BEHAVIOR_ROAM;
                work->animRequest       = ACTOR_01600_ANIMATION_LOOK_AROUND;
                work->sight.body.flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;
            }
            work->forwardSpeed = 0;
            _actor01600StepRootMotion(actor);
            break;
        case ACTOR_01600_BEHAVIOR_POSED:
            work->animRequest = ACTOR_01600_ANIMATION_POSED;
            break;
        case ACTOR_01600_BEHAVIOR_BUILDUP_DOWN:
            _actor01600ReleaseGrab(actor);
            if (work->animRequest == ACTOR_01600_ANIMATION_STAGGER) {
                if (work->animFrame >= 0x2C && work->recoilActive == 0 &&
                    ((u16)work->animFrame & 2)) {
                    work->recoilActive      = 1;
                    gRandomLcgState         = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                    work->recoilRotation.vx = ((gRandomLcgState >> 11) & 0x60) + 0x20;
                }
            } else if (work->animRequest == ACTOR_01600_ANIMATION_RISE_BACK) {
                if (work->animFrame >= 0x32) {
                    work->behavior          = ACTOR_01600_BEHAVIOR_ROAM;
                    work->animRequest       = ACTOR_01600_ANIMATION_LOOK_AROUND;
                    work->sight.body.flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;
                }
            }
            if (damageTickEnemyBuildup(actor->spawnArg2.pointer) != 0) {
                work->animRequest = ACTOR_01600_ANIMATION_RISE_BACK;
            }
            work->forwardSpeed = 0;
            _actor01600StepRootMotion(actor);
            break;
    }

    animationRequest = work->animRequest;
    if (animationRequest == ACTOR_01600_ANIMATION_REST || animationRequest == ACTOR_01600_ANIMATION_RISE_BACK || animationRequest == ACTOR_01600_ANIMATION_RISE_FORWARD || animationRequest == ACTOR_01600_ANIMATION_QUIET_16 ||
        animationRequest == ACTOR_01600_ANIMATION_BUILDUP || animationRequest == ACTOR_01600_ANIMATION_GRAB_APPROACH || animationRequest == ACTOR_01600_ANIMATION_GRAB_HOLD || animationRequest == ACTOR_01600_ANIMATION_GRAB_RELEASE ||
        animationRequest == ACTOR_01600_ANIMATION_LUNGE_LAND || animationRequest == ACTOR_01600_ANIMATION_SETTLE) {
        return;
    }
    soundCountdown       = work->idleSoundTimer - 1;
    work->idleSoundTimer = soundCountdown;
    if (soundCountdown != 0) {
        return;
    }
    gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
    soundChoice     = (gRandomLcgState >> 16) % 5;
    switch (soundChoice) {
        case 0:
            soundKey = ((((Enemy*)actor->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | ACTOR_01600_SOUND_IDLE_1;
            _actor01600PlaySoundAtRoot(soundKey, rootCoord);
            break;
        case 1:
            soundKey = ((((Enemy*)actor->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | ACTOR_01600_SOUND_IDLE_2;
            _actor01600PlaySoundAtRoot(soundKey, rootCoord);
            break;
        case 2:
            soundKey = ((((Enemy*)actor->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | ACTOR_01600_SOUND_IDLE_3;
            _actor01600PlaySoundAtRoot(soundKey, rootCoord);
            break;
    }
    work->idleSoundTimer = ACTOR_01600_IDLE_SOUND_PERIOD;
}

/// Advances the scavenger's resting, waking, heading search and roaming hop animations.
///
/// Consumes the sight contact once; an alert selects attack behavior and
/// disables both sensors before the current roam animation finishes its tick.
/// Noise wakes an active placement after its reaction delay. A clear heading
/// selects a turn or up to three hops, then another heading search.
/// Updates requests and speeds; animation playback and root motion run afterwards.
static void _actor01600StepRoam(Task* actor)
{
    Enemy*                 enemy;
    _Actor01600Work*       work;
    GfxCoord*              rootCoord;
    TmdObject*             model;
    s16                    frameOffset;
    s16                    reactionFramesLeft;
    s16                    roamTimer;
    s16                    animationCase;
    s16                    jumpHeight;
    s16                    repeatJumpHeight;
    s16                    frame;
    s32                    headingResult;
    s32                    soundKey;
    u8*                    scratchHead;
    s32                    turnMagnitude;
    WorldCollisionContact* sightContacts;
    u16                    probeFlags;
    u16                    hopFrame;

    enum {
        ACTOR_01600_ROAM_SCRATCH_BYTES  = 8,
        ACTOR_01600_ROAM_SOUND_SCRIPT_1 = 0x40100001,
        ACTOR_01600_ROAM_SOUND_SCRIPT_2 = 0x40100002,
        ACTOR_01600_ROAM_SOUND_SCRIPT_3 = 0x40100003,
        ACTOR_01600_ROAM_SOUND_SCRIPT_4 = 0x40100004,
    };

    work = actor->work;
    // The original reserves eight untouched bytes across all nested calls.
    scratchHead              = SCRATCH_STACK_CURSOR(u8);
    sightContacts            = work->sight.contacts;
    SCRATCH_STACK_CURSOR(u8) = scratchHead - ACTOR_01600_ROAM_SCRATCH_BYTES;
    model                    = actor->extra.tmd;
    rootCoord                = model->coords;
    enemy                    = actor->spawnArg2.pointer;
    // Consume sight results before advancing the roaming animation.
    if (worldCollisionCountContactsByKind(sightContacts, WORLD_COLLISION_CONTACT_PLAYER_BODY) != 0) {
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
    worldCollisionClearContacts(sightContacts);
    if (work->noiseHeard == 1) {
        reactionFramesLeft  = (u16)work->reactionDelay - 1;
        work->reactionDelay = reactionFramesLeft;
        if (reactionFramesLeft == 0) {
            work->animFrame   = 0;
            work->animRequest = ACTOR_01600_ANIMATION_WAKE;
            work->noiseHeard  = 0;
        }
    }
    roamTimer        = (u16)work->stateTimer + 1;
    work->stateTimer = roamTimer;
    if (roamTimer >= 0x1F) {
        work->stateTimer = 0;
    }
    // Clip-relative timing controls the wake, heading search, turns and hop run.
    animationCase = (u16)work->animRequest - ACTOR_01600_ANIMATION_REST;
    switch (animationCase) {
        case ACTOR_01600_ANIMATION_REST - ACTOR_01600_ANIMATION_REST:
            work->sight.shape.ends[0].vz = ACTOR_01600_SIGHT_REST_REACH;
            work->forwardSpeed           = 0;
            work->animRate               = ANIMATION_RATE_ONE;
            if (work->animFrame >= 0x3E) {
                work->animFrame = 0;
            }
            if (enemy->place->mode == ACTOR_01600_PLACEMENT_ACTIVE) {
                if ((gSceneCombatState.signals.bytes.actionFlags & SCENE_COMBAT_ACTION_NOISE) || (gSceneCombatState.signals.packed & SCENE_COMBAT_SIGNAL_CAST_FOOTSTEP_OR_ALERT)) {
                    work->noiseHeard = 1;
                }
            }
            break;
        case ACTOR_01600_ANIMATION_WAKE - ACTOR_01600_ANIMATION_REST:
            work->sight.shape.ends[0].vz = ACTOR_01600_SIGHT_AWAKE_REACH;
            work->forwardSpeed           = 0;
            work->animRate               = ANIMATION_RATE_ONE;
            work->animBlendFrames        = 0;
            if (work->animFrame >= 0x36) {
                work->animRequest           = ACTOR_01600_ANIMATION_LOOK_AROUND;
                work->animFrame             = 0;
                work->pathSearchPhase       = ACTOR_01600_PATH_SEARCH_PROBE_TARGET;
                work->field_514             = 1;
                work->pathProbe.body.flags |= (WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED);
            }
            break;
        case ACTOR_01600_ANIMATION_LOOK_AROUND - ACTOR_01600_ANIMATION_REST:
            work->forwardSpeed    = 0;
            work->animRate        = ANIMATION_RATE_ONE;
            work->animBlendFrames = 0;
            headingResult         = _actor01600SearchClearHeading(actor) & 0xFF;
            if (headingResult != 0) {
                probeFlags                 = work->pathProbe.body.flags & (WORLD_COLLISION_BODY_FLAGS_MASK ^ (WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED));
                work->pathProbe.body.flags = probeFlags;
                if (headingResult != ACTOR_01600_HEADING_BLOCKED) {
                    turnMagnitude   = work->turnRequest;
                    work->hopCount  = 0U;
                    turnMagnitude   = abs(turnMagnitude);
                    work->animFrame = 0;
                    if ((turnMagnitude >= 0x201) || ((headingResult & 0xF) == ACTOR_01600_HEADING_ARC)) {
                        work->turnMode = ACTOR_01600_TURN_REQUEST_BEGIN;
                        if ((headingResult & 0xF0) == ACTOR_01600_HEADING_DIRECTION) {
                            work->animRequest = ACTOR_01600_ANIMATION_TURN;
                        } else {
                            work->animRequest = ACTOR_01600_ANIMATION_TURN_WRAPPED;
                        }
                    } else {
                        work->turnMode    = ACTOR_01600_TURN_TRACK_TARGET;
                        work->animRequest = ACTOR_01600_ANIMATION_ROAM_HOP;
                    }
                } else {
                    work->pathProbe.body.flags = probeFlags | (WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED);
                    work->pathSearchPhase      = ACTOR_01600_PATH_SEARCH_PROBE_TARGET;
                }
            }
            if (work->animFrame >= 0x57) {
                work->animFrame   = 0;
                work->animPlaying = 0;
                work->animRequest = ACTOR_01600_ANIMATION_LOOK_AROUND;
            }
            break;
        case ACTOR_01600_ANIMATION_ROAM_IDLE - ACTOR_01600_ANIMATION_REST:
            work->animBlendFrames = 0;
            work->animRate        = ANIMATION_RATE_ONE;
            work->forwardSpeed    = 0;
            if (work->animFrame >= 0x3D) {
                work->animFrame   = 0;
                work->animRequest = ACTOR_01600_ANIMATION_ROAM_IDLE;
            }
            break;
        case ACTOR_01600_ANIMATION_ROAM_HOP - ACTOR_01600_ANIMATION_REST:
            work->animRate        = ANIMATION_RATE_ONE;
            work->animBlendFrames = 4;
            if (work->airborne != 0) {
                if (work->animFrame >= 0xC) {
                    work->verticalSpeed = (u16)work->verticalSpeed + 0xF;
                } else {
                    work->verticalSpeed = (u16)work->verticalSpeed + 0xA;
                }
                jumpHeight       = (u16)work->jumpHeight + (u16)work->verticalSpeed;
                work->jumpHeight = jumpHeight;
                if (jumpHeight >= 0) {
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
            hopFrame = (u16)work->animFrame;
            if ((u32)(hopFrame - 5) < 0x10U) {
                if ((s16)hopFrame >= 0xC) {
                    work->forwardSpeed = 0x5A;
                } else {
                    work->forwardSpeed = 0x3C;
                }
                _actor01600StepTurn(actor);
            } else {
                work->forwardSpeed = 0;
            }
            if (work->animFrame == 0x14) {
                soundKey = (((u16)((Enemy*)actor->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | ACTOR_01600_ROAM_SOUND_SCRIPT_2;
                _actor01600PlaySoundAtRoot(soundKey, rootCoord);
            }
            if (work->animFrame >= 0x15) {
                soundKey = (((u16)((Enemy*)actor->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | ACTOR_01600_ROAM_SOUND_SCRIPT_1;
                _actor01600PlaySoundAtRoot(soundKey, rootCoord);
                work->animRequest      = ACTOR_01600_ANIMATION_ROAM_REPEAT;
                work->airborne         = 0;
                work->verticalSpeed    = 0;
                work->jumpHeight       = 0;
                work->hopFrameOffset   = 0;
                work->animFrame        = 0;
                work->hopCount         = (u16)(work->hopCount + 1);
                work->sight.body.flags = (work->sight.body.flags | WORLD_COLLISION_BODY_PAIR_ENABLED) & (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_GRID_ENABLED);
            }
            break;
        case ACTOR_01600_ANIMATION_ROAM_REPEAT - ACTOR_01600_ANIMATION_REST:
            work->animRate = ANIMATION_RATE_ONE;
            if (work->airborne != 0) {
                if (work->animFrame >= (work->hopFrameOffset + 0xC)) {
                    work->verticalSpeed = (u16)work->verticalSpeed + 0xF;
                } else {
                    work->verticalSpeed = (u16)work->verticalSpeed + 0xA;
                }
                repeatJumpHeight = (u16)work->jumpHeight + (u16)work->verticalSpeed;
                work->jumpHeight = repeatJumpHeight;
                if (repeatJumpHeight >= 0) {
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
                soundKey = (((u16)((Enemy*)actor->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | ACTOR_01600_ROAM_SOUND_SCRIPT_2;
                _actor01600PlaySoundAtRoot(soundKey, rootCoord);
            }
            if (work->animFrame >= (work->hopFrameOffset + 0x17)) {
                soundKey = (((u16)((Enemy*)actor->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | ACTOR_01600_ROAM_SOUND_SCRIPT_1;
                _actor01600PlaySoundAtRoot(soundKey, rootCoord);
                work->hopCount        = (u16)(work->hopCount + 1);
                work->animBlendFrames = 0;
                work->airborne        = 0;
                work->verticalSpeed   = 0;
                work->jumpHeight      = 0;
                work->field_514       = 0;
                work->animFrame       = 0;
                work->hopFrameOffset  = -3;
                if ((s16)work->hopCount >= 3) {
                    work->animRequest = ACTOR_01600_ANIMATION_ROAM_LAND;
                } else {
                    work->animRequest = ACTOR_01600_ANIMATION_ROAM_REPEAT;
                }
                work->sight.body.flags = (work->sight.body.flags | WORLD_COLLISION_BODY_PAIR_ENABLED) & (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_GRID_ENABLED);
            }
            break;
        case ACTOR_01600_ANIMATION_ROAM_LAND - ACTOR_01600_ANIMATION_REST:
            work->animRate        = ANIMATION_RATE_ONE;
            work->animBlendFrames = 4;
            work->forwardSpeed    = 0;
            if (work->animFrame >= 0xA) {
                work->animRequest           = ACTOR_01600_ANIMATION_LOOK_AROUND;
                work->field_514             = 1;
                work->alerted               = 0;
                work->pathSearchPhase       = ACTOR_01600_PATH_SEARCH_PROBE_TARGET;
                work->pathProbe.body.flags |= (WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED);
                work->sight.body.flags      = (work->sight.body.flags | WORLD_COLLISION_BODY_PAIR_ENABLED) & (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_GRID_ENABLED);
            }
            break;
        case ACTOR_01600_ANIMATION_TURN - ACTOR_01600_ANIMATION_REST:
        case ACTOR_01600_ANIMATION_TURN_WRAPPED - ACTOR_01600_ANIMATION_REST:
            work->animRate        = ANIMATION_RATE_ONE;
            work->animBlendFrames = 0;
            if (work->animRequest == ACTOR_01600_ANIMATION_TURN) {
                if (work->animFrame == 0xF) {
                    soundKey = (((u16)((Enemy*)actor->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | ACTOR_01600_ROAM_SOUND_SCRIPT_3;
                    _actor01600PlaySoundAtRoot(soundKey, rootCoord);
                }
                if (work->animFrame == 0x11) {
                    soundKey = (((u16)((Enemy*)actor->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | ACTOR_01600_ROAM_SOUND_SCRIPT_4;
                    _actor01600PlaySoundAtRoot(soundKey, rootCoord);
                }
            } else {
                if (work->animFrame == 0xF) {
                    soundKey = (((u16)((Enemy*)actor->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | ACTOR_01600_ROAM_SOUND_SCRIPT_4;
                    _actor01600PlaySoundAtRoot(soundKey, rootCoord);
                }
                if (work->animFrame == 0x12) {
                    soundKey = (((u16)((Enemy*)actor->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | ACTOR_01600_ROAM_SOUND_SCRIPT_3;
                    _actor01600PlaySoundAtRoot(soundKey, rootCoord);
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
                    work->animRequest = ACTOR_01600_ANIMATION_ROAM_HOP;
                }
            }
            break;
        case ACTOR_01600_ANIMATION_HIT_WAKE - ACTOR_01600_ANIMATION_REST:
            work->animBlendFrames = 0;
            work->animRate        = ANIMATION_RATE_ONE;
            work->forwardSpeed    = 0;
            if (work->animFrame >= 0x28) {
                work->alerted     = 1;
                work->animFrame   = 0;
                work->animRequest = ACTOR_01600_ANIMATION_ROAM_HOP;
            }
            break;
        default:
            break;
    }
    SCRATCH_STACK_RELEASE_BYTES(ACTOR_01600_ROAM_SCRATCH_BYTES);
}

/// Advances approach, turns, lunges, grabs and knockback for an attacking scavenger.
///
/// Animation ticks drive motion, bite arming and target release. Vertical speeds
/// and offsets retain halfword wrap; negative Y speed rises. Grab targets must
/// keep their model coordinate 17 live through release. The ordinary release
/// clears the package-wide grab latch; early target-death exits retain it.
/// Playback and root-motion integration run after this action update.
static void _actor01600StepAttack(Task* actor)
{
    enum {
        ACTOR_01600_GRAB_BITE_PERIOD        = 20,
        ACTOR_01600_GRAB_RELEASE_BITE_COUNT = 5,
        ACTOR_01600_GRAB_EFFECT_ATTACK_KEY  = 0x1001,
        ACTOR_01600_GRAB_OFFSET_COORD       = 17,
        ACTOR_01600_ATTACK_BODY_RADIUS      = 600,
    };

    PlayerStatus*    playerStatus = &gPlayerStatus;
    s32              backwardSpeed;
    s32              playerReleaseHeight;
    s32              companionReleaseHeight;
    SVECTOR          biteEffectOffset;
    s32              horizontalDistance;
    Enemy*           enemy;
    _Actor01600Work* work;
    GfxCoord*        effectCoord;
    GfxCoord*        rootCoord;
    s16              grabTargetIndex;
    s16              lungeHeight;
    s16              recoveryLungeHeight;
    s16              knockbackFrame;
    s16              attackAction;
    s16              recoveryAnimation;
    s16              takeoffFrame;
    s16              grabAnimationSelector;
    s32              soundKey;
    s32              nearestPlayerIndex;
    s32              relativeYaw;
    s32              soundScript;
    u16              biteTimer;
    u16              lungeFrame;
    u16              releaseFrame;
    GfxCoord*        heldRootCoord;
    GfxCoord*        releaseRootCoord;
    GfxCoord*        heldGrabOffset;
    GfxCoord*        releaseGrabOffset;

    enemy              = actor->spawnArg2.pointer;
    work               = actor->work;
    rootCoord          = actor->extra.tmd->coords;
    nearestPlayerIndex = _actor01600SelectNearestPlayer(actor) & 0xFF;
    effectCoord        = &actor->extra.tmd->coords[2];
    memset(&biteEffectOffset, 0, sizeof(biteEffectOffset));
    biteEffectOffset.vy = 0x32;
    attackAction        = work->attackAction;
    switch (attackAction) {
        case ACTOR_01600_ACTION_ADVANCE:
            work->animBlendFrames = 4;
            work->animRequest     = ACTOR_01600_ANIMATION_ADVANCE;
            work->forwardSpeed    = 0;
            work->animRate        = ANIMATION_RATE_ONE;
            if ((u32)((u16)work->animFrame - 9) < 9U) {
                work->forwardSpeed = 0x3C;
                _actor01600StepTurn(actor);
                _actor01600Sidestep(actor, 0x46);
                if (work->targetAnchorPhase == ACTOR_01600_TARGET_ANCHOR_FOLLOW) {
                    work->targetAnchorPhase = ACTOR_01600_TARGET_ANCHOR_DETACH;
                }
            }
            if (work->animFrame >= 0x12) {
                soundKey = (((u16)((Enemy*)actor->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | ACTOR_01600_SOUND_SCRIPT_4;
                _actor01600PlaySoundAtRoot(soundKey, rootCoord);
                if (_actor01600ResumeRoamIfBlocked(actor) & 0xFF) {
                    break;
                }
                work->attackAction = ACTOR_01600_ACTION_ZIGZAG;
                work->animFrame    = 0;
                work->forwardSpeed = 0;
                work->animRequest  = ACTOR_01600_ANIMATION_ROAM_REPEAT;
            }
            relativeYaw = _actor01600MeasureTarget(actor, &horizontalDistance);
            if (!(_actor01600TryBeginAttackTurn(actor, relativeYaw, horizontalDistance) & 0xFF)) {
                if (_actor01600TryBeginGrab(actor, relativeYaw, horizontalDistance, nearestPlayerIndex) & 0xFF) {
                    if (work->animFrame >= 0xA) {
                        soundKey = (((u16)((Enemy*)actor->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | ACTOR_01600_SOUND_SCRIPT_3;
                        _actor01600PlaySoundAtRoot(soundKey, rootCoord);
                        soundKey = (((u16)((Enemy*)actor->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | ACTOR_01600_SOUND_SCRIPT_4;
                        _actor01600PlaySoundAtRoot(soundKey, rootCoord);
                        work->animFrame = 0;
                        return;
                    }
                } else if ((_actor01600TryBeginLunge(actor, relativeYaw, horizontalDistance) & 0xFF) && (work->animFrame >= 0xA)) {
                    soundKey = (((u16)((Enemy*)actor->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | ACTOR_01600_SOUND_SCRIPT_3;
                    _actor01600PlaySoundAtRoot(soundKey, rootCoord);
                    soundKey = (((u16)((Enemy*)actor->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | ACTOR_01600_SOUND_SCRIPT_4;
                    _actor01600PlaySoundAtRoot(soundKey, rootCoord);
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
            work->animRate        = ANIMATION_RATE_ONE;
            work->forwardSpeed    = 0x1E;
            work->animRequest     = ACTOR_01600_ANIMATION_ZIGZAG;
            if ((u32)((u16)work->animFrame - 8) < 8U) {
                work->forwardSpeed = 0x3C;
                _actor01600StepTurn(actor);
                _actor01600Sidestep(actor, -0x5A);
                if (work->targetAnchorPhase == ACTOR_01600_TARGET_ANCHOR_FOLLOW) {
                    work->targetAnchorPhase = ACTOR_01600_TARGET_ANCHOR_DETACH;
                }
            }
            if (work->animFrame == 0x12) {
                soundKey = (((u16)((Enemy*)actor->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | ACTOR_01600_SOUND_SCRIPT_3;
                _actor01600PlaySoundAtRoot(soundKey, rootCoord);
            }
            if ((u32)((u16)work->animFrame - 0x1A) < 9U) {
                work->forwardSpeed = 0x3C;
                _actor01600StepTurn(actor);
                _actor01600Sidestep(actor, 0x46);
                if (work->targetAnchorPhase == ACTOR_01600_TARGET_ANCHOR_FOLLOW) {
                    work->targetAnchorPhase = ACTOR_01600_TARGET_ANCHOR_DETACH;
                }
            }
            if (work->animFrame >= 0x22) {
                soundKey = (((u16)((Enemy*)actor->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | ACTOR_01600_SOUND_SCRIPT_4;
                _actor01600PlaySoundAtRoot(soundKey, rootCoord);
                work->animFrame = 0;
                if (_actor01600ResumeRoamIfBlocked(actor) & 0xFF) {
                    return;
                }
            }
            relativeYaw = _actor01600MeasureTarget(actor, &horizontalDistance);
            if (!(_actor01600TryBeginAttackTurn(actor, relativeYaw, horizontalDistance) & 0xFF)) {
                if (_actor01600TryBeginGrab(actor, relativeYaw, horizontalDistance, nearestPlayerIndex) & 0xFF) {
                    if (work->animFrame >= 8) {
                        soundKey = (((u16)((Enemy*)actor->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | ACTOR_01600_SOUND_SCRIPT_3;
                        _actor01600PlaySoundAtRoot(soundKey, rootCoord);
                        soundKey = (((u16)((Enemy*)actor->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | ACTOR_01600_SOUND_SCRIPT_4;
                        _actor01600PlaySoundAtRoot(soundKey, rootCoord);
                        work->animFrame = 0;
                        return;
                    }
                } else if ((_actor01600TryBeginLunge(actor, relativeYaw, horizontalDistance) & 0xFF) && (work->animFrame >= 8)) {
                    soundKey = (((u16)((Enemy*)actor->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | ACTOR_01600_SOUND_SCRIPT_3;
                    _actor01600PlaySoundAtRoot(soundKey, rootCoord);
                    soundKey = (((u16)((Enemy*)actor->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | ACTOR_01600_SOUND_SCRIPT_4;
                    _actor01600PlaySoundAtRoot(soundKey, rootCoord);
                    work->animFrame = 0;
                    return;
                }
            }
            break;
        case ACTOR_01600_ACTION_TURN:
            work->animRequest     = ACTOR_01600_ANIMATION_TURN;
            work->animRate        = ANIMATION_RATE_ONE;
            work->animBlendFrames = 0;
            work->forwardSpeed    = 0;
            if (work->animFrame == 0xF) {
                soundKey = (((u16)((Enemy*)actor->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | ACTOR_01600_SOUND_SCRIPT_3;
                _actor01600PlaySoundAtRoot(soundKey, rootCoord);
            }
            if (work->animFrame == 0x11) {
                soundScript = ACTOR_01600_SOUND_SCRIPT_4;
                soundKey    = (((u16)((Enemy*)actor->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | soundScript;
                _actor01600PlaySoundAtRoot(soundKey, rootCoord);
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
            work->animRequest     = ACTOR_01600_ANIMATION_TURN_WRAPPED;
            work->animRate        = ANIMATION_RATE_ONE;
            work->animBlendFrames = 0;
            work->forwardSpeed    = 0;
            if (work->animFrame == 0xF) {
                soundKey = (((u16)((Enemy*)actor->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | ACTOR_01600_SOUND_SCRIPT_4;
                _actor01600PlaySoundAtRoot(soundKey, rootCoord);
            }
            if (work->animFrame == 0x12) {
                soundScript = ACTOR_01600_SOUND_SCRIPT_3;
                soundKey    = (((u16)((Enemy*)actor->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | soundScript;
                _actor01600PlaySoundAtRoot(soundKey, rootCoord);
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
            work->animRate        = ANIMATION_RATE_ONE;
            work->animRequest     = ACTOR_01600_ANIMATION_SETTLE;
            work->forwardSpeed    = 0;
            work->animBlendFrames = 0;
            if (work->animFrame == 1) {
                soundKey = (((u16)((Enemy*)actor->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | ACTOR_01600_SOUND_SCRIPT_E;
                _actor01600PlaySoundAtRoot(soundKey, rootCoord);
            }
            if (work->animFrame >= 0x25) {
                work->attackAction    = ACTOR_01600_ACTION_ADVANCE;
                work->animFrame       = 0;
                work->shadowUnderBody = 0;
                return;
            }
            break;
        case ACTOR_01600_ACTION_LUNGE:
            work->animRate        = ANIMATION_RATE_ONE;
            work->forwardSpeed    = 0;
            work->animBlendFrames = 0;
            work->animRequest     = ACTOR_01600_ANIMATION_LUNGE;
            if (work->airborne != 0) {
                work->animRate = ACTOR_01600_FAST_ANIMATION_RATE;
                if (work->verticalSpeed < 0) {
                    work->verticalSpeed = (u16)work->verticalSpeed + 0xF;
                } else {
                    work->verticalSpeed = (u16)work->verticalSpeed + 0x14;
                }
                lungeHeight      = (u16)work->jumpHeight + (u16)work->verticalSpeed;
                work->jumpHeight = lungeHeight;
                if (lungeHeight >= 0) {
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
                soundKey = (((u16)((Enemy*)actor->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | ACTOR_01600_SOUND_SCRIPT_1;
                _actor01600PlaySoundAtRoot(soundKey, rootCoord);
                soundKey = (((u16)((Enemy*)actor->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | ACTOR_01600_SOUND_SCRIPT_2;
                _actor01600PlaySoundAtRoot(soundKey, rootCoord);
                work->airborne      = 0;
                work->verticalSpeed = 0;
                work->jumpHeight    = 0;
            }
            lungeFrame = (u16)work->animFrame;
            if ((u32)(lungeFrame - 8) < 0x17U) {
                if (work->biteLanded == 0) {
                    if ((s16)lungeFrame < 0xC) {
                        work->bodySphere.body.pos.vy = -ACTOR_01600_ATTACK_BODY_RADIUS;
                        work->bodySphere.body.radius = ACTOR_01600_ATTACK_BODY_RADIUS;
                        work->forwardSpeed           = 0x12C;
                    } else if ((s16)lungeFrame < 0x18) {
                        work->bodySphere.body.pos.vy = -ACTOR_01600_BODY_RADIUS;
                        work->bodySphere.body.radius = ACTOR_01600_BODY_RADIUS;
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
            work->animRate        = ANIMATION_RATE_ONE;
            work->animBlendFrames = 0;
            if (work->airborne != 0) {
                work->animRate = ACTOR_01600_FAST_ANIMATION_RATE;
                if (work->verticalSpeed < 0) {
                    work->verticalSpeed = (u16)work->verticalSpeed + 0xF;
                } else {
                    work->verticalSpeed = (u16)work->verticalSpeed + 0x14;
                }
                recoveryLungeHeight = (u16)work->jumpHeight + (u16)work->verticalSpeed;
                work->jumpHeight    = recoveryLungeHeight;
                if (recoveryLungeHeight >= 0) {
                    work->airborne      = 0;
                    work->verticalSpeed = 0;
                    work->jumpHeight    = 0;
                }
            }
            recoveryAnimation = work->animRequest;
            // Finish the entrance leap through landing and getting up.
            switch (recoveryAnimation) {
                case ACTOR_01600_ANIMATION_LUNGE:
                    takeoffFrame = work->animFrame;
                    if (takeoffFrame == 8) {
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
                                work->bodySphere.body.pos.vy = -ACTOR_01600_ATTACK_BODY_RADIUS;
                                work->bodySphere.body.radius = ACTOR_01600_ATTACK_BODY_RADIUS;
                                work->forwardSpeed           = 0x12C;
                            } else {
                                if (work->animFrame < 0x18) {
                                    work->bodySphere.body.pos.vy = -ACTOR_01600_BODY_RADIUS;
                                    work->bodySphere.body.radius = ACTOR_01600_BODY_RADIUS;
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
                        work->animRequest  = ACTOR_01600_ANIMATION_LUNGE_LAND;
                        work->animFrame    = 0;
                        work->longLeap     = 0;
                        work->entranceLeap = 0;
                        return;
                    }
                    break;
                case ACTOR_01600_ANIMATION_LUNGE_LAND:
                    work->forwardSpeed = 0x3C;
                    if (work->animFrame == 0xB) {
                        soundKey = (((u16)((Enemy*)actor->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | ACTOR_01600_SOUND_SCRIPT_5;
                        _actor01600PlaySoundAtRoot(soundKey, rootCoord);
                    }
                    if (work->animFrame >= 0xD) {
                        work->animRate     = ANIMATION_RATE_ONE;
                        work->forwardSpeed = 0;
                    }
                    if (work->animFrame >= 0x28) {
                        work->animRequest = ACTOR_01600_ANIMATION_RISE_FORWARD;
                        work->animFrame   = 0;
                    }
                    break;
                case ACTOR_01600_ANIMATION_RISE_FORWARD:
                    if (work->animFrame == 0x1F) {
                        soundKey = (((u16)((Enemy*)actor->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | ACTOR_01600_SOUND_SCRIPT_1;
                        _actor01600PlaySoundAtRoot(soundKey, rootCoord);
                        soundKey = (((u16)((Enemy*)actor->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | ACTOR_01600_SOUND_SCRIPT_2;
                        _actor01600PlaySoundAtRoot(soundKey, rootCoord);
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
            work->animRate        = ANIMATION_RATE_ONE;
            work->airborne        = 0;
            work->verticalSpeed   = 0;
            grabAnimationSelector = (u16)work->animRequest - ACTOR_01600_ANIMATION_SETTLE;
            work->jumpHeight      = 0;
            // Follow the held actor until release; keep the halfword selector wrap.
            switch (grabAnimationSelector) {
                case ACTOR_01600_ANIMATION_GRAB_APPROACH - ACTOR_01600_ANIMATION_SETTLE:
                    _actor01600StepTurn(actor);
                    _actor01600Sidestep(actor, 0xA);
                    if (work->animFrame >= 0xD) {
                        work->forwardSpeed = 0x5A;
                    }
                    if (work->animFrame >= 0x12) {
                        if (_actor01600TryAcquireGrab(actor) & 0xFF) {
                            grabTargetIndex = work->grabTargetIndex;
                            if (grabTargetIndex == PLAYER_ACTOR_TASK_COMPANION) {
                                if (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.companionHp > 0) {
                                    if (taskMessageDispatch(work->grabTarget, GAME_ACTOR_MESSAGE_APPLY_DAMAGE, damagePackEnemyAttackKey(enemy, 0), 0) == grabTargetIndex) {
                                        work->animRequest   = ACTOR_01600_ANIMATION_SETTLE;
                                        work->forwardSpeed  = 0;
                                        work->animFrame     = 0;
                                        work->grabEndHeight = (s32)rootCoord->coord.t[1];
                                        return;
                                    }
                                } else {
                                    work->animRequest   = ACTOR_01600_ANIMATION_SETTLE;
                                    work->forwardSpeed  = 0;
                                    work->animFrame     = 0;
                                    work->grabEndHeight = (s32)rootCoord->coord.t[1];
                                    return;
                                }
                            } else if (playerStatus->hp <= 0) {
                                work->animRequest   = ACTOR_01600_ANIMATION_SETTLE;
                                work->forwardSpeed  = 0;
                                work->animFrame     = 0;
                                work->grabEndHeight = (s32)rootCoord->coord.t[1];
                                return;
                            }
                            work->holdingTarget   = 1;
                            work->animBlendFrames = 0;
                            work->animRequest     = ACTOR_01600_ANIMATION_GRAB_HOLD;
                            soundKey              = (((u16)((Enemy*)actor->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | ACTOR_01600_SOUND_SCRIPT_D;
                            _actor01600PlaySoundAtRoot(soundKey, rootCoord);
                            if (work->grabTargetIndex != 0) {
                                Actor01600_D127D8.animationId = 1;
                                TASK_MESSAGE_DISPATCH_POINTER(work->grabTarget, ANIMATION_MESSAGE_INSTALL_AND_PLAY, &Actor01600_D127D8, 0);
                                soundKey = (((u16)((Enemy*)actor->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | ACTOR_01600_SOUND_COMPANION_GRAB;
                                _actor01600PlaySoundAtRoot(soundKey, rootCoord);
                            } else {
                                Actor01600_D127D8.animationId = 2;
                                TASK_MESSAGE_DISPATCH_POINTER(work->grabTarget, ANIMATION_MESSAGE_REPLACE_AND_PLAY, &Actor01600_D127D8, 0);
                                soundKey = (((u16)((Enemy*)actor->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | ACTOR_01600_SOUND_PLAYER_GRAB;
                                _actor01600PlaySoundAtRoot(soundKey, rootCoord);
                            }
                            work->grabBiteCount = 2;
                        } else {
                            work->animBlendFrames = 4;
                            work->shadowUnderBody = 0;
                            work->animRequest     = ACTOR_01600_ANIMATION_LUNGE_LAND;
                        }
                        work->forwardSpeed  = 0;
                        work->animFrame     = 0;
                        work->grabBiteTimer = 0U;
                        return;
                    }
                    break;
                case ACTOR_01600_ANIMATION_GRAB_HOLD - ACTOR_01600_ANIMATION_SETTLE:
                    heldRootCoord         = work->grabTarget->extra.tmd->coords;
                    heldGrabOffset        = &heldRootCoord[ACTOR_01600_GRAB_OFFSET_COORD];
                    rootCoord->coord.t[0] = heldRootCoord->coord.t[0] + heldGrabOffset->coord.t[0];
                    rootCoord->coord.t[2] = heldRootCoord->coord.t[2] + heldGrabOffset->coord.t[2];
                    biteTimer             = work->grabBiteTimer + 1;
                    work->grabBiteTimer   = biteTimer;
                    if ((s16)biteTimer == ACTOR_01600_GRAB_BITE_PERIOD) {
                        work->grabBiteTimer = 0U;
                        work->grabBiteCount = (u16)work->grabBiteCount + 1;
                        effectSpawnHit(damageGetPlayerAttackEffectId(ACTOR_01600_GRAB_EFFECT_ATTACK_KEY), effectCoord, &biteEffectOffset, &work->hitEffect);
                        if (work->grabTargetIndex == PLAYER_ACTOR_TASK_PLAYER) {
                            padScriptSpawnVariableMotorRamp(0xA, 0x80U, 0x80U);
                        }
                        soundKey = (((u16)((Enemy*)actor->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | ACTOR_01600_SOUND_SCRIPT_D;
                        _actor01600PlaySoundAtRoot(soundKey, rootCoord);
                        if (work->grabTargetIndex == PLAYER_ACTOR_TASK_PLAYER) {
                            taskMessageDispatch(work->grabTarget, GAME_ACTOR_MESSAGE_APPLY_DAMAGE, damagePackEnemyAttackKey(enemy, 0), 0);
                        }
                        if (playerStatus->hp <= 0) {
                            if (work->grabTargetIndex == PLAYER_ACTOR_TASK_PLAYER) {
                                work->animRequest             = ACTOR_01600_ANIMATION_SETTLE;
                                work->forwardSpeed            = 0;
                                work->animFrame               = 0;
                                playerReleaseHeight           = rootCoord->coord.t[1];
                                work->holdingTarget           = 0;
                                work->grabEndHeight           = playerReleaseHeight;
                                Actor01600_D127D8.animationId = 0;
                                Actor01600_D127D8.blend       = ANIMATION_BLEND_RESET;
                                Actor01600_D127D8.blendFrames = 0;
                                taskMessageDispatch(work->grabTarget, GAME_ACTOR_MESSAGE_END_SCRIPTED, 0, 0);
                                return;
                            }
                        }
                    }
                    if (work->grabTargetIndex == PLAYER_ACTOR_TASK_PLAYER) {
                        if (work->animFrame >= 0x31) {
                            if (work->grabBiteCount >= ACTOR_01600_GRAB_RELEASE_BITE_COUNT) {
                                Actor01600_D127D8.animationId = 3;
                                Actor01600_D127D8.blend       = ANIMATION_BLEND_INTERPOLATE;
                                Actor01600_D127D8.blendFrames = 1;
                                TASK_MESSAGE_DISPATCH_POINTER(work->grabTarget, ANIMATION_MESSAGE_REPLACE_AND_PLAY, &Actor01600_D127D8, 0);
                                work->animPlaying = 0;
                                work->animFrame   = 0;
                                work->animRequest = ACTOR_01600_ANIMATION_GRAB_RELEASE;
                            }
                            if (work->animFrame >= 0x31) {
                                work->animFrame = 0;
                                return;
                            }
                        }
                    } else {
                        if (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.companionHp <= 0) {
                            work->animRequest             = ACTOR_01600_ANIMATION_SETTLE;
                            work->forwardSpeed            = 0;
                            work->animFrame               = 0;
                            companionReleaseHeight        = rootCoord->coord.t[1];
                            work->holdingTarget           = 0;
                            work->grabEndHeight           = companionReleaseHeight;
                            Actor01600_D127D8.animationId = 0;
                            Actor01600_D127D8.blend       = ANIMATION_BLEND_RESET;
                            Actor01600_D127D8.blendFrames = 0;
                            taskMessageDispatch(work->grabTarget, GAME_ACTOR_MESSAGE_END_SCRIPTED, 0, 0);
                            return;
                        }
                        if (work->animFrame >= 0x31) {
                            work->animFrame   = 0;
                            work->animRequest = ACTOR_01600_ANIMATION_GRAB_RELEASE;
                            return;
                        }
                    }
                    break;
                case ACTOR_01600_ANIMATION_GRAB_RELEASE - ACTOR_01600_ANIMATION_SETTLE:
                    if (work->animFrame >= 0x10) {
                        work->forwardSpeed = 0;
                        releaseFrame       = (u16)work->animFrame;
                        if ((u32)(releaseFrame - 0x10) < 2U) {
                            work->forwardSpeed = -0x12C;
                        } else if ((s16)releaseFrame < 0x1D) {
                            work->forwardSpeed = -0x64;
                        } else if ((s16)releaseFrame < 0x24) {
                            work->forwardSpeed = -0x28;
                        }
                    } else {
                        releaseRootCoord      = work->grabTarget->extra.tmd->coords;
                        releaseGrabOffset     = &releaseRootCoord[ACTOR_01600_GRAB_OFFSET_COORD];
                        rootCoord->coord.t[0] = releaseRootCoord->coord.t[0] + releaseGrabOffset->coord.t[0];
                        rootCoord->coord.t[2] = releaseRootCoord->coord.t[2] + releaseGrabOffset->coord.t[2];
                    }
                    if (work->animFrame >= 0x2D) {
                        work->holdingTarget           = 0;
                        Actor01600_D127D8.animationId = 0;
                        Actor01600_D127D8.blend       = ANIMATION_BLEND_RESET;
                        Actor01600_D127D8.blendFrames = 0;
                        taskMessageDispatch(work->grabTarget, GAME_ACTOR_MESSAGE_END_SCRIPTED, 0, 0);
                    }
                    if (work->animFrame >= 0x38) {
                        work->animRequest  = ACTOR_01600_ANIMATION_SETTLE;
                        work->forwardSpeed = 0;
                        Actor01600_D12870  = 0;
                    }
                    if (work->animFrame == 0x13) {
                        soundKey = (((u16)((Enemy*)actor->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | ACTOR_01600_SOUND_SCRIPT_B;
                        _actor01600PlaySoundAtRoot(soundKey, rootCoord);
                        if (work->grabTargetIndex == PLAYER_ACTOR_TASK_PLAYER) {
                            padScriptSpawnVariableMotorRamp(0xA, 0xD0U, 0xD0U);
                        }
                    }
                    if (work->animFrame == 0x1F) {
                        soundKey = (((u16)((Enemy*)actor->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | ACTOR_01600_SOUND_SCRIPT_C;
                        _actor01600PlaySoundAtRoot(soundKey, rootCoord);
                        soundKey = (((u16)((Enemy*)actor->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | ACTOR_01600_SOUND_SCRIPT_1;
                        _actor01600PlaySoundAtRoot(soundKey, rootCoord);
                    }
                    if (work->animFrame == 0x21) {
                        soundKey = (((u16)((Enemy*)actor->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | ACTOR_01600_SOUND_SCRIPT_2;
                        _actor01600PlaySoundAtRoot(soundKey, rootCoord);
                        return;
                    }
                    break;
                case ACTOR_01600_ANIMATION_SETTLE - ACTOR_01600_ANIMATION_SETTLE:
                    if (work->animFrame == 1) {
                        soundKey = (((u16)((Enemy*)actor->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | ACTOR_01600_SOUND_SCRIPT_E;
                        _actor01600PlaySoundAtRoot(soundKey, rootCoord);
                    }
                    if (work->animFrame >= 0x25) {
                        work->attackAction    = ACTOR_01600_ACTION_ADVANCE;
                        work->animFrame       = 0;
                        work->shadowUnderBody = 0;
                        return;
                    }
                    break;
                case ACTOR_01600_ANIMATION_LUNGE_LAND - ACTOR_01600_ANIMATION_SETTLE:
                    work->forwardSpeed    = 0x28;
                    work->shadowUnderBody = 0;
                    work->animBlendFrames = 4;
                    if (work->animFrame >= 0xD) {
                        work->forwardSpeed = 0;
                    }
                    if (work->animFrame == 0xD) {
                        soundKey = (((u16)((Enemy*)actor->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | ACTOR_01600_SOUND_SCRIPT_5;
                        _actor01600PlaySoundAtRoot(soundKey, rootCoord);
                    }
                    if (work->animFrame >= 0x30) {
                        work->animRequest  = ACTOR_01600_ANIMATION_RISE_FORWARD;
                        work->forwardSpeed = 0;
                        work->animFrame    = 0;
                    }
                    break;
                case ACTOR_01600_ANIMATION_RISE_FORWARD - ACTOR_01600_ANIMATION_SETTLE:
                    work->shadowUnderBody = 0;
                    if (work->animFrame == 0x1F) {
                        soundKey = (((u16)((Enemy*)actor->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | ACTOR_01600_SOUND_SCRIPT_1;
                        _actor01600PlaySoundAtRoot(soundKey, rootCoord);
                        soundKey = (((u16)((Enemy*)actor->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | ACTOR_01600_SOUND_SCRIPT_2;
                        _actor01600PlaySoundAtRoot(soundKey, rootCoord);
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
            work->animRate        = ACTOR_01600_FAST_ANIMATION_RATE;
            work->animBlendFrames = 0;
            switch (work->animRequest) {
                case ACTOR_01600_ANIMATION_THROWN_FORWARD:
                case ACTOR_01600_ANIMATION_STAGGER:
                    knockbackFrame = work->animFrame;
                    if (knockbackFrame <= 0) {
                        work->bodySphere.body.pos.vy = -ACTOR_01600_ATTACK_BODY_RADIUS;
                        work->bodySphere.body.radius = ACTOR_01600_ATTACK_BODY_RADIUS;
                        work->forwardSpeed           = 0x1F4;
                    } else if (knockbackFrame < 0x13) {
                        work->bodySphere.body.pos.vy = -ACTOR_01600_BODY_RADIUS;
                        work->bodySphere.body.radius = ACTOR_01600_BODY_RADIUS;
                        work->forwardSpeed           = 0x4B;
                    } else {
                        work->forwardSpeed = 0;
                    }
                    if (work->animRequest == ACTOR_01600_ANIMATION_STAGGER) {
                        backwardSpeed      = -work->forwardSpeed;
                        work->forwardSpeed = backwardSpeed;
                    }
                    if (work->airborne != 0) {
                        work->verticalSpeed += 0xF;
                        work->jumpHeight    += work->verticalSpeed;
                        if (work->jumpHeight >= 0) {
                            soundKey = (((u16)((Enemy*)actor->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | ACTOR_01600_SOUND_SCRIPT_5;
                            _actor01600PlaySoundAtRoot(soundKey, rootCoord);
                            work->airborne   = 0;
                            work->jumpHeight = 0;
                        }
                    }
                    if (work->animFrame >= 0x28) {
                        if (work->animRequest == ACTOR_01600_ANIMATION_STAGGER) {
                            work->animRequest = ACTOR_01600_ANIMATION_RISE_BACK;
                        } else {
                            work->animRequest = ACTOR_01600_ANIMATION_RISE_FORWARD;
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
                case ACTOR_01600_ANIMATION_RISE_FORWARD:
                case ACTOR_01600_ANIMATION_RISE_BACK:
                    work->animRate     = ANIMATION_RATE_ONE;
                    work->forwardSpeed = 0;
                    if (work->animFrame == 0x19) {
                        soundKey = (((u16)((Enemy*)actor->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | ACTOR_01600_SOUND_SCRIPT_2;
                        _actor01600PlaySoundAtRoot(soundKey, rootCoord);
                    }
                    if (work->animFrame == 0x1B) {
                        soundKey = (((u16)((Enemy*)actor->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | ACTOR_01600_SOUND_SCRIPT_1;
                        _actor01600PlaySoundAtRoot(soundKey, rootCoord);
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

/// Draws the scavenger's ground shadow under its root or its grabbing body.
///
/// Requires composed view-space caches. The airborne root shadow is offset down
/// along its local Y axis by 128 minus the signed jump displacement. A grabbing
/// body uses a floor projection and the room shade. Radius is 448 world units.
static void _actor01600DrawGroundShadow(Task* actor)
{
    enum { ACTOR_01600_SHADOW_RADIUS    = 448,
           ACTOR_01600_SHADOW_DROP_BIAS = 128 };

    VECTOR3                         groundPoint;
    _Actor01600GroundShadowScratch* scratch;
    _Actor01600Work*                work;
    GfxCoord*                       rootCoord;
    s32                             rootOffsetY;

    work      = actor->work;
    rootCoord = actor->extra.tmd->coords;
    if (work->shadowUnderBody == 0) {
        scratch = SCRATCH_STACK_RESERVE_BLOCK(_Actor01600GroundShadowScratch);
        if (work->airborne != 0) {
            gte_SetRotMatrix(&rootCoord->workm);
            scratch->drop.vx = 0;
            rootOffsetY      = work->jumpHeight - ACTOR_01600_SHADOW_DROP_BIAS;
            scratch->drop.vy = -rootOffsetY;
            scratch->drop.vz = 0;
            gte_ldv0(&scratch->drop);
            gte_rtv0();
            gte_stlvnl(&scratch->centre);
            scratch->centre.vx += rootCoord->workm.t[0];
            scratch->centre.vy += rootCoord->workm.t[1];
            scratch->centre.vz += rootCoord->workm.t[2];
        } else {
            scratch->centre.vx = rootCoord->workm.t[0];
            scratch->centre.vy = rootCoord->workm.t[1];
            scratch->centre.vz = rootCoord->workm.t[2];
        }
        effectDrawGroundShadow(&scratch->centre, ACTOR_01600_SHADOW_RADIUS, 0);
        SCRATCH_STACK_RELEASE_BLOCK(_Actor01600GroundShadowScratch);
        return;
    }
    if (worldCollisionProjectGroundPoint(MATRIX_TRANS(&rootCoord[1].workm), &groundPoint) != 0) {
        effectDrawGroundShadow(&groundPoint, ACTOR_01600_SHADOW_RADIUS, gRoomEffectState->groundShadowShade);
    }
}

/// Saves the corpse root's position and applies forward and vertical collapse motion.
///
/// Distances use the root's parent frame; the facing basis is Q12. Suspension suppresses
/// vertical motion, and a grounded corpse steps down by 128 units for floor correction.
static __inline__ void _actor01600MoveCollapsingBody(Task* actor)
{
    enum { ACTOR_01600_COLLAPSE_GROUND_STEP = 128,
           ACTOR_01600_COLLAPSE_BASIS_SHIFT = 12 };
    GfxCoord*        motionCoord;
    _Actor01600Work* motionWork;
    motionCoord = actor->extra.tmd->coords;
    motionWork  = actor->work;

    motionWork->previousPosition.vx = motionCoord->coord.t[0];
    motionWork->previousPosition.vy = motionCoord->coord.t[1];
    motionWork->previousPosition.vz = motionCoord->coord.t[2];

    motionCoord->coord.t[0] += (motionCoord->coord.m[0][2] * motionWork->forwardSpeed) >> ACTOR_01600_COLLAPSE_BASIS_SHIFT;
    motionCoord->coord.t[2] += (motionCoord->coord.m[2][2] * motionWork->forwardSpeed) >> ACTOR_01600_COLLAPSE_BASIS_SHIFT;
    if (motionWork->suspended == 0) {
        if (motionWork->airborne != 0) {
            motionCoord->coord.t[1] += motionWork->verticalSpeed;
        } else {
            motionCoord->coord.t[1] += ACTOR_01600_COLLAPSE_GROUND_STEP;
        }
    }
}

/// Unlinks a dying scavenger, collapses its corpse and finishes its wave lifetime.
///
/// Task state 2 owns this handler. Exploded bodies skip collapse effects; other
/// bodies flatten for 60 ticks, then hide for 60 ticks before destruction.
/// The last scavenger of the final wave waits for UI gates and reports itself
/// to the room, which keeps the task alive. Animation and lighting continue
/// during collapse; paused or hidden combat control postpones death progress.
static void _actor01600StepDeath(Enemy* enemy, Task* actor)
{
    _Actor01600Work*  work;
    TmdObject*        model;
    GfxCoord*         rootCoord;
    GfxCoord*         bodyCoord;
    SceneCombatState* combatState;
    AttachmentState*  attachment;
    s32               unusedDistance;
    s32               yawMagnitude;
    s32               behavior;
    s16               collapseTimer;
    s16               exitCountdown;
    s16               colorTimer;
    s32               actorControl;

    model        = actor->extra.tmd;
    work         = actor->work;
    rootCoord    = model->coords;
    actorControl = gSceneCombatState.actorControl;
    if (actorControl == SCENE_COMBAT_ACTORS_PAUSED) {
        return;
    }
    if (actorControl > SCENE_COMBAT_ACTORS_PAUSED) {
        if (actorControl == SCENE_COMBAT_ACTORS_HIDDEN) {
            model->flags                 |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
            enemy->node.state.parts.flags = WORLD_TARGET_NOT_LOCKABLE;
            return;
        }
    }
    switch (work->deathPhase) {
        case ACTOR_01600_DEATH_BEGIN:
            // Release the target and detach collision/lock-on before the corpse falls.
            _actor01600ReleaseGrab(actor);
            if (work->burstState != ACTOR_01600_BURST_COMPLETE) {
                behavior              = work->behavior;
                work->animBlendFrames = 0;
                work->animRate        = ACTOR_01600_FAST_ANIMATION_RATE;
                if (behavior != ACTOR_01600_BEHAVIOR_STAGGER && behavior != ACTOR_01600_BEHAVIOR_BUILDUP_DOWN) {
                    yawMagnitude = _actor01600MeasureTarget(actor, &unusedDistance);
                    if (yawMagnitude < 0) {
                        yawMagnitude = -yawMagnitude;
                    }
                    work->animRequest = (yawMagnitude < ACTOR_TRANSFORM_ANGLE_HALF_TURN / 2) ? ACTOR_01600_ANIMATION_STAGGER : ACTOR_01600_ANIMATION_THROWN_FORWARD;
                }
                work->stateTimer  = 0;
                work->deathScaleY = ONE;
                work->deathMatrix = rootCoord[0].coord;
                worldCoordSetActorColorMode(enemy, ENEMY_COLOR_WEIGHTED);
            }
            enemy->node.state.parts.flags = WORLD_TARGET_NOT_LOCKABLE;
            enemy->recs                   = 0;
            worldTargetUnlinkNode(&enemy->node);
            worldCollisionUnlinkBody(&work->pathProbe.body);
            worldCollisionUnlinkBody(&work->sight.body);
            worldCollisionUnlinkBody(&work->bodySphere.body);
            worldCollisionUnlinkBody(&work->bite.body);
            combatState = &gSceneCombatState;
            if (combatState->actor01600Wave >= ACTOR_01600_WAVE_FINAL) {
                if (_actor01600CheckSceneChildrenHidden() == 1) {
                    combatState->actor01600Wave = combatState->actor01600Wave + 1;
                }
            } else {
                sceneReleaseBattleRefWithRewards(actor, 0x10);
            }
            work->deathPhase = ACTOR_01600_DEATH_COLLAPSE;
            break;
        case ACTOR_01600_DEATH_COLLAPSE:
            _actor01600SquashCorpse(actor);
            collapseTimer    = work->stateTimer + 1;
            work->stateTimer = collapseTimer;
            if (work->burstState != ACTOR_01600_BURST_COMPLETE) {
                if (collapseTimer == 0xA) {
                    model->flags = TMD_OBJECT_SEMI_TRANS;
                }
                if (work->stateTimer == 0xF) {
                    effectSpawn(EFFECT_CORPSE_BURN, rootCoord, 1, NULL);
                }
                if (work->stateTimer < 0x10) {
                    _actor01600MoveCollapsingBody(actor);
                }
            }
            if (work->stateTimer >= 0x3C) {
                work->deathPhase = ACTOR_01600_DEATH_REMOVE;
            }
            break;
        case ACTOR_01600_DEATH_REMOVE:
            // The final survivor remains alive while the room takes over its event.
            if (gSceneCombatState.actor01600Wave >= ACTOR_01600_WAVE_FINAL) {
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
                    TASK_MESSAGE_DISPATCH_POINTER(gameGetTaskSlot(GAME_TASK_SLOT_ROOM), ROOM_MESSAGE_ACTOR_EVENT, actor, 0);
                    work->deathPhase         = ACTOR_01600_DEATH_REPORTED;
                    actor->extra.tmd->flags |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
                    actor->extra.tmd->flags |= TMD_OBJECT_SKIP_AUTO_BUFFER;
                    break;
                }
                sceneReleaseBattleRefWithRewards(actor, 0x10);
            }
            work->exitTimer          = 0x3C;
            actor->extra.tmd->flags |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
            Actor01600_D12874--;
            actor->extra.tmd->flags |= TMD_OBJECT_SKIP_AUTO_BUFFER;
            work->deathPhase         = ACTOR_01600_DEATH_EXIT;
            break;
        case ACTOR_01600_DEATH_EXIT:
            exitCountdown   = work->exitTimer - 1;
            work->exitTimer = exitCountdown;
            if (exitCountdown == 0) {
                _actor01600Exit(actor);
            }
            return;
        default:
            return;
    }

    _actor01600StepAnimation(actor);
    rootCoord->composeStamp = GRAPHICS_COORD_DIRTY;
    actorRenderComposeCoord(rootCoord);
    colorTimer       = work->colorTimer + 1;
    work->colorTimer = colorTimer;
    if (colorTimer < ACTOR_01600_COLOR_INTERVAL && gGameSession->viewDirty != 1) {
        return;
    }
    work->colorTimer = 0;

    bodyCoord = &actor->extra.tmd->coords[1];
    _actor01600SampleColorAtCoord(enemy, bodyCoord);
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

/// Tries to take scripted control of the selected grab target and face it toward the scavenger.
///
/// Returns 1 after the button-press hold accepts and the shared grab latch is
/// set, otherwise 0. Requires a live `grabTarget` and player slot 0 or 1.
/// Reach is strictly below 1000 world units and yaw within a quarter turn;
/// those measurements reselect the nearer player, while the hold and placement
/// address the saved grab target. The target escapes after five button presses.
/// Placement retains its position and narrows the offset used for yaw to s16.
static s32 _actor01600TryAcquireGrab(Task* actor)
{
    SVECTOR3         targetToRoot;
    s32              horizontalDistance;
    _Actor01600Work* work;
    GfxCoord*        rootCoord;
    Task*            target;
    s16              targetYaw;
    s16              wrappedYaw;
    s32              yawMagnitude;
    GfxCoord*        targetCoord;

    enum { ACTOR_01600_GRAB_ESCAPE_PRESSES = 5 };

    work        = actor->work;
    target      = work->grabTarget;
    targetCoord = target->extra.tmd->coords;
    rootCoord   = actor->extra.tmd->coords;
    if (((GameActor*)gPlayerActorTasks[work->grabTargetIndex]->work)->mode != GAME_ACTOR_MODE_SCRIPTED) {
        if (Actor01600_D12870 != 1) {
            yawMagnitude = _actor01600MeasureTarget(actor, &horizontalDistance);
            if (yawMagnitude < 0) {
                yawMagnitude = -yawMagnitude;
            }
            if (yawMagnitude < ACTOR_01600_GRAB_TAKE_YAW + 1) {
                if (horizontalDistance < ACTOR_01600_GRAB_REACH) {
                    Actor01600_D12878.pressCount = ACTOR_01600_GRAB_ESCAPE_PRESSES;
                    if (work->grabTargetIndex != PLAYER_ACTOR_TASK_PLAYER) {
                        Actor01600_D12878.animation.animationId = 1;
                    } else {
                        Actor01600_D12878.animation.animationId = 2;
                    }
                    if (TASK_MESSAGE_DISPATCH_POINTER(target, GAME_ACTOR_MESSAGE_AWAIT_BUTTON_PRESSES, &Actor01600_D12878, 0) == 0) {
                        targetCoord->composeStamp = GRAPHICS_COORD_DIRTY;
                        targetToRoot.vx           = rootCoord->coord.t[0] - targetCoord->coord.t[0];
                        targetToRoot.vy           = 0;
                        targetToRoot.vz           = rootCoord->coord.t[2] - targetCoord->coord.t[2];
                        targetYaw                 = ratan2(targetToRoot.vx, targetToRoot.vz);
                        wrappedYaw                = targetYaw;
                        if (targetYaw >= ACTOR_TRANSFORM_ANGLE_HALF_TURN + 1) {
                            wrappedYaw = targetYaw - ACTOR_TRANSFORM_ANGLE_TURN;
                        } else if (targetYaw < -ACTOR_TRANSFORM_ANGLE_HALF_TURN) {
                            wrappedYaw = targetYaw + ACTOR_TRANSFORM_ANGLE_TURN;
                        }
                        Actor01600_D12890.rot.vx = 0;
                        Actor01600_D12890.rot.vy = wrappedYaw;
                        Actor01600_D12890.rot.vz = 0;
                        Actor01600_D12890.pos.vx = targetCoord->coord.t[0];
                        Actor01600_D12890.pos.vy = targetCoord->coord.t[1];
                        Actor01600_D12890.pos.vz = targetCoord->coord.t[2];
                        TASK_MESSAGE_DISPATCH_POINTER(target, GAME_ACTOR_MESSAGE_PLACE, &Actor01600_D12890, 0);
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

/// Returns a blocked attacker to heading-search roaming and disables sight.
///
/// Requires the actor's live work. Clears its alert, requests look-around and
/// resets the heading probe before another roaming step; no target is released.
static __inline__ void _actor01600ReturnToRoam(_Actor01600Work* work)
{
    work->animRequest       = ACTOR_01600_ANIMATION_LOOK_AROUND;
    work->alerted           = 0;
    work->behavior          = ACTOR_01600_BEHAVIOR_ROAM;
    work->pathSearchPhase   = ACTOR_01600_PATH_SEARCH_PROBE_TARGET;
    work->field_514         = 1;
    work->sight.body.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ (WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED));
}

/// Begins a grab approach when the selected player is close and nearly ahead.
///
/// `playerIndex` is a live player-task slot (0 player, 1 companion); yaw is
/// the signed relative bearing in 4096-unit turns and distance is horizontal
/// world units. Returns 1 for a grab approach or a blocked-path return to roam,
/// and 0 when ineligible. Admission compares height with player slot 0 even
/// when the companion is selected. The saved target is borrowed through the grab.
static s32 _actor01600TryBeginGrab(Task* actor, s32 relativeYaw, s32 horizontalDistance, s32 playerIndex)
{
    _Actor01600Work* work;
    GfxCoord*        rootCoord;
    GfxCoord*        playerCoord;
    s32              heightGap;
    s32              yawMagnitude;
    s32              resumedRoam;
    s32              playerHeight;
    s32              heightOrYawSign;

    work        = actor->work;
    rootCoord   = actor->extra.tmd->coords;
    playerCoord = (*gPlayerActorTasks)->extra.tmd->coords;
    if (((GameActor*)gPlayerActorTasks[playerIndex]->work)->mode != GAME_ACTOR_MODE_SCRIPTED) {
        if (Actor01600_D12870 == 0) {
            playerHeight    = playerCoord->coord.t[1];
            heightOrYawSign = rootCoord->coord.t[1];
            heightGap       = playerHeight - heightOrYawSign;
            if (heightGap < 0) {
                heightGap = -heightGap;
            }
            if (heightGap < ACTOR_01600_GRAB_HEIGHT_GAP + 1) {
                if (horizontalDistance < ACTOR_01600_GRAB_REACH + 1) {
                    heightOrYawSign = relativeYaw >= 0;
                    yawMagnitude    = heightOrYawSign ? relativeYaw : -relativeYaw;
                    if (yawMagnitude < ACTOR_01600_GRAB_BEGIN_YAW + 1) {
                        work->pathProbe.body.flags |= (WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED);
                        _actor01600SearchClearHeading(actor);
                        if (work->pathSearchPhase >= ACTOR_01600_PATH_SEARCH_BEGIN_SWEEP) {
                            _actor01600ReturnToRoam(work);
                            resumedRoam = 1;
                        } else {
                            resumedRoam = 0;
                        }
                        if ((u8)resumedRoam) {
                            return 1;
                        }
                        work->attackAction    = ACTOR_01600_ACTION_GRAB;
                        work->animRequest     = ACTOR_01600_ANIMATION_GRAB_APPROACH;
                        work->field_52C       = 0;
                        work->grabTargetIndex = playerIndex;
                        work->grabTarget      = gPlayerActorTasks[playerIndex];
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

/// Steps the lock-on anchor through follow, detach, hold and catch-up during a hop.
///
/// Normally publishes the model root to the enemy record. Once detached, the
/// separate anchor holds XZ for eight ticks and catches up in five truncated
/// steps; Y follows the root throughout. Both coordinates and the enemy
/// record must remain live. Rebuilds the anchor's composed matrix each call.
static void _actor01600StepTargetAnchor(Task* actor)
{
    Enemy*           enemy;
    _Actor01600Work* work;
    GfxCoord*        rootCoord;
    s16              anchorPhase;
    u16              holdFrames;
    u16              catchUpFrames;

    enum { ACTOR_01600_ANCHOR_HOLD_FRAMES     = 8,
           ACTOR_01600_ANCHOR_CATCH_UP_FRAMES = 5 };

    work        = actor->work;
    enemy       = actor->spawnArg2.pointer;
    anchorPhase = work->targetAnchorPhase;
    rootCoord   = actor->extra.tmd->coords;
    switch (anchorPhase) {
        case ACTOR_01600_TARGET_ANCHOR_FOLLOW:
            work->targetAnchor.coord.t[0] = rootCoord->coord.t[0];
            work->targetAnchor.coord.t[2] = rootCoord->coord.t[2];
            enemy->coord                  = rootCoord;
            break;
        case ACTOR_01600_TARGET_ANCHOR_DETACH:
            work->targetAnchor.coord.t[0] = rootCoord->coord.t[0];
            work->targetAnchor.coord.t[2] = rootCoord->coord.t[2];
            enemy->coord                  = &work->targetAnchor;
            work->targetAnchorTimer       = 0U;
            work->targetAnchorPhase       = (u16)work->targetAnchorPhase + 1;
            break;
        case ACTOR_01600_TARGET_ANCHOR_HOLD:
            holdFrames              = work->targetAnchorTimer + 1;
            work->targetAnchorTimer = holdFrames;
            if ((s16)holdFrames < ACTOR_01600_ANCHOR_HOLD_FRAMES) {
                break;
            }
            work->targetAnchorTimer = 0U;
            work->targetAnchorPhase = (u16)work->targetAnchorPhase + 1;
            break;
        case ACTOR_01600_TARGET_ANCHOR_AIM:
            work->targetAnchorStepX = (s16)((rootCoord->coord.t[0] - work->targetAnchor.coord.t[0]) / ACTOR_01600_ANCHOR_CATCH_UP_FRAMES);
            work->targetAnchorStepZ = (s16)((rootCoord->coord.t[2] - work->targetAnchor.coord.t[2]) / ACTOR_01600_ANCHOR_CATCH_UP_FRAMES);
            work->targetAnchorPhase = (u16)work->targetAnchorPhase + 1;
            break;
        case ACTOR_01600_TARGET_ANCHOR_CATCH_UP:
            work->targetAnchor.coord.t[0] += work->targetAnchorStepX;
            work->targetAnchor.coord.t[2] += work->targetAnchorStepZ;
            catchUpFrames                  = work->targetAnchorTimer + 1;
            work->targetAnchorTimer        = catchUpFrames;
            if ((s16)catchUpFrames >= ACTOR_01600_ANCHOR_CATCH_UP_FRAMES) {
                work->targetAnchorTimer = 0U;
                work->targetAnchorPhase = ACTOR_01600_TARGET_ANCHOR_FOLLOW;
            }
            break;
    }
    work->targetAnchor.coord.t[1]   = rootCoord->coord.t[1];
    work->targetAnchor.composeStamp = GRAPHICS_COORD_DIRTY;
    actorRenderComposeCoord(&work->targetAnchor);
}

/// Rotates the path probe's root-local reach to its stored yaw.
///
/// Requires initialized signed-halfword reach components in world units and
/// a readable external yaw halfword, in 4096-unit turns. Writes the rotation
/// basis and far-end XYZ; vector pads and matrix translation remain untouched.
/// Borrows the caller's scratch block and overwrites GTE rotation state.
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

/// Holds a placed scavenger until its wave cue and prepares its active entrance.
///
/// Returns 1 while ordinary live processing must be skipped, including removal;
/// returns 0 once the collision bodies and active behavior are ready. An active
/// actor can still be forced into posed behavior by its room or placement.
/// Waiting scripted actors advance their animation and staged motion here.
/// Second-wave placement indices must fit the room's entry table: 0..3 in
/// stage 3/area 15, or 0..4 in stage 4/area 4. Placement data owns that bound.
static s32 _actor01600StepActivation(Task* actor)
{
    SVECTOR          entryRotation;
    Enemy*           enemy;
    AreaPlacement*   placement;
    _Actor01600Work* work;
    GfxCoord*        entryRootCoord;
    GfxCoord*        rootCoord;
    TmdObject*       bufferModel;
    TmdObject*       visibleModel;
    TmdObject*       lateBufferModel;
    TmdObject*       lateVisibleModel;
    SVECTOR*         firstEntryPosition;
    SVECTOR*         firstEntryPositions;
    SVECTOR*         secondEntryPositions;
    SVECTOR*         secondEntryPosition;
    s16              firstWaveDelay;
    s16              finalWaveDelay;
    s16              lateWaveDelay;
    s32              soundKey;
    s32              wave;
    u16              placementMode;
    u8               room;

    rootCoord = actor->extra.tmd->coords;
    enemy     = actor->spawnArg2.pointer;
    work      = actor->work;
    if ((u32)(gGameSession->location.loc.stage - 2) < 2U) {
        if ((GAME_LOCATION_WORD(gGameSession->location.loc) & GAME_LOCATION_KEY(0, 255, 255, 0)) == GAME_LOCATION_KEY(0, 34, 1, 0)) {
            if ((u16)work->behavior < (u32)ACTOR_01600_BEHAVIOR_STAGGER) {
                work->behavior = ACTOR_01600_BEHAVIOR_POSED;
            }
        }
        if (((u32)(gGameSession->location.loc.stage - 2) < 2U) && (gGameSession->location.loc.area == 0x26) &&
            ((room = gGameSession->location.loc.room, (room == 1)) || (room == 3)) && ((u16)work->behavior < (u32)ACTOR_01600_BEHAVIOR_STAGGER)) {
            work->behavior = ACTOR_01600_BEHAVIOR_POSED;
        }
    }
    if (((u8)enemy->place->variant & 0x80) && ((u16)work->behavior < (u32)ACTOR_01600_BEHAVIOR_STAGGER)) {
        work->behavior = ACTOR_01600_BEHAVIOR_POSED;
    }
    if (work->active != 0) {
        return 0;
    }

    placement     = enemy->place;
    placementMode = placement->mode;
    switch (placementMode) {
        case ACTOR_01600_PLACEMENT_FIRST_WAVE:
        case ACTOR_01600_PLACEMENT_SECOND_WAVE:
            _actor01600StepTurn(actor);
            if ((u32)((u8)gSceneCombatState.actor01600Wave - 1) >= 2U) {
                goto skipLiveUpdate;
            }
            wave = (s8)(u8)gSceneCombatState.actor01600Wave;
            if (wave == ACTOR_01600_WAVE_FIRST) {
                soundKey = (((u16)((Enemy*)actor->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | ACTOR_01600_SOUND_SCRIPT_E;
                _actor01600PlaySoundAtRoot(soundKey, rootCoord);
                firstWaveDelay      = (u16)work->reactionDelay - 1;
                work->reactionDelay = firstWaveDelay;
                if ((firstWaveDelay << 0x10) != 0) {
                    goto skipLiveUpdate;
                }
                if ((u8)enemy->place->variant == 0) {
                    work->attackAction = ACTOR_01600_ACTION_LUNGE_RECOVER;
                    work->animRequest  = ACTOR_01600_ANIMATION_LUNGE;
                } else {
                    work->entranceLeap = (s16)wave;
                    work->attackAction = ACTOR_01600_ACTION_LUNGE;
                }
                work->shadowHidden = 1;
            } else if (wave == ACTOR_01600_WAVE_SECOND) {
                if ((GAME_LOCATION_WORD(gGameSession->location.loc) & GAME_LOCATION_STAGE_AREA_MASK) == GAME_LOCATION_KEY(3, 15, 0, 0)) {

                    firstEntryPositions   = Actor01600_D09F1C;
                    firstEntryPosition    = &firstEntryPositions[(u16)enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT];
                    rootCoord->coord.t[0] = firstEntryPosition->vx;
                    rootCoord->coord.t[1] = firstEntryPosition->vy;
                    rootCoord->coord.t[2] = firstEntryPosition->vz;
                }
                if ((GAME_LOCATION_WORD(gGameSession->location.loc) & GAME_LOCATION_STAGE_AREA_MASK) == GAME_LOCATION_KEY(4, 4, 0, 0)) {

                    secondEntryPositions  = Actor01600_D09F3C;
                    secondEntryPosition   = &secondEntryPositions[(u16)enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT];
                    rootCoord->coord.t[0] = secondEntryPosition->vx;
                    rootCoord->coord.t[1] = secondEntryPosition->vy;
                    rootCoord->coord.t[2] = secondEntryPosition->vz;
                }
                sceneEngageBattle(1);
            }
            enemy->node.state.parts.flags = 0;
            work->active                  = 1;
            _actor01600LinkCollisionBodies(actor);
            if (gSceneCombatState.actor01600Wave == ACTOR_01600_WAVE_FIRST) {
                work->bodySphere.body.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_GRID_ENABLED);
            }
            tmdAllocPrimitiveBuffer(actor->extra.tmd);
            bufferModel           = actor->extra.tmd;
            bufferModel->flags   &= (u16)~TMD_OBJECT_SKIP_AUTO_BUFFER;
            visibleModel          = actor->extra.tmd;
            visibleModel->flags  &= (u16)~TMD_OBJECT_SKIP_ACTIVE_DRAW;
            work->behavior        = ACTOR_01600_BEHAVIOR_ATTACK;
            work->animBlendFrames = 0;
            work->animPlaying     = 0;
            work->animFrame       = 0;
            work->biteLanded      = 0;
            return 0;
        case ACTOR_01600_PLACEMENT_SCRIPTED:
            if (gSceneCombatState.actor01600Wave == placementMode) {
                if ((u8)placement->variant == 1) {
                    _actor01600Remove(actor, 1);
                skipLiveUpdate:
                    return 1;
                }
                finalWaveDelay      = (u16)work->reactionDelay - 1;
                work->reactionDelay = finalWaveDelay;
                if ((finalWaveDelay << 0x10) == 0) {
                    enemy->node.state.parts.flags = 0;
                    _actor01600LinkCollisionBodies(actor);
                    work->attackAction    = ACTOR_01600_ACTION_LUNGE_RECOVER;
                    work->active          = 1;
                    work->behavior        = ACTOR_01600_BEHAVIOR_ATTACK;
                    work->animBlendFrames = 0;
                    work->animPlaying     = 0;
                    work->animFrame       = 0;
                    work->biteLanded      = 0;
                    work->animRequest     = ACTOR_01600_ANIMATION_LUNGE;
                    return 0;
                }
                goto skipLiveUpdate;
            }
            if (actor->extra.tmd->flags & TMD_OBJECT_SKIP_ACTIVE_DRAW) {
                goto skipLiveUpdate;
            }
            _actor01600StepAnimation(actor);
            _actor01600StepStagedMotion(actor);
            if (work->colorRefresh != 0) {
                _actor01600SampleColorAtCoord(enemy, actor->extra.tmd->coords + 1);
                work->colorRefresh = 0;
            }
            rootCoord->composeStamp = GRAPHICS_COORD_DIRTY;
            goto skipLiveUpdate;
        default:
            if (gSceneCombatState.actor01600Wave < (s32)enemy->place->mode) {
                goto skipLiveUpdate;
            }
            lateWaveDelay       = (u16)work->reactionDelay - 1;
            work->reactionDelay = lateWaveDelay;
            if ((lateWaveDelay << 0x10) != 0) {
                goto skipLiveUpdate;
            }
            enemy->node.state.parts.flags = 0;
            work->active                  = 1;
            _actor01600LinkCollisionBodies(actor);
            tmdAllocPrimitiveBuffer(actor->extra.tmd);
            lateBufferModel          = actor->extra.tmd;
            lateBufferModel->flags  &= (u16)~TMD_OBJECT_SKIP_AUTO_BUFFER;
            lateVisibleModel         = actor->extra.tmd;
            lateVisibleModel->flags &= (u16)~TMD_OBJECT_SKIP_ACTIVE_DRAW;
            work->attackAction       = ACTOR_01600_ACTION_LUNGE_RECOVER;
            work->behavior           = ACTOR_01600_BEHAVIOR_ATTACK;
            work->animBlendFrames    = 0;
            work->animPlaying        = 0;
            work->animFrame          = 0;
            work->biteLanded         = 0;
            work->longLeap           = 1;
            work->animRequest        = ACTOR_01600_ANIMATION_LUNGE;
            entryRootCoord           = actor->extra.tmd->coords;
            memset(&entryRotation, 0, sizeof(entryRotation));
            entryRotation.vy = -0x400;
            RotMatrix(&entryRotation, &entryRootCoord->coord);
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

/// Applies placement-variant motion while a scripted scavenger waits for activation.
///
/// Variant 1 moves and turns the entry leap, or spins and removes a commanded
/// body when playback is disabled. Variants 2 and 4 run delayed turns at their
/// own yaw rates. Positions use world units, angles use 4096 units per turn,
/// and delay/rotation accumulators retain signed-halfword interpretation.
static void _actor01600StepStagedMotion(Task* actor)
{
    SVECTOR          limbRotation;
    GfxCoord*        motionCoord;
    _Actor01600Work* work;
    GfxCoord*        bodyCoord;
    u32              variant;
    s32              scriptedAnimation;
    s32              effectFrame;
    u16              delayCountdown;
    u16              bodyRoll;
    s32              steppedPosition;

    motionCoord = actor->extra.tmd->coords;
    work        = actor->work;
    variant     = ((Enemy*)actor->spawnArg2.pointer)->place->variant;
    bodyCoord   = &actor->extra.tmd->coords[1];

    if (variant == 1) {
        switch (work->animRequest) {
            case ACTOR_01600_ANIMATION_LUNGE:
                work->colorRefresh = variant;
                if ((u32)((u16)work->animFrame - 6) < 0xEU) {
                    motionCoord->coord.t[1] -= 0xC8;
                }
                if ((u32)((u16)work->animFrame - 0x14) < 0xFU) {
                    steppedPosition         = motionCoord->coord.t[1] + 0x96;
                    motionCoord->coord.t[1] = steppedPosition;
                    if (steppedPosition >= -0x497) {
                        motionCoord->coord.t[1] = -0x498;
                    }
                }
                if ((u32)((u16)work->animFrame - 0xB) < 0x14U) {
                    steppedPosition         = motionCoord->coord.t[0] + ((motionCoord->coord.m[0][2] * 0x4B) >> 0xB);
                    motionCoord->coord.t[0] = steppedPosition;
                    if (steppedPosition < 0x3B23) {
                        motionCoord->coord.t[0] = 0x3B23;
                    }
                    steppedPosition         = motionCoord->coord.t[2] + ((motionCoord->coord.m[2][2] * 0x4B) >> 0xB);
                    motionCoord->coord.t[2] = steppedPosition;
                    if (steppedPosition >= -0xD11) {
                        motionCoord->coord.t[2] = -0xD12;
                    }
                }
                if (work->animFrame >= 0x31) {
                    work->animRequest = ACTOR_01600_ANIMATION_TURN_WRAPPED;
                    work->animPlaying = 0;
                    work->animFrame   = 0;
                }
                break;
            case ACTOR_01600_ANIMATION_TURN_WRAPPED:
                if ((u32)((u16)work->animFrame - 4) < 0x11U) {
                    work->scriptedRotation.vx  = 0;
                    work->scriptedRotation.vz  = 0;
                    work->scriptedRotation.vy += 0x32;
                    RotMatrix(&work->scriptedRotation, &motionCoord->coord);
                }
                if (work->animFrame >= 0x1C) {
                    work->animRequest = ACTOR_01600_ANIMATION_SETTLE;
                    work->animPlaying = 0;
                    work->animFrame   = 0;
                }
                break;
            case ACTOR_01600_ANIMATION_NONE:
                effectFrame = work->animFrame;
                if (effectFrame == 2) {
                    work->hitEffect.coord      = bodyCoord;
                    work->hitEffect.spawnArgLo = 0x100;
                    work->hitEffect.spawnArgHi = effectFrame;
                    effectSpawnHit(EFFECT_HIT_KIND_WEAPON_PUFF, bodyCoord, 0, &work->hitEffect);
                    effectSpawn(EFFECT_CRITICAL_HIT, &actor->extra.tmd->coords[1], 0, NULL);
                }
                if ((u32)((u16)work->animFrame - 0xF) < 8U) {
                    motionCoord->coord.t[1] += 0x80;
                }
                if (work->animFrame >= 3) {
                    motionCoord->coord.t[0] += 0x10E;
                    motionCoord->coord.t[2] -= 0xC8;
                    motionCoord              = &actor->extra.tmd->coords[6];
                    limbRotation.vx          = -0x400;
                    limbRotation.vy          = 0;
                    limbRotation.vz          = 0;
                    RotMatrix(&limbRotation, &motionCoord->coord);
                    motionCoord     = &actor->extra.tmd->coords[8];
                    limbRotation.vx = -0x400;
                    limbRotation.vy = 0;
                    limbRotation.vz = 0;
                    RotMatrix(&limbRotation, &motionCoord->coord);
                    if (work->animFrame >= 3) {
                        work->scriptedRotation.vx  = 0x384;
                        work->scriptedRotation.vy += 0x96;
                        bodyRoll                   = work->scriptedRotation.vz + 0x64;
                        work->scriptedRotation.vz  = bodyRoll;
                        if ((s16)bodyRoll >= 0x384) {
                            work->scriptedRotation.vz = 0x384;
                        }
                        motionCoord = &actor->extra.tmd->coords[1];
                        RotMatrix(&work->scriptedRotation, &motionCoord->coord);
                    }
                }
                if (work->animFrame >= 0x12) {
                    _actor01600Remove(actor, 0);
                }
                work->animFrame += 1;
                break;
        }
    }
    if (variant == 2) {
        delayCountdown      = work->scriptedDelay - 1;
        work->scriptedDelay = delayCountdown;
        if ((s16)delayCountdown < 0) {
            scriptedAnimation   = work->scriptedAnim;
            work->scriptedDelay = 1;
            switch (scriptedAnimation) {
                case ACTOR_01600_ANIMATION_TURN:
                    work->animRequest = scriptedAnimation;
                    if ((u32)((u16)work->animFrame - 4) < 0x11U) {
                        work->scriptedRotation.vx  = 0;
                        work->scriptedRotation.vz  = 0;
                        work->scriptedRotation.vy += 0x32;
                        RotMatrix(&work->scriptedRotation, &motionCoord->coord);
                    }
                    if (work->animFrame >= 0x1C) {
                        work->scriptedAnim = ACTOR_01600_ANIMATION_SETTLE;
                        work->animRequest  = ACTOR_01600_ANIMATION_SETTLE;
                        work->animPlaying  = 0;
                        work->animFrame    = 0;
                    }
                    break;
                case ACTOR_01600_ANIMATION_SETTLE:
                    work->animRequest = scriptedAnimation;
                    if (work->animFrame >= 0x32) {
                        work->animFrame    = 0;
                        work->animPlaying  = 0;
                        work->animRequest  = ACTOR_01600_ANIMATION_ROAM_IDLE;
                        work->scriptedAnim = 0;
                    }
                    break;
            }
        }
    }
    if (variant == 4) {
        delayCountdown      = work->scriptedDelay - 1;
        work->scriptedDelay = delayCountdown;
        if ((s16)delayCountdown < 0) {
            scriptedAnimation   = work->scriptedAnim;
            work->scriptedDelay = 1;
            if (scriptedAnimation == ACTOR_01600_ANIMATION_TURN) {
                work->animRequest = scriptedAnimation;
                if ((u32)((u16)work->animFrame - 4) < 0x11U) {
                    work->scriptedRotation.vx  = 0;
                    work->scriptedRotation.vz  = 0;
                    work->scriptedRotation.vy -= 0xFA;
                    RotMatrix(&work->scriptedRotation, &motionCoord->coord);
                }
                if (work->animFrame >= 0x1C) {
                    work->scriptedAnim = ACTOR_01600_ANIMATION_SETTLE;
                    work->animRequest  = ACTOR_01600_ANIMATION_SETTLE;
                    work->animPlaying  = 0;
                    work->animFrame    = 0;
                }
            }
        }
    }
}

/// Spawns textured detached body parts and the burst effect at the scavenger's body.
///
/// An intact body picks a head, ear or leg and then hides its original model.
/// A nonzero burst state spawns the head and both ears without changing the
/// original model flags. Each spawn borrows the selected asset from this overlay;
/// the bank-8 descriptor is rebound synchronously before creating each part.
static void _actor01600SpawnBurstParts(Task* actor)
{
    // Billboard size parameters occupy the gravity particle's low 12 argument bits.
    enum {
        ACTOR_01600_MULTI_PART_BURST_SIZE  = 0x300,
        ACTOR_01600_SINGLE_PART_BURST_SIZE = 0x50,
    };

    EffectWork* effect;
    TmdObject*  hiddenModel;
    TmdObject*  bufferModel;
    s32         randomState;
    s32         partChoice;

    if (((_Actor01600Work*)actor->work)->burstState != ACTOR_01600_BURST_INTACT) {
        ACTOR_01600_SPAWN_BURST_PART(actor, &_gActor01600ScavengerBurstHead, 1, effect);
        ACTOR_01600_SPAWN_BURST_PART(actor, &_gActor01600ScavengerBurstEar, 2, effect);
        ACTOR_01600_SPAWN_BURST_PART(actor, &_gActor01600ScavengerBurstEar, 3, effect);
        effectSpawn(EFFECT_030, actor->extra.tmd->coords + 1, ACTOR_01600_MULTI_PART_BURST_SIZE, &Actor01600_D12868);
        return;
    }
    randomState     = (gRandomLcgState * RANDOM_LCG_MULTIPLIER) + RANDOM_LCG_INCREMENT;
    gRandomLcgState = randomState;
    partChoice      = ((u32)randomState >> 0x10) & 3;
    switch (partChoice) {
        case 0:
        case 1:
            ACTOR_01600_SPAWN_BURST_PART(actor, &_gActor01600ScavengerBurstHead, 1, effect);
            break;
        case 2:
            ACTOR_01600_SPAWN_BURST_PART(actor, &_gActor01600ScavengerBurstEar, 2, effect);
            break;
        case 3:
            ACTOR_01600_SPAWN_BURST_PART(actor, &_gActor01600ScavengerBurstLeg, 6, effect);
            break;
    }
    effectSpawn(EFFECT_030, actor->extra.tmd->coords + 1, ACTOR_01600_SINGLE_PART_BURST_SIZE, &Actor01600_D12868);
    hiddenModel         = actor->extra.tmd;
    hiddenModel->flags |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
    bufferModel         = actor->extra.tmd;
    bufferModel->flags |= TMD_OBJECT_SKIP_AUTO_BUFFER;
}

#undef ACTOR_01600_SPAWN_BURST_PART

/// Dispatches the scavenger task's setup, live or death state to its enemy handler.
///
/// Requires state 0..2 and a live Enemy in `spawnArg2.pointer`. The descriptor
/// and handler table belong to this overlay instance. The state indexes the
/// three-entry handler table without a bounds check.
static void _actor01600Task(Task* actor)
{
    EnemyTaskFuncTable3 handlers;

    handlers = Actor01600_D00004;
    handlers.funcs[actor->state](((Enemy*)actor->spawnArg2.pointer), actor);
}

/// Saves the pre-step root position and applies this frame's forward and vertical motion.
///
/// Forward distance is world units along the root's Q12 Z axis. Unsuspended
/// airborne roots add their signed vertical speed (negative rises); grounded
/// roots step downward by 128 units for the following floor correction.
/// Suspension suppresses only the vertical step. Composition is left to the caller.
static void _actor01600StepRootMotion(Task* actor)
{
    _Actor01600Work* work;
    GfxCoord*        rootCoord;

    enum { ACTOR_01600_GROUND_STEP = 0x80 };

    rootCoord                 = actor->extra.tmd->coords;
    work                      = actor->work;
    work->previousPosition.vx = rootCoord->coord.t[0];
    work->previousPosition.vy = rootCoord->coord.t[1];
    work->previousPosition.vz = rootCoord->coord.t[2];
    rootCoord->coord.t[0]    += (rootCoord->coord.m[0][2] * work->forwardSpeed) >> 0xC;
    rootCoord->coord.t[2]    += (rootCoord->coord.m[2][2] * work->forwardSpeed) >> 0xC;
    if (work->suspended == 0) {
        if (work->airborne != 0) {
            rootCoord->coord.t[1] += work->verticalSpeed;
            return;
        }
        rootCoord->coord.t[1] += ACTOR_01600_GROUND_STEP;
    }
}

/// Refreshes the scavenger's lighting color at model coordinate 1's cached view-space position.
///
/// Requires a live enemy and an initialized body cache; does not compose it.
/// Borrows and releases one VECTOR scratch block, publishing its cursor after
/// the position is filled.
static void _actor01600RefreshBodyColor(Enemy* enemy, Task* actor)
{
    GfxCoord* bodyCoord;
    void**    cursorSlot;
    VECTOR*   scratchCursor;
    VECTOR*   position;

    bodyCoord                         = &actor->extra.tmd->coords[1];
    cursorSlot                        = SCRATCH_HEAD_ADDR;
    scratchCursor                     = SCRATCH_HEAD_AT(cursorSlot, VECTOR);
    position                          = scratchCursor - 1;
    position->vx                      = bodyCoord->workm.t[0];
    position->vy                      = bodyCoord->workm.t[1];
    position->vz                      = bodyCoord->workm.t[2];
    SCRATCH_HEAD_AT(cursorSlot, void) = position;
    worldCoordUpdateActorColor(enemy, position, 0, 0);
    SCRATCH_POP_BYTES_AT(cursorSlot, sizeof(*position));
}

/// Flattens the corpse from its saved death transform without compounding scale.
///
/// Decreases the Q12 Y scale by 80 while it exceeds 512, preserving the
/// retained undershoot instead of clamping. X and Z stay at unity.
/// Requires the saved death matrix and one free `ActorScaleScratch` block;
/// restores the full root transform before scaling and marks composition dirty.
static void _actor01600SquashCorpse(Task* actor)
{
    GfxCoord*          rootCoord;
    ActorScaleScratch* scratchHead;
    ActorScaleScratch* scratch;
    _Actor01600Work*   work;

    enum { ACTOR_01600_CORPSE_MIN_SCALE_Y = 0x200,
           ACTOR_01600_CORPSE_SCALE_STEP  = 0x50 };

    scratchHead                             = SCRATCH_STACK_CURSOR(ActorScaleScratch);
    work                                    = actor->work;
    scratch                                 = scratchHead - 1;
    SCRATCH_STACK_CURSOR(ActorScaleScratch) = scratch;
    rootCoord                               = actor->extra.tmd->coords;
    if (work->deathScaleY >= ACTOR_01600_CORPSE_MIN_SCALE_Y + 1) {
        work->deathScaleY = (u16)work->deathScaleY - ACTOR_01600_CORPSE_SCALE_STEP;
    }
    ACTOR_01600_APPLY_CORPSE_SCALE(rootCoord, &work->deathMatrix, work->deathScaleY, scratch);
    SCRATCH_STACK_RELEASE_BLOCK(ActorScaleScratch);
}

/// Moves the model root sideways across its facing on the ground plane.
///
/// `lateralDistance` is in world units, narrowed to signed 16 bits. Positive
/// moves toward the yaw-rotated +X axis: (cos yaw, 0, -sin yaw), hence +X
/// when facing +Z. Pitch, roll and basis scale do not affect the step.
/// Borrows and releases one sidestep scratch block; composition is left to the caller.
static void _actor01600Sidestep(Task* actor, s32 lateralDistance)
{
    MATRIX*                     rotation;
    _Actor01600SidestepScratch* scratch;
    GfxCoord*                   rootCoord;

    rootCoord                                        = actor->extra.tmd->coords;
    scratch                                          = SCRATCH_STACK_CURSOR(_Actor01600SidestepScratch) - 1;
    scratch->step.vx                                 = (s16)lateralDistance;
    scratch->step.vy                                 = 0;
    scratch->step.vz                                 = 0;
    SCRATCH_STACK_CURSOR(_Actor01600SidestepScratch) = scratch;
    gfxReadMatrixZAxis(&actor->extra.tmd->coords->coord, &scratch->facing);
    rotation     = &scratch->rotation;
    scratch->yaw = ratan2(scratch->facing.vx, scratch->facing.vz);
    gfxSetRotIdentity(rotation);
    RotMatrixY(scratch->yaw, rotation);
    ApplyMatrixLV(rotation, &scratch->step, &scratch->step);
    rootCoord->coord.t[0] += scratch->step.vx;
    rootCoord->coord.t[1] += scratch->step.vy;
    rootCoord->coord.t[2] += scratch->step.vz;
    SCRATCH_STACK_RELEASE_BLOCK(_Actor01600SidestepScratch);
}

/// Right-multiplies the animated body's basis by recoil without changing translation.
///
/// Matrices use Q12 coefficients, and the destination must not alias `recoil`.
/// Reads the animated basis once into GTE rotation state before writing columns.
static __inline__ void _actor01600ComposeRecoil(MATRIX* body, const MATRIX* recoil)
{
    gte_SetRotMatrix(body);
    gte_ldclmv(recoil);
    gte_rtir();
    gte_stclmv(body);
    gte_ldclmv(&recoil->m[0][1]);
    gte_rtir();
    gte_stclmv(&body->m[0][1]);
    gte_ldclmv(&recoil->m[0][2]);
    gte_rtir();
    gte_stclmv(&body->m[0][2]);
}

/// Adds the current recoil rotation to model part 1, then decays its pitch.
///
/// Angles use 4096 units per turn. A nonzero pitch of at most 32 clears the
/// recoil flag; a larger pitch loses 32 per tick. Zero leaves the flag as is.
/// Requires the animated part-1 basis and one free MATRIX scratch block;
/// preserves translation and changes GTE rotation state.
static void _actor01600StepRecoil(Task* actor)
{
    _Actor01600Work* work;
    GfxCoord*        rootCoord;
    MATRIX*          scratch;
    MATRIX*          scratchHead;
    s16              recoilPitch;

    enum { ACTOR_01600_RECOIL_DECAY_STEP = 32 };

    scratchHead                  = SCRATCH_STACK_CURSOR(MATRIX);
    SCRATCH_STACK_CURSOR(MATRIX) = scratchHead - 1;
    scratch                      = scratchHead - 1;
    work                         = actor->work;
    rootCoord                    = actor->extra.tmd->coords;
    // Apply recoil to the animated body part, preserving its translation.
    RotMatrix(&work->recoilRotation, scratch);
    _actor01600ComposeRecoil(&rootCoord[1].coord, scratch);
    recoilPitch = work->recoilRotation.vx;
    if (recoilPitch != 0) {
        if (recoilPitch < ACTOR_01600_RECOIL_DECAY_STEP + 1) {
            work->recoilRotation.vx = 0;
            work->recoilActive      = 0;
        } else {
            work->recoilRotation.vx = (u16)work->recoilRotation.vx - ACTOR_01600_RECOIL_DECAY_STEP;
        }
    }
    SCRATCH_STACK_RELEASE_BLOCK(MATRIX);
}

/// Probes the target heading and resumes roaming if its direct path is blocked.
///
/// Enables the path probe and advances one search phase. Returns 1 when the
/// search enters its sweep phases, resetting the search and disabling sight;
/// otherwise returns 0 with the directed probe still enabled. Contacts are
/// from the preceding collision pass, so this is an incremental check.
static s32 _actor01600ResumeRoamIfBlocked(Task* actor)
{
    _Actor01600Work* work;

    work                        = actor->work;
    work->pathProbe.body.flags |= (WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED);
    _actor01600SearchClearHeading(actor);
    if (work->pathSearchPhase >= ACTOR_01600_PATH_SEARCH_BEGIN_SWEEP) {
        _actor01600ReturnToRoam(work);
        return 1;
    }
    return 0;
}

/// Starts an attack turn when the target lies more than 768 yaw units off-axis.
///
/// `relativeYaw` is signed, in 4096-unit turns; `unusedDistance` is ignored.
/// Returns 1 after selecting the direct or wrapped turn action and resetting
/// timing, otherwise 0. Rate is the shorter magnitude divided by 16, at least
/// 32 yaw units per tick; the original signed request is retained.
static s32 _actor01600TryBeginAttackTurn(Task* actor, s32 relativeYaw, s32 unusedDistance)
{
    SVECTOR          facingAxis;
    SVECTOR          unusedVector; // Unused storage in the retained stack frame; role unproven
    _Actor01600Work* work;
    s32              turnMagnitude;
    s32              wrappedMagnitude;
    s32              shorterMagnitude;

    work = actor->work;
    if (ABS(relativeYaw) < ACTOR_01600_ATTACK_TURN_YAW + 1) {
        return 0;
    }
    work->turnRequest = relativeYaw;
    // The facing is sampled here even though the computed yaw is discarded.
    gfxReadMatrixZAxis(&actor->extra.tmd->coords->coord, &facingAxis);
    ratan2(facingAxis.vx, facingAxis.vz);
    turnMagnitude    = ABS(work->turnRequest);
    wrappedMagnitude = ACTOR_TRANSFORM_ANGLE_TURN - turnMagnitude;
    if (wrappedMagnitude < turnMagnitude) {
        work->attackAction = ACTOR_01600_ACTION_TURN_WRAPPED;
        shorterMagnitude   = wrappedMagnitude;
    } else {
        work->attackAction = ACTOR_01600_ACTION_TURN;
        shorterMagnitude   = turnMagnitude;
    }
    work->turnRate = shorterMagnitude / 16;
    if (work->turnRate < ACTOR_01600_MIN_TURN_STEP) {
        work->turnRate = ACTOR_01600_MIN_TURN_STEP;
    }
    work->animFrame = 0;
    work->turnMode  = ACTOR_01600_TURN_REQUEST_BEGIN;
    return 1;
}

/// Selects a lunge against a nearby target ahead, or resumes roaming on a blocked path.
///
/// Yaw is signed in 4096-unit turns and horizontal distance is in world units.
/// Admits distance <= 2000 and |yaw| <= 512; returns 1 when it handles the
/// choice, including a return to roam, otherwise 0. A clear path selects the
/// recovery lunge for 20 of 100 random residues and the ordinary lunge otherwise.
static s32 _actor01600TryBeginLunge(Task* actor, s32 relativeYaw, s32 horizontalDistance)
{
    _Actor01600Work* work;
    s32              randomProduct;
    s32              resumedRoam;
    s32              yawMagnitude;
    u32              randomState;

    enum { ACTOR_01600_LUNGE_RECOVER_PERCENT = 20 };

    work = actor->work;
    if (horizontalDistance < ACTOR_01600_LUNGE_REACH + 1) {
        yawMagnitude = (relativeYaw >= 0 ? relativeYaw : -relativeYaw);
        if (yawMagnitude < ACTOR_01600_LUNGE_YAW + 1) {
            work->pathProbe.body.flags |= (WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED);
            _actor01600SearchClearHeading(actor);
            resumedRoam = 0;
            if (work->pathSearchPhase >= ACTOR_01600_PATH_SEARCH_BEGIN_SWEEP) {
                _actor01600ReturnToRoam(work);
                resumedRoam = 1;
            }
            if ((u8)resumedRoam) {
                return 1;
            } else {
                randomProduct         = gRandomLcgState * RANDOM_LCG_MULTIPLIER;
                randomState           = randomProduct + RANDOM_LCG_INCREMENT;
                work->animBlendFrames = 0;
                work->animPlaying     = 0;
                work->biteLanded      = 0;
                gRandomLcgState       = randomState;
                if ((u32)(((randomState >> 16) % 100) & 0xFFFF) < (u32)ACTOR_01600_LUNGE_RECOVER_PERCENT) {
                    work->attackAction = ACTOR_01600_ACTION_LUNGE_RECOVER;
                    work->animRequest  = ACTOR_01600_ANIMATION_LUNGE;
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

/// Reports whether a scene child model is hidden before advancing the final wave.
///
/// Returns 1 if any child skips active drawing, 0 if none do, or 0xFF if the
/// scene has no children. Requires the scene slot and model-bearing children
/// in a closed sibling ring; it does not change their state.
static u8 _actor01600CheckSceneChildrenHidden(void)
{
    enum { ACTOR_01600_SCENE_CHILDREN_EMPTY = 0xFF };

    Task* firstChild;
    Task* child;

    firstChild = gameGetTaskSlot(GAME_TASK_SLOT_SCENE)->firstChild;
    if (firstChild == NULL) {
        return ACTOR_01600_SCENE_CHILDREN_EMPTY;
    }
    child = firstChild;
    do {
        if (child->extra.tmd->flags & TMD_OBJECT_SKIP_ACTIVE_DRAW) {
            return 1;
        }
        child = child->nextSibling;
    } while (child != firstChild);
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

/// Gives a spawned burst part the source actor's texture-page and CLUT-row offsets.
///
/// Both tasks must own live TMD objects. Rebuilds both primitive-buffer halves
/// when the part has a buffer; an unbuffered part receives the offsets only.
static void _actor01600CopyBurstTextures(Task* burstPart, Task* actor)
{
    TmdObject* actorModel;
    TmdObject* partModel;

    actorModel                   = actor->extra.tmd;
    partModel                    = burstPart->extra.tmd;
    partModel->texturePageOffset = actorModel->texturePageOffset;
    partModel->clutRowOffset     = actorModel->clutRowOffset;
    if (partModel->buffer != NULL) {
        tmdBuildBufferHalf(partModel);
        tmdBuildBufferHalf(partModel);
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
