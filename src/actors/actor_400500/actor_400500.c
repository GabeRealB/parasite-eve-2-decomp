#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/inline_c.h>
#include <psyq/memory.h>

#include "common.h"
#include "gte.h"

#include "actors/actor.h"

#include "gameplay/actor.h"
#include "gameplay/actor_render.h"
#include "gameplay/animation.h"
#include "gameplay/area_entry.h"
#include "gameplay/attachment_state.h"
#include "gameplay/attachments.h"
#include "gameplay/collision.h"
#include "gameplay/damage.h"
#include "gameplay/effects.h"
#include "gameplay/enemy.h"
#include "gameplay/hud_sprites.h"
#include "gameplay/loading.h"
#include "gameplay/message.h"
#include "gameplay/model_lighting.h"
#include "gameplay/pad_script.h"
#include "gameplay/enemy_params.h"
#include "gameplay/player_actor.h"
#include "gameplay/room_effects.h"
#include "gameplay/scene_combat.h"
#include "gameplay/scene_runtime.h"
#include "gameplay/sprites.h"
#include "gameplay/world_collision.h"
#include "gameplay/world_coords.h"
#include "gameplay/world_targets.h"

#include "main/coord.h"
#include "main/display.h"
#include "main/display_types.h"
#include "main/gameflag.h"
#include "main/random.h"
#include "main/gfx.h"
#include "main/gfx_types.h"
#include "main/mc.h"
#include "main/mc_types.h"
#include "main/mem.h"
#include "main/scratch.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/sound.h"
#include "main/stage.h"
#include "main/task.h"
#include "main/task_types.h"
#include "main/tmd.h"
#include "main/tmd_types.h"
#include "main/wipsys.h"
#include "main/wipsys_types.h"

#include "overlay.h"
static void _frameCaptureQueue(s32 orderingTableSlot);
#define FRAME_CAPTURE_QUEUE _frameCaptureQueue
#include "../../shared/frame_capture.h"
#include "../../shared/coord_math.h"

/// Values of `_Actor400500GrayStalkerWork::state` while the Stalker is alive.
///
/// Each is entered by name, together with a reset of `subState`, and selects
/// a handler of `_Actor400500StateTable`. The death sequence and the teardown
/// step the same field through tables of their own, outside this numbering.
enum {
    ACTOR_400500_STATE_CRAWL          = 0x0, // crawls the ceiling or the floor along the room's axes; picks the attacks, the drop and the jump
    ACTOR_400500_STATE_GRAB           = 0x1, // reaches down from the ceiling for a target right below it and carries what it catches
    ACTOR_400500_STATE_STRIKE_CEILING = 0x2, // swings the right arm out from the ceiling and strikes
    ACTOR_400500_STATE_STRIKE_FLOOR   = 0x3, // swings the left arm out on the floor and strikes, then crawls on or jumps up
    ACTOR_400500_STATE_FALL           = 0x4, // falls from the ceiling onto its back and gets up into `CRAWL_FALLEN`
    ACTOR_400500_STATE_KNOCKDOWN      = 0x5, // recoil of a knockdown hit; one taken on the ceiling ends in a fall
    ACTOR_400500_STATE_AMBUSH         = 0x6, // first state: waits cloaked until the target comes close, then strikes, or until it is hit
    ACTOR_400500_STATE_DROP           = 0x7, // lets go of the ceiling and lands on the floor where it hung
    ACTOR_400500_STATE_JUMP           = 0x8, // jumps from the floor back onto the ceiling
    ACTOR_400500_STATE_TURN_OVER      = 0x9, // ends `CRAWL_FALLEN`: turns over, reversing `yaw`, then crawls on or jumps up
    ACTOR_400500_STATE_CRAWL_FALLEN   = 0xA, // crawls the floor after a fall from the ceiling, until it turns over
    ACTOR_400500_STATE_ROOM_SEQUENCE  = 0xB, // sequence started by room command 1, whose steps wait for commands 2, 3 and 4
    ACTOR_400500_STATE_STATUS_HOLD    = 0xC, // recoil held until the status buildup runs out; one taken on the ceiling ends in a fall
    ACTOR_400500_STATE_COUNT                 // number of states, and of the handlers in `_Actor400500StateTable`
};

/// The living Stalker's state handlers, indexed by `ACTOR_400500_STATE_*`.
///
/// The package defines one table. While combat is running, the task's
/// per-frame handler copies it to the stack by assignment, which is why the
/// array is wrapped in a struct, and calls the entry of the current state.
/// The call is unconditional and every entry is a handler. Most of them run a
/// table of their own in turn, indexed by `subState`.
typedef struct {
    TaskFunc handlers[ACTOR_400500_STATE_COUNT]; // Handler of each `ACTOR_400500_STATE_*`, taking the Stalker's task
} _Actor400500StateTable;
STATIC_ASSERT_SIZEOF(_Actor400500StateTable, ACTOR_400500_STATE_COUNT * sizeof(TaskFunc));

/// Values of `_Actor400500GrayStalkerWork::animRequest`.
enum {
    ACTOR_400500_ANIM_REQUEST_BLEND   = 1, // blend into `animId` over `animBlendFrames` frames
    ACTOR_400500_ANIM_REQUEST_RESET   = 2, // cut straight to `animId`
    ACTOR_400500_ANIM_REQUEST_PLAYING = 3  // `animId` has been applied and is playing
};

/// Bits of `_Actor400500GrayStalkerWork::posture`.
enum {
    ACTOR_400500_POSTURE_ON_FLOOR = 1 << 0, // standing on the floor; clear while it hangs from the ceiling
    ACTOR_400500_POSTURE_ON_BACK  = 1 << 1  // landed on its back; cleared when it rights itself
};

/// Values of `_Actor400500GrayStalkerWork::cloakRequest`.
///
/// The running bit is the sign bit of the byte, so a fade in progress reads
/// negative. The low bits say which fade it is.
enum {
    ACTOR_400500_CLOAK_HIDE      = 0,    // cloak, then fade the body out and drop it from lock-on
    ACTOR_400500_CLOAK_SHOW      = 1,    // fade the body in and make it lockable, then drop the cloak
    ACTOR_400500_CLOAK_KIND_MASK = 0x7F, // the fade a request asks for
    ACTOR_400500_CLOAK_RUNNING   = 0x80  // the fade has not finished
};

/// Values of `_Actor400500GrayStalkerWork::turnRequest`.
enum {
    ACTOR_400500_TURN_NONE     = 0,
    ACTOR_400500_TURN_YAW_DOWN = 1, // turn on the spot towards lower `yaw`
    ACTOR_400500_TURN_YAW_UP   = 2  // turn on the spot towards higher `yaw`
};

/// Values of `_Actor400500GrayStalkerWork::hitReaction`, the reaction a hit asks for.
enum {
    ACTOR_400500_HIT_REACTION_NONE   = 0,
    ACTOR_400500_HIT_REACTION_LIGHT  = 1, // light recoil
    ACTOR_400500_HIT_REACTION_HEAVY  = 2, // heavy recoil
    ACTOR_400500_HIT_REACTION_STATUS = 3, // held until the status buildup runs out
    ACTOR_400500_HIT_REACTION_BLAST  = 4  // heavy recoil; a blast that kills bursts the body
};

/// Target-range gates in game-coordinate units; bearings use 4096 units per turn.
enum {
    ACTOR_400500_GRAB_RANGE              = 0x500,
    ACTOR_400500_STRIKE_RANGE            = 0x640,
    ACTOR_400500_GRAB_HALF_ANGLE         = 0x2E0,
    ACTOR_400500_STRIKE_HALF_ANGLE       = 0x300,
    ACTOR_400500_TARGET_MOVE_RANGE_SCALE = 8
};

/// Sound-script entries for step contacts and grab cues, before adding a placement tag.
enum {
    ACTOR_400500_SOUND_STEP_1        = 0x40050001,
    ACTOR_400500_SOUND_STEP_2        = 0x40050002,
    ACTOR_400500_SOUND_GRAB_FRAME_12 = 6, // Uses bank zero for this cue
    ACTOR_400500_SOUND_GRAB_FRAME_30 = 0x40050007,
    ACTOR_400500_SOUND_GRAB_FRAME_42 = 0x40050008
};

/// Model coordinates at the ends of the two arm chains.
enum {
    ACTOR_400500_PART_RIGHT_ARM_TIP = 8,
    ACTOR_400500_PART_LEFT_ARM_TIP  = 11
};

/// Steps of the ordinary crawl route, indexed by the living crawl's `subState`.
enum {
    ACTOR_400500_CRAWL_STEP_SELECT                 = 0,
    ACTOR_400500_CRAWL_STEP_POSITIVE_X             = 1,
    ACTOR_400500_CRAWL_STEP_TURN_NEGATIVE_X        = 2,
    ACTOR_400500_CRAWL_STEP_NEGATIVE_X             = 3,
    ACTOR_400500_CRAWL_STEP_TURN_POSITIVE_X        = 4,
    ACTOR_400500_CRAWL_STEP_CORNER_CHOICE          = 5,
    ACTOR_400500_CRAWL_STEP_POSITIVE_Z             = 6,
    ACTOR_400500_CRAWL_STEP_TURN_NEGATIVE_Z        = 7,
    ACTOR_400500_CRAWL_STEP_NEGATIVE_Z             = 8,
    ACTOR_400500_CRAWL_STEP_CORNER_TURN_NEGATIVE_X = 9,
    ACTOR_400500_CRAWL_STEP_TURN_POSITIVE_Z        = 10
};

/// IDs of the route rectangles; zero is outside every rectangle.
enum {
    ACTOR_400500_CRAWL_ZONE_X_SEGMENT = 1,
    ACTOR_400500_CRAWL_ZONE_CORNER    = 2,
    ACTOR_400500_CRAWL_ZONE_Z_SEGMENT = 3,
    ACTOR_400500_CRAWL_ZONE_X_END     = 4,
    ACTOR_400500_CRAWL_ZONE_PATROL    = 5,
    ACTOR_400500_CRAWL_ZONE_Z_END     = 6
};

/// Crawl headings in 4096ths of a turn, clip index and sixteenth-frame rate.
enum {
    ACTOR_400500_CRAWL_YAW_POSITIVE_Z = 0,
    ACTOR_400500_CRAWL_YAW_POSITIVE_X = ACTOR_TRANSFORM_ANGLE_TURN / 4,
    ACTOR_400500_CRAWL_YAW_NEGATIVE_Z = ACTOR_TRANSFORM_ANGLE_HALF_TURN,
    ACTOR_400500_CRAWL_YAW_NEGATIVE_X = ACTOR_TRANSFORM_ANGLE_TURN * 3 / 4,
    ACTOR_400500_ANIM_CRAWL           = 2,
    ACTOR_400500_CRAWL_RATE           = ANIMATION_RATE_ONE * 3 / 2
};

/// The two crawl lines and the ceiling reset position, in room-coordinate units.
enum {
    ACTOR_400500_CRAWL_LINE_X               = 16500,
    ACTOR_400500_CRAWL_LINE_Z               = -8350,
    ACTOR_400500_CRAWL_RESET_X              = -1000,
    ACTOR_400500_CRAWL_CEILING_Y            = -4000,
    ACTOR_400500_CRAWL_RESET_Z              = -8470,
    ACTOR_400500_POSTURE_CHANGE_TARGET_GAP  = 4000,
    ACTOR_400500_POSTURE_CHANGE_CHANCE_MASK = 31 // One accepted high-word value out of 32
};

/// Arm-strike cues, whole state ticks and 4096ths-of-a-turn return increments.
enum {
    ACTOR_400500_SOUND_ARM_STRIKE            = 0x40050005,
    ACTOR_400500_CEILING_STRIKE_CONTACT_TICK = 11,
    ACTOR_400500_CEILING_STRIKE_RETURN_TICK  = 18,
    ACTOR_400500_FLOOR_STRIKE_CONTACT_TICK   = 13,
    ACTOR_400500_FLOOR_STRIKE_RETURN_TICK    = 16,
    ACTOR_400500_ARM_RETURN_ANGLE_STEP       = 128,
    ACTOR_400500_ATTACK_COOLDOWN_TICKS       = 60,
    ACTOR_400500_ARM_LEFT                    = 0,
    ACTOR_400500_ARM_RIGHT                   = 1
};

/// Motion, recoil and fallen-route values used by the Stalker's transition handlers.
///
/// Heights are root-parent game coordinates, angles are 4096ths of a turn,
/// and delays are whole handler ticks. Animation IDs index the loaded set table.
enum {
    ACTOR_400500_PART_BODY                 = 3,
    ACTOR_400500_POSTURE_CHANGE_STEP_START = 0,
    ACTOR_400500_TURN_OVER_STEP_START      = 0,
    ACTOR_400500_CLOAK_PHASE_FIRST_RAMP    = 0,
    ACTOR_400500_FLOOR_Y                   = -1000,
    ACTOR_400500_FALL_PIVOT_Y              = -2200,
    ACTOR_400500_ANIM_CEILING_HOLD         = 1,
    ACTOR_400500_ANIM_FALLEN_CRAWL         = 4,
    ACTOR_400500_ANIM_UPRIGHT_RECOIL       = 15,
    ACTOR_400500_ANIM_ON_BACK_RECOIL       = 17,
    ACTOR_400500_ANIM_FALL_LANDING         = 19,
    ACTOR_400500_ANIM_POSTURE_SETTLE       = 25,
    ACTOR_400500_RECOIL_STEP_FLOOR_WAIT    = 1,
    ACTOR_400500_RECOIL_STEP_CEILING_WAIT  = 3,
    ACTOR_400500_SOUND_DROP_LANDING        = 0x40050003,
    ACTOR_400500_SOUND_FALL_LANDING        = 0x40050006,
    ACTOR_400500_FALLEN_TARGET_GAP         = 4000
};

/// Arm extension angles in 4096ths of a turn and animation-set indices.
enum {
    ACTOR_400500_ARM_EXTEND_ANGLE_STEP  = 128,
    ACTOR_400500_ARM_STRIKE_SWING_ANGLE = ACTOR_TRANSFORM_ANGLE_TURN / 8,
    ACTOR_400500_ANIM_FLOOR_STRIKE      = 7,
    ACTOR_400500_ANIM_CEILING_STRIKE    = 8,
    ACTOR_400500_ANIM_UPRIGHT_RECOVERY  = 16,
    ACTOR_400500_ANIM_ON_BACK_RECOVERY  = 18,
    ACTOR_400500_ANIM_FALL_RECOVERY     = 20
};

/// Sound-script cue used to begin a floor/ceiling posture transition.
enum { ACTOR_400500_SOUND_POSTURE_CHANGE = 0x40050004 };

/// Steps of crawling on the back, indexed by the fallen crawl's `subState`.
enum {
    ACTOR_400500_FALLEN_CRAWL_STEP_SELECT          = 0,
    ACTOR_400500_FALLEN_CRAWL_STEP_POSITIVE_X      = 1,
    ACTOR_400500_FALLEN_CRAWL_STEP_TURN_NEGATIVE_X = 2,
    ACTOR_400500_FALLEN_CRAWL_STEP_NEGATIVE_X      = 3,
    ACTOR_400500_FALLEN_CRAWL_STEP_TURN_POSITIVE_X = 4,
    ACTOR_400500_FALLEN_CRAWL_STEP_CORNER_CHOICE   = 5,
    ACTOR_400500_FALLEN_CRAWL_STEP_POSITIVE_Z      = 6,
    ACTOR_400500_FALLEN_CRAWL_STEP_TURN_NEGATIVE_Z = 7,
    ACTOR_400500_FALLEN_CRAWL_STEP_NEGATIVE_Z      = 8
};

/// Work block of the Gray Stalker task.
///
/// The spawn handler allocates it zeroed and keeps it at `Task::work`. It holds
/// the animation context and its storage, the matrices the model borrows, the
/// body sphere and the two pairs of arm spheres with their contact records, and
/// the state machine: the task state picks a table of states, `state` an entry
/// of that table and `subState` a step of that entry's own table.
///
/// The Stalker crawls the ceiling or the floor along the room's axes, turning
/// on the spot between them. It is cloaked between attacks: a hide request
/// fades the body out and takes it off lock-on, and every attack, recoil and
/// fall starts by requesting a show.
typedef struct {
    ActorAnimRig18        rig;                 // playback of the model's parts: slots 1 to 17 play `animId`, and slot 1's status tells when it ended
    byte                  field_404[0x404];    // never accessed
    MATRIX                savedRootMtx;        // root matrix at the start of the death shrink; each frame rescales a copy of it
    WorldCollisionBody    body;                // sphere on part 3 that takes hits; shrunk while the grab reaches out
    WorldCollisionContact bodyContacts[3];     // contacts of `body`; also the enemy's hit records
    WorldCollisionBody    rightArmOuter;       // attack sphere far along part 7; enabled only while the right arm strikes
    WorldCollisionBody    rightArmInner;       // attack sphere nearer the root of part 7; shares `rightArmContacts`
    WorldCollisionContact rightArmContacts[1]; // contacts of the two right-arm spheres
    WorldCollisionBody    leftArmOuter;        // attack sphere far along part 10; enabled only while the left arm strikes
    WorldCollisionBody    leftArmInner;        // attack sphere nearer the root of part 10; shares `leftArmContacts`
    WorldCollisionContact leftArmContacts[1];  // contacts of the two left-arm spheres
    EffectSpawnArg        effectArg;           // argument record of the effects its hits spawn, hung off part 3
    s16                   pitch;               // root pitch in 4096ths of a turn; swept through half a turn by a drop or a jump
    s16                   yaw;                 // root heading in 4096ths of a turn
    s16                   roll;                // root roll in 4096ths of a turn; half a turn apart on the ceiling and on the floor
    byte                  field_94E[2];        // never accessed
    s16                   dropStartX;          // root X when the drop from the ceiling began; the drop lands back on it
    byte                  field_952[2];        // never accessed
    s16                   dropStartZ;          // root Z when the drop from the ceiling began; the drop lands back on it
    byte                  field_956[6];        // never accessed
    MATRIX                colorMtx;            // storage for the model's `TmdObject::colorMtx`
    MATRIX                lightMtx;            // storage for the model's `TmdObject::lightMtx`
    byte                  field_99C[4];        // never accessed
    SVECTOR3              anchorPos;           // view-space X and Z a part is pinned to while the root moves around it; `vy` is never accessed
    byte                  field_9A6[0x16];     // never accessed
    s16                   armReachAngle;       // turn of parts 6 and 9 towards the target while the grab reaches; eased back to zero after
    byte                  field_9BE[2];        // never accessed
    VECTOR                rootPos;             // root position at the start of the frame; never read
    SVECTOR               playerPrevPos;       // the player's root position when the previous frame ended
    SVECTOR               playerLocalMove;     // the player's movement since then, turned into the Stalker's heading; `vz` biases the attack ranges
    SVECTOR3              toTarget;            // offset from the root to the player, or to the patrol point while the player is in zone 5
    byte                  field_9E6[0xA];      // never accessed
    Task*                 armTasks[2];         // tasks of the arm models (0 left on part 10, 1 right on part 7), shown only during an arm strike
    s16                   animRate;            // playback rate of slots 1..17; `ANIMATION_RATE_ONE` is normal speed
    s16                   animRequest;         // `ACTOR_400500_ANIM_REQUEST_*`
    s16                   appliedAnim;         // animation last applied to the slots
    s16                   animId;              // requested animation: index into the animation set table
    s16                   animFrames;          // frames since `animId` was applied; rescaled when a blend repeats it at a new rate
    s16                   shrinkScaleY;        // Y scale of the death shrink, 0x1000 = 1.0
    u16                   stateFrames;         // frames spent in the current state or sub-state
    u16                   state;               // index into the state table of the current task state
    u16                   subState;            // index into the step table of the current state
    u16                   deathStep;           // step of the fall a kill on the ceiling runs before the death sequence
    byte                  field_A0C[2];        // never accessed
    s16                   animBlendFrames;     // frames a blend request takes; cleared when the blend starts
    s16                   moveAccel;           // added to `moveSpeed` each frame; itself changes by 2 each frame
    s16                   moveSpeed;           // vertical speed of a drop or a jump
    byte                  field_A14[2];        // never accessed
    s16                   targetDist;          // horizontal length of `toTarget`
    s16                   playerCaught;        // 1 once the grab has caught the player; the grab then carries the player with part 8
    s16                   zone;                // id of the `ActorZone` holding the root, 0 outside them all
    u16                   playerZone;          // id of the `ActorZone` holding the player, 0 outside them all or without a player
    u16                   posture;             // `ACTOR_400500_POSTURE_*`
    s16                   cloakLevel;          // grey level of the cloak shading, 0 (none) to 0xFF
    u16                   patrolFrames;        // frames the player has spent in zone 5; bit 8 picks the end of the patrol line `toTarget` aims at
    s16                   colorBlend;          // the model's `TmdObject::shading.colorBlend`, 0 to `TMD_OBJECT_COLOR_BLEND_ONE`
    u16                   armSwingAngle;       // yaw an arm model has swung out by, 0 to 0x200 in steps of 0x80
    s16                   shadowShade;         // brightness of the limb shadows, 0 to 0xFF
    s16                   cloakTimer;          // frames the holding phase of a cloak fade has lasted
    s16                   hideHoldFrames;      // frames a hide holds the cloak before the body fades out
    s16                   hideCooldownReset;   // value `hideCooldown` starts from when a show completes; longer at lower health
    s16                   hideCooldown;        // frames before another hide may be requested
    s16                   attackCooldown;      // frames before the next attack; 0x3C after one ends
    s16                   attackCooling;       // 1 while `attackCooldown` was still running this frame
    u16                   targetBearing;       // heading of `toTarget` relative to `yaw`, 0..0xFFF
    s16                   turnRequest;         // `ACTOR_400500_TURN_*`
    s16                   turnStarted;         // 1 once the requested turn's animation has been started
    s16                   hitTaken;            // 1 when a hit or a status tick dealt damage; lets `hitReaction` be consumed
    s16                   hitReaction;         // `ACTOR_400500_HIT_REACTION_*` awaiting the state machine
    s16                   lastHitReaction;     // the last reaction a hit asked for; never cleared, so the death can tell a blast
    s16                   deathHeld;           // 1 while the current move must finish before the death sequence starts
    s16                   hitCooldown;         // frames before another hit is taken; set from the hit's id parameter 2
    s8                    cloakRequest;        // `ACTOR_400500_CLOAK_*`
    s8                    cloakPhase;          // step of the running cloak fade (0 first ramp, 1 hold, 2 second ramp)
    s8                    grabStep;            // player animation of a grab (0 none, 1 caught, 2 start the follow-up, 3 release follow-up, 4 hit follow-up)
    s8                    ceilingFallPending;  // asks a ceiling crawl to fall onto its back; the crawl states take it, but nothing sets it
    s8                    knockdownPending;    // set by a hit whose id parameter 0 is 8 or 9; the next state step enters the knockdown
    u8                    roomCommand;         // kind of the last room command (1..4); 1 starts the room-driven sequence, whose steps wait for 2, 3 and 4
    u8                    commandActive;       // 1 from room command 1 until command 4 is taken; keeps `WORLD_TARGET_KEEP_SCANNED` off the target
    u8                    grabLanded;          // 1 once the grab has hit the player; picks the player's follow-up animation
    byte                  field_A4E[2];        // never accessed
} _Actor400500GrayStalkerWork;
STATIC_ASSERT_SIZEOF(_Actor400500GrayStalkerWork, 0xA50);

extern ActorZone D_actor_400500_80153D6C[];

static void _actor400500DrawLimbShadow(Task* task, s16 firstJoint, s16 secondJoint, s16 halfWidth, s32 worldY, s32 shade);
static void _actor400500FinishGrabRecovery(Task* task);
static s32  _actor400500LocalizeWorldRotation(const GfxCoord* joint, MATRIX* worldRotation);

/* `D_800678F0` selects the model stream a following `effectSpawn` uses as the
 * source for the effect's own `TmdObject`.
 *
 * Storing to a bare `extern` pointer next to pointer-based struct traffic lets
 * GCC 2.8.1's `fixed_scalar_and_varying_struct_p` conclude the two cannot
 * alias, so the scheduler sinks the store past the `TmdObject` loads that
 * follow. Declared as a scalar, `_actor400500SpawnDeathFragments` scores 87.27%
 * (12 register and 8 reorder penalties); as a one-element array it is exact,
 * the same remedy `actor_400600` needed for the same global. */
extern void* D_800678F0[1];

/* The records closing three of the overlay's model streams, selected through
   `D_800678F0`. */
static TmdSource _gActor400500GrayStalkerBurstHead;
static TmdSource _gActor400500GrayStalkerBurstLegRight;
static TmdSource _gActor400500GrayStalkerBurstLegLeft;

extern EnemyParams D_actor_400500_80153C90;
// Message-table callbacks use the argument views required by this TU.

extern TaskMessageEntry D_actor_400500_80153CA0[2];
extern u8               D_actor_400500_80153CC0[];
extern TaskDesc         D_actor_400500_80153D48[];
extern u16              D_actor_400500_80153DB4[];
extern u8               D_actor_400500_80153DD4[];

static void _actor400500UpdateTargetMotion(Task* task);
static void _actor400500DrawLimbShadows(Task* task, s16 worldY, s32 shade);
static s32  _actor400500TryStartAttack(Task* task);
static void _actor400500TickCloakFade(Task* task);
static s32  _actor400500TickTurn(Task* task);
static s32  _actor400500HandleHeavyHitReaction(Task* task);
static s32  _actor400500HandleFallenHitReaction(Task* task);
static void _actor400500TickCrawl(Task* task);
static void _actor400500TickScriptedCrawl(Task* task);
static void _actor400500TickFallenCrawl(Task* task);
static void _actor400500TakeDamage(Task* task);
static void func_actor_400500_80135770(Task* arg0);
static void _actor400500TickCrawlState(Task* task);
static void _actor400500SelectCrawlRoute(Task* task);
static void _actor400500TickCrawlPositiveX(Task* task);
static void _actor400500TickCrawlTurnNegativeX(Task* task);
static void _actor400500TickCrawlNegativeX(Task* task);
static void _actor400500TickCrawlTurnPositiveX(Task* task);
static void _actor400500TickCrawlCornerChoice(Task* task);
static void _actor400500TickCrawlPositiveZ(Task* task);
static void _actor400500TickCrawlTurnNegativeZ(Task* task);
static void _actor400500TickCrawlNegativeZ(Task* task);
static void _actor400500TickCrawlCornerTurnNegativeX(Task* task);
static void _actor400500TickCrawlTurnPositiveZ(Task* task);
static void _actor400500TickCeilingStrikeState(Task* task);
static void _actor400500TickCeilingArmStrike(Task* task);
static void _actor400500TickFloorStrikeState(Task* task);
static void _actor400500TickFloorArmStrike(Task* task);
static void _actor400500RetractFloorStrikeArm(Task* task);
static void _actor400500FinishFloorArmStrike(Task* task);
static void _actor400500TickCeilingFallState(Task* task);
static void _actor400500TickKnockdownState(Task* task);
static void _actor400500StartKnockdownRecoil(Task* task);
static void _actor400500TickAmbushState(Task* task);
static void _actor400500WaitAmbushTrigger(Task* task);
static void _actor400500TickDropState(Task* task);
static void _actor400500TickDropFromCeiling(Task* task);
static void _actor400500FinishDropLanding(Task* task);
static void _actor400500TickJumpState(Task* task);
static void _actor400500TickJumpToCeiling(Task* task);
static void _actor400500TickTurnOverState(Task* task);
static void _actor400500TickTurnOver(Task* task);
static void _actor400500TickStatusHoldState(Task* task);
static void _actor400500StartStatusHoldRecoil(Task* task);
static void func_actor_400500_8013A700(Task* arg0);
static void _actor400500StartDeath(Task* task);
static void _actor400500WaitDeathAnimation(Task* task);
static void func_actor_400500_8013ABE4(Task* arg0);
static void _actor400500TickFallenCrawlState(Task* task);
static void _actor400500SelectFallenCrawlRoute(Task* task);
static void _actor400500TickFallenCrawlPositiveX(Task* task);
static void _actor400500TickFallenCrawlCornerChoice(Task* task);
static void _actor400500UpdateCloakTiming(Task* task);
static void _actor400500TickRoomSequenceState(Task* task);
static void func_actor_400500_8013BA24(Task* arg0);
static void _actor400500StartCrawlDeathFall(Task* task);
static void _actor400500TickCrawlDeathFall(Task* task);
static void _actor400500FinishCrawlDeathLanding(Task* task);
static void _actor400500StartCeilingStrikeDeathFall(Task* task);
static void _actor400500TickCeilingStrikeDeathFall(Task* task);
static void _actor400500FinishCeilingStrikeDeathLanding(Task* task);
static void _actor400500StartCeilingArmStrike(Task* task);
static void _actor400500ExtendCeilingStrikeArm(Task* task);
static void _actor400500StartFloorArmStrike(Task* task);
static void _actor400500ExtendFloorStrikeArm(Task* task);
static void _actor400500StartCeilingFall(Task* task);
static void _actor400500TickCeilingFall(Task* task);
static void _actor400500FinishCeilingFallLanding(Task* task);
static void _actor400500FinishCeilingFallRecovery(Task* task);
static void _actor400500FinishFloorKnockdownRecoil(Task* task);
static void _actor400500FinishFloorKnockdownRecovery(Task* task);
static void _actor400500FinishCeilingKnockdownRecoil(Task* task);
static void _actor400500TickKnockdownCeilingFall(Task* task);
static void _actor400500FinishKnockdownCeilingLanding(Task* task);
static void _actor400500FinishKnockdownCeilingRecovery(Task* task);
static void _actor400500StartAmbush(Task* task);
static void _actor400500StartDropFromCeiling(Task* task);
static void _actor400500StartJumpToCeiling(Task* task);
static void _actor400500FinishJumpToCeiling(Task* task);
static void _actor400500StartTurnOver(Task* task);
static void _actor400500CheckFallenCrawlNegativeXHeading(Task* task);
static void _actor400500TickFallenCrawlFacingNegativeX(Task* task);
static void _actor400500CheckFallenCrawlPositiveXHeading(Task* task);
static void _actor400500TickFallenCrawlFacingPositiveZ(Task* task);
static void _actor400500CheckFallenCrawlNegativeZHeading(Task* task);
static void _actor400500TickFallenCrawlFacingNegativeZ(Task* task);
static void _actor400500CheckFallenCrawlCornerNegativeXHeading(Task* task);
static void _actor400500CheckFallenCrawlPositiveZHeading(Task* task);
static void _actor400500StartRoomSequence(Task* task);
static void _actor400500WaitRoomCrawlCommand(Task* task);
static void _actor400500TickRoomSequenceCrawl(Task* task);
static void _actor400500WaitRoomRevealCommand(Task* task);
static void _actor400500WaitRoomReleaseCommand(Task* task);
static void _actor400500FinishFloorStatusHoldRecoil(Task* task);
static void _actor400500FinishFloorStatusHoldRecovery(Task* task);
static void _actor400500FinishCeilingStatusHoldRecoil(Task* task);
static void _actor400500TickStatusHoldCeilingFall(Task* task);
static void _actor400500FinishStatusHoldCeilingLanding(Task* task);
static void _actor400500FinishStatusHoldCeilingRecovery(Task* task);
static void _actor400500PrepareDeathShrink(Task* task);
static void _actor400500WaitDeathShrinkDelay(Task* task);
static void _actor400500BeginTeardown(Task* task);
static void _actor400500HideBurstBody(Task* task);
static void _actor400500WaitBurstBufferDelay(Task* task);
static void func_actor_400500_8013DA68(Task* arg0);
static void _actor400500BeginBurstTeardown(Task* task);
static void _actor400500EnterState(Task* task, s16 state);
static s32  _actor400500TryTurnOverNearTarget(Task* task);
static void _actor400500SamplePartWorldXZ(Task* task, s16 partIndex, SVECTOR3* worldPosition);
static void _actor400500ResetAnimSlots(Task* task);
static void _actor400500RequestAnimReset(Task* task, s16 setIndex, s16 rateSixteenths);
static void _actor400500BlendAnimSlots(Task* task);
static s16  _actor400500RescaleAnimFrames(Task* task, s16 frames);
static s32  _actor400500CheckAnimBoundary(Task* task);
static void _actor400500CopyRotation(const MATRIX* source, MATRIX* destination);
static void _actor400500TickTeardown(Task* task);
static void _actor400500ClearHitReaction(Task* task);
static void _actor400500KillArmModels(Task* task);
static void _actor400500WaitDestroyDelay(Task* task);

static TmdSource _gActor400500GrayStalkerBody;
void             func_actor_400500_8013DE98(Task*);

static TmdSource _gActor400500GrayStalkerBurstArmRight;
static TmdSource _gActor400500GrayStalkerBurstArmLeft;
static s32       _actor400500ApplyRoomCommand(Task* task, s32 messageId, const ActorCommand* command, s32 unusedPayload);
static void      _actor400500LeftArmTask(Task* task);
static void      _actor400500RightArmTask(Task* task);

static TmdBone _gActor400500GrayStalkerBodySkeleton[18] = {
#include "assets/gray_stalker_body_skeleton.inc"
};

static u32 _gActor400500GrayStalkerBodyPartVerts[18] = {
#include "assets/gray_stalker_body_partVerts.inc"
};

static SVECTOR _gActor400500GrayStalkerBodyVerts[257] = {
#include "assets/gray_stalker_body_verts.inc"
};

static SVECTOR _gActor400500GrayStalkerBodyNormals[249] = {
#include "assets/gray_stalker_body_normals.inc"
};

static u32 _gActor400500GrayStalkerBodyStream[3666] = {
#include "assets/gray_stalker_body_stream.inc"
};

static TmdSource _gActor400500GrayStalkerBody = {
    0,
    36152,
    13736,
    18,
    _gActor400500GrayStalkerBodyPartVerts,
    _gActor400500GrayStalkerBodyVerts,
    _gActor400500GrayStalkerBodyNormals,
    _gActor400500GrayStalkerBodySkeleton,
    _gActor400500GrayStalkerBodyStream,
};

static TmdBone _gActor400500GrayStalkerBurstArmRightSkeleton[1] = {
#include "assets/gray_stalker_burst_arm_right_skeleton.inc"
};

static u32 _gActor400500GrayStalkerBurstArmRightPartVerts[1] = {
#include "assets/gray_stalker_burst_arm_right_partVerts.inc"
};

static SVECTOR _gActor400500GrayStalkerBurstArmRightVerts[10] = {
#include "assets/gray_stalker_burst_arm_right_verts.inc"
};

static SVECTOR _gActor400500GrayStalkerBurstArmRightNormals[12] = {
#include "assets/gray_stalker_burst_arm_right_normals.inc"
};

static u32 _gActor400500GrayStalkerBurstArmRightStream[85] = {
#include "assets/gray_stalker_burst_arm_right_stream.inc"
};

static TmdSource _gActor400500GrayStalkerBurstArmRight = {
    0,
    528,
    0,
    1,
    _gActor400500GrayStalkerBurstArmRightPartVerts,
    _gActor400500GrayStalkerBurstArmRightVerts,
    _gActor400500GrayStalkerBurstArmRightNormals,
    _gActor400500GrayStalkerBurstArmRightSkeleton,
    _gActor400500GrayStalkerBurstArmRightStream,
};

static TmdBone _gActor400500GrayStalkerBurstArmLeftSkeleton[1] = {
#include "assets/gray_stalker_burst_arm_left_skeleton.inc"
};

static u32 _gActor400500GrayStalkerBurstArmLeftPartVerts[1] = {
#include "assets/gray_stalker_burst_arm_left_partVerts.inc"
};

static SVECTOR _gActor400500GrayStalkerBurstArmLeftVerts[10] = {
#include "assets/gray_stalker_burst_arm_left_verts.inc"
};

static SVECTOR _gActor400500GrayStalkerBurstArmLeftNormals[17] = {
#include "assets/gray_stalker_burst_arm_left_normals.inc"
};

static u32 _gActor400500GrayStalkerBurstArmLeftStream[85] = {
#include "assets/gray_stalker_burst_arm_left_stream.inc"
};

static TmdSource _gActor400500GrayStalkerBurstArmLeft = {
    0,
    528,
    0,
    1,
    _gActor400500GrayStalkerBurstArmLeftPartVerts,
    _gActor400500GrayStalkerBurstArmLeftVerts,
    _gActor400500GrayStalkerBurstArmLeftNormals,
    _gActor400500GrayStalkerBurstArmLeftSkeleton,
    _gActor400500GrayStalkerBurstArmLeftStream,
};

static TmdBone _gActor400500GrayStalkerBurstHeadSkeleton[1] = {
#include "assets/gray_stalker_burst_head_skeleton.inc"
};

static u32 _gActor400500GrayStalkerBurstHeadPartVerts[1] = {
#include "assets/gray_stalker_burst_head_partVerts.inc"
};

static SVECTOR _gActor400500GrayStalkerBurstHeadVerts[36] = {
#include "assets/gray_stalker_burst_head_verts.inc"
};

static SVECTOR _gActor400500GrayStalkerBurstHeadNormals[50] = {
#include "assets/gray_stalker_burst_head_normals.inc"
};

static u32 _gActor400500GrayStalkerBurstHeadStream[342] = {
#include "assets/gray_stalker_burst_head_stream.inc"
};

static TmdSource _gActor400500GrayStalkerBurstHead = {
    0,
    2300,
    0,
    1,
    _gActor400500GrayStalkerBurstHeadPartVerts,
    _gActor400500GrayStalkerBurstHeadVerts,
    _gActor400500GrayStalkerBurstHeadNormals,
    _gActor400500GrayStalkerBurstHeadSkeleton,
    _gActor400500GrayStalkerBurstHeadStream,
};

static TmdBone _gActor400500GrayStalkerBurstLegRightSkeleton[1] = {
#include "assets/gray_stalker_burst_leg_right_skeleton.inc"
};

static u32 _gActor400500GrayStalkerBurstLegRightPartVerts[1] = {
#include "assets/gray_stalker_burst_leg_right_partVerts.inc"
};

static SVECTOR _gActor400500GrayStalkerBurstLegRightVerts[25] = {
#include "assets/gray_stalker_burst_leg_right_verts.inc"
};

static SVECTOR _gActor400500GrayStalkerBurstLegRightNormals[33] = {
#include "assets/gray_stalker_burst_leg_right_normals.inc"
};

static u32 _gActor400500GrayStalkerBurstLegRightStream[250] = {
#include "assets/gray_stalker_burst_leg_right_stream.inc"
};

static TmdSource _gActor400500GrayStalkerBurstLegRight = {
    0,
    1644,
    0,
    1,
    _gActor400500GrayStalkerBurstLegRightPartVerts,
    _gActor400500GrayStalkerBurstLegRightVerts,
    _gActor400500GrayStalkerBurstLegRightNormals,
    _gActor400500GrayStalkerBurstLegRightSkeleton,
    _gActor400500GrayStalkerBurstLegRightStream,
};

static TmdBone _gActor400500GrayStalkerBurstLegLeftSkeleton[1] = {
#include "assets/gray_stalker_burst_leg_left_skeleton.inc"
};

static u32 _gActor400500GrayStalkerBurstLegLeftPartVerts[1] = {
#include "assets/gray_stalker_burst_leg_left_partVerts.inc"
};

static SVECTOR _gActor400500GrayStalkerBurstLegLeftVerts[31] = {
#include "assets/gray_stalker_burst_leg_left_verts.inc"
};

static SVECTOR _gActor400500GrayStalkerBurstLegLeftNormals[39] = {
#include "assets/gray_stalker_burst_leg_left_normals.inc"
};

static u32 _gActor400500GrayStalkerBurstLegLeftStream[282] = {
#include "assets/gray_stalker_burst_leg_left_stream.inc"
};

static TmdSource _gActor400500GrayStalkerBurstLegLeft = {
    0,
    1900,
    0,
    1,
    _gActor400500GrayStalkerBurstLegLeftPartVerts,
    _gActor400500GrayStalkerBurstLegLeftVerts,
    _gActor400500GrayStalkerBurstLegLeftNormals,
    _gActor400500GrayStalkerBurstLegLeftSkeleton,
    _gActor400500GrayStalkerBurstLegLeftStream,
};

static AnimationPackedPose _gActor400500Animation134F0Bank1[26] = {
#include "assets/actor_400500_animation_134F0_bank1.inc"
};

static AnimationPackedRotation _gActor400500Animation134F0Bank4[323] = {
#include "assets/actor_400500_animation_134F0_bank4.inc"
};

static AnimationRecord _gActor400500Animation134F0Records[408] = {
#include "assets/actor_400500_animation_134F0_records.inc"
};

static u16 _gActor400500Animation134F0Indices[18] = {
#include "assets/actor_400500_animation_134F0_indices.inc"
};

static AnimationSet _gActor400500Animation134F0 = {
    _gActor400500Animation134F0Records,
    _gActor400500Animation134F0Indices,
    { NULL, _gActor400500Animation134F0Bank1, NULL, NULL, _gActor400500Animation134F0Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor400500Animation13C28Bank1[22] = {
#include "assets/actor_400500_animation_13C28_bank1.inc"
};

static AnimationPackedRotation _gActor400500Animation13C28Bank4[156] = {
#include "assets/actor_400500_animation_13C28_bank4.inc"
};

static AnimationRecord _gActor400500Animation13C28Records[221] = {
#include "assets/actor_400500_animation_13C28_records.inc"
};

static u16 _gActor400500Animation13C28Indices[18] = {
#include "assets/actor_400500_animation_13C28_indices.inc"
};

static AnimationSet _gActor400500Animation13C28 = {
    _gActor400500Animation13C28Records,
    _gActor400500Animation13C28Indices,
    { NULL, _gActor400500Animation13C28Bank1, NULL, NULL, _gActor400500Animation13C28Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor400500Animation1430CBank1[14] = {
#include "assets/actor_400500_animation_1430C_bank1.inc"
};

static AnimationPackedRotation _gActor400500Animation1430CBank4[157] = {
#include "assets/actor_400500_animation_1430C_bank4.inc"
};

static AnimationRecord _gActor400500Animation1430CRecords[223] = {
#include "assets/actor_400500_animation_1430C_records.inc"
};

static u16 _gActor400500Animation1430CIndices[18] = {
#include "assets/actor_400500_animation_1430C_indices.inc"
};

static AnimationSet _gActor400500Animation1430C = {
    _gActor400500Animation1430CRecords,
    _gActor400500Animation1430CIndices,
    { NULL, _gActor400500Animation1430CBank1, NULL, NULL, _gActor400500Animation1430CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor400500Animation14BA4Bank1[25] = {
#include "assets/actor_400500_animation_14BA4_bank1.inc"
};

static AnimationPackedRotation _gActor400500Animation14BA4Bank4[191] = {
#include "assets/actor_400500_animation_14BA4_bank4.inc"
};

static AnimationRecord _gActor400500Animation14BA4Records[265] = {
#include "assets/actor_400500_animation_14BA4_records.inc"
};

static u16 _gActor400500Animation14BA4Indices[18] = {
#include "assets/actor_400500_animation_14BA4_indices.inc"
};

static AnimationSet _gActor400500Animation14BA4 = {
    _gActor400500Animation14BA4Records,
    _gActor400500Animation14BA4Indices,
    { NULL, _gActor400500Animation14BA4Bank1, NULL, NULL, _gActor400500Animation14BA4Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor400500Animation15A60Bank1[26] = {
#include "assets/actor_400500_animation_15A60_bank1.inc"
};

static AnimationPackedRotation _gActor400500Animation15A60Bank4[380] = {
#include "assets/actor_400500_animation_15A60_bank4.inc"
};

static AnimationRecord _gActor400500Animation15A60Records[466] = {
#include "assets/actor_400500_animation_15A60_records.inc"
};

static u16 _gActor400500Animation15A60Indices[18] = {
#include "assets/actor_400500_animation_15A60_indices.inc"
};

static AnimationSet _gActor400500Animation15A60 = {
    _gActor400500Animation15A60Records,
    _gActor400500Animation15A60Indices,
    { NULL, _gActor400500Animation15A60Bank1, NULL, NULL, _gActor400500Animation15A60Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor400500Animation168E0Bank1[30] = {
#include "assets/actor_400500_animation_168E0_bank1.inc"
};

static AnimationPackedRotation _gActor400500Animation168E0Bank4[371] = {
#include "assets/actor_400500_animation_168E0_bank4.inc"
};

static AnimationRecord _gActor400500Animation168E0Records[448] = {
#include "assets/actor_400500_animation_168E0_records.inc"
};

static u16 _gActor400500Animation168E0Indices[18] = {
#include "assets/actor_400500_animation_168E0_indices.inc"
};

static AnimationSet _gActor400500Animation168E0 = {
    _gActor400500Animation168E0Records,
    _gActor400500Animation168E0Indices,
    { NULL, _gActor400500Animation168E0Bank1, NULL, NULL, _gActor400500Animation168E0Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor400500Animation171A0Bank1[17] = {
#include "assets/actor_400500_animation_171A0_bank1.inc"
};

static AnimationPackedRotation _gActor400500Animation171A0Bank4[214] = {
#include "assets/actor_400500_animation_171A0_bank4.inc"
};

static AnimationRecord _gActor400500Animation171A0Records[276] = {
#include "assets/actor_400500_animation_171A0_records.inc"
};

static u16 _gActor400500Animation171A0Indices[18] = {
#include "assets/actor_400500_animation_171A0_indices.inc"
};

static AnimationSet _gActor400500Animation171A0 = {
    _gActor400500Animation171A0Records,
    _gActor400500Animation171A0Indices,
    { NULL, _gActor400500Animation171A0Bank1, NULL, NULL, _gActor400500Animation171A0Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor400500Animation17B18Bank1[19] = {
#include "assets/actor_400500_animation_17B18_bank1.inc"
};

static AnimationPackedRotation _gActor400500Animation17B18Bank4[233] = {
#include "assets/actor_400500_animation_17B18_bank4.inc"
};

static AnimationRecord _gActor400500Animation17B18Records[297] = {
#include "assets/actor_400500_animation_17B18_records.inc"
};

static u16 _gActor400500Animation17B18Indices[18] = {
#include "assets/actor_400500_animation_17B18_indices.inc"
};

static AnimationSet _gActor400500Animation17B18 = {
    _gActor400500Animation17B18Records,
    _gActor400500Animation17B18Indices,
    { NULL, _gActor400500Animation17B18Bank1, NULL, NULL, _gActor400500Animation17B18Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor400500Animation17FECBank1[9] = {
#include "assets/actor_400500_animation_17FEC_bank1.inc"
};

static AnimationPackedRotation _gActor400500Animation17FECBank4[114] = {
#include "assets/actor_400500_animation_17FEC_bank4.inc"
};

static AnimationRecord _gActor400500Animation17FECRecords[149] = {
#include "assets/actor_400500_animation_17FEC_records.inc"
};

static u16 _gActor400500Animation17FECIndices[18] = {
#include "assets/actor_400500_animation_17FEC_indices.inc"
};

static AnimationSet _gActor400500Animation17FEC = {
    _gActor400500Animation17FECRecords,
    _gActor400500Animation17FECIndices,
    { NULL, _gActor400500Animation17FECBank1, NULL, NULL, _gActor400500Animation17FECBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor400500Animation18BCCBank1[23] = {
#include "assets/actor_400500_animation_18BCC_bank1.inc"
};

static AnimationPackedRotation _gActor400500Animation18BCCBank4[295] = {
#include "assets/actor_400500_animation_18BCC_bank4.inc"
};

static AnimationRecord _gActor400500Animation18BCCRecords[377] = {
#include "assets/actor_400500_animation_18BCC_records.inc"
};

static u16 _gActor400500Animation18BCCIndices[18] = {
#include "assets/actor_400500_animation_18BCC_indices.inc"
};

static AnimationSet _gActor400500Animation18BCC = {
    _gActor400500Animation18BCCRecords,
    _gActor400500Animation18BCCIndices,
    { NULL, _gActor400500Animation18BCCBank1, NULL, NULL, _gActor400500Animation18BCCBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor400500Animation19124Bank1[9] = {
#include "assets/actor_400500_animation_19124_bank1.inc"
};

static AnimationPackedRotation _gActor400500Animation19124Bank4[125] = {
#include "assets/actor_400500_animation_19124_bank4.inc"
};

static AnimationRecord _gActor400500Animation19124Records[171] = {
#include "assets/actor_400500_animation_19124_records.inc"
};

static u16 _gActor400500Animation19124Indices[18] = {
#include "assets/actor_400500_animation_19124_indices.inc"
};

static AnimationSet _gActor400500Animation19124 = {
    _gActor400500Animation19124Records,
    _gActor400500Animation19124Indices,
    { NULL, _gActor400500Animation19124Bank1, NULL, NULL, _gActor400500Animation19124Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor400500Animation19AE4Bank1[18] = {
#include "assets/actor_400500_animation_19AE4_bank1.inc"
};

static AnimationPackedRotation _gActor400500Animation19AE4Bank4[242] = {
#include "assets/actor_400500_animation_19AE4_bank4.inc"
};

static AnimationRecord _gActor400500Animation19AE4Records[309] = {
#include "assets/actor_400500_animation_19AE4_records.inc"
};

static u16 _gActor400500Animation19AE4Indices[18] = {
#include "assets/actor_400500_animation_19AE4_indices.inc"
};

static AnimationSet _gActor400500Animation19AE4 = {
    _gActor400500Animation19AE4Records,
    _gActor400500Animation19AE4Indices,
    { NULL, _gActor400500Animation19AE4Bank1, NULL, NULL, _gActor400500Animation19AE4Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor400500Animation19F10Bank1[9] = {
#include "assets/actor_400500_animation_19F10_bank1.inc"
};

static AnimationPackedRotation _gActor400500Animation19F10Bank4[95] = {
#include "assets/actor_400500_animation_19F10_bank4.inc"
};

static AnimationRecord _gActor400500Animation19F10Records[126] = {
#include "assets/actor_400500_animation_19F10_records.inc"
};

static u16 _gActor400500Animation19F10Indices[18] = {
#include "assets/actor_400500_animation_19F10_indices.inc"
};

static AnimationSet _gActor400500Animation19F10 = {
    _gActor400500Animation19F10Records,
    _gActor400500Animation19F10Indices,
    { NULL, _gActor400500Animation19F10Bank1, NULL, NULL, _gActor400500Animation19F10Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor400500Animation1A47CBank1[9] = {
#include "assets/actor_400500_animation_1A47C_bank1.inc"
};

static AnimationPackedRotation _gActor400500Animation1A47CBank4[136] = {
#include "assets/actor_400500_animation_1A47C_bank4.inc"
};

static AnimationRecord _gActor400500Animation1A47CRecords[165] = {
#include "assets/actor_400500_animation_1A47C_records.inc"
};

static u16 _gActor400500Animation1A47CIndices[18] = {
#include "assets/actor_400500_animation_1A47C_indices.inc"
};

static AnimationSet _gActor400500Animation1A47C = {
    _gActor400500Animation1A47CRecords,
    _gActor400500Animation1A47CIndices,
    { NULL, _gActor400500Animation1A47CBank1, NULL, NULL, _gActor400500Animation1A47CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor400500Animation1AE34Bank1[15] = {
#include "assets/actor_400500_animation_1AE34_bank1.inc"
};

static AnimationPackedRotation _gActor400500Animation1AE34Bank4[227] = {
#include "assets/actor_400500_animation_1AE34_bank4.inc"
};

static AnimationRecord _gActor400500Animation1AE34Records[331] = {
#include "assets/actor_400500_animation_1AE34_records.inc"
};

static u16 _gActor400500Animation1AE34Indices[18] = {
#include "assets/actor_400500_animation_1AE34_indices.inc"
};

static AnimationSet _gActor400500Animation1AE34 = {
    _gActor400500Animation1AE34Records,
    _gActor400500Animation1AE34Indices,
    { NULL, _gActor400500Animation1AE34Bank1, NULL, NULL, _gActor400500Animation1AE34Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor400500Animation1B3A0Bank1[10] = {
#include "assets/actor_400500_animation_1B3A0_bank1.inc"
};

static AnimationPackedRotation _gActor400500Animation1B3A0Bank4[129] = {
#include "assets/actor_400500_animation_1B3A0_bank4.inc"
};

static AnimationRecord _gActor400500Animation1B3A0Records[169] = {
#include "assets/actor_400500_animation_1B3A0_records.inc"
};

static u16 _gActor400500Animation1B3A0Indices[18] = {
#include "assets/actor_400500_animation_1B3A0_indices.inc"
};

static AnimationSet _gActor400500Animation1B3A0 = {
    _gActor400500Animation1B3A0Records,
    _gActor400500Animation1B3A0Indices,
    { NULL, _gActor400500Animation1B3A0Bank1, NULL, NULL, _gActor400500Animation1B3A0Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor400500Animation1C12CBank1[16] = {
#include "assets/actor_400500_animation_1C12C_bank1.inc"
};

static AnimationPackedRotation _gActor400500Animation1C12CBank4[363] = {
#include "assets/actor_400500_animation_1C12C_bank4.inc"
};

static AnimationRecord _gActor400500Animation1C12CRecords[437] = {
#include "assets/actor_400500_animation_1C12C_records.inc"
};

static u16 _gActor400500Animation1C12CIndices[18] = {
#include "assets/actor_400500_animation_1C12C_indices.inc"
};

static AnimationSet _gActor400500Animation1C12C = {
    _gActor400500Animation1C12CRecords,
    _gActor400500Animation1C12CIndices,
    { NULL, _gActor400500Animation1C12CBank1, NULL, NULL, _gActor400500Animation1C12CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor400500Animation1C7BCBank1[12] = {
#include "assets/actor_400500_animation_1C7BC_bank1.inc"
};

static AnimationPackedRotation _gActor400500Animation1C7BCBank4[167] = {
#include "assets/actor_400500_animation_1C7BC_bank4.inc"
};

static AnimationRecord _gActor400500Animation1C7BCRecords[198] = {
#include "assets/actor_400500_animation_1C7BC_records.inc"
};

static u16 _gActor400500Animation1C7BCIndices[18] = {
#include "assets/actor_400500_animation_1C7BC_indices.inc"
};

static AnimationSet _gActor400500Animation1C7BC = {
    _gActor400500Animation1C7BCRecords,
    _gActor400500Animation1C7BCIndices,
    { NULL, _gActor400500Animation1C7BCBank1, NULL, NULL, _gActor400500Animation1C7BCBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor400500Animation1CB44Bank1[5] = {
#include "assets/actor_400500_animation_1CB44_bank1.inc"
};

static AnimationPackedRotation _gActor400500Animation1CB44Bank4[79] = {
#include "assets/actor_400500_animation_1CB44_bank4.inc"
};

static AnimationRecord _gActor400500Animation1CB44Records[113] = {
#include "assets/actor_400500_animation_1CB44_records.inc"
};

static u16 _gActor400500Animation1CB44Indices[18] = {
#include "assets/actor_400500_animation_1CB44_indices.inc"
};

static AnimationSet _gActor400500Animation1CB44 = {
    _gActor400500Animation1CB44Records,
    _gActor400500Animation1CB44Indices,
    { NULL, _gActor400500Animation1CB44Bank1, NULL, NULL, _gActor400500Animation1CB44Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor400500Animation1D0E4Bank1[11] = {
#include "assets/actor_400500_animation_1D0E4_bank1.inc"
};

static AnimationPackedRotation _gActor400500Animation1D0E4Bank4[139] = {
#include "assets/actor_400500_animation_1D0E4_bank4.inc"
};

static AnimationRecord _gActor400500Animation1D0E4Records[169] = {
#include "assets/actor_400500_animation_1D0E4_records.inc"
};

static u16 _gActor400500Animation1D0E4Indices[18] = {
#include "assets/actor_400500_animation_1D0E4_indices.inc"
};

static AnimationSet _gActor400500Animation1D0E4 = {
    _gActor400500Animation1D0E4Records,
    _gActor400500Animation1D0E4Indices,
    { NULL, _gActor400500Animation1D0E4Bank1, NULL, NULL, _gActor400500Animation1D0E4Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor400500Animation1D560Bank1[10] = {
#include "assets/actor_400500_animation_1D560_bank1.inc"
};

static AnimationPackedRotation _gActor400500Animation1D560Bank4[104] = {
#include "assets/actor_400500_animation_1D560_bank4.inc"
};

static AnimationRecord _gActor400500Animation1D560Records[134] = {
#include "assets/actor_400500_animation_1D560_records.inc"
};

static u16 _gActor400500Animation1D560Indices[18] = {
#include "assets/actor_400500_animation_1D560_indices.inc"
};

static AnimationSet _gActor400500Animation1D560 = {
    _gActor400500Animation1D560Records,
    _gActor400500Animation1D560Indices,
    { NULL, _gActor400500Animation1D560Bank1, NULL, NULL, _gActor400500Animation1D560Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor400500Animation1E0CCBank1[40] = {
#include "assets/actor_400500_animation_1E0CC_bank1.inc"
};

static AnimationPackedRotation _gActor400500Animation1E0CCBank4[266] = {
#include "assets/actor_400500_animation_1E0CC_bank4.inc"
};

static AnimationRecord _gActor400500Animation1E0CCRecords[326] = {
#include "assets/actor_400500_animation_1E0CC_records.inc"
};

static u16 _gActor400500Animation1E0CCIndices[18] = {
#include "assets/actor_400500_animation_1E0CC_indices.inc"
};

static AnimationSet _gActor400500Animation1E0CC = {
    _gActor400500Animation1E0CCRecords,
    _gActor400500Animation1E0CCIndices,
    { NULL, _gActor400500Animation1E0CCBank1, NULL, NULL, _gActor400500Animation1E0CCBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor400500Animation1E718Bank1[20] = {
#include "assets/actor_400500_animation_1E718_bank1.inc"
};

static AnimationPackedRotation _gActor400500Animation1E718Bank4[133] = {
#include "assets/actor_400500_animation_1E718_bank4.inc"
};

static AnimationRecord _gActor400500Animation1E718Records[191] = {
#include "assets/actor_400500_animation_1E718_records.inc"
};

static u16 _gActor400500Animation1E718Indices[18] = {
#include "assets/actor_400500_animation_1E718_indices.inc"
};

static AnimationSet _gActor400500Animation1E718 = {
    _gActor400500Animation1E718Records,
    _gActor400500Animation1E718Indices,
    { NULL, _gActor400500Animation1E718Bank1, NULL, NULL, _gActor400500Animation1E718Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor400500Animation1ECA4Bank1[16] = {
#include "assets/actor_400500_animation_1ECA4_bank1.inc"
};

static AnimationPackedRotation _gActor400500Animation1ECA4Bank4[117] = {
#include "assets/actor_400500_animation_1ECA4_bank4.inc"
};

static AnimationRecord _gActor400500Animation1ECA4Records[171] = {
#include "assets/actor_400500_animation_1ECA4_records.inc"
};

static u16 _gActor400500Animation1ECA4Indices[18] = {
#include "assets/actor_400500_animation_1ECA4_indices.inc"
};

static AnimationSet _gActor400500Animation1ECA4 = {
    _gActor400500Animation1ECA4Records,
    _gActor400500Animation1ECA4Indices,
    { NULL, _gActor400500Animation1ECA4Bank1, NULL, NULL, _gActor400500Animation1ECA4Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor400500Animation1F010Bank1[6] = {
#include "assets/actor_400500_animation_1F010_bank1.inc"
};

static AnimationPackedRotation _gActor400500Animation1F010Bank4[78] = {
#include "assets/actor_400500_animation_1F010_bank4.inc"
};

static AnimationRecord _gActor400500Animation1F010Records[104] = {
#include "assets/actor_400500_animation_1F010_records.inc"
};

static u16 _gActor400500Animation1F010Indices[18] = {
#include "assets/actor_400500_animation_1F010_indices.inc"
};

static AnimationSet _gActor400500Animation1F010 = {
    _gActor400500Animation1F010Records,
    _gActor400500Animation1F010Indices,
    { NULL, _gActor400500Animation1F010Bank1, NULL, NULL, _gActor400500Animation1F010Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor400500Animation1F510Bank1[11] = {
#include "assets/actor_400500_animation_1F510_bank1.inc"
};

static AnimationPackedRotation _gActor400500Animation1F510Bank4[113] = {
#include "assets/actor_400500_animation_1F510_bank4.inc"
};

static AnimationRecord _gActor400500Animation1F510Records[155] = {
#include "assets/actor_400500_animation_1F510_records.inc"
};

static u16 _gActor400500Animation1F510Indices[18] = {
#include "assets/actor_400500_animation_1F510_indices.inc"
};

static AnimationSet _gActor400500Animation1F510 = {
    _gActor400500Animation1F510Records,
    _gActor400500Animation1F510Indices,
    { NULL, _gActor400500Animation1F510Bank1, NULL, NULL, _gActor400500Animation1F510Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor400500Animation1FB0CBank1[10] = {
#include "assets/actor_400500_animation_1FB0C_bank1.inc"
};

static AnimationPackedRotation _gActor400500Animation1FB0CBank4[150] = {
#include "assets/actor_400500_animation_1FB0C_bank4.inc"
};

static AnimationRecord _gActor400500Animation1FB0CRecords[184] = {
#include "assets/actor_400500_animation_1FB0C_records.inc"
};

static u16 _gActor400500Animation1FB0CIndices[18] = {
#include "assets/actor_400500_animation_1FB0C_indices.inc"
};

static AnimationSet _gActor400500Animation1FB0C = {
    _gActor400500Animation1FB0CRecords,
    _gActor400500Animation1FB0CIndices,
    { NULL, _gActor400500Animation1FB0CBank1, NULL, NULL, _gActor400500Animation1FB0CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor400500Animation1FCD0Bank1[2] = {
#include "assets/actor_400500_animation_1FCD0_bank1.inc"
};

static AnimationPackedRotation _gActor400500Animation1FCD0Bank4[16] = {
#include "assets/actor_400500_animation_1FCD0_bank4.inc"
};

static AnimationRecord _gActor400500Animation1FCD0Records[72] = {
#include "assets/actor_400500_animation_1FCD0_records.inc"
};

static u16 _gActor400500Animation1FCD0Indices[18] = {
#include "assets/actor_400500_animation_1FCD0_indices.inc"
};

static AnimationSet _gActor400500Animation1FCD0 = {
    _gActor400500Animation1FCD0Records,
    _gActor400500Animation1FCD0Indices,
    { NULL, _gActor400500Animation1FCD0Bank1, NULL, NULL, _gActor400500Animation1FCD0Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor400500Animation1FE94Bank1[2] = {
#include "assets/actor_400500_animation_1FE94_bank1.inc"
};

static AnimationPackedRotation _gActor400500Animation1FE94Bank4[16] = {
#include "assets/actor_400500_animation_1FE94_bank4.inc"
};

static AnimationRecord _gActor400500Animation1FE94Records[72] = {
#include "assets/actor_400500_animation_1FE94_records.inc"
};

static u16 _gActor400500Animation1FE94Indices[18] = {
#include "assets/actor_400500_animation_1FE94_indices.inc"
};

static AnimationSet _gActor400500Animation1FE94 = {
    _gActor400500Animation1FE94Records,
    _gActor400500Animation1FE94Indices,
    { NULL, _gActor400500Animation1FE94Bank1, NULL, NULL, _gActor400500Animation1FE94Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor400500Animation20058Bank1[2] = {
#include "assets/actor_400500_animation_20058_bank1.inc"
};

static AnimationPackedRotation _gActor400500Animation20058Bank4[16] = {
#include "assets/actor_400500_animation_20058_bank4.inc"
};

static AnimationRecord _gActor400500Animation20058Records[72] = {
#include "assets/actor_400500_animation_20058_records.inc"
};

static u16 _gActor400500Animation20058Indices[18] = {
#include "assets/actor_400500_animation_20058_indices.inc"
};

static AnimationSet _gActor400500Animation20058 = {
    _gActor400500Animation20058Records,
    _gActor400500Animation20058Indices,
    { NULL, _gActor400500Animation20058Bank1, NULL, NULL, _gActor400500Animation20058Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor400500Animation2021CBank1[2] = {
#include "assets/actor_400500_animation_2021C_bank1.inc"
};

static AnimationPackedRotation _gActor400500Animation2021CBank4[16] = {
#include "assets/actor_400500_animation_2021C_bank4.inc"
};

static AnimationRecord _gActor400500Animation2021CRecords[72] = {
#include "assets/actor_400500_animation_2021C_records.inc"
};

static u16 _gActor400500Animation2021CIndices[18] = {
#include "assets/actor_400500_animation_2021C_indices.inc"
};

static AnimationSet _gActor400500Animation2021C = {
    _gActor400500Animation2021CRecords,
    _gActor400500Animation2021CIndices,
    { NULL, _gActor400500Animation2021CBank1, NULL, NULL, _gActor400500Animation2021CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor400500Animation206C4Bank1[8] = {
#include "assets/actor_400500_animation_206C4_bank1.inc"
};

static AnimationPackedRotation _gActor400500Animation206C4Bank4[105] = {
#include "assets/actor_400500_animation_206C4_bank4.inc"
};

static AnimationRecord _gActor400500Animation206C4Records[150] = {
#include "assets/actor_400500_animation_206C4_records.inc"
};

static u16 _gActor400500Animation206C4Indices[18] = {
#include "assets/actor_400500_animation_206C4_indices.inc"
};

static AnimationSet _gActor400500Animation206C4 = {
    _gActor400500Animation206C4Records,
    _gActor400500Animation206C4Indices,
    { NULL, _gActor400500Animation206C4Bank1, NULL, NULL, _gActor400500Animation206C4Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor400500Animation2141CBank1[25] = {
#include "assets/actor_400500_animation_2141C_bank1.inc"
};

static AnimationPackedRotation _gActor400500Animation2141CBank4[343] = {
#include "assets/actor_400500_animation_2141C_bank4.inc"
};

static AnimationRecord _gActor400500Animation2141CRecords[416] = {
#include "assets/actor_400500_animation_2141C_records.inc"
};

static u16 _gActor400500Animation2141CIndices[20] = {
#include "assets/actor_400500_animation_2141C_indices.inc"
};

static AnimationSet _gActor400500Animation2141C = {
    _gActor400500Animation2141CRecords,
    _gActor400500Animation2141CIndices,
    { NULL, _gActor400500Animation2141CBank1, NULL, NULL, _gActor400500Animation2141CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor400500Animation21C64Bank1[15] = {
#include "assets/actor_400500_animation_21C64_bank1.inc"
};

static AnimationPackedRotation _gActor400500Animation21C64Bank4[203] = {
#include "assets/actor_400500_animation_21C64_bank4.inc"
};

static AnimationRecord _gActor400500Animation21C64Records[262] = {
#include "assets/actor_400500_animation_21C64_records.inc"
};

static u16 _gActor400500Animation21C64Indices[20] = {
#include "assets/actor_400500_animation_21C64_indices.inc"
};

static AnimationSet _gActor400500Animation21C64 = {
    _gActor400500Animation21C64Records,
    _gActor400500Animation21C64Indices,
    { NULL, _gActor400500Animation21C64Bank1, NULL, NULL, _gActor400500Animation21C64Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor400500Animation21E3CBank1[2] = {
#include "assets/actor_400500_animation_21E3C_bank1.inc"
};

static AnimationPackedRotation _gActor400500Animation21E3CBank4[16] = {
#include "assets/actor_400500_animation_21E3C_bank4.inc"
};

static AnimationRecord _gActor400500Animation21E3CRecords[76] = {
#include "assets/actor_400500_animation_21E3C_records.inc"
};

static u16 _gActor400500Animation21E3CIndices[20] = {
#include "assets/actor_400500_animation_21E3C_indices.inc"
};

static AnimationSet _gActor400500Animation21E3C = {
    _gActor400500Animation21E3CRecords,
    _gActor400500Animation21E3CIndices,
    { NULL, _gActor400500Animation21E3CBank1, NULL, NULL, _gActor400500Animation21E3CBank4, NULL, NULL, NULL },
};

DamageAttack D_actor_400500_80153C84[3] = {
    { 28, 7 },
    { 35, 10 },
    { 8, 7 },
};

EnemyParams D_actor_400500_80153C90 = { D_actor_400500_80153C84, 450, 500, 200, 15, 100, 8, 100, 10 };

TaskMessageEntry D_actor_400500_80153CA0[2] = {
    { ACTOR_COMMAND_MESSAGE_APPLY, _actor400500ApplyRoomCommand },
    { TASK_MESSAGE_TABLE_END, NULL },
};

/// Borrowed player animation table; entry zero is unused.
static AnimationSet* _gActor400500PlayerAnimationSets[4] = {
    NULL,
    &_gActor400500Animation2141C,
    &_gActor400500Animation21C64,
    &_gActor400500Animation21E3C,
};

u8 D_actor_400500_80153CC0[136] = {
    0,
    0,
    0,
    0,
    16,
    83,
    20,
    128,
    72,
    90,
    20,
    128,
    44,
    97,
    20,
    128,
    196,
    105,
    20,
    128,
    128,
    120,
    20,
    128,
    0,
    135,
    20,
    128,
    192,
    143,
    20,
    128,
    56,
    153,
    20,
    128,
    12,
    158,
    20,
    128,
    236,
    169,
    20,
    128,
    68,
    175,
    20,
    128,
    4,
    185,
    20,
    128,
    48,
    189,
    20,
    128,
    156,
    194,
    20,
    128,
    84,
    204,
    20,
    128,
    192,
    209,
    20,
    128,
    76,
    223,
    20,
    128,
    220,
    229,
    20,
    128,
    100,
    233,
    20,
    128,
    4,
    239,
    20,
    128,
    128,
    243,
    20,
    128,
    236,
    254,
    20,
    128,
    56,
    5,
    21,
    128,
    196,
    10,
    21,
    128,
    48,
    14,
    21,
    128,
    48,
    19,
    21,
    128,
    44,
    25,
    21,
    128,
    240,
    26,
    21,
    128,
    180,
    28,
    21,
    128,
    120,
    30,
    21,
    128,
    60,
    32,
    21,
    128,
    228,
    36,
    21,
    128,
    0,
    0,
    0,
    0,
};

TaskDesc D_actor_400500_80153D48[2] = {
    { { { TASK_BODY_TMD, 96 } }, _actor400500LeftArmTask, { .model = &_gActor400500GrayStalkerBurstArmLeft } },
    { { { TASK_BODY_TMD, 96 } }, _actor400500RightArmTask, { .model = &_gActor400500GrayStalkerBurstArmRight } },
};

TaskDesc D_actor_400500_80153D60 = { { { TASK_BODY_TMD, 96 } }, func_actor_400500_8013DE98, { .model = &_gActor400500GrayStalkerBody } };

ActorZone D_actor_400500_80153D6C[7] = {
    { -1700, -10200, 1700, 3200, 4 },
    { 0, -10200, 15300, 3200, 1 },
    { 15000, -10200, 3000, 3200, 2 },
    { 15000, -7200, 3000, 5200, 3 },
    { 4200, -11800, 2200, 1600, 5 },
    { 15000, -2000, 3000, 1800, 6 },
    { 0, 0, 0, 0, ACTOR_ZONE_END },
};

u16 D_actor_400500_80153DB4[16] = {
    0,
    0,
    0,
    0,
    0,
    0,
    32,
    48,
    64,
    80,
    96,
    80,
    64,
    48,
    0,
    0,
};

u8 D_actor_400500_80153DD4[33] = { 26, 26, 26, 27, 27, 15, 15, 26, 15, 26, 26, 27, 27, 15, 15, 26, 26, 27, 27, 30, 27, 26, 26, 26, 26, 26, 15, 15, 15, 15, 15, 15, 15 };

static void           _actor400500InitCollisionBodies(Task* task);
static void           _actor400500SpawnArmModels(Task* task);
static void           _actor400500PlaceAtArrival(Task* task);
static inline void    _actor400500SetState(Task* task, s32 state, s32 subState);
static inline void    _actor400500PlayPlacedSound(Task* task, s32 baseSoundId);
static void           _actor400500PositionGrabbedPlayer(Task* task, s32 easePosition);
static void           _actor400500SpawnDeathFragments(Task* task);
static void           _actor400500InitTask(Task* task);
static inline s32     _actor400500FindRouteZone(Task* task);
static inline VECTOR* _actor400500PushCachedPosition(const GfxCoord* coord);
static inline void    _scratchStackReleaseBytes(s32 byteCount);
static __inline__ u8* push_proj(void);
static void           _actor400500StartGrab(Task* task);
static inline void    _actor400500RequestCloakFade(Task* task, s32 cloakRequest);
static inline s32     _actor400500AccumulateWorldRotation(const GfxCoord* coord, MATRIX* worldRotation);
static inline void    _actor400500TurnPartWorldYaw(GfxCoord* part, u16 yawDelta);
static inline s16     _actor400500PlayerDistance(GfxCoord* part);
static inline void    _actor400500RequestAnimBlend(Task* task, s32 setIndex);
static void           func_actor_400500_8013771C(Task* arg0);
static inline void    _actor400500UpdateColor(Task* task, const GfxCoord* coord, const TmdObject* model);

/// Grab-release clip and positioning policies of the captured-player helpers.
///
/// Clip IDs index the Stalker's animation table; positioning tests the signed
/// low halfword of the policy and uses quarter-step easing for a nonzero value.
enum { ACTOR_400500_ANIM_GRAB_MISS     = 6,
       ACTOR_400500_GRAB_POSITION_SNAP = 0,
       ACTOR_400500_GRAB_POSITION_EASE = 1 };

/// Model-coordinate parents of the Stalker's arm spheres and attached strike models.
enum { ACTOR_400500_PART_RIGHT_ARM = 7,
       ACTOR_400500_PART_LEFT_ARM  = 10 };

/// Links the Stalker's body sphere and four initially disabled arm attack spheres.
///
/// Requires zeroed live work, a populated 18-coordinate model and live enemy
/// attack parameters. Positions/radii are part-local game units. The body
/// uses three contacts; each arm's two spheres share one contact record and
/// attack row zero. Linked spheres and contact arrays remain borrowed until
/// task teardown unlinks them. Reinitializing a linked work block is invalid.
static void _actor400500InitCollisionBodies(Task* task)
{
    // Captures task; sphere is a stable body lvalue and contactArray a complete array.
    // Arguments are evaluated repeatedly and must have no side effects. Links a
    // disabled sphere and reinitializes its pair's shared contacts intentionally.
    // Expands a braced statement block and is undefined after its four uses.
#define ACTOR_400500_INIT_ARM_ATTACK_SPHERE(sphere, contactArray, partIndex, localX, localRadius) \
    {                                                                                             \
        (sphere).key              = damagePackEnemyAttackKey(task->spawnArg2.pointer, 0);         \
        (sphere).coord            = &task->extra.tmd->coords[(partIndex)];                        \
        (sphere).context.contacts = (contactArray);                                               \
        (sphere).pos.vx           = (localX);                                                     \
        (sphere).pos.vy           = 0;                                                            \
        (sphere).pos.vz           = 0;                                                            \
        (sphere).radius           = (localRadius);                                                \
        (sphere).flags            = WORLD_COLLISION_BODY_SPHERE;                                  \
        worldCollisionLinkBody(WORLD_COLLISION_LIST_ENEMY_ATTACKS, &(sphere));                    \
        worldCollisionInitContacts((contactArray), ARRAY_SIZE(contactArray), 0);                  \
        (sphere).flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);  \
    }
    enum { ACTOR_400500_BODY_ID = 5 };
    _Actor400500GrayStalkerWork* work;

    work = task->work;

    // Enable the receiving body independently of the dormant arm attacks.
    work->body.coord            = &task->extra.tmd->coords[ACTOR_400500_PART_BODY];
    work->body.context.contacts = work->bodyContacts;
    work->body.pos.vz           = 0x110;
    work->body.pos.vx           = 0;
    work->body.pos.vy           = 0;
    work->body.key              = WORLD_COLLISION_CONTACT_ENEMY_BODY | ACTOR_400500_BODY_ID;
    work->body.radius           = 0x260;
    work->body.flags            = WORLD_COLLISION_BODY_SPHERE;
    worldCollisionLinkBody(WORLD_COLLISION_LIST_ENEMY_BODIES, &work->body);
    worldCollisionInitContacts(work->bodyContacts, ARRAY_SIZE(work->bodyContacts), 0);
    work->body.flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;

    // Each pair shares its arm contact record and attack identity.
    ACTOR_400500_INIT_ARM_ATTACK_SPHERE(work->rightArmOuter, work->rightArmContacts, ACTOR_400500_PART_RIGHT_ARM, -0x460, 0x290);

    ACTOR_400500_INIT_ARM_ATTACK_SPHERE(work->rightArmInner, work->rightArmContacts, ACTOR_400500_PART_RIGHT_ARM, -0x200, 0x250);

    ACTOR_400500_INIT_ARM_ATTACK_SPHERE(work->leftArmOuter, work->leftArmContacts, ACTOR_400500_PART_LEFT_ARM, 0x460, 0x290);

    ACTOR_400500_INIT_ARM_ATTACK_SPHERE(work->leftArmInner, work->leftArmContacts, ACTOR_400500_PART_LEFT_ARM, 0x200, 0x250);
#undef ACTOR_400500_INIT_ARM_ATTACK_SPHERE
}

/// Gives an attached strike-arm model its parent's texture-page and CLUT offsets.
///
/// Both models must remain live for this call. Copies signed texture-page word
/// displacements and CLUT-row displacements; no pointer or texture is owned.
/// With allocated packet storage, rebuilds both halves and preserves the next
/// half selector. The arm source, buffer capacities, GPU lifetime and scratch
/// space must satisfy `tmdBuildBufferHalf`. With no buffer, stores offsets only.
static inline void _actor400500ApplyArmTexturePlacement(const TmdObject* parentModel, TmdObject* armModel)
{
    armModel->texturePageOffset = parentModel->texturePageOffset;
    armModel->clutRowOffset     = parentModel->clutRowOffset;
    if (armModel->buffer != NULL) {
        tmdBuildBufferHalf(armModel);
        tmdBuildBufferHalf(armModel);
    }
}

/// Spawns and attaches the hidden left and right arm models used by strikes.
///
/// Requires initialized Stalker work/model and loaded arm descriptors/textures;
/// both task allocations must succeed. Roots attach to parts 10 and 7 at
/// opposite local-X offsets and 4096-unit yaw angles. Copies the parent's
/// texture offsets and rebuilds both primitive halves when allocated. The
/// Stalker retains both child handles, controls their visibility and kills
/// them during teardown; their per-frame callbacks are idle.
static void _actor400500SpawnArmModels(Task* task)
{
    enum { ACTOR_400500_ARM_ATTACHMENT_YAW = 384 };
    _Actor400500GrayStalkerWork* work;
    MATRIX                       armRotation;
    GfxCoord*                    partCoords;
    GfxCoord*                    rightArmCoord;
    GfxCoord*                    leftArmCoord;
    GfxCoord*                    childRoot;
    Task*                        armTask;
    TmdObject*                   armModel;
    TmdObject*                   texturedModel;
    TmdObject*                   parentModel;

    partCoords                            = task->extra.tmd->coords;
    work                                  = task->work;
    rightArmCoord                         = &partCoords[ACTOR_400500_PART_RIGHT_ARM];
    leftArmCoord                          = &partCoords[ACTOR_400500_PART_LEFT_ARM];
    armTask                               = taskSpawnFromTable(D_actor_400500_80153D48, ACTOR_400500_ARM_LEFT, 0, 0);
    work->armTasks[ACTOR_400500_ARM_LEFT] = armTask;
    armModel                              = armTask->extra.tmd;
    childRoot                             = armModel->coords;
    armModel->flags                       = TMD_OBJECT_SKIP_ACTIVE_DRAW;
    childRoot->parent                     = leftArmCoord;
    childRoot->coord.t[0]                 = 0x400;
    childRoot->coord.t[1]                 = 0;
    childRoot->coord.t[2]                 = 0;
    gfxSetRotIdentity(&armRotation);
    RotMatrixY((s16)-ACTOR_400500_ARM_ATTACHMENT_YAW, &armRotation);
    _actor400500CopyRotation(&armRotation, &childRoot->coord);
    parentModel   = task->extra.tmd;
    texturedModel = armTask->extra.tmd;
    _actor400500ApplyArmTexturePlacement(parentModel, texturedModel);
    armTask                                = taskSpawnFromTable(D_actor_400500_80153D48, ACTOR_400500_ARM_RIGHT, 0, 0);
    work->armTasks[ACTOR_400500_ARM_RIGHT] = armTask;
    armModel                               = armTask->extra.tmd;
    childRoot                              = armModel->coords;
    armModel->flags                        = TMD_OBJECT_SKIP_ACTIVE_DRAW;
    childRoot->parent                      = rightArmCoord;
    childRoot->coord.t[0]                  = -0x400;
    childRoot->coord.t[1]                  = 0;
    childRoot->coord.t[2]                  = 0;
    parentModel                            = task->extra.tmd;
    texturedModel                          = armTask->extra.tmd;
    _actor400500ApplyArmTexturePlacement(parentModel, texturedModel);
    gfxSetRotIdentity(&armRotation);
    RotMatrixY((s16)ACTOR_400500_ARM_ATTACHMENT_YAW, &armRotation);
    _actor400500CopyRotation(&armRotation, &childRoot->coord);
}

/// Updates the target offset, horizontal range/bearing and player movement in the Stalker's axes.
///
/// Requires live work/model roots in a common parent frame. Without a player,
/// leaves the snapshots intact. Offsets narrow to signed 16-bit game units;
/// the horizontal length also narrows to s16, and bearing wraps to 0..4095.
/// Uses the previous tick's player zone: zone 5 selects alternating patrol
/// endpoints every 256 calls and suppresses attacks with a two-tick cooldown.
/// The player-position snapshot is advanced separately after the state step.
static void _actor400500UpdateTargetMotion(Task* task)
{
    enum { ACTOR_400500_PATROL_X_FAR           = 10000,
           ACTOR_400500_PATROL_X_NEAR          = 1000,
           ACTOR_400500_PATROL_Z               = -8400,
           ACTOR_400500_PATROL_ENDPOINT_BIT    = 1 << 8,
           ACTOR_400500_PATROL_ATTACK_COOLDOWN = 2 };
    SVECTOR                      targetOffset;
    SVECTOR*                     targetOffsetPtr;
    SVECTOR                      playerDelta;
    MATRIX                       inverseYaw;
    _Actor400500GrayStalkerWork* work;
    GfxCoord*                    rootCoord;
    GfxCoord*                    playerRoot;
    s16                          distanceXZ;
    s16                          yaw;
    s16                          targetZ;
    s32                          patrolY;
    s32                          patrolZ;
    u16                          patrolTicks;

    work      = task->work;
    rootCoord = task->extra.tmd->coords;
    if (gPlayerActorTasks[PLAYER_ACTOR_TASK_PLAYER] != NULL) {
        playerRoot       = gPlayerActorTasks[PLAYER_ACTOR_TASK_PLAYER]->extra.tmd->coords;
        work->rootPos.vx = rootCoord->coord.t[0];
        work->rootPos.vy = rootCoord->coord.t[1];
        work->rootPos.vz = rootCoord->coord.t[2];
        if ((s16)work->playerZone != ACTOR_400500_CRAWL_ZONE_PATROL) {
            targetOffset.vx = (u16)playerRoot->coord.t[0] - (u16)rootCoord->coord.t[0];
            targetOffset.vy = (u16)playerRoot->coord.t[1] - (u16)rootCoord->coord.t[1];
            targetOffset.vz = (u16)playerRoot->coord.t[2] - (u16)rootCoord->coord.t[2];
        } else {
            work->attackCooldown = ACTOR_400500_PATROL_ATTACK_COOLDOWN;
            patrolTicks          = work->patrolFrames + 1;
            work->patrolFrames   = patrolTicks;
            if (!(patrolTicks & ACTOR_400500_PATROL_ENDPOINT_BIT)) {
                targetOffset.vx = ACTOR_400500_PATROL_X_FAR - (u16)rootCoord->coord.t[0];
            } else {
                targetOffset.vx = ACTOR_400500_PATROL_X_NEAR - (u16)rootCoord->coord.t[0];
            }
            patrolY         = ACTOR_400500_FLOOR_Y;
            targetOffset.vy = patrolY - (u16)rootCoord->coord.t[1];
            patrolZ         = ACTOR_400500_PATROL_Z;
            targetOffset.vz = patrolZ - (u16)rootCoord->coord.t[2];
        }
        // Save the unnormalized offset before computing its heading.
        distanceXZ        = SquareRoot0((targetOffset.vx * targetOffset.vx) + (targetOffset.vz * targetOffset.vz));
        work->toTarget.vx = (u16)targetOffset.vx;
        targetOffsetPtr   = &targetOffset;
        work->toTarget.vy = (u16)targetOffset.vy;
        targetZ           = (u16)targetOffset.vz;
        work->targetDist  = distanceXZ;
        work->toTarget.vz = targetZ;
        VectorNormalSS(targetOffsetPtr, targetOffsetPtr);
        work->targetBearing = (ratan2(targetOffset.vx, targetOffset.vz) - (u16)work->yaw) & (ACTOR_TRANSFORM_ANGLE_TURN - 1);
        // Rotate the player's frame-to-frame displacement into the Stalker's yaw.
        playerDelta.vx = (u16)playerRoot->coord.t[0] - (u16)work->playerPrevPos.vx;
        playerDelta.vy = (u16)playerRoot->coord.t[1] - (u16)work->playerPrevPos.vy;
        playerDelta.vz = (u16)playerRoot->coord.t[2] - (u16)work->playerPrevPos.vz;
        gfxSetRotIdentity(&inverseYaw);
        yaw = work->yaw;
        RotMatrixY(-yaw, &inverseYaw);
        ApplyMatrixSV(&inverseYaw, &playerDelta, &work->playerLocalMove);
    }
}

/// Draws a horizontal subtractive limb shadow at a caller-selected world height.
///
/// Requires a live 18-coordinate model rooted under the composed view; joint
/// indices are 0..17, and equal indices draw nothing. halfWidth and worldY
/// are world-coordinate units. X/Z endpoints and corners truncate to signed
/// halfwords; half spans remain s32 and overhang each end by half the limb.
/// shade is 0..255 texture modulation, grey in rooms 1/3/5/6 and half green
/// elsewhere. A negative GTE FLAG discards the quad rather than fully clipping it.
/// Requires GTE/scratch facilities, loaded texture/CLUT, room for POLY_FT4 in
/// the frame arena and the current 1024-tag depth table. Changes caches/GTE;
/// a queued packet remains live until frame DMA completes.
static void _actor400500DrawLimbShadow(Task* task, s16 firstJoint, s16 secondJoint, s16 halfWidth, s32 worldY, s32 shade)
{
    enum { ACTOR_400500_SHADOW_TRIG_FRACTION_BITS = 12,
           ACTOR_400500_SHADOW_TEXTURE_4_BIT      = 0,
           ACTOR_400500_SHADOW_U_MIN              = 192,
           ACTOR_400500_SHADOW_U_MAX              = 247,
           ACTOR_400500_SHADOW_V_MIN              = 152,
           ACTOR_400500_SHADOW_V_MAX              = 207,
           ACTOR_400500_SHADOW_PAGE_X             = 512,
           ACTOR_400500_SHADOW_PAGE_Y             = 0,
           ACTOR_400500_SHADOW_CLUT_X             = 48,
           ACTOR_400500_SHADOW_CLUT_Y             = 266 };
    MATRIX    firstMatrix;
    MATRIX    secondMatrix;
    SVECTOR   first;
    SVECTOR   second;
    SVECTOR   corner0;
    SVECTOR   corner1;
    SVECTOR   corner2;
    SVECTOR   corner3;
    long      screen0;
    long      screen1;
    long      screen2;
    long      screen3;
    long      depthCue;
    long      projectionFlags;
    s16       segmentYaw;
    GfxCoord* secondCoord;
    GfxCoord* firstCoord;
    s32       widthCosine0;
    s32       widthCosine1;
    s32       widthCosine2;
    s32       widthCosine3;
    s32       halfSpanX;
    s32       halfSpanZ;
    s32       depth;
    GfxCoord* partCoords;
    GfxCoord* viewCoord;
    POLY_FT4* poly;
    u8        room;
    u8        modulation;

    // Captures screen0..3, depth, shade, modulation, poly and room. Invoke only
    // inside this projection's success block; it expands one braced statement block.
    // Inputs are packed screen halves, sorting Z/4 and byte-range modulation.
    // Requires the loaded shadow texture and room for POLY_FT4 in the frame arena;
    // queues a subtractive packet in the 1024-tag OT, live through frame DMA.
#define ACTOR_400500_QUEUE_LIMB_SHADOW()                                                                                                                \
    {                                                                                                                                                   \
        poly           = gGpuPrimCursor;                                                                                                                \
        gGpuPrimCursor = poly + 1;                                                                                                                      \
        setPolyFT4(poly);                                                                                                                               \
        setSemiTrans(poly, true);                                                                                                                       \
        GPU_PRIMITIVE_XY_WORD(poly, 0) = screen0;                                                                                                       \
        GPU_PRIMITIVE_XY_WORD(poly, 1) = screen1;                                                                                                       \
        GPU_PRIMITIVE_XY_WORD(poly, 2) = screen2;                                                                                                       \
        GPU_PRIMITIVE_XY_WORD(poly, 3) = screen3;                                                                                                       \
        setUV4(poly, ACTOR_400500_SHADOW_U_MIN, ACTOR_400500_SHADOW_V_MIN,                                                                              \
               ACTOR_400500_SHADOW_U_MAX, ACTOR_400500_SHADOW_V_MIN,                                                                                    \
               ACTOR_400500_SHADOW_U_MIN, ACTOR_400500_SHADOW_V_MAX,                                                                                    \
               ACTOR_400500_SHADOW_U_MAX, ACTOR_400500_SHADOW_V_MAX);                                                                                   \
        poly->tpage = getTPage(ACTOR_400500_SHADOW_TEXTURE_4_BIT, GPU_BLEND_SUBTRACT,                                                                   \
                               ACTOR_400500_SHADOW_PAGE_X, ACTOR_400500_SHADOW_PAGE_Y);                                                                 \
        poly->clut  = getClut(ACTOR_400500_SHADOW_CLUT_X, ACTOR_400500_SHADOW_CLUT_Y);                                                                  \
        room        = gGameSession->location.loc.room;                                                                                                  \
        if ((room == 1) || (room == 3) || (room == 5) || (room == 6)) {                                                                                 \
            poly->r0 = modulation;                                                                                                                      \
            poly->g0 = modulation;                                                                                                                      \
            poly->b0 = modulation;                                                                                                                      \
        } else {                                                                                                                                        \
            poly->r0 = shade;                                                                                                                           \
            poly->g0 = modulation >> 1;                                                                                                                 \
            poly->b0 = shade;                                                                                                                           \
        }                                                                                                                                               \
        addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((u32)(depth << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK), poly); \
    }
    modulation  = shade;
    partCoords  = task->extra.tmd->coords;
    firstCoord  = partCoords + firstJoint;
    secondCoord = partCoords + secondJoint;
    if (firstJoint != secondJoint) {
        // Remove the view transform; positions and final corners narrow to halfwords.
        actorRenderComposeCoord(firstCoord);
        actorRenderComposeCoord(secondCoord);
        gfxMakeRelativeTransform(&gGfxViewCoord.workm, &firstCoord->workm, &firstMatrix);
        gfxMakeRelativeTransform(&gGfxViewCoord.workm, &secondCoord->workm, &secondMatrix);
        first.vy   = (s16)worldY;
        second.vy  = (s16)worldY;
        first.vx   = firstMatrix.t[0];
        first.vz   = firstMatrix.t[2];
        second.vx  = secondMatrix.t[0];
        second.vz  = secondMatrix.t[2];
        segmentYaw = ratan2((s16)secondMatrix.t[0] - (s16)firstMatrix.t[0], (s16)secondMatrix.t[2] - (s16)firstMatrix.t[2]);
        // Overhang each end by half the limb span and widen perpendicular to it.
        halfSpanX    = (first.vx - second.vx) / 2;
        halfSpanZ    = (first.vz - second.vz) / 2;
        widthCosine0 = rcos(segmentYaw) * halfWidth;
        corner0.vy   = (s16)worldY;
        corner0.vx   = halfSpanX + (first.vx - (widthCosine0 >> ACTOR_400500_SHADOW_TRIG_FRACTION_BITS));
        corner0.vz   = halfSpanZ + (first.vz + ((rsin(segmentYaw) * halfWidth) >> ACTOR_400500_SHADOW_TRIG_FRACTION_BITS));
        widthCosine1 = rcos(segmentYaw) * halfWidth;
        corner1.vy   = (s16)worldY;
        corner1.vx   = halfSpanX + (first.vx + (widthCosine1 >> ACTOR_400500_SHADOW_TRIG_FRACTION_BITS));
        corner1.vz   = halfSpanZ + (first.vz - ((rsin(segmentYaw) * halfWidth) >> ACTOR_400500_SHADOW_TRIG_FRACTION_BITS));
        widthCosine2 = rcos(segmentYaw) * halfWidth;
        corner2.vy   = (s16)worldY;
        corner2.vx   = (second.vx - (widthCosine2 >> ACTOR_400500_SHADOW_TRIG_FRACTION_BITS)) - halfSpanX;
        corner2.vz   = (second.vz + ((rsin(segmentYaw) * halfWidth) >> ACTOR_400500_SHADOW_TRIG_FRACTION_BITS)) - halfSpanZ;
        widthCosine3 = rcos(segmentYaw) * halfWidth;
        corner3.vy   = (s16)worldY;
        corner3.vx   = (second.vx + (widthCosine3 >> ACTOR_400500_SHADOW_TRIG_FRACTION_BITS)) - halfSpanX;
        corner3.vz   = (second.vz - ((rsin(segmentYaw) * halfWidth) >> ACTOR_400500_SHADOW_TRIG_FRACTION_BITS)) - halfSpanZ;
        // Project the world-ground corners through the refreshed view transform.
        viewCoord               = &gGfxViewCoord;
        viewCoord->composeStamp = GRAPHICS_COORD_DIRTY;
        actorRenderComposeCoord(viewCoord);
        gte_SetRotMatrix(&gGfxViewCoord.workm);
        gte_SetTransMatrix(&gGfxViewCoord.workm);
        depth = RotTransPers4(&corner0, &corner1, &corner2, &corner3, &screen0, &screen1, &screen2, &screen3,
                              &depthCue, &projectionFlags);
        if (projectionFlags >= 0) {
            ACTOR_400500_QUEUE_LIMB_SHADOW();
        }
    }
#undef ACTOR_400500_QUEUE_LIMB_SHADOW
}

/// Draws the Stalker's thirteen limb shadows on one horizontal world plane.
///
/// Requires a live 18-coordinate model beneath the composed view and the shadow
/// texture/palette and GPU packet storage. `worldY` is a signed game-coordinate
/// height; only the low byte of `shade` is used. Each segment has half-width 256.
/// The segment drawer applies the room's green tint and refreshes coordinate caches.
static void _actor400500DrawLimbShadows(Task* task, s16 worldY, s32 shade)
{
    enum { ACTOR_400500_LIMB_SHADOW_HALF_WIDTH = 256,
           ACTOR_400500_SHADOW_SHADE_MASK      = 0xFF };
    s32 byteShade;

    byteShade = shade & ACTOR_400500_SHADOW_SHADE_MASK;
    _actor400500DrawLimbShadow(task, ACTOR_400500_PART_BODY, 9, ACTOR_400500_LIMB_SHADOW_HALF_WIDTH, (s32)worldY, byteShade);
    _actor400500DrawLimbShadow(task, 9, ACTOR_400500_PART_LEFT_ARM, ACTOR_400500_LIMB_SHADOW_HALF_WIDTH, (s32)worldY, byteShade);
    _actor400500DrawLimbShadow(task, ACTOR_400500_PART_LEFT_ARM, ACTOR_400500_PART_LEFT_ARM_TIP, ACTOR_400500_LIMB_SHADOW_HALF_WIDTH, (s32)worldY, byteShade);
    _actor400500DrawLimbShadow(task, ACTOR_400500_PART_BODY, 6, ACTOR_400500_LIMB_SHADOW_HALF_WIDTH, (s32)worldY, byteShade);
    _actor400500DrawLimbShadow(task, 6, ACTOR_400500_PART_RIGHT_ARM, ACTOR_400500_LIMB_SHADOW_HALF_WIDTH, (s32)worldY, byteShade);
    _actor400500DrawLimbShadow(task, ACTOR_400500_PART_RIGHT_ARM, ACTOR_400500_PART_RIGHT_ARM_TIP, ACTOR_400500_LIMB_SHADOW_HALF_WIDTH, (s32)worldY, byteShade);
    _actor400500DrawLimbShadow(task, 1, 5, ACTOR_400500_LIMB_SHADOW_HALF_WIDTH, (s32)worldY, byteShade);
    _actor400500DrawLimbShadow(task, 1, 0xC, ACTOR_400500_LIMB_SHADOW_HALF_WIDTH, (s32)worldY, byteShade);
    _actor400500DrawLimbShadow(task, 0xC, 0xD, ACTOR_400500_LIMB_SHADOW_HALF_WIDTH, (s32)worldY, byteShade);
    _actor400500DrawLimbShadow(task, 0xD, 0xE, ACTOR_400500_LIMB_SHADOW_HALF_WIDTH, (s32)worldY, byteShade);
    _actor400500DrawLimbShadow(task, 1, 0xF, ACTOR_400500_LIMB_SHADOW_HALF_WIDTH, (s32)worldY, byteShade);
    _actor400500DrawLimbShadow(task, 0xF, 0x10, ACTOR_400500_LIMB_SHADOW_HALF_WIDTH, (s32)worldY, byteShade);
    _actor400500DrawLimbShadow(task, 0x10, 0x11, ACTOR_400500_LIMB_SHADOW_HALF_WIDTH, (s32)worldY, byteShade);
}

/// Places the Stalker on its ceiling route for the saved arrival selector.
///
/// Requires live work/model parented to the composed view and a valid saved
/// location. Arrival 1 starts at X 5280 facing -Z, 2 at X 16500 facing +Z,
/// and 3 at X 4000 facing -X; all use Y -4000, Z -8350 and half-turn roll.
/// Other selectors retain position/yaw/roll. Rebuilds the root basis without
/// pitch and samples part 11's world X/Z into the anchor, leaving anchor Y intact.
static void _actor400500PlaceAtArrival(Task* task)
{
    _Actor400500GrayStalkerWork* work;
    MATRIX                       rootRotation;
    GfxCoord*                    rootCoord;

    work      = task->work;
    rootCoord = task->extra.tmd->coords;
    switch (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.warp) {
        case 1:
            work->yaw             = ACTOR_TRANSFORM_ANGLE_HALF_TURN;
            work->roll            = ACTOR_TRANSFORM_ANGLE_HALF_TURN;
            rootCoord->coord.t[0] = 0x14A0;
            rootCoord->coord.t[1] = ACTOR_400500_CRAWL_CEILING_Y;
            rootCoord->coord.t[2] = ACTOR_400500_CRAWL_LINE_Z;
            break;
        case 2:
            work->roll            = ACTOR_TRANSFORM_ANGLE_HALF_TURN;
            work->yaw             = ACTOR_400500_CRAWL_YAW_POSITIVE_Z;
            rootCoord->coord.t[0] = ACTOR_400500_CRAWL_LINE_X;
            rootCoord->coord.t[1] = ACTOR_400500_CRAWL_CEILING_Y;
            rootCoord->coord.t[2] = ACTOR_400500_CRAWL_LINE_Z;
            break;
        case 3:
            work->yaw             = ACTOR_400500_CRAWL_YAW_NEGATIVE_X;
            work->roll            = ACTOR_TRANSFORM_ANGLE_HALF_TURN;
            rootCoord->coord.t[0] = 0xFA0;
            rootCoord->coord.t[1] = ACTOR_400500_CRAWL_CEILING_Y;
            rootCoord->coord.t[2] = ACTOR_400500_CRAWL_LINE_Z;
            break;
    }
    gfxSetRotIdentity(&rootRotation);
    RotMatrixZ(work->roll, &rootRotation);
    RotMatrixY(work->yaw, &rootRotation);
    _actor400500CopyRotation(&rootRotation, &rootCoord->coord);
    _actor400500SamplePartWorldXZ(task, ACTOR_400500_PART_LEFT_ARM_TIP, &work->anchorPos);
}

/// Enters a grab or arm strike when the target's range, bearing and cooldown allow it.
///
/// Returns 1 after entering an attack state, otherwise 0. Requires the live
/// Stalker's work and this frame's target measurements. Zone 5 suppresses attacks.
/// The narrower grab gate takes priority: failing its cooldown or ceiling test
/// returns 0 without trying a strike. Grab accepts unsigned cooldowns 0..7;
/// strikes require zero. Positive local player Z movement reduces both ranges.
static s32 _actor400500TryStartAttack(Task* task)
{
    enum { ACTOR_400500_NO_ATTACK_PLAYER_ZONE = 5 };
    _Actor400500GrayStalkerWork* work;
    _Actor400500GrayStalkerWork* strikeWork;

    work = (_Actor400500GrayStalkerWork*)task->work;
    if ((s16)work->playerZone != ACTOR_400500_NO_ATTACK_PLAYER_ZONE) {
        // Unsigned subtraction tests the cone across the 0/4096 bearing wrap.
        if ((work->targetDist < (ACTOR_400500_GRAB_RANGE - (work->playerLocalMove.vz * ACTOR_400500_TARGET_MOVE_RANGE_SCALE))) &&
            ((u32)(work->targetBearing - ACTOR_400500_GRAB_HALF_ANGLE) >= (ONE - 2 * ACTOR_400500_GRAB_HALF_ANGLE + 1U))) {
            if ((((u16)work->attackCooldown >> 3) == 0) && !(work->posture & ACTOR_400500_POSTURE_ON_FLOOR)) {
                _actor400500EnterState(task, ACTOR_400500_STATE_GRAB);
                return 1;
            }
            return 0;
        }
        if ((work->targetDist < (ACTOR_400500_STRIKE_RANGE - (work->playerLocalMove.vz * ACTOR_400500_TARGET_MOVE_RANGE_SCALE))) &&
            ((u32)(work->targetBearing - ACTOR_400500_STRIKE_HALF_ANGLE) >= (ONE - 2 * ACTOR_400500_STRIKE_HALF_ANGLE + 1U)) &&
            (work->attackCooldown == 0)) {
            strikeWork = (_Actor400500GrayStalkerWork*)task->work;
            if (!(strikeWork->posture & ACTOR_400500_POSTURE_ON_FLOOR)) {
                _actor400500EnterState(task, ACTOR_400500_STATE_STRIKE_CEILING);
            } else {
                _actor400500EnterState(task, ACTOR_400500_STATE_STRIKE_FLOOR);
            }
            return 1;
        }
        return 0;
    }
    return 0;
}

/// Advances a requested hide/show cloak fade and ages the hide cooldown once.
///
/// Requires live work, enemy/model and layer-material state. Negative signed
/// request bytes run phases 0 ramp, 1 hold and 2 ramp; nonnegative bytes only
/// age cooldown. Cloak and shadow levels use 0..255; colour blend is Q12 unity.
/// Ramps retain signed halfword truncation and quarter-step rounding. Hiding
/// sets draw exclusion and removes lock-on; showing clears exclusion before its ramp.
/// Show completion refreshes HP-dependent timing and seeds only an idle cooldown.
/// That newly seeded cooldown is decremented on the completion call too.
static void _actor400500TickCloakFade(Task* task)
{
    enum { ACTOR_400500_CLOAK_PHASE_HOLD        = 1,
           ACTOR_400500_CLOAK_PHASE_SECOND_RAMP = 2,
           ACTOR_400500_CLOAK_IDLE              = 0,
           ACTOR_400500_CLOAK_FULL_LEVEL        = 255,
           ACTOR_400500_CLOAK_HIDE_SNAP_LEVEL   = 248,
           ACTOR_400500_CLOAK_SHOW_SNAP_LEVEL   = 9,
           ACTOR_400500_CLOAK_SHOW_HOLD_TICKS   = 17,
           ACTOR_400500_CLOAK_BLEND_SNAP_GAP    = 16,
           ACTOR_400500_CLOAK_FULL_SHADOW_SHADE = 255 };
    _Actor400500GrayStalkerWork* work;
    Enemy*                       enemy;
    TmdObject*                   model;

    enemy = task->spawnArg2.pointer;
    work  = task->work;
    model = task->extra.tmd;
    if (work->cloakRequest < 0) {
        // Hiding builds the cloak material, holds it, then removes the body and lock-on.
        if (!((u8)work->cloakRequest & ACTOR_400500_CLOAK_SHOW)) {
            switch (work->cloakPhase) {
                case ACTOR_400500_CLOAK_PHASE_FIRST_RAMP:
                    work->cloakLevel = (u16)work->cloakLevel + ((s16)(ACTOR_400500_CLOAK_FULL_LEVEL - (u16)work->cloakLevel) >> 2);
                    if (work->cloakLevel >= ACTOR_400500_CLOAK_HIDE_SNAP_LEVEL) {
                        work->cloakLevel = ACTOR_400500_CLOAK_FULL_LEVEL;
                        work->cloakTimer = 0;
                        work->cloakPhase = (u8)work->cloakPhase + 1;
                    }
                    modelLightingSetLayerMaterials(work->cloakLevel);
                    break;
                case ACTOR_400500_CLOAK_PHASE_HOLD:
                    work->cloakTimer = (u16)work->cloakTimer + 1;
                    if (work->hideHoldFrames < work->cloakTimer) {
                        work->cloakPhase = (u8)work->cloakPhase + 1;
                    }
                    break;
                case ACTOR_400500_CLOAK_PHASE_SECOND_RAMP:
                    work->colorBlend  = (u16)work->colorBlend + ((s16) - (u16)work->colorBlend >> 2);
                    work->shadowShade = (u16)work->shadowShade + (-work->shadowShade >> 2);
                    if (work->colorBlend == 0) {
                        enemy->node.state.parts.flags = WORLD_TARGET_NOT_LOCKABLE;
                        if (work->commandActive == 0) {
                            enemy->node.state.parts.flags = (WORLD_TARGET_NOT_LOCKABLE | WORLD_TARGET_KEEP_SCANNED);
                        }
                        work->shadowShade  = 0;
                        work->cloakRequest = ACTOR_400500_CLOAK_IDLE;
                        model->flags      |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
                    }
                    model->shading.colorBlend = work->colorBlend;
                    break;
            }
        } else {
            // Showing restores draw/lock-on first, then removes the cloak material.
            switch (work->cloakPhase) {
                case ACTOR_400500_CLOAK_PHASE_FIRST_RAMP:
                    enemy->node.state.parts.flags = 0;
                    if (work->commandActive == 0) {
                        enemy->node.state.parts.flags = WORLD_TARGET_KEEP_SCANNED;
                    }
                    model->flags     &= ~TMD_OBJECT_SKIP_ACTIVE_DRAW;
                    work->colorBlend  = (u16)work->colorBlend + ((s16)(TMD_OBJECT_COLOR_BLEND_ONE - (u16)work->colorBlend) >> 2);
                    work->shadowShade = (u16)work->shadowShade + ((ACTOR_400500_CLOAK_FULL_SHADOW_SHADE - work->shadowShade) >> 2);
                    if (work->colorBlend >= TMD_OBJECT_COLOR_BLEND_ONE - ACTOR_400500_CLOAK_BLEND_SNAP_GAP) {
                        work->shadowShade = ACTOR_400500_CLOAK_FULL_SHADOW_SHADE;
                        work->colorBlend  = TMD_OBJECT_COLOR_BLEND_ONE;
                        work->cloakTimer  = 0;
                        work->cloakPhase  = (u8)work->cloakPhase + 1;
                    }
                    model->shading.colorBlend = work->colorBlend;
                    break;
                case ACTOR_400500_CLOAK_PHASE_HOLD:
                    work->cloakTimer = (u16)work->cloakTimer + 1;
                    if (work->cloakTimer >= ACTOR_400500_CLOAK_SHOW_HOLD_TICKS) {
                        work->cloakPhase = (u8)work->cloakPhase + 1;
                    }
                    break;
                case ACTOR_400500_CLOAK_PHASE_SECOND_RAMP:
                    work->cloakLevel = (u16)work->cloakLevel + ((s16) - (u16)work->cloakLevel >> 2);
                    if (work->cloakLevel < ACTOR_400500_CLOAK_SHOW_SNAP_LEVEL) {
                        work->cloakLevel   = 0;
                        work->cloakRequest = ACTOR_400500_CLOAK_IDLE;
                        _actor400500UpdateCloakTiming(task);
                        if (work->hideCooldown == 0) {
                            work->hideCooldown = (u16)work->hideCooldownReset;
                        }
                    }
                    modelLightingSetLayerMaterials(work->cloakLevel);
                    break;
            }
        }
    }
    if (work->hideCooldown > 0) {
        work->hideCooldown = (u16)work->hideCooldown - 1;
    }
}

/// Queues a packed sound-script start at the model root's cached view position.
///
/// Requires a live TMD task, composed root, initialized GTE/scratch facilities
/// and loaded sound resources that remain live through playback. `soundId`
/// supplies bank, instance and entry bytes unchanged. Pan uses three SPU steps
/// per signed-byte unit; depth uses 256 game-coordinate units per signed-byte
/// unit. Admission failure is ignored and no task pointer is retained.
static inline void _actor400500EnqueueRootSound(Task* task, s32 soundId)
{
    s32 panOffset;

    panOffset = (s8)worldCoordGetOriginAudioPan(task->extra.tmd->coords);
    sndEvtRequestScriptStart(soundId, panOffset, (s8)worldCoordGetOriginAudioDepth(task->extra.tmd->coords));
}

/// Advances a requested turn in place and consumes hits while it runs.
///
/// Requires the live work block and this frame's animation-slot results.
/// Returns 1 for either yaw direction, including the completion tick, and 0
/// without a turn. Requests the matching clip once at normal speed; yaw uses
/// 4096 units per turn and the 16-frame profile repeats until a boundary.
/// Completion snaps yaw to a 512-unit grid. Sound uses the composed root and
/// placement tag. Animation playback and the root's rotation update happen elsewhere.
static s32 _actor400500TickTurn(Task* task)
{
    enum {
        ACTOR_400500_ANIM_TURN_YAW_DOWN = 0x17,
        ACTOR_400500_ANIM_TURN_YAW_UP   = 0x18,
        ACTOR_400500_TURN_PROFILE_MASK  = ARRAY_SIZE(D_actor_400500_80153DB4) - 1,
        ACTOR_400500_TURN_SOUND_PHASE   = 8,
        ACTOR_400500_TURN_GRID_MASK     = 0xE00
    };
    _Actor400500GrayStalkerWork* work;
    s32                          soundId;
    u16                          nextYaw;

    work = (_Actor400500GrayStalkerWork*)task->work;
    // Consume the requested turn before ordinary crawling resumes.
    if (work->turnRequest == ACTOR_400500_TURN_YAW_DOWN) {
        if (work->turnStarted == 0) {
            _actor400500RequestAnimReset(task, ACTOR_400500_ANIM_TURN_YAW_DOWN, ANIMATION_RATE_ONE);
            work->stateFrames = 0;
            work->turnStarted = 1;
        }
        work->stateFrames = work->stateFrames + 1;
        if ((work->stateFrames & ACTOR_400500_TURN_PROFILE_MASK) == ACTOR_400500_TURN_SOUND_PHASE) {
            soundId = ((((Enemy*)task->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | ACTOR_400500_SOUND_STEP_1;
            _actor400500EnqueueRootSound(task, soundId);
        }
        nextYaw   = (u16)work->yaw - D_actor_400500_80153DB4[work->stateFrames & ACTOR_400500_TURN_PROFILE_MASK];
        work->yaw = nextYaw;
        if ((_actor400500CheckAnimBoundary(task) << 0x10) != 0) {
            work->stateFrames = 0;
            work->turnRequest = ACTOR_400500_TURN_NONE;
            work->yaw         = (u16)work->yaw & ACTOR_400500_TURN_GRID_MASK;
        }
        _actor400500ClearHitReaction(task);
        return 1;
    }
    if (work->turnRequest == ACTOR_400500_TURN_YAW_UP) {
        if (work->turnStarted == 0) {
            _actor400500RequestAnimReset(task, ACTOR_400500_ANIM_TURN_YAW_UP, ANIMATION_RATE_ONE);
            work->stateFrames = 0;
            work->turnStarted = 1;
        }
        work->stateFrames = work->stateFrames + 1;
        if ((work->stateFrames & ACTOR_400500_TURN_PROFILE_MASK) == ACTOR_400500_TURN_SOUND_PHASE) {
            soundId = ((((Enemy*)task->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | ACTOR_400500_SOUND_STEP_2;
            _actor400500EnqueueRootSound(task, soundId);
        }
        nextYaw   = (u16)work->yaw + D_actor_400500_80153DB4[work->stateFrames & ACTOR_400500_TURN_PROFILE_MASK];
        work->yaw = nextYaw;
        if ((_actor400500CheckAnimBoundary(task) << 0x10) != 0) {
            work->stateFrames = 0;
            work->turnRequest = ACTOR_400500_TURN_NONE;
            work->yaw         = (u16)work->yaw & ACTOR_400500_TURN_GRID_MASK;
        }
        _actor400500ClearHitReaction(task);
        return 1;
    }
    return 0;
}

/// Returns 1 when slot 1 reports a reached boundary, control jump, or held boundary pose.
///
/// Requires the live Stalker's initialized animation rig. Reads the latest slot
/// results without advancing playback or consuming the flags.
static inline s32 _actor400500HasAnimBoundary(Task* task)
{
    _Actor400500GrayStalkerWork* work = (_Actor400500GrayStalkerWork*)task->work;

    if ((work->rig.slots[1].status.fields.flags & ANIMATION_SLOT_REACHED_BOUNDARY) ||
        (work->rig.slots[1].status.word & (ANIMATION_SLOT_FOLLOWED_JUMP | ANIMATION_SLOT_SETTLED))) {
        return 1;
    }
    return 0;
}

/// Consumes light hits and handles heavy, blast and status recoil requests.
///
/// Requires the live Stalker's work. Returns 0 for no active hit or a discarded
/// light reaction; returns 1 while heavy recoil is active or after entering the
/// status-hold state. Heavy and blast reactions show the body and request a
/// four-frame blend into recoil; the active hit clears at an animation boundary.
static s32 _actor400500HandleHeavyHitReaction(Task* task)
{
    enum {
        ACTOR_400500_ANIM_HEAVY_RECOIL = 0xA,
        ACTOR_400500_HIT_BLEND_FRAMES  = 4
    };
    _Actor400500GrayStalkerWork* work;
    _Actor400500GrayStalkerWork* animationWork;
    s16                          hitActive;
    s16                          reaction;
    s32                          cloakMode;

    work      = (_Actor400500GrayStalkerWork*)task->work;
    hitActive = work->hitTaken;
    if (hitActive == 1) {
        reaction = work->hitReaction;
        if (reaction == ACTOR_400500_HIT_REACTION_LIGHT) {
            work->hitTaken    = 0;
            work->hitReaction = ACTOR_400500_HIT_REACTION_NONE;
            return 0;
        }
        if ((reaction == ACTOR_400500_HIT_REACTION_HEAVY) || (reaction == ACTOR_400500_HIT_REACTION_BLAST)) {
            if ((work->cloakRequest >= 0) || (((u8)work->cloakRequest & ACTOR_400500_CLOAK_KIND_MASK) != ACTOR_400500_CLOAK_SHOW)) {
                cloakMode          = ACTOR_400500_CLOAK_RUNNING | ACTOR_400500_CLOAK_SHOW;
                work->cloakRequest = cloakMode;
                work->cloakPhase   = 0;
            }
            animationWork                  = (_Actor400500GrayStalkerWork*)task->work;
            animationWork->animBlendFrames = ACTOR_400500_HIT_BLEND_FRAMES;
            animationWork->animRate        = ANIMATION_RATE_ONE;
            animationWork->animId          = ACTOR_400500_ANIM_HEAVY_RECOIL;
            animationWork->animRequest     = ACTOR_400500_ANIM_REQUEST_BLEND;
            work->hitReaction              = ACTOR_400500_HIT_REACTION_NONE;
        } else if (reaction == ACTOR_400500_HIT_REACTION_STATUS) {
            _actor400500EnterState(task, ACTOR_400500_STATE_STATUS_HOLD);
            work->hitTaken    = 0;
            work->hitReaction = ACTOR_400500_HIT_REACTION_NONE;
            return 1;
        }
        if (_actor400500HasAnimBoundary(task)) {
            work->hitTaken = 0;
        }
        return 1;
    }
    return 0;
}

/// Handles light, heavy, blast and status recoil during fallen crawling.
///
/// Requires the live Stalker's work and current slot results; the caller supplies
/// the fallen posture context. Returns 0 unless `hitTaken` is exactly 1.
/// An identified reaction requests a show without the hide cooldown gate and
/// requests a cut to its recoil clip, consuming the reaction code. Heavy and blast share
/// a clip; status uses a separate clip. Returns 1 even on the animation boundary
/// that clears the active hit. Playback advances in the calling state handler.
static s32 _actor400500HandleFallenHitReaction(Task* task)
{
    enum {
        ACTOR_400500_ANIM_FALLEN_LIGHT_RECOIL  = 0xB,
        ACTOR_400500_ANIM_FALLEN_HEAVY_RECOIL  = 0xC,
        ACTOR_400500_ANIM_FALLEN_STATUS_RECOIL = 0xE,
        ACTOR_400500_FALLEN_LIGHT_RECOIL_RATE  = 0x1C
    };
    _Actor400500GrayStalkerWork* work;
    _Actor400500GrayStalkerWork* animationWork;
    s16                          hitActive;
    s16                          reaction;
    s32                          cloakRequest;

    work      = (_Actor400500GrayStalkerWork*)task->work;
    hitActive = work->hitTaken;
    if (hitActive == 1) {
        reaction = work->hitReaction;
        if (reaction == ACTOR_400500_HIT_REACTION_LIGHT) {
            if ((work->cloakRequest >= 0) || (((u8)work->cloakRequest & ACTOR_400500_CLOAK_KIND_MASK) != ACTOR_400500_CLOAK_SHOW)) {
                cloakRequest       = ACTOR_400500_CLOAK_RUNNING | ACTOR_400500_CLOAK_SHOW;
                work->cloakRequest = cloakRequest;
                work->cloakPhase   = 0;
            }
            animationWork              = (_Actor400500GrayStalkerWork*)task->work;
            animationWork->animRate    = ACTOR_400500_FALLEN_LIGHT_RECOIL_RATE;
            animationWork->animId      = ACTOR_400500_ANIM_FALLEN_LIGHT_RECOIL;
            animationWork->animRequest = ACTOR_400500_ANIM_REQUEST_RESET;
            work->hitReaction          = ACTOR_400500_HIT_REACTION_NONE;
        } else if (reaction == ACTOR_400500_HIT_REACTION_HEAVY) {
            if ((work->cloakRequest >= 0) || (((u8)work->cloakRequest & ACTOR_400500_CLOAK_KIND_MASK) != ACTOR_400500_CLOAK_SHOW)) {
                cloakRequest       = ACTOR_400500_CLOAK_RUNNING | ACTOR_400500_CLOAK_SHOW;
                work->cloakRequest = cloakRequest;
                work->cloakPhase   = 0;
            }
            animationWork              = (_Actor400500GrayStalkerWork*)task->work;
            animationWork->animRate    = ANIMATION_RATE_ONE;
            animationWork->animId      = ACTOR_400500_ANIM_FALLEN_HEAVY_RECOIL;
            animationWork->animRequest = ACTOR_400500_ANIM_REQUEST_RESET;
            work->hitReaction          = ACTOR_400500_HIT_REACTION_NONE;
        } else if (reaction == ACTOR_400500_HIT_REACTION_BLAST) {
            if ((work->cloakRequest >= 0) || (((u8)work->cloakRequest & ACTOR_400500_CLOAK_KIND_MASK) != ACTOR_400500_CLOAK_SHOW)) {
                cloakRequest       = ACTOR_400500_CLOAK_RUNNING | ACTOR_400500_CLOAK_SHOW;
                work->cloakRequest = cloakRequest;
                work->cloakPhase   = 0;
            }
            animationWork              = (_Actor400500GrayStalkerWork*)task->work;
            animationWork->animRate    = ANIMATION_RATE_ONE;
            animationWork->animId      = ACTOR_400500_ANIM_FALLEN_HEAVY_RECOIL;
            animationWork->animRequest = ACTOR_400500_ANIM_REQUEST_RESET;
            work->hitReaction          = ACTOR_400500_HIT_REACTION_NONE;
        } else if (reaction == ACTOR_400500_HIT_REACTION_STATUS) {
            if ((work->cloakRequest >= 0) || (((u8)work->cloakRequest & ACTOR_400500_CLOAK_KIND_MASK) != ACTOR_400500_CLOAK_SHOW)) {
                cloakRequest       = ACTOR_400500_CLOAK_RUNNING | ACTOR_400500_CLOAK_SHOW;
                work->cloakRequest = cloakRequest;
                work->cloakPhase   = 0;
            }
            animationWork              = (_Actor400500GrayStalkerWork*)task->work;
            animationWork->animRate    = ANIMATION_RATE_ONE;
            animationWork->animId      = ACTOR_400500_ANIM_FALLEN_STATUS_RECOIL;
            animationWork->animRequest = ACTOR_400500_ANIM_REQUEST_RESET;
            work->hitReaction          = ACTOR_400500_HIT_REACTION_NONE;
        }
        if (_actor400500HasAnimBoundary(task)) {
            work->hitTaken = 0;
        }
        return 1;
    }
    return 0;
}

/// Requests a cut to an animation set at a signed sixteenth-frame playback rate.
///
/// Requires the live Stalker's work and a loaded set at `setIndex` (1..32)
/// in its borrowed table; entries 0 and 33 are NULL. Latches the request;
/// `_actor400500TickAnim` resets slots 1..17 on the next update. The rate is
/// retained as s16 here and narrowed to the slot's signed low byte when applied;
/// `ANIMATION_RATE_ONE` is one normal-rate frame per tick.
static inline void _actor400500SetAnim(Task* task, s16 setIndex, s16 rateSixteenths)
{
    _Actor400500GrayStalkerWork* work = (_Actor400500GrayStalkerWork*)task->work;

    work->animRate    = rateSixteenths;
    work->animId      = setIndex;
    work->animRequest = ACTOR_400500_ANIM_REQUEST_RESET;
}

/// Selects a living Stalker state and its sub-state without resetting frame counters.
///
/// Requires a live Stalker work block. `state` is an `ACTOR_400500_STATE_*`
/// and `subState` must index that state's step table; both stores keep the low
/// 16 bits. Current callers enter crawl or ambush at sub-state zero.
static inline void _actor400500SetState(Task* task, s32 state, s32 subState)
{
    _Actor400500GrayStalkerWork* work;

    work           = (_Actor400500GrayStalkerWork*)task->work;
    work->state    = state;
    work->subState = subState;
}

/// Applies a pending animation request and advances the Stalker's part slots once.
///
/// Requires the live, initialized rig and loaded clip data. Drives slots 1..17,
/// leaving root slot 0 untouched. A repeated blend request rescales the elapsed
/// whole-frame counter; a new blend or cut resets it, and ordinary playback
/// increments it with halfword wrapping. Slots consume the rate's signed low
/// byte in sixteenths of a frame. All borrowed rig/model storage stays live.
static inline void _actor400500TickAnim(Task* task)
{
    _Actor400500GrayStalkerWork* work;
    s32                          slotIndex;

    work = (_Actor400500GrayStalkerWork*)task->work;
    // Apply a request before advancing the part poses at the requested rate.
    if (work->animRequest == ACTOR_400500_ANIM_REQUEST_BLEND) {
        if (work->appliedAnim != work->animId) {
            work->animFrames = 0;
        } else {
            work->animFrames = _actor400500RescaleAnimFrames(task, work->animFrames);
        }
        _actor400500BlendAnimSlots(task);
        work->animRequest = ACTOR_400500_ANIM_REQUEST_PLAYING;
    } else if (work->animRequest == ACTOR_400500_ANIM_REQUEST_RESET) {
        _actor400500ResetAnimSlots(task);
        work->animRequest = ACTOR_400500_ANIM_REQUEST_PLAYING;
        work->animFrames  = 0;
    } else if (work->animRequest == ACTOR_400500_ANIM_REQUEST_PLAYING) {
        work->animFrames = (u16)work->animFrames + 1;
    }
    slotIndex = 1;
    do {
        work->rig.slots[slotIndex].rate = work->animRate;
        animationTickSlot(&work->rig.anim, slotIndex);
        slotIndex++;
    } while (slotIndex < ARRAY_SIZE(work->rig.slots));
}

/// Consumes a pending knockdown and enters its first state-machine step.
///
/// Requires a live Stalker work block. Clears any nonzero `knockdownPending`
/// and enters `ACTOR_400500_STATE_KNOCKDOWN` at sub-state zero, returning 1.
/// Returns 0 with no change when no request is pending; frame counters stay intact.
static inline s32 _actor400500TakeKnockdown(Task* task)
{
    _Actor400500GrayStalkerWork* work = (_Actor400500GrayStalkerWork*)task->work;

    if (work->knockdownPending != 0) {
        work->knockdownPending = 0;
        _actor400500EnterState(task, ACTOR_400500_STATE_KNOCKDOWN);
        return 1;
    }
    return 0;
}

/// Samples a model part's world X/Z position into signed halfwords.
///
/// The live model root must be parented directly to `gGfxViewCoord`, with an
/// acyclic hierarchy and `partIndex` in its coordinate array. `worldPosition`
/// is writable for this call only: X/Z retain the low 16 bits, and Y is left
/// untouched. Requires the composed view, GTE and scratch-stack facilities.
/// Refreshes the part's cache, removes the view transform, then marks it dirty.
static inline void _actor400500ReadPartWorldXZ(Task* task, s16 partIndex, SVECTOR3* worldPosition)
{
    MATRIX    partWorldTransform;
    GfxCoord* partCoord;

    partCoord = &task->extra.tmd->coords[partIndex];
    actorRenderComposeCoord(partCoord);
    gfxMakeRelativeTransform(&gGfxViewCoord.workm, &partCoord->workm, &partWorldTransform);
    worldPosition->vx       = partWorldTransform.t[0];
    worldPosition->vz       = partWorldTransform.t[2];
    partCoord->composeStamp = GRAPHICS_COORD_DIRTY;
}

/// Moves the model root in X/Z to hold a part at a saved world position.
///
/// The live model root must be parented directly to `gGfxViewCoord`, with an
/// acyclic hierarchy and `partIndex` in its coordinate array. `worldPosition`
/// supplies signed game-coordinate X/Z for this call only; Y is ignored and
/// root Y is preserved. A sample from `_actor400500ReadPartWorldXZ` already
/// carries halfword truncation. Requires the composed view, GTE and scratch
/// stack; refreshes the part and root caches after moving the root.
static inline void _actor400500PinPartWorldXZ(Task* task, s16 partIndex, const SVECTOR3* worldPosition)
{
    MATRIX    rootWorldTransform;
    MATRIX    partWorldTransform;
    GfxCoord* partCoord;
    GfxCoord* coords;

    coords    = task->extra.tmd->coords;
    partCoord = &coords[partIndex];
    actorRenderComposeCoord(partCoord);
    gfxMakeRelativeTransform(&gGfxViewCoord.workm, &coords[0].workm, &rootWorldTransform);
    gfxMakeRelativeTransform(&gGfxViewCoord.workm, &partCoord->workm, &partWorldTransform);
    // Subtract the part-to-root offset from the saved world position.
    coords[0].coord.t[0]    = worldPosition->vx - (partWorldTransform.t[0] - rootWorldTransform.t[0]);
    coords[0].coord.t[2]    = worldPosition->vz - (partWorldTransform.t[2] - rootWorldTransform.t[2]);
    coords[0].composeStamp  = GRAPHICS_COORD_DIRTY;
    partCoord->composeStamp = GRAPHICS_COORD_DIRTY;
    actorRenderComposeCoord(partCoord);
    actorRenderComposeCoord(coords);
}

/// Queues a packed sound-script id with spatial pan and attenuation at the model root.
///
/// The live TMD root's local-to-view cache must already be composed. `soundId`
/// includes the bank, instance and entry bytes; no placement tag is added.
/// Requires the origin-audio projection/scratch facilities and loaded sound
/// resources. Pan and depth are narrowed to signed bytes; admission failure
/// is ignored. Sound resources must remain loaded through playback; the request
/// retains no task pointer.
static inline void _actor400500EnqueueSound(Task* task, s32 soundId)
{
    s32 panOffset;

    panOffset = (s8)worldCoordGetOriginAudioPan(task->extra.tmd->coords);
    sndEvtRequestScriptStart(soundId, panOffset, (s8)worldCoordGetOriginAudioDepth(task->extra.tmd->coords));
}

/// Queues a sound-script entry with this enemy placement's instance tag.
///
/// Requires a live enemy in `spawnArg2.pointer` and a composed TMD root.
/// `baseSoundId` supplies the bank and entry bytes, normally with an empty
/// instance byte; the placement index is ORed into that byte. The bank stays
/// as supplied, including bank zero. Pan and attenuation use signed low bytes.
/// Admission failure is ignored; sound resources must stay loaded for playback.
/// Neither the task nor the enemy pointer is retained.
static inline void _actor400500PlayPlacedSound(Task* task, s32 baseSoundId)
{
    s32 placedSoundId;

    placedSoundId = ((((Enemy*)task->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | baseSoundId;
    _actor400500EnqueueRootSound(task, placedSoundId);
}

/// Replaces a coordinate's rotation with a unit-scale yaw in its parent's frame.
///
/// `yawAngle` uses 4096 units per turn. The live writable coordinate keeps its
/// translation, matrix alignment bytes, stored Euler state and parent link.
/// Only the nine Q12 rotation coefficients are replaced; pitch, roll and scale
/// are discarded. Leaves the cache stamp untouched; callers arrange recomposition.
static inline void _actor400500SetCoordYaw(GfxCoord* coord, s16 yawAngle)
{
    MATRIX yawRotation;

    gfxSetRotIdentity(&yawRotation);
    RotMatrixY(yawAngle, &yawRotation);
    _actor400500CopyRotation(&yawRotation, &coord->coord);
}

/// Converts a clip frame to a rate-scaled whole tick, retaining the low byte.
///
/// Requires live Stalker work and a nonnegative clip frame whose product with
/// 256 fits s32. Current callers use contact boundaries through clip frame 27.
/// Rates use signed sixteenths of a frame; zero yields zero. Signed division
/// occurs before unsigned division by 16 and byte narrowing, including at a
/// negative rate. This only reads the rate and retains no pointer.
static inline u8 _actor400500GaitFrameTick(Task* task, s32 clipFrame)
{
    u8 frameTick;

    if (((_Actor400500GrayStalkerWork*)task->work)->animRate == 0) {
        frameTick = 0;
    } else {
        frameTick = (u32)((clipFrame * ANIMATION_RATE_ONE * ANIMATION_RATE_ONE) /
                          ((_Actor400500GrayStalkerWork*)task->work)->animRate) /
                    ANIMATION_RATE_ONE;
    }
    return frameTick;
}

/// Moves a crawl cycle by alternately anchoring the two arm tips in world X/Z.
///
/// Starts clip 2 at 1.5 normal speed only when another clip is selected; the
/// existing clip's rate is retained. Left contact spans clip frames 0..11 and
/// right contact 12..21, inclusive, with a placement-tagged sound at each start.
/// Requires a live 18-part rig beneath the view coordinate and current slot
/// results. A boundary restarts elapsed frames; ordinary playback advances in
/// the calling state. Rate-scaled whole-tick bounds narrow to u8, zero at rate
/// zero; anchor coordinates retain signed low halfwords. Root Y stays intact.
static void _actor400500TickCrawl(Task* task)
{
    enum { ACTOR_400500_GAIT_CLIP = 2 };
    _Actor400500GrayStalkerWork* work;
    GfxCoord*                    rootCoord;
    u8                           firstContactEnd;
    u8                           secondContactStart;
    u8                           secondContactEnd;
    // Byte bounds preserve the clip-to-tick conversion and its wrapping.
    u8  firstContactStart;
    s32 placedSoundId;

    work      = (_Actor400500GrayStalkerWork*)task->work;
    rootCoord = task->extra.tmd->coords;
    if (work->animId != ACTOR_400500_GAIT_CLIP) {
        _actor400500SetAnim(task, ACTOR_400500_GAIT_CLIP, ANIMATION_RATE_ONE * 3 / 2);
        _actor400500TickAnim(task);
    }
    firstContactStart  = 0;
    firstContactEnd    = _actor400500GaitFrameTick(task, 11);
    secondContactStart = _actor400500GaitFrameTick(task, 12);
    secondContactEnd   = _actor400500GaitFrameTick(task, 21);
    if (_actor400500HasAnimBoundary(task)) {
        work->animFrames = 0;
    }
    // Sample each planted tip before moving the root around its saved position.
    if (work->animFrames == firstContactStart) {
        _actor400500ReadPartWorldXZ(task, ACTOR_400500_PART_LEFT_ARM_TIP, &work->anchorPos);
        placedSoundId = ((((Enemy*)task->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | ACTOR_400500_SOUND_STEP_1;
        _actor400500EnqueueSound(task, placedSoundId);
    }
    if (work->animFrames == secondContactStart) {
        _actor400500ReadPartWorldXZ(task, ACTOR_400500_PART_RIGHT_ARM_TIP, &work->anchorPos);
        placedSoundId = ((((Enemy*)task->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | ACTOR_400500_SOUND_STEP_2;
        _actor400500EnqueueSound(task, placedSoundId);
    }
    if (work->animFrames >= firstContactStart && work->animFrames <= firstContactEnd) {
        _actor400500PinPartWorldXZ(task, ACTOR_400500_PART_LEFT_ARM_TIP, &work->anchorPos);
    }
    if (work->animFrames >= secondContactStart && work->animFrames <= secondContactEnd) {
        _actor400500PinPartWorldXZ(task, ACTOR_400500_PART_RIGHT_ARM_TIP, &work->anchorPos);
    }
    rootCoord->composeStamp = GRAPHICS_COORD_DIRTY;
}

/// Moves the room-command crawl cycle at its currently selected animation rate.
///
/// Cuts to clip 2 at the existing rate only when a different clip is selected.
/// Left-arm contact spans clip frames 0..11 and right contact 12..21, inclusive,
/// with a placement-tagged sound at each start. Requires a live 18-part rig
/// beneath the view coordinate and current slot results. A boundary restarts
/// elapsed frames; ordinary playback advances in the calling state. Rate-scaled
/// whole-tick bounds narrow to u8, zero at rate zero; world X/Z anchors retain
/// signed low halfwords and moving the root preserves Y.
static void _actor400500TickScriptedCrawl(Task* task)
{
    enum { ACTOR_400500_GAIT_CLIP = 2 };
    _Actor400500GrayStalkerWork* work;
    GfxCoord*                    rootCoord;
    u8                           firstContactEnd;
    u8                           secondContactStart;
    u8                           secondContactEnd;
    // Byte bounds preserve the clip-to-tick conversion and its wrapping.
    u8 firstContactStart;

    work      = (_Actor400500GrayStalkerWork*)task->work;
    rootCoord = task->extra.tmd->coords;
    if (work->animId != ACTOR_400500_GAIT_CLIP) {
        _actor400500SetAnim(task, ACTOR_400500_GAIT_CLIP, work->animRate);
        _actor400500TickAnim(task);
    }
    firstContactStart  = 0;
    firstContactEnd    = _actor400500GaitFrameTick(task, 11);
    secondContactStart = _actor400500GaitFrameTick(task, 12);
    secondContactEnd   = _actor400500GaitFrameTick(task, 21);
    if (_actor400500HasAnimBoundary(task)) {
        work->animFrames = 0;
    }
    // Sample each planted tip before moving the root around its saved position.
    if (work->animFrames == firstContactStart) {
        _actor400500ReadPartWorldXZ(task, ACTOR_400500_PART_LEFT_ARM_TIP, &work->anchorPos);
        _actor400500PlayPlacedSound(task, ACTOR_400500_SOUND_STEP_1);
    }
    if (work->animFrames == secondContactStart) {
        _actor400500ReadPartWorldXZ(task, ACTOR_400500_PART_RIGHT_ARM_TIP, &work->anchorPos);
        _actor400500PlayPlacedSound(task, ACTOR_400500_SOUND_STEP_2);
    }
    if (work->animFrames >= firstContactStart && work->animFrames <= firstContactEnd) {
        _actor400500PinPartWorldXZ(task, ACTOR_400500_PART_LEFT_ARM_TIP, &work->anchorPos);
    }
    if (work->animFrames >= secondContactStart && work->animFrames <= secondContactEnd) {
        _actor400500PinPartWorldXZ(task, ACTOR_400500_PART_RIGHT_ARM_TIP, &work->anchorPos);
    }
    rootCoord->composeStamp = GRAPHICS_COORD_DIRTY;
}

/// Moves a fallen crawl cycle by anchoring the right arm, then the left arm.
///
/// Starts clip 4 at normal speed only when another clip is selected; the
/// existing clip's rate is retained. Right contact spans clip frames 0..13 and
/// left contact 14..27, inclusive, with a placement-tagged sound at each start.
/// Requires a live 18-part rig beneath the view coordinate and current slot
/// results. A boundary restarts elapsed frames; ordinary playback advances in
/// the calling state. Rate-scaled whole-tick bounds narrow to u8, zero at rate
/// zero; anchor coordinates retain signed low halfwords. Root Y stays intact.
static void _actor400500TickFallenCrawl(Task* task)
{
    enum { ACTOR_400500_GAIT_CLIP = 4 };
    _Actor400500GrayStalkerWork* work;
    GfxCoord*                    rootCoord;
    u8                           firstContactEnd;
    u8                           secondContactStart;
    u8                           secondContactEnd;
    // Byte bounds preserve the clip-to-tick conversion and its wrapping.
    u8  firstContactStart;
    s32 placedSoundId;

    work      = (_Actor400500GrayStalkerWork*)task->work;
    rootCoord = task->extra.tmd->coords;
    if (work->animId != ACTOR_400500_GAIT_CLIP) {
        _actor400500SetAnim(task, ACTOR_400500_GAIT_CLIP, ANIMATION_RATE_ONE);
        _actor400500TickAnim(task);
    }
    firstContactStart  = 0;
    firstContactEnd    = _actor400500GaitFrameTick(task, 13);
    secondContactStart = _actor400500GaitFrameTick(task, 14);
    secondContactEnd   = _actor400500GaitFrameTick(task, 27);
    if (_actor400500HasAnimBoundary(task)) {
        work->animFrames = 0;
    }
    // Sample each planted tip before moving the root around its saved position.
    if (work->animFrames == firstContactStart) {
        _actor400500ReadPartWorldXZ(task, ACTOR_400500_PART_RIGHT_ARM_TIP, &work->anchorPos);
        placedSoundId = ((((Enemy*)task->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | ACTOR_400500_SOUND_STEP_1;
        _actor400500EnqueueSound(task, placedSoundId);
    }
    if (work->animFrames == secondContactStart) {
        _actor400500ReadPartWorldXZ(task, ACTOR_400500_PART_LEFT_ARM_TIP, &work->anchorPos);
        placedSoundId = ((((Enemy*)task->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | ACTOR_400500_SOUND_STEP_2;
        _actor400500EnqueueSound(task, placedSoundId);
    }
    if (work->animFrames >= firstContactStart && work->animFrames <= firstContactEnd) {
        _actor400500PinPartWorldXZ(task, ACTOR_400500_PART_RIGHT_ARM_TIP, &work->anchorPos);
    }
    if (work->animFrames >= secondContactStart && work->animFrames <= secondContactEnd) {
        _actor400500PinPartWorldXZ(task, ACTOR_400500_PART_LEFT_ARM_TIP, &work->anchorPos);
    }
    rootCoord->composeStamp = GRAPHICS_COORD_DIRTY;
}

/// Consumes body attacks and status ticks, applies HP loss and queues the Stalker's recoil.
///
/// Requires live enemy/work/model and all three initialized contact records.
/// Attack keys must index the resident damage tables. HP loss, Life Drain
/// credit, readouts and hit effects obey the hit cooldown; attack attributes
/// still run during cooldown, including Life Drain motes. Critical damage
/// multiplies the base's unsigned low halfword by four; HP/readout amounts
/// narrow to signed halfwords. Fatal direct hits hold death until the move ends.
/// Status ticks retain their separate HP clamping behavior. Clears all body
/// contacts and decrements/clamps the hit cooldown after processing.
static void _actor400500TakeDamage(Task* task)
{
    enum { ACTOR_400500_HEAVY_HIT_HP                = 50,
           ACTOR_400500_ATTACK_REACTION_BLAST       = 4,
           ACTOR_400500_ATTACK_REACTION_HEAVY       = 5,
           ACTOR_400500_ATTACK_REACTION_KNOCKDOWN_8 = 8,
           ACTOR_400500_ATTACK_REACTION_KNOCKDOWN_9 = 9 };
    _Actor400500GrayStalkerWork* work;
    Enemy*                       enemy;
    u32                          baseDamage;
    u32                          hpDamage;
    s16                          hpDamage16;
    s16                          remainingHp;
    u8                           reactionFlags;
    s32                          damageOverTime;
    s16                          damageOverTime16;
    s32                          contactIndex;

    work  = task->work;
    enemy = task->spawnArg2.pointer;
    // HP cooldown gates damage, while each attack may still request a status or recoil.
    for (contactIndex = 0; contactIndex < ARRAY_SIZE(work->bodyContacts); contactIndex++) {
        if ((work->bodyContacts[contactIndex].key.value & WORLD_COLLISION_CONTACT_KIND_MASK) == WORLD_COLLISION_CONTACT_ATTACK) {
            if (work->hitCooldown == 0) {
                work->hitTaken    = 1;
                baseDamage        = damageComputePlayerAttack(work->bodyContacts[contactIndex].key.value, work->targetDist, 0, 0);
                hpDamage          = baseDamage;
                work->hitCooldown = damageGetPlayerAttackHitCooldown(work->bodyContacts[contactIndex].key.value);
                if (damageRollCriticalHit(enemy, work->bodyContacts[contactIndex].key.value, 0) != 0) {
                    hpDamage = ((u32)baseDamage << 16) >> 14;
                    effectSpawn(EFFECT_CRITICAL_HIT, &task->extra.tmd->coords[ACTOR_400500_PART_BODY], 0, NULL);
                }
                hpDamage16 = hpDamage;
                damageAccumulateLifeDrainHp(enemy, work->bodyContacts[contactIndex].key.value, hpDamage16, 0);
                worldTargetAddReadoutAmount(&enemy->node, hpDamage16, 0);
                remainingHp = (u16)enemy->hp - hpDamage;
                enemy->hp   = remainingHp;
                if ((remainingHp << 16) <= 0) {
                    enemy->hp       = 0;
                    work->deathHeld = 1;
                }
                effectSpawnHit(
                    damageGetPlayerAttackEffectId(work->bodyContacts[contactIndex].key.value),
                    &task->extra.tmd->coords[ACTOR_400500_PART_BODY],
                    NULL,
                    &work->effectArg);
                if (hpDamage16 >= ACTOR_400500_HEAVY_HIT_HP) {
                    work->hitReaction     = ACTOR_400500_HIT_REACTION_HEAVY;
                    hpDamage              = baseDamage;
                    work->lastHitReaction = ACTOR_400500_HIT_REACTION_HEAVY;
                } else {
                    work->hitReaction     = ACTOR_400500_HIT_REACTION_LIGHT;
                    work->lastHitReaction = ACTOR_400500_HIT_REACTION_LIGHT;
                }
            } else if ((damageGetPlayerAttackEffectId(work->bodyContacts[contactIndex].key.value)) == EFFECT_HIT_KIND_LIFE_DRAIN_MOTES) {
                effectSpawnHit(EFFECT_HIT_KIND_LIFE_DRAIN_MOTES, &task->extra.tmd->coords[1], NULL, &work->effectArg);
            }
            switch (damageGetPlayerAttackReaction(work->bodyContacts[contactIndex].key.value) & 0xFFFF) {
                case DAMAGE_PLAYER_REACTION_NONE:
                    break;
                case DAMAGE_PLAYER_REACTION_STAGGER:
                    damageStartEnemyStagger(enemy);
                    break;
                case DAMAGE_PLAYER_REACTION_BUILDUP:
                    damageStartEnemyBuildup(enemy, work->bodyContacts[contactIndex].key.value, 0);
                    break;
                case DAMAGE_PLAYER_REACTION_POISON:
                    damageTryStartEnemyDamageOverTime(enemy, work->bodyContacts[contactIndex].key.value, 0);
                    break;
                case ACTOR_400500_ATTACK_REACTION_BLAST:
                    work->hitReaction     = ACTOR_400500_HIT_REACTION_BLAST;
                    work->lastHitReaction = ACTOR_400500_HIT_REACTION_BLAST;
                    break;
                case ACTOR_400500_ATTACK_REACTION_HEAVY:
                    work->hitReaction     = ACTOR_400500_HIT_REACTION_HEAVY;
                    work->lastHitReaction = ACTOR_400500_HIT_REACTION_HEAVY;
                    break;
                case DAMAGE_PLAYER_REACTION_EXPLOSION:
                    work->hitReaction     = ACTOR_400500_HIT_REACTION_BLAST;
                    work->lastHitReaction = ACTOR_400500_HIT_REACTION_BLAST;
                    break;
                case DAMAGE_PLAYER_REACTION_INCENDIARY:
                    work->hitReaction     = ACTOR_400500_HIT_REACTION_HEAVY;
                    work->lastHitReaction = ACTOR_400500_HIT_REACTION_HEAVY;
                    break;
                case ACTOR_400500_ATTACK_REACTION_KNOCKDOWN_8:
                case ACTOR_400500_ATTACK_REACTION_KNOCKDOWN_9:
                    work->knockdownPending = 1;
                    break;
            }
        }
    }

    // Consume accumulated status requests after the contacts, then clear the frame records.
    reactionFlags = enemy->reactionFlags;
    if (reactionFlags & ENEMY_REACTION_STAGGER) {
        enemy->reactionFlags  = reactionFlags & ENEMY_REACTION_STAGGER_CLEAR;
        work->hitReaction     = ACTOR_400500_HIT_REACTION_HEAVY;
        work->lastHitReaction = ACTOR_400500_HIT_REACTION_HEAVY;
    }
    if (enemy->reactionFlags & ENEMY_REACTION_BUILDUP) {
        enemy->reactionFlags &= ENEMY_REACTION_BUILDUP_CLEAR;
        work->hitReaction     = ACTOR_400500_HIT_REACTION_STATUS;
        work->lastHitReaction = ACTOR_400500_HIT_REACTION_STATUS;
    }
    if (enemy->reactionFlags & ENEMY_REACTION_DAMAGE_OVER_TIME_BITS) {
        damageOverTime   = damageTickEnemyDamageOverTime(enemy);
        damageOverTime16 = damageOverTime;
        if (damageOverTime16 != 0) {
            enemy->hp = (u16)enemy->hp - damageOverTime;
            worldTargetAddReadoutAmount(&enemy->node, damageOverTime16, 0);
            if (enemy->hp < 0) {
                enemy->hp = 0;
            }
            work->hitTaken        = 1;
            work->hitReaction     = ACTOR_400500_HIT_REACTION_HEAVY;
            work->lastHitReaction = ACTOR_400500_HIT_REACTION_HEAVY;
        }
        if (damageIsEnemyDamageOverTimeExpired(enemy) != 0) {
            enemy->reactionFlags &= ENEMY_REACTION_DAMAGE_OVER_TIME_CLEAR;
        }
    }
    worldCollisionClearContacts(work->bodyContacts);
    if (work->hitCooldown > 0) {
        work->hitCooldown = (u16)work->hitCooldown - 1;
        return;
    }
    work->hitCooldown = 0;
}

/// Positions the captured player at the next grab pose, then rewinds the Stalker's part animation.
///
/// Requires the live Stalker rig and a player model rooted in the same world
/// frame; an absent player leaves all state intact. The part-8 local point
/// (352,328,704) transforms to signed-halfword world coordinates. Changes only
/// player X/Z: zero low halfword of `easePosition` snaps, otherwise each axis
/// closes one quarter of the gap. Composes the player, ticks backward at -16
/// sixteenths per frame and restores rate +16. Both passes process requests
/// and the wrapping frame counter; this is not a side-effect-free pose sample.
/// Requires composed view/GTE and the animation helpers' scratch space.
static void _actor400500PositionGrabbedPlayer(Task* task, s32 easePosition)
{
    SVECTOR                      grabPosition;
    GfxCoord*                    coords;
    GfxCoord*                    grabJoint;
    GfxCoord*                    playerRoot;
    Task*                        playerTask;
    _Actor400500GrayStalkerWork* work;
    s32                          currentPosition;
    s32                          targetPosition;

    coords     = task->extra.tmd->coords;
    playerTask = gPlayerActorTasks[PLAYER_ACTOR_TASK_PLAYER];
    grabJoint  = coords + ACTOR_400500_PART_RIGHT_ARM_TIP;
    work       = task->work;
    if (playerTask != NULL) {
        playerRoot = playerTask->extra.tmd->coords;
        // Sample the next arm pose before restoring playback with the reverse pass.
        _actor400500TickAnim(task);
        gGfxViewCoord.composeStamp = GRAPHICS_COORD_DIRTY;
        actorRenderComposeCoord(&gGfxViewCoord);
        grabJoint->composeStamp = GRAPHICS_COORD_DIRTY;
        actorRenderComposeCoord(grabJoint);
        grabPosition.vx = 0x160;
        grabPosition.vy = 0x148;
        grabPosition.vz = 0x2C0;
        _actorRenderTransformPointToWorld(grabJoint, &grabPosition);
        if ((easePosition << 0x10) == 0) {
            playerRoot->coord.t[0] = grabPosition.vx;
            playerRoot->coord.t[2] = grabPosition.vz;
        } else {
            targetPosition         = grabPosition.vx;
            currentPosition        = playerRoot->coord.t[0];
            currentPosition       += (targetPosition - currentPosition) >> 2;
            playerRoot->coord.t[0] = currentPosition;
            targetPosition         = grabPosition.vz;
            currentPosition        = playerRoot->coord.t[2];
            currentPosition       += (targetPosition - currentPosition) >> 2;
            playerRoot->coord.t[2] = currentPosition;
        }
        playerRoot->composeStamp = GRAPHICS_COORD_DIRTY;
        actorRenderComposeCoord(playerRoot);
        work->animRate = -ANIMATION_RATE_ONE;
        _actor400500TickAnim(task);
        work->animRate = ANIMATION_RATE_ONE;
    }
}

/// Copies body texture placement to a live death fragment and rebuilds both packet halves.
///
/// Both models and their loaded textures must remain live; the fragment's
/// existing buffer must satisfy `tmdBuildBufferHalf`. Neither pointer is retained.
static inline void _actor400500ApplyFragmentTexturePlacement(Task* task, EffectWork* fragment)
{
    TmdObject* bodyModel     = task->extra.tmd;
    TmdObject* fragmentModel = fragment->task->extra.tmd;

    fragmentModel->texturePageOffset = bodyModel->texturePageOffset;
    fragmentModel->clutRowOffset     = bodyModel->clutRowOffset;
    if (fragmentModel->buffer != NULL) {
        tmdBuildBufferHalf(fragmentModel);
        tmdBuildBufferHalf(fragmentModel);
    }
}

/// Spawns the blast death's head/two-leg model chunks and three gravity particles.
///
/// Requires the live body coordinates and loaded fragment/texture resources.
/// Selects each model before its spawn, then copies texture-page/CLUT placement
/// to any successful chunk and rebuilds both existing primitive halves. Spawn
/// failures are independent and ignored. Size 512 supplies the chunks' puff
/// size and the gravity particles' drawing extent in game units. Effect tasks
/// own the spawned work/models and retain their borrowed package resources.
static void _actor400500SpawnDeathFragments(Task* task)
{
    enum { ACTOR_400500_DEATH_FRAGMENT_SIZE = 512 };
    EffectWork* headEffect;
    EffectWork* rightLegEffect;
    EffectWork* leftLegEffect;

    D_800678F0[0] = &_gActor400500GrayStalkerBurstHead;
    headEffect    = effectSpawn(EFFECT_BODY_CHUNK, &task->extra.tmd->coords[ACTOR_400500_PART_BODY], ACTOR_400500_DEATH_FRAGMENT_SIZE, NULL);
    if (headEffect != NULL) {
        _actor400500ApplyFragmentTexturePlacement(task, headEffect);
    }
    D_800678F0[0]  = &_gActor400500GrayStalkerBurstLegRight;
    rightLegEffect = effectSpawn(EFFECT_BODY_CHUNK, &task->extra.tmd->coords[1], ACTOR_400500_DEATH_FRAGMENT_SIZE, NULL);
    if (rightLegEffect != NULL) {
        _actor400500ApplyFragmentTexturePlacement(task, rightLegEffect);
    }
    D_800678F0[0] = &_gActor400500GrayStalkerBurstLegLeft;
    leftLegEffect = effectSpawn(EFFECT_BODY_CHUNK, &task->extra.tmd->coords[1], ACTOR_400500_DEATH_FRAGMENT_SIZE, NULL);
    if (leftLegEffect != NULL) {
        _actor400500ApplyFragmentTexturePlacement(task, leftLegEffect);
    }
    effectSpawn(EFFECT_030, &task->extra.tmd->coords[1], ACTOR_400500_DEATH_FRAGMENT_SIZE, NULL);
    effectSpawn(EFFECT_030, &task->extra.tmd->coords[2], ACTOR_400500_DEATH_FRAGMENT_SIZE, NULL);
    effectSpawn(EFFECT_030, &task->extra.tmd->coords[ACTOR_400500_PART_BODY], ACTOR_400500_DEATH_FRAGMENT_SIZE, NULL);
}

#include "../../shared/frame_capture.inc.c"

/// Requests hiding when no matching fade is running and the cooldown is zero.
///
/// `task` is evaluated once. `workCursor` and `requestValue` must be writable,
/// side-effect-free identifiers of work-pointer and s32 type, respectively.
/// Refreshes the cursor and forms the full-width request before byte truncation;
/// a matching running fade retains its phase. Neither pointer is retained.
#define ACTOR_400500_REQUEST_HIDE(task, workCursor, requestValue)                              \
    {                                                                                          \
        (workCursor) = (task)->work;                                                           \
        if ((((workCursor)->cloakRequest >= 0) ||                                              \
             ((u8)(workCursor)->cloakRequest & ACTOR_400500_CLOAK_KIND_MASK)) &&               \
            ((workCursor)->hideCooldown == 0)) {                                               \
            (requestValue)             = ACTOR_400500_CLOAK_RUNNING | ACTOR_400500_CLOAK_HIDE; \
            (workCursor)->cloakRequest = (requestValue);                                       \
            (workCursor)->cloakPhase   = ACTOR_400500_CLOAK_PHASE_FIRST_RAMP;                  \
        }                                                                                      \
    }

/// Allocates and initializes the Gray Stalker's combat task and attached arms.
///
/// Requires a live model, enemy spawn record and loaded package resources.
/// Owns a zeroed work block; allocation failure destroys the enemy/task.
/// Initializes the borrowed animation bank, lighting storage and collision
/// bodies, links the target, and acquires a battle reference. Arm allocations
/// must succeed. Enters ambush at the saved arrival pose, requests hiding and
/// selects battle music; subsequent task states own playback and teardown.
static void _actor400500InitTask(Task* task)
{
    enum { ACTOR_400500_INITIAL_CLOAK_HOLD_TICKS = 16,
           ACTOR_400500_VISIBLE_CLOAK_HOLD_TICKS = 8192,
           ACTOR_400500_BATTLE_MUSIC_ENTRY       = 2,
           ACTOR_400500_HIT_EFFECT_ARGUMENT      = 256,
           ACTOR_400500_HIT_EFFECT_COUNT         = 3,
           ACTOR_400500_CLOAK_MAX_LEVEL          = 255,
           ACTOR_400500_SHADOW_MAX_SHADE         = 255,
           ACTOR_400500_AMBUSH_STEP_START        = 0 };
    TmdObject*                   model;
    Enemy*                       enemy;
    GfxCoord*                    rootCoord;
    _Actor400500GrayStalkerWork* work;
    _Actor400500GrayStalkerWork* movementWork;
    _Actor400500GrayStalkerWork* appearanceWork;
    GfxCoord*                    playerRoot;
    GfxCoord*                    partCoords;
    TmdObject*                   litModel;
    _Actor400500GrayStalkerWork* cloakWork;
    s32                          cloakRequest;
    u8                           room;

    model      = task->extra.tmd;
    enemy      = task->spawnArg2.pointer;
    rootCoord  = model->coords;
    task->work = memCalloc(sizeof(_Actor400500GrayStalkerWork), 0);
    work       = task->work;
    if (work == NULL) {
        enemyDestroy(enemy, task);
        return;
    }
    // Publish work-owned lighting and hit storage to the model and enemy.
    model->lightMtx   = &work->lightMtx;
    model->colorMtx   = &work->colorMtx;
    model->flags      = 0;
    enemy->field_4    = &rootCoord->coord;
    enemy->field_48   = 0;
    enemy->bodyPos.vx = 0;
    enemy->bodyPos.vy = 0;
    enemy->bodyPos.vz = 0;
    enemy->coord      = &task->extra.tmd->coords[ACTOR_400500_PART_BODY];
    worldTargetLinkNode(&enemy->node);
    enemy->node.state.parts.flags = WORLD_TARGET_NOT_LOCKABLE;
    enemy->recs                   = work->bodyContacts;
    enemy->param                  = &D_actor_400500_80153C90;
    enemy->hp = enemy->hpMax = D_actor_400500_80153C90.hpMax;
    // Borrow the package clip bank and install the initial crawl pose.
    animationInitContext(&work->rig.anim, (AnimationSet**)D_actor_400500_80153CC0, model, work->rig.poses,
                         work->rig.slots);
    rootCoord->parent = &gGfxViewCoord;
    _actor400500SetAnim(task, ACTOR_400500_ANIM_CRAWL, ACTOR_400500_CRAWL_RATE);
    _actor400500TickAnim(task);
    task->msgTable = D_actor_400500_80153CA0;
    _actor400500PlaceAtArrival(task);
    movementWork = task->work;
    if (gPlayerActorTasks[PLAYER_ACTOR_TASK_PLAYER] != NULL) {
        playerRoot                     = gPlayerActorTasks[PLAYER_ACTOR_TASK_PLAYER]->extra.tmd->coords;
        movementWork->playerPrevPos.vx = playerRoot->coord.t[0];
        movementWork->playerPrevPos.vy = playerRoot->coord.t[1];
        movementWork->playerPrevPos.vz = playerRoot->coord.t[2];
    }
    // The night-room variants begin cloaked; other rooms hold the visible phase.
    appearanceWork = task->work;
    room           = gGameSession->location.loc.room;
    litModel       = task->extra.tmd;
    if ((room == 1) || (room == 3) || (room == 5) || (room == 6)) {
        appearanceWork->cloakLevel     = ACTOR_400500_CLOAK_MAX_LEVEL;
        appearanceWork->colorBlend     = 0;
        appearanceWork->shadowShade    = 0;
        appearanceWork->hideHoldFrames = ACTOR_400500_INITIAL_CLOAK_HOLD_TICKS;
    } else {
        appearanceWork->colorBlend     = TMD_OBJECT_COLOR_BLEND_ONE;
        appearanceWork->shadowShade    = ACTOR_400500_SHADOW_MAX_SHADE;
        appearanceWork->cloakLevel     = 0;
        appearanceWork->hideHoldFrames = ACTOR_400500_VISIBLE_CLOAK_HOLD_TICKS;
    }
    modelLightingSetLayerMaterials(appearanceWork->cloakLevel);
    litModel->shading.colorBlend = appearanceWork->colorBlend;
    // Join combat only after the body, attacks and attached arms are ready.
    _actor400500InitCollisionBodies(task);
    _actor400500SpawnArmModels(task);
    sceneAcquireBattleRef(0);
    partCoords                 = task->extra.tmd->coords;
    work->effectArg.spawnArgLo = ACTOR_400500_HIT_EFFECT_ARGUMENT;
    work->effectArg.spawnArgHi = ACTOR_400500_HIT_EFFECT_COUNT;
    work->effectArg.coord      = &partCoords[ACTOR_400500_PART_BODY];
    _actor400500SetState(task, ACTOR_400500_STATE_AMBUSH, ACTOR_400500_AMBUSH_STEP_START);
    gStageSceneMusicEntry = ACTOR_400500_BATTLE_MUSIC_ENTRY;
    ACTOR_400500_REQUEST_HIDE(task, cloakWork, cloakRequest);
    task->state = task->state + 1;
}

/// Handlers the task entry `func_actor_400500_8013DE98` runs by task state:
/// set-up, the per-frame state machine run by `state`, a second one run by
/// `state` from task state 2, and the teardown that kills the child tasks
/// and destroys the enemy 0x12D frames later.
static const TaskFuncTable4 D_actor_400500_80131E4C = { {
    _actor400500InitTask,
    func_actor_400500_80135770,
    func_actor_400500_8013A700,
    _actor400500TickTeardown,
} };

/// Returns the first crawl-route zone containing a model root's X/Z, or zero outside all zones.
///
/// Requires a live root in the route rectangles' room frame. X/Z narrow to
/// signed 16-bit game coordinates; both edges are inclusive and table order
/// resolves overlaps. Scans through `ACTOR_ZONE_END`; nonzero results are IDs
/// 1..6. The same rectangles classify the Stalker and the player.
static inline s32 _actor400500FindRouteZone(Task* task)
{
    const ActorZone* zone;
    GfxCoord*        rootCoord = task->extra.tmd->coords;
    s16              x         = rootCoord->coord.t[0];
    s16              z         = rootCoord->coord.t[2];

    for (zone = D_actor_400500_80153D6C; zone->id != ACTOR_ZONE_END; zone++) {
        if (zone->x <= x && x <= zone->x + zone->width && zone->z <= z && z <= zone->z + zone->depth) {
            return zone->id;
        }
    }
    return 0;
}

/// Reserves a VECTOR scratch block containing a coordinate's cached XYZ translation.
///
/// Does not compose or convert `coord`; its cache must already describe the
/// caller's desired frame. Requires an initialized word-aligned scratch stack
/// with room for one VECTOR plus nested consumers. Only XYZ are written;
/// pad is untouched. The returned block is borrowed until the caller releases
/// it in reverse order with `SCRATCH_STACK_RELEASE_BLOCK(VECTOR)`.
static inline VECTOR* _actor400500PushCachedPosition(const GfxCoord* coord)
{
    VECTOR* position = (VECTOR*)(SCRATCH_STACK_CURSOR(u8) - sizeof(VECTOR));

    position->vx                 = coord->workm.t[0];
    position->vy                 = coord->workm.t[1];
    position->vz                 = coord->workm.t[2];
    SCRATCH_STACK_CURSOR(VECTOR) = position;
    return position;
}

/// Releases a caller-selected byte extent from the shared scratch stack.
///
/// Requires an initialized cursor and a nonnegative byte count equal to the
/// latest live reservation(s). Releases in reverse order without bounds checks;
/// no released pointer remains usable. The signed count preserves the inline
/// release's interface; callers with typed blocks pass their complete sizeof.
static inline void _scratchStackReleaseBytes(s32 byteCount)
{
    SCRATCH_STACK_RELEASE_BYTES(byteCount);
}

static __inline__ u8* push_proj(void)
{
    u8*                      head  = SCRATCH_STACK_CURSOR(u8);
    ActorOriginDepthScratch* block = (ActorOriginDepthScratch*)(head - sizeof(ActorOriginDepthScratch));

    SCRATCH_STACK_CURSOR(ActorOriginDepthScratch)                                   = block;
    ((ActorOriginDepthScratch*)(head - sizeof(ActorOriginDepthScratch)))->origin.vx = 0;
    block->origin.vy                                                                = 0;
    block->origin.vz                                                                = 0;
    return head;
}

static const _Actor400500StateTable D_actor_400500_80131E5C = { {
    _actor400500TickCrawlState,
    func_actor_400500_8013BA24,
    _actor400500TickCeilingStrikeState,
    _actor400500TickFloorStrikeState,
    _actor400500TickCeilingFallState,
    _actor400500TickKnockdownState,
    _actor400500TickAmbushState,
    _actor400500TickDropState,
    _actor400500TickJumpState,
    _actor400500TickTurnOverState,
    _actor400500TickFallenCrawlState,
    _actor400500TickRoomSequenceState,
    _actor400500TickStatusHoldState,
} };

static void func_actor_400500_80135770(Task* arg0)
{
    _Actor400500GrayStalkerWork* work;
    Enemy*                       enemy;
    TmdObject*                   extra0;
    TmdObject*                   obj;
    Task*                        slot;
    GfxCoord*                    part2;
    PlayerStatus*                cfg;
    AnimationPlayRequest         msg;
    _Actor400500StateTable       states;
    MATRIX                       rot;
    s8                           handshake;
    _Actor400500GrayStalkerWork* work_pos;
    _Actor400500GrayStalkerWork* work_dead;
    _Actor400500GrayStalkerWork* work_rot;
    s16                          ang;
    s16                          ang_z;
    s16                          ang_y;
    GfxCoord*                    rot_root;
    GfxCoord*                    player;
    TmdObject*                   extra;
    TmdObject*                   extra2;
    TmdObject*                   trans_obj;
    VECTOR*                      color;
    u8*                          head;
    GfxCoord*                    color_part;
    u8                           mode;
    s16                          trans;
    s16                          trans_y;
    ActorOriginDepthScratch*     proj;
    SVECTOR*                     vecp;
    MATRIX*                      workm;

    cfg    = &gPlayerStatus;
    work   = (_Actor400500GrayStalkerWork*)arg0->work;
    enemy  = (Enemy*)arg0->spawnArg2.pointer;
    extra0 = arg0->extra.tmd;
    part2  = extra0->coords + 2;
    obj    = extra0;
    slot   = *gPlayerActorTasks;
    states = D_actor_400500_80131E5C;

    handshake = work->grabStep;
    switch (handshake) {
        case 1:
            if (taskMessageDispatch(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), ANIMATION_MESSAGE_IS_PLAYING, 0, 0) == 0) {
                work->grabStep = 2;
            }
            break;
        case 2:
            msg.source.sets          = _gActor400500PlayerAnimationSets;
            msg.blend                = ANIMATION_BLEND_RESET;
            msg.blendFrames          = 0;
            msg.enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE;
            if (work->grabLanded != 0) {
                msg.animationId = 3;
                work->grabStep  = 4;
                TASK_MESSAGE_DISPATCH_POINTER(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), ANIMATION_MESSAGE_REPLACE_AND_PLAY, &msg, 0);
            } else {
                msg.animationId = handshake;
                work->grabStep  = 3;
                TASK_MESSAGE_DISPATCH_POINTER(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), ANIMATION_MESSAGE_INSTALL_AND_PLAY, &msg, 0);
            }
            break;
        case 3:
            if (taskMessageDispatch(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), ANIMATION_MESSAGE_IS_PLAYING, 0, 0) == 0) {
                taskMessageDispatch(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), GAME_ACTOR_MESSAGE_END_SCRIPTED, 0, 0);
                work->grabStep = 0;
            }
            break;
    }

    switch (gSceneCombatState.actorControl) {
        case SCENE_COMBAT_ACTORS_HIDDEN:
            obj->flags |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
            return;
        case SCENE_COMBAT_ACTORS_RUNNING:
            _actor400500UpdateTargetMotion(arg0);
            work->zone = _actor400500FindRouteZone(arg0);
            if (slot == NULL) {
                work->playerZone = 0;
            } else {
                work->playerZone = _actor400500FindRouteZone(slot);
            }
            states.handlers[(s16)work->state](arg0);
            work_pos = (_Actor400500GrayStalkerWork*)arg0->work;
            if (*gPlayerActorTasks != NULL) {
                player                     = (*gPlayerActorTasks)->extra.tmd->coords;
                work_pos->playerPrevPos.vx = player->coord.t[0];
                work_pos->playerPrevPos.vy = player->coord.t[1];
                work_pos->playerPrevPos.vz = player->coord.t[2];
            }
            work_rot        = (_Actor400500GrayStalkerWork*)arg0->work;
            rot_root        = arg0->extra.tmd->coords;
            ang             = work_rot->pitch;
            ang_y           = work_rot->yaw;
            work_rot->pitch = ang & 0xFFF;
            ang_z           = work_rot->roll;
            work_rot->yaw   = ang_y & 0xFFF;
            work_rot->roll  = ang_z & 0xFFF;
            gfxSetRotIdentity(&rot);
            RotMatrixZ(work_rot->roll, &rot);
            RotMatrixX(work_rot->pitch, &rot);
            RotMatrixY(work_rot->yaw, &rot);
            _actor400500CopyRotation(&rot, &rot_root->coord);
            _actor400500TickCloakFade(arg0);
            if (work->attackCooldown > 0) {
                work->attackCooldown = (u16)work->attackCooldown - 1;
                work->attackCooling  = 1;
            } else {
                work->attackCooling = 0;
            }
            obj->flags &= (u16)~TMD_OBJECT_SKIP_ACTIVE_DRAW;
            if ((enemy->hp > 0) || (cfg->hp <= 0)) {
                _actor400500TakeDamage(arg0);
            } else if ((work->deathHeld == 0) && (work->grabStep == 0)) {
                work_dead           = (_Actor400500GrayStalkerWork*)arg0->work;
                arg0->state         = 2;
                work_dead->state    = 0;
                work_dead->subState = 0;
            }
        case SCENE_COMBAT_ACTORS_PAUSED:
            extra      = arg0->extra.tmd;
            extra2     = extra;
            color_part = extra->coords + 1;
            color      = _actor400500PushCachedPosition(color_part);
            worldCoordUpdateActorColor(arg0->spawnArg2.pointer, color, 0, 0);
            mode = gGameSession->location.loc.room;
            if ((mode == 1) || (mode == 3) || (mode == 5) || (mode == 6)) {
                trans_obj = extra2;
                trans     = 0x200;
                trans_y   = trans;
            } else {
                trans_obj = extra;
                trans     = 0x400;
                trans_y   = 0x1000;
            }
            worldCoordSetModelAmbientColor(trans_obj, trans, trans_y, trans);
            _scratchStackReleaseBytes(sizeof(VECTOR));
            if (gGameSession->sceneUpdatesPaused != 0) {
                _actor400500DrawLimbShadows(arg0, -0xFA0, (u8)work->shadowShade);
                return;
            }
            _actor400500DrawLimbShadows(arg0, -0xFA0, ((u16)work->shadowShade >> 2) & 0xFF);
            _actor400500DrawLimbShadows(arg0, -0x3E8, (u8)work->shadowShade);
            head = push_proj();
            proj = (ActorOriginDepthScratch*)(head - sizeof(ActorOriginDepthScratch));
            actorRenderComposeCoord(part2);
            vecp  = &proj->origin;
            workm = &part2->workm;
            gte_SetRotMatrix(workm);
            gte_SetTransMatrix(workm);
            gte_ldv0(vecp);
            gte_rtps();
            gte_stsxy(&((ActorOriginDepthScratch*)(head - sizeof(ActorOriginDepthScratch)))->screenPos);
            gte_stdp(&((ActorOriginDepthScratch*)(head - sizeof(ActorOriginDepthScratch)))->depthCue);
            gte_stflg(&((ActorOriginDepthScratch*)(head - sizeof(ActorOriginDepthScratch)))->flag);
            gte_stszotz(&((ActorOriginDepthScratch*)(head - sizeof(ActorOriginDepthScratch)))->otz);
            if (proj->flag < 0) {
                proj->otz = 0;
            }
            proj->otz = (proj->otz >> 4) + 0x1E;
            FRAME_CAPTURE_QUEUE(proj->otz);
            _scratchStackReleaseBytes(sizeof(ActorOriginDepthScratch));
            return;
    }
}

/// Sub-state handlers `_actor400500TickCrawlState` copies onto the stack and runs
/// by `subState` every frame.
static const TaskFuncTable11 D_actor_400500_80131E90 = { {
    _actor400500SelectCrawlRoute,
    _actor400500TickCrawlPositiveX,
    _actor400500TickCrawlTurnNegativeX,
    _actor400500TickCrawlNegativeX,
    _actor400500TickCrawlTurnPositiveX,
    _actor400500TickCrawlCornerChoice,
    _actor400500TickCrawlPositiveZ,
    _actor400500TickCrawlTurnNegativeZ,
    _actor400500TickCrawlNegativeZ,
    _actor400500TickCrawlCornerTurnNegativeX,
    _actor400500TickCrawlTurnPositiveZ,
} };

/// Handlers `_actor400500TickCrawlState` also runs, by `deathStep`, while the
/// enemy is out of hit points.
static const TaskFuncTable3 D_actor_400500_80131EBC = { {
    _actor400500StartCrawlDeathFall,
    _actor400500TickCrawlDeathFall,
    _actor400500FinishCrawlDeathLanding,
} };

/// Disables contact participation for all four arm attack spheres.
///
/// Borrows writable Stalker work for this call. Clears pair/trigger participation
/// while retaining list links, recorded contacts and all other body flags.
/// The receiving body and both arm models retain their current state.
static inline void _actor400500DisableArmContacts(_Actor400500GrayStalkerWork* work)
{
    work->rightArmOuter.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
    work->rightArmInner.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
    work->leftArmOuter.flags  &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
    work->leftArmInner.flags  &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
}

/// Requests a cloak fade without consulting the hide cooldown.
///
/// Requires live work. `cloakRequest` combines the running bit with HIDE or
/// SHOW. Preserves a matching running fade; otherwise stores the signed low
/// byte of the full-width request and resets its phase. No pointer is retained.
static inline void _actor400500RequestCloakFadeIgnoringCooldown(Task* task, s32 cloakRequest)
{
    _Actor400500GrayStalkerWork* work = task->work;

    if ((work->cloakRequest >= 0) || (((u8)work->cloakRequest & ACTOR_400500_CLOAK_KIND_MASK) != (cloakRequest & ACTOR_400500_CLOAK_KIND_MASK))) {
        work->cloakRequest = cloakRequest;
        work->cloakPhase   = ACTOR_400500_CLOAK_PHASE_FIRST_RAMP;
    }
}

/// Dispatches the ceiling/floor crawl or its ceiling-death fall each frame.
///
/// Requires initialized Stalker work, enemy and rig. `subState` indexes the
/// eleven crawl steps; at nonpositive HP, `deathStep` indexes three death-fall
/// steps instead. Playback advances after a death step, but before a living
/// crawl step unless it is the route-selection step. Requests hiding while
/// alive; death requests showing and disables all arm attack contacts.
static void _actor400500TickCrawlState(Task* task)
{
    _Actor400500GrayStalkerWork* work;
    Enemy*                       enemy;
    TaskFuncTable11              crawlSteps;
    TaskFuncTable3               deathSteps;
    _Actor400500GrayStalkerWork* cloakWork;
    s32                          cloakRequest;

    work       = task->work;
    enemy      = task->spawnArg2.pointer;
    crawlSteps = D_actor_400500_80131E90;
    deathSteps = D_actor_400500_80131EBC;
    if (enemy->hp <= 0) {
        // Finish the ceiling-death fall while showing the body and ending attacks.
        deathSteps.funcs[(s16)work->deathStep](task);
        _actor400500TickAnim(task);
        _actor400500RequestCloakFadeIgnoringCooldown(task, ACTOR_400500_CLOAK_RUNNING | ACTOR_400500_CLOAK_SHOW);
        _actor400500DisableArmContacts(work);
        return;
    }
    if ((s16)work->subState != ACTOR_400500_CRAWL_STEP_SELECT) {
        _actor400500TickAnim(task);
    }
    crawlSteps.funcs[(s16)work->subState](task);
    ACTOR_400500_REQUEST_HIDE(task, cloakWork, cloakRequest);
}

#undef ACTOR_400500_REQUEST_HIDE

/// Copies the crawl reset's nine Q12 rotation coefficients into the root matrix.
///
/// Requires live readable/writable matrices, borrowed for this call. Source and
/// destination may be the same matrix. The 18-byte rotation transfer preserves
/// translation and alignment bytes; a complete MATRIX assignment would not.
static inline void _actor400500CopyCrawlResetRotation(const MATRIX* source, MATRIX* destination)
{
    destination->m[0][0] = source->m[0][0];
    destination->m[0][1] = source->m[0][1];
    destination->m[0][2] = source->m[0][2];
    destination->m[1][0] = source->m[1][0];
    destination->m[1][1] = source->m[1][1];
    destination->m[1][2] = source->m[1][2];
    destination->m[2][0] = source->m[2][0];
    destination->m[2][1] = source->m[2][1];
    destination->m[2][2] = source->m[2][2];
}

/// Consumes a ceiling-fall request through the caller's live work cursor.
///
/// `task`, `workCursor` and `taken` must be side-effect-free identifiers. The
/// cursor is refreshed, the request is cleared and `taken` receives 0 or 1;
/// entering fall evaluates `task` again. The caller supplies a writable work
/// pointer and s32 result. No pointer is retained.
#define ACTOR_400500_TAKE_CEILING_FALL(task, workCursor, taken)          \
    {                                                                    \
        (workCursor) = (task)->work;                                     \
        if ((workCursor)->ceilingFallPending != 0) {                     \
            (workCursor)->ceilingFallPending = 0;                        \
            if ((workCursor)->posture & ACTOR_400500_POSTURE_ON_FLOOR) { \
                (taken) = 0;                                             \
            } else {                                                     \
                _actor400500EnterState((task), ACTOR_400500_STATE_FALL); \
                (taken) = 1;                                             \
            }                                                            \
        } else {                                                         \
            (taken) = 0;                                                 \
        }                                                                \
    }

/// Chooses a crawl direction from the room zone and current target offset.
///
/// Requires the live Stalker work, model beneath the view coordinate and current
/// targeting results. Knockdown, ceiling fall and attack requests take priority.
/// Otherwise resets the crawl clip, clears turn progress and saves the left arm
/// tip's world X/Z anchor. An unrecognized zone restores the ceiling start pose.
static void _actor400500SelectCrawlRoute(Task* task)
{
    MATRIX                       resetRotation;
    MATRIX*                      rotation;
    MATRIX*                      rootRotation;
    _Actor400500GrayStalkerWork* work;
    _Actor400500GrayStalkerWork* fallWork;
    _Actor400500GrayStalkerWork* turnWork;
    GfxCoord*                    rootCoord;
    s32                          fallTaken;
    s32                          yawHeading;
    s32                          playerZone;

    work       = task->work;
    yawHeading = (u16)work->yaw & ACTOR_TRANSFORM_ANGLE_MASK;
    rootCoord  = task->extra.tmd->coords;
    if (!_actor400500TakeKnockdown(task)) {
        ACTOR_400500_TAKE_CEILING_FALL(task, fallWork, fallTaken);
        if ((fallTaken == 0) && ((_actor400500TryStartAttack(task) << 0x10) == 0)) {
            turnWork              = task->work;
            turnWork->turnRequest = ACTOR_400500_TURN_NONE;
            turnWork->turnStarted = 0;
            _actor400500SetAnim(task, ACTOR_400500_ANIM_CRAWL, ACTOR_400500_CRAWL_RATE);
            _actor400500TickAnim(task);
            switch ((s16)((u16)work->zone - ACTOR_400500_CRAWL_ZONE_X_SEGMENT)) {
                case ACTOR_400500_CRAWL_ZONE_X_END - ACTOR_400500_CRAWL_ZONE_X_SEGMENT:
                    if (yawHeading == ACTOR_400500_CRAWL_YAW_POSITIVE_X) {
                        work->subState = ACTOR_400500_CRAWL_STEP_POSITIVE_X;
                    } else {
                        work->subState = ACTOR_400500_CRAWL_STEP_TURN_POSITIVE_X;
                    }
                    break;
                case ACTOR_400500_CRAWL_ZONE_X_SEGMENT - ACTOR_400500_CRAWL_ZONE_X_SEGMENT:
                    if (work->toTarget.vx < 0) {
                        work->subState = ACTOR_400500_CRAWL_STEP_NEGATIVE_X;
                    } else {
                        work->subState = ACTOR_400500_CRAWL_STEP_POSITIVE_X;
                    }
                    break;
                case ACTOR_400500_CRAWL_ZONE_CORNER - ACTOR_400500_CRAWL_ZONE_X_SEGMENT:
                    playerZone = (s16)work->playerZone;
                    if ((playerZone == ACTOR_400500_CRAWL_ZONE_X_SEGMENT) || (playerZone == ACTOR_400500_CRAWL_ZONE_X_END) || (playerZone == ACTOR_400500_CRAWL_ZONE_PATROL)) {
                        work->subState = ACTOR_400500_CRAWL_STEP_NEGATIVE_X;
                    } else if ((playerZone == ACTOR_400500_CRAWL_ZONE_Z_SEGMENT) && (yawHeading == ACTOR_400500_CRAWL_YAW_POSITIVE_Z) && (rootCoord->coord.t[0] >= ACTOR_400500_CRAWL_LINE_X)) {
                        work->subState = ACTOR_400500_CRAWL_STEP_POSITIVE_Z;
                    } else if ((s16)work->playerZone == ACTOR_400500_CRAWL_ZONE_CORNER) {
                        if (rootCoord->coord.t[2] >= (ACTOR_400500_CRAWL_LINE_Z + 1)) {
                            work->subState = ACTOR_400500_CRAWL_STEP_POSITIVE_Z;
                        } else {
                            work->subState = ACTOR_400500_CRAWL_STEP_POSITIVE_X;
                        }
                    } else {
                        work->subState = ACTOR_400500_CRAWL_STEP_POSITIVE_X;
                    }
                    break;
                case ACTOR_400500_CRAWL_ZONE_Z_SEGMENT - ACTOR_400500_CRAWL_ZONE_X_SEGMENT:
                    if (work->toTarget.vz < 0) {
                        work->subState = ACTOR_400500_CRAWL_STEP_NEGATIVE_Z;
                    } else {
                        work->subState = ACTOR_400500_CRAWL_STEP_POSITIVE_Z;
                    }
                    break;
                case ACTOR_400500_CRAWL_ZONE_Z_END - ACTOR_400500_CRAWL_ZONE_X_SEGMENT:
                    if (yawHeading == ACTOR_400500_CRAWL_YAW_NEGATIVE_Z) {
                        work->subState = ACTOR_400500_CRAWL_STEP_NEGATIVE_Z;
                    } else {
                        work->subState = ACTOR_400500_CRAWL_STEP_TURN_NEGATIVE_Z;
                    }
                    break;
                default:
                    rootCoord->coord.t[0] = ACTOR_400500_CRAWL_RESET_X;
                    rootCoord->coord.t[1] = ACTOR_400500_CRAWL_CEILING_Y;
                    rootCoord->coord.t[2] = ACTOR_400500_CRAWL_RESET_Z;
                    work->yaw             = ACTOR_400500_CRAWL_YAW_POSITIVE_X;
                    work->roll            = ACTOR_TRANSFORM_ANGLE_HALF_TURN;
                    rotation              = &resetRotation;
                    work->pitch           = 0;
                    work->posture         = 0;
                    gfxSetRotIdentity(rotation);
                    RotMatrixZ(work->roll, rotation);
                    RotMatrixY(work->yaw, rotation);
                    rootRotation = &rootCoord->coord;
                    _actor400500CopyCrawlResetRotation(rotation, rootRotation);
                    _actor400500ReadPartWorldXZ(task, ACTOR_400500_PART_LEFT_ARM_TIP, &work->anchorPos);
                    work->subState = ACTOR_400500_CRAWL_STEP_POSITIVE_X;
                    break;
            }
            _actor400500ReadPartWorldXZ(task, ACTOR_400500_PART_LEFT_ARM_TIP, &work->anchorPos);
        }
    }
}

/// Crawls toward +X on the fixed-Z route, turning or taking the corner as needed.
///
/// Requires live Stalker work/root and current target and animation-slot results.
/// State changes, recoil and active turns preempt route movement. At the X end,
/// a cooled attack permits reversal; while cooling, a distant target gives a
/// 1/32 chance per eligible tick to drop from the ceiling or jump off the floor.
/// The caller advances ordinary animation playback.
static void _actor400500TickCrawlPositiveX(Task* task)
{
    _Actor400500GrayStalkerWork* work;
    _Actor400500GrayStalkerWork* requestWork;
    GfxCoord*                    rootCoord;
    s32                          knockdownTaken;
    s32                          fallTaken;
    s32                          yawHeading;
    s32                          zone;
    s32                          nextState;
    u32                          randomState;
    u16                          playerZone;

    work      = task->work;
    rootCoord = task->extra.tmd->coords;
    // Consume preempting actions before changing the route or pinning its axis.
    knockdownTaken = _actor400500TakeKnockdown(task);
    if (knockdownTaken == 0) {
        ACTOR_400500_TAKE_CEILING_FALL(task, requestWork, fallTaken);
        if ((fallTaken == 0) && ((_actor400500TryStartAttack(task) << 0x10) == 0) &&
            ((_actor400500HandleHeavyHitReaction(task) << 0x10) == 0) &&
            ((_actor400500TickTurn(task) << 0x10) == 0)) {
            yawHeading = (u16)work->yaw;
            if ((yawHeading & ACTOR_TRANSFORM_ANGLE_MASK) != ACTOR_400500_CRAWL_YAW_POSITIVE_X) {
                if (((ACTOR_400500_CRAWL_YAW_POSITIVE_X - yawHeading) << 0x14) > 0) {
                    requestWork              = task->work;
                    requestWork->turnRequest = ACTOR_400500_TURN_YAW_UP;
                } else {
                    requestWork              = task->work;
                    requestWork->turnRequest = ACTOR_400500_TURN_YAW_DOWN;
                }
                requestWork->turnStarted = 0;
            } else {
                zone = work->zone;
                if (zone != ACTOR_400500_CRAWL_ZONE_X_SEGMENT) {
                    if ((zone == ACTOR_400500_CRAWL_ZONE_CORNER) && (rootCoord->coord.t[0] >= (ACTOR_400500_CRAWL_LINE_X + 1))) {
                        playerZone = work->playerZone;
                        if (((u32)(playerZone - ACTOR_400500_CRAWL_ZONE_CORNER) < 2U) || ((s16)playerZone == ACTOR_400500_CRAWL_ZONE_Z_END)) {
                            work->subState = ACTOR_400500_CRAWL_STEP_CORNER_CHOICE;
                        } else {
                            work->subState = zone;
                        }
                    } else {
                        _actor400500TickCrawl(task);
                    }
                } else {
                    if (work->attackCooling == 0) {
                        if (work->toTarget.vx <= 0) {
                            work->subState = ACTOR_400500_CRAWL_STEP_TURN_NEGATIVE_X;
                        }
                    } else if (work->toTarget.vx < (-ACTOR_400500_POSTURE_CHANGE_TARGET_GAP + 1)) {
                        randomState     = (gRandomLcgState * RANDOM_LCG_MULTIPLIER) + RANDOM_LCG_INCREMENT;
                        gRandomLcgState = randomState;
                        if (((randomState >> 0x10) & ACTOR_400500_POSTURE_CHANGE_CHANCE_MASK) == 0) {
                            if (!(work->posture & ACTOR_400500_POSTURE_ON_FLOOR)) {
                                requestWork = task->work;
                                nextState   = ACTOR_400500_STATE_DROP;
                            } else {
                                requestWork = task->work;
                                nextState   = ACTOR_400500_STATE_JUMP;
                            }
                            requestWork->state    = nextState;
                            requestWork->subState = ACTOR_400500_CRAWL_STEP_SELECT;
                        }
                    }
                    _actor400500TickCrawl(task);
                }
            }
            rootCoord->coord.t[2] = ACTOR_400500_CRAWL_LINE_Z;
        }
    }
}

/// Turns the X-route crawl toward -X, then restarts the crawl clip.
///
/// Requires live Stalker work and current target and slot results. Pending state
/// changes, recoil and active turns take priority; a 4096-unit yaw uses the signed
/// wrapped 12-bit difference to choose the next turn. Playback advances elsewhere.
static void _actor400500TickCrawlTurnNegativeX(Task* task)
{
    _Actor400500GrayStalkerWork* work;
    _Actor400500GrayStalkerWork* requestWork;
    _Actor400500GrayStalkerWork* increasingTurnWork;
    _Actor400500GrayStalkerWork* decreasingTurnWork;
    s32                          knockdownTaken;
    s32                          fallTaken;
    s32                          yawHeading;

    work = task->work;
    // Consume preempting actions before changing the route or pinning its axis.
    knockdownTaken = _actor400500TakeKnockdown(task);
    if (knockdownTaken == 0) {
        ACTOR_400500_TAKE_CEILING_FALL(task, requestWork, fallTaken);
        if ((fallTaken == 0) && ((_actor400500TryStartAttack(task) << 0x10) == 0) &&
            ((_actor400500HandleHeavyHitReaction(task) << 0x10) == 0) &&
            ((_actor400500TickTurn(task) << 0x10) == 0)) {
            yawHeading = (u16)work->yaw;
            if ((yawHeading & ACTOR_TRANSFORM_ANGLE_MASK) == ACTOR_400500_CRAWL_YAW_NEGATIVE_X) {
                _actor400500SetAnim(task, ACTOR_400500_ANIM_CRAWL, ACTOR_400500_CRAWL_RATE);
                work->subState = ACTOR_400500_CRAWL_STEP_NEGATIVE_X;
                return;
            }
            if (((ACTOR_400500_CRAWL_YAW_NEGATIVE_X - yawHeading) << 0x14) > 0) {
                increasingTurnWork              = task->work;
                increasingTurnWork->turnRequest = ACTOR_400500_TURN_YAW_UP;
                increasingTurnWork->turnStarted = 0;
            } else {
                decreasingTurnWork              = task->work;
                decreasingTurnWork->turnRequest = ACTOR_400500_TURN_YAW_DOWN;
                decreasingTurnWork->turnStarted = 0;
            }
        }
    }
}

/// Crawls toward -X on the fixed-Z route and reverses at the X end or target.
///
/// Requires live Stalker work/root and current target and slot results. State
/// changes, recoil and active turns preempt movement. A distant target while an
/// attack cools gives a 1/32 chance per eligible tick to change floor/ceiling
/// posture. The caller advances ordinary animation playback.
static void _actor400500TickCrawlNegativeX(Task* task)
{
    _Actor400500GrayStalkerWork* work;
    _Actor400500GrayStalkerWork* requestWork;
    GfxCoord*                    rootCoord;
    s32                          knockdownTaken;
    s32                          fallTaken;
    s32                          yawHeading;
    s32                          zone;
    s32                          nextState;
    u32                          randomState;

    work      = task->work;
    rootCoord = task->extra.tmd->coords;
    // Consume preempting actions before changing the route or pinning its axis.
    knockdownTaken = _actor400500TakeKnockdown(task);
    if (knockdownTaken == 0) {
        ACTOR_400500_TAKE_CEILING_FALL(task, requestWork, fallTaken);
        if ((fallTaken == 0) && ((_actor400500TryStartAttack(task) << 0x10) == 0) &&
            ((_actor400500HandleHeavyHitReaction(task) << 0x10) == 0) &&
            ((_actor400500TickTurn(task) << 0x10) == 0)) {
            yawHeading = (u16)work->yaw;
            if ((yawHeading & ACTOR_TRANSFORM_ANGLE_MASK) != ACTOR_400500_CRAWL_YAW_NEGATIVE_X) {
                if (((ACTOR_400500_CRAWL_YAW_NEGATIVE_X - yawHeading) << 0x14) > 0) {
                    requestWork              = task->work;
                    requestWork->turnRequest = ACTOR_400500_TURN_YAW_UP;
                } else {
                    requestWork              = task->work;
                    requestWork->turnRequest = ACTOR_400500_TURN_YAW_DOWN;
                }
                requestWork->turnStarted = 0;
            } else {
                zone = work->zone;
                if (zone != ACTOR_400500_CRAWL_ZONE_X_SEGMENT) {
                    if (zone == ACTOR_400500_CRAWL_ZONE_X_END) {
                        work->subState = zone;
                    }
                } else if (work->attackCooling == 0) {
                    if (work->toTarget.vx >= 0) {
                        work->subState = ACTOR_400500_CRAWL_STEP_TURN_POSITIVE_X;
                    }
                } else if (work->toTarget.vx >= ACTOR_400500_POSTURE_CHANGE_TARGET_GAP) {
                    randomState     = (gRandomLcgState * RANDOM_LCG_MULTIPLIER) + RANDOM_LCG_INCREMENT;
                    gRandomLcgState = randomState;
                    if (((randomState >> 0x10) & ACTOR_400500_POSTURE_CHANGE_CHANCE_MASK) == 0) {
                        if (!(work->posture & ACTOR_400500_POSTURE_ON_FLOOR)) {
                            requestWork = task->work;
                            nextState   = ACTOR_400500_STATE_DROP;
                        } else {
                            requestWork = task->work;
                            nextState   = ACTOR_400500_STATE_JUMP;
                        }
                        requestWork->state    = nextState;
                        requestWork->subState = ACTOR_400500_CRAWL_STEP_SELECT;
                    }
                }
                _actor400500TickCrawl(task);
            }
            rootCoord->coord.t[2] = ACTOR_400500_CRAWL_LINE_Z;
        }
    }
}

/// Turns the X-route crawl toward +X, then restarts the crawl clip.
///
/// Requires live Stalker work and current target and slot results. Pending state
/// changes, recoil and active turns take priority. At the X-end zone the target's
/// Z side chooses the turn; elsewhere the wrapped 12-bit yaw difference does.
static void _actor400500TickCrawlTurnPositiveX(Task* task)
{
    _Actor400500GrayStalkerWork* work;
    _Actor400500GrayStalkerWork* requestWork;
    s32                          knockdownTaken;
    s32                          fallTaken;
    s32                          yawHeading;

    work = task->work;
    // Consume preempting actions before changing the route or pinning its axis.
    knockdownTaken = _actor400500TakeKnockdown(task);
    if (knockdownTaken == 0) {
        ACTOR_400500_TAKE_CEILING_FALL(task, requestWork, fallTaken);
        if ((fallTaken == 0) && ((_actor400500TryStartAttack(task) << 0x10) == 0) &&
            ((_actor400500HandleHeavyHitReaction(task) << 0x10) == 0) &&
            ((_actor400500TickTurn(task) << 0x10) == 0)) {
            yawHeading = (u16)work->yaw;
            if ((yawHeading & ACTOR_TRANSFORM_ANGLE_MASK) == ACTOR_400500_CRAWL_YAW_POSITIVE_X) {
                _actor400500SetAnim(task, ACTOR_400500_ANIM_CRAWL, ACTOR_400500_CRAWL_RATE);
                work->subState = ACTOR_400500_CRAWL_STEP_POSITIVE_X;
                return;
            }
            if ((s16)work->playerZone == ACTOR_400500_CRAWL_ZONE_X_END) {
                if (work->toTarget.vz > 0) {
                    requestWork              = task->work;
                    requestWork->turnRequest = ACTOR_400500_TURN_YAW_UP;
                } else {
                    requestWork              = task->work;
                    requestWork->turnRequest = ACTOR_400500_TURN_YAW_DOWN;
                }
            } else if (((ACTOR_400500_CRAWL_YAW_POSITIVE_X - yawHeading) << 0x14) > 0) {
                requestWork              = task->work;
                requestWork->turnRequest = ACTOR_400500_TURN_YAW_UP;
            } else {
                requestWork              = task->work;
                requestWork->turnRequest = ACTOR_400500_TURN_YAW_DOWN;
            }
            requestWork->turnStarted = 0;
        }
    }
}

/// Chooses the corner's outgoing +Z or -X crawl from the player's zone.
///
/// Requires live Stalker work and current target and slot results. Player zones
/// 2, 3 and 6 select +Z; all others select -X. State changes, recoil and active
/// turns preempt routing. Reaching the chosen yaw restarts the crawl clip.
static void _actor400500TickCrawlCornerChoice(Task* task)
{
    _Actor400500GrayStalkerWork* work;
    _Actor400500GrayStalkerWork* requestWork;
    s32                          knockdownTaken;
    s32                          fallTaken;
    s32                          yawHeading;

    work = task->work;
    // Consume preempting actions before changing the route or pinning its axis.
    knockdownTaken = _actor400500TakeKnockdown(task);
    if (knockdownTaken == 0) {
        ACTOR_400500_TAKE_CEILING_FALL(task, requestWork, fallTaken);
        if ((fallTaken == 0) && ((_actor400500TryStartAttack(task) << 0x10) == 0) &&
            ((_actor400500HandleHeavyHitReaction(task) << 0x10) == 0) &&
            ((_actor400500TickTurn(task) << 0x10) == 0)) {
            if (((u32)(work->playerZone - ACTOR_400500_CRAWL_ZONE_CORNER) < 2U) || ((s16)work->playerZone == ACTOR_400500_CRAWL_ZONE_Z_END)) {
                yawHeading = (u16)work->yaw;
                if ((yawHeading & ACTOR_TRANSFORM_ANGLE_MASK) == ACTOR_400500_CRAWL_YAW_POSITIVE_Z) {
                    requestWork              = task->work;
                    requestWork->animRate    = ACTOR_400500_CRAWL_RATE;
                    requestWork->animId      = ACTOR_400500_ANIM_CRAWL;
                    requestWork->animRequest = ACTOR_400500_ANIM_REQUEST_RESET;
                    work->subState           = ACTOR_400500_CRAWL_STEP_POSITIVE_Z;
                    return;
                }
                if (((ACTOR_400500_CRAWL_YAW_POSITIVE_Z - yawHeading) << 0x14) > 0) {
                    requestWork              = task->work;
                    requestWork->turnRequest = ACTOR_400500_TURN_YAW_UP;
                } else {
                    requestWork              = task->work;
                    requestWork->turnRequest = ACTOR_400500_TURN_YAW_DOWN;
                }
            } else {
                yawHeading = (u16)work->yaw;
                if ((yawHeading & ACTOR_TRANSFORM_ANGLE_MASK) == ACTOR_400500_CRAWL_YAW_NEGATIVE_X) {
                    requestWork              = task->work;
                    requestWork->animRate    = ACTOR_400500_CRAWL_RATE;
                    requestWork->animId      = ACTOR_400500_ANIM_CRAWL;
                    requestWork->animRequest = ACTOR_400500_ANIM_REQUEST_RESET;
                    work->subState           = ACTOR_400500_CRAWL_STEP_NEGATIVE_X;
                    return;
                }
                if (((ACTOR_400500_CRAWL_YAW_NEGATIVE_X - yawHeading) << 0x14) > 0) {
                    requestWork              = task->work;
                    requestWork->turnRequest = ACTOR_400500_TURN_YAW_UP;
                } else {
                    requestWork              = task->work;
                    requestWork->turnRequest = ACTOR_400500_TURN_YAW_DOWN;
                }
            }
            requestWork->turnStarted = 0;
        }
    }
}

/// Crawls toward +Z on the fixed-X route and reverses at the Z end or target.
///
/// Requires live Stalker work/root and current target and slot results. Pending
/// state changes, recoil and active turns preempt movement. The root stays on
/// X = 16500; the caller advances ordinary animation playback.
static void _actor400500TickCrawlPositiveZ(Task* task)
{
    _Actor400500GrayStalkerWork* work;
    _Actor400500GrayStalkerWork* requestWork;
    GfxCoord*                    rootCoord;
    s32                          knockdownTaken;
    s32                          fallTaken;
    s32                          yawHeading;

    work      = task->work;
    rootCoord = task->extra.tmd->coords;
    // Consume preempting actions before changing the route or pinning its axis.
    knockdownTaken = _actor400500TakeKnockdown(task);
    if (knockdownTaken == 0) {
        ACTOR_400500_TAKE_CEILING_FALL(task, requestWork, fallTaken);
        if ((fallTaken == 0) && ((_actor400500TryStartAttack(task) << 0x10) == 0) &&
            ((_actor400500HandleHeavyHitReaction(task) << 0x10) == 0) &&
            ((_actor400500TickTurn(task) << 0x10) == 0)) {
            yawHeading = (u16)work->yaw;
            if (yawHeading & ACTOR_TRANSFORM_ANGLE_MASK) {
                if (((ACTOR_400500_CRAWL_YAW_POSITIVE_Z - yawHeading) << 0x14) > 0) {
                    requestWork              = task->work;
                    requestWork->turnRequest = ACTOR_400500_TURN_YAW_UP;
                } else {
                    requestWork              = task->work;
                    requestWork->turnRequest = ACTOR_400500_TURN_YAW_DOWN;
                }
                requestWork->turnStarted = 0;
            } else {
                if (work->zone != ACTOR_400500_CRAWL_ZONE_Z_SEGMENT) {
                    if (work->zone == ACTOR_400500_CRAWL_ZONE_Z_END) {
                        work->subState = ACTOR_400500_CRAWL_STEP_TURN_NEGATIVE_Z;
                    }
                } else if ((work->attackCooling == 0) && (work->toTarget.vz < 0)) {
                    work->subState = ACTOR_400500_CRAWL_STEP_TURN_NEGATIVE_Z;
                }
                _actor400500TickCrawl(task);
            }
            rootCoord->coord.t[0] = ACTOR_400500_CRAWL_LINE_X;
        }
    }
}

/// Turns the Z-route crawl toward -Z, then restarts the crawl clip.
///
/// Requires live Stalker work and current target and slot results. Pending state
/// changes, recoil and active turns take priority. At the Z-end zone the target's
/// X side chooses the turn; elsewhere the wrapped 12-bit yaw difference does.
static void _actor400500TickCrawlTurnNegativeZ(Task* task)
{
    _Actor400500GrayStalkerWork* work;
    _Actor400500GrayStalkerWork* requestWork;
    s32                          knockdownTaken;
    s32                          fallTaken;
    s32                          yawHeading;

    work = task->work;
    // Consume preempting actions before changing the route or pinning its axis.
    knockdownTaken = _actor400500TakeKnockdown(task);
    if (knockdownTaken == 0) {
        ACTOR_400500_TAKE_CEILING_FALL(task, requestWork, fallTaken);
        if ((fallTaken == 0) && ((_actor400500TryStartAttack(task) << 0x10) == 0) &&
            ((_actor400500HandleHeavyHitReaction(task) << 0x10) == 0) &&
            ((_actor400500TickTurn(task) << 0x10) == 0)) {
            yawHeading = (u16)work->yaw;
            if ((yawHeading & ACTOR_TRANSFORM_ANGLE_MASK) == ACTOR_400500_CRAWL_YAW_NEGATIVE_Z) {
                _actor400500SetAnim(task, ACTOR_400500_ANIM_CRAWL, ACTOR_400500_CRAWL_RATE);
                work->subState = ACTOR_400500_CRAWL_STEP_NEGATIVE_Z;
                return;
            }
            if ((s16)work->playerZone == ACTOR_400500_CRAWL_ZONE_Z_END) {
                if (work->toTarget.vx > 0) {
                    requestWork              = task->work;
                    requestWork->turnRequest = ACTOR_400500_TURN_YAW_UP;
                } else {
                    requestWork              = task->work;
                    requestWork->turnRequest = ACTOR_400500_TURN_YAW_DOWN;
                }
            } else if (((ACTOR_400500_CRAWL_YAW_NEGATIVE_Z - yawHeading) << 0x14) > 0) {
                requestWork              = task->work;
                requestWork->turnRequest = ACTOR_400500_TURN_YAW_UP;
            } else {
                requestWork              = task->work;
                requestWork->turnRequest = ACTOR_400500_TURN_YAW_DOWN;
            }
            requestWork->turnStarted = 0;
        }
    }
}

/// Crawls toward -Z, choosing an X-route turn at the corner or reversing at the target.
///
/// Requires live Stalker work/root and current target and slot results. Pending
/// state changes, recoil and active turns preempt movement. The root stays on
/// X = 16500; the caller advances ordinary animation playback.
static void _actor400500TickCrawlNegativeZ(Task* task)
{
    _Actor400500GrayStalkerWork* work;
    _Actor400500GrayStalkerWork* requestWork;
    GfxCoord*                    rootCoord;
    s32                          knockdownTaken;
    s32                          fallTaken;
    s32                          yawHeading;

    work      = task->work;
    rootCoord = task->extra.tmd->coords;
    // Consume preempting actions before changing the route or pinning its axis.
    knockdownTaken = _actor400500TakeKnockdown(task);
    if (knockdownTaken == 0) {
        ACTOR_400500_TAKE_CEILING_FALL(task, requestWork, fallTaken);
        if ((fallTaken == 0) && ((_actor400500TryStartAttack(task) << 0x10) == 0) &&
            ((_actor400500HandleHeavyHitReaction(task) << 0x10) == 0) &&
            ((_actor400500TickTurn(task) << 0x10) == 0)) {
            yawHeading = (u16)work->yaw;
            if ((yawHeading & ACTOR_TRANSFORM_ANGLE_MASK) != ACTOR_400500_CRAWL_YAW_NEGATIVE_Z) {
                if (((ACTOR_400500_CRAWL_YAW_NEGATIVE_Z - yawHeading) << 0x14) > 0) {
                    requestWork              = task->work;
                    requestWork->turnRequest = ACTOR_400500_TURN_YAW_UP;
                } else {
                    requestWork              = task->work;
                    requestWork->turnRequest = ACTOR_400500_TURN_YAW_DOWN;
                }
                requestWork->turnStarted = 0;
            } else {
                switch (work->zone) {
                    case ACTOR_400500_CRAWL_ZONE_CORNER:
                        if (rootCoord->coord.t[2] < ACTOR_400500_CRAWL_LINE_Z) {
                            work->subState = ACTOR_400500_CRAWL_STEP_CORNER_TURN_NEGATIVE_X;
                        }
                        break;
                    case ACTOR_400500_CRAWL_ZONE_Z_SEGMENT:
                        if ((work->attackCooling == 0) && (work->toTarget.vz > 0)) {
                            work->subState = ACTOR_400500_CRAWL_STEP_TURN_POSITIVE_Z;
                        }
                        break;
                }
                _actor400500TickCrawl(task);
            }
            rootCoord->coord.t[0] = ACTOR_400500_CRAWL_LINE_X;
        }
    }
}

/// Turns from the Z-route corner toward -X, then restarts the X-route crawl.
///
/// Requires live Stalker work and current target and slot results. Pending state
/// changes, recoil and active turns take priority. Yaw uses 4096 units per turn;
/// the signed wrapped 12-bit difference chooses the next requested turn.
static void _actor400500TickCrawlCornerTurnNegativeX(Task* task)
{
    _Actor400500GrayStalkerWork* work;
    _Actor400500GrayStalkerWork* requestWork;
    _Actor400500GrayStalkerWork* increasingTurnWork;
    _Actor400500GrayStalkerWork* decreasingTurnWork;
    s32                          knockdownTaken;
    s32                          fallTaken;
    s32                          yawHeading;

    work = task->work;
    // Consume preempting actions before changing the route or pinning its axis.
    knockdownTaken = _actor400500TakeKnockdown(task);
    if (knockdownTaken == 0) {
        ACTOR_400500_TAKE_CEILING_FALL(task, requestWork, fallTaken);
        if ((fallTaken == 0) && ((_actor400500TryStartAttack(task) << 0x10) == 0) &&
            ((_actor400500HandleHeavyHitReaction(task) << 0x10) == 0) &&
            ((_actor400500TickTurn(task) << 0x10) == 0)) {
            yawHeading = (u16)work->yaw;
            if ((yawHeading & ACTOR_TRANSFORM_ANGLE_MASK) == ACTOR_400500_CRAWL_YAW_NEGATIVE_X) {
                _actor400500SetAnim(task, ACTOR_400500_ANIM_CRAWL, ACTOR_400500_CRAWL_RATE);
                work->subState = ACTOR_400500_CRAWL_STEP_NEGATIVE_X;
                return;
            }
            if (((ACTOR_400500_CRAWL_YAW_NEGATIVE_X - yawHeading) << 0x14) > 0) {
                increasingTurnWork              = task->work;
                increasingTurnWork->turnRequest = ACTOR_400500_TURN_YAW_UP;
                increasingTurnWork->turnStarted = 0;
            } else {
                decreasingTurnWork              = task->work;
                decreasingTurnWork->turnRequest = ACTOR_400500_TURN_YAW_DOWN;
                decreasingTurnWork->turnStarted = 0;
            }
        }
    }
}

/// Turns the Z-route crawl toward +Z, then restarts the crawl clip.
///
/// Requires live Stalker work and current target and slot results. Pending state
/// changes, recoil and active turns take priority. Yaw uses 4096 units per turn;
/// the signed wrapped 12-bit difference chooses the next requested turn.
static void _actor400500TickCrawlTurnPositiveZ(Task* task)
{
    _Actor400500GrayStalkerWork* work;
    _Actor400500GrayStalkerWork* requestWork;
    _Actor400500GrayStalkerWork* increasingTurnWork;
    _Actor400500GrayStalkerWork* decreasingTurnWork;
    s32                          knockdownTaken;
    s32                          fallTaken;
    s32                          yawHeading;

    work = task->work;
    // Consume preempting actions before changing the route or pinning its axis.
    knockdownTaken = _actor400500TakeKnockdown(task);
    if (knockdownTaken == 0) {
        ACTOR_400500_TAKE_CEILING_FALL(task, requestWork, fallTaken);
        if ((fallTaken == 0) && ((_actor400500TryStartAttack(task) << 0x10) == 0) &&
            ((_actor400500HandleHeavyHitReaction(task) << 0x10) == 0) &&
            ((_actor400500TickTurn(task) << 0x10) == 0)) {
            yawHeading = (u16)work->yaw;
            if ((yawHeading & ACTOR_TRANSFORM_ANGLE_MASK) == ACTOR_400500_CRAWL_YAW_POSITIVE_Z) {
                _actor400500SetAnim(task, ACTOR_400500_ANIM_CRAWL, ACTOR_400500_CRAWL_RATE);
                work->subState = ACTOR_400500_CRAWL_STEP_POSITIVE_Z;
                return;
            }
            if (((ACTOR_400500_CRAWL_YAW_POSITIVE_Z - yawHeading) << 0x14) > 0) {
                increasingTurnWork              = task->work;
                increasingTurnWork->turnRequest = ACTOR_400500_TURN_YAW_UP;
                increasingTurnWork->turnStarted = 0;
            } else {
                decreasingTurnWork              = task->work;
                decreasingTurnWork->turnRequest = ACTOR_400500_TURN_YAW_DOWN;
                decreasingTurnWork->turnStarted = 0;
            }
        }
    }
}

#undef ACTOR_400500_TAKE_CEILING_FALL

/// Starts the ceiling grab with a smaller receiving body and a show request.
///
/// Requires initialized Stalker work and rig. Requests showing even during a
/// hide cooldown, cuts to the grab clip at normal rate and advances it once.
/// Clears the catch, reach-angle and tick state, then advances the grab sub-step.
/// The receiving sphere radius is reduced from 608 to 304 part-local units.
static void _actor400500StartGrab(Task* task)
{
    enum { ACTOR_400500_ANIM_GRAB        = 5,
           ACTOR_400500_GRAB_BODY_RADIUS = 304 };
    _Actor400500GrayStalkerWork* work;

    work              = task->work;
    work->body.radius = ACTOR_400500_GRAB_BODY_RADIUS;
    _actor400500RequestCloakFadeIgnoringCooldown(task, ACTOR_400500_CLOAK_RUNNING | ACTOR_400500_CLOAK_SHOW);
    _actor400500SetAnim(task, ACTOR_400500_ANIM_GRAB, ANIMATION_RATE_ONE);
    _actor400500TickAnim(task);
    work->stateFrames   = 0;
    work->playerCaught  = 0;
    work->armReachAngle = 0;
    work->subState      = work->subState + 1;
}

/// Requests a cloak fade when no matching fade is running and its cooldown is zero.
///
/// Requires a live Stalker work block. `cloakRequest` combines
/// `ACTOR_400500_CLOAK_RUNNING` with `HIDE` or `SHOW`; the signed low byte is
/// stored and the phase resets to zero. A matching running request keeps its
/// progress. The cooldown gates both kinds of request; a blocked request is lost.
static inline void _actor400500RequestCloakFade(Task* task, s32 cloakRequest)
{
    _Actor400500GrayStalkerWork* work;

    work = (_Actor400500GrayStalkerWork*)task->work;
    if (((work->cloakRequest >= 0) || ((work->cloakRequest & ACTOR_400500_CLOAK_KIND_MASK) != (cloakRequest & ACTOR_400500_CLOAK_KIND_MASK))) && (work->hideCooldown == 0)) {
        work->cloakRequest = cloakRequest;
        work->cloakPhase   = 0;
    }
}

/// Pre-multiplies and normalizes a Q12 rotation using caller-owned matrix storage.
///
/// Requires separate live, word-aligned matrices; `rotation` is writable and
/// `normalizedRotation` is caller-owned temporary storage. The GTE multiplies
/// `parentRotation * rotation`, then normalization writes the nine signed Q12
/// coefficients. The complete MATRIX copy also transfers the temporary's
/// untouched translation/alignment bytes: only the resulting 3x3 is valid.
/// Retains no pointer and clobbers GTE rotation/product registers.
static inline void _actor400500PreMultiplyNormalizedRotation(const MATRIX* parentRotation, MATRIX* rotation, MATRIX* normalizedRotation)
{
    gte_SetRotMatrix(parentRotation);
    MulRotMatrix(rotation);
    MatrixNormal(rotation, normalizedRotation);
    *rotation = *normalizedRotation;
}

/// Accumulates a coordinate's rotation into world space, excluding the view matrix.
///
/// Requires a live acyclic chain and a separate writable, word-aligned matrix.
/// The local basis seeds the result unchanged; each intervening parent is
/// normalized before multiplication, then the product is normalized as well.
/// Coefficients are Q12. Returns 1 on reaching `gGfxViewCoord`, 0 at NULL,
/// leaving the accumulated basis on either exit. Only the 3x3 is valid: a
/// normalized whole-matrix copy leaves other bytes unspecified. Coordinates
/// and cache stamps stay intact; GTE working registers change.
static inline s32 _actor400500AccumulateWorldRotation(const GfxCoord* coord, MATRIX* worldRotation)
{
    MATRIX          normalizedRotation;
    MATRIX          parentRotation;
    const GfxCoord* ancestor;

    ancestor       = coord->parent;
    *worldRotation = coord->coord;
    while (1) {
        if (ancestor == NULL) {
            return 0;
        }
        if (ancestor == &gGfxViewCoord) {
            return 1;
        }
        parentRotation = ancestor->coord;
        MatrixNormal(&parentRotation, &parentRotation);
        _actor400500PreMultiplyNormalizedRotation(&parentRotation, worldRotation, &normalizedRotation);
        ancestor = ancestor->parent;
    }
}

/// Adds a world-space yaw to a model part and refreshes its composed transform.
///
/// `yawDelta` is narrowed to a signed angle in 4096 units per turn. Requires a
/// writable part with a non-NULL parent and a live acyclic chain reaching the
/// excluded view coordinate. Ancestor bases and products are normalized; the
/// result replaces only the nine Q12 rotation coefficients, preserving local
/// translation and Euler state. Requires the composed view and an initialized,
/// word-aligned scratch stack with room for one MATRIX, released before return.
/// Composition may update ancestor caches and clobbers GTE working registers.
static inline void _actor400500TurnPartWorldYaw(GfxCoord* part, u16 yawDelta)
{
    MATRIX* worldRotation;

    worldRotation = SCRATCH_STACK_RESERVE_BLOCK(MATRIX);
    _actor400500AccumulateWorldRotation(part, worldRotation);
    RotMatrixY((s16)(yawDelta), worldRotation);
    _actor400500LocalizeWorldRotation(part, worldRotation);
    memcpy(part->coord.m, worldRotation->m, sizeof(part->coord.m));
    part->composeStamp = GRAPHICS_COORD_DIRTY;
    actorRenderComposeCoord(part);
    SCRATCH_STACK_RELEASE_BLOCK(MATRIX);
}

/// Returns world X/Z distance from `part` to player part 4, or 32767 without a player.
///
/// Requires live coordinate hierarchies under the composed view, an orthonormal
/// view basis and initialized GTE/scratch facilities. Composes both parts and
/// removes the view transform before measuring in game-coordinate units. Each
/// separation component and the returned length narrow to signed 16 bits;
/// there is no saturation. Y is ignored and no pointer is retained.
static inline s16 _actor400500PlayerDistance(GfxCoord* part)
{
    enum { ACTOR_400500_NO_PLAYER_DISTANCE = 0x7FFF,
           ACTOR_400500_PLAYER_GRAB_PART   = 4 };
    MATRIX    playerWorld;
    MATRIX    partWorld;
    GfxCoord* playerCoords;
    SVECTOR   separation;

    if (gPlayerActorTasks[PLAYER_ACTOR_TASK_PLAYER] == NULL) {
        return ACTOR_400500_NO_PLAYER_DISTANCE;
    }
    playerCoords = gPlayerActorTasks[PLAYER_ACTOR_TASK_PLAYER]->extra.tmd->coords;
    actorRenderComposeCoord(&playerCoords[ACTOR_400500_PLAYER_GRAB_PART]);
    actorRenderComposeCoord(part);
    gfxMakeRelativeTransform(&gGfxViewCoord.workm, &playerCoords[ACTOR_400500_PLAYER_GRAB_PART].workm, &playerWorld);
    gfxMakeRelativeTransform(&gGfxViewCoord.workm, &part->workm, &partWorld);
    separation.vx = (u16)playerWorld.t[0] - (u16)partWorld.t[0];
    separation.vz = (u16)playerWorld.t[2] - (u16)partWorld.t[2];
    return SquareRoot0((separation.vx * separation.vx) + (separation.vz * separation.vz));
}

/// Queues a two-frame blend into a clip at normal playback speed.
///
/// Requires live Stalker work and `setIndex` within its loaded animation table.
/// The index narrows to s16. Playback begins on the next animation tick; this
/// request itself does not update poses or frame counters.
static inline void _actor400500RequestAnimBlend(Task* task, s32 setIndex)
{
    enum { ACTOR_400500_QUICK_BLEND_FRAMES = 2 };
    _Actor400500GrayStalkerWork* work;

    work                  = task->work;
    work->animBlendFrames = ACTOR_400500_QUICK_BLEND_FRAMES;
    work->animRate        = ANIMATION_RATE_ONE;
    work->animId          = setIndex;
    work->animRequest     = ACTOR_400500_ANIM_REQUEST_BLEND;
}

static void func_actor_400500_8013771C(Task* arg0)
{
    enum { ACTOR400500_ATTACK_GRAB = 1 };
    SVECTOR                      in;
    SVECTOR                      out;
    MATRIX                       rot;
    _Actor400500GrayStalkerWork* work;
    _Actor400500GrayStalkerWork* hit;
    GameActor*                   player;
    const Enemy*                 enemy;
    s16                          vz;
    s32                          r;
    s32                          cond;

    work              = (_Actor400500GrayStalkerWork*)arg0->work;
    player            = gPlayerActorTasks[PLAYER_ACTOR_TASK_PLAYER]->work;
    enemy             = arg0->spawnArg2.pointer;
    work->stateFrames = work->stateFrames + 1;
    _actor400500TickAnim(arg0);
    if ((s16)work->stateFrames < 0xF) {
        in.vx = (u16)work->toTarget.vx;
        in.vy = 0;
        vz    = (u16)work->toTarget.vz;
        in.vz = vz;
        gfxSetRotIdentity(&rot);
        RotMatrixY(work->yaw, &rot);
        ApplyMatrixSV(&rot, &in, &out);
        r                    = ratan2(out.vx, work->toTarget.vy - 0x6A0);
        work->armReachAngle += ((-r - (u16)work->armReachAngle) << 20) >> 23;
    } else {
        work->armReachAngle += -((u16)work->armReachAngle << 20) >> 23;
    }
    if ((s16)work->stateFrames == 7) {
        if (player->mode != GAME_ACTOR_MODE_SCRIPTED) {
            if (_actor400500PlayerDistance(&arg0->extra.tmd->coords[ACTOR_400500_PART_RIGHT_ARM_TIP]) < ACTOR_400500_GRAB_RANGE) {
                AnimationPlayRequest msg;

                msg.source.sets          = _gActor400500PlayerAnimationSets;
                msg.animationId          = 1;
                msg.blend                = ANIMATION_BLEND_RESET;
                msg.blendFrames          = 0;
                msg.enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE;
                TASK_MESSAGE_DISPATCH_POINTER(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), ANIMATION_MESSAGE_INSTALL_AND_PLAY, &msg, 0);
                work->grabStep     = 1;
                Gp_StateC08.flags |= ATTACHMENT_FLAG_EVENT_LOCK;
                work->playerCaught = 1;
            }
        }
    }
    if (((s16)work->stateFrames == 0xE) && (work->playerCaught == 0)) {
        _actor400500RequestAnimBlend(arg0, ACTOR_400500_ANIM_GRAB_MISS);
        work->stateFrames = 0;
        _actor400500RequestCloakFade(arg0, ACTOR_400500_CLOAK_RUNNING | ACTOR_400500_CLOAK_HIDE);
        work->subState = work->subState + 1;
    }
    _actor400500TurnPartWorldYaw(&arg0->extra.tmd->coords[6], work->armReachAngle);
    _actor400500TurnPartWorldYaw(&arg0->extra.tmd->coords[9], work->armReachAngle);
    if (((u32)(work->stateFrames - 7) < 4U) && (work->playerCaught == 1)) {
        _actor400500PositionGrabbedPlayer(arg0, ACTOR_400500_GRAB_POSITION_EASE);
    }
    if (((u32)(work->stateFrames - 0xB) < 0x14U) && (work->playerCaught == 1)) {
        _actor400500PositionGrabbedPlayer(arg0, ACTOR_400500_GRAB_POSITION_SNAP);
    }
    if (((s16)work->stateFrames == 0xC) && (work->playerCaught != 0)) {
        _actor400500PlayPlacedSound(arg0, ACTOR_400500_SOUND_GRAB_FRAME_12);
    }
    if ((s16)work->stateFrames == 0x1E) {
        _actor400500PlayPlacedSound(arg0, ACTOR_400500_SOUND_GRAB_FRAME_30);
    }
    if ((s16)work->stateFrames == 0x2A) {
        padScriptSpawnVariableMotorRamp(8, 0xC0U, 8U);
        _actor400500PlayPlacedSound(arg0, ACTOR_400500_SOUND_GRAB_FRAME_42);
    }
    if ((s16)work->stateFrames == 0x1F) {
        padScriptSpawnVariableMotorRamp(6, 0xFFU, 0x80U);
        if (_damageApplyEnemyAttackToPlayer(enemy, ACTOR400500_ATTACK_GRAB) != 0) {
            player->state    = 0xA;
            work->grabLanded = 1;
        }
    }
    hit = (_Actor400500GrayStalkerWork*)arg0->work;
    if ((hit->rig.slots[1].status.fields.flags & ANIMATION_SLOT_REACHED_BOUNDARY) ||
        (hit->rig.slots[1].status.word & (ANIMATION_SLOT_FOLLOWED_JUMP | ANIMATION_SLOT_SETTLED))) {
        cond = 1;
    } else {
        cond = 0;
    }
    if (cond) {
        _actor400500SetState(arg0, ACTOR_400500_STATE_CRAWL, 0);
        work->attackCooldown = 0x3C;
        work->body.radius    = 0x260;
    }
}

/// Retracts the grab reach and returns to crawling when its clip ends.
///
/// Requires initialized work, rig and arm-chain coordinates. Advances playback
/// first, then eases the signed-halfword reach angle toward zero and applies it
/// to parts 6 and 9. A slot-1 boundary, control jump or settled pose restores the
/// 608-unit body radius, requests hiding when allowed and starts a 60-tick cooldown.
static void _actor400500FinishGrabRecovery(Task* task)
{
    enum { ACTOR_400500_RESTORED_BODY_RADIUS = 608 };
    _Actor400500GrayStalkerWork* work;
    s32                          animationEnded;

    work              = task->work;
    work->stateFrames = work->stateFrames + 1;
    _actor400500TickAnim(task);

    work->armReachAngle += -(work->armReachAngle * 16) >> 7;
    _actor400500TurnPartWorldYaw(&task->extra.tmd->coords[6], work->armReachAngle);
    _actor400500TurnPartWorldYaw(&task->extra.tmd->coords[9], work->armReachAngle);

    animationEnded = _actor400500HasAnimBoundary(task);
    if (animationEnded) {
        work->body.radius = ACTOR_400500_RESTORED_BODY_RADIUS;
        _actor400500SetState(task, ACTOR_400500_STATE_CRAWL, ACTOR_400500_CRAWL_STEP_SELECT);
        _actor400500RequestCloakFade(task, ACTOR_400500_CLOAK_RUNNING | ACTOR_400500_CLOAK_HIDE);
        work->attackCooldown = ACTOR_400500_ATTACK_COOLDOWN_TICKS;
    }
}

/// Sub-state handlers `func_actor_400500_8013BA24` copies onto the stack and runs by `subState`.
static const TaskFuncTable3 D_actor_400500_80131EE4 = { {
    _actor400500StartGrab,
    func_actor_400500_8013771C,
    _actor400500FinishGrabRecovery,
} };

/// Sub-state handlers `_actor400500TickCeilingStrikeState` copies onto the stack and runs
/// by `subState` while the enemy is alive.
static const TaskFuncTable3 D_actor_400500_80131EF0 = { {
    _actor400500StartCeilingArmStrike,
    _actor400500ExtendCeilingStrikeArm,
    _actor400500TickCeilingArmStrike,
} };

/// Handlers `_actor400500TickCeilingStrikeState` runs by `deathStep`, instead of the
/// sub-state, once the enemy is out of hit points.
static const TaskFuncTable3 D_actor_400500_80131EFC = { {
    _actor400500StartCeilingStrikeDeathFall,
    _actor400500TickCeilingStrikeDeathFall,
    _actor400500FinishCeilingStrikeDeathLanding,
} };

/// Dispatches the ceiling right-arm strike or its death-fall sequence.
///
/// Requires live work, enemy and rig. Living `subState` and dying `deathStep`
/// each address three steps. A pending knockdown returns before playback;
/// other living ticks handle heavy-hit recoil after the strike step. Death
/// disables all arm contacts; a fatal blast releases the death hold immediately.
/// Playback advances after either branch, including a fatal blast.
static void _actor400500TickCeilingStrikeState(Task* task)
{
    _Actor400500GrayStalkerWork* work;
    Enemy*                       enemy;
    TaskFuncTable3               strikeSteps;
    TaskFuncTable3               deathSteps;

    work        = task->work;
    enemy       = task->spawnArg2.pointer;
    strikeSteps = D_actor_400500_80131EF0;
    deathSteps  = D_actor_400500_80131EFC;
    if (enemy->hp <= 0) {
        if (work->lastHitReaction == ACTOR_400500_HIT_REACTION_BLAST) {
            work->deathHeld = 0;
        } else {
            deathSteps.funcs[(s16)work->deathStep](task);
        }
        _actor400500DisableArmContacts(work);
    } else {
        if (_actor400500TakeKnockdown(task)) {
            return;
        }
        strikeSteps.funcs[(s16)work->subState](task);
        ((_Actor400500GrayStalkerWork*)task->work)->ceilingFallPending = 0;
        _actor400500HandleHeavyHitReaction(task);
    }
    _actor400500TickAnim(task);
}

/// Times the ceiling strike's right-arm contacts and return to crawling.
///
/// Requires the live Stalker work, composed model root, enemy placement and
/// right-arm child. Increments a wrapping halfword tick counter, sounds and
/// enables the two attack spheres at tick 11, and retracts the arm from tick 18.
/// Once fully retracted, hides the child and disables the spheres. An animation
/// boundary returns to crawl, requests a hide when permitted and starts a
/// 60-tick attack cooldown; the caller advances playback.
static void _actor400500TickCeilingArmStrike(Task* task)
{
    _Actor400500GrayStalkerWork* work;
    MATRIX                       unusedRotation; // Untouched storage retained in the handler's stack frame.
    s32                          placedSoundId;
    s32                          animationEnded;
    u16                          strikeFrame;

    work              = task->work;
    strikeFrame       = work->stateFrames + 1;
    work->stateFrames = strikeFrame;
    // Contact activation precedes the timed retraction and animation completion.
    if ((s16)strikeFrame == ACTOR_400500_CEILING_STRIKE_CONTACT_TICK) {
        work->rightArmOuter.flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;
        work->rightArmInner.flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;
        placedSoundId              = ((((Enemy*)task->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | ACTOR_400500_SOUND_ARM_STRIKE;
        _actor400500EnqueueRootSound(task, placedSoundId);
    }
    if ((s16)work->stateFrames >= ACTOR_400500_CEILING_STRIKE_RETURN_TICK) {
        if ((s16)work->armSwingAngle != 0) {
            work->armSwingAngle = (u16)work->armSwingAngle - ACTOR_400500_ARM_RETURN_ANGLE_STEP;
        } else {
            ((_Actor400500GrayStalkerWork*)task->work)->armTasks[ACTOR_400500_ARM_RIGHT]->extra.tmd->flags = TMD_OBJECT_SKIP_ACTIVE_DRAW;
            work->rightArmOuter.flags                                                                     &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
            work->rightArmInner.flags                                                                     &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        }
    }
    animationEnded = _actor400500HasAnimBoundary(task);
    if (animationEnded) {
        _actor400500SetState(task, ACTOR_400500_STATE_CRAWL, ACTOR_400500_CRAWL_STEP_SELECT);
        _actor400500RequestCloakFade(task, ACTOR_400500_CLOAK_RUNNING | ACTOR_400500_CLOAK_HIDE);
        work->attackCooldown = ACTOR_400500_ATTACK_COOLDOWN_TICKS;
    }
}

/// Sub-state handlers `_actor400500TickFloorStrikeState` copies onto the stack and runs by `subState`.
static const TaskFuncTable5 D_actor_400500_80131F08 = { {
    _actor400500StartFloorArmStrike,
    _actor400500ExtendFloorStrikeArm,
    _actor400500TickFloorArmStrike,
    _actor400500RetractFloorStrikeArm,
    _actor400500FinishFloorArmStrike,
} };

/// Dispatches the five-step floor left-arm strike and its hit reactions.
///
/// Requires live work, enemy and rig. A living pending knockdown returns before
/// playback; death releases its hold and disables all arm contacts. Other ticks
/// process heavy-hit recoil, advance playback and clear any ceiling-fall request.
static void _actor400500TickFloorStrikeState(Task* task)
{
    _Actor400500GrayStalkerWork* work;
    Enemy*                       enemy;
    TaskFuncTable5               strikeSteps;

    work        = task->work;
    enemy       = task->spawnArg2.pointer;
    strikeSteps = D_actor_400500_80131F08;
    if (enemy->hp > 0) {
        if (_actor400500TakeKnockdown(task)) {
            return;
        }
        strikeSteps.funcs[(s16)work->subState](task);
    } else {
        work->deathHeld = 0;
        _actor400500DisableArmContacts(work);
    }
    _actor400500HandleHeavyHitReaction(task);
    _actor400500TickAnim(task);
    ((_Actor400500GrayStalkerWork*)task->work)->ceilingFallPending = 0;
}

/// Times the floor strike's left-arm contacts and advances to its recovery step.
///
/// Requires the live Stalker work, composed model root, enemy placement and
/// left-arm child. Increments a wrapping halfword tick counter, sounds and
/// enables the two attack spheres at tick 13, and retracts the arm from tick 16.
/// Once fully retracted, hides the child and disables the spheres. An animation
/// boundary advances the sub-state; the caller advances playback.
static void _actor400500TickFloorArmStrike(Task* task)
{
    _Actor400500GrayStalkerWork* work;
    MATRIX                       unusedRotation; // Untouched storage retained in the handler's stack frame.
    s32                          placedSoundId;
    s32                          animationEnded;
    u16                          strikeFrame;

    work              = task->work;
    strikeFrame       = work->stateFrames + 1;
    work->stateFrames = strikeFrame;
    // Contact activation precedes the timed retraction and animation completion.
    if ((s16)strikeFrame == ACTOR_400500_FLOOR_STRIKE_CONTACT_TICK) {
        work->leftArmOuter.flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;
        work->leftArmInner.flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;
        placedSoundId             = ((((Enemy*)task->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | ACTOR_400500_SOUND_ARM_STRIKE;
        _actor400500EnqueueRootSound(task, placedSoundId);
    }
    if ((s16)work->stateFrames >= ACTOR_400500_FLOOR_STRIKE_RETURN_TICK) {
        if ((s16)work->armSwingAngle != 0) {
            work->armSwingAngle = (u16)work->armSwingAngle - ACTOR_400500_ARM_RETURN_ANGLE_STEP;
        } else {
            ((_Actor400500GrayStalkerWork*)task->work)->armTasks[ACTOR_400500_ARM_LEFT]->extra.tmd->flags = TMD_OBJECT_SKIP_ACTIVE_DRAW;
            work->leftArmOuter.flags                                                                     &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
            work->leftArmInner.flags                                                                     &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        }
    }
    animationEnded = _actor400500HasAnimBoundary(task);
    if (animationEnded) {
        work->subState = work->subState + 1;
    }
}

/// Retracts the left arm model during the floor strike's recovery.
///
/// Requires live Stalker work and the left-arm child. The swing angle wraps as
/// a halfword, while the yaw helper narrows the negated subtraction to s16.
/// Keeps the child visible until the stored angle is nonpositive, then hides
/// it and advances the sub-state. The caller advances body animation.
static void _actor400500RetractFloorStrikeArm(Task* task)
{
    _Actor400500GrayStalkerWork* work;
    Task*                        leftArmTask;
    s32                          swingAngle;

    work                          = task->work;
    swingAngle                    = work->armSwingAngle - ACTOR_400500_ARM_RETURN_ANGLE_STEP;
    work->armSwingAngle           = swingAngle;
    leftArmTask                   = ((_Actor400500GrayStalkerWork*)task->work)->armTasks[ACTOR_400500_ARM_LEFT];
    leftArmTask->extra.tmd->flags = 0;
    _actor400500SetCoordYaw(leftArmTask->extra.tmd->coords, -swingAngle);
    if ((s16)work->armSwingAngle <= 0) {
        ((_Actor400500GrayStalkerWork*)task->work)->armTasks[ACTOR_400500_ARM_LEFT]->extra.tmd->flags = TMD_OBJECT_SKIP_ACTIVE_DRAW;
        work->subState                                                                                = work->subState + 1;
    }
}

/// Ends floor-strike recovery by choosing crawl or a jump back to the ceiling.
///
/// Requires live Stalker work and the latest slot results. At a boundary,
/// requests a hide when allowed, starts the attack cooldown and advances the
/// random generator once: one of four high-word values crawls, three jump.
/// The chosen state's first step runs on a later tick.
static void _actor400500FinishFloorArmStrike(Task* task)
{
    enum { ACTOR_400500_FLOOR_STRIKE_ROUTE_CHANCE_MASK = 3 }; // Crawl on one of four high-word values.
    _Actor400500GrayStalkerWork* boundaryWork;
    _Actor400500GrayStalkerWork* cloakWork;
    s32                          animationEnded;
    s32                          cloakRequest;
    u32                          randomState;

    boundaryWork   = task->work;
    animationEnded = _actor400500HasAnimBoundary(task);
    if (animationEnded) {
        cloakWork = task->work;
        if (((cloakWork->cloakRequest >= 0) || ((u8)cloakWork->cloakRequest & ACTOR_400500_CLOAK_KIND_MASK)) && (cloakWork->hideCooldown == 0)) {
            cloakRequest            = ACTOR_400500_CLOAK_RUNNING | ACTOR_400500_CLOAK_HIDE;
            cloakWork->cloakRequest = cloakRequest;
            cloakWork->cloakPhase   = ACTOR_400500_CLOAK_PHASE_FIRST_RAMP;
        }
        boundaryWork->attackCooldown = ACTOR_400500_ATTACK_COOLDOWN_TICKS;
        randomState                  = (gRandomLcgState * RANDOM_LCG_MULTIPLIER) + RANDOM_LCG_INCREMENT;
        gRandomLcgState              = randomState;
        if (!((randomState >> 16) & ACTOR_400500_FLOOR_STRIKE_ROUTE_CHANCE_MASK)) {
            _actor400500SetState(task, ACTOR_400500_STATE_CRAWL, ACTOR_400500_CRAWL_STEP_SELECT);
            return;
        }
        _actor400500SetState(task, ACTOR_400500_STATE_JUMP, ACTOR_400500_POSTURE_CHANGE_STEP_START);
    }
}

/// Sub-state handlers `_actor400500TickCeilingFallState` copies onto the stack and runs by `subState`.
static const TaskFuncTable4 D_actor_400500_80131F1C = { {
    _actor400500StartCeilingFall,
    _actor400500TickCeilingFall,
    _actor400500FinishCeilingFallLanding,
    _actor400500FinishCeilingFallRecovery,
} };

/// Dispatches the four-step ceiling fall onto the back and recovery.
///
/// Requires initialized work, enemy and rig. A fatal blast releases the death
/// hold and disables arm contacts without advancing playback. Other ticks run
/// the indexed fall step, advance playback and clear active hit/fall requests.
/// Repository sources never set the fall-request byte: this living state has
/// no proven source-side entry; external reachability remains unproven.
static void _actor400500TickCeilingFallState(Task* task)
{
    _Actor400500GrayStalkerWork* work;
    Enemy*                       enemy;
    TaskFuncTable4               fallSteps;
    _Actor400500GrayStalkerWork* reactionWork;

    work      = task->work;
    enemy     = task->spawnArg2.pointer;
    fallSteps = D_actor_400500_80131F1C;
    if ((enemy->hp <= 0) && (work->lastHitReaction == ACTOR_400500_HIT_REACTION_BLAST)) {
        work->deathHeld = 0;
        _actor400500DisableArmContacts(work);
        return;
    }
    fallSteps.funcs[(s16)work->subState](task);
    _actor400500TickAnim(task);
    reactionWork                     = task->work;
    reactionWork->hitTaken           = 0;
    reactionWork->hitReaction        = ACTOR_400500_HIT_REACTION_NONE;
    reactionWork                     = task->work;
    reactionWork->ceilingFallPending = 0;
}

/// Sub-state handlers `_actor400500TickKnockdownState` copies onto the stack and runs by `subState`.
static const TaskFuncTable7 D_actor_400500_80131F2C = { {
    _actor400500StartKnockdownRecoil,
    _actor400500FinishFloorKnockdownRecoil,
    _actor400500FinishFloorKnockdownRecovery,
    _actor400500FinishCeilingKnockdownRecoil,
    _actor400500TickKnockdownCeilingFall,
    _actor400500FinishKnockdownCeilingLanding,
    _actor400500FinishKnockdownCeilingRecovery,
} };

/// Dispatches the seven posture-specific knockdown and recovery steps.
///
/// Requires initialized work and rig, with `subState` in 0..6. The step runs
/// before pending animation requests are applied and slots 1..17 advance.
static void _actor400500TickKnockdownState(Task* task)
{
    _Actor400500GrayStalkerWork* work;
    TaskFuncTable7               recoilSteps;

    work        = task->work;
    recoilSteps = D_actor_400500_80131F2C;
    recoilSteps.funcs[(s16)work->subState](task);
    _actor400500TickAnim(task);
}

/// Starts knockdown recoil for the current ceiling, upright or on-back posture.
///
/// Requires live Stalker work and both arm children. Requests a show and
/// disables their drawing and contacts. Ceiling recoil holds death and enters
/// the ceiling-fall branch; either floor posture enters the floor wait branch.
/// Only animation requests are latched; the caller advances playback.
static void _actor400500StartKnockdownRecoil(Task* task)
{
    _Actor400500GrayStalkerWork* work;
    s32                          cloakRequest;
    u16                          posture;
    u8                           unusedStack[0x30]; // Unaccessed local storage; its original purpose is unproven.

    work = task->work;
    if ((work->cloakRequest >= 0) || (((u8)work->cloakRequest & ACTOR_400500_CLOAK_KIND_MASK) != ACTOR_400500_CLOAK_SHOW)) {
        cloakRequest       = ACTOR_400500_CLOAK_RUNNING | ACTOR_400500_CLOAK_SHOW;
        work->cloakRequest = cloakRequest;
        work->cloakPhase   = ACTOR_400500_CLOAK_PHASE_FIRST_RAMP;
    }
    ((_Actor400500GrayStalkerWork*)task->work)->armTasks[ACTOR_400500_ARM_RIGHT]->extra.tmd->flags = TMD_OBJECT_SKIP_ACTIVE_DRAW;
    work->rightArmOuter.flags                                                                     &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
    work->rightArmInner.flags                                                                     &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
    ((_Actor400500GrayStalkerWork*)task->work)->armTasks[ACTOR_400500_ARM_LEFT]->extra.tmd->flags  = TMD_OBJECT_SKIP_ACTIVE_DRAW;
    work->leftArmOuter.flags                                                                      &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
    posture                                                                                        = work->posture;
    work->leftArmInner.flags                                                                      &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
    if (!(posture & ACTOR_400500_POSTURE_ON_FLOOR)) {
        work->deathHeld = 1;
        _actor400500SetAnim(task, ACTOR_400500_ANIM_UPRIGHT_RECOIL, ANIMATION_RATE_ONE);
        work->subState = ACTOR_400500_RECOIL_STEP_CEILING_WAIT;
    } else if (!(posture & ACTOR_400500_POSTURE_ON_BACK)) {
        _actor400500SetAnim(task, ACTOR_400500_ANIM_UPRIGHT_RECOIL, ANIMATION_RATE_ONE);
        work->subState = ACTOR_400500_RECOIL_STEP_FLOOR_WAIT;
    } else {
        _actor400500SetAnim(task, ACTOR_400500_ANIM_ON_BACK_RECOIL, ANIMATION_RATE_ONE);
        work->subState = ACTOR_400500_RECOIL_STEP_FLOOR_WAIT;
    }
}

/// Clears the current hit/recoil request and pending ceiling fall after a state step.
///
/// Requires live Stalker work. Keeps knockdown and the last hit's reaction
/// available to their separate consumers. No animation or state is changed.
static inline void _actor400500ClearHitAndFallRequests(Task* task)
{
    _Actor400500GrayStalkerWork* work = task->work;

    work->hitTaken           = 0;
    work->hitReaction        = ACTOR_400500_HIT_REACTION_NONE;
    work                     = task->work;
    work->ceilingFallPending = 0;
}

/// Advances the cloaked ambush, then consumes this frame's hit and fall requests.
///
/// Requires live Stalker work, enemy and an initialized rig. `subState` is 0 to start
/// the ambush or 1 to wait for its trigger. The selected step runs before
/// playback; pending knockdown wins over any state that step selected.
static void _actor400500TickAmbushState(Task* task)
{
    _Actor400500GrayStalkerWork* work            = task->work;
    TaskFunc                     stepHandlers[2] = { _actor400500StartAmbush, _actor400500WaitAmbushTrigger };

    stepHandlers[(s16)work->subState](task);
    _actor400500TickAnim(task);
    _actor400500ClearHitAndFallRequests(task);
    _actor400500TakeKnockdown(task);
}

/// Activates the cloaked ambush on nearby target measurements or a damage tick.
///
/// Requires live Stalker work/enemy and composed root. Horizontal target
/// range below 2500 game units takes priority: engages battle, sounds the
/// placed cue, requests revealing and enters ceiling strike. Otherwise,
/// hitTaken equal to 1 sounds and reveals into ordinary crawl. Reveal keeps
/// an already running show and bypasses hide cooldown in both branches.
static void _actor400500WaitAmbushTrigger(Task* task)
{
    enum { ACTOR_400500_AMBUSH_TRIGGER_RANGE = 2500 };
    _Actor400500GrayStalkerWork* work;
    _Actor400500GrayStalkerWork* proximityCloakWork;
    _Actor400500GrayStalkerWork* hitCloakWork;
    s16                          hitTaken;
    s32                          cloakRequest;

    work = task->work;
    // Proximity wins even when a damage tick is also pending.
    if (work->targetDist < ACTOR_400500_AMBUSH_TRIGGER_RANGE) {
        sceneEngageBattle(1);
        _actor400500PlayPlacedSound(task, ACTOR_400500_SOUND_POSTURE_CHANGE);
        proximityCloakWork = task->work;
        if ((proximityCloakWork->cloakRequest >= 0) || (((u8)proximityCloakWork->cloakRequest & ACTOR_400500_CLOAK_KIND_MASK) != ACTOR_400500_CLOAK_SHOW)) {
            cloakRequest                     = ACTOR_400500_CLOAK_RUNNING | ACTOR_400500_CLOAK_SHOW;
            proximityCloakWork->cloakRequest = cloakRequest;
            proximityCloakWork->cloakPhase   = ACTOR_400500_CLOAK_PHASE_FIRST_RAMP;
        }
        _actor400500SetState(task, ACTOR_400500_STATE_STRIKE_CEILING, 0);
        return;
    }
    hitTaken = work->hitTaken;
    if (hitTaken == 1) {
        _actor400500PlayPlacedSound(task, ACTOR_400500_SOUND_POSTURE_CHANGE);
        hitCloakWork = task->work;
        if ((hitCloakWork->cloakRequest >= 0) || (((u8)hitCloakWork->cloakRequest & ACTOR_400500_CLOAK_KIND_MASK) != hitTaken)) {
            cloakRequest               = ACTOR_400500_CLOAK_RUNNING | ACTOR_400500_CLOAK_SHOW;
            hitCloakWork->cloakRequest = cloakRequest;
            hitCloakWork->cloakPhase   = ACTOR_400500_CLOAK_PHASE_FIRST_RAMP;
        }
        _actor400500SetState(task, ACTOR_400500_STATE_CRAWL, ACTOR_400500_CRAWL_STEP_SELECT);
    }
}

/// Sub-state handlers `_actor400500TickDropState` copies onto the stack and runs by `subState`.
static const TaskFuncTable3 D_actor_400500_80131F48 = { {
    _actor400500StartDropFromCeiling,
    _actor400500TickDropFromCeiling,
    _actor400500FinishDropLanding,
} };

/// Dispatches the deliberate ceiling drop and its floor landing.
///
/// Requires live Stalker work, enemy and rig; `subState` indexes the three
/// drop steps. Playback advances before dispatch except on the start step,
/// whose request first plays next tick. Clears hit and ceiling-fall requests
/// after dispatch; the landing step handles knockdown and the death hold.
static void _actor400500TickDropState(Task* task)
{
    _Actor400500GrayStalkerWork* work;
    TaskFuncTable3               stepHandlers;

    work         = task->work;
    stepHandlers = D_actor_400500_80131F48;
    if ((s16)work->subState != ACTOR_400500_POSTURE_CHANGE_STEP_START) {
        _actor400500TickAnim(task);
    }
    stepHandlers.funcs[(s16)work->subState](task);
    _actor400500ClearHitAndFallRequests(task);
}

/// Rebuilds the drop landing's root basis from its normalized Euler angles.
///
/// Requires live Stalker work and the model root. Angles use 4096ths of a turn;
/// the new basis has Q12 unit scale. Copies only its nine rotation coefficients,
/// preserving translation, alignment bytes and the root's composition stamp.
/// The caller marks the root dirty and refreshes it after applying the pose.
static inline void _actor400500RebuildDropLandingRotation(Task* task)
{
    MATRIX                       rotation;
    _Actor400500GrayStalkerWork* work = task->work;
    GfxCoord*                    root = task->extra.tmd->coords;

    work->pitch &= ACTOR_TRANSFORM_ANGLE_MASK;
    work->yaw   &= ACTOR_TRANSFORM_ANGLE_MASK;
    work->roll  &= ACTOR_TRANSFORM_ANGLE_MASK;
    gfxSetRotIdentity(&rotation);
    RotMatrixZ(work->roll, &rotation);
    RotMatrixX(work->pitch, &rotation);
    RotMatrixY(work->yaw, &rotation);
    _actor400500CopyRotation(&rotation, &root->coord);
}

/// Drops from the ceiling while returning the body's world X/Z to the drop origin.
///
/// Requires live Stalker work, the 18-part model rooted directly beneath the
/// composed view, and initialized animation. Samples part 3 for seven ticks;
/// thereafter eases its signed-halfword anchor toward the saved drop position
/// and moves the root around it. Acceleration and speed wrap as halfwords.
/// On crossing the floor, resets pitch/roll, reverses yaw and starts the
/// landing pose, advancing playback immediately before refreshing the root.
static void _actor400500TickDropFromCeiling(Task* task)
{
    enum { ACTOR_400500_DROP_MOVE_TICK  = 8,
           ACTOR_400500_DROP_PITCH_STEP = 128 };
    _Actor400500GrayStalkerWork* work;
    GfxCoord*                    rootCoord;
    s32                          anchorX;
    s32                          anchorZ;
    s32                          landingX;
    s32                          landingZ;
    u16                          accelerationBits;
    u16                          speedBits;
    u16                          pitch;
    s32                          rootY;

    work      = task->work;
    rootCoord = task->extra.tmd->coords;
    // Sample the body before the downward motion begins.
    if ((s16)++work->stateFrames < ACTOR_400500_DROP_MOVE_TICK) {
        _actor400500ReadPartWorldXZ(task, ACTOR_400500_PART_BODY, &work->anchorPos);
        return;
    }

    // Move the root around the body anchor as it eases toward the drop origin.
    landingX           = work->dropStartX;
    anchorX            = work->anchorPos.vx;
    landingZ           = work->dropStartZ;
    anchorZ            = work->anchorPos.vz;
    work->anchorPos.vx = (u16)work->anchorPos.vx + ((landingX - anchorX) >> 2);
    work->anchorPos.vz = (u16)work->anchorPos.vz + ((landingZ - anchorZ) >> 2);
    _actor400500PinPartWorldXZ(task, ACTOR_400500_PART_BODY, &work->anchorPos);
    accelerationBits      = (u16)work->moveAccel + 2;
    speedBits             = (u16)work->moveSpeed + accelerationBits;
    work->moveSpeed       = speedBits;
    work->moveAccel       = accelerationBits;
    rootY                 = rootCoord->coord.t[1] + (s16)speedBits;
    rootCoord->coord.t[1] = rootY;
    pitch                 = work->pitch;
    if ((pitch & ACTOR_TRANSFORM_ANGLE_MASK) != ACTOR_TRANSFORM_ANGLE_HALF_TURN) {
        work->pitch = pitch - ACTOR_400500_DROP_PITCH_STEP;
    }

    // Apply the landing pose now so the refreshed root uses it this tick.
    if (rootCoord->coord.t[1] >= ACTOR_400500_FLOOR_Y + 1) {
        rootY                 = ACTOR_400500_FLOOR_Y;
        rootCoord->coord.t[0] = work->dropStartX;
        rootCoord->coord.t[2] = work->dropStartZ;
        rootCoord->coord.t[1] = rootY;
        work->subState        = work->subState + 1;
        rootCoord->coord.t[1] = rootY;
        work->pitch           = 0;
        work->roll            = 0;
        work->yaw             = (u16)work->yaw + ACTOR_TRANSFORM_ANGLE_HALF_TURN;
        _actor400500RebuildDropLandingRotation(task);
        _actor400500SetAnim(task, ACTOR_400500_ANIM_POSTURE_SETTLE, ANIMATION_RATE_ONE);
        _actor400500TickAnim(task);
        rootCoord->composeStamp = GRAPHICS_COORD_DIRTY;
        actorRenderComposeCoord(rootCoord);
        work->stateFrames = 0;
    }
}

/// Finishes the deliberate drop's landing and returns to upright floor crawling.
///
/// Requires the live enemy/work, composed model root and latest slot results.
/// Sounds on the first tick, consumes knockdown before testing the animation
/// boundary, and sets the floor posture when crawling resumes. A dead enemy
/// releases the death hold without a sound or state change.
static void _actor400500FinishDropLanding(Task* task)
{
    _Actor400500GrayStalkerWork* work;
    Enemy*                       enemy;
    s32                          placedSoundId;
    s32                          knockdownTaken;
    s32                          animationEnded;

    enemy = task->spawnArg2.pointer;
    work  = task->work;
    if (enemy->hp > 0) {
        if ((s16)++work->stateFrames == 1) {
            placedSoundId = ((((Enemy*)task->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | ACTOR_400500_SOUND_DROP_LANDING;
            _actor400500EnqueueRootSound(task, placedSoundId);
        }
        knockdownTaken = _actor400500TakeKnockdown(task);
        if (knockdownTaken == 0) {
            animationEnded = _actor400500HasAnimBoundary(task);
            if (animationEnded) {
                _actor400500SetState(task, ACTOR_400500_STATE_CRAWL, ACTOR_400500_CRAWL_STEP_SELECT);
                work->posture |= ACTOR_400500_POSTURE_ON_FLOOR;
            }
        }
    } else {
        work->deathHeld = 0;
    }
}

/// Sub-state handlers `_actor400500TickJumpState` copies onto the stack and runs by `subState`.
static const TaskFuncTable3 D_actor_400500_80131F54 = { {
    _actor400500StartJumpToCeiling,
    _actor400500TickJumpToCeiling,
    _actor400500FinishJumpToCeiling,
} };

/// Dispatches the floor-to-ceiling jump and advances its requested pose.
///
/// Requires live Stalker work, enemy and rig; `subState` indexes the three
/// jump steps. Playback advances after dispatch. Clears hit and ceiling-fall
/// requests afterward; the jump's wind-up handles death and knockdown.
static void _actor400500TickJumpState(Task* task)
{
    _Actor400500GrayStalkerWork* work;
    TaskFuncTable3               stepHandlers;

    work         = task->work;
    stepHandlers = D_actor_400500_80131F54;
    stepHandlers.funcs[(s16)work->subState](task);
    _actor400500TickAnim(task);
    _actor400500ClearHitAndFallRequests(task);
}

/// Advances the floor-to-ceiling jump and starts its ceiling landing pose.
///
/// Requires live enemy/work and the model root. The first eight ticks permit
/// death or knockdown to interrupt. Thereafter halfword acceleration/speed
/// drive root Y while pitch approaches half a turn. Crossing the ceiling
/// clamps Y, reverses yaw, restores the ceiling roll and sounds both contacts.
/// The caller advances animation and the next step clears the floor posture.
static void _actor400500TickJumpToCeiling(Task* task)
{
    enum { ACTOR_400500_JUMP_MOVE_TICK  = 9,
           ACTOR_400500_JUMP_PITCH_STEP = 152 };
    _Actor400500GrayStalkerWork* work;
    _Actor400500GrayStalkerWork* transitionWork;
    GfxCoord*                    rootCoord;
    Enemy*                       enemy;
    u16                          accelerationBits;
    u16                          speedBits;
    s32                          rootY;
    s16                          nextPitch;
    s32                          firstPlacedSoundId;
    s32                          secondPlacedSoundId;
    rootCoord = task->extra.tmd->coords;

    work  = task->work;
    enemy = task->spawnArg2.pointer;
    // Only the wind-up can be interrupted; flight completes even after a kill.
    if ((s16)++work->stateFrames < ACTOR_400500_JUMP_MOVE_TICK) {
        if (enemy->hp <= 0) {
            work->deathHeld = 0;
            return;
        }
        transitionWork = task->work;
        if (transitionWork->knockdownPending != 0) {
            transitionWork->knockdownPending = 0;
            _actor400500EnterState(task, ACTOR_400500_STATE_KNOCKDOWN);
        }
    } else {
        accelerationBits      = (u16)work->moveAccel - 2;
        speedBits             = (u16)work->moveSpeed + accelerationBits;
        work->moveSpeed       = speedBits;
        work->moveAccel       = accelerationBits;
        rootY                 = rootCoord->coord.t[1] - (s16)speedBits;
        rootCoord->coord.t[1] = rootY;
        if (work->pitch < ACTOR_TRANSFORM_ANGLE_HALF_TURN) {
            nextPitch   = (u16)work->pitch + ACTOR_400500_JUMP_PITCH_STEP;
            work->pitch = nextPitch;
            if (nextPitch >= ACTOR_TRANSFORM_ANGLE_HALF_TURN + 1) {
                work->pitch = ACTOR_TRANSFORM_ANGLE_HALF_TURN;
            }
        }
        if (rootCoord->coord.t[1] < ACTOR_400500_CRAWL_CEILING_Y) {
            rootCoord->coord.t[1] = ACTOR_400500_CRAWL_CEILING_Y;
            work->subState        = work->subState + 1;
            rootCoord->coord.t[1] = ACTOR_400500_CRAWL_CEILING_Y;
            work->pitch           = 0;
            work->roll            = ACTOR_TRANSFORM_ANGLE_HALF_TURN;
            work->yaw             = (u16)work->yaw + ACTOR_TRANSFORM_ANGLE_HALF_TURN;
            _actor400500SetAnim(task, ACTOR_400500_ANIM_POSTURE_SETTLE, ANIMATION_RATE_ONE);
            firstPlacedSoundId = ((((Enemy*)task->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | ACTOR_400500_SOUND_STEP_1;
            _actor400500EnqueueRootSound(task, firstPlacedSoundId);
            secondPlacedSoundId = ((((Enemy*)task->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | ACTOR_400500_SOUND_STEP_2;
            _actor400500EnqueueRootSound(task, secondPlacedSoundId);
        }
    }
}

/// Dispatches righting the Stalker from its on-back floor posture.
///
/// Requires live Stalker work, enemy and rig. `subState` is 0 to start the
/// turn-over or 1 to finish it. Advances playback after dispatch and clears
/// hit and ceiling-fall requests; the finishing step handles knockdown.
static void _actor400500TickTurnOverState(Task* task)
{
    _Actor400500GrayStalkerWork* work            = task->work;
    TaskFunc                     stepHandlers[2] = { _actor400500StartTurnOver, _actor400500TickTurnOver };

    stepHandlers[(s16)work->subState](task);
    _actor400500TickAnim(task);
    _actor400500ClearHitAndFallRequests(task);
}

/// Turn-over pivot coordinate and signed tick sentinel shared by its two steps.
enum { ACTOR_400500_TURN_OVER_PIVOT_PART = 14,
       ACTOR_400500_TURN_OVER_PIN_TICK   = -1 };

/// Rebuilds the turn-over root's Q12 basis from normalized Z/X/Y Euler angles.
///
/// Requires live work/root. Angles use 4096ths of a turn; only nine rotation
/// coefficients are copied. Translation, alignment bytes and cache stamps
/// remain intact, so the caller dirties and composes the root after its pose.
static inline void _actor400500RebuildTurnOverRotation(Task* task)
{
    MATRIX                       rotation;
    _Actor400500GrayStalkerWork* work = task->work;
    GfxCoord*                    root = task->extra.tmd->coords;

    work->pitch &= ACTOR_TRANSFORM_ANGLE_MASK;
    work->yaw   &= ACTOR_TRANSFORM_ANGLE_MASK;
    work->roll  &= ACTOR_TRANSFORM_ANGLE_MASK;
    gfxSetRotIdentity(&rotation);
    RotMatrixZ(work->roll, &rotation);
    RotMatrixX(work->pitch, &rotation);
    RotMatrixY(work->yaw, &rotation);
    _actor400500CopyRotation(&rotation, &root->coord);
}

/// Finishes the on-back turn-over pose and chooses the next living state.
///
/// Requires live work, enemy, initialized rig and an 18-part model beneath the
/// composed view. A signed tick sentinel of -1 pins part 14 to the saved world
/// X/Z anchor. At a slot-1 boundary, reverses yaw by half a turn and rebuilds
/// the root basis. A living enemy clears ON_BACK and advances the hold pose,
/// then consumes knockdown or chooses crawl/jump with equal high-word chances.
/// A dead enemy releases its hold without clearing ON_BACK or advancing the rig.
static void _actor400500TickTurnOver(Task* task)
{
    enum { ACTOR_400500_TURN_OVER_ROUTE_CHANCE_MASK = 1 };
    _Actor400500GrayStalkerWork* work;
    GfxCoord*                    rootCoord;
    Enemy*                       enemy;
    s32                          animationEnded;
    s32                          knockdownTaken;
    s32                          pinSentinel;
    u32                          randomState;

    pinSentinel = ACTOR_400500_TURN_OVER_PIN_TICK;
    rootCoord   = task->extra.tmd->coords;
    work        = task->work;
    enemy       = task->spawnArg2.pointer;
    if ((s16)work->stateFrames == pinSentinel) {
        // Hold the planted foot while the root turns around its world X/Z.
        _actor400500PinPartWorldXZ(task, ACTOR_400500_TURN_OVER_PIVOT_PART, &work->anchorPos);
    }
    animationEnded = _actor400500HasAnimBoundary(task);
    if (animationEnded) {
        // Replace the flipped basis before applying the living or dying pose.
        if (enemy->hp > 0) {
            work->posture &= ~ACTOR_400500_POSTURE_ON_BACK;
            work->yaw      = ((u16)work->yaw + ACTOR_TRANSFORM_ANGLE_HALF_TURN) & ACTOR_TRANSFORM_ANGLE_MASK;
            _actor400500RebuildTurnOverRotation(task);
            _actor400500SetAnim(task, ACTOR_400500_ANIM_CEILING_HOLD, ANIMATION_RATE_ONE);
            _actor400500TickAnim(task);
            rootCoord->composeStamp = GRAPHICS_COORD_DIRTY;
            actorRenderComposeCoord(rootCoord);
            knockdownTaken = _actor400500TakeKnockdown(task);
            if (knockdownTaken == 0) {
                randomState     = (gRandomLcgState * RANDOM_LCG_MULTIPLIER) + RANDOM_LCG_INCREMENT;
                gRandomLcgState = randomState;
                if (((randomState >> 16) & ACTOR_400500_TURN_OVER_ROUTE_CHANCE_MASK) == 0) {
                    _actor400500SetState(task, ACTOR_400500_STATE_CRAWL, ACTOR_400500_CRAWL_STEP_SELECT);
                    return;
                }
                _actor400500SetState(task, ACTOR_400500_STATE_JUMP, ACTOR_400500_POSTURE_CHANGE_STEP_START);
            }
        } else {
            work->deathHeld = 0;
            work->yaw       = ((u16)work->yaw + ACTOR_TRANSFORM_ANGLE_HALF_TURN) & ACTOR_TRANSFORM_ANGLE_MASK;
            _actor400500RebuildTurnOverRotation(task);
            rootCoord->composeStamp = GRAPHICS_COORD_DIRTY;
            actorRenderComposeCoord(rootCoord);
        }
    }
}

/// Sub-state handlers `_actor400500TickStatusHoldState` copies onto the stack and runs by `subState`.
static const TaskFuncTable7 D_actor_400500_80131F60 = { {
    _actor400500StartStatusHoldRecoil,
    _actor400500FinishFloorStatusHoldRecoil,
    _actor400500FinishFloorStatusHoldRecovery,
    _actor400500FinishCeilingStatusHoldRecoil,
    _actor400500TickStatusHoldCeilingFall,
    _actor400500FinishStatusHoldCeilingLanding,
    _actor400500FinishStatusHoldCeilingRecovery,
} };

/// Dispatches status-held recoil and its posture-dependent recovery.
///
/// Requires live Stalker work, enemy and rig; `subState` indexes the seven
/// recoil/fall/recovery steps. Playback advances after dispatch. The steps
/// consume status buildup, manage the death hold and select the next crawl.
static void _actor400500TickStatusHoldState(Task* task)
{
    _Actor400500GrayStalkerWork* work;
    TaskFuncTable7               stepHandlers;

    work         = task->work;
    stepHandlers = D_actor_400500_80131F60;
    stepHandlers.funcs[(s16)work->subState](task);
    _actor400500TickAnim(task);
}

/// Starts status-held recoil for the current ceiling, upright or on-back posture.
///
/// Requires live Stalker work and both arm children. Shows the body and disables
/// arm drawing and contacts. The floor branch waits for status buildup to end;
/// the ceiling branch holds death while its recoil proceeds into a fall.
/// Only animation requests are latched; the caller advances playback.
static void _actor400500StartStatusHoldRecoil(Task* task)
{
    _Actor400500GrayStalkerWork* work;
    s32                          cloakRequest;
    u16                          posture;
    u8                           unusedStack[0x30]; // Unaccessed local storage; its original purpose is unproven.

    work = task->work;
    if ((work->cloakRequest >= 0) || (((u8)work->cloakRequest & ACTOR_400500_CLOAK_KIND_MASK) != ACTOR_400500_CLOAK_SHOW)) {
        cloakRequest       = ACTOR_400500_CLOAK_RUNNING | ACTOR_400500_CLOAK_SHOW;
        work->cloakRequest = cloakRequest;
        work->cloakPhase   = ACTOR_400500_CLOAK_PHASE_FIRST_RAMP;
    }
    ((_Actor400500GrayStalkerWork*)task->work)->armTasks[ACTOR_400500_ARM_RIGHT]->extra.tmd->flags = TMD_OBJECT_SKIP_ACTIVE_DRAW;
    work->rightArmOuter.flags                                                                     &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
    work->rightArmInner.flags                                                                     &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
    ((_Actor400500GrayStalkerWork*)task->work)->armTasks[ACTOR_400500_ARM_LEFT]->extra.tmd->flags  = TMD_OBJECT_SKIP_ACTIVE_DRAW;
    work->leftArmOuter.flags                                                                      &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
    posture                                                                                        = work->posture;
    work->leftArmInner.flags                                                                      &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
    if (!(posture & ACTOR_400500_POSTURE_ON_FLOOR)) {
        work->deathHeld = 1;
        _actor400500SetAnim(task, ACTOR_400500_ANIM_UPRIGHT_RECOIL, ANIMATION_RATE_ONE);
        work->subState = ACTOR_400500_RECOIL_STEP_CEILING_WAIT;
    } else if (!(posture & ACTOR_400500_POSTURE_ON_BACK)) {
        _actor400500SetAnim(task, ACTOR_400500_ANIM_UPRIGHT_RECOIL, ANIMATION_RATE_ONE);
        work->subState = ACTOR_400500_RECOIL_STEP_FLOOR_WAIT;
    } else {
        _actor400500SetAnim(task, ACTOR_400500_ANIM_ON_BACK_RECOIL, ANIMATION_RATE_ONE);
        work->subState = ACTOR_400500_RECOIL_STEP_FLOOR_WAIT;
    }
}

/// State handlers `func_actor_400500_8013A700` copies onto the stack and runs
/// by `state`.
static const TaskFuncTable10 D_actor_400500_80131F7C = { {
    _actor400500StartDeath,
    _actor400500WaitDeathAnimation,
    _actor400500PrepareDeathShrink,
    _actor400500WaitDeathShrinkDelay,
    func_actor_400500_8013ABE4,
    _actor400500BeginTeardown,
    _actor400500HideBurstBody,
    _actor400500WaitBurstBufferDelay,
    func_actor_400500_8013DA68,
    _actor400500BeginBurstTeardown,
} };

/// Updates actor lighting at a cached coordinate position and applies the room's ambient tint.
///
/// Requires a live enemy/model with writable lighting matrices. Samples the
/// cache exactly as stored; does not compose or convert `coord`.
/// Ambient channels use Q12: grey 1/8 in rooms 1/3/5/6, otherwise green with
/// red/blue 1/4. Borrows one VECTOR scratch block plus the lighting query's
/// nested reservations, releasing it before return; changes GTE state.
static inline void _actor400500UpdateColor(Task* task, const GfxCoord* coord, const TmdObject* model)
{
    enum { ACTOR_400500_AMBIENT_GREY           = ONE / 8,
           ACTOR_400500_AMBIENT_GREEN_RED_BLUE = ONE / 4 };
    VECTOR* position;
    u8      room;

    position = _actor400500PushCachedPosition(coord);
    worldCoordUpdateActorColor(task->spawnArg2.pointer, position, 0, 0);
    room = gGameSession->location.loc.room;
    if ((room == 1) || (room == 3) || (room == 5) || (room == 6)) {
        worldCoordSetModelAmbientColor(model, ACTOR_400500_AMBIENT_GREY, ACTOR_400500_AMBIENT_GREY, ACTOR_400500_AMBIENT_GREY);
    } else {
        worldCoordSetModelAmbientColor(model, ACTOR_400500_AMBIENT_GREEN_RED_BLUE, ONE, ACTOR_400500_AMBIENT_GREEN_RED_BLUE);
    }
    SCRATCH_STACK_RELEASE_BLOCK(VECTOR);
}

static void func_actor_400500_8013A700(Task* arg0)
{
    TmdObject*                   extra;
    _Actor400500GrayStalkerWork* work;
    TaskFuncTable10              sp;

    extra = arg0->extra.tmd;
    work  = (_Actor400500GrayStalkerWork*)arg0->work;
    sp    = D_actor_400500_80131F7C;
    switch (gSceneCombatState.actorControl) {
        case SCENE_COMBAT_ACTORS_HIDDEN:
            extra->flags |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
            return;
        case SCENE_COMBAT_ACTORS_RUNNING:
            sp.funcs[(s16)work->state](arg0);
        case SCENE_COMBAT_ACTORS_PAUSED:
            _actor400500UpdateColor(arg0, &arg0->extra.tmd->coords[1], arg0->extra.tmd);
            if (work->shadowShade != 0) {
                _actor400500DrawLimbShadows(arg0, -0xFA0, (u8)(work->shadowShade >> 2));
                _actor400500DrawLimbShadows(arg0, -0x3E8, (u8)work->shadowShade);
            }
            return;
    }
}

/// Starts the death pose, releases battle participation and selects the death path.
///
/// Requires the live enemy, work and initialized rig. The current animation
/// must index the 33-entry death-pose map (0..32); its mapped clip starts at
/// normal rate and ticks immediately. Unlinks the target and all five bodies,
/// clears borrowed hit records and records defeat. A blast skips the pose
/// wait/shrink path and starts the body-hide/burst path instead.
static void _actor400500StartDeath(Task* task)
{
    enum { ACTOR_400500_DEATH_STEP_HIDE_BODY = 6 };
    _Actor400500GrayStalkerWork* work;
    _Actor400500GrayStalkerWork* transitionWork;
    Enemy*                       enemy;
    s32                          deathAnimId;

    work              = task->work;
    enemy             = task->spawnArg2.pointer;
    deathAnimId       = D_actor_400500_80153DD4[work->animId];
    work->animRate    = ANIMATION_RATE_ONE;
    work->animRequest = ACTOR_400500_ANIM_REQUEST_RESET;
    work->animId      = deathAnimId;
    _actor400500TickAnim(task);
    // End target/contact participation and award the kill before choosing its death path.
    worldTargetUnlinkNode(&enemy->node);
    sceneReleaseBattleRefWithRewards(task, 0);
    enemy->recs = 0;
    worldCollisionUnlinkBody(&work->body);
    worldCollisionUnlinkBody(&work->rightArmOuter);
    worldCollisionUnlinkBody(&work->leftArmOuter);
    worldCollisionUnlinkBody(&work->rightArmInner);
    worldCollisionUnlinkBody(&work->leftArmInner);
    gameFlagSetNibble(GAME_FLAG_GRAY_STALKER_DEFEATED, 1);
    if (work->lastHitReaction == ACTOR_400500_HIT_REACTION_BLAST) {
        transitionWork           = task->work;
        transitionWork->state    = ACTOR_400500_DEATH_STEP_HIDE_BODY;
        transitionWork->subState = 0;
        return;
    }
    work->state = work->state + 1;
}

/// Advances the death pose until slot 1 reaches an animation boundary.
///
/// Requires live Stalker work and initialized playback. Advances slots 1..17
/// before reading slot 1; a reached boundary, followed jump or settled pose
/// advances the death state to shrink preparation.
static void _actor400500WaitDeathAnimation(Task* task)
{
    _Actor400500GrayStalkerWork* work;
    s32                          animationEnded;

    work = task->work;
    _actor400500TickAnim(task);
    animationEnded = _actor400500HasAnimBoundary(task);
    if (animationEnded) {
        work->state = work->state + 1;
    }
}

static void func_actor_400500_8013ABE4(Task* arg0)
{
    _Actor400500GrayStalkerWork* work;
    TmdObject*                   model;
    GfxCoord*                    coord;
    VECTOR                       scale;
    SVECTOR                      pos;
    u16                          frame;

    work  = (_Actor400500GrayStalkerWork*)arg0->work;
    model = arg0->extra.tmd;
    coord = model->coords;

    work->cloakLevel          = (u16)work->cloakLevel + ((s16)(0xFF - (u16)work->cloakLevel) >> 4);
    work->colorBlend          = (u16)work->colorBlend + ((s16) - (u16)work->colorBlend >> 4);
    work->shadowShade         = (u16)work->shadowShade + (-work->shadowShade >> 4);
    model->shading.colorBlend = work->colorBlend;
    modelLightingSetLayerMaterials(work->cloakLevel);

    work->shrinkScaleY = (u16)work->shrinkScaleY - 0x30;
    scale.vx           = 0x1000;
    scale.vy           = work->shrinkScaleY;
    scale.vz           = 0x1000;
    coord->coord       = work->savedRootMtx;
    ScaleMatrix(&coord->coord, &scale);
    coord->composeStamp = GRAPHICS_COORD_DIRTY;

    frame             = work->stateFrames + 1;
    work->stateFrames = frame;
    if ((s16)frame == 0x10) {
        pos.vx = 0;
        pos.vy = 0;
        pos.vz = 0;
        effectSpawn(EFFECT_CORPSE_BURN, coord, 5, &pos);
    }
    if ((s16)work->stateFrames >= 0x41) {
        model->flags |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
        work->state   = work->state + 1;
    }
}

/// Sub-state handlers `_actor400500TickFallenCrawlState` copies onto the stack and runs by `subState`.
static const TaskFuncTable11 D_actor_400500_80131FA4 = { {
    _actor400500SelectFallenCrawlRoute,
    _actor400500TickFallenCrawlPositiveX,
    _actor400500CheckFallenCrawlNegativeXHeading,
    _actor400500TickFallenCrawlFacingNegativeX,
    _actor400500CheckFallenCrawlPositiveXHeading,
    _actor400500TickFallenCrawlCornerChoice,
    _actor400500TickFallenCrawlFacingPositiveZ,
    _actor400500CheckFallenCrawlNegativeZHeading,
    _actor400500TickFallenCrawlFacingNegativeZ,
    _actor400500CheckFallenCrawlCornerNegativeXHeading,
    _actor400500CheckFallenCrawlPositiveZHeading,
} };

/// Dispatches crawling on the back and requests cloaking when cooldown permits.
///
/// Requires live Stalker work, enemy and rig; `subState` indexes the eleven
/// fallen-route steps. Playback advances before dispatch except at route
/// selection. At nonpositive HP it releases the death hold instead of moving,
/// then still attempts the cooldown-gated hide request.
static void _actor400500TickFallenCrawlState(Task* task)
{
    _Actor400500GrayStalkerWork* work;
    TaskFuncTable11              stepHandlers;
    Enemy*                       enemy;

    work         = task->work;
    enemy        = task->spawnArg2.pointer;
    stepHandlers = D_actor_400500_80131FA4;
    if ((s16)work->subState != ACTOR_400500_FALLEN_CRAWL_STEP_SELECT) {
        _actor400500TickAnim(task);
    }
    if (enemy->hp > 0) {
        stepHandlers.funcs[(s16)work->subState](task);
    } else {
        work->deathHeld = 0;
    }
    _actor400500RequestCloakFade(task, ACTOR_400500_CLOAK_RUNNING | ACTOR_400500_CLOAK_HIDE);
}

/// Chooses the first crawl-on-back route step from the actor's zone and target.
///
/// Requires live Stalker work, target/zone data and the 18-part model beneath
/// the composed view. Knockdown and a nearby target's turn-over take priority.
/// Otherwise restarts and ticks the fallen gait, selects a route, and samples
/// the right-arm tip's world X/Z into signed halfwords without changing Y.
/// An unrecognized zone resets the root position and yaw to the crawl origin.
static void _actor400500SelectFallenCrawlRoute(Task* task)
{
    _Actor400500GrayStalkerWork* work;
    GfxCoord*                    rootCoord;
    s32                          knockdownTaken;
    s32                          yawHeading;
    u16                          playerZone;

    work           = task->work;
    yawHeading     = (u16)work->yaw & ACTOR_TRANSFORM_ANGLE_MASK;
    rootCoord      = task->extra.tmd->coords;
    knockdownTaken = _actor400500TakeKnockdown(task);
    if ((knockdownTaken == 0) && ((_actor400500TryTurnOverNearTarget(task) << 0x10) == 0)) {
        _actor400500SetAnim(task, ACTOR_400500_ANIM_FALLEN_CRAWL, ANIMATION_RATE_ONE);
        _actor400500TickAnim(task);
        // Choose the route in the room axes before saving the planted arm tip.
        switch ((s16)((u16)work->zone - ACTOR_400500_CRAWL_ZONE_X_SEGMENT)) {
            case ACTOR_400500_CRAWL_ZONE_X_END - ACTOR_400500_CRAWL_ZONE_X_SEGMENT:
                if (yawHeading != ACTOR_400500_CRAWL_YAW_POSITIVE_X) {
                    work->subState = ACTOR_400500_FALLEN_CRAWL_STEP_TURN_POSITIVE_X;
                } else {
                    work->subState = ACTOR_400500_FALLEN_CRAWL_STEP_POSITIVE_X;
                }
                break;
            case ACTOR_400500_CRAWL_ZONE_X_SEGMENT - ACTOR_400500_CRAWL_ZONE_X_SEGMENT:
                if (work->toTarget.vx >= 0) {
                    work->subState = ACTOR_400500_FALLEN_CRAWL_STEP_NEGATIVE_X;
                } else {
                    work->subState = ACTOR_400500_FALLEN_CRAWL_STEP_POSITIVE_X;
                }
                break;
            case ACTOR_400500_CRAWL_ZONE_CORNER - ACTOR_400500_CRAWL_ZONE_X_SEGMENT:
                playerZone = work->playerZone;
                if ((u32)(playerZone - ACTOR_400500_CRAWL_ZONE_X_SEGMENT) < 2U) {
                    work->subState = ACTOR_400500_FALLEN_CRAWL_STEP_NEGATIVE_X;
                } else if (((s16)playerZone == ACTOR_400500_CRAWL_ZONE_X_END) || ((s16)work->playerZone == ACTOR_400500_CRAWL_ZONE_PATROL)) {
                    work->subState = ACTOR_400500_FALLEN_CRAWL_STEP_NEGATIVE_X;
                } else if (((s16)playerZone == ACTOR_400500_CRAWL_ZONE_Z_SEGMENT) && (yawHeading == ACTOR_400500_CRAWL_YAW_POSITIVE_Z)) {
                    if (rootCoord->coord.t[0] >= ACTOR_400500_CRAWL_LINE_X) {
                        work->subState = ACTOR_400500_FALLEN_CRAWL_STEP_POSITIVE_Z;
                    } else {
                        work->subState = ACTOR_400500_FALLEN_CRAWL_STEP_POSITIVE_X;
                    }
                } else {
                    work->subState = ACTOR_400500_FALLEN_CRAWL_STEP_POSITIVE_X;
                }
                break;
            case ACTOR_400500_CRAWL_ZONE_Z_SEGMENT - ACTOR_400500_CRAWL_ZONE_X_SEGMENT:
                if (work->toTarget.vz < 0) {
                    work->subState = ACTOR_400500_FALLEN_CRAWL_STEP_POSITIVE_Z;
                } else {
                    work->subState = ACTOR_400500_FALLEN_CRAWL_STEP_NEGATIVE_Z;
                }
                break;
            case ACTOR_400500_CRAWL_ZONE_Z_END - ACTOR_400500_CRAWL_ZONE_X_SEGMENT:
                if (yawHeading == ACTOR_400500_CRAWL_YAW_NEGATIVE_Z) {
                    work->subState = ACTOR_400500_FALLEN_CRAWL_STEP_NEGATIVE_Z;
                } else {
                    work->subState = ACTOR_400500_FALLEN_CRAWL_STEP_TURN_NEGATIVE_Z;
                }
                break;
            default:
                rootCoord->coord.t[0] = ACTOR_400500_CRAWL_RESET_X;
                rootCoord->coord.t[1] = ACTOR_400500_CRAWL_CEILING_Y;
                rootCoord->coord.t[2] = ACTOR_400500_CRAWL_RESET_Z;
                work->yaw             = ACTOR_400500_CRAWL_YAW_POSITIVE_X;
                work->subState        = ACTOR_400500_FALLEN_CRAWL_STEP_POSITIVE_X;
                break;
        }
        _actor400500ReadPartWorldXZ(task, ACTOR_400500_PART_RIGHT_ARM_TIP, &work->anchorPos);
    }
}

/// Follows the fallen crawl's positive-X segment or chooses its next transition.
///
/// Requires live Stalker work/root, zone/target data and current slot results.
/// Knockdown, near-target turn-over and recoil preempt movement in that order.
/// A wrong heading turns over; the corner selects a route from the player's
/// zone, and a target far behind requests turn-over while still ticking gait.
/// Every non-preempted path pins root Z to the crawl line.
static void _actor400500TickFallenCrawlPositiveX(Task* task)
{
    _Actor400500GrayStalkerWork* work;
    GfxCoord*                    rootCoord;
    s32                          knockdownTaken;
    s32                          actorZone;
    u16                          playerZone;

    work           = task->work;
    rootCoord      = task->extra.tmd->coords;
    knockdownTaken = _actor400500TakeKnockdown(task);
    if ((knockdownTaken == 0) && ((_actor400500TryTurnOverNearTarget(task) << 0x10) == 0) &&
        ((_actor400500HandleFallenHitReaction(task) << 0x10) == 0)) {
        if (((u16)work->yaw & ACTOR_TRANSFORM_ANGLE_MASK) != ACTOR_400500_CRAWL_YAW_POSITIVE_X) {
            _actor400500SetState(task, ACTOR_400500_STATE_TURN_OVER, ACTOR_400500_TURN_OVER_STEP_START);
        } else {
            actorZone = work->zone;
            if (actorZone != ACTOR_400500_CRAWL_ZONE_X_SEGMENT) {
                if ((actorZone == ACTOR_400500_CRAWL_ZONE_CORNER) && (rootCoord->coord.t[0] >= ACTOR_400500_CRAWL_LINE_X + 1)) {
                    playerZone = work->playerZone;
                    if (((u32)(playerZone - ACTOR_400500_CRAWL_ZONE_CORNER) < 2U) || ((s16)playerZone == ACTOR_400500_CRAWL_ZONE_Z_END)) {
                        work->subState = ACTOR_400500_FALLEN_CRAWL_STEP_CORNER_CHOICE;
                    } else {
                        work->subState = actorZone;
                    }
                } else {
                    _actor400500TickFallenCrawl(task);
                }
            } else {
                if (work->toTarget.vx < -ACTOR_400500_FALLEN_TARGET_GAP + 1) {
                    _actor400500SetState(task, ACTOR_400500_STATE_TURN_OVER, ACTOR_400500_TURN_OVER_STEP_START);
                }
                _actor400500TickFallenCrawl(task);
            }
        }
        rootCoord->coord.t[2] = ACTOR_400500_CRAWL_LINE_Z;
    }
}

/// Chooses a fallen crawl's corner exit when its heading is already suitable.
///
/// Requires live Stalker work and current target/zone/slot results. Knockdown,
/// near-target turn-over and recoil preempt this choice in that order. Player
/// zones 2, 3 or 6 select the positive-Z heading; the other zones select
/// negative X. A matching heading requests the gait and enters that segment;
/// any unsuitable heading enters turn-over instead.
static void _actor400500TickFallenCrawlCornerChoice(Task* task)
{
    _Actor400500GrayStalkerWork* work;
    s32                          knockdownTaken;
    u16                          playerZone;

    work           = task->work;
    knockdownTaken = _actor400500TakeKnockdown(task);
    if ((knockdownTaken == 0) && ((_actor400500TryTurnOverNearTarget(task) << 0x10) == 0) &&
        ((_actor400500HandleFallenHitReaction(task) << 0x10) == 0)) {
        playerZone = work->playerZone;
        if (((u32)(playerZone - ACTOR_400500_CRAWL_ZONE_CORNER) < 2U) || ((s16)playerZone == ACTOR_400500_CRAWL_ZONE_Z_END)) {
            if (!((u16)work->yaw & ACTOR_TRANSFORM_ANGLE_MASK)) {
                _actor400500SetAnim(task, ACTOR_400500_ANIM_FALLEN_CRAWL, ANIMATION_RATE_ONE);
                work->subState = ACTOR_400500_FALLEN_CRAWL_STEP_POSITIVE_Z;
                return;
            }
        } else if (((u16)work->yaw & ACTOR_TRANSFORM_ANGLE_MASK) == ACTOR_400500_CRAWL_YAW_NEGATIVE_X) {
            _actor400500SetAnim(task, ACTOR_400500_ANIM_FALLEN_CRAWL, ANIMATION_RATE_ONE);
            work->subState = ACTOR_400500_FALLEN_CRAWL_STEP_NEGATIVE_X;
            return;
        }
        _actor400500SetState(task, ACTOR_400500_STATE_TURN_OVER, ACTOR_400500_TURN_OVER_STEP_START);
    }
}

/// Selects cloak hold and hide cooldown frame counts from the room and remaining HP.
///
/// Requires live Stalker work/enemy. Strict HP thresholds choose five bands:
/// separately truncated half plus quarter, then quarter, eighth and sixteenth
/// of maximum HP. Equality takes the lower-health band.
/// Rooms 1, 3, 5 and 6 hold for 16..80 frames and reset cooldown to 0..256.
/// Other rooms hold for 8192 frames and reset cooldown to 32..256. The hold
/// phase ends after its count is exceeded; this call does not restart a fade
/// or change the running cooldown. HP fractions retain signed shift behavior.
static void _actor400500UpdateCloakTiming(Task* task)
{
    enum {
        ACTOR_400500_HIDE_HOLD_STEP_FRAMES         = 16,
        ACTOR_400500_HIDE_COOLDOWN_STEP_FRAMES     = 64,
        ACTOR_400500_LONG_HIDE_HOLD_FRAMES         = 8192,
        ACTOR_400500_LONG_HIDE_MIN_COOLDOWN_FRAMES = 32
    };
    _Actor400500GrayStalkerWork* work;
    Enemy*                       enemy;
    u8                           room;

    work  = task->work;
    room  = gGameSession->location.loc.room;
    enemy = task->spawnArg2.pointer;
    // Rooms 1, 3, 5 and 6 shorten the visible cloak hold as HP rises.
    if ((room == 1) || (room == 3) || (room == 5) || (room == 6)) {
        s16 hp;
        s32 maxHpHighHalf;
        s32 quarterHp;

        hp            = enemy->hp;
        maxHpHighHalf = enemy->hpMax << 0x10;
        quarterHp     = maxHpHighHalf >> 0x12;
        if ((quarterHp + (maxHpHighHalf >> 0x11)) < hp) {
            work->hideHoldFrames    = ACTOR_400500_HIDE_HOLD_STEP_FRAMES;
            work->hideCooldownReset = 0;
            return;
        }
        if (quarterHp < hp) {
            work->hideHoldFrames    = 2 * ACTOR_400500_HIDE_HOLD_STEP_FRAMES;
            work->hideCooldownReset = ACTOR_400500_HIDE_COOLDOWN_STEP_FRAMES;
            return;
        }
        if ((maxHpHighHalf >> 0x13) < hp) {
            work->hideHoldFrames    = 3 * ACTOR_400500_HIDE_HOLD_STEP_FRAMES;
            work->hideCooldownReset = 2 * ACTOR_400500_HIDE_COOLDOWN_STEP_FRAMES;
            return;
        }
        if ((maxHpHighHalf >> 0x14) < hp) {
            work->hideHoldFrames    = 4 * ACTOR_400500_HIDE_HOLD_STEP_FRAMES;
            work->hideCooldownReset = 3 * ACTOR_400500_HIDE_COOLDOWN_STEP_FRAMES;
            return;
        }
        work->hideHoldFrames    = 5 * ACTOR_400500_HIDE_HOLD_STEP_FRAMES;
        work->hideCooldownReset = 4 * ACTOR_400500_HIDE_COOLDOWN_STEP_FRAMES;
        return;
    } else {
        s16 hp;
        s32 maxHpHighHalf;
        s32 quarterHp;

        hp            = enemy->hp;
        maxHpHighHalf = enemy->hpMax << 0x10;
        quarterHp     = maxHpHighHalf >> 0x12;
        if ((quarterHp + (maxHpHighHalf >> 0x11)) < hp) {
            work->hideHoldFrames    = ACTOR_400500_LONG_HIDE_HOLD_FRAMES;
            work->hideCooldownReset = ACTOR_400500_LONG_HIDE_MIN_COOLDOWN_FRAMES;
            return;
        }
        if (quarterHp < hp) {
            work->hideHoldFrames    = ACTOR_400500_LONG_HIDE_HOLD_FRAMES;
            work->hideCooldownReset = ACTOR_400500_HIDE_COOLDOWN_STEP_FRAMES;
            return;
        }
        if ((maxHpHighHalf >> 0x13) < hp) {
            work->hideHoldFrames    = ACTOR_400500_LONG_HIDE_HOLD_FRAMES;
            work->hideCooldownReset = 2 * ACTOR_400500_HIDE_COOLDOWN_STEP_FRAMES;
            return;
        }
        if ((maxHpHighHalf >> 0x14) < hp) {
            work->hideHoldFrames    = ACTOR_400500_LONG_HIDE_HOLD_FRAMES;
            work->hideCooldownReset = 3 * ACTOR_400500_HIDE_COOLDOWN_STEP_FRAMES;
            return;
        }
        work->hideHoldFrames    = ACTOR_400500_LONG_HIDE_HOLD_FRAMES;
        work->hideCooldownReset = 4 * ACTOR_400500_HIDE_COOLDOWN_STEP_FRAMES;
    }
}

/// Sub-state handlers `_actor400500TickRoomSequenceState` copies onto the stack and runs by `subState`.
static const TaskFuncTable5 D_actor_400500_80131FEC = { {
    _actor400500StartRoomSequence,
    _actor400500WaitRoomCrawlCommand,
    _actor400500TickRoomSequenceCrawl,
    _actor400500WaitRoomRevealCommand,
    _actor400500WaitRoomReleaseCommand,
} };

/// Dispatches the room-command sequence and advances its requested pose.
///
/// Requires live Stalker work, enemy and rig; `subState` indexes its five steps.
/// Commands 2, 3 and 4 release successive waits after command 1 starts the
/// sequence. Playback advances after every step, including a command wait.
static void _actor400500TickRoomSequenceState(Task* task)
{
    _Actor400500GrayStalkerWork* work;
    TaskFuncTable5               stepHandlers;

    work         = task->work;
    stepHandlers = D_actor_400500_80131FEC;
    stepHandlers.funcs[(s16)work->subState](task);
    _actor400500TickAnim(task);
}

/// Converts a world rotation in place to the frame of a coordinate's parent.
///
/// Requires a non-NULL parent and a live acyclic chain, with a separate writable
/// word-aligned Q12 rotation matrix. The immediate parent's basis is used as
/// stored; each further ancestor and its product are normalized. Transposing
/// the resulting basis gives the inverse when that basis is orthonormal.
/// Returns 1 after the inverse multiply, 0 unchanged when the parent is the
/// view or the chain ends before the view. Translation stays intact; the SDK
/// multiply also writes matrix alignment bytes. GTE working registers change.
static s32 _actor400500LocalizeWorldRotation(const GfxCoord* joint, MATRIX* worldRotation)
{
    /// Pre-multiplies and normalizes a Q12 basis using this function's matrix storage.
    ///
    /// Arguments must be side-effect-free, disjoint pointers to live word-aligned
    /// matrices. Rotation is evaluated three times, normalized storage twice.
    /// Only the 3x3 result is valid; the complete copy leaves other bytes unspecified.
    /// No caller identifiers are captured; the macro is undefined after this function.
#define ACTOR_400500_PREMULTIPLY_NORMALIZED_ROTATION(parentBasis, rotation, normalized) \
    do {                                                                                \
        gte_SetRotMatrix(parentBasis);                                                  \
        MulRotMatrix(rotation);                                                         \
        MatrixNormal(rotation, normalized);                                             \
        *(rotation) = *(normalized);                                                    \
    } while (0)
    MATRIX          parentWorldRotation;
    MATRIX          parentRotation;
    MATRIX          normalizedRotation;
    MATRIX          inverseParentRotation;
    const GfxCoord* ancestor;
    const GfxCoord* viewCoord;
    MATRIX*         parentRotationPtr;

    ancestor = joint->parent;
    if (ancestor == &gGfxViewCoord) {
        return 0;
    }
    viewCoord           = &gGfxViewCoord;
    parentRotationPtr   = &parentRotation;
    parentWorldRotation = ancestor->coord;
    while (1) {
        ancestor = ancestor->parent;
        if (ancestor == NULL) {
            return 0;
        }
        if (ancestor == viewCoord) {
            break;
        }
        parentRotation = ancestor->coord;
        MatrixNormal(parentRotationPtr, parentRotationPtr);
        ACTOR_400500_PREMULTIPLY_NORMALIZED_ROTATION(parentRotationPtr, &parentWorldRotation, &normalizedRotation);
    }
    // The inverse basis removes all ancestors below the excluded view node.
    gte_TransposeMatrix(&parentWorldRotation, &inverseParentRotation);
    gte_SetRotMatrix(&inverseParentRotation);
    MulRotMatrix(worldRotation);
    return 1;
}
#undef ACTOR_400500_PREMULTIPLY_NORMALIZED_ROTATION

#include "../../shared/coord_math_local_to_world.inc.c"

static void func_actor_400500_8013BA24(Task* arg0)
{
    _Actor400500GrayStalkerWork* work;
    _Actor400500GrayStalkerWork* work2;
    TaskFuncTable3               sp;

    work = (_Actor400500GrayStalkerWork*)arg0->work;
    sp   = D_actor_400500_80131EE4;
    sp.funcs[(s16)work->subState](arg0);
    work2                     = (_Actor400500GrayStalkerWork*)arg0->work;
    work2->hitTaken           = 0;
    work2->hitReaction        = ACTOR_400500_HIT_REACTION_NONE;
    work2                     = (_Actor400500GrayStalkerWork*)arg0->work;
    work2->ceilingFallPending = 0;
}

/// Holds a non-blast ceiling-crawl death until the body falls and lands.
///
/// Requires live Stalker work. A blast or floor posture releases the hold
/// immediately. Otherwise requests the ceiling hold pose, clears the fall's
/// counters and vertical motion, and advances the three-step death cursor.
/// The calling crawl state advances animation.
static void _actor400500StartCrawlDeathFall(Task* task)
{
    _Actor400500GrayStalkerWork* work;

    work = task->work;
    if (work->lastHitReaction == ACTOR_400500_HIT_REACTION_BLAST) {
        work->deathHeld = 0;
        return;
    }
    if (!(work->posture & ACTOR_400500_POSTURE_ON_FLOOR)) {
        work->deathHeld = 1;
        _actor400500SetAnim(task, ACTOR_400500_ANIM_CEILING_HOLD, ANIMATION_RATE_ONE);
        work->stateFrames  = 0;
        work->playerCaught = 0;
        work->moveAccel    = 0;
        work->moveSpeed    = 0;
        work->deathStep    = work->deathStep + 1;
        return;
    }
    work->deathHeld = 0;
}

/// Falls from a lethal ceiling crawl and starts the death landing pose.
///
/// Requires live Stalker work/root with the death cursor on its fall step.
/// Halfword acceleration/speed increase downward motion. Passing the body
/// pivot height advances the cursor, requests landing, adds half a turn to
/// roll and places the root at floor height. The caller advances animation.
static void _actor400500TickCrawlDeathFall(Task* task)
{
    _Actor400500GrayStalkerWork* work;
    GfxCoord*                    rootCoord;

    work                   = task->work;
    rootCoord              = task->extra.tmd->coords;
    work->moveAccel       += 2;
    work->moveSpeed       += work->moveAccel;
    rootCoord->coord.t[1] += work->moveSpeed;
    if (rootCoord->coord.t[1] >= ACTOR_400500_FALL_PIVOT_Y + 1) {
        rootCoord->coord.t[1] = ACTOR_400500_FALL_PIVOT_Y;
        work->deathStep++;
        _actor400500SetAnim(task, ACTOR_400500_ANIM_FALL_LANDING, ANIMATION_RATE_ONE);
        work->roll           += ACTOR_TRANSFORM_ANGLE_HALF_TURN;
        rootCoord->coord.t[1] = ACTOR_400500_FLOOR_Y;
        work->stateFrames     = 0;
    }
}

/// Sounds the lethal crawl's landing and releases its death hold at a boundary.
///
/// Requires live Stalker work/enemy, composed root and latest slot results.
/// Sounds once while the reset state counter is zero, then increments that
/// counter. A slot boundary, control jump or held boundary pose releases the
/// hold so the task can enter its ordinary death sequence.
static void _actor400500FinishCrawlDeathLanding(Task* task)
{
    _Actor400500GrayStalkerWork* work;
    s32                          placedSoundId;
    s32                          animationEnded;

    work = task->work;
    if ((s16)work->stateFrames == 0) {
        placedSoundId = ((((Enemy*)task->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | ACTOR_400500_SOUND_FALL_LANDING;
        _actor400500EnqueueRootSound(task, placedSoundId);
        work->stateFrames = work->stateFrames + 1;
    }
    animationEnded = _actor400500HasAnimBoundary(task);
    if (animationEnded) {
        work->deathHeld = 0;
    }
}

/// Holds a ceiling-strike death and initializes its fall motion.
///
/// Requires live Stalker work and the ceiling-strike death cursor at step zero.
/// Clears the frame counter, catch flag and halfword vertical motion, then
/// advances the cursor. The strike state bypasses this sequence for blast kills.
static void _actor400500StartCeilingStrikeDeathFall(Task* task)
{
    _Actor400500GrayStalkerWork* work = task->work;
    work->deathHeld                   = 1;
    work->stateFrames                 = 0;
    work->playerCaught                = 0;
    work->moveAccel                   = 0;
    work->moveSpeed                   = 0;
    work->deathStep                   = work->deathStep + 1;
}

/// Falls from a lethal ceiling strike and starts the death landing pose.
///
/// Requires live Stalker work/root with the strike death cursor on its fall step.
/// Halfword acceleration/speed increase downward motion. Passing the body
/// pivot height advances the cursor, requests landing, adds half a turn to
/// roll and places the root at floor height. The caller advances animation.
static void _actor400500TickCeilingStrikeDeathFall(Task* task)
{
    _Actor400500GrayStalkerWork* work;
    GfxCoord*                    rootCoord;

    work                   = task->work;
    rootCoord              = task->extra.tmd->coords;
    work->moveAccel       += 2;
    work->moveSpeed       += work->moveAccel;
    rootCoord->coord.t[1] += work->moveSpeed;
    if (rootCoord->coord.t[1] >= ACTOR_400500_FALL_PIVOT_Y + 1) {
        rootCoord->coord.t[1] = ACTOR_400500_FALL_PIVOT_Y;
        work->deathStep++;
        _actor400500SetAnim(task, ACTOR_400500_ANIM_FALL_LANDING, ANIMATION_RATE_ONE);
        work->roll           += ACTOR_TRANSFORM_ANGLE_HALF_TURN;
        rootCoord->coord.t[1] = ACTOR_400500_FLOOR_Y;
        work->stateFrames     = 0;
    }
}

/// Sounds the lethal ceiling strike's landing and releases its death hold at a boundary.
///
/// Requires live Stalker work/enemy, a composed root and latest slot results.
/// Sounds once while the signed-halfword state counter is zero, then advances
/// that counter. A slot boundary, control jump or held boundary pose releases
/// the hold so the task can enter its ordinary death sequence.
static void _actor400500FinishCeilingStrikeDeathLanding(Task* task)
{
    _Actor400500GrayStalkerWork* work;
    s32                          placedSoundId;
    s32                          animationEnded;

    work = task->work;
    if ((s16)work->stateFrames == 0) {
        placedSoundId = ((((Enemy*)task->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | ACTOR_400500_SOUND_FALL_LANDING;
        _actor400500EnqueueRootSound(task, placedSoundId);
        work->stateFrames = work->stateFrames + 1;
    }
    animationEnded = _actor400500HasAnimBoundary(task);
    if (animationEnded) {
        work->deathHeld = 0;
    }
}

/// Shows the body and initializes the ceiling strike's right-arm extension.
///
/// Requires live Stalker work with ceiling-strike sub-state zero. Requests the
/// ceiling hold clip at normal rate, clears the tick/catch/swing counters, and
/// advances to extension. A running show fade keeps its progress; a new show
/// bypasses the hide cooldown. The calling state advances animation.
static void _actor400500StartCeilingArmStrike(Task* task)
{
    _Actor400500GrayStalkerWork* work;
    _Actor400500GrayStalkerWork* cloakWork;
    s32                          cloakRequest;

    work           = task->work;
    work->animRate = ANIMATION_RATE_ONE;
    cloakWork      = task->work;
    if ((cloakWork->cloakRequest >= 0) || (((u8)cloakWork->cloakRequest & ACTOR_400500_CLOAK_KIND_MASK) != ACTOR_400500_CLOAK_SHOW)) {
        cloakRequest            = ACTOR_400500_CLOAK_RUNNING | ACTOR_400500_CLOAK_SHOW;
        cloakWork->cloakRequest = cloakRequest;
        cloakWork->cloakPhase   = ACTOR_400500_CLOAK_PHASE_FIRST_RAMP;
    }
    _actor400500SetAnim(task, ACTOR_400500_ANIM_CEILING_HOLD, ANIMATION_RATE_ONE);
    work->stateFrames   = 0;
    work->playerCaught  = 0;
    work->armSwingAngle = 0;
    work->subState      = work->subState + 1;
}

/// Swings the right-arm model out and starts the ceiling strike clip at its limit.
///
/// Requires live Stalker work and its right-arm TMD child. Adds 128 angle units
/// per tick to the wrapping halfword and displays the arm at positive local yaw.
/// A signed-halfword test at 512 units (one eighth turn) advances to the timed
/// strike and clears its tick/catch counters. Playback and composition happen
/// in the calling state/model update; the child remains borrowed.
static void _actor400500ExtendCeilingStrikeArm(Task* task)
{
    _Actor400500GrayStalkerWork* work;
    _Actor400500GrayStalkerWork* armWork;
    Task*                        rightArmTask;
    s32                          swingAngle;

    work                           = task->work;
    work->armSwingAngle            = work->armSwingAngle + ACTOR_400500_ARM_EXTEND_ANGLE_STEP;
    work->stateFrames              = work->stateFrames + 1;
    armWork                        = task->work;
    rightArmTask                   = armWork->armTasks[ACTOR_400500_ARM_RIGHT];
    swingAngle                     = work->armSwingAngle;
    rightArmTask->extra.tmd->flags = 0;
    _actor400500SetCoordYaw(rightArmTask->extra.tmd->coords, swingAngle);
    if ((s16)work->armSwingAngle >= ACTOR_400500_ARM_STRIKE_SWING_ANGLE) {
        _actor400500SetAnim(task, ACTOR_400500_ANIM_CEILING_STRIKE, ANIMATION_RATE_ONE);
        work->playerCaught = 0;
        work->stateFrames  = 0;
        work->subState     = work->subState + 1;
    }
}

/// Shows the body and initializes the floor strike's left-arm extension.
///
/// Requires live Stalker work with floor-strike sub-state zero. Requests the
/// hold clip at normal rate, clears the tick/catch/swing counters, and advances
/// to extension. A running show fade keeps its progress; a new show bypasses
/// the hide cooldown. The calling state advances animation.
static void _actor400500StartFloorArmStrike(Task* task)
{
    _Actor400500GrayStalkerWork* work;
    s32                          cloakRequest;

    work = task->work;
    if ((work->cloakRequest >= 0) || (((u8)work->cloakRequest & ACTOR_400500_CLOAK_KIND_MASK) != ACTOR_400500_CLOAK_SHOW)) {
        cloakRequest       = ACTOR_400500_CLOAK_RUNNING | ACTOR_400500_CLOAK_SHOW;
        work->cloakRequest = cloakRequest;
        work->cloakPhase   = ACTOR_400500_CLOAK_PHASE_FIRST_RAMP;
    }
    _actor400500SetAnim(task, ACTOR_400500_ANIM_CEILING_HOLD, ANIMATION_RATE_ONE);
    work->stateFrames   = 0;
    work->playerCaught  = 0;
    work->armSwingAngle = 0;
    work->subState      = work->subState + 1;
}

/// Swings the left-arm model out and starts the floor strike clip at its limit.
///
/// Requires live Stalker work and its left-arm TMD child. Adds 128 angle units
/// per tick to the wrapping halfword and displays the arm at negative local yaw.
/// A signed-halfword test at 512 units (one eighth turn) advances to the timed
/// strike and clears its tick/catch counters. Playback and composition happen
/// in the calling state/model update; the child remains borrowed.
static void _actor400500ExtendFloorStrikeArm(Task* task)
{
    _Actor400500GrayStalkerWork* work;
    _Actor400500GrayStalkerWork* armWork;
    Task*                        leftArmTask;
    s32                          swingAngle;

    work                          = task->work;
    work->armSwingAngle           = work->armSwingAngle + ACTOR_400500_ARM_EXTEND_ANGLE_STEP;
    work->stateFrames             = work->stateFrames + 1;
    armWork                       = task->work;
    leftArmTask                   = armWork->armTasks[ACTOR_400500_ARM_LEFT];
    swingAngle                    = work->armSwingAngle;
    leftArmTask->extra.tmd->flags = 0;
    _actor400500SetCoordYaw(leftArmTask->extra.tmd->coords, -swingAngle);
    if ((s16)work->armSwingAngle >= ACTOR_400500_ARM_STRIKE_SWING_ANGLE) {
        _actor400500SetAnim(task, ACTOR_400500_ANIM_FLOOR_STRIKE, ANIMATION_RATE_ONE);
        work->playerCaught = 0;
        work->stateFrames  = 0;
        work->subState     = work->subState + 1;
    }
}

/// Shows the body and starts the ordinary ceiling fall with zero vertical motion.
///
/// Requires live Stalker work with fall sub-state zero. Requests the hold clip
/// at normal rate, clears tick/catch and signed-halfword motion, then advances
/// to falling. A running show fade keeps its progress; a new show bypasses the
/// hide cooldown. The calling state advances animation.
static void _actor400500StartCeilingFall(Task* task)
{
    _Actor400500GrayStalkerWork* work;
    s32                          cloakRequest;

    work = task->work;
    if ((work->cloakRequest >= 0) || (((u8)work->cloakRequest & ACTOR_400500_CLOAK_KIND_MASK) != ACTOR_400500_CLOAK_SHOW)) {
        cloakRequest       = ACTOR_400500_CLOAK_RUNNING | ACTOR_400500_CLOAK_SHOW;
        work->cloakRequest = cloakRequest;
        work->cloakPhase   = ACTOR_400500_CLOAK_PHASE_FIRST_RAMP;
    }
    _actor400500SetAnim(task, ACTOR_400500_ANIM_CEILING_HOLD, ANIMATION_RATE_ONE);
    work->stateFrames  = 0;
    work->playerCaught = 0;
    work->moveAccel    = 0;
    work->moveSpeed    = 0;
    work->subState     = work->subState + 1;
}

/// Integrates the ordinary ceiling fall and starts its landing pose.
///
/// Requires live Stalker work/root with the fall sub-state active. Acceleration
/// and speed wrap as signed halfwords. Passing root Y -2200 advances the step,
/// requests landing, reverses roll and places the root at Y -1000 in its parent
/// frame. Sets the on-back bit; the floor bit is set only after recovery.
/// The caller advances animation and rebuilds the root basis.
static void _actor400500TickCeilingFall(Task* task)
{
    _Actor400500GrayStalkerWork* work;
    GfxCoord*                    rootCoord;

    work                   = task->work;
    rootCoord              = task->extra.tmd->coords;
    work->moveAccel       += 2;
    work->moveSpeed       += work->moveAccel;
    rootCoord->coord.t[1] += work->moveSpeed;
    if (rootCoord->coord.t[1] >= ACTOR_400500_FALL_PIVOT_Y + 1) {
        // Switch from the ceiling pivot to the floor pose after crossing it.
        rootCoord->coord.t[1] = ACTOR_400500_FALL_PIVOT_Y;
        work->subState++;
        _actor400500SetAnim(task, ACTOR_400500_ANIM_FALL_LANDING, ANIMATION_RATE_ONE);
        work->roll           += ACTOR_TRANSFORM_ANGLE_HALF_TURN;
        rootCoord->coord.t[1] = ACTOR_400500_FLOOR_Y;
        work->stateFrames     = 0;
        work->posture        |= ACTOR_400500_POSTURE_ON_BACK;
    }
}

/// Sounds the ordinary fall's landing and starts recovery at an animation boundary.
///
/// Requires live Stalker work/enemy, composed root and latest slot results.
/// Sounds once at signed-halfword counter zero, then advances that counter.
/// A living enemy requests recovery and advances the sub-state; a dead enemy
/// releases the death hold at the same boundary without requesting recovery.
static void _actor400500FinishCeilingFallLanding(Task* task)
{
    _Actor400500GrayStalkerWork* work;
    Enemy*                       enemy;
    s32                          placedSoundId;
    s32                          animationEnded;

    work  = task->work;
    enemy = task->spawnArg2.pointer;
    if ((s16)work->stateFrames == 0) {
        placedSoundId = ((enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | ACTOR_400500_SOUND_FALL_LANDING;
        _actor400500EnqueueRootSound(task, placedSoundId);
        work->stateFrames = work->stateFrames + 1;
    }
    animationEnded = _actor400500HasAnimBoundary(task);
    if (animationEnded) {
        if (enemy->hp > 0) {
            _actor400500SetAnim(task, ACTOR_400500_ANIM_FALL_RECOVERY, ANIMATION_RATE_ONE);
            work->subState = work->subState + 1;
        } else {
            work->deathHeld = 0;
        }
    }
}

/// Ends ordinary fall recovery by entering fallen crawling and setting floor posture.
///
/// Requires live Stalker work/enemy and latest slot results. At a boundary, a
/// living enemy enters fallen crawl at route selection, preserving the on-back
/// bit. A dead enemy instead releases the death hold without changing state.
static void _actor400500FinishCeilingFallRecovery(Task* task)
{
    _Actor400500GrayStalkerWork* work;
    Enemy*                       enemy;
    s32                          animationEnded;

    work           = task->work;
    enemy          = task->spawnArg2.pointer;
    animationEnded = _actor400500HasAnimBoundary(task);
    if (animationEnded) {
        if (enemy->hp > 0) {
            _actor400500SetState(task, ACTOR_400500_STATE_CRAWL_FALLEN, ACTOR_400500_FALLEN_CRAWL_STEP_SELECT);
            work->posture = work->posture | ACTOR_400500_POSTURE_ON_FLOOR;
            return;
        }
        work->deathHeld = 0;
    }
}

/// Ends floor knockdown recoil and requests recovery for the current posture.
///
/// Requires live Stalker work/enemy and latest slot results. A living enemy
/// waits for a boundary, requests upright or on-back recovery at normal rate,
/// and selects the floor-recovery step. Death releases the hold immediately.
static void _actor400500FinishFloorKnockdownRecoil(Task* task)
{
    enum { ACTOR_400500_KNOCKDOWN_STEP_FLOOR_RECOVERY = 2 };
    Enemy*                       enemy;
    _Actor400500GrayStalkerWork* work;
    s32                          animationEnded;

    enemy = task->spawnArg2.pointer;
    work  = task->work;
    if (enemy->hp > 0) {
        animationEnded = _actor400500HasAnimBoundary(task);
        if (animationEnded) {
            if (!(work->posture & ACTOR_400500_POSTURE_ON_BACK)) {
                _actor400500SetAnim(task, ACTOR_400500_ANIM_UPRIGHT_RECOVERY, ANIMATION_RATE_ONE);
            } else {
                _actor400500SetAnim(task, ACTOR_400500_ANIM_ON_BACK_RECOVERY, ANIMATION_RATE_ONE);
            }
            work->subState = ACTOR_400500_KNOCKDOWN_STEP_FLOOR_RECOVERY;
        }
    } else {
        work->deathHeld = 0;
    }
}

/// Ends floor knockdown recovery and resumes crawling for the current posture.
///
/// Requires live Stalker work/enemy and latest slot results. A living enemy
/// clears pending knockdown at a boundary and enters ordinary or fallen crawl
/// at route selection. Death releases the hold immediately, keeping the pending
/// flag and the living state unchanged.
static void _actor400500FinishFloorKnockdownRecovery(Task* task)
{
    Enemy*                       enemy;
    _Actor400500GrayStalkerWork* work;
    s32                          animationEnded;

    enemy = task->spawnArg2.pointer;
    work  = task->work;
    if (enemy->hp > 0) {
        animationEnded = _actor400500HasAnimBoundary(task);
        if (animationEnded) {
            work->knockdownPending = 0;
            if (!(work->posture & ACTOR_400500_POSTURE_ON_BACK)) {
                _actor400500SetState(task, ACTOR_400500_STATE_CRAWL, ACTOR_400500_CRAWL_STEP_SELECT);
            } else {
                _actor400500SetState(task, ACTOR_400500_STATE_CRAWL_FALLEN, ACTOR_400500_FALLEN_CRAWL_STEP_SELECT);
            }
        }
    } else {
        work->deathHeld = 0;
    }
}

/// Ends ceiling knockdown recoil and initializes the fall motion.
///
/// Requires live Stalker work/enemy and latest slot results. A living enemy
/// waits for a boundary; death also starts the fall. Clears tick/catch and
/// signed-halfword vertical motion before advancing the sub-state. It keeps
/// the death hold set until landing completes.
static void _actor400500FinishCeilingKnockdownRecoil(Task* task)
{
    _Actor400500GrayStalkerWork* work;
    Enemy*                       enemy;
    s32                          animationEnded;

    work           = task->work;
    enemy          = task->spawnArg2.pointer;
    animationEnded = _actor400500HasAnimBoundary(task);
    if (animationEnded || (enemy->hp <= 0)) {
        work->stateFrames  = 0;
        work->playerCaught = 0;
        work->moveAccel    = 0;
        work->moveSpeed    = 0;
        work->subState     = work->subState + 1;
    }
}

/// Advances knockdown fall acceleration, speed and root height by one tick.
///
/// Both pointers borrow live callback storage. Motion narrows after each
/// addition to a signed halfword; height uses the resulting signed speed in
/// root-parent game coordinates. Retains neither pointer.
static inline void _actor400500StepKnockdownFallMotion(_Actor400500GrayStalkerWork* work, GfxCoord* rootCoord)
{
    work->moveAccel       += 2;
    work->moveSpeed       += work->moveAccel;
    rootCoord->coord.t[1] += work->moveSpeed;
}

/// Integrates the knockdown ceiling fall and starts its landing pose.
///
/// Requires live Stalker work/root with the knockdown fall sub-state active.
/// Acceleration and speed wrap as signed halfwords. Passing root Y -2200
/// advances the step, requests landing, reverses roll and places the root at
/// Y -1000 in its parent frame. Sets only the floor posture bit, preserving
/// the on-back bit. The caller advances animation and rebuilds the root basis.
static void _actor400500TickKnockdownCeilingFall(Task* task)
{
    _Actor400500GrayStalkerWork* work;
    GfxCoord*                    rootCoord;

    work      = task->work;
    rootCoord = task->extra.tmd->coords;
    _actor400500StepKnockdownFallMotion(work, rootCoord);
    if (rootCoord->coord.t[1] >= ACTOR_400500_FALL_PIVOT_Y + 1) {
        // Switch from the ceiling pivot to the floor pose after crossing it.
        rootCoord->coord.t[1] = ACTOR_400500_FALL_PIVOT_Y;
        work->subState++;
        _actor400500SetAnim(task, ACTOR_400500_ANIM_FALL_LANDING, ANIMATION_RATE_ONE);
        work->roll           += ACTOR_TRANSFORM_ANGLE_HALF_TURN;
        rootCoord->coord.t[1] = ACTOR_400500_FLOOR_Y;
        work->stateFrames     = 0;
        work->posture        |= ACTOR_400500_POSTURE_ON_FLOOR;
    }
}

/// Sounds the knockdown fall's landing and clears pending knockdown at a boundary.
///
/// Requires live Stalker work/enemy, composed root and latest slot results.
/// Sounds once at signed-halfword counter zero, then advances that counter.
/// At a boundary, a living enemy requests recovery and advances the sub-state;
/// a dead enemy releases the death hold. Both clear pending knockdown.
static void _actor400500FinishKnockdownCeilingLanding(Task* task)
{
    _Actor400500GrayStalkerWork* work;
    Enemy*                       enemy;
    s32                          placedSoundId;
    s32                          animationEnded;

    work  = task->work;
    enemy = task->spawnArg2.pointer;
    if ((s16)work->stateFrames == 0) {
        placedSoundId = ((enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | ACTOR_400500_SOUND_FALL_LANDING;
        _actor400500EnqueueRootSound(task, placedSoundId);
        work->stateFrames = work->stateFrames + 1;
    }
    animationEnded = _actor400500HasAnimBoundary(task);
    if (animationEnded) {
        work->knockdownPending = 0;
        if (enemy->hp > 0) {
            _actor400500SetAnim(task, ACTOR_400500_ANIM_FALL_RECOVERY, ANIMATION_RATE_ONE);
            work->subState = work->subState + 1;
        } else {
            work->deathHeld = 0;
        }
    }
}

/// Ends knockdown ceiling-fall recovery by entering fallen crawling.
///
/// Requires live Stalker work and latest slot results. At a boundary, enters
/// fallen crawl at route selection regardless of health, keeping posture and
/// the death hold unchanged. The landing step already established floor posture.
static void _actor400500FinishKnockdownCeilingRecovery(Task* task)
{
    s32 animationEnded;

    animationEnded = _actor400500HasAnimBoundary(task);
    if (animationEnded) {
        _actor400500SetState(task, ACTOR_400500_STATE_CRAWL_FALLEN, ACTOR_400500_FALLEN_CRAWL_STEP_SELECT);
    }
}

/// Starts the ambush wait and requests the ceiling-hold pose at normal rate.
///
/// Requires live Stalker work and rig. Requests hiding only when its cooldown
/// permits, latches a cut to the hold clip and advances the ambush sub-step.
/// The enclosing state handler applies that animation request.
static void _actor400500StartAmbush(Task* task)
{
    _Actor400500GrayStalkerWork* work;

    work           = task->work;
    work->animRate = ANIMATION_RATE_ONE;
    _actor400500RequestCloakFade(task, ACTOR_400500_CLOAK_RUNNING | ACTOR_400500_CLOAK_HIDE);
    _actor400500SetAnim(task, ACTOR_400500_ANIM_CEILING_HOLD, ANIMATION_RATE_ONE);
    work->subState = work->subState + 1;
}

/// Starts a deliberate ceiling drop and saves its landing X/Z.
///
/// Requires live Stalker work/enemy and a composed model root. Sounds the
/// posture-change cue, requests the drop clip at normal speed, clears the
/// catch/tick counters and starts halfword vertical motion with acceleration
/// 128 and speed zero. Saved root-parent X/Z retain their low 16 bits; the
/// next sub-state integrates the drop and returns to that position.
static void _actor400500StartDropFromCeiling(Task* task)
{
    enum { ACTOR_400500_ANIM_DROP_START    = 32,
           ACTOR_400500_DROP_INITIAL_ACCEL = 128 };
    _Actor400500GrayStalkerWork* work;
    GfxCoord*                    rootCoord;
    s32                          placedSoundId;
    const Enemy*                 enemy;

    rootCoord     = task->extra.tmd->coords;
    enemy         = task->spawnArg2.pointer;
    placedSoundId = ((enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | ACTOR_400500_SOUND_POSTURE_CHANGE;
    work          = task->work;
    _actor400500EnqueueRootSound(task, placedSoundId);
    // Prepare the drop before capturing the root-parent landing position.
    _actor400500SetAnim(task, ACTOR_400500_ANIM_DROP_START, ANIMATION_RATE_ONE);
    work->stateFrames  = 0;
    work->playerCaught = 0;
    work->moveAccel    = ACTOR_400500_DROP_INITIAL_ACCEL;
    work->moveSpeed    = 0;
    work->stateFrames  = 0;
    work->subState     = work->subState + 1;
    work->dropStartX   = rootCoord->coord.t[0];
    work->dropStartZ   = rootCoord->coord.t[2];
}

/// Starts the floor-to-ceiling jump's animation and upward motion.
///
/// Requires live Stalker work/enemy and a composed model root. Sounds the
/// posture-change cue, requests the jump clip at normal speed, clears pitch
/// and the catch/tick counters, and starts halfword motion with speed 300
/// and acceleration zero. The next sub-state subtracts that speed from root Y.
static void _actor400500StartJumpToCeiling(Task* task)
{
    enum { ACTOR_400500_ANIM_JUMP_START    = 21,
           ACTOR_400500_JUMP_INITIAL_SPEED = 300 };
    _Actor400500GrayStalkerWork* work;
    s32                          placedSoundId;
    const Enemy*                 enemy;

    work          = task->work;
    enemy         = task->spawnArg2.pointer;
    placedSoundId = ((enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | ACTOR_400500_SOUND_POSTURE_CHANGE;
    _actor400500EnqueueRootSound(task, placedSoundId);
    // The jump integrator subtracts speed from root Y after the wind-up.
    _actor400500SetAnim(task, ACTOR_400500_ANIM_JUMP_START, ANIMATION_RATE_ONE);
    work->stateFrames  = 0;
    work->playerCaught = 0;
    work->moveAccel    = 0;
    work->moveSpeed    = ACTOR_400500_JUMP_INITIAL_SPEED;
    work->pitch        = 0;
    work->subState     = work->subState + 1;
}

/// Returns to ceiling crawling when the jump's landing animation reaches a boundary.
///
/// Requires live Stalker work and current slot results. Enters crawl at route
/// selection and clears only the floor posture bit; the preceding landing
/// step has already restored the ceiling position and roll.
static void _actor400500FinishJumpToCeiling(Task* task)
{
    _Actor400500GrayStalkerWork* work;
    s32                          animationEnded;

    work           = task->work;
    animationEnded = _actor400500HasAnimBoundary(task);
    if (animationEnded) {
        _actor400500SetState(task, ACTOR_400500_STATE_CRAWL, ACTOR_400500_CRAWL_STEP_SELECT);
        work->posture &= ~ACTOR_400500_POSTURE_ON_FLOOR;
    }
}

/// Starts the turn-over clip and optionally saves its world-space foot pivot.
///
/// Requires live Stalker work and an 18-part model beneath the composed view.
/// In the X-route zone facing either X direction, saves part 14's signed-low-
/// halfword world X/Z and stores the -1 tick sentinel used by turn-over pinning.
/// Otherwise starts at tick zero. Latches the clip at normal rate and advances
/// the sub-step; the enclosing state handler applies the request.
static void _actor400500StartTurnOver(Task* task)
{
    enum { ACTOR_400500_ANIM_TURN_OVER = 22 };
    _Actor400500GrayStalkerWork* work;
    s32                          yaw;
    s32                          wrappedYaw;
    s32                          pinSentinel;

    work              = task->work;
    yaw               = (u16)work->yaw;
    work->stateFrames = 0;
    _actor400500SetAnim(task, ACTOR_400500_ANIM_TURN_OVER, ANIMATION_RATE_ONE);
    wrappedYaw = yaw & ACTOR_TRANSFORM_ANGLE_MASK;
    if ((work->zone == ACTOR_400500_CRAWL_ZONE_X_SEGMENT) && ((wrappedYaw == ACTOR_400500_CRAWL_YAW_POSITIVE_X) || (wrappedYaw == ACTOR_400500_CRAWL_YAW_NEGATIVE_X))) {
        pinSentinel       = ACTOR_400500_TURN_OVER_PIN_TICK;
        work->stateFrames = pinSentinel;
        _actor400500ReadPartWorldXZ(task, ACTOR_400500_TURN_OVER_PIVOT_PART, &work->anchorPos);
    }
    work->subState = work->subState + 1;
}

/// Resumes the fallen gait facing -X, or enters turn-over for an unsuitable heading.
///
/// Requires live Stalker work and current target/slot results. Knockdown,
/// near-target turn-over and fallen recoil preempt the heading check in that
/// order. A matching 4096-unit yaw restarts the fallen gait at normal speed
/// and selects the negative-X segment; this step does not rotate the actor.
static void _actor400500CheckFallenCrawlNegativeXHeading(Task* task)
{
    _Actor400500GrayStalkerWork* work;
    s32                          knockdownTaken;

    work           = task->work;
    knockdownTaken = _actor400500TakeKnockdown(task);
    if ((knockdownTaken == 0) && ((s16)_actor400500TryTurnOverNearTarget(task) == 0) &&
        ((s16)_actor400500HandleFallenHitReaction(task) == 0)) {
        if (((u16)work->yaw & ACTOR_TRANSFORM_ANGLE_MASK) == ACTOR_400500_CRAWL_YAW_NEGATIVE_X) {
            _actor400500SetAnim(task, ACTOR_400500_ANIM_FALLEN_CRAWL, ANIMATION_RATE_ONE);
            work->subState = ACTOR_400500_FALLEN_CRAWL_STEP_NEGATIVE_X;
            return;
        }
        _actor400500SetState(task, ACTOR_400500_STATE_TURN_OVER, ACTOR_400500_TURN_OVER_STEP_START);
    }
}

/// Runs the fallen gait facing -X and chooses its next transition.
///
/// Requires live Stalker work/root and current zone, target and slot results.
/// Knockdown, near-target turn-over and recoil preempt movement in that order.
/// A wrong heading enters turn-over. Zone 4 selects the positive-X heading
/// check; target offset X at least +4000 in zone 1 enters turn-over while
/// still ticking the gait. Every non-preempted path pins root Z to the crawl line.
static void _actor400500TickFallenCrawlFacingNegativeX(Task* task)
{
    _Actor400500GrayStalkerWork* work;
    GfxCoord*                    rootCoord;
    s32                          knockdownTaken;
    s32                          actorZone;

    work           = task->work;
    rootCoord      = task->extra.tmd->coords;
    knockdownTaken = _actor400500TakeKnockdown(task);
    if ((knockdownTaken == 0) && ((s16)_actor400500TryTurnOverNearTarget(task) == 0) &&
        ((s16)_actor400500HandleFallenHitReaction(task) == 0)) {
        if (((u16)work->yaw & ACTOR_TRANSFORM_ANGLE_MASK) != ACTOR_400500_CRAWL_YAW_NEGATIVE_X) {
            _actor400500SetState(task, ACTOR_400500_STATE_TURN_OVER, ACTOR_400500_TURN_OVER_STEP_START);
        } else {
            actorZone = work->zone;
            if (actorZone != ACTOR_400500_CRAWL_ZONE_X_SEGMENT) {
                if (actorZone == ACTOR_400500_CRAWL_ZONE_X_END) {
                    work->subState = actorZone;
                }
            } else if (work->toTarget.vx >= ACTOR_400500_FALLEN_TARGET_GAP) {
                _actor400500SetState(task, ACTOR_400500_STATE_TURN_OVER, ACTOR_400500_TURN_OVER_STEP_START);
            }
            // Finish this gait tick even when a different state or step was selected.
            _actor400500TickFallenCrawl(task);
        }
        rootCoord->coord.t[2] = ACTOR_400500_CRAWL_LINE_Z;
    }
}

/// Resumes the fallen gait facing +X, or enters turn-over for an unsuitable heading.
///
/// Requires live Stalker work and current target/slot results. Knockdown,
/// near-target turn-over and fallen recoil preempt the heading check in that
/// order. A matching 4096-unit yaw restarts the fallen gait at normal speed
/// and selects the positive-X segment; this step does not rotate the actor.
static void _actor400500CheckFallenCrawlPositiveXHeading(Task* task)
{
    _Actor400500GrayStalkerWork* work;
    s32                          knockdownTaken;

    work           = task->work;
    knockdownTaken = _actor400500TakeKnockdown(task);
    if ((knockdownTaken == 0) && ((s16)_actor400500TryTurnOverNearTarget(task) == 0) &&
        ((s16)_actor400500HandleFallenHitReaction(task) == 0)) {
        if (((u16)work->yaw & ACTOR_TRANSFORM_ANGLE_MASK) == ACTOR_400500_CRAWL_YAW_POSITIVE_X) {
            _actor400500SetAnim(task, ACTOR_400500_ANIM_FALLEN_CRAWL, ANIMATION_RATE_ONE);
            work->subState = ACTOR_400500_FALLEN_CRAWL_STEP_POSITIVE_X;
            return;
        }
        _actor400500SetState(task, ACTOR_400500_STATE_TURN_OVER, ACTOR_400500_TURN_OVER_STEP_START);
    }
}

/// Runs the fallen gait facing +Z and selects the opposite-heading check.
///
/// Requires live Stalker work/root and current zone, target and slot results.
/// Knockdown, near-target turn-over and recoil preempt movement in that order.
/// A wrong heading enters turn-over. Zone 6, or target offset Z > 0 in zone 3,
/// selects the negative-Z heading check while still ticking the gait.
/// Every non-preempted path pins root X to the crawl line.
static void _actor400500TickFallenCrawlFacingPositiveZ(Task* task)
{
    _Actor400500GrayStalkerWork* work;
    GfxCoord*                    rootCoord;
    s32                          knockdownTaken;

    work           = task->work;
    rootCoord      = task->extra.tmd->coords;
    knockdownTaken = _actor400500TakeKnockdown(task);
    if ((knockdownTaken == 0) && ((s16)_actor400500TryTurnOverNearTarget(task) == 0) &&
        ((s16)_actor400500HandleFallenHitReaction(task) == 0)) {
        if (((u16)work->yaw & ACTOR_TRANSFORM_ANGLE_MASK) != ACTOR_400500_CRAWL_YAW_POSITIVE_Z) {
            _actor400500SetState(task, ACTOR_400500_STATE_TURN_OVER, ACTOR_400500_TURN_OVER_STEP_START);
        } else {
            if (work->zone != ACTOR_400500_CRAWL_ZONE_Z_SEGMENT) {
                if (work->zone == ACTOR_400500_CRAWL_ZONE_Z_END) {
                    work->subState = ACTOR_400500_FALLEN_CRAWL_STEP_TURN_NEGATIVE_Z;
                }
            } else if (work->toTarget.vz > 0) {
                work->subState = ACTOR_400500_FALLEN_CRAWL_STEP_TURN_NEGATIVE_Z;
            }
            // Finish this gait tick even when a different state or step was selected.
            _actor400500TickFallenCrawl(task);
        }
        rootCoord->coord.t[0] = ACTOR_400500_CRAWL_LINE_X;
    }
}

/// Resumes the fallen gait facing -Z, or enters turn-over for an unsuitable heading.
///
/// Requires live Stalker work and current target/slot results. Knockdown,
/// near-target turn-over and fallen recoil preempt the heading check in that
/// order. A matching 4096-unit yaw restarts the fallen gait at normal speed
/// and selects the negative-Z segment; this step does not rotate the actor.
static void _actor400500CheckFallenCrawlNegativeZHeading(Task* task)
{
    _Actor400500GrayStalkerWork* work;
    s32                          knockdownTaken;

    work           = task->work;
    knockdownTaken = _actor400500TakeKnockdown(task);
    if ((knockdownTaken == 0) && ((s16)_actor400500TryTurnOverNearTarget(task) == 0) &&
        ((s16)_actor400500HandleFallenHitReaction(task) == 0)) {
        if (((u16)work->yaw & ACTOR_TRANSFORM_ANGLE_MASK) == ACTOR_400500_CRAWL_YAW_NEGATIVE_Z) {
            _actor400500SetAnim(task, ACTOR_400500_ANIM_FALLEN_CRAWL, ANIMATION_RATE_ONE);
            work->subState = ACTOR_400500_FALLEN_CRAWL_STEP_NEGATIVE_Z;
            return;
        }
        _actor400500SetState(task, ACTOR_400500_STATE_TURN_OVER, ACTOR_400500_TURN_OVER_STEP_START);
    }
}

/// Runs the fallen gait facing -Z and selects a corner or opposite-heading check.
///
/// Requires live Stalker work/root and current zone, target and slot results.
/// Knockdown, near-target turn-over and recoil preempt movement in that order.
/// A wrong heading enters turn-over. Crossing the fixed-Z line in zone 2
/// selects the corner's negative-X check; target offset Z < 0 in zone 3 selects
/// the positive-Z check. Both still tick gait; non-preempted paths pin root X.
static void _actor400500TickFallenCrawlFacingNegativeZ(Task* task)
{
    enum { ACTOR_400500_FALLEN_CRAWL_STEP_CORNER_TURN_NEGATIVE_X = 9,
           ACTOR_400500_FALLEN_CRAWL_STEP_TURN_POSITIVE_Z        = 10 };
    _Actor400500GrayStalkerWork* work;
    GfxCoord*                    rootCoord;
    s32                          knockdownTaken;

    work           = task->work;
    rootCoord      = task->extra.tmd->coords;
    knockdownTaken = _actor400500TakeKnockdown(task);
    if ((knockdownTaken == 0) && ((s16)_actor400500TryTurnOverNearTarget(task) == 0) &&
        ((s16)_actor400500HandleFallenHitReaction(task) == 0)) {
        if (((u16)work->yaw & ACTOR_TRANSFORM_ANGLE_MASK) != ACTOR_400500_CRAWL_YAW_NEGATIVE_Z) {
            _actor400500SetState(task, ACTOR_400500_STATE_TURN_OVER, ACTOR_400500_TURN_OVER_STEP_START);
        } else {
            switch (work->zone) {
                case ACTOR_400500_CRAWL_ZONE_CORNER:
                    if (rootCoord->coord.t[2] < ACTOR_400500_CRAWL_LINE_Z) {
                        work->subState = ACTOR_400500_FALLEN_CRAWL_STEP_CORNER_TURN_NEGATIVE_X;
                    }
                    break;
                case ACTOR_400500_CRAWL_ZONE_Z_SEGMENT:
                    if (work->toTarget.vz < 0) {
                        work->subState = ACTOR_400500_FALLEN_CRAWL_STEP_TURN_POSITIVE_Z;
                    }
                    break;
            }
            // Finish this gait tick even when a different state or step was selected.
            _actor400500TickFallenCrawl(task);
        }
        rootCoord->coord.t[0] = ACTOR_400500_CRAWL_LINE_X;
    }
}

/// Selects the fallen gait facing -X at the corner, or enters turn-over.
///
/// Requires live Stalker work and current target/slot results. Knockdown,
/// near-target turn-over and fallen recoil preempt the heading check in that
/// order. A matching 4096-unit yaw restarts the fallen gait at normal speed
/// and selects the negative-X segment; this step does not rotate the actor.
static void _actor400500CheckFallenCrawlCornerNegativeXHeading(Task* task)
{
    _Actor400500GrayStalkerWork* work;
    s32                          knockdownTaken;

    work           = task->work;
    knockdownTaken = _actor400500TakeKnockdown(task);
    if ((knockdownTaken == 0) && ((s16)_actor400500TryTurnOverNearTarget(task) == 0) &&
        ((s16)_actor400500HandleFallenHitReaction(task) == 0)) {
        if (((u16)work->yaw & ACTOR_TRANSFORM_ANGLE_MASK) == ACTOR_400500_CRAWL_YAW_NEGATIVE_X) {
            _actor400500SetAnim(task, ACTOR_400500_ANIM_FALLEN_CRAWL, ANIMATION_RATE_ONE);
            work->subState = ACTOR_400500_FALLEN_CRAWL_STEP_NEGATIVE_X;
            return;
        }
        _actor400500SetState(task, ACTOR_400500_STATE_TURN_OVER, ACTOR_400500_TURN_OVER_STEP_START);
    }
}

/// Resumes the fallen gait facing +Z, or enters turn-over for an unsuitable heading.
///
/// Requires live Stalker work and current target/slot results. Knockdown,
/// near-target turn-over and fallen recoil preempt the heading check in that
/// order. A matching 4096-unit yaw restarts the fallen gait at normal speed
/// and selects the positive-Z segment; this step does not rotate the actor.
static void _actor400500CheckFallenCrawlPositiveZHeading(Task* task)
{
    _Actor400500GrayStalkerWork* work;
    s32                          knockdownTaken;

    work           = task->work;
    knockdownTaken = _actor400500TakeKnockdown(task);
    if ((knockdownTaken == 0) && ((s16)_actor400500TryTurnOverNearTarget(task) == 0) &&
        ((s16)_actor400500HandleFallenHitReaction(task) == 0)) {
        if (((u16)work->yaw & ACTOR_TRANSFORM_ANGLE_MASK) == ACTOR_400500_CRAWL_YAW_POSITIVE_Z) {
            _actor400500SetAnim(task, ACTOR_400500_ANIM_FALLEN_CRAWL, ANIMATION_RATE_ONE);
            work->subState = ACTOR_400500_FALLEN_CRAWL_STEP_POSITIVE_Z;
            return;
        }
        _actor400500SetState(task, ACTOR_400500_STATE_TURN_OVER, ACTOR_400500_TURN_OVER_STEP_START);
    }
}

/// Places the Stalker on the ceiling at the room-command sequence's starting point.
///
/// Requires live Stalker work and model root. Sets root-parent position to
/// (16500, -4000, -10000), yaw to +Z and roll to half a 4096-unit turn.
/// Requests the ceiling-hold clip at quarter speed, clears the tick counter
/// and advances to the command-2 wait. Pitch and posture remain as supplied.
static void _actor400500StartRoomSequence(Task* task)
{
    enum { ACTOR_400500_ROOM_SEQUENCE_START_Z = -10000 };
    _Actor400500GrayStalkerWork* work;
    GfxCoord*                    rootCoord;

    work                  = task->work;
    rootCoord             = task->extra.tmd->coords;
    work->roll            = ACTOR_TRANSFORM_ANGLE_HALF_TURN;
    work->yaw             = ACTOR_400500_CRAWL_YAW_POSITIVE_Z;
    rootCoord->coord.t[0] = ACTOR_400500_CRAWL_LINE_X;
    rootCoord->coord.t[1] = ACTOR_400500_CRAWL_CEILING_Y;
    rootCoord->coord.t[2] = ACTOR_400500_ROOM_SEQUENCE_START_Z;
    work->stateFrames     = 0;
    _actor400500SetAnim(task, ACTOR_400500_ANIM_CEILING_HOLD, ANIMATION_RATE_ONE / 4);
    work->subState = work->subState + 1;
}

/// Requests a room-sequence reveal without waiting for the hide cooldown.
///
/// Requires live Stalker work. A running show retains its current phase;
/// otherwise starts the show at its first ramp. Playback runs elsewhere.
static inline void _actor400500RequestRoomSequenceShow(Task* task)
{
    _Actor400500GrayStalkerWork* work = task->work;
    s32                          cloakRequest;

    if ((work->cloakRequest >= 0) || (((u8)work->cloakRequest & ACTOR_400500_CLOAK_KIND_MASK) != ACTOR_400500_CLOAK_SHOW)) {
        cloakRequest       = ACTOR_400500_CLOAK_RUNNING | ACTOR_400500_CLOAK_SHOW;
        work->cloakRequest = cloakRequest;
        work->cloakPhase   = ACTOR_400500_CLOAK_PHASE_FIRST_RAMP;
    }
}

/// Waits for room command 2 before revealing the Stalker and starting the slow crawl.
///
/// Requires live Stalker work. Sets the hide holding time to 600 ticks and
/// requests a show without the hide cooldown gate, preserving a show already
/// running. Clears the tick counter and advances to crawling; animation and
/// cloak playback run in the calling frame update.
static void _actor400500WaitRoomCrawlCommand(Task* task)
{
    enum { ACTOR_400500_ROOM_COMMAND_CRAWL         = 2,
           ACTOR_400500_ROOM_CRAWL_HIDE_HOLD_TICKS = 600 };
    _Actor400500GrayStalkerWork* work;

    work = task->work;
    if (work->roomCommand == ACTOR_400500_ROOM_COMMAND_CRAWL) {
        work->hideHoldFrames = ACTOR_400500_ROOM_CRAWL_HIDE_HOLD_TICKS;
        _actor400500RequestRoomSequenceShow(task);
        work->stateFrames = 0;
        work->subState    = work->subState + 1;
    }
}

/// Requests a ten-frame blend from room-sequence crawling to the ceiling hold.
///
/// Requires live Stalker work and its loaded ceiling-hold animation set.
/// The caller advances playback after the request, at normal speed.
static inline void _actor400500BlendRoomSequenceHold(Task* task)
{
    enum { ACTOR_400500_ROOM_SEQUENCE_HOLD_BLEND_FRAMES = 10 };
    _Actor400500GrayStalkerWork* work = task->work;

    work->animBlendFrames = ACTOR_400500_ROOM_SEQUENCE_HOLD_BLEND_FRAMES;
    work->animRate        = ANIMATION_RATE_ONE;
    work->animId          = ACTOR_400500_ANIM_CEILING_HOLD;
    work->animRequest     = ACTOR_400500_ANIM_REQUEST_BLEND;
}

/// Crawls the room sequence to its stop point, then blends into the ceiling hold.
///
/// Requires live Stalker work/root and current slot results for the 18-part
/// model beneath the composed view. The signed-halfword first tick requests
/// hide through the cooldown gate; rejection delays another attempt until
/// counter wrap. The crawl retains its rate. Root-parent Z at least -8799 requests
/// a ten-frame hold blend at normal speed and advances to the command-3 wait.
static void _actor400500TickRoomSequenceCrawl(Task* task)
{
    enum { ACTOR_400500_ROOM_SEQUENCE_STOP_Z = -8799 };
    _Actor400500GrayStalkerWork* work;
    GfxCoord*                    rootCoord;

    work      = task->work;
    rootCoord = task->extra.tmd->coords;
    // The wrapping tick counter and cooldown gate this hide request.
    if ((s16)++work->stateFrames == 1) {
        _actor400500RequestCloakFade(task, ACTOR_400500_CLOAK_RUNNING | ACTOR_400500_CLOAK_HIDE);
    }
    _actor400500TickScriptedCrawl(task);
    if (rootCoord->coord.t[2] >= ACTOR_400500_ROOM_SEQUENCE_STOP_Z) {
        _actor400500BlendRoomSequenceHold(task);
        work->subState = work->subState + 1;
    }
}

/// Waits for room command 3 before revealing the Stalker for the sequence's final wait.
///
/// Requires live Stalker work. Clears the tick counter, changes the hide
/// holding time to 16 ticks and requests a show without the hide cooldown
/// gate, preserving a show already running. Advances to the command-4 wait;
/// animation and cloak playback run in the calling frame update.
static void _actor400500WaitRoomRevealCommand(Task* task)
{
    enum { ACTOR_400500_ROOM_COMMAND_REVEAL         = 3,
           ACTOR_400500_ROOM_REVEAL_HIDE_HOLD_TICKS = 16 };
    _Actor400500GrayStalkerWork* work;

    work = task->work;
    if (work->roomCommand == ACTOR_400500_ROOM_COMMAND_REVEAL) {
        work->stateFrames    = 0;
        work->hideHoldFrames = ACTOR_400500_ROOM_REVEAL_HIDE_HOLD_TICKS;
        _actor400500RequestRoomSequenceShow(task);
        work->subState = work->subState + 1;
    }
}

/// Waits for command 4 to release the room sequence into ordinary crawling.
///
/// Requires live Stalker work/enemy and a composed root for the placed cue.
/// The wrapping counter sounds on signed-halfword tick 30 before checking
/// the command. Release clears commandActive and enters route selection;
/// the command latch and animation remain as supplied.
static void _actor400500WaitRoomReleaseCommand(Task* task)
{
    enum { ACTOR_400500_ROOM_COMMAND_RELEASE    = 4,
           ACTOR_400500_ROOM_RELEASE_SOUND_TICK = 30 };
    _Actor400500GrayStalkerWork* work;

    work = task->work;
    if ((s16)++work->stateFrames == ACTOR_400500_ROOM_RELEASE_SOUND_TICK) {
        _actor400500PlayPlacedSound(task, ACTOR_400500_SOUND_POSTURE_CHANGE);
    }
    if (work->roomCommand == ACTOR_400500_ROOM_COMMAND_RELEASE) {
        work->commandActive = 0;
        _actor400500SetState(task, ACTOR_400500_STATE_CRAWL, ACTOR_400500_CRAWL_STEP_SELECT);
    }
}

/// Ends floor status-held recoil when the enemy's buildup drains.
///
/// Requires live Stalker work and enemy. Ticks buildup only while alive and
/// requests posture-selected recovery at normal rate when it finishes.
/// Death releases the hold immediately; this step does not wait for a clip boundary.
static void _actor400500FinishFloorStatusHoldRecoil(Task* task)
{
    enum { ACTOR_400500_STATUS_HOLD_STEP_FLOOR_RECOVERY = 2 };
    Enemy*                       enemy;
    _Actor400500GrayStalkerWork* work;

    enemy = task->spawnArg2.pointer;
    work  = task->work;
    if (enemy->hp > 0) {
        if (damageTickEnemyBuildup(enemy) != 0) {
            if (!(work->posture & ACTOR_400500_POSTURE_ON_BACK)) {
                _actor400500SetAnim(task, ACTOR_400500_ANIM_UPRIGHT_RECOVERY, ANIMATION_RATE_ONE);
            } else {
                _actor400500SetAnim(task, ACTOR_400500_ANIM_ON_BACK_RECOVERY, ANIMATION_RATE_ONE);
            }
            work->subState = ACTOR_400500_STATUS_HOLD_STEP_FLOOR_RECOVERY;
        }
    } else {
        work->deathHeld = 0;
    }
}

/// Ends status-hold floor recovery and resumes posture-selected crawling.
///
/// Requires live Stalker work/enemy and latest slot results. While alive,
/// a boundary clears pending knockdown and enters the selected crawl's first
/// route step. Death instead releases the hold, retaining the pending flag.
static void _actor400500FinishFloorStatusHoldRecovery(Task* task)
{
    Enemy*                       enemy;
    _Actor400500GrayStalkerWork* work;
    s32                          animationEnded;

    enemy = task->spawnArg2.pointer;
    work  = task->work;
    if (enemy->hp > 0) {
        animationEnded = _actor400500HasAnimBoundary(task);
        if (animationEnded) {
            work->knockdownPending = 0;
            if (!(work->posture & ACTOR_400500_POSTURE_ON_BACK)) {
                _actor400500SetState(task, ACTOR_400500_STATE_CRAWL, ACTOR_400500_CRAWL_STEP_SELECT);
            } else {
                _actor400500SetState(task, ACTOR_400500_STATE_CRAWL_FALLEN, ACTOR_400500_FALLEN_CRAWL_STEP_SELECT);
            }
        }
    } else {
        work->deathHeld = 0;
    }
}

/// Ends ceiling status-held recoil and initializes its fall motion.
///
/// Requires live Stalker work/enemy and latest slot results. A clip boundary
/// or death clears the tick/catch fields and signed-halfword vertical motion,
/// then advances to falling. It leaves the death hold set through landing.
static void _actor400500FinishCeilingStatusHoldRecoil(Task* task)
{
    _Actor400500GrayStalkerWork* work;
    Enemy*                       enemy;
    s32                          animationEnded;

    work           = task->work;
    enemy          = task->spawnArg2.pointer;
    animationEnded = _actor400500HasAnimBoundary(task);
    if (animationEnded || (enemy->hp <= 0)) {
        work->stateFrames  = 0;
        work->playerCaught = 0;
        work->moveAccel    = 0;
        work->moveSpeed    = 0;
        work->subState     = work->subState + 1;
    }
}

/// Integrates one status-held fall tick in signed-halfword vertical motion.
///
/// Requires writable live work and root storage for this call. Acceleration
/// gains two game-coordinate units per tick squared; acceleration and speed
/// narrow to signed halfwords after each addition. The resulting signed speed
/// advances the full-width root-parent Y without composing or dirtying the cache;
/// the enclosing frame handler rebuilds it. No pointer is retained.
static inline void _actor400500StepStatusHoldFallMotion(_Actor400500GrayStalkerWork* work, GfxCoord* rootCoord)
{
    work->moveAccel       += 2;
    work->moveSpeed       += work->moveAccel;
    rootCoord->coord.t[1] += work->moveSpeed;
}

/// Integrates the status-held ceiling fall and requests its landing pose.
///
/// Requires live Stalker work/root. Motion wraps as signed halfwords. Crossing
/// root-parent Y -2200 advances the step, requests landing at normal rate,
/// reverses roll and puts root Y at -1000. Sets ON_FLOOR while preserving
/// ON_BACK; the caller advances animation and rebuilds the root basis.
static void _actor400500TickStatusHoldCeilingFall(Task* task)
{
    _Actor400500GrayStalkerWork* work;
    GfxCoord*                    rootCoord;

    work      = task->work;
    rootCoord = task->extra.tmd->coords;
    _actor400500StepStatusHoldFallMotion(work, rootCoord);
    if (rootCoord->coord.t[1] >= ACTOR_400500_FALL_PIVOT_Y + 1) {
        // Switch the ceiling pivot to the floor pose after crossing its height.
        rootCoord->coord.t[1] = ACTOR_400500_FALL_PIVOT_Y;
        work->subState++;
        _actor400500SetAnim(task, ACTOR_400500_ANIM_FALL_LANDING, ANIMATION_RATE_ONE);
        work->roll           += ACTOR_TRANSFORM_ANGLE_HALF_TURN;
        rootCoord->coord.t[1] = ACTOR_400500_FLOOR_Y;
        work->stateFrames     = 0;
        work->posture        |= ACTOR_400500_POSTURE_ON_FLOOR;
    }
}

/// Sounds the status-held fall landing and starts recovery at a boundary.
///
/// Requires live Stalker work/enemy, composed root and latest slot results.
/// Signed-halfword counter zero sounds once and advances the counter. At a
/// boundary, clears pending knockdown before testing health: a living enemy
/// requests normal-rate recovery; a dead enemy releases the death hold.
static void _actor400500FinishStatusHoldCeilingLanding(Task* task)
{
    _Actor400500GrayStalkerWork* work;
    Enemy*                       enemy;
    s32                          placedSoundId;
    s32                          animationEnded;

    work  = task->work;
    enemy = task->spawnArg2.pointer;
    if ((s16)work->stateFrames == 0) {
        placedSoundId = ((enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | ACTOR_400500_SOUND_FALL_LANDING;
        _actor400500EnqueueRootSound(task, placedSoundId);
        work->stateFrames = work->stateFrames + 1;
    }
    animationEnded = _actor400500HasAnimBoundary(task);
    if (animationEnded) {
        work->knockdownPending = 0;
        if (enemy->hp > 0) {
            _actor400500SetAnim(task, ACTOR_400500_ANIM_FALL_RECOVERY, ANIMATION_RATE_ONE);
            work->subState = work->subState + 1;
        } else {
            work->deathHeld = 0;
        }
    }
}

/// Ends status-held ceiling-fall recovery by entering fallen crawling.
///
/// Requires live Stalker work and latest slot results. A boundary enters the
/// first fallen-crawl route step regardless of health, preserving posture
/// and the death hold.
static void _actor400500FinishStatusHoldCeilingRecovery(Task* task)
{
    s32 animationEnded;

    animationEnded = _actor400500HasAnimBoundary(task);
    if (animationEnded) {
        _actor400500SetState(task, ACTOR_400500_STATE_CRAWL_FALLEN, ACTOR_400500_FALLEN_CRAWL_STEP_SELECT);
    }
}

/// Saves the root pose and starts the ordinary death shrink's reveal delay.
///
/// Requires live Stalker work, enemy and model root. Snapshots the complete
/// root MATRIX, initializes Q12 Y scale to one, selects weighted actor colour
/// and clears the wrapping tick counter before advancing the death state.
/// The saved matrix remains owned by the work through the shrink.
static void _actor400500PrepareDeathShrink(Task* task)
{
    _Actor400500GrayStalkerWork* work;
    TmdObject*                   model;
    GfxCoord*                    rootCoord;

    model              = task->extra.tmd;
    work               = task->work;
    rootCoord          = model->coords;
    work->shrinkScaleY = ONE;
    work->savedRootMtx = rootCoord->coord;
    worldCoordSetActorColorMode(task->spawnArg2.pointer, ENEMY_COLOR_WEIGHTED);
    work->stateFrames = 0;
    work->state       = work->state + 1;
}

/// Waits 24 ticks, restores the uncloaked shading and advances to death shrinking.
///
/// Requires live work/model and a counter cleared by shrink preparation.
/// The counter wraps as u16 and compares as s16. On completion, publishes
/// zero cloak material weight, full Q12 colour blend and full shadow shade,
/// then clears the counter for the shrink. Playback is not advanced here.
static void _actor400500WaitDeathShrinkDelay(Task* task)
{
    enum { ACTOR_400500_DEATH_SHRINK_DELAY_TICKS = 24,
           ACTOR_400500_DEATH_SHADOW_FULL_SHADE  = 255 };
    _Actor400500GrayStalkerWork* work;
    TmdObject*                   model;
    u16                          elapsedTicks;

    work              = task->work;
    model             = task->extra.tmd;
    elapsedTicks      = work->stateFrames + 1;
    work->stateFrames = elapsedTicks;
    if ((s16)elapsedTicks >= ACTOR_400500_DEATH_SHRINK_DELAY_TICKS) {
        work->cloakLevel  = 0;
        work->colorBlend  = TMD_OBJECT_COLOR_BLEND_ONE;
        work->shadowShade = ACTOR_400500_DEATH_SHADOW_FULL_SHADE;
        modelLightingSetLayerMaterials(work->cloakLevel);
        model->shading.colorBlend = work->colorBlend;
        work->stateFrames         = 0;
        work->state               = work->state + 1;
    }
}

/// Hands an ordinary death to arm teardown and the final destruction delay.
///
/// Requires live Stalker work after the shrink hides its body. Selects task
/// state 3 at its first teardown step and clears the unused sub-state; work,
/// enemy and both arm handles remain live for the teardown callbacks.
static void _actor400500BeginTeardown(Task* task)
{
    enum { ACTOR_400500_TASK_TEARDOWN           = 3,
           ACTOR_400500_TEARDOWN_STEP_KILL_ARMS = 0 };
    _Actor400500GrayStalkerWork* work = task->work;

    task->state    = ACTOR_400500_TASK_TEARDOWN;
    work->state    = ACTOR_400500_TEARDOWN_STEP_KILL_ARMS;
    work->subState = 0;
}

/// Hides the blast-killed body and its shadows, then starts the buffer-release delay.
///
/// Requires the live death work/model after collision/target teardown. Leaves
/// the model and primitive buffer allocated, clears the wrapping tick counter
/// and advances the death step. Fragment spawning occurs after the delay.
static void _actor400500HideBurstBody(Task* task)
{
    TmdObject*                   model;
    _Actor400500GrayStalkerWork* work;

    model             = task->extra.tmd;
    work              = task->work;
    model->flags     |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
    work->stateFrames = 0;
    work->shadowShade = 0;
    work->state       = work->state + 1;
}

/// Waits two death callbacks before advancing to primitive-buffer release and fragment spawning.
///
/// Requires live work and a counter cleared when the burst body was hidden.
/// The u16 counter wraps, then its signed low halfword is compared. Advances
/// only the death-step selector; allocation release belongs to the next step.
static void _actor400500WaitBurstBufferDelay(Task* task)
{
    enum { ACTOR_400500_BURST_BUFFER_DELAY_TICKS = 2 };
    _Actor400500GrayStalkerWork* work;
    u16                          elapsedTicks;

    work              = task->work;
    elapsedTicks      = work->stateFrames + 1;
    work->stateFrames = elapsedTicks;
    if ((s16)elapsedTicks >= ACTOR_400500_BURST_BUFFER_DELAY_TICKS) {
        work->state = work->state + 1;
    }
}

static void func_actor_400500_8013DA68(Task* arg0)
{
    TmdObject*                   model;
    _Actor400500GrayStalkerWork* work;

    model = arg0->extra.tmd;
    work  = (_Actor400500GrayStalkerWork*)arg0->work;
    tmdFreePrimitiveBuffer(model);
    model->flags |= TMD_OBJECT_SKIP_AUTO_BUFFER;
    _actor400500SpawnDeathFragments(arg0);
    work->state = work->state + 1;
}

/// Hands a burst death to arm teardown and the final destruction delay.
///
/// Requires live work/enemy after primitive-buffer release and fragment spawn.
/// Selects task state 3, teardown step 0 and clears sub-state; retained arm
/// handles and work stay live for the teardown callbacks.
static void _actor400500BeginBurstTeardown(Task* task)
{
    enum { ACTOR_400500_TASK_TEARDOWN           = 3,
           ACTOR_400500_TEARDOWN_STEP_KILL_ARMS = 0 };
    _Actor400500GrayStalkerWork* work = task->work;

    task->state    = ACTOR_400500_TASK_TEARDOWN;
    work->state    = ACTOR_400500_TEARDOWN_STEP_KILL_ARMS;
    work->subState = 0;
}

/// Receives room commands that control the Gray Stalker's scripted sequence.
///
/// Borrows a live `ActorCommand` during `ACTOR_COMMAND_MESSAGE_APPLY` dispatch;
/// context, messageId and unusedPayload are unread. Command 1 restarts the
/// sequence at its first step with a 30-tick hide hold; 2 requests crawling,
/// 3 requests revealing and 4 releases the sequence to normal crawling.
/// Other commands leave the latch intact. No payload pointer is retained.
/// The matching callback falls off its s32 body; callers must ignore its result.
static s32 _actor400500ApplyRoomCommand(Task* task, s32 messageId, const ActorCommand* command, s32 unusedPayload)
{
    enum { ACTOR_400500_ROOM_COMMAND_START           = 1,
           ACTOR_400500_ROOM_COMMAND_CRAWL           = 2,
           ACTOR_400500_ROOM_COMMAND_REVEAL          = 3,
           ACTOR_400500_ROOM_COMMAND_RELEASE         = 4,
           ACTOR_400500_ROOM_COMMAND_HIDE_HOLD_TICKS = 30 };
    _Actor400500GrayStalkerWork* work;
    s32                          roomCommand;

    work        = task->work;
    roomCommand = command->command;
    switch (roomCommand) {
        case ACTOR_400500_ROOM_COMMAND_START:
            work->commandActive  = roomCommand;
            work->hideHoldFrames = ACTOR_400500_ROOM_COMMAND_HIDE_HOLD_TICKS;
            work->roomCommand    = roomCommand;
            _actor400500EnterState(task, ACTOR_400500_STATE_ROOM_SEQUENCE);
            break;
        case ACTOR_400500_ROOM_COMMAND_CRAWL:
            work->roomCommand = roomCommand;
            break;
        case ACTOR_400500_ROOM_COMMAND_REVEAL:
            work->roomCommand = roomCommand;
            break;
        case ACTOR_400500_ROOM_COMMAND_RELEASE:
            work->roomCommand = roomCommand;
            break;
    }
}

/// Enters a living `ACTOR_400500_STATE_*` state at its first sub-state.
///
/// Requires the live Stalker's work. Resets only `subState`; timers, animation
/// and the task's outer lifecycle state are retained for the new handler.
static void _actor400500EnterState(Task* task, s16 state)
{
    _Actor400500GrayStalkerWork* work = (_Actor400500GrayStalkerWork*)task->work;

    work->state    = state;
    work->subState = 0;
}

/// Enters the turn-over state when the target is close and in the forward cone.
///
/// Returns 1 after entering `ACTOR_400500_STATE_TURN_OVER`, otherwise 0.
/// Fallen-crawl callers supply the posture context; this test reads only the
/// live work's target measurements, using the strike range and angle gate
/// without an attack cooldown or player-zone restriction.
static s32 _actor400500TryTurnOverNearTarget(Task* task)
{
    _Actor400500GrayStalkerWork* work = (_Actor400500GrayStalkerWork*)task->work;

    if ((work->targetDist < (ACTOR_400500_STRIKE_RANGE - (work->playerLocalMove.vz * ACTOR_400500_TARGET_MOVE_RANGE_SCALE))) &&
        ((u32)(work->targetBearing - ACTOR_400500_STRIKE_HALF_ANGLE) >= (ONE - 2 * ACTOR_400500_STRIKE_HALF_ANGLE + 1U))) {
        work->state    = ACTOR_400500_STATE_TURN_OVER;
        work->subState = 0;
        return 1;
    }
    return 0;
}

/// Samples a part's world X/Z through the out-of-line placement entry.
///
/// Has the live model, composed view and coordinate-index contract of
/// `_actor400500ReadPartWorldXZ`. Output X/Z retain signed low halfwords;
/// Y stays untouched. The output is borrowed only during this call, and
/// the sampled part is marked dirty after removing the view transform.
static void _actor400500SamplePartWorldXZ(Task* task, s16 partIndex, SVECTOR3* worldPosition)
{
    _actor400500ReadPartWorldXZ(task, partIndex, worldPosition);
}

/// Restarts part slots 1..17 on the requested animation set.
///
/// Requires the live initialized rig and a loaded set at `animId`. Root slot 0
/// is untouched. Each reset replaces the preceding rate assignment with
/// `ANIMATION_RATE_ONE`; the tick driver reinstalls the requested signed low-byte
/// rate before advancing poses. Records the set as applied; no pose is ticked here.
static void _actor400500ResetAnimSlots(Task* task)
{
    _Actor400500GrayStalkerWork* work;
    s32                          slotIndex;

    work      = (_Actor400500GrayStalkerWork*)task->work;
    slotIndex = 1;
    do {
        work->rig.slots[slotIndex].rate = work->animRate;
        animationResetSlot(&work->rig.anim, slotIndex, work->animId);
        slotIndex++;
    } while (slotIndex < ARRAY_SIZE(work->rig.slots));
    work->appliedAnim = work->animId;
}

/// Requests a cut to a loaded animation set without applying it yet.
///
/// Has the same live-work, set-index and sixteenth-frame rate contract as
/// `_actor400500SetAnim`; turn handling uses this out-of-line entry.
static void _actor400500RequestAnimReset(Task* task, s16 setIndex, s16 rateSixteenths)
{
    _actor400500SetAnim(task, setIndex, rateSixteenths);
}

/// Applies the requested rate and blends part slots into a changed animation set.
///
/// Requires the live initialized rig and a loaded set at `animId`. A repeated
/// set changes only slots 1..17's signed low-byte rates and retains the pending
/// blend duration. A changed set captures each slot's ticked pose and blends to
/// track offset zero over `animBlendFrames` whole normal-rate frames, then clears
/// that duration. Root slot 0 is untouched; the requested set becomes applied.
static void _actor400500BlendAnimSlots(Task* task)
{
    s32                          sameAnimation;
    _Actor400500GrayStalkerWork* work;
    s32                          slotIndex;

    work          = (_Actor400500GrayStalkerWork*)task->work;
    slotIndex     = 1;
    sameAnimation = work->appliedAnim == work->animId;
    // The one-pass scope preserves this compiler's allocation of the shared loop counter.
    do {
        if (sameAnimation) {
            do {
                work->rig.slots[slotIndex].rate = work->animRate;
                slotIndex++;
            } while (slotIndex < ARRAY_SIZE(work->rig.slots));
        } else {
            do {
                work->rig.slots[slotIndex].rate = work->animRate;
                animationSeekSlotWithBlend(&work->rig.anim, slotIndex, work->animId, 0, work->animBlendFrames);
                slotIndex++;
            } while (slotIndex < ARRAY_SIZE(work->rig.slots));
            work->animBlendFrames = 0;
        }
    } while (0);
    work->appliedAnim = work->animId;
}

/// Converts a whole-frame counter to the current playback rate with signed-halfword wrapping.
///
/// Requires the live Stalker's work. Returns zero at zero rate; otherwise divides
/// `frames * 256` by the signed s16 rate, truncating toward zero, then shifts
/// right by four (rounding negative results down) and keeps the signed low
/// halfword. This preserves the two-stage rounding used by animation windows.
static s16 _actor400500RescaleAnimFrames(Task* task, s16 frames)
{
    _Actor400500GrayStalkerWork* work;
    s16                          rateSixteenths;

    work           = (_Actor400500GrayStalkerWork*)task->work;
    rateSixteenths = work->animRate;
    if (rateSixteenths == 0) {
        return 0;
    }
    return (s16)((frames * (ANIMATION_RATE_ONE * ANIMATION_RATE_ONE) / rateSixteenths) >> 4);
}

/// Returns the slot-1 boundary, control-jump or held-pose predicate used by turns.
///
/// Has the same initialized-rig contract as `_actor400500HasAnimBoundary` and
/// leaves the latest slot flags intact.
static s32 _actor400500CheckAnimBoundary(Task* task)
{
    return _actor400500HasAnimBoundary(task);
}

/// Copies nine rotation coefficients while preserving the destination's translation and alignment bytes.
///
/// Both matrices must be live for this call; the source is read-only. They may
/// be the same complete matrix, otherwise their rotation storage must be disjoint.
/// Coefficients retain their signed halfword representation (`ONE` is 1.0).
/// Changes no coordinate cache stamp and retains neither pointer.
static void _actor400500CopyRotation(const MATRIX* source, MATRIX* destination)
{
    destination->m[0][0] = source->m[0][0];
    destination->m[0][1] = source->m[0][1];
    destination->m[0][2] = source->m[0][2];
    destination->m[1][0] = source->m[1][0];
    destination->m[1][1] = source->m[1][1];
    destination->m[1][2] = source->m[1][2];
    destination->m[2][0] = source->m[2][0];
    destination->m[2][1] = source->m[2][1];
    destination->m[2][2] = source->m[2][2];
}

/// Per-frame entry point of the actor's task: runs the handler its state
/// selects from `D_actor_400500_80131E4C` - set-up, the per-frame state
/// machine, the death sequence and the post-death timeout. The table is a
/// local, so GCC copies it from `.rodata` onto the stack every frame.
void func_actor_400500_8013DE98(Task* task)
{
    TaskFuncTable4 states;

    states = D_actor_400500_80131E4C;
    states.funcs[task->state](task);
}

/// Runs the post-death arm cleanup or final enemy-destruction delay.
///
/// Requires live work/enemy in task state 3; work state must be 0 (kill arm
/// tasks) or 1 (wait 301 callbacks, then destroy). Copies both callbacks to a
/// local array and dispatches without a bounds check. The selected destruction
/// callback may end the task/work lifetime; nothing is accessed afterwards.
static void _actor400500TickTeardown(Task* task)
{
    enum { ACTOR_400500_TEARDOWN_STEP_COUNT = 2 };
    _Actor400500GrayStalkerWork* work                                    = task->work;
    TaskFunc                     steps[ACTOR_400500_TEARDOWN_STEP_COUNT] = {
        _actor400500KillArmModels,
        _actor400500WaitDestroyDelay,
    };

    steps[(s16)work->state](task);
}

/// Clears the active hit and its pending recoil code in a live Stalker work block.
///
/// The last hit's reaction remains available to death handling.
static void _actor400500ClearHitReaction(Task* task)
{
    _Actor400500GrayStalkerWork* work = (_Actor400500GrayStalkerWork*)task->work;

    work->hitTaken    = 0;
    work->hitReaction = ACTOR_400500_HIT_REACTION_NONE;
}

/// Idle per-frame callback of the left arm's attached model task.
///
/// The parent Stalker drives rotation and visibility beneath body part 10,
/// then kills this child during teardown. The task parameter is unused.
static void _actor400500LeftArmTask(Task* task)
{
}

/// Idle per-frame callback of the right arm's attached model task.
///
/// The parent Stalker drives rotation and visibility beneath body part 7,
/// then kills this child during teardown. The task parameter is unused.
static void _actor400500RightArmTask(Task* task)
{
}

/// Kills both retained arm tasks and starts the post-death destruction delay.
///
/// Requires live work; each non-NULL arm handle must still refer to its child.
/// Handles are retained after killing, so this teardown step must run once.
/// Clears the halfword tick counter and advances the teardown state.
static void _actor400500KillArmModels(Task* task)
{
    _Actor400500GrayStalkerWork* work;
    Task*                        armTask;
    s32                          armIndex;

    work = task->work;
    for (armIndex = 0; armIndex < ARRAY_SIZE(work->armTasks); armIndex++) {
        armTask = work->armTasks[armIndex];
        if (armTask != NULL) {
            taskKill(armTask);
        }
    }
    work->stateFrames = 0;
    work->state       = work->state + 1;
}

/// Destroys the Stalker's enemy and task on the 301st delay callback.
///
/// Requires live work and enemy after the arm teardown. The stored counter wraps
/// as u16 but the threshold compares its signed low halfword; entry starts at
/// zero and ticks once per callback. Destruction ends the work/model lifetime.
static void _actor400500WaitDestroyDelay(Task* task)
{
    enum { ACTOR_400500_DESTROY_DELAY_TICKS = 301 };
    _Actor400500GrayStalkerWork* work;
    u16                          elapsedTicks;

    work              = task->work;
    elapsedTicks      = work->stateFrames + 1;
    work->stateFrames = elapsedTicks;
    if ((s16)elapsedTicks >= ACTOR_400500_DESTROY_DELAY_TICKS) {
        enemyDestroy(task->spawnArg2.pointer, task);
    }
}
