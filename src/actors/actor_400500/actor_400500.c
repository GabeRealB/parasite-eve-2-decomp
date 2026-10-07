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

static void func_actor_400500_80132628(Task* task, s16 firstJoint, s16 secondJoint, s16 width, s32 height, s32 shade);
static void func_actor_400500_80138088(Task* task);
static s32  func_actor_400500_8013B720(GfxCoord* coord, MATRIX* matrix);

/* `D_800678F0` selects the model stream a following `effectSpawn` uses as the
 * source for the effect's own `TmdObject`.
 *
 * Storing to a bare `extern` pointer next to pointer-based struct traffic lets
 * GCC 2.8.1's `fixed_scalar_and_varying_struct_p` conclude the two cannot
 * alias, so the scheduler sinks the store past the `TmdObject` loads that
 * follow. Declared as a scalar, `func_actor_400500_80134B88` scores 87.27%
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

static void func_actor_400500_80132438(Task* arg0);
static void func_actor_400500_80132AB0(Task* arg0, s16 arg1, s32 arg2);
static s32  _actor400500TryStartAttack(Task* task);
static void func_actor_400500_80132E94(Task* arg0);
static s32  func_actor_400500_80133160(Task* arg0);
static s32  _actor400500HandleHeavyHitReaction(Task* task);
static s32  func_actor_400500_80133460(Task* arg0);
static void func_actor_400500_801335E8(Task* arg0);
static void func_actor_400500_80133B14(Task* arg0);
static void func_actor_400500_8013403C(Task* arg0);
static void func_actor_400500_8013456C(Task* arg0);
static void func_actor_400500_80135770(Task* arg0);
static void func_actor_400500_80135EBC(Task* arg0);
static void func_actor_400500_801361EC(Task* arg0);
static void func_actor_400500_8013662C(Task* arg0);
static void func_actor_400500_80136864(Task* arg0);
static void func_actor_400500_801369A4(Task* arg0);
static void func_actor_400500_80136B94(Task* arg0);
static void func_actor_400500_80136D00(Task* arg0);
static void func_actor_400500_80136EB8(Task* arg0);
static void func_actor_400500_80137034(Task* arg0);
static void func_actor_400500_801371A0(Task* arg0);
static void func_actor_400500_80137338(Task* arg0);
static void func_actor_400500_80137478(Task* arg0);
static void func_actor_400500_801385D0(Task* arg0);
static void func_actor_400500_801387E8(Task* arg0);
static void func_actor_400500_8013899C(Task* arg0);
static void func_actor_400500_80138B78(Task* arg0);
static void func_actor_400500_80138CE8(Task* arg0);
static void func_actor_400500_80138DC4(Task* arg0);
static void func_actor_400500_80138EA0(Task* arg0);
static void func_actor_400500_8013905C(Task* arg0);
static void func_actor_400500_801391B0(Task* arg0);
static void func_actor_400500_801392D8(Task* arg0);
static void func_actor_400500_80139448(Task* arg0);
static void func_actor_400500_801395D0(Task* arg0);
static void func_actor_400500_8013973C(Task* arg0);
static void func_actor_400500_80139AC4(Task* arg0);
static void func_actor_400500_80139C1C(Task* arg0);
static void func_actor_400500_80139D70(Task* arg0);
static void func_actor_400500_80139F6C(Task* arg0);
static void func_actor_400500_8013A0B8(Task* arg0);
static void func_actor_400500_8013A484(Task* arg0);
static void func_actor_400500_8013A5D8(Task* arg0);
static void func_actor_400500_8013A700(Task* arg0);
static void func_actor_400500_8013A8E4(Task* arg0);
static void func_actor_400500_8013AA98(Task* arg0);
static void func_actor_400500_8013ABE4(Task* arg0);
static void func_actor_400500_8013AD60(Task* arg0);
static void func_actor_400500_8013AF44(Task* arg0);
static void func_actor_400500_8013B228(Task* arg0);
static void func_actor_400500_8013B374(Task* arg0);
static void func_actor_400500_8013B4A4(Task* arg0);
static void func_actor_400500_8013B5E0(Task* arg0);
static void func_actor_400500_8013BA24(Task* arg0);
static void func_actor_400500_8013BAA4(Task* arg0);
static void func_actor_400500_8013BB18(Task* arg0);
static void func_actor_400500_8013BBB0(Task* arg0);
static void func_actor_400500_8013BC9C(Task* arg0);
static void func_actor_400500_8013BCCC(Task* arg0);
static void func_actor_400500_8013BD64(Task* arg0);
static void func_actor_400500_8013BE50(Task* arg0);
static void func_actor_400500_8013BEC4(Task* arg0);
static void func_actor_400500_8013BFB0(Task* arg0);
static void func_actor_400500_8013C018(Task* arg0);
static void func_actor_400500_8013C108(Task* arg0);
static void func_actor_400500_8013C174(Task* arg0);
static void func_actor_400500_8013C218(Task* arg0);
static void func_actor_400500_8013C348(Task* arg0);
static void func_actor_400500_8013C3C4(Task* arg0);
static void func_actor_400500_8013C474(Task* arg0);
static void func_actor_400500_8013C508(Task* arg0);
static void func_actor_400500_8013C578(Task* arg0);
static void func_actor_400500_8013C61C(Task* arg0);
static void func_actor_400500_8013C750(Task* arg0);
static void func_actor_400500_8013C7A4(Task* arg0);
static void func_actor_400500_8013C818(Task* task);
static void func_actor_400500_8013C908(Task* arg0);
static void func_actor_400500_8013C9D4(Task* arg0);
static void func_actor_400500_8013CA38(Task* arg0);
static void func_actor_400500_8013CB0C(Task* arg0);
static void func_actor_400500_8013CBD8(Task* arg0);
static void func_actor_400500_8013CCDC(Task* arg0);
static void func_actor_400500_8013CDA8(Task* arg0);
static void func_actor_400500_8013CE9C(Task* arg0);
static void func_actor_400500_8013CF68(Task* arg0);
static void func_actor_400500_8013D078(Task* arg0);
static void func_actor_400500_8013D144(Task* arg0);
static void func_actor_400500_8013D210(Task* arg0);
static void func_actor_400500_8013D274(Task* arg0);
static void func_actor_400500_8013D2D8(Task* arg0);
static void func_actor_400500_8013D3B8(Task* arg0);
static void func_actor_400500_8013D420(Task* arg0);
static void func_actor_400500_8013D4F0(Task* arg0);
static void func_actor_400500_8013D59C(Task* arg0);
static void func_actor_400500_8013D630(Task* arg0);
static void func_actor_400500_8013D6A0(Task* arg0);
static void func_actor_400500_8013D744(Task* arg0);
static void func_actor_400500_8013D878(Task* arg0);
static void func_actor_400500_8013D8CC(Task* arg0);
static void func_actor_400500_8013D958(Task* arg0);
static void func_actor_400500_8013D9DC(Task* arg0);
static void func_actor_400500_8013D9F4(Task* arg0);
static void func_actor_400500_8013DA24(Task* arg0);
static void func_actor_400500_8013DA68(Task* arg0);
static void func_actor_400500_8013DACC(Task* arg0);
static void _actor400500EnterState(Task* task, s16 state);
static s32  _actor400500TryTurnOverNearTarget(Task* task);
static void func_actor_400500_8013DBCC(Task* arg0, s16 arg1, SVECTOR3* arg2);
static void _actor400500ResetAnimSlots(Task* task);
static void _actor400500RequestAnimReset(Task* task, s16 setIndex, s16 rateSixteenths);
static void _actor400500BlendAnimSlots(Task* task);
static s16  _actor400500RescaleAnimFrames(Task* task, s16 frames);
static s32  _actor400500CheckAnimBoundary(Task* task);
static void _actor400500CopyRotation(const MATRIX* source, MATRIX* destination);
static void func_actor_400500_8013DEFC(Task* arg0);
static void func_actor_400500_8013DF50(Task* arg0);
static void func_actor_400500_8013DF74(Task* arg0);
static void func_actor_400500_8013DFE4(Task* arg0);

static TmdSource _gActor400500GrayStalkerBody;
void             func_actor_400500_8013DE98(Task*);

static TmdSource _gActor400500GrayStalkerBurstArmRight;
static TmdSource _gActor400500GrayStalkerBurstArmLeft;
s32              func_actor_400500_8013DAE4(Task*, s32, u16*, s32);
void             func_actor_400500_8013DF64(Task*);
void             func_actor_400500_8013DF6C(Task*);

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
    { ACTOR_COMMAND_MESSAGE_APPLY, func_actor_400500_8013DAE4 },
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
    { { { TASK_BODY_TMD, 96 } }, func_actor_400500_8013DF64, { .model = &_gActor400500GrayStalkerBurstArmLeft } },
    { { { TASK_BODY_TMD, 96 } }, func_actor_400500_8013DF6C, { .model = &_gActor400500GrayStalkerBurstArmRight } },
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

static void               func_actor_400500_80132000(Task* arg0);
static void               func_actor_400500_8013226C(Task* arg0);
static void               func_actor_400500_80132C54(Task* arg0);
static inline void        _actor400500SetState(Task* task, s32 state, s32 subState);
static inline void        _actor400500PlaySound(Task* task, s32 id);
static void               func_actor_400500_801348D8(Task* arg0, s32 arg1);
static void               func_actor_400500_80134B88(Task* arg0);
static void               func_actor_400500_80135414(Task* arg0);
static __inline__ s32     lookup_zone(Task* task);
static __inline__ VECTOR* push_color(GfxCoord* coord);
static __inline__ void    pop_scratch(s32 n);
static __inline__ u8*     push_proj(void);
static void               func_actor_400500_801375B8(Task* arg0);
static inline void        _actor400500RequestMode(Task* task, s32 mode);
static inline s32         _actor400500CoordToView(GfxCoord* coord, MATRIX* matrix);
static inline void        _actor400500TurnPart(GfxCoord* part, u16 heading);
static inline s16         _actor400500PlayerDistance(GfxCoord* part);
static inline void        _actor400500PlayAnim(Task* task, s32 id);
static void               func_actor_400500_8013771C(Task* arg0);
static inline void        _actor400500UpdateColor(Task* arg0, GfxCoord* coord, TmdObject* obj);

static void func_actor_400500_80132000(Task* arg0)
{
    _Actor400500GrayStalkerWork* work;

    work = (_Actor400500GrayStalkerWork*)arg0->work;

    work->body.coord            = &arg0->extra.tmd->coords[3];
    work->body.context.contacts = work->bodyContacts;
    work->body.pos.vz           = 0x110;
    work->body.pos.vx           = 0;
    work->body.pos.vy           = 0;
    work->body.key              = 0x30005;
    work->body.radius           = 0x260;
    work->body.flags            = WORLD_COLLISION_BODY_SPHERE;
    worldCollisionLinkBody(WORLD_COLLISION_LIST_ENEMY_BODIES, &work->body);
    worldCollisionInitContacts(work->bodyContacts, ARRAY_SIZE(work->bodyContacts), 0);
    work->body.flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;

    work->rightArmOuter.key              = damagePackEnemyAttackKey(arg0->spawnArg2.pointer, 0);
    work->rightArmOuter.coord            = &arg0->extra.tmd->coords[7];
    work->rightArmOuter.context.contacts = work->rightArmContacts;
    work->rightArmOuter.pos.vx           = -0x460;
    work->rightArmOuter.pos.vy           = 0;
    work->rightArmOuter.pos.vz           = 0;
    work->rightArmOuter.radius           = 0x290;
    work->rightArmOuter.flags            = WORLD_COLLISION_BODY_SPHERE;
    worldCollisionLinkBody(WORLD_COLLISION_LIST_ENEMY_ATTACKS, &work->rightArmOuter);
    worldCollisionInitContacts(work->rightArmContacts, ARRAY_SIZE(work->rightArmContacts), 0);
    work->rightArmOuter.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);

    work->rightArmInner.key              = damagePackEnemyAttackKey(arg0->spawnArg2.pointer, 0);
    work->rightArmInner.coord            = &arg0->extra.tmd->coords[7];
    work->rightArmInner.context.contacts = work->rightArmContacts;
    work->rightArmInner.pos.vx           = -0x200;
    work->rightArmInner.pos.vy           = 0;
    work->rightArmInner.pos.vz           = 0;
    work->rightArmInner.radius           = 0x250;
    work->rightArmInner.flags            = WORLD_COLLISION_BODY_SPHERE;
    worldCollisionLinkBody(WORLD_COLLISION_LIST_ENEMY_ATTACKS, &work->rightArmInner);
    worldCollisionInitContacts(work->rightArmContacts, ARRAY_SIZE(work->rightArmContacts), 0);
    work->rightArmInner.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);

    work->leftArmOuter.key              = damagePackEnemyAttackKey(arg0->spawnArg2.pointer, 0);
    work->leftArmOuter.coord            = &arg0->extra.tmd->coords[10];
    work->leftArmOuter.context.contacts = work->leftArmContacts;
    work->leftArmOuter.pos.vx           = 0x460;
    work->leftArmOuter.pos.vy           = 0;
    work->leftArmOuter.pos.vz           = 0;
    work->leftArmOuter.radius           = 0x290;
    work->leftArmOuter.flags            = WORLD_COLLISION_BODY_SPHERE;
    worldCollisionLinkBody(WORLD_COLLISION_LIST_ENEMY_ATTACKS, &work->leftArmOuter);
    worldCollisionInitContacts(work->leftArmContacts, ARRAY_SIZE(work->leftArmContacts), 0);
    work->leftArmOuter.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);

    work->leftArmInner.key              = damagePackEnemyAttackKey(arg0->spawnArg2.pointer, 0);
    work->leftArmInner.coord            = &arg0->extra.tmd->coords[10];
    work->leftArmInner.context.contacts = work->leftArmContacts;
    work->leftArmInner.pos.vx           = 0x200;
    work->leftArmInner.pos.vy           = 0;
    work->leftArmInner.pos.vz           = 0;
    work->leftArmInner.radius           = 0x250;
    work->leftArmInner.flags            = WORLD_COLLISION_BODY_SPHERE;
    worldCollisionLinkBody(WORLD_COLLISION_LIST_ENEMY_ATTACKS, &work->leftArmInner);
    worldCollisionInitContacts(work->leftArmContacts, ARRAY_SIZE(work->leftArmContacts), 0);
    work->leftArmInner.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
}

static void func_actor_400500_8013226C(Task* arg0)
{
    _Actor400500GrayStalkerWork* work;
    GfxMatrix                    rot;
    GfxCoord*                    parts;
    GfxCoord*                    part7;
    GfxCoord*                    part10;
    GfxCoord*                    coord;
    Task*                        child;
    TmdObject*                   extra;
    TmdObject*                   tmd;
    TmdObject*                   parentTmd;

    parts             = arg0->extra.tmd->coords;
    work              = (_Actor400500GrayStalkerWork*)arg0->work;
    part7             = &parts[7];
    part10            = &parts[10];
    child             = taskSpawnFromTable(D_actor_400500_80153D48, 0, 0, 0);
    work->armTasks[0] = child;
    extra             = child->extra.tmd;
    coord             = extra->coords;
    extra->flags      = TMD_OBJECT_SKIP_ACTIVE_DRAW;
    coord->parent     = part10;
    coord->coord.t[0] = 0x400;
    coord->coord.t[1] = 0;
    coord->coord.t[2] = 0;
    gfxSetRotIdentity(&rot.mat);
    RotMatrixY((s16)(-0x180), &rot.mat);
    _actor400500CopyRotation(&rot.mat, &coord->coord);
    parentTmd              = arg0->extra.tmd;
    tmd                    = child->extra.tmd;
    tmd->texturePageOffset = parentTmd->texturePageOffset;
    tmd->clutRowOffset     = parentTmd->clutRowOffset;
    if (tmd->buffer != NULL) {
        tmdBuildBufferHalf(tmd);
        tmdBuildBufferHalf(tmd);
    }
    child                  = taskSpawnFromTable(D_actor_400500_80153D48, 1, 0, 0);
    work->armTasks[1]      = child;
    extra                  = child->extra.tmd;
    coord                  = extra->coords;
    extra->flags           = TMD_OBJECT_SKIP_ACTIVE_DRAW;
    coord->parent          = part7;
    coord->coord.t[0]      = -0x400;
    coord->coord.t[1]      = 0;
    coord->coord.t[2]      = 0;
    parentTmd              = arg0->extra.tmd;
    tmd                    = child->extra.tmd;
    tmd->texturePageOffset = parentTmd->texturePageOffset;
    tmd->clutRowOffset     = parentTmd->clutRowOffset;
    if (tmd->buffer != NULL) {
        tmdBuildBufferHalf(tmd);
        tmdBuildBufferHalf(tmd);
    }
    gfxSetRotIdentity(&rot.mat);
    RotMatrixY((s16)(0x180), &rot.mat);
    _actor400500CopyRotation(&rot.mat, &coord->coord);
}

static void func_actor_400500_80132438(Task* arg0)
{
    SVECTOR                      dir;
    SVECTOR*                     dirp;
    SVECTOR                      delta;
    GfxMatrix                    rot;
    _Actor400500GrayStalkerWork* work;
    GfxCoord*                    coord;
    GfxCoord*                    other;
    s16                          dist;
    s16                          heading;
    s16                          vz;
    s32                          y;
    s32                          z;
    u16                          counter;

    work  = (_Actor400500GrayStalkerWork*)arg0->work;
    coord = arg0->extra.tmd->coords;
    if (gPlayerActorTasks[PLAYER_ACTOR_TASK_PLAYER] != NULL) {
        other            = gPlayerActorTasks[PLAYER_ACTOR_TASK_PLAYER]->extra.tmd->coords;
        work->rootPos.vx = coord->coord.t[0];
        work->rootPos.vy = coord->coord.t[1];
        work->rootPos.vz = coord->coord.t[2];
        if ((s16)work->playerZone != 5) {
            dir.vx = (u16)other->coord.t[0] - (u16)coord->coord.t[0];
            dir.vy = (u16)other->coord.t[1] - (u16)coord->coord.t[1];
            dir.vz = (u16)other->coord.t[2] - (u16)coord->coord.t[2];
        } else {
            work->attackCooldown = 2;
            counter              = work->patrolFrames + 1;
            work->patrolFrames   = counter;
            if (!(counter & 0x100)) {
                dir.vx = 0x2710 - (u16)coord->coord.t[0];
            } else {
                dir.vx = 0x3E8 - (u16)coord->coord.t[0];
            }
            y      = -0x3E8;
            dir.vy = y - (u16)coord->coord.t[1];
            z      = -0x20D0;
            dir.vz = z - (u16)coord->coord.t[2];
        }
        dist = SquareRoot0((dir.vx * dir.vx) + (dir.vz * dir.vz));
        do {
            work->toTarget.vx = (u16)dir.vx;
            dirp              = &dir;
            work->toTarget.vy = (u16)dir.vy;
        } while (0);
        vz                = (u16)dir.vz;
        work->targetDist  = dist;
        work->toTarget.vz = vz;
        VectorNormalSS(dirp, dirp);
        work->targetBearing = (ratan2(dir.vx, dir.vz) - (u16)work->yaw) & 0xFFF;
        delta.vx            = (u16)other->coord.t[0] - (u16)work->playerPrevPos.vx;
        delta.vy            = (u16)other->coord.t[1] - (u16)work->playerPrevPos.vy;
        delta.vz            = (u16)other->coord.t[2] - (u16)work->playerPrevPos.vz;
        gfxSetRotIdentity(&rot.mat);
        heading = work->yaw;
        RotMatrixY(-heading, &rot.mat);
        ApplyMatrixSV(&rot.mat, &delta, &work->playerLocalMove);
    }
}

static void func_actor_400500_80132628(Task* task, s16 firstJoint, s16 secondJoint, s16 width, s32 height, s32 shade)
{
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
    long      perspective;
    long      flags;
    s16       angle;
    GfxCoord* secondCoord;
    GfxCoord* firstCoord;
    s32       offset0;
    s32       offset1;
    s32       offset2;
    s32       offset3;
    s32       halfX;
    s32       halfZ;
    s32       depth;
    GfxCoord* coords;
    GfxCoord* viewCoord;
    POLY_FT4* poly;
    u8        room;
    u8        col;

    col         = shade;
    coords      = task->extra.tmd->coords;
    firstCoord  = coords + firstJoint;
    secondCoord = coords + secondJoint;
    if (firstJoint != secondJoint) {
        actorRenderComposeCoord(firstCoord);
        actorRenderComposeCoord(secondCoord);
        gfxMakeRelativeTransform(&gGfxViewCoord.workm, &firstCoord->workm, &firstMatrix);
        gfxMakeRelativeTransform(&gGfxViewCoord.workm, &secondCoord->workm, &secondMatrix);
        first.vy   = (s16)height;
        second.vy  = (s16)height;
        first.vx   = firstMatrix.t[0];
        first.vz   = firstMatrix.t[2];
        second.vx  = secondMatrix.t[0];
        second.vz  = secondMatrix.t[2];
        angle      = ratan2((s16)secondMatrix.t[0] - (s16)firstMatrix.t[0], (s16)secondMatrix.t[2] - (s16)firstMatrix.t[2]);
        halfX      = (first.vx - second.vx) / 2;
        halfZ      = (first.vz - second.vz) / 2;
        offset0    = rcos(angle) * width;
        corner0.vy = (s16)height;
        corner0.vx = halfX + (first.vx - (offset0 >> 0xC));
        corner0.vz = halfZ + (first.vz + ((s32)(rsin(angle) * width) >> 0xC));
        offset1    = rcos(angle) * width;
        corner1.vy = (s16)height;
        corner1.vx = halfX + (first.vx + (offset1 >> 0xC));
        corner1.vz = halfZ + (first.vz - ((s32)(rsin(angle) * width) >> 0xC));
        offset2    = rcos(angle) * width;
        corner2.vy = (s16)height;
        corner2.vx = (second.vx - (offset2 >> 0xC)) - halfX;
        corner2.vz = (second.vz + ((s32)(rsin(angle) * width) >> 0xC)) - halfZ;
        offset3    = rcos(angle) * width;
        corner3.vy = (s16)height;
        corner3.vx = (second.vx + (offset3 >> 0xC)) - halfX;
        corner3.vz = (second.vz - ((s32)(rsin(angle) * width) >> 0xC)) - halfZ;
        /* `gGfxViewCoord`, reached back from its `workm`: the address is built
           from `gGfxViewCoord.workm`, whose high half the GTE loads below share. */
        viewCoord               = &gGfxViewCoord;
        viewCoord->composeStamp = GRAPHICS_COORD_DIRTY;
        actorRenderComposeCoord(viewCoord);
        gte_SetRotMatrix(&gGfxViewCoord.workm);
        gte_SetTransMatrix(&gGfxViewCoord.workm);
        depth = RotTransPers4(&corner0, &corner1, &corner2, &corner3, &screen0, &screen1, &screen2, &screen3,
                              &perspective, &flags);
        if (flags >= 0) {
            poly           = gGpuPrimCursor;
            gGpuPrimCursor = poly + 1;
            setlen(poly, 9);
            poly->code                     = 0x2E;
            GPU_PRIMITIVE_XY_WORD(poly, 0) = screen0;
            GPU_PRIMITIVE_XY_WORD(poly, 1) = screen1;
            GPU_PRIMITIVE_XY_WORD(poly, 2) = screen2;
            GPU_PRIMITIVE_XY_WORD(poly, 3) = screen3;
            setUV4(poly, 0xC0, 0x98, 0xF7, 0x98, 0xC0, 0xCF, 0xF7, 0xCF);
            poly->tpage = 0x48;
            poly->clut  = 0x4283;
            room        = gGameSession->location.loc.room;
            if ((room == 1) || (room == 3) || (room == 5) || (room == 6)) {
                poly->r0 = col;
                poly->g0 = col;
                poly->b0 = col;
            } else {
                poly->r0 = shade;
                poly->g0 = col >> 1;
                poly->b0 = shade;
            }
            addPrim((&gGpuCurrentOt[((((u32)(depth << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)) / sizeof(*gGpuCurrentOt)]), poly);
        }
    }
}

static void func_actor_400500_80132AB0(Task* arg0, s16 arg1, s32 arg2)
{
    s32 temp_s2;

    temp_s2 = arg2 & 0xFF;
    func_actor_400500_80132628(arg0, 3, 9, 0x100, (s32)arg1, temp_s2);
    func_actor_400500_80132628(arg0, 9, 0xA, 0x100, (s32)arg1, temp_s2);
    func_actor_400500_80132628(arg0, 0xA, 0xB, 0x100, (s32)arg1, temp_s2);
    func_actor_400500_80132628(arg0, 3, 6, 0x100, (s32)arg1, temp_s2);
    func_actor_400500_80132628(arg0, 6, 7, 0x100, (s32)arg1, temp_s2);
    func_actor_400500_80132628(arg0, 7, 8, 0x100, (s32)arg1, temp_s2);
    func_actor_400500_80132628(arg0, 1, 5, 0x100, (s32)arg1, temp_s2);
    func_actor_400500_80132628(arg0, 1, 0xC, 0x100, (s32)arg1, temp_s2);
    func_actor_400500_80132628(arg0, 0xC, 0xD, 0x100, (s32)arg1, temp_s2);
    func_actor_400500_80132628(arg0, 0xD, 0xE, 0x100, (s32)arg1, temp_s2);
    func_actor_400500_80132628(arg0, 1, 0xF, 0x100, (s32)arg1, temp_s2);
    func_actor_400500_80132628(arg0, 0xF, 0x10, 0x100, (s32)arg1, temp_s2);
    func_actor_400500_80132628(arg0, 0x10, 0x11, 0x100, (s32)arg1, temp_s2);
}

static void func_actor_400500_80132C54(Task* arg0)
{
    _Actor400500GrayStalkerWork* work;
    GfxMatrix                    rot;
    GfxCoord*                    coord;
    s32                          tx;

    work  = (_Actor400500GrayStalkerWork*)arg0->work;
    coord = arg0->extra.tmd->coords;
    switch (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.warp) {
        case 1:
            tx                = 0x800;
            work->yaw         = tx;
            work->roll        = tx;
            tx                = 0x14A0;
            coord->coord.t[0] = tx;
            tx                = -0xFA0;
            coord->coord.t[1] = tx;
            tx                = -0x209E;
            coord->coord.t[2] = tx;
            break;
        case 2:
            tx                = 0x800;
            work->roll        = tx;
            tx                = 0x4074;
            work->yaw         = 0;
            coord->coord.t[0] = tx;
            tx                = -0xFA0;
            coord->coord.t[1] = tx;
            tx                = -0x209E;
            coord->coord.t[2] = tx;
            break;
        case 3:
            tx                = 0xC00;
            work->yaw         = tx;
            tx                = 0x800;
            work->roll        = tx;
            tx                = 0xFA0;
            coord->coord.t[0] = tx;
            tx                = -0xFA0;
            coord->coord.t[1] = tx;
            tx                = -0x209E;
            coord->coord.t[2] = tx;
            break;
    }
    gfxSetRotIdentity(&rot.mat);
    RotMatrixZ(work->roll, &rot.mat);
    RotMatrixY(work->yaw, &rot.mat);
    _actor400500CopyRotation(&rot.mat, &coord->coord);
    func_actor_400500_8013DBCC(arg0, 0xB, &work->anchorPos);
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

static void func_actor_400500_80132E94(Task* arg0)
{
    _Actor400500GrayStalkerWork* work;
    Enemy*                       enemy;
    TmdObject*                   extra;

    enemy = (Enemy*)arg0->spawnArg2.pointer;
    work  = (_Actor400500GrayStalkerWork*)arg0->work;
    extra = arg0->extra.tmd;
    if (work->cloakRequest < 0) {
        if (!((u8)work->cloakRequest & ACTOR_400500_CLOAK_SHOW)) {
            switch (work->cloakPhase) {
                case 0:
                    work->cloakLevel = (u16)work->cloakLevel + ((s16)(0xFF - (u16)work->cloakLevel) >> 2);
                    if (work->cloakLevel >= 0xF8) {
                        work->cloakLevel = 0xFF;
                        work->cloakTimer = 0;
                        work->cloakPhase = (u8)work->cloakPhase + 1;
                    }
                    modelLightingSetLayerMaterials(work->cloakLevel);
                    break;
                case 1:
                    work->cloakTimer = (u16)work->cloakTimer + 1;
                    if (work->hideHoldFrames < work->cloakTimer) {
                        work->cloakPhase = (u8)work->cloakPhase + 1;
                    }
                    break;
                case 2:
                    work->colorBlend  = (u16)work->colorBlend + ((s16) - (u16)work->colorBlend >> 2);
                    work->shadowShade = (u16)work->shadowShade + (-work->shadowShade >> 2);
                    if (work->colorBlend == 0) {
                        enemy->node.state.parts.flags = WORLD_TARGET_NOT_LOCKABLE;
                        if (work->commandActive == 0) {
                            enemy->node.state.parts.flags = (WORLD_TARGET_NOT_LOCKABLE | WORLD_TARGET_KEEP_SCANNED);
                        }
                        work->shadowShade  = 0;
                        work->cloakRequest = 0;
                        extra->flags      |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
                    }
                    extra->shading.colorBlend = work->colorBlend;
                    break;
            }
        } else {
            switch (work->cloakPhase) {
                case 0:
                    enemy->node.state.parts.flags = 0;
                    if (work->commandActive == 0) {
                        enemy->node.state.parts.flags = WORLD_TARGET_KEEP_SCANNED;
                    }
                    extra->flags     &= ~TMD_OBJECT_SKIP_ACTIVE_DRAW;
                    work->colorBlend  = (u16)work->colorBlend + ((s16)(TMD_OBJECT_COLOR_BLEND_ONE - (u16)work->colorBlend) >> 2);
                    work->shadowShade = (u16)work->shadowShade + ((0xFF - work->shadowShade) >> 2);
                    if (work->colorBlend >= TMD_OBJECT_COLOR_BLEND_ONE - 16) {
                        work->shadowShade = 0xFF;
                        work->colorBlend  = TMD_OBJECT_COLOR_BLEND_ONE;
                        work->cloakTimer  = 0;
                        work->cloakPhase  = (u8)work->cloakPhase + 1;
                    }
                    extra->shading.colorBlend = work->colorBlend;
                    break;
                case 1:
                    work->cloakTimer = (u16)work->cloakTimer + 1;
                    if (work->cloakTimer >= 0x11) {
                        work->cloakPhase = (u8)work->cloakPhase + 1;
                    }
                    break;
                case 2:
                    work->cloakLevel = (u16)work->cloakLevel + ((s16) - (u16)work->cloakLevel >> 2);
                    if (work->cloakLevel < 9) {
                        work->cloakLevel   = 0;
                        work->cloakRequest = 0;
                        func_actor_400500_8013B4A4(arg0);
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

static s32 func_actor_400500_80133160(Task* arg0)
{
    _Actor400500GrayStalkerWork* work;
    s32                          soundId;
    s32                          pan;
    u16                          heading;

    work = (_Actor400500GrayStalkerWork*)arg0->work;
    if (work->turnRequest == ACTOR_400500_TURN_YAW_DOWN) {
        if (work->turnStarted == 0) {
            _actor400500RequestAnimReset(arg0, 0x17, 0x10);
            work->stateFrames = 0;
            work->turnStarted = 1;
        }
        work->stateFrames = work->stateFrames + 1;
        if ((work->stateFrames & 0xF) == 8) {
            soundId = ((((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x40050001;
            pan     = worldCoordGetOriginAudioPan(arg0->extra.tmd->coords);
            pan   <<= 24;
            pan   >>= 24;
            sndEvtRequestScriptStart(soundId, pan, (s8)worldCoordGetOriginAudioDepth(arg0->extra.tmd->coords));
        }
        heading   = (u16)work->yaw - D_actor_400500_80153DB4[work->stateFrames & 0xF];
        work->yaw = heading;
        if ((_actor400500CheckAnimBoundary(arg0) << 0x10) != 0) {
            work->stateFrames = 0;
            work->turnRequest = ACTOR_400500_TURN_NONE;
            work->yaw         = (u16)work->yaw & 0xE00;
        }
        func_actor_400500_8013DF50(arg0);
        return 1;
    }
    if (work->turnRequest == ACTOR_400500_TURN_YAW_UP) {
        if (work->turnStarted == 0) {
            _actor400500RequestAnimReset(arg0, 0x18, 0x10);
            work->stateFrames = 0;
            work->turnStarted = 1;
        }
        work->stateFrames = work->stateFrames + 1;
        if ((work->stateFrames & 0xF) == 8) {
            soundId = ((((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x40050002;
            pan     = worldCoordGetOriginAudioPan(arg0->extra.tmd->coords);
            pan   <<= 24;
            pan   >>= 24;
            sndEvtRequestScriptStart(soundId, pan, (s8)worldCoordGetOriginAudioDepth(arg0->extra.tmd->coords));
        }
        heading   = (u16)work->yaw + D_actor_400500_80153DB4[work->stateFrames & 0xF];
        work->yaw = heading;
        if ((_actor400500CheckAnimBoundary(arg0) << 0x10) != 0) {
            work->stateFrames = 0;
            work->turnRequest = ACTOR_400500_TURN_NONE;
            work->yaw         = (u16)work->yaw & 0xE00;
        }
        func_actor_400500_8013DF50(arg0);
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

static s32 func_actor_400500_80133460(Task* arg0)
{
    _Actor400500GrayStalkerWork* work;
    _Actor400500GrayStalkerWork* work2;
    _Actor400500GrayStalkerWork* hit;
    s16                          mode;
    s16                          sub;
    s32                          flag;
    s32                          cond;

    work = (_Actor400500GrayStalkerWork*)arg0->work;
    mode = work->hitTaken;
    if (mode == 1) {
        sub = work->hitReaction;
        if (sub == mode) {
            if ((work->cloakRequest >= 0) || (((u8)work->cloakRequest & ACTOR_400500_CLOAK_KIND_MASK) != mode)) {
                flag               = ACTOR_400500_CLOAK_RUNNING | ACTOR_400500_CLOAK_SHOW;
                work->cloakRequest = flag;
                work->cloakPhase   = 0;
            }
            work2              = (_Actor400500GrayStalkerWork*)arg0->work;
            work2->animRate    = 0x1C;
            work2->animId      = 0xB;
            work2->animRequest = ACTOR_400500_ANIM_REQUEST_RESET;
            work->hitReaction  = ACTOR_400500_HIT_REACTION_NONE;
        } else if (sub == ACTOR_400500_HIT_REACTION_HEAVY) {
            if ((work->cloakRequest >= 0) || (((u8)work->cloakRequest & ACTOR_400500_CLOAK_KIND_MASK) != mode)) {
                flag               = ACTOR_400500_CLOAK_RUNNING | ACTOR_400500_CLOAK_SHOW;
                work->cloakRequest = flag;
                work->cloakPhase   = 0;
            }
            work2              = (_Actor400500GrayStalkerWork*)arg0->work;
            work2->animRate    = ANIMATION_RATE_ONE;
            work2->animId      = 0xC;
            work2->animRequest = ACTOR_400500_ANIM_REQUEST_RESET;
            work->hitReaction  = ACTOR_400500_HIT_REACTION_NONE;
        } else if (sub == ACTOR_400500_HIT_REACTION_BLAST) {
            if ((work->cloakRequest >= 0) || (((u8)work->cloakRequest & ACTOR_400500_CLOAK_KIND_MASK) != mode)) {
                flag               = ACTOR_400500_CLOAK_RUNNING | ACTOR_400500_CLOAK_SHOW;
                work->cloakRequest = flag;
                work->cloakPhase   = 0;
            }
            work2              = (_Actor400500GrayStalkerWork*)arg0->work;
            work2->animRate    = ANIMATION_RATE_ONE;
            work2->animId      = 0xC;
            work2->animRequest = ACTOR_400500_ANIM_REQUEST_RESET;
            work->hitReaction  = ACTOR_400500_HIT_REACTION_NONE;
        } else if (sub == ACTOR_400500_HIT_REACTION_STATUS) {
            if ((work->cloakRequest >= 0) || (((u8)work->cloakRequest & ACTOR_400500_CLOAK_KIND_MASK) != mode)) {
                flag               = ACTOR_400500_CLOAK_RUNNING | ACTOR_400500_CLOAK_SHOW;
                work->cloakRequest = flag;
                work->cloakPhase   = 0;
            }
            work2              = (_Actor400500GrayStalkerWork*)arg0->work;
            work2->animRate    = ANIMATION_RATE_ONE;
            work2->animId      = 0xE;
            work2->animRequest = ACTOR_400500_ANIM_REQUEST_RESET;
            work->hitReaction  = ACTOR_400500_HIT_REACTION_NONE;
        }
        hit = (_Actor400500GrayStalkerWork*)arg0->work;
        if ((hit->rig.slots[1].status.fields.flags & ANIMATION_SLOT_REACHED_BOUNDARY) ||
            (hit->rig.slots[1].status.word & (ANIMATION_SLOT_FOLLOWED_JUMP | ANIMATION_SLOT_SETTLED))) {
            cond = 1;
        } else {
            cond = 0;
        }
        if (cond) {
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

/// Writes `state` and `subState` into the enemy's state and sub-state indices.
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

/// Consumes a pending knockdown: clears the request, enters the knockdown
/// state and returns 1; returns 0 when none is pending.
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

/// Queues sound `id` from the enemy's position, in its placement's sound bank.
static inline void _actor400500PlaySound(Task* task, s32 id)
{
    s32 sound;
    s32 pan;

    sound = ((((Enemy*)task->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | id;
    pan   = (s8)worldCoordGetOriginAudioPan(task->extra.tmd->coords);
    sndEvtRequestScriptStart(sound, pan, (s8)worldCoordGetOriginAudioDepth(task->extra.tmd->coords));
}

/// Replaces the rotation of `coord` with a rotation about Y by `yaw`.
static inline void _actor400500SetCoordYaw(GfxCoord* coord, s16 yaw)
{
    MATRIX rot;

    gfxSetRotIdentity(&rot);
    RotMatrixY(yaw, &rot);
    _actor400500CopyRotation(&rot, &coord->coord);
}

/// Plays animation 2 at rate 0x18, moving the root so that node 0xB holds its
/// world-space position from frame 0 to 0xB00 / rate / 16 and node 8 from
/// 0xC00 / rate / 16 to 0x1500 / rate / 16, each with a sound on its first
/// frame. The cycle restarts at frame 0 on a slot-1 animation boundary, jump or hold.
static void func_actor_400500_801335E8(Task* arg0)
{
    _Actor400500GrayStalkerWork* work;
    GfxCoord*                    coord;
    u8                           tmp0;
    u8                           tmp1;
    u8                           tmp2;
    u8                           end0;
    u8                           start1;
    u8                           end1;
    /* The first window starts at frame 0: `start0` is a `u8` bound like the
     * other three, assigned first. Its two tests then compare against a
     * register the image zeroes at the first of them; a literal 0 folds. */
    u8  start0;
    s32 sound;

    work  = (_Actor400500GrayStalkerWork*)arg0->work;
    coord = arg0->extra.tmd->coords;
    if (work->animId != 2) {
        _actor400500SetAnim(arg0, 2, 0x18);
        _actor400500TickAnim(arg0);
    }
    start0 = 0;
    if (((_Actor400500GrayStalkerWork*)arg0->work)->animRate == 0) {
        tmp0 = 0;
    } else {
        tmp0 = (u32)(0xB00 / ((_Actor400500GrayStalkerWork*)arg0->work)->animRate) >> 4;
    }
    end0 = tmp0;
    if (((_Actor400500GrayStalkerWork*)arg0->work)->animRate == 0) {
        tmp1 = 0;
    } else {
        tmp1 = (u32)(0xC00 / ((_Actor400500GrayStalkerWork*)arg0->work)->animRate) >> 4;
    }
    start1 = tmp1;
    if (((_Actor400500GrayStalkerWork*)arg0->work)->animRate == 0) {
        tmp2 = 0;
    } else {
        tmp2 = (u32)(0x1500 / ((_Actor400500GrayStalkerWork*)arg0->work)->animRate) >> 4;
    }
    end1 = tmp2;
    if (_actor400500HasAnimBoundary(arg0)) {
        work->animFrames = 0;
    }
    if (work->animFrames == start0) {
        _actor400500ReadPartWorldXZ(arg0, 0xB, &work->anchorPos);
        sound = ((((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x40050001;
        _actor400500EnqueueSound(arg0, sound);
    }
    if (work->animFrames == start1) {
        _actor400500ReadPartWorldXZ(arg0, 8, &work->anchorPos);
        sound = ((((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x40050002;
        _actor400500EnqueueSound(arg0, sound);
    }
    if (work->animFrames >= start0 && work->animFrames <= end0) {
        _actor400500PinPartWorldXZ(arg0, 0xB, &work->anchorPos);
    }
    if (work->animFrames >= start1 && work->animFrames <= end1) {
        _actor400500PinPartWorldXZ(arg0, 8, &work->anchorPos);
    }
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
}

/// Plays animation 2 at the current rate, moving the root so that node 0xB
/// holds its world-space position from frame 0 to 0xB00 / rate / 16 and node 8
/// from 0xC00 / rate / 16 to 0x1500 / rate / 16, each with a sound on its
/// first frame. The cycle restarts at frame 0 on a slot-1 animation boundary, jump or hold.
static void func_actor_400500_80133B14(Task* arg0)
{
    _Actor400500GrayStalkerWork* work;
    GfxCoord*                    coord;
    u8                           tmp0;
    u8                           tmp1;
    u8                           tmp2;
    u8                           end0;
    u8                           start1;
    u8                           end1;
    /* The first window starts at frame 0: `start0` is a `u8` bound like the
     * other three, assigned first. Its two tests then compare against a
     * register the image zeroes at the first of them; a literal 0 folds. */
    u8 start0;

    work  = (_Actor400500GrayStalkerWork*)arg0->work;
    coord = arg0->extra.tmd->coords;
    if (work->animId != 2) {
        _actor400500SetAnim(arg0, 2, work->animRate);
        _actor400500TickAnim(arg0);
    }
    start0 = 0;
    if (((_Actor400500GrayStalkerWork*)arg0->work)->animRate == 0) {
        tmp0 = 0;
    } else {
        tmp0 = (u32)(0xB00 / ((_Actor400500GrayStalkerWork*)arg0->work)->animRate) >> 4;
    }
    end0 = tmp0;
    if (((_Actor400500GrayStalkerWork*)arg0->work)->animRate == 0) {
        tmp1 = 0;
    } else {
        tmp1 = (u32)(0xC00 / ((_Actor400500GrayStalkerWork*)arg0->work)->animRate) >> 4;
    }
    start1 = tmp1;
    if (((_Actor400500GrayStalkerWork*)arg0->work)->animRate == 0) {
        tmp2 = 0;
    } else {
        tmp2 = (u32)(0x1500 / ((_Actor400500GrayStalkerWork*)arg0->work)->animRate) >> 4;
    }
    end1 = tmp2;
    if (_actor400500HasAnimBoundary(arg0)) {
        work->animFrames = 0;
    }
    if (work->animFrames == start0) {
        _actor400500ReadPartWorldXZ(arg0, 0xB, &work->anchorPos);
        _actor400500PlaySound(arg0, 0x40050001);
    }
    if (work->animFrames == start1) {
        _actor400500ReadPartWorldXZ(arg0, 8, &work->anchorPos);
        _actor400500PlaySound(arg0, 0x40050002);
    }
    if (work->animFrames >= start0 && work->animFrames <= end0) {
        _actor400500PinPartWorldXZ(arg0, 0xB, &work->anchorPos);
    }
    if (work->animFrames >= start1 && work->animFrames <= end1) {
        _actor400500PinPartWorldXZ(arg0, 8, &work->anchorPos);
    }
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
}

/// Plays animation 4 at rate 0x10, moving the root so that node 8 holds its
/// world-space position from frame 0 to 0xD00 / rate / 16 and node 0xB from
/// 0xE00 / rate / 16 to 0x1B00 / rate / 16, each with a sound on its first
/// frame. The cycle restarts at frame 0 on a slot-1 animation boundary, jump or hold.
static void func_actor_400500_8013403C(Task* arg0)
{
    _Actor400500GrayStalkerWork* work;
    GfxCoord*                    coord;
    u8                           tmp0;
    u8                           tmp1;
    u8                           tmp2;
    u8                           end0;
    u8                           start1;
    u8                           end1;
    /* The first window starts at frame 0: `start0` is a `u8` bound like the
     * other three, assigned first. Its two tests then compare against a
     * register the image zeroes at the first of them; a literal 0 folds. */
    u8  start0;
    s32 sound;

    work  = (_Actor400500GrayStalkerWork*)arg0->work;
    coord = arg0->extra.tmd->coords;
    if (work->animId != 4) {
        _actor400500SetAnim(arg0, 4, ANIMATION_RATE_ONE);
        _actor400500TickAnim(arg0);
    }
    start0 = 0;
    if (((_Actor400500GrayStalkerWork*)arg0->work)->animRate == 0) {
        tmp0 = 0;
    } else {
        tmp0 = (u32)(0xD00 / ((_Actor400500GrayStalkerWork*)arg0->work)->animRate) >> 4;
    }
    end0 = tmp0;
    if (((_Actor400500GrayStalkerWork*)arg0->work)->animRate == 0) {
        tmp1 = 0;
    } else {
        tmp1 = (u32)(0xE00 / ((_Actor400500GrayStalkerWork*)arg0->work)->animRate) >> 4;
    }
    start1 = tmp1;
    if (((_Actor400500GrayStalkerWork*)arg0->work)->animRate == 0) {
        tmp2 = 0;
    } else {
        tmp2 = (u32)(0x1B00 / ((_Actor400500GrayStalkerWork*)arg0->work)->animRate) >> 4;
    }
    end1 = tmp2;
    if (_actor400500HasAnimBoundary(arg0)) {
        work->animFrames = 0;
    }
    if (work->animFrames == start0) {
        _actor400500ReadPartWorldXZ(arg0, 8, &work->anchorPos);
        sound = ((((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x40050001;
        _actor400500EnqueueSound(arg0, sound);
    }
    if (work->animFrames == start1) {
        _actor400500ReadPartWorldXZ(arg0, 0xB, &work->anchorPos);
        sound = ((((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x40050002;
        _actor400500EnqueueSound(arg0, sound);
    }
    if (work->animFrames >= start0 && work->animFrames <= end0) {
        _actor400500PinPartWorldXZ(arg0, 8, &work->anchorPos);
    }
    if (work->animFrames >= start1 && work->animFrames <= end1) {
        _actor400500PinPartWorldXZ(arg0, 0xB, &work->anchorPos);
    }
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
}

static void func_actor_400500_8013456C(Task* arg0)
{
    _Actor400500GrayStalkerWork* work;
    Enemy*                       enemy;
    u32                          dmg;
    u32                          amount;
    s16                          amount16;
    s16                          hp;
    u8                           flags;
    s32                          tmp;
    s16                          tick;
    s32                          i;

    work  = (_Actor400500GrayStalkerWork*)arg0->work;
    enemy = (Enemy*)arg0->spawnArg2.pointer;
    for (i = 0; i < 3; i++) {
        if ((work->bodyContacts[i].key.value & 0xFFFF0000) == 0x20000) {
            if (work->hitCooldown == 0) {
                work->hitTaken    = 1;
                dmg               = damageComputePlayerAttack(work->bodyContacts[i].key.value, work->targetDist, 0, 0);
                amount            = dmg;
                work->hitCooldown = damageGetPlayerAttackHitCooldown(work->bodyContacts[i].key.value);
                if (damageRollCriticalHit(enemy, work->bodyContacts[i].key.value, 0) != 0) {
                    amount = ((u32)dmg << 16) >> 14;
                    effectSpawn(EFFECT_CRITICAL_HIT, &arg0->extra.tmd->coords[3], 0, NULL);
                }
                amount16 = amount;
                damageAccumulateLifeDrainHp(enemy, work->bodyContacts[i].key.value, amount16, 0);
                worldTargetAddReadoutAmount(&enemy->node, amount16, 0);
                hp        = (u16)enemy->hp - amount;
                enemy->hp = hp;
                if ((hp << 16) <= 0) {
                    enemy->hp       = 0;
                    work->deathHeld = 1;
                }
                effectSpawnHit(
                    damageGetPlayerAttackEffectId(work->bodyContacts[i].key.value),
                    &arg0->extra.tmd->coords[3],
                    NULL,
                    &work->effectArg);
                if (amount16 >= 0x32) {
                    work->hitReaction     = ACTOR_400500_HIT_REACTION_HEAVY;
                    amount                = dmg;
                    work->lastHitReaction = ACTOR_400500_HIT_REACTION_HEAVY;
                } else {
                    work->hitReaction     = ACTOR_400500_HIT_REACTION_LIGHT;
                    work->lastHitReaction = ACTOR_400500_HIT_REACTION_LIGHT;
                }
            } else if ((damageGetPlayerAttackEffectId(work->bodyContacts[i].key.value)) == 0xD) {
                effectSpawnHit(EFFECT_HIT_KIND_LIFE_DRAIN_MOTES, &arg0->extra.tmd->coords[1], NULL, &work->effectArg);
            }
            switch (damageGetPlayerAttackReaction(work->bodyContacts[i].key.value) & 0xFFFF) {
                case DAMAGE_PLAYER_REACTION_NONE:
                    break;
                case DAMAGE_PLAYER_REACTION_STAGGER:
                    damageStartEnemyStagger(enemy);
                    break;
                case DAMAGE_PLAYER_REACTION_BUILDUP:
                    damageStartEnemyBuildup(enemy, work->bodyContacts[i].key.value, 0);
                    break;
                case DAMAGE_PLAYER_REACTION_POISON:
                    damageTryStartEnemyDamageOverTime(enemy, work->bodyContacts[i].key.value, 0);
                    break;
                case 4:
                    work->hitReaction     = ACTOR_400500_HIT_REACTION_BLAST;
                    work->lastHitReaction = ACTOR_400500_HIT_REACTION_BLAST;
                    break;
                case 5:
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
                case 8:
                case 9:
                    work->knockdownPending = 1;
                    break;
            }
        }
    }

    flags = enemy->reactionFlags;
    if (flags & ENEMY_REACTION_STAGGER) {
        enemy->reactionFlags  = flags & ENEMY_REACTION_STAGGER_CLEAR;
        work->hitReaction     = ACTOR_400500_HIT_REACTION_HEAVY;
        work->lastHitReaction = ACTOR_400500_HIT_REACTION_HEAVY;
    }
    if (enemy->reactionFlags & ENEMY_REACTION_BUILDUP) {
        enemy->reactionFlags &= ENEMY_REACTION_BUILDUP_CLEAR;
        work->hitReaction     = ACTOR_400500_HIT_REACTION_STATUS;
        work->lastHitReaction = ACTOR_400500_HIT_REACTION_STATUS;
    }
    if (enemy->reactionFlags & ENEMY_REACTION_DAMAGE_OVER_TIME_BITS) {
        tmp  = damageTickEnemyDamageOverTime(enemy);
        tick = tmp;
        if (tick != 0) {
            enemy->hp = (u16)enemy->hp - tmp;
            worldTargetAddReadoutAmount(&enemy->node, tick, 0);
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

static void func_actor_400500_801348D8(Task* arg0, s32 arg1)
{
    SVECTOR                      pos;
    GfxCoord*                    coords;
    GfxCoord*                    joint;
    GfxCoord*                    player;
    Task*                        slot;
    _Actor400500GrayStalkerWork* work;
    _Actor400500GrayStalkerWork* work2;
    _Actor400500GrayStalkerWork* work3;
    s32                          i;
    s32                          cur;
    s32                          sample;

    coords = arg0->extra.tmd->coords;
    slot   = *gPlayerActorTasks;
    joint  = coords + 8;
    work   = (_Actor400500GrayStalkerWork*)arg0->work;
    if (slot != NULL) {
        player = slot->extra.tmd->coords;
        work2  = work;
        if (work->animRequest == ACTOR_400500_ANIM_REQUEST_BLEND) {
            if (work->appliedAnim != work->animId) {
                work->animFrames = 0;
            } else {
                work->animFrames = _actor400500RescaleAnimFrames(arg0, work->animFrames);
            }
            _actor400500BlendAnimSlots(arg0);
            work2->animRequest = ACTOR_400500_ANIM_REQUEST_PLAYING;
        } else if (work->animRequest == ACTOR_400500_ANIM_REQUEST_RESET) {
            _actor400500ResetAnimSlots(arg0);
            work->animRequest = ACTOR_400500_ANIM_REQUEST_PLAYING;
            work->animFrames  = 0;
        } else if (work->animRequest == ACTOR_400500_ANIM_REQUEST_PLAYING) {
            work->animFrames = (u16)work->animFrames + 1;
        }
        i = 1;
        do {
            work2->rig.slots[i].rate = work2->animRate;
            animationTickSlot(&work2->rig.anim, i);
            i++;
        } while (i < ARRAY_SIZE(work2->rig.slots));
        gGfxViewCoord.composeStamp = GRAPHICS_COORD_DIRTY;
        actorRenderComposeCoord(&gGfxViewCoord);
        joint->composeStamp = GRAPHICS_COORD_DIRTY;
        actorRenderComposeCoord(joint);
        pos.vx = 0x160;
        pos.vy = 0x148;
        pos.vz = 0x2C0;
        _actorRenderTransformPointToWorld(joint, &pos);
        if ((arg1 << 0x10) == 0) {
            player->coord.t[0] = pos.vx;
            player->coord.t[2] = pos.vz;
        } else {
            sample             = pos.vx;
            cur                = player->coord.t[0];
            cur               += (sample - cur) >> 2;
            player->coord.t[0] = cur;
            sample             = pos.vz;
            cur                = player->coord.t[2];
            cur               += (sample - cur) >> 2;
            player->coord.t[2] = cur;
        }
        player->composeStamp = GRAPHICS_COORD_DIRTY;
        actorRenderComposeCoord(player);
        work->animRate = -ANIMATION_RATE_ONE;
        work3          = (_Actor400500GrayStalkerWork*)arg0->work;
        if (work3->animRequest == ACTOR_400500_ANIM_REQUEST_BLEND) {
            if (work3->appliedAnim != work3->animId) {
                work3->animFrames = 0;
            } else {
                work3->animFrames = _actor400500RescaleAnimFrames(arg0, work3->animFrames);
            }
            _actor400500BlendAnimSlots(arg0);
            work3->animRequest = ACTOR_400500_ANIM_REQUEST_PLAYING;
        } else if (work3->animRequest == ACTOR_400500_ANIM_REQUEST_RESET) {
            _actor400500ResetAnimSlots(arg0);
            work3->animRequest = ACTOR_400500_ANIM_REQUEST_PLAYING;
            work3->animFrames  = 0;
        } else if (work3->animRequest == ACTOR_400500_ANIM_REQUEST_PLAYING) {
            work3->animFrames = (u16)work3->animFrames + 1;
        }
        i = 1;
        do {
            work3->rig.slots[i].rate = work3->animRate;
            animationTickSlot(&work3->rig.anim, i);
            i++;
        } while (i < ARRAY_SIZE(work3->rig.slots));
        work->animRate = ANIMATION_RATE_ONE;
    }
}

static void func_actor_400500_80134B88(Task* arg0)
{
    EffectWork* eff;
    EffectWork* eff2;
    EffectWork* eff3;
    TmdObject*  dst;
    TmdObject*  dst2;
    TmdObject*  dst3;
    TmdObject*  src;
    TmdObject*  src2;
    TmdObject*  src3;

    D_800678F0[0] = &_gActor400500GrayStalkerBurstHead;
    eff           = effectSpawn(EFFECT_BODY_CHUNK, &arg0->extra.tmd->coords[3], 0x200, NULL);
    if (eff != NULL) {
        src                    = arg0->extra.tmd;
        dst                    = eff->task->extra.tmd;
        dst->texturePageOffset = src->texturePageOffset;
        dst->clutRowOffset     = src->clutRowOffset;
        if (dst->buffer != NULL) {
            tmdBuildBufferHalf(dst);
            tmdBuildBufferHalf(dst);
        }
    }
    D_800678F0[0] = &_gActor400500GrayStalkerBurstLegRight;
    eff2          = effectSpawn(EFFECT_BODY_CHUNK, &arg0->extra.tmd->coords[1], 0x200, NULL);
    if (eff2 != NULL) {
        src2                    = arg0->extra.tmd;
        dst2                    = eff2->task->extra.tmd;
        dst2->texturePageOffset = src2->texturePageOffset;
        dst2->clutRowOffset     = src2->clutRowOffset;
        if (dst2->buffer != NULL) {
            tmdBuildBufferHalf(dst2);
            tmdBuildBufferHalf(dst2);
        }
    }
    D_800678F0[0] = &_gActor400500GrayStalkerBurstLegLeft;
    eff3          = effectSpawn(EFFECT_BODY_CHUNK, &arg0->extra.tmd->coords[1], 0x200, NULL);
    if (eff3 != NULL) {
        src3                    = arg0->extra.tmd;
        dst3                    = eff3->task->extra.tmd;
        dst3->texturePageOffset = src3->texturePageOffset;
        dst3->clutRowOffset     = src3->clutRowOffset;
        if (dst3->buffer != NULL) {
            tmdBuildBufferHalf(dst3);
            tmdBuildBufferHalf(dst3);
        }
    }
    effectSpawn(EFFECT_030, &arg0->extra.tmd->coords[1], 0x200, NULL);
    effectSpawn(EFFECT_030, &arg0->extra.tmd->coords[2], 0x200, NULL);
    effectSpawn(EFFECT_030, &arg0->extra.tmd->coords[3], 0x200, NULL);
}

#include "../../shared/frame_capture.inc.c"

static void func_actor_400500_80135414(Task* arg0)
{
    TmdObject*                   extra;
    Enemy*                       enemy;
    GfxCoord*                    coord;
    _Actor400500GrayStalkerWork* work;
    _Actor400500GrayStalkerWork* work4;
    _Actor400500GrayStalkerWork* work5;
    GfxCoord*                    player;
    GfxCoord*                    coord2;
    TmdObject*                   extra2;
    _Actor400500GrayStalkerWork* work7;
    s32                          flag;
    u8                           mode;

    extra      = arg0->extra.tmd;
    enemy      = arg0->spawnArg2.pointer;
    coord      = extra->coords;
    arg0->work = memCalloc(sizeof(_Actor400500GrayStalkerWork), 0);
    work       = (_Actor400500GrayStalkerWork*)arg0->work;
    if (work == NULL) {
        enemyDestroy(enemy, arg0);
        return;
    }
    extra->lightMtx   = &work->lightMtx;
    extra->colorMtx   = &work->colorMtx;
    extra->flags      = 0;
    enemy->field_4    = &coord->coord;
    enemy->field_48   = 0;
    enemy->bodyPos.vx = 0;
    enemy->bodyPos.vy = 0;
    enemy->bodyPos.vz = 0;
    enemy->coord      = &arg0->extra.tmd->coords[3];
    worldTargetLinkNode(&enemy->node);
    enemy->node.state.parts.flags = WORLD_TARGET_NOT_LOCKABLE;
    enemy->recs                   = work->bodyContacts;
    enemy->param                  = &D_actor_400500_80153C90;
    enemy->hp = enemy->hpMax = D_actor_400500_80153C90.hpMax;
    animationInitContext(&work->rig.anim, (AnimationSet**)D_actor_400500_80153CC0, extra, work->rig.poses,
                         work->rig.slots);
    coord->parent = &gGfxViewCoord;
    _actor400500SetAnim(arg0, 2, 0x18);
    _actor400500TickAnim(arg0);
    arg0->msgTable = D_actor_400500_80153CA0;
    func_actor_400500_80132C54(arg0);
    work4 = (_Actor400500GrayStalkerWork*)arg0->work;
    if (gPlayerActorTasks[PLAYER_ACTOR_TASK_PLAYER] != NULL) {
        player                  = gPlayerActorTasks[PLAYER_ACTOR_TASK_PLAYER]->extra.tmd->coords;
        work4->playerPrevPos.vx = player->coord.t[0];
        work4->playerPrevPos.vy = player->coord.t[1];
        work4->playerPrevPos.vz = player->coord.t[2];
    }
    work5  = (_Actor400500GrayStalkerWork*)arg0->work;
    mode   = gGameSession->location.loc.room;
    extra2 = arg0->extra.tmd;
    if ((mode == 1) || (mode == 3) || (mode == 5) || (mode == 6)) {
        work5->cloakLevel     = 0xFF;
        work5->colorBlend     = 0;
        work5->shadowShade    = 0;
        work5->hideHoldFrames = 0x10;
    } else {
        work5->colorBlend     = TMD_OBJECT_COLOR_BLEND_ONE;
        work5->shadowShade    = 0xFF;
        work5->cloakLevel     = 0;
        work5->hideHoldFrames = 0x2000;
    }
    modelLightingSetLayerMaterials(work5->cloakLevel);
    extra2->shading.colorBlend = work5->colorBlend;
    func_actor_400500_80132000(arg0);
    func_actor_400500_8013226C(arg0);
    sceneAcquireBattleRef(0);
    coord2                     = arg0->extra.tmd->coords;
    work->effectArg.spawnArgLo = 0x100;
    work->effectArg.spawnArgHi = 3;
    work->effectArg.coord      = &coord2[3];
    _actor400500SetState(arg0, ACTOR_400500_STATE_AMBUSH, 0);
    gStageSceneMusicEntry = 2;
    work7                 = (_Actor400500GrayStalkerWork*)arg0->work;
    if (((work7->cloakRequest >= 0) || ((u8)work7->cloakRequest & ACTOR_400500_CLOAK_KIND_MASK)) && (work7->hideCooldown == 0)) {
        flag                = ACTOR_400500_CLOAK_RUNNING | ACTOR_400500_CLOAK_HIDE;
        work7->cloakRequest = flag;
        work7->cloakPhase   = 0;
    }
    arg0->state = arg0->state + 1;
}

/// Handlers the task entry `func_actor_400500_8013DE98` runs by task state:
/// set-up, the per-frame state machine run by `state`, a second one run by
/// `state` from task state 2, and the teardown that kills the child tasks
/// and destroys the enemy 0x12D frames later.
static const TaskFuncTable4 D_actor_400500_80131E4C = { {
    func_actor_400500_80135414,
    func_actor_400500_80135770,
    func_actor_400500_8013A700,
    func_actor_400500_8013DEFC,
} };

static __inline__ s32 lookup_zone(Task* task)
{
    ActorZone* zone;
    u16        id_u;
    s16        zone_id;
    GfxCoord*  root;
    u16        px_u, pz_u;
    s16        px, pz;

    zone    = D_actor_400500_80153D6C;
    id_u    = (u16)zone->id;
    root    = task->extra.tmd->coords;
    zone_id = zone->id;
    px_u    = (u16)root->coord.t[0];
    pz_u    = (u16)root->coord.t[2];
    if (zone_id != ACTOR_ZONE_END) {
        px = (s16)px_u;
        pz = (s16)pz_u;
        do {
            if ((px >= zone->x) && ((zone->x + zone->width) >= px) &&
                (pz >= zone->z) && ((zone->z + zone->depth) >= pz)) {
                return (s16)id_u;
            }
            zone++;
            id_u = (u16)zone->id;
        } while (zone->id != ACTOR_ZONE_END);
    }
    return 0;
}

static __inline__ VECTOR* push_color(GfxCoord* coord)
{
    VECTOR* block = (VECTOR*)(SCRATCH_STACK_CURSOR(u8) - 0x10);

    ((VECTOR*)(SCRATCH_STACK_CURSOR(u8) - 0x10))->vx = coord->workm.t[0];
    block->vy                                        = coord->workm.t[1];
    block->vz                                        = coord->workm.t[2];
    SCRATCH_STACK_CURSOR(VECTOR)                     = block;
    return block;
}

static __inline__ void pop_scratch(s32 n)
{
    SCRATCH_STACK_RELEASE_BYTES(n);
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
    func_actor_400500_80135EBC,
    func_actor_400500_8013BA24,
    func_actor_400500_801385D0,
    func_actor_400500_8013899C,
    func_actor_400500_80138EA0,
    func_actor_400500_8013905C,
    func_actor_400500_801392D8,
    func_actor_400500_801395D0,
    func_actor_400500_80139C1C,
    func_actor_400500_80139F6C,
    func_actor_400500_8013AD60,
    func_actor_400500_8013B5E0,
    func_actor_400500_8013A484,
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
    GfxMatrix                    rot;
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
            func_actor_400500_80132438(arg0);
            work->zone = lookup_zone(arg0);
            if (slot == NULL) {
                work->playerZone = 0;
            } else {
                work->playerZone = lookup_zone(slot);
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
            gfxSetRotIdentity(&rot.mat);
            RotMatrixZ(work_rot->roll, &rot.mat);
            RotMatrixX(work_rot->pitch, &rot.mat);
            RotMatrixY(work_rot->yaw, &rot.mat);
            _actor400500CopyRotation(&rot.mat, &rot_root->coord);
            func_actor_400500_80132E94(arg0);
            if (work->attackCooldown > 0) {
                work->attackCooldown = (u16)work->attackCooldown - 1;
                work->attackCooling  = 1;
            } else {
                work->attackCooling = 0;
            }
            obj->flags &= (u16)~TMD_OBJECT_SKIP_ACTIVE_DRAW;
            if ((enemy->hp > 0) || (cfg->hp <= 0)) {
                func_actor_400500_8013456C(arg0);
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
            color      = push_color(color_part);
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
            pop_scratch(0x10);
            if (gGameSession->sceneUpdatesPaused != 0) {
                func_actor_400500_80132AB0(arg0, -0xFA0, (u8)work->shadowShade);
                return;
            }
            func_actor_400500_80132AB0(arg0, -0xFA0, ((u16)work->shadowShade >> 2) & 0xFF);
            func_actor_400500_80132AB0(arg0, -0x3E8, (u8)work->shadowShade);
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
            frameCaptureQueue(proj->otz);
            pop_scratch(sizeof(ActorOriginDepthScratch));
            return;
    }
}

/// Sub-state handlers `func_actor_400500_80135EBC` copies onto the stack and runs
/// by `subState` every frame.
static const TaskFuncTable11 D_actor_400500_80131E90 = { {
    func_actor_400500_801361EC,
    func_actor_400500_8013662C,
    func_actor_400500_80136864,
    func_actor_400500_801369A4,
    func_actor_400500_80136B94,
    func_actor_400500_80136D00,
    func_actor_400500_80136EB8,
    func_actor_400500_80137034,
    func_actor_400500_801371A0,
    func_actor_400500_80137338,
    func_actor_400500_80137478,
} };

/// Handlers `func_actor_400500_80135EBC` also runs, by `deathStep`, while the
/// enemy is out of hit points.
static const TaskFuncTable3 D_actor_400500_80131EBC = { {
    func_actor_400500_8013BAA4,
    func_actor_400500_8013BB18,
    func_actor_400500_8013BBB0,
} };

static void func_actor_400500_80135EBC(Task* arg0)
{
    _Actor400500GrayStalkerWork* work;
    Enemy*                       enemy;
    TaskFuncTable11              sp10;
    TaskFuncTable3               sp40;
    _Actor400500GrayStalkerWork* work2;
    s32                          i;
    _Actor400500GrayStalkerWork* work3;
    s32                          flag;
    _Actor400500GrayStalkerWork* work4;

    work  = (_Actor400500GrayStalkerWork*)arg0->work;
    enemy = (Enemy*)arg0->spawnArg2.pointer;
    sp10  = D_actor_400500_80131E90;
    sp40  = D_actor_400500_80131EBC;
    if (enemy->hp <= 0) {
        sp40.funcs[(s16)work->deathStep](arg0);
        work2 = (_Actor400500GrayStalkerWork*)arg0->work;
        if (work2->animRequest == ACTOR_400500_ANIM_REQUEST_BLEND) {
            if (work2->appliedAnim != work2->animId) {
                work2->animFrames = 0;
            } else {
                work2->animFrames = _actor400500RescaleAnimFrames(arg0, work2->animFrames);
            }
            _actor400500BlendAnimSlots(arg0);
            work2->animRequest = ACTOR_400500_ANIM_REQUEST_PLAYING;
        } else if (work2->animRequest == ACTOR_400500_ANIM_REQUEST_RESET) {
            _actor400500ResetAnimSlots(arg0);
            work2->animRequest = ACTOR_400500_ANIM_REQUEST_PLAYING;
            work2->animFrames  = 0;
        } else if (work2->animRequest == ACTOR_400500_ANIM_REQUEST_PLAYING) {
            work2->animFrames = (u16)work2->animFrames + 1;
        }
        i = 1;
        do {
            work2->rig.slots[i].rate = work2->animRate;
            animationTickSlot(&work2->rig.anim, i);
            i++;
        } while (i < ARRAY_SIZE(work2->rig.slots));
        work3 = (_Actor400500GrayStalkerWork*)arg0->work;
        if ((work3->cloakRequest >= 0) || (((u8)work3->cloakRequest & ACTOR_400500_CLOAK_KIND_MASK) != ACTOR_400500_CLOAK_SHOW)) {
            flag                = ACTOR_400500_CLOAK_RUNNING | ACTOR_400500_CLOAK_SHOW;
            work3->cloakRequest = flag;
            work3->cloakPhase   = 0;
        }
        work->rightArmOuter.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->rightArmInner.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->leftArmOuter.flags  &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->leftArmInner.flags  &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        return;
    }
    if ((s16)work->subState != 0) {
        work4 = (_Actor400500GrayStalkerWork*)arg0->work;
        if (work4->animRequest == ACTOR_400500_ANIM_REQUEST_BLEND) {
            if (work4->appliedAnim != work4->animId) {
                work4->animFrames = 0;
            } else {
                work4->animFrames = _actor400500RescaleAnimFrames(arg0, work4->animFrames);
            }
            _actor400500BlendAnimSlots(arg0);
            work4->animRequest = ACTOR_400500_ANIM_REQUEST_PLAYING;
        } else if (work4->animRequest == ACTOR_400500_ANIM_REQUEST_RESET) {
            _actor400500ResetAnimSlots(arg0);
            work4->animRequest = ACTOR_400500_ANIM_REQUEST_PLAYING;
            work4->animFrames  = 0;
        } else if (work4->animRequest == ACTOR_400500_ANIM_REQUEST_PLAYING) {
            work4->animFrames = (u16)work4->animFrames + 1;
        }
        i = 1;
        do {
            work4->rig.slots[i].rate = work4->animRate;
            animationTickSlot(&work4->rig.anim, i);
            i++;
        } while (i < ARRAY_SIZE(work4->rig.slots));
    }
    sp10.funcs[(s16)work->subState](arg0);
    work3 = (_Actor400500GrayStalkerWork*)arg0->work;
    if (((work3->cloakRequest >= 0) || ((u8)work3->cloakRequest & ACTOR_400500_CLOAK_KIND_MASK)) && (work3->hideCooldown == 0)) {
        flag                = ACTOR_400500_CLOAK_RUNNING | ACTOR_400500_CLOAK_HIDE;
        work3->cloakRequest = flag;
        work3->cloakPhase   = 0;
    }
}

static void func_actor_400500_801361EC(Task* arg0)
{
    GfxMatrix                    rot;
    GfxMatrix*                   src;
    MATRIX*                      dst;
    _Actor400500GrayStalkerWork* work;
    _Actor400500GrayStalkerWork* workA;
    _Actor400500GrayStalkerWork* work3;
    GfxCoord*                    coord;
    s32                          flag2;
    s32                          heading;
    s32                          tx;
    s32                          playerZone;

    work    = (_Actor400500GrayStalkerWork*)arg0->work;
    heading = (u16)work->yaw & 0xFFF;
    coord   = arg0->extra.tmd->coords;
    if (!_actor400500TakeKnockdown(arg0)) {
        workA = (_Actor400500GrayStalkerWork*)arg0->work;
        if (workA->ceilingFallPending != 0) {
            workA->ceilingFallPending = 0;
            if (workA->posture & ACTOR_400500_POSTURE_ON_FLOOR) {
                flag2 = 0;
            } else {
                _actor400500EnterState(arg0, ACTOR_400500_STATE_FALL);
                flag2 = 1;
            }
        } else {
            flag2 = 0;
        }
        if ((flag2 == 0) && ((_actor400500TryStartAttack(arg0) << 0x10) == 0)) {
            work3              = (_Actor400500GrayStalkerWork*)arg0->work;
            work3->turnRequest = ACTOR_400500_TURN_NONE;
            work3->turnStarted = 0;
            _actor400500SetAnim(arg0, 2, 0x18);
            _actor400500TickAnim(arg0);
            switch ((s16)((u16)work->zone - 1)) {
                case 3:
                    if (heading == 0x400) {
                        work->subState = 1;
                    } else {
                        work->subState = 4;
                    }
                    break;
                case 0:
                    if (work->toTarget.vx < 0) {
                        work->subState = 3;
                    } else {
                        work->subState = 1;
                    }
                    break;
                case 1:
                    playerZone = (s16)work->playerZone;
                    if ((playerZone == 1) || (playerZone == 4) || (playerZone == 5)) {
                        work->subState = 3;
                    } else if ((playerZone == 3) && (heading == 0) && (coord->coord.t[0] >= 0x4074)) {
                        work->subState = 6;
                    } else if ((s16)work->playerZone == 2) {
                        if (coord->coord.t[2] >= -0x209D) {
                            work->subState = 6;
                        } else {
                            work->subState = 1;
                        }
                    } else {
                        work->subState = 1;
                    }
                    break;
                case 2:
                    if (work->toTarget.vz < 0) {
                        work->subState = 8;
                    } else {
                        work->subState = 6;
                    }
                    break;
                case 5:
                    if (heading == 0x800) {
                        work->subState = 8;
                    } else {
                        work->subState = 7;
                    }
                    break;
                default:
                    tx                = -0x3E8;
                    coord->coord.t[0] = tx;
                    tx                = -0xFA0;
                    coord->coord.t[1] = tx;
                    tx                = -0x2116;
                    coord->coord.t[2] = tx;
                    tx                = 0x400;
                    work->yaw         = tx;
                    tx                = 0x800;
                    work->roll        = tx;
                    src               = &rot;
                    work->pitch       = 0;
                    work->posture     = 0;
                    gfxSetRotIdentity(&src->mat);
                    RotMatrixZ(work->roll, &src->mat);
                    RotMatrixY(work->yaw, &src->mat);
                    dst          = &coord->coord;
                    dst->m[0][0] = src->mat.m[0][0];
                    dst->m[0][1] = src->mat.m[0][1];
                    dst->m[0][2] = src->mat.m[0][2];
                    dst->m[1][0] = src->mat.m[1][0];
                    dst->m[1][1] = src->mat.m[1][1];
                    dst->m[1][2] = src->mat.m[1][2];
                    dst->m[2][0] = src->mat.m[2][0];
                    dst->m[2][1] = src->mat.m[2][1];
                    dst->m[2][2] = src->mat.m[2][2];
                    _actor400500ReadPartWorldXZ(arg0, 11, &work->anchorPos);
                    work->subState = 1;
                    break;
            }
            _actor400500ReadPartWorldXZ(arg0, 11, &work->anchorPos);
        }
    }
}

static void func_actor_400500_8013662C(Task* arg0)
{
    _Actor400500GrayStalkerWork* work;
    _Actor400500GrayStalkerWork* work2;
    GfxCoord*                    coord;
    s32                          flag;
    s32                          flag2;
    s32                          heading;
    s32                          zone;
    s32                          val;
    u32                          rnd;
    u16                          playerZone;

    work  = (_Actor400500GrayStalkerWork*)arg0->work;
    coord = arg0->extra.tmd->coords;
    if (work->knockdownPending != 0) {
        work->knockdownPending = 0;
        _actor400500EnterState(arg0, ACTOR_400500_STATE_KNOCKDOWN);
        flag = 1;
    } else {
        flag = 0;
    }
    if (flag == 0) {
        work2 = (_Actor400500GrayStalkerWork*)arg0->work;
        if (work2->ceilingFallPending != 0) {
            work2->ceilingFallPending = 0;
            if (work2->posture & ACTOR_400500_POSTURE_ON_FLOOR) {
                flag2 = 0;
            } else {
                _actor400500EnterState(arg0, ACTOR_400500_STATE_FALL);
                flag2 = 1;
            }
        } else {
            flag2 = 0;
        }
        if ((flag2 == 0) && ((_actor400500TryStartAttack(arg0) << 0x10) == 0) &&
            ((_actor400500HandleHeavyHitReaction(arg0) << 0x10) == 0) &&
            ((func_actor_400500_80133160(arg0) << 0x10) == 0)) {
            heading = (u16)work->yaw;
            if ((heading & 0xFFF) != 0x400) {
                if (((0x400 - heading) << 0x14) > 0) {
                    work2              = (_Actor400500GrayStalkerWork*)arg0->work;
                    work2->turnRequest = ACTOR_400500_TURN_YAW_UP;
                } else {
                    work2              = (_Actor400500GrayStalkerWork*)arg0->work;
                    work2->turnRequest = ACTOR_400500_TURN_YAW_DOWN;
                }
                work2->turnStarted = 0;
            } else {
                zone = work->zone;
                if (zone != 1) {
                    if ((zone == 2) && (coord->coord.t[0] >= 0x4075)) {
                        playerZone = work->playerZone;
                        if (((u32)(playerZone - 2) < 2U) || ((s16)playerZone == 6)) {
                            work->subState = 5;
                        } else {
                            work->subState = zone;
                        }
                    } else {
                        func_actor_400500_801335E8(arg0);
                    }
                } else {
                    if (work->attackCooling == 0) {
                        if (work->toTarget.vx <= 0) {
                            work->subState = 2;
                        }
                    } else if (work->toTarget.vx < -0xF9F) {
                        rnd             = (gRandomLcgState * RANDOM_LCG_MULTIPLIER) + RANDOM_LCG_INCREMENT;
                        gRandomLcgState = rnd;
                        if (((rnd >> 0x10) & 0x1F) == 0) {
                            if (!(work->posture & ACTOR_400500_POSTURE_ON_FLOOR)) {
                                work2 = (_Actor400500GrayStalkerWork*)arg0->work;
                                val   = ACTOR_400500_STATE_DROP;
                            } else {
                                work2 = (_Actor400500GrayStalkerWork*)arg0->work;
                                val   = ACTOR_400500_STATE_JUMP;
                            }
                            work2->state    = val;
                            work2->subState = 0;
                        }
                    }
                    func_actor_400500_801335E8(arg0);
                }
            }
            coord->coord.t[2] = -0x209E;
        }
    }
}

static void func_actor_400500_80136864(Task* arg0)
{
    _Actor400500GrayStalkerWork* work;
    _Actor400500GrayStalkerWork* work2;
    _Actor400500GrayStalkerWork* work3;
    _Actor400500GrayStalkerWork* work4;
    s32                          flag;
    s32                          flag2;
    s32                          heading;

    work = (_Actor400500GrayStalkerWork*)arg0->work;
    if (work->knockdownPending != 0) {
        work->knockdownPending = 0;
        _actor400500EnterState(arg0, ACTOR_400500_STATE_KNOCKDOWN);
        flag = 1;
    } else {
        flag = 0;
    }
    if (flag == 0) {
        work2 = (_Actor400500GrayStalkerWork*)arg0->work;
        if (work2->ceilingFallPending != 0) {
            work2->ceilingFallPending = 0;
            if (work2->posture & ACTOR_400500_POSTURE_ON_FLOOR) {
                flag2 = 0;
            } else {
                _actor400500EnterState(arg0, ACTOR_400500_STATE_FALL);
                flag2 = 1;
            }
        } else {
            flag2 = 0;
        }
        if ((flag2 == 0) && ((_actor400500TryStartAttack(arg0) << 0x10) == 0) &&
            ((_actor400500HandleHeavyHitReaction(arg0) << 0x10) == 0) &&
            ((func_actor_400500_80133160(arg0) << 0x10) == 0)) {
            heading = (u16)work->yaw;
            if ((heading & 0xFFF) == 0xC00) {
                work2              = (_Actor400500GrayStalkerWork*)arg0->work;
                work2->animRate    = 0x18;
                work2->animId      = 2;
                work2->animRequest = ACTOR_400500_ANIM_REQUEST_RESET;
                work->subState     = 3;
                return;
            }
            if (((0xC00 - heading) << 0x14) > 0) {
                work3              = (_Actor400500GrayStalkerWork*)arg0->work;
                work3->turnRequest = ACTOR_400500_TURN_YAW_UP;
                work3->turnStarted = 0;
            } else {
                work4              = (_Actor400500GrayStalkerWork*)arg0->work;
                work4->turnRequest = ACTOR_400500_TURN_YAW_DOWN;
                work4->turnStarted = 0;
            }
        }
    }
}

static void func_actor_400500_801369A4(Task* arg0)
{
    _Actor400500GrayStalkerWork* work;
    _Actor400500GrayStalkerWork* work2;
    GfxCoord*                    coord;
    s32                          flag;
    s32                          flag2;
    s32                          heading;
    s32                          zone;
    s32                          val;
    u32                          rnd;

    work  = (_Actor400500GrayStalkerWork*)arg0->work;
    coord = arg0->extra.tmd->coords;
    if (work->knockdownPending != 0) {
        work->knockdownPending = 0;
        _actor400500EnterState(arg0, ACTOR_400500_STATE_KNOCKDOWN);
        flag = 1;
    } else {
        flag = 0;
    }
    if (flag == 0) {
        work2 = (_Actor400500GrayStalkerWork*)arg0->work;
        if (work2->ceilingFallPending != 0) {
            work2->ceilingFallPending = 0;
            if (work2->posture & ACTOR_400500_POSTURE_ON_FLOOR) {
                flag2 = 0;
            } else {
                _actor400500EnterState(arg0, ACTOR_400500_STATE_FALL);
                flag2 = 1;
            }
        } else {
            flag2 = 0;
        }
        if ((flag2 == 0) && ((_actor400500TryStartAttack(arg0) << 0x10) == 0) &&
            ((_actor400500HandleHeavyHitReaction(arg0) << 0x10) == 0) &&
            ((func_actor_400500_80133160(arg0) << 0x10) == 0)) {
            heading = (u16)work->yaw;
            if ((heading & 0xFFF) != 0xC00) {
                if (((0xC00 - heading) << 0x14) > 0) {
                    work2              = (_Actor400500GrayStalkerWork*)arg0->work;
                    work2->turnRequest = ACTOR_400500_TURN_YAW_UP;
                } else {
                    work2              = (_Actor400500GrayStalkerWork*)arg0->work;
                    work2->turnRequest = ACTOR_400500_TURN_YAW_DOWN;
                }
                work2->turnStarted = 0;
            } else {
                zone = work->zone;
                if (zone != 1) {
                    if (zone == 4) {
                        work->subState = zone;
                    }
                } else if (work->attackCooling == 0) {
                    if (work->toTarget.vx >= 0) {
                        work->subState = 4;
                    }
                } else if (work->toTarget.vx >= 0xFA0) {
                    rnd             = (gRandomLcgState * RANDOM_LCG_MULTIPLIER) + RANDOM_LCG_INCREMENT;
                    gRandomLcgState = rnd;
                    if (((rnd >> 0x10) & 0x1F) == 0) {
                        if (!(work->posture & ACTOR_400500_POSTURE_ON_FLOOR)) {
                            work2 = (_Actor400500GrayStalkerWork*)arg0->work;
                            val   = ACTOR_400500_STATE_DROP;
                        } else {
                            work2 = (_Actor400500GrayStalkerWork*)arg0->work;
                            val   = ACTOR_400500_STATE_JUMP;
                        }
                        work2->state    = val;
                        work2->subState = 0;
                    }
                }
                func_actor_400500_801335E8(arg0);
            }
            coord->coord.t[2] = -0x209E;
        }
    }
}

static void func_actor_400500_80136B94(Task* arg0)
{
    _Actor400500GrayStalkerWork* work;
    _Actor400500GrayStalkerWork* work2;
    s32                          flag;
    s32                          flag2;
    s32                          heading;

    work = (_Actor400500GrayStalkerWork*)arg0->work;
    if (work->knockdownPending != 0) {
        work->knockdownPending = 0;
        _actor400500EnterState(arg0, ACTOR_400500_STATE_KNOCKDOWN);
        flag = 1;
    } else {
        flag = 0;
    }
    if (flag == 0) {
        work2 = (_Actor400500GrayStalkerWork*)arg0->work;
        if (work2->ceilingFallPending != 0) {
            work2->ceilingFallPending = 0;
            if (work2->posture & ACTOR_400500_POSTURE_ON_FLOOR) {
                flag2 = 0;
            } else {
                _actor400500EnterState(arg0, ACTOR_400500_STATE_FALL);
                flag2 = 1;
            }
        } else {
            flag2 = 0;
        }
        if ((flag2 == 0) && ((_actor400500TryStartAttack(arg0) << 0x10) == 0) &&
            ((_actor400500HandleHeavyHitReaction(arg0) << 0x10) == 0) &&
            ((func_actor_400500_80133160(arg0) << 0x10) == 0)) {
            heading = (u16)work->yaw;
            if ((heading & 0xFFF) == 0x400) {
                work2              = (_Actor400500GrayStalkerWork*)arg0->work;
                work2->animRate    = 0x18;
                work2->animId      = 2;
                work2->animRequest = ACTOR_400500_ANIM_REQUEST_RESET;
                work->subState     = 1;
                return;
            }
            if ((s16)work->playerZone == 4) {
                if (work->toTarget.vz > 0) {
                    work2              = (_Actor400500GrayStalkerWork*)arg0->work;
                    work2->turnRequest = ACTOR_400500_TURN_YAW_UP;
                } else {
                    work2              = (_Actor400500GrayStalkerWork*)arg0->work;
                    work2->turnRequest = ACTOR_400500_TURN_YAW_DOWN;
                }
            } else if (((0x400 - heading) << 0x14) > 0) {
                work2              = (_Actor400500GrayStalkerWork*)arg0->work;
                work2->turnRequest = ACTOR_400500_TURN_YAW_UP;
            } else {
                work2              = (_Actor400500GrayStalkerWork*)arg0->work;
                work2->turnRequest = ACTOR_400500_TURN_YAW_DOWN;
            }
            work2->turnStarted = 0;
        }
    }
}

static void func_actor_400500_80136D00(Task* arg0)
{
    _Actor400500GrayStalkerWork* work;
    _Actor400500GrayStalkerWork* work2;
    s32                          flag;
    s32                          flag2;
    s32                          heading;

    work = (_Actor400500GrayStalkerWork*)arg0->work;
    if (work->knockdownPending != 0) {
        work->knockdownPending = 0;
        _actor400500EnterState(arg0, ACTOR_400500_STATE_KNOCKDOWN);
        flag = 1;
    } else {
        flag = 0;
    }
    if (flag == 0) {
        work2 = (_Actor400500GrayStalkerWork*)arg0->work;
        if (work2->ceilingFallPending != 0) {
            work2->ceilingFallPending = 0;
            if (work2->posture & ACTOR_400500_POSTURE_ON_FLOOR) {
                flag2 = 0;
            } else {
                _actor400500EnterState(arg0, ACTOR_400500_STATE_FALL);
                flag2 = 1;
            }
        } else {
            flag2 = 0;
        }
        if ((flag2 == 0) && ((_actor400500TryStartAttack(arg0) << 0x10) == 0) &&
            ((_actor400500HandleHeavyHitReaction(arg0) << 0x10) == 0) &&
            ((func_actor_400500_80133160(arg0) << 0x10) == 0)) {
            if (((u32)(work->playerZone - 2) < 2U) || ((s16)work->playerZone == 6)) {
                heading = (u16)work->yaw;
                if ((heading & 0xFFF) == 0) {
                    work2              = (_Actor400500GrayStalkerWork*)arg0->work;
                    work2->animRate    = 0x18;
                    work2->animId      = 2;
                    work2->animRequest = ACTOR_400500_ANIM_REQUEST_RESET;
                    work->subState     = 6;
                    return;
                }
                if (((0 - heading) << 0x14) > 0) {
                    work2              = (_Actor400500GrayStalkerWork*)arg0->work;
                    work2->turnRequest = ACTOR_400500_TURN_YAW_UP;
                } else {
                    work2              = (_Actor400500GrayStalkerWork*)arg0->work;
                    work2->turnRequest = ACTOR_400500_TURN_YAW_DOWN;
                }
            } else {
                heading = (u16)work->yaw;
                if ((heading & 0xFFF) == 0xC00) {
                    work2              = (_Actor400500GrayStalkerWork*)arg0->work;
                    work2->animRate    = 0x18;
                    work2->animId      = 2;
                    work2->animRequest = ACTOR_400500_ANIM_REQUEST_RESET;
                    work->subState     = 3;
                    return;
                }
                if (((0xC00 - heading) << 0x14) > 0) {
                    work2              = (_Actor400500GrayStalkerWork*)arg0->work;
                    work2->turnRequest = ACTOR_400500_TURN_YAW_UP;
                } else {
                    work2              = (_Actor400500GrayStalkerWork*)arg0->work;
                    work2->turnRequest = ACTOR_400500_TURN_YAW_DOWN;
                }
            }
            work2->turnStarted = 0;
        }
    }
}

static void func_actor_400500_80136EB8(Task* arg0)
{
    _Actor400500GrayStalkerWork* work;
    _Actor400500GrayStalkerWork* work2;
    GfxCoord*                    coord;
    s32                          flag;
    s32                          flag2;
    s32                          heading;

    work  = (_Actor400500GrayStalkerWork*)arg0->work;
    coord = arg0->extra.tmd->coords;
    if (work->knockdownPending != 0) {
        work->knockdownPending = 0;
        _actor400500EnterState(arg0, ACTOR_400500_STATE_KNOCKDOWN);
        flag = 1;
    } else {
        flag = 0;
    }
    if (flag == 0) {
        work2 = (_Actor400500GrayStalkerWork*)arg0->work;
        if (work2->ceilingFallPending != 0) {
            work2->ceilingFallPending = 0;
            if (work2->posture & ACTOR_400500_POSTURE_ON_FLOOR) {
                flag2 = 0;
            } else {
                _actor400500EnterState(arg0, ACTOR_400500_STATE_FALL);
                flag2 = 1;
            }
        } else {
            flag2 = 0;
        }
        if ((flag2 == 0) && ((_actor400500TryStartAttack(arg0) << 0x10) == 0) &&
            ((_actor400500HandleHeavyHitReaction(arg0) << 0x10) == 0) &&
            ((func_actor_400500_80133160(arg0) << 0x10) == 0)) {
            heading = (u16)work->yaw;
            if (heading & 0xFFF) {
                if (((0 - heading) << 0x14) > 0) {
                    work2              = (_Actor400500GrayStalkerWork*)arg0->work;
                    work2->turnRequest = ACTOR_400500_TURN_YAW_UP;
                } else {
                    work2              = (_Actor400500GrayStalkerWork*)arg0->work;
                    work2->turnRequest = ACTOR_400500_TURN_YAW_DOWN;
                }
                work2->turnStarted = 0;
            } else {
                if (work->zone != 3) {
                    if (work->zone == 6) {
                        work->subState = 7;
                    }
                } else if ((work->attackCooling == 0) && (work->toTarget.vz < 0)) {
                    work->subState = 7;
                }
                func_actor_400500_801335E8(arg0);
            }
            coord->coord.t[0] = 0x4074;
        }
    }
}

static void func_actor_400500_80137034(Task* arg0)
{
    _Actor400500GrayStalkerWork* work;
    _Actor400500GrayStalkerWork* work2;
    s32                          flag;
    s32                          flag2;
    s32                          heading;

    work = (_Actor400500GrayStalkerWork*)arg0->work;
    if (work->knockdownPending != 0) {
        work->knockdownPending = 0;
        _actor400500EnterState(arg0, ACTOR_400500_STATE_KNOCKDOWN);
        flag = 1;
    } else {
        flag = 0;
    }
    if (flag == 0) {
        work2 = (_Actor400500GrayStalkerWork*)arg0->work;
        if (work2->ceilingFallPending != 0) {
            work2->ceilingFallPending = 0;
            if (work2->posture & ACTOR_400500_POSTURE_ON_FLOOR) {
                flag2 = 0;
            } else {
                _actor400500EnterState(arg0, ACTOR_400500_STATE_FALL);
                flag2 = 1;
            }
        } else {
            flag2 = 0;
        }
        if ((flag2 == 0) && ((_actor400500TryStartAttack(arg0) << 0x10) == 0) &&
            ((_actor400500HandleHeavyHitReaction(arg0) << 0x10) == 0) &&
            ((func_actor_400500_80133160(arg0) << 0x10) == 0)) {
            heading = (u16)work->yaw;
            if ((heading & 0xFFF) == 0x800) {
                work2              = (_Actor400500GrayStalkerWork*)arg0->work;
                work2->animRate    = 0x18;
                work2->animId      = 2;
                work2->animRequest = ACTOR_400500_ANIM_REQUEST_RESET;
                work->subState     = 8;
                return;
            }
            if ((s16)work->playerZone == 6) {
                if (work->toTarget.vx > 0) {
                    work2              = (_Actor400500GrayStalkerWork*)arg0->work;
                    work2->turnRequest = ACTOR_400500_TURN_YAW_UP;
                } else {
                    work2              = (_Actor400500GrayStalkerWork*)arg0->work;
                    work2->turnRequest = ACTOR_400500_TURN_YAW_DOWN;
                }
            } else if (((0x800 - heading) << 0x14) > 0) {
                work2              = (_Actor400500GrayStalkerWork*)arg0->work;
                work2->turnRequest = ACTOR_400500_TURN_YAW_UP;
            } else {
                work2              = (_Actor400500GrayStalkerWork*)arg0->work;
                work2->turnRequest = ACTOR_400500_TURN_YAW_DOWN;
            }
            work2->turnStarted = 0;
        }
    }
}

static void func_actor_400500_801371A0(Task* arg0)
{
    _Actor400500GrayStalkerWork* work;
    _Actor400500GrayStalkerWork* work2;
    GfxCoord*                    coord;
    s32                          flag;
    s32                          flag2;
    s32                          heading;

    work  = (_Actor400500GrayStalkerWork*)arg0->work;
    coord = arg0->extra.tmd->coords;
    if (work->knockdownPending != 0) {
        work->knockdownPending = 0;
        _actor400500EnterState(arg0, ACTOR_400500_STATE_KNOCKDOWN);
        flag = 1;
    } else {
        flag = 0;
    }
    if (flag == 0) {
        work2 = (_Actor400500GrayStalkerWork*)arg0->work;
        if (work2->ceilingFallPending != 0) {
            work2->ceilingFallPending = 0;
            if (work2->posture & ACTOR_400500_POSTURE_ON_FLOOR) {
                flag2 = 0;
            } else {
                _actor400500EnterState(arg0, ACTOR_400500_STATE_FALL);
                flag2 = 1;
            }
        } else {
            flag2 = 0;
        }
        if ((flag2 == 0) && ((_actor400500TryStartAttack(arg0) << 0x10) == 0) &&
            ((_actor400500HandleHeavyHitReaction(arg0) << 0x10) == 0) &&
            ((func_actor_400500_80133160(arg0) << 0x10) == 0)) {
            heading = (u16)work->yaw;
            if ((heading & 0xFFF) != 0x800) {
                if (((0x800 - heading) << 0x14) > 0) {
                    work2              = (_Actor400500GrayStalkerWork*)arg0->work;
                    work2->turnRequest = ACTOR_400500_TURN_YAW_UP;
                } else {
                    work2              = (_Actor400500GrayStalkerWork*)arg0->work;
                    work2->turnRequest = ACTOR_400500_TURN_YAW_DOWN;
                }
                work2->turnStarted = 0;
            } else {
                switch (work->zone) {
                    case 2:
                        if (coord->coord.t[2] < -0x209E) {
                            work->subState = 9;
                        }
                        break;
                    case 3:
                        if ((work->attackCooling == 0) && (work->toTarget.vz > 0)) {
                            work->subState = 0xA;
                        }
                        break;
                }
                func_actor_400500_801335E8(arg0);
            }
            coord->coord.t[0] = 0x4074;
        }
    }
}

static void func_actor_400500_80137338(Task* arg0)
{
    _Actor400500GrayStalkerWork* work;
    _Actor400500GrayStalkerWork* work2;
    _Actor400500GrayStalkerWork* work3;
    _Actor400500GrayStalkerWork* work4;
    s32                          flag;
    s32                          flag2;
    s32                          heading;

    work = (_Actor400500GrayStalkerWork*)arg0->work;
    if (work->knockdownPending != 0) {
        work->knockdownPending = 0;
        _actor400500EnterState(arg0, ACTOR_400500_STATE_KNOCKDOWN);
        flag = 1;
    } else {
        flag = 0;
    }
    if (flag == 0) {
        work2 = (_Actor400500GrayStalkerWork*)arg0->work;
        if (work2->ceilingFallPending != 0) {
            work2->ceilingFallPending = 0;
            if (work2->posture & ACTOR_400500_POSTURE_ON_FLOOR) {
                flag2 = 0;
            } else {
                _actor400500EnterState(arg0, ACTOR_400500_STATE_FALL);
                flag2 = 1;
            }
        } else {
            flag2 = 0;
        }
        if ((flag2 == 0) && ((_actor400500TryStartAttack(arg0) << 0x10) == 0) &&
            ((_actor400500HandleHeavyHitReaction(arg0) << 0x10) == 0) &&
            ((func_actor_400500_80133160(arg0) << 0x10) == 0)) {
            heading = (u16)work->yaw;
            if ((heading & 0xFFF) == 0xC00) {
                work2              = (_Actor400500GrayStalkerWork*)arg0->work;
                work2->animRate    = 0x18;
                work2->animId      = 2;
                work2->animRequest = ACTOR_400500_ANIM_REQUEST_RESET;
                work->subState     = 3;
                return;
            }
            if (((0xC00 - heading) << 0x14) > 0) {
                work3              = (_Actor400500GrayStalkerWork*)arg0->work;
                work3->turnRequest = ACTOR_400500_TURN_YAW_UP;
                work3->turnStarted = 0;
            } else {
                work4              = (_Actor400500GrayStalkerWork*)arg0->work;
                work4->turnRequest = ACTOR_400500_TURN_YAW_DOWN;
                work4->turnStarted = 0;
            }
        }
    }
}

static void func_actor_400500_80137478(Task* arg0)
{
    _Actor400500GrayStalkerWork* work;
    _Actor400500GrayStalkerWork* work2;
    _Actor400500GrayStalkerWork* work3;
    _Actor400500GrayStalkerWork* work4;
    s32                          flag;
    s32                          flag2;
    s32                          heading;

    work = (_Actor400500GrayStalkerWork*)arg0->work;
    if (work->knockdownPending != 0) {
        work->knockdownPending = 0;
        _actor400500EnterState(arg0, ACTOR_400500_STATE_KNOCKDOWN);
        flag = 1;
    } else {
        flag = 0;
    }
    if (flag == 0) {
        work2 = (_Actor400500GrayStalkerWork*)arg0->work;
        if (work2->ceilingFallPending != 0) {
            work2->ceilingFallPending = 0;
            if (work2->posture & ACTOR_400500_POSTURE_ON_FLOOR) {
                flag2 = 0;
            } else {
                _actor400500EnterState(arg0, ACTOR_400500_STATE_FALL);
                flag2 = 1;
            }
        } else {
            flag2 = 0;
        }
        if ((flag2 == 0) && ((_actor400500TryStartAttack(arg0) << 0x10) == 0) &&
            ((_actor400500HandleHeavyHitReaction(arg0) << 0x10) == 0) &&
            ((func_actor_400500_80133160(arg0) << 0x10) == 0)) {
            heading = (u16)work->yaw;
            if ((heading & 0xFFF) == 0) {
                work2              = (_Actor400500GrayStalkerWork*)arg0->work;
                work2->animRate    = 0x18;
                work2->animId      = 2;
                work2->animRequest = ACTOR_400500_ANIM_REQUEST_RESET;
                work->subState     = 6;
                return;
            }
            if (((0 - heading) << 0x14) > 0) {
                work3              = (_Actor400500GrayStalkerWork*)arg0->work;
                work3->turnRequest = ACTOR_400500_TURN_YAW_UP;
                work3->turnStarted = 0;
            } else {
                work4              = (_Actor400500GrayStalkerWork*)arg0->work;
                work4->turnRequest = ACTOR_400500_TURN_YAW_DOWN;
                work4->turnStarted = 0;
            }
        }
    }
}

static void func_actor_400500_801375B8(Task* arg0)
{
    _Actor400500GrayStalkerWork* work;
    _Actor400500GrayStalkerWork* work2;
    _Actor400500GrayStalkerWork* work3;
    _Actor400500GrayStalkerWork* work4;
    s32                          flag;
    s32                          i;

    work              = (_Actor400500GrayStalkerWork*)arg0->work;
    work->body.radius = 0x130;
    work2             = (_Actor400500GrayStalkerWork*)arg0->work;
    if ((work2->cloakRequest >= 0) || (((u8)work2->cloakRequest & ACTOR_400500_CLOAK_KIND_MASK) != ACTOR_400500_CLOAK_SHOW)) {
        flag                = ACTOR_400500_CLOAK_RUNNING | ACTOR_400500_CLOAK_SHOW;
        work2->cloakRequest = flag;
        work2->cloakPhase   = 0;
    }
    work3              = (_Actor400500GrayStalkerWork*)arg0->work;
    work3->animRate    = ANIMATION_RATE_ONE;
    work3->animId      = 5;
    work3->animRequest = ACTOR_400500_ANIM_REQUEST_RESET;
    work4              = (_Actor400500GrayStalkerWork*)arg0->work;
    if (work4->animRequest == ACTOR_400500_ANIM_REQUEST_BLEND) {
        if (work4->appliedAnim != work4->animId) {
            work4->animFrames = 0;
        } else {
            work4->animFrames = _actor400500RescaleAnimFrames(arg0, work4->animFrames);
        }
        _actor400500BlendAnimSlots(arg0);
        work4->animRequest = ACTOR_400500_ANIM_REQUEST_PLAYING;
    } else if (work4->animRequest == ACTOR_400500_ANIM_REQUEST_RESET) {
        _actor400500ResetAnimSlots(arg0);
        work4->animRequest = ACTOR_400500_ANIM_REQUEST_PLAYING;
        work4->animFrames  = 0;
    } else if (work4->animRequest == ACTOR_400500_ANIM_REQUEST_PLAYING) {
        work4->animFrames = (u16)work4->animFrames + 1;
    }
    i = 1;
    do {
        work4->rig.slots[i].rate = work4->animRate;
        animationTickSlot(&work4->rig.anim, i);
        i++;
    } while (i < ARRAY_SIZE(work4->rig.slots));
    work->stateFrames   = 0;
    work->playerCaught  = 0;
    work->armReachAngle = 0;
    work->subState      = work->subState + 1;
}

/// Requests the cloak fade `mode` (`ACTOR_400500_CLOAK_*`) and restarts its
/// phases, unless a fade of that kind is already running or `hideCooldown` is
/// nonzero.
static inline void _actor400500RequestMode(Task* task, s32 mode)
{
    _Actor400500GrayStalkerWork* work;

    work = (_Actor400500GrayStalkerWork*)task->work;
    if (((work->cloakRequest >= 0) || ((work->cloakRequest & ACTOR_400500_CLOAK_KIND_MASK) != (mode & ACTOR_400500_CLOAK_KIND_MASK))) && (work->hideCooldown == 0)) {
        work->cloakRequest = mode;
        work->cloakPhase   = 0;
    }
}

/// Composes `coord`'s matrix with each ancestor's normalised matrix up the
/// `parent` chain, leaving the product in `matrix`. Returns 1 when the chain
/// reaches the view coordinate and 0 when it ends before it.
static inline s32 _actor400500CoordToView(GfxCoord* coord, MATRIX* matrix)
{
    MATRIX    result;
    MATRIX    parent;
    GfxCoord* current;

    current = coord->parent;
    *matrix = coord->coord;
    while (1) {
        if (current == NULL) {
            return 0;
        }
        if (current == &gGfxViewCoord) {
            return 1;
        }
        parent = current->coord;
        MatrixNormal(&parent, &parent);
        gte_SetRotMatrix(&parent);
        MulRotMatrix(matrix);
        MatrixNormal(matrix, &result);
        *matrix = result;
        current = current->parent;
    }
}

/// Turns `part` by `heading` in view space: builds the part's view-space
/// matrix in a scratch-pad block, applies `heading` with `RotMatrixY`,
/// takes it back into the part's own space with `func_actor_400500_8013B720`,
/// and copies the resulting rotation (not the translation) into the part
/// before refreshing it.
static inline void _actor400500TurnPart(GfxCoord* part, u16 heading)
{
    MATRIX* matrix;

    matrix = SCRATCH_STACK_RESERVE_BLOCK(MATRIX);
    _actor400500CoordToView(part, matrix);
    RotMatrixY((s16)(heading), matrix);
    func_actor_400500_8013B720(part, matrix);
    memcpy(part->coord.m, matrix->m, sizeof(part->coord.m));
    part->composeStamp = GRAPHICS_COORD_DIRTY;
    actorRenderComposeCoord(part);
    SCRATCH_STACK_RELEASE_BLOCK(MATRIX);
}

/// Returns the horizontal distance in view space between the player's part 4
/// and `part`, or 0x7FFF when there is no player.
static inline s16 _actor400500PlayerDistance(GfxCoord* part)
{
    MATRIX    playerView;
    MATRIX    partView;
    GfxCoord* playerCoords;
    SVECTOR   delta;

    if (gPlayerActorTasks[PLAYER_ACTOR_TASK_PLAYER] == NULL) {
        return 0x7FFF;
    }
    playerCoords = gPlayerActorTasks[PLAYER_ACTOR_TASK_PLAYER]->extra.tmd->coords;
    actorRenderComposeCoord(&playerCoords[4]);
    actorRenderComposeCoord(part);
    gfxMakeRelativeTransform(&gGfxViewCoord.workm, &playerCoords[4].workm, &playerView);
    gfxMakeRelativeTransform(&gGfxViewCoord.workm, &part->workm, &partView);
    delta.vx = (u16)playerView.t[0] - (u16)partView.t[0];
    delta.vz = (u16)playerView.t[2] - (u16)partView.t[2];
    return SquareRoot0((delta.vx * delta.vx) + (delta.vz * delta.vz));
}

/// Starts animation `id` at rate 0x10.
static inline void _actor400500PlayAnim(Task* task, s32 id)
{
    _Actor400500GrayStalkerWork* work;

    work                  = (_Actor400500GrayStalkerWork*)task->work;
    work->animBlendFrames = 2;
    work->animRate        = ANIMATION_RATE_ONE;
    work->animId          = id;
    work->animRequest     = ACTOR_400500_ANIM_REQUEST_BLEND;
}

static void func_actor_400500_8013771C(Task* arg0)
{
    SVECTOR                      in;
    SVECTOR                      out;
    GfxMatrix                    rot;
    _Actor400500GrayStalkerWork* work;
    _Actor400500GrayStalkerWork* hit;
    GameActor*                   player;
    void*                        spawn;
    s16                          vz;
    s32                          r;
    s32                          cond;

    work              = (_Actor400500GrayStalkerWork*)arg0->work;
    player            = gPlayerActorTasks[PLAYER_ACTOR_TASK_PLAYER]->work;
    spawn             = arg0->spawnArg2.pointer;
    work->stateFrames = work->stateFrames + 1;
    _actor400500TickAnim(arg0);
    if ((s16)work->stateFrames < 0xF) {
        in.vx = (u16)work->toTarget.vx;
        in.vy = 0;
        vz    = (u16)work->toTarget.vz;
        in.vz = vz;
        gfxSetRotIdentity(&rot.mat);
        RotMatrixY(work->yaw, &rot.mat);
        ApplyMatrixSV(&rot.mat, &in, &out);
        r                    = ratan2(out.vx, work->toTarget.vy - 0x6A0);
        work->armReachAngle += ((-r - (u16)work->armReachAngle) << 20) >> 23;
    } else {
        work->armReachAngle += -((u16)work->armReachAngle << 20) >> 23;
    }
    if ((s16)work->stateFrames == 7) {
        if (player->mode != GAME_ACTOR_MODE_SCRIPTED) {
            if (_actor400500PlayerDistance(&arg0->extra.tmd->coords[8]) < 0x500) {
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
        _actor400500PlayAnim(arg0, 6);
        work->stateFrames = 0;
        _actor400500RequestMode(arg0, ACTOR_400500_CLOAK_RUNNING | ACTOR_400500_CLOAK_HIDE);
        work->subState = work->subState + 1;
    }
    _actor400500TurnPart(&arg0->extra.tmd->coords[6], work->armReachAngle);
    _actor400500TurnPart(&arg0->extra.tmd->coords[9], work->armReachAngle);
    if (((u32)(work->stateFrames - 7) < 4U) && (work->playerCaught == 1)) {
        func_actor_400500_801348D8(arg0, 1);
    }
    if (((u32)(work->stateFrames - 0xB) < 0x14U) && (work->playerCaught == 1)) {
        func_actor_400500_801348D8(arg0, 0);
    }
    if (((s16)work->stateFrames == 0xC) && (work->playerCaught != 0)) {
        _actor400500PlaySound(arg0, 6);
    }
    if ((s16)work->stateFrames == 0x1E) {
        _actor400500PlaySound(arg0, 0x40050007);
    }
    if ((s16)work->stateFrames == 0x2A) {
        padScriptSpawnVariableMotorRamp(8, 0xC0U, 8U);
        _actor400500PlaySound(arg0, 0x40050008);
    }
    if ((s16)work->stateFrames == 0x1F) {
        padScriptSpawnVariableMotorRamp(6, 0xFFU, 0x80U);
        if (actorPlayerContactMessage(spawn, 1) != 0) {
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

static void func_actor_400500_80138088(Task* arg0)
{
    _Actor400500GrayStalkerWork* work;
    _Actor400500GrayStalkerWork* work2;
    _Actor400500GrayStalkerWork* hit;
    s32                          i;
    s32                          cond;

    work              = (_Actor400500GrayStalkerWork*)arg0->work;
    work->stateFrames = work->stateFrames + 1;
    work2             = (_Actor400500GrayStalkerWork*)arg0->work;
    if (work2->animRequest == ACTOR_400500_ANIM_REQUEST_BLEND) {
        if (work2->appliedAnim != work2->animId) {
            work2->animFrames = 0;
        } else {
            work2->animFrames = _actor400500RescaleAnimFrames(arg0, work2->animFrames);
        }
        _actor400500BlendAnimSlots(arg0);
        work2->animRequest = ACTOR_400500_ANIM_REQUEST_PLAYING;
    } else if (work2->animRequest == ACTOR_400500_ANIM_REQUEST_RESET) {
        _actor400500ResetAnimSlots(arg0);
        work2->animRequest = ACTOR_400500_ANIM_REQUEST_PLAYING;
        work2->animFrames  = 0;
    } else if (work2->animRequest == ACTOR_400500_ANIM_REQUEST_PLAYING) {
        work2->animFrames = (u16)work2->animFrames + 1;
    }
    i = 1;
    do {
        work2->rig.slots[i].rate = work2->animRate;
        animationTickSlot(&work2->rig.anim, i);
        i++;
    } while (i < ARRAY_SIZE(work2->rig.slots));

    work->armReachAngle += -(work->armReachAngle * 16) >> 7;
    _actor400500TurnPart(&arg0->extra.tmd->coords[6], work->armReachAngle);
    _actor400500TurnPart(&arg0->extra.tmd->coords[9], work->armReachAngle);

    hit = (_Actor400500GrayStalkerWork*)arg0->work;
    if ((hit->rig.slots[1].status.fields.flags & ANIMATION_SLOT_REACHED_BOUNDARY) ||
        (hit->rig.slots[1].status.word & (ANIMATION_SLOT_FOLLOWED_JUMP | ANIMATION_SLOT_SETTLED))) {
        cond = 1;
    } else {
        cond = 0;
    }
    if (cond) {
        work->body.radius = 0x260;
        _actor400500SetState(arg0, ACTOR_400500_STATE_CRAWL, 0);
        _actor400500RequestMode(arg0, ACTOR_400500_CLOAK_RUNNING | ACTOR_400500_CLOAK_HIDE);
        work->attackCooldown = 0x3C;
    }
}

/// Sub-state handlers `func_actor_400500_8013BA24` copies onto the stack and runs by `subState`.
static const TaskFuncTable3 D_actor_400500_80131EE4 = { {
    func_actor_400500_801375B8,
    func_actor_400500_8013771C,
    func_actor_400500_80138088,
} };

/// Sub-state handlers `func_actor_400500_801385D0` copies onto the stack and runs
/// by `subState` while the enemy is alive.
static const TaskFuncTable3 D_actor_400500_80131EF0 = { {
    func_actor_400500_8013BE50,
    func_actor_400500_8013BEC4,
    func_actor_400500_801387E8,
} };

/// Handlers `func_actor_400500_801385D0` runs by `deathStep`, instead of the
/// sub-state, once the enemy is out of hit points.
static const TaskFuncTable3 D_actor_400500_80131EFC = { {
    func_actor_400500_8013BC9C,
    func_actor_400500_8013BCCC,
    func_actor_400500_8013BD64,
} };

static void func_actor_400500_801385D0(Task* arg0)
{
    _Actor400500GrayStalkerWork* work;
    Enemy*                       enemy;
    TaskFuncTable3               sp10;
    TaskFuncTable3               sp20;

    work  = (_Actor400500GrayStalkerWork*)arg0->work;
    enemy = (Enemy*)arg0->spawnArg2.pointer;
    sp10  = D_actor_400500_80131EF0;
    sp20  = D_actor_400500_80131EFC;
    if (enemy->hp <= 0) {
        if (work->lastHitReaction == ACTOR_400500_HIT_REACTION_BLAST) {
            work->deathHeld = 0;
        } else {
            sp20.funcs[(s16)work->deathStep](arg0);
        }
        work->rightArmOuter.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->rightArmInner.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->leftArmOuter.flags  &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->leftArmInner.flags  &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
    } else {
        if (_actor400500TakeKnockdown(arg0)) {
            return;
        }
        sp10.funcs[(s16)work->subState](arg0);
        ((_Actor400500GrayStalkerWork*)arg0->work)->ceilingFallPending = 0;
        _actor400500HandleHeavyHitReaction(arg0);
    }
    _actor400500TickAnim(arg0);
}

static void func_actor_400500_801387E8(Task* arg0)
{
    _Actor400500GrayStalkerWork* work;
    _Actor400500GrayStalkerWork* work2;
    _Actor400500GrayStalkerWork* work3;
    _Actor400500GrayStalkerWork* hit;
    GfxMatrix                    rot;
    s32                          soundId;
    s32                          pan;
    s32                          cond;
    s32                          flag;
    u16                          frame;

    work              = (_Actor400500GrayStalkerWork*)arg0->work;
    frame             = work->stateFrames + 1;
    work->stateFrames = frame;
    if ((s16)frame == 0xB) {
        work->rightArmOuter.flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;
        work->rightArmInner.flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;
        soundId                    = ((((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x40050005;
        pan                        = (s8)worldCoordGetOriginAudioPan(arg0->extra.tmd->coords);
        sndEvtRequestScriptStart(soundId, pan, (s8)worldCoordGetOriginAudioDepth(arg0->extra.tmd->coords));
    }
    if ((s16)work->stateFrames >= 0x12) {
        if ((s16)work->armSwingAngle != 0) {
            work->armSwingAngle = (u16)work->armSwingAngle - 0x80;
        } else {
            ((_Actor400500GrayStalkerWork*)arg0->work)->armTasks[1]->extra.tmd->flags = TMD_OBJECT_SKIP_ACTIVE_DRAW;
            work->rightArmOuter.flags                                                &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
            work->rightArmInner.flags                                                &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
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
        work2           = (_Actor400500GrayStalkerWork*)arg0->work;
        work2->state    = ACTOR_400500_STATE_CRAWL;
        work2->subState = 0;
        work3           = (_Actor400500GrayStalkerWork*)arg0->work;
        if (((work3->cloakRequest >= 0) || ((u8)work3->cloakRequest & ACTOR_400500_CLOAK_KIND_MASK)) && (work3->hideCooldown == 0)) {
            flag                = ACTOR_400500_CLOAK_RUNNING | ACTOR_400500_CLOAK_HIDE;
            work3->cloakRequest = flag;
            work3->cloakPhase   = 0;
        }
        work->attackCooldown = 0x3C;
    }
}

/// Sub-state handlers `func_actor_400500_8013899C` copies onto the stack and runs by `subState`.
static const TaskFuncTable5 D_actor_400500_80131F08 = { {
    func_actor_400500_8013BFB0,
    func_actor_400500_8013C018,
    func_actor_400500_80138B78,
    func_actor_400500_80138CE8,
    func_actor_400500_80138DC4,
} };

static void func_actor_400500_8013899C(Task* arg0)
{
    _Actor400500GrayStalkerWork* work;
    Enemy*                       enemy;
    TaskFuncTable5               sp;

    work  = (_Actor400500GrayStalkerWork*)arg0->work;
    enemy = (Enemy*)arg0->spawnArg2.pointer;
    sp    = D_actor_400500_80131F08;
    if (enemy->hp > 0) {
        if (_actor400500TakeKnockdown(arg0)) {
            return;
        }
        sp.funcs[(s16)work->subState](arg0);
    } else {
        work->deathHeld            = 0;
        work->rightArmOuter.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->rightArmInner.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->leftArmOuter.flags  &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->leftArmInner.flags  &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
    }
    _actor400500HandleHeavyHitReaction(arg0);
    _actor400500TickAnim(arg0);
    ((_Actor400500GrayStalkerWork*)arg0->work)->ceilingFallPending = 0;
}

static void func_actor_400500_80138B78(Task* arg0)
{
    _Actor400500GrayStalkerWork* work;
    _Actor400500GrayStalkerWork* hit;
    GfxMatrix                    rot;
    s32                          soundId;
    s32                          pan;
    s32                          cond;
    u16                          frame;

    work              = (_Actor400500GrayStalkerWork*)arg0->work;
    frame             = work->stateFrames + 1;
    work->stateFrames = frame;
    if ((s16)frame == 0xD) {
        work->leftArmOuter.flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;
        work->leftArmInner.flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;
        soundId                   = ((((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x40050005;
        pan                       = (s8)worldCoordGetOriginAudioPan(arg0->extra.tmd->coords);
        sndEvtRequestScriptStart(soundId, pan, (s8)worldCoordGetOriginAudioDepth(arg0->extra.tmd->coords));
    }
    if ((s16)work->stateFrames >= 0x10) {
        if ((s16)work->armSwingAngle != 0) {
            work->armSwingAngle = (u16)work->armSwingAngle - 0x80;
        } else {
            ((_Actor400500GrayStalkerWork*)arg0->work)->armTasks[0]->extra.tmd->flags = TMD_OBJECT_SKIP_ACTIVE_DRAW;
            work->leftArmOuter.flags                                                 &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
            work->leftArmInner.flags                                                 &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
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
        work->subState = work->subState + 1;
    }
}

static void func_actor_400500_80138CE8(Task* arg0)
{
    _Actor400500GrayStalkerWork* work;
    Task*                        child;
    s32                          angle;

    work                    = (_Actor400500GrayStalkerWork*)arg0->work;
    angle                   = work->armSwingAngle - 0x80;
    work->armSwingAngle     = angle;
    child                   = ((_Actor400500GrayStalkerWork*)arg0->work)->armTasks[0];
    child->extra.tmd->flags = 0;
    _actor400500SetCoordYaw(child->extra.tmd->coords, -angle);
    if ((s16)work->armSwingAngle <= 0) {
        ((_Actor400500GrayStalkerWork*)arg0->work)->armTasks[0]->extra.tmd->flags = TMD_OBJECT_SKIP_ACTIVE_DRAW;
        work->subState                                                            = work->subState + 1;
    }
}

static void func_actor_400500_80138DC4(Task* arg0)
{
    _Actor400500GrayStalkerWork* hit;
    _Actor400500GrayStalkerWork* work;
    _Actor400500GrayStalkerWork* work2;
    _Actor400500GrayStalkerWork* work3;
    s32                          cond;
    s32                          flag;
    u32                          rnd;

    hit = (_Actor400500GrayStalkerWork*)arg0->work;
    if ((hit->rig.slots[1].status.fields.flags & ANIMATION_SLOT_REACHED_BOUNDARY) ||
        (hit->rig.slots[1].status.word & (ANIMATION_SLOT_FOLLOWED_JUMP | ANIMATION_SLOT_SETTLED))) {
        cond = 1;
    } else {
        cond = 0;
    }
    if (cond) {
        work = (_Actor400500GrayStalkerWork*)arg0->work;
        if (((work->cloakRequest >= 0) || ((u8)work->cloakRequest & ACTOR_400500_CLOAK_KIND_MASK)) && (work->hideCooldown == 0)) {
            flag               = ACTOR_400500_CLOAK_RUNNING | ACTOR_400500_CLOAK_HIDE;
            work->cloakRequest = flag;
            work->cloakPhase   = 0;
        }
        hit->attackCooldown = 0x3C;
        rnd                 = (gRandomLcgState * RANDOM_LCG_MULTIPLIER) + RANDOM_LCG_INCREMENT;
        gRandomLcgState     = rnd;
        if (!((rnd >> 0x10) & 3)) {
            work2           = (_Actor400500GrayStalkerWork*)arg0->work;
            work2->state    = ACTOR_400500_STATE_CRAWL;
            work2->subState = 0;
            return;
        }
        work3           = (_Actor400500GrayStalkerWork*)arg0->work;
        work3->state    = ACTOR_400500_STATE_JUMP;
        work3->subState = 0;
    }
}

/// Sub-state handlers `func_actor_400500_80138EA0` copies onto the stack and runs by `subState`.
static const TaskFuncTable4 D_actor_400500_80131F1C = { {
    func_actor_400500_8013C108,
    func_actor_400500_8013C174,
    func_actor_400500_8013C218,
    func_actor_400500_8013C348,
} };

static void func_actor_400500_80138EA0(Task* arg0)
{
    _Actor400500GrayStalkerWork* work;
    Enemy*                       enemy;
    TaskFuncTable4               handlers;
    _Actor400500GrayStalkerWork* work2;
    s32                          i;
    _Actor400500GrayStalkerWork* work3;

    work     = (_Actor400500GrayStalkerWork*)arg0->work;
    enemy    = (Enemy*)arg0->spawnArg2.pointer;
    handlers = D_actor_400500_80131F1C;
    if ((enemy->hp <= 0) && (work->lastHitReaction == ACTOR_400500_HIT_REACTION_BLAST)) {
        work->deathHeld            = 0;
        work->rightArmOuter.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->rightArmInner.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->leftArmOuter.flags  &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->leftArmInner.flags  &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        return;
    }
    handlers.funcs[(s16)work->subState](arg0);
    work2 = (_Actor400500GrayStalkerWork*)arg0->work;
    if (work2->animRequest == ACTOR_400500_ANIM_REQUEST_BLEND) {
        if (work2->appliedAnim != work2->animId) {
            work2->animFrames = 0;
        } else {
            work2->animFrames = _actor400500RescaleAnimFrames(arg0, work2->animFrames);
        }
        _actor400500BlendAnimSlots(arg0);
        work2->animRequest = ACTOR_400500_ANIM_REQUEST_PLAYING;
    } else if (work2->animRequest == ACTOR_400500_ANIM_REQUEST_RESET) {
        _actor400500ResetAnimSlots(arg0);
        work2->animRequest = ACTOR_400500_ANIM_REQUEST_PLAYING;
        work2->animFrames  = 0;
    } else if (work2->animRequest == ACTOR_400500_ANIM_REQUEST_PLAYING) {
        work2->animFrames = (u16)work2->animFrames + 1;
    }
    i = 1;
    do {
        work2->rig.slots[i].rate = work2->animRate;
        animationTickSlot(&work2->rig.anim, i);
        i++;
    } while (i < ARRAY_SIZE(work2->rig.slots));
    work3                     = (_Actor400500GrayStalkerWork*)arg0->work;
    work3->hitTaken           = 0;
    work3->hitReaction        = ACTOR_400500_HIT_REACTION_NONE;
    work3                     = (_Actor400500GrayStalkerWork*)arg0->work;
    work3->ceilingFallPending = 0;
}

/// Sub-state handlers `func_actor_400500_8013905C` copies onto the stack and runs by `subState`.
static const TaskFuncTable7 D_actor_400500_80131F2C = { {
    func_actor_400500_801391B0,
    func_actor_400500_8013C3C4,
    func_actor_400500_8013C474,
    func_actor_400500_8013C508,
    func_actor_400500_8013C578,
    func_actor_400500_8013C61C,
    func_actor_400500_8013C750,
} };

static void func_actor_400500_8013905C(Task* arg0)
{
    _Actor400500GrayStalkerWork* work;
    TaskFuncTable7               sp;
    _Actor400500GrayStalkerWork* work2;
    s32                          i;

    work = (_Actor400500GrayStalkerWork*)arg0->work;
    sp   = D_actor_400500_80131F2C;
    sp.funcs[(s16)work->subState](arg0);
    work2 = (_Actor400500GrayStalkerWork*)arg0->work;
    if (work2->animRequest == ACTOR_400500_ANIM_REQUEST_BLEND) {
        if (work2->appliedAnim != work2->animId) {
            work2->animFrames = 0;
        } else {
            work2->animFrames = _actor400500RescaleAnimFrames(arg0, work2->animFrames);
        }
        _actor400500BlendAnimSlots(arg0);
        work2->animRequest = ACTOR_400500_ANIM_REQUEST_PLAYING;
    } else if (work2->animRequest == ACTOR_400500_ANIM_REQUEST_RESET) {
        _actor400500ResetAnimSlots(arg0);
        work2->animRequest = ACTOR_400500_ANIM_REQUEST_PLAYING;
        work2->animFrames  = 0;
    } else if (work2->animRequest == ACTOR_400500_ANIM_REQUEST_PLAYING) {
        work2->animFrames = (u16)work2->animFrames + 1;
    }
    i = 1;
    do {
        work2->rig.slots[i].rate = work2->animRate;
        animationTickSlot(&work2->rig.anim, i);
        i++;
    } while (i < ARRAY_SIZE(work2->rig.slots));
}

static void func_actor_400500_801391B0(Task* arg0)
{
    _Actor400500GrayStalkerWork* work;
    _Actor400500GrayStalkerWork* work2;
    _Actor400500GrayStalkerWork* work3;
    _Actor400500GrayStalkerWork* work4;
    s32                          flag;
    u16                          flags;
    u8                           unused[0x30];

    work = (_Actor400500GrayStalkerWork*)arg0->work;
    if ((work->cloakRequest >= 0) || (((u8)work->cloakRequest & ACTOR_400500_CLOAK_KIND_MASK) != ACTOR_400500_CLOAK_SHOW)) {
        flag               = ACTOR_400500_CLOAK_RUNNING | ACTOR_400500_CLOAK_SHOW;
        work->cloakRequest = flag;
        work->cloakPhase   = 0;
    }
    ((_Actor400500GrayStalkerWork*)arg0->work)->armTasks[1]->extra.tmd->flags = TMD_OBJECT_SKIP_ACTIVE_DRAW;
    work->rightArmOuter.flags                                                &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
    work->rightArmInner.flags                                                &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
    ((_Actor400500GrayStalkerWork*)arg0->work)->armTasks[0]->extra.tmd->flags = TMD_OBJECT_SKIP_ACTIVE_DRAW;
    work->leftArmOuter.flags                                                 &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
    flags                                                                     = work->posture;
    work->leftArmInner.flags                                                 &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
    if (!(flags & ACTOR_400500_POSTURE_ON_FLOOR)) {
        work->deathHeld    = 1;
        work2              = (_Actor400500GrayStalkerWork*)arg0->work;
        work2->animRate    = ANIMATION_RATE_ONE;
        work2->animId      = 0xF;
        work2->animRequest = ACTOR_400500_ANIM_REQUEST_RESET;
        work->subState     = 3;
    } else if (!(flags & ACTOR_400500_POSTURE_ON_BACK)) {
        work3              = (_Actor400500GrayStalkerWork*)arg0->work;
        work3->animRate    = ANIMATION_RATE_ONE;
        work3->animId      = 0xF;
        work3->animRequest = ACTOR_400500_ANIM_REQUEST_RESET;
        work->subState     = 1;
    } else {
        work4              = (_Actor400500GrayStalkerWork*)arg0->work;
        work4->animRate    = ANIMATION_RATE_ONE;
        work4->animId      = 0x11;
        work4->animRequest = ACTOR_400500_ANIM_REQUEST_RESET;
        work->subState     = 1;
    }
}

static void func_actor_400500_801392D8(Task* arg0)
{
    _Actor400500GrayStalkerWork* work             = (_Actor400500GrayStalkerWork*)arg0->work;
    void                         (*fns[2])(Task*) = { func_actor_400500_8013C7A4, func_actor_400500_80139448 };
    _Actor400500GrayStalkerWork* work2;
    _Actor400500GrayStalkerWork* work3;
    _Actor400500GrayStalkerWork* work4;
    s32                          i;

    fns[(s16)work->subState](arg0);
    work2 = (_Actor400500GrayStalkerWork*)arg0->work;
    if (work2->animRequest == ACTOR_400500_ANIM_REQUEST_BLEND) {
        if (work2->appliedAnim != work2->animId) {
            work2->animFrames = 0;
        } else {
            work2->animFrames = _actor400500RescaleAnimFrames(arg0, work2->animFrames);
        }
        _actor400500BlendAnimSlots(arg0);
        work2->animRequest = ACTOR_400500_ANIM_REQUEST_PLAYING;
    } else if (work2->animRequest == ACTOR_400500_ANIM_REQUEST_RESET) {
        _actor400500ResetAnimSlots(arg0);
        work2->animRequest = ACTOR_400500_ANIM_REQUEST_PLAYING;
        work2->animFrames  = 0;
    } else if (work2->animRequest == ACTOR_400500_ANIM_REQUEST_PLAYING) {
        work2->animFrames = (u16)work2->animFrames + 1;
    }
    i = 1;
    do {
        work2->rig.slots[i].rate = work2->animRate;
        animationTickSlot(&work2->rig.anim, i);
        i++;
    } while (i < ARRAY_SIZE(work2->rig.slots));
    work3                     = (_Actor400500GrayStalkerWork*)arg0->work;
    work3->hitTaken           = 0;
    work3->hitReaction        = ACTOR_400500_HIT_REACTION_NONE;
    work3                     = (_Actor400500GrayStalkerWork*)arg0->work;
    work3->ceilingFallPending = 0;
    work4                     = (_Actor400500GrayStalkerWork*)arg0->work;
    if (work4->knockdownPending != 0) {
        work4->knockdownPending = 0;
        _actor400500EnterState(arg0, ACTOR_400500_STATE_KNOCKDOWN);
    }
}

static void func_actor_400500_80139448(Task* arg0)
{
    _Actor400500GrayStalkerWork* work;
    _Actor400500GrayStalkerWork* work2;
    _Actor400500GrayStalkerWork* work3;
    _Actor400500GrayStalkerWork* work4;
    _Actor400500GrayStalkerWork* work5;
    s16                          mode;
    s32                          soundId;
    s32                          pan;
    s32                          soundId2;
    s32                          pan2;
    s32                          flag;

    work = (_Actor400500GrayStalkerWork*)arg0->work;
    if (work->targetDist < 0x9C4) {
        sceneEngageBattle(1);
        soundId = ((((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x40050004;
        pan     = (s8)worldCoordGetOriginAudioPan(arg0->extra.tmd->coords);
        sndEvtRequestScriptStart(soundId, pan, (s8)worldCoordGetOriginAudioDepth(arg0->extra.tmd->coords));
        work2 = (_Actor400500GrayStalkerWork*)arg0->work;
        if ((work2->cloakRequest >= 0) || (((u8)work2->cloakRequest & ACTOR_400500_CLOAK_KIND_MASK) != ACTOR_400500_CLOAK_SHOW)) {
            flag                = ACTOR_400500_CLOAK_RUNNING | ACTOR_400500_CLOAK_SHOW;
            work2->cloakRequest = flag;
            work2->cloakPhase   = 0;
        }
        work3           = (_Actor400500GrayStalkerWork*)arg0->work;
        work3->state    = ACTOR_400500_STATE_STRIKE_CEILING;
        work3->subState = 0;
        return;
    }
    mode = work->hitTaken;
    if (mode == 1) {
        soundId2 = ((((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x40050004;
        pan2     = (s8)worldCoordGetOriginAudioPan(arg0->extra.tmd->coords);
        sndEvtRequestScriptStart(soundId2, pan2, (s8)worldCoordGetOriginAudioDepth(arg0->extra.tmd->coords));
        work4 = (_Actor400500GrayStalkerWork*)arg0->work;
        if ((work4->cloakRequest >= 0) || (((u8)work4->cloakRequest & ACTOR_400500_CLOAK_KIND_MASK) != mode)) {
            flag                = ACTOR_400500_CLOAK_RUNNING | ACTOR_400500_CLOAK_SHOW;
            work4->cloakRequest = flag;
            work4->cloakPhase   = 0;
        }
        work5           = (_Actor400500GrayStalkerWork*)arg0->work;
        work5->state    = ACTOR_400500_STATE_CRAWL;
        work5->subState = 0;
    }
}

/// Sub-state handlers `func_actor_400500_801395D0` copies onto the stack and runs by `subState`.
static const TaskFuncTable3 D_actor_400500_80131F48 = { {
    func_actor_400500_8013C818,
    func_actor_400500_8013973C,
    func_actor_400500_80139AC4,
} };

static void func_actor_400500_801395D0(Task* arg0)
{
    _Actor400500GrayStalkerWork* work;
    TaskFuncTable3               sp;
    _Actor400500GrayStalkerWork* work2;
    s32                          i;
    _Actor400500GrayStalkerWork* work3;

    work = (_Actor400500GrayStalkerWork*)arg0->work;
    sp   = D_actor_400500_80131F48;
    if ((s16)work->subState != 0) {
        work2 = (_Actor400500GrayStalkerWork*)arg0->work;
        if (work2->animRequest == ACTOR_400500_ANIM_REQUEST_BLEND) {
            if (work2->appliedAnim != work2->animId) {
                work2->animFrames = 0;
            } else {
                work2->animFrames = _actor400500RescaleAnimFrames(arg0, work2->animFrames);
            }
            _actor400500BlendAnimSlots(arg0);
            work2->animRequest = ACTOR_400500_ANIM_REQUEST_PLAYING;
        } else if (work2->animRequest == ACTOR_400500_ANIM_REQUEST_RESET) {
            _actor400500ResetAnimSlots(arg0);
            work2->animRequest = ACTOR_400500_ANIM_REQUEST_PLAYING;
            work2->animFrames  = 0;
        } else if (work2->animRequest == ACTOR_400500_ANIM_REQUEST_PLAYING) {
            work2->animFrames = (u16)work2->animFrames + 1;
        }
        i = 1;
        do {
            work2->rig.slots[i].rate = work2->animRate;
            animationTickSlot(&work2->rig.anim, i);
            i++;
        } while (i < ARRAY_SIZE(work2->rig.slots));
    }
    sp.funcs[(s16)work->subState](arg0);
    work3                     = (_Actor400500GrayStalkerWork*)arg0->work;
    work3->hitTaken           = 0;
    work3->hitReaction        = ACTOR_400500_HIT_REACTION_NONE;
    work3                     = (_Actor400500GrayStalkerWork*)arg0->work;
    work3->ceilingFallPending = 0;
}

static void func_actor_400500_8013973C(Task* arg0)
{
    GfxMatrix                    rot;
    MATRIX                       local0;
    MATRIX                       local3;
    GfxMatrix*                   src;
    MATRIX*                      view;
    _Actor400500GrayStalkerWork* work;
    _Actor400500GrayStalkerWork* workRot;
    _Actor400500GrayStalkerWork* workAnim;
    _Actor400500GrayStalkerWork* work3;
    GfxCoord*                    coordsEarly;
    GfxCoord*                    coordsMain;
    GfxCoord*                    coordsRot;
    GfxCoord*                    part3;
    GfxCoord*                    root;
    SVECTOR3*                    pos;
    SVECTOR3*                    pos2;
    SVECTOR3*                    posMain;
    SVECTOR3*                    posMain2;
    s32                          i;
    s32                          three;
    s32                          curX;
    s32                          curZ;
    s32                          tgtX;
    s32                          tgtZ;
    s32                          dx;
    s32                          dz;
    u16                          step;
    u16                          accum;
    u16                          pitch;
    s32                          y;
    s32                          viewZ;

    work = (_Actor400500GrayStalkerWork*)arg0->work;
    root = arg0->extra.tmd->coords;
    if ((s16)++work->stateFrames < 8) {
        pos2        = &work->anchorPos;
        coordsEarly = arg0->extra.tmd->coords;
        actorRenderComposeCoord(&coordsEarly[3]);
        gfxMakeRelativeTransform(&gGfxViewCoord.workm, &coordsEarly[3].workm, &rot.mat);
        pos                         = pos2;
        pos->vx                     = rot.mat.t[0];
        pos->vz                     = rot.mat.t[2];
        coordsEarly[3].composeStamp = GRAPHICS_COORD_DIRTY;
        return;
    }
    tgtX               = work->dropStartX;
    curX               = work->anchorPos.vx;
    tgtZ               = work->dropStartZ;
    curZ               = work->anchorPos.vz;
    work->anchorPos.vx = (u16)work->anchorPos.vx + ((tgtX - curX) >> 2);
    work->anchorPos.vz = (u16)work->anchorPos.vz + ((tgtZ - curZ) >> 2);
    posMain2           = &work->anchorPos;
    coordsMain         = arg0->extra.tmd->coords;
    part3              = &coordsMain[3];
    actorRenderComposeCoord(part3);
    view = &gGfxViewCoord.workm;
    gfxMakeRelativeTransform(view, &coordsMain->workm, &local0);
    gfxMakeRelativeTransform(view, &coordsMain[3].workm, &local3);
    posMain                    = posMain2;
    dx                         = local3.t[0] - local0.t[0];
    coordsMain->coord.t[0]     = posMain->vx - dx;
    viewZ                      = posMain->vz;
    dz                         = local3.t[2] - local0.t[2];
    coordsMain->coord.t[2]     = viewZ - dz;
    coordsMain->composeStamp   = GRAPHICS_COORD_DIRTY;
    coordsMain[3].composeStamp = GRAPHICS_COORD_DIRTY;
    actorRenderComposeCoord(part3);
    actorRenderComposeCoord(coordsMain);
    step             = (u16)work->moveAccel + 2;
    accum            = (u16)work->moveSpeed + step;
    work->moveSpeed  = accum;
    work->moveAccel  = step;
    y                = root->coord.t[1] + (s16)accum;
    root->coord.t[1] = y;
    pitch            = work->pitch;
    three            = 3;
    if ((pitch & 0xFFF) != 0x800) {
        work->pitch = pitch - 0x80;
    }
    if (root->coord.t[1] >= -0x3E7) {
        y                         = -0x3E8;
        root->coord.t[0]          = work->dropStartX;
        root->coord.t[2]          = work->dropStartZ;
        root->coord.t[1]          = y;
        work->subState            = work->subState + 1;
        root->coord.t[1]          = y;
        src                       = &rot;
        work->pitch               = 0;
        work->roll                = 0;
        work->yaw                 = (u16)work->yaw + 0x800;
        workRot                   = (_Actor400500GrayStalkerWork*)arg0->work;
        coordsRot                 = arg0->extra.tmd->coords;
        workRot->pitch           &= 0xFFF;
        workRot->yaw             &= 0xFFF;
        workRot->roll            &= 0xFFF;
        rot.rotationWords.m00M01  = ONE;
        rot.rotationWords.m02M10  = 0;
        src->rotationWords.m11M12 = ONE;
        rot.rotationWords.m20M21  = 0;
        src->rotationWords.m22    = ONE;
        RotMatrixZ(workRot->roll, &src->mat);
        RotMatrixX(workRot->pitch, &src->mat);
        RotMatrixY(workRot->yaw, &src->mat);
        _actor400500CopyRotation(&src->mat, &coordsRot->coord);
        work3              = (_Actor400500GrayStalkerWork*)arg0->work;
        work3->animRate    = ANIMATION_RATE_ONE;
        work3->animId      = 0x19;
        work3->animRequest = ACTOR_400500_ANIM_REQUEST_RESET;
        workAnim           = (_Actor400500GrayStalkerWork*)arg0->work;
        if (workAnim->animRequest == ACTOR_400500_ANIM_REQUEST_BLEND) {
            if (workAnim->appliedAnim != workAnim->animId) {
                workAnim->animFrames = 0;
            } else {
                workAnim->animFrames = _actor400500RescaleAnimFrames(arg0, workAnim->animFrames);
            }
            _actor400500BlendAnimSlots(arg0);
            workAnim->animRequest = ACTOR_400500_ANIM_REQUEST_PLAYING;
        } else if (workAnim->animRequest == ACTOR_400500_ANIM_REQUEST_RESET) {
            _actor400500ResetAnimSlots(arg0);
            workAnim->animRequest = three;
            workAnim->animFrames  = 0;
        } else if (workAnim->animRequest == three) {
            workAnim->animFrames = (u16)workAnim->animFrames + 1;
        }
        i = 1;
        do {
            workAnim->rig.slots[i].rate = workAnim->animRate;
            animationTickSlot(&workAnim->rig.anim, i);
            i++;
        } while (i < ARRAY_SIZE(workAnim->rig.slots));
        root->composeStamp = GRAPHICS_COORD_DIRTY;
        actorRenderComposeCoord(root);
        work->stateFrames = 0;
    }
}

static void func_actor_400500_80139AC4(Task* arg0)
{
    _Actor400500GrayStalkerWork* work;
    _Actor400500GrayStalkerWork* work2;
    _Actor400500GrayStalkerWork* work3;
    _Actor400500GrayStalkerWork* hit;
    Enemy*                       enemy;
    s32                          soundId;
    s32                          pan;
    s32                          flag;
    s32                          cond;

    enemy = (Enemy*)arg0->spawnArg2.pointer;
    work  = (_Actor400500GrayStalkerWork*)arg0->work;
    if (enemy->hp > 0) {
        if ((s16)++work->stateFrames == 1) {
            soundId = ((((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x40050003;
            pan     = (s8)worldCoordGetOriginAudioPan(arg0->extra.tmd->coords);
            sndEvtRequestScriptStart(soundId, pan, (s8)worldCoordGetOriginAudioDepth(arg0->extra.tmd->coords));
        }
        work2 = (_Actor400500GrayStalkerWork*)arg0->work;
        if (work2->knockdownPending != 0) {
            work2->knockdownPending = 0;
            _actor400500EnterState(arg0, ACTOR_400500_STATE_KNOCKDOWN);
            flag = 1;
        } else {
            flag = 0;
        }
        if (flag == 0) {
            hit = (_Actor400500GrayStalkerWork*)arg0->work;
            if ((hit->rig.slots[1].status.fields.flags & ANIMATION_SLOT_REACHED_BOUNDARY) ||
                (hit->rig.slots[1].status.word & (ANIMATION_SLOT_FOLLOWED_JUMP | ANIMATION_SLOT_SETTLED))) {
                cond = 1;
            } else {
                cond = 0;
            }
            if (cond) {
                work3           = (_Actor400500GrayStalkerWork*)arg0->work;
                work3->state    = ACTOR_400500_STATE_CRAWL;
                work3->subState = 0;
                work->posture  |= ACTOR_400500_POSTURE_ON_FLOOR;
            }
        }
    } else {
        work->deathHeld = 0;
    }
}

/// Sub-state handlers `func_actor_400500_80139C1C` copies onto the stack and runs by `subState`.
static const TaskFuncTable3 D_actor_400500_80131F54 = { {
    func_actor_400500_8013C908,
    func_actor_400500_80139D70,
    func_actor_400500_8013C9D4,
} };

static void func_actor_400500_80139C1C(Task* arg0)
{
    _Actor400500GrayStalkerWork* work;
    TaskFuncTable3               sp;
    _Actor400500GrayStalkerWork* work2;
    s32                          i;
    _Actor400500GrayStalkerWork* work3;

    work = (_Actor400500GrayStalkerWork*)arg0->work;
    sp   = D_actor_400500_80131F54;
    sp.funcs[(s16)work->subState](arg0);
    work2 = (_Actor400500GrayStalkerWork*)arg0->work;
    if (work2->animRequest == ACTOR_400500_ANIM_REQUEST_BLEND) {
        if (work2->appliedAnim != work2->animId) {
            work2->animFrames = 0;
        } else {
            work2->animFrames = _actor400500RescaleAnimFrames(arg0, work2->animFrames);
        }
        _actor400500BlendAnimSlots(arg0);
        work2->animRequest = ACTOR_400500_ANIM_REQUEST_PLAYING;
    } else if (work2->animRequest == ACTOR_400500_ANIM_REQUEST_RESET) {
        _actor400500ResetAnimSlots(arg0);
        work2->animRequest = ACTOR_400500_ANIM_REQUEST_PLAYING;
        work2->animFrames  = 0;
    } else if (work2->animRequest == ACTOR_400500_ANIM_REQUEST_PLAYING) {
        work2->animFrames = (u16)work2->animFrames + 1;
    }
    i = 1;
    do {
        work2->rig.slots[i].rate = work2->animRate;
        animationTickSlot(&work2->rig.anim, i);
        i++;
    } while (i < ARRAY_SIZE(work2->rig.slots));
    work3                     = (_Actor400500GrayStalkerWork*)arg0->work;
    work3->hitTaken           = 0;
    work3->hitReaction        = ACTOR_400500_HIT_REACTION_NONE;
    work3                     = (_Actor400500GrayStalkerWork*)arg0->work;
    work3->ceilingFallPending = 0;
}

static void func_actor_400500_80139D70(Task* arg0)
{
    _Actor400500GrayStalkerWork* work;
    _Actor400500GrayStalkerWork* work2;
    GfxCoord*                    coord;
    Enemy*                       enemy;
    u16                          step;
    u16                          accum;
    s32                          y;
    s16                          angle;
    s32                          soundId;
    s32                          pan;
    s32                          soundId2;
    s32                          pan2;

    coord = arg0->extra.tmd->coords;
    work  = (_Actor400500GrayStalkerWork*)arg0->work;
    enemy = (Enemy*)arg0->spawnArg2.pointer;
    if ((s16)++work->stateFrames < 9) {
        if (enemy->hp <= 0) {
            work->deathHeld = 0;
            return;
        }
        work2 = (_Actor400500GrayStalkerWork*)arg0->work;
        if (work2->knockdownPending != 0) {
            work2->knockdownPending = 0;
            _actor400500EnterState(arg0, ACTOR_400500_STATE_KNOCKDOWN);
        }
    } else {
        step              = (u16)work->moveAccel - 2;
        accum             = (u16)work->moveSpeed + step;
        work->moveSpeed   = accum;
        work->moveAccel   = step;
        y                 = coord->coord.t[1] - (s16)accum;
        coord->coord.t[1] = y;
        if (work->pitch < 0x800) {
            angle       = (u16)work->pitch + 0x98;
            work->pitch = angle;
            if (angle >= 0x801) {
                work->pitch = 0x800;
            }
        }
        if (coord->coord.t[1] < -0xFA0) {
            coord->coord.t[1]  = -0xFA0;
            work->subState     = work->subState + 1;
            coord->coord.t[1]  = -0xFA0;
            work->pitch        = 0;
            work->roll         = 0x800;
            work->yaw          = (u16)work->yaw + 0x800;
            work2              = (_Actor400500GrayStalkerWork*)arg0->work;
            work2->animRate    = ANIMATION_RATE_ONE;
            work2->animId      = 0x19;
            work2->animRequest = ACTOR_400500_ANIM_REQUEST_RESET;
            soundId            = ((((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x40050001;
            pan                = (s8)worldCoordGetOriginAudioPan(arg0->extra.tmd->coords);
            sndEvtRequestScriptStart(soundId, pan, (s8)worldCoordGetOriginAudioDepth(arg0->extra.tmd->coords));
            soundId2 = ((((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x40050002;
            pan2     = (s8)worldCoordGetOriginAudioPan(arg0->extra.tmd->coords);
            sndEvtRequestScriptStart(soundId2, pan2, (s8)worldCoordGetOriginAudioDepth(arg0->extra.tmd->coords));
        }
    }
}

static void func_actor_400500_80139F6C(Task* arg0)
{
    _Actor400500GrayStalkerWork* work             = (_Actor400500GrayStalkerWork*)arg0->work;
    void                         (*fns[2])(Task*) = { func_actor_400500_8013CA38, func_actor_400500_8013A0B8 };
    _Actor400500GrayStalkerWork* work2;
    _Actor400500GrayStalkerWork* work3;
    s32                          i;

    fns[(s16)work->subState](arg0);
    work2 = (_Actor400500GrayStalkerWork*)arg0->work;
    if (work2->animRequest == ACTOR_400500_ANIM_REQUEST_BLEND) {
        if (work2->appliedAnim != work2->animId) {
            work2->animFrames = 0;
        } else {
            work2->animFrames = _actor400500RescaleAnimFrames(arg0, work2->animFrames);
        }
        _actor400500BlendAnimSlots(arg0);
        work2->animRequest = ACTOR_400500_ANIM_REQUEST_PLAYING;
    } else if (work2->animRequest == ACTOR_400500_ANIM_REQUEST_RESET) {
        _actor400500ResetAnimSlots(arg0);
        work2->animRequest = ACTOR_400500_ANIM_REQUEST_PLAYING;
        work2->animFrames  = 0;
    } else if (work2->animRequest == ACTOR_400500_ANIM_REQUEST_PLAYING) {
        work2->animFrames = (u16)work2->animFrames + 1;
    }
    i = 1;
    do {
        work2->rig.slots[i].rate = work2->animRate;
        animationTickSlot(&work2->rig.anim, i);
        i++;
    } while (i < ARRAY_SIZE(work2->rig.slots));
    work3                     = (_Actor400500GrayStalkerWork*)arg0->work;
    work3->hitTaken           = 0;
    work3->hitReaction        = ACTOR_400500_HIT_REACTION_NONE;
    work3                     = (_Actor400500GrayStalkerWork*)arg0->work;
    work3->ceilingFallPending = 0;
}

static void func_actor_400500_8013A0B8(Task* arg0)
{
    GfxMatrix                    rot;
    MATRIX                       local2;
    GfxMatrix*                   src;
    _Actor400500GrayStalkerWork* work;
    _Actor400500GrayStalkerWork* ang;
    _Actor400500GrayStalkerWork* work3;
    _Actor400500GrayStalkerWork* nextWork;
    _Actor400500GrayStalkerWork* anim;
    _Actor400500GrayStalkerWork* hit;
    SVECTOR3*                    pos;
    GfxCoord*                    coords;
    GfxCoord*                    coord;
    GfxCoord*                    coord14;
    Enemy*                       enemy;
    MATRIX*                      view;
    SVECTOR3*                    pos2;
    s32                          z;
    s32                          cond;
    s32                          flag;
    s32                          i;
    s32                          dx;
    s32                          dz;
    s32                          delta;
    s32                          neg;
    u32                          rnd;

    neg    = -1;
    coords = arg0->extra.tmd->coords;
    work   = (_Actor400500GrayStalkerWork*)arg0->work;
    enemy  = (Enemy*)arg0->spawnArg2.pointer;
    if ((s16)work->stateFrames == neg) {
        coord14 = &coords[0xE];
        actorRenderComposeCoord(coord14);
        pos2 = &work->anchorPos;
        view = &gGfxViewCoord.workm;
        gfxMakeRelativeTransform(view, &coords->workm, &rot.mat);
        gfxMakeRelativeTransform(view, &coord14->workm, &local2);
        dx                    = local2.t[0] - rot.mat.t[0];
        coords->coord.t[0]    = work->anchorPos.vx - dx;
        pos                   = pos2;
        z                     = pos->vz;
        delta                 = local2.t[2] - rot.mat.t[2];
        coords->composeStamp  = GRAPHICS_COORD_DIRTY;
        coord14->composeStamp = GRAPHICS_COORD_DIRTY;
        dz                    = delta;
        coords->coord.t[2]    = z - dz;
        actorRenderComposeCoord(coord14);
        actorRenderComposeCoord(coords);
    }
    hit = (_Actor400500GrayStalkerWork*)arg0->work;
    if ((hit->rig.slots[1].status.fields.flags & ANIMATION_SLOT_REACHED_BOUNDARY) ||
        (hit->rig.slots[1].status.word & (ANIMATION_SLOT_FOLLOWED_JUMP | ANIMATION_SLOT_SETTLED))) {
        cond = 1;
    } else {
        cond = 0;
    }
    if (cond) {
        src = &rot;
        if (enemy->hp > 0) {
            work->posture            &= ~ACTOR_400500_POSTURE_ON_BACK;
            work->yaw                 = ((u16)work->yaw + 0x800) & 0xFFF;
            ang                       = (_Actor400500GrayStalkerWork*)arg0->work;
            coord                     = arg0->extra.tmd->coords;
            ang->pitch               &= 0xFFF;
            ang->yaw                 &= 0xFFF;
            ang->roll                &= 0xFFF;
            rot.rotationWords.m00M01  = ONE;
            rot.rotationWords.m02M10  = 0;
            src->rotationWords.m11M12 = ONE;
            rot.rotationWords.m20M21  = 0;
            src->rotationWords.m22    = ONE;
            RotMatrixZ(ang->roll, &src->mat);
            RotMatrixX(ang->pitch, &src->mat);
            RotMatrixY(ang->yaw, &src->mat);
            _actor400500CopyRotation(&src->mat, &coord->coord);
            work3              = (_Actor400500GrayStalkerWork*)arg0->work;
            work3->animRate    = ANIMATION_RATE_ONE;
            work3->animId      = 1;
            work3->animRequest = ACTOR_400500_ANIM_REQUEST_RESET;
            anim               = (_Actor400500GrayStalkerWork*)arg0->work;
            if (anim->animRequest == ACTOR_400500_ANIM_REQUEST_BLEND) {
                if (anim->appliedAnim != anim->animId) {
                    anim->animFrames = 0;
                } else {
                    anim->animFrames = _actor400500RescaleAnimFrames(arg0, anim->animFrames);
                }
                _actor400500BlendAnimSlots(arg0);
                anim->animRequest = ACTOR_400500_ANIM_REQUEST_PLAYING;
            } else if (anim->animRequest == ACTOR_400500_ANIM_REQUEST_RESET) {
                _actor400500ResetAnimSlots(arg0);
                anim->animRequest = ACTOR_400500_ANIM_REQUEST_PLAYING;
                anim->animFrames  = 0;
            } else if (anim->animRequest == ACTOR_400500_ANIM_REQUEST_PLAYING) {
                anim->animFrames = (u16)anim->animFrames + 1;
            }
            i = 1;
            do {
                anim->rig.slots[i].rate = anim->animRate;
                animationTickSlot(&anim->rig.anim, i);
                i++;
            } while (i < ARRAY_SIZE(anim->rig.slots));
            coords->composeStamp = GRAPHICS_COORD_DIRTY;
            actorRenderComposeCoord(coords);
            work3 = (_Actor400500GrayStalkerWork*)arg0->work;
            if (work3->knockdownPending != 0) {
                work3->knockdownPending = 0;
                _actor400500EnterState(arg0, ACTOR_400500_STATE_KNOCKDOWN);
                flag = 1;
            } else {
                flag = 0;
            }
            if (flag == 0) {
                rnd             = (gRandomLcgState * RANDOM_LCG_MULTIPLIER) + RANDOM_LCG_INCREMENT;
                gRandomLcgState = rnd;
                if (((rnd >> 0x10) & 1) == 0) {
                    nextWork           = (_Actor400500GrayStalkerWork*)arg0->work;
                    nextWork->state    = ACTOR_400500_STATE_CRAWL;
                    nextWork->subState = 0;
                    return;
                }
                work3           = (_Actor400500GrayStalkerWork*)arg0->work;
                work3->state    = ACTOR_400500_STATE_JUMP;
                work3->subState = 0;
            }
        } else {
            work->deathHeld           = 0;
            work->yaw                 = ((u16)work->yaw + 0x800) & 0xFFF;
            ang                       = (_Actor400500GrayStalkerWork*)arg0->work;
            coord                     = arg0->extra.tmd->coords;
            ang->pitch               &= 0xFFF;
            ang->yaw                 &= 0xFFF;
            ang->roll                &= 0xFFF;
            rot.rotationWords.m00M01  = ONE;
            rot.rotationWords.m02M10  = 0;
            src->rotationWords.m11M12 = ONE;
            rot.rotationWords.m20M21  = 0;
            src->rotationWords.m22    = ONE;
            RotMatrixZ(ang->roll, &src->mat);
            RotMatrixX(ang->pitch, &src->mat);
            RotMatrixY(ang->yaw, &src->mat);
            _actor400500CopyRotation(&src->mat, &coord->coord);
            coords->composeStamp = GRAPHICS_COORD_DIRTY;
            actorRenderComposeCoord(coords);
        }
    }
}

/// Sub-state handlers `func_actor_400500_8013A484` copies onto the stack and runs by `subState`.
static const TaskFuncTable7 D_actor_400500_80131F60 = { {
    func_actor_400500_8013A5D8,
    func_actor_400500_8013D4F0,
    func_actor_400500_8013D59C,
    func_actor_400500_8013D630,
    func_actor_400500_8013D6A0,
    func_actor_400500_8013D744,
    func_actor_400500_8013D878,
} };

static void func_actor_400500_8013A484(Task* arg0)
{
    _Actor400500GrayStalkerWork* work;
    TaskFuncTable7               sp;
    _Actor400500GrayStalkerWork* work2;
    s32                          i;

    work = (_Actor400500GrayStalkerWork*)arg0->work;
    sp   = D_actor_400500_80131F60;
    sp.funcs[(s16)work->subState](arg0);
    work2 = (_Actor400500GrayStalkerWork*)arg0->work;
    if (work2->animRequest == ACTOR_400500_ANIM_REQUEST_BLEND) {
        if (work2->appliedAnim != work2->animId) {
            work2->animFrames = 0;
        } else {
            work2->animFrames = _actor400500RescaleAnimFrames(arg0, work2->animFrames);
        }
        _actor400500BlendAnimSlots(arg0);
        work2->animRequest = ACTOR_400500_ANIM_REQUEST_PLAYING;
    } else if (work2->animRequest == ACTOR_400500_ANIM_REQUEST_RESET) {
        _actor400500ResetAnimSlots(arg0);
        work2->animRequest = ACTOR_400500_ANIM_REQUEST_PLAYING;
        work2->animFrames  = 0;
    } else if (work2->animRequest == ACTOR_400500_ANIM_REQUEST_PLAYING) {
        work2->animFrames = (u16)work2->animFrames + 1;
    }
    i = 1;
    do {
        work2->rig.slots[i].rate = work2->animRate;
        animationTickSlot(&work2->rig.anim, i);
        i++;
    } while (i < ARRAY_SIZE(work2->rig.slots));
}

static void func_actor_400500_8013A5D8(Task* arg0)
{
    _Actor400500GrayStalkerWork* work;
    _Actor400500GrayStalkerWork* work2;
    _Actor400500GrayStalkerWork* work3;
    _Actor400500GrayStalkerWork* work4;
    s32                          flag;
    u16                          flags;
    u8                           unused[0x30];

    work = (_Actor400500GrayStalkerWork*)arg0->work;
    if ((work->cloakRequest >= 0) || (((u8)work->cloakRequest & ACTOR_400500_CLOAK_KIND_MASK) != ACTOR_400500_CLOAK_SHOW)) {
        flag               = ACTOR_400500_CLOAK_RUNNING | ACTOR_400500_CLOAK_SHOW;
        work->cloakRequest = flag;
        work->cloakPhase   = 0;
    }
    ((_Actor400500GrayStalkerWork*)arg0->work)->armTasks[1]->extra.tmd->flags = TMD_OBJECT_SKIP_ACTIVE_DRAW;
    work->rightArmOuter.flags                                                &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
    work->rightArmInner.flags                                                &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
    ((_Actor400500GrayStalkerWork*)arg0->work)->armTasks[0]->extra.tmd->flags = TMD_OBJECT_SKIP_ACTIVE_DRAW;
    work->leftArmOuter.flags                                                 &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
    flags                                                                     = work->posture;
    work->leftArmInner.flags                                                 &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
    if (!(flags & ACTOR_400500_POSTURE_ON_FLOOR)) {
        work->deathHeld    = 1;
        work2              = (_Actor400500GrayStalkerWork*)arg0->work;
        work2->animRate    = ANIMATION_RATE_ONE;
        work2->animId      = 0xF;
        work2->animRequest = ACTOR_400500_ANIM_REQUEST_RESET;
        work->subState     = 3;
    } else if (!(flags & ACTOR_400500_POSTURE_ON_BACK)) {
        work3              = (_Actor400500GrayStalkerWork*)arg0->work;
        work3->animRate    = ANIMATION_RATE_ONE;
        work3->animId      = 0xF;
        work3->animRequest = ACTOR_400500_ANIM_REQUEST_RESET;
        work->subState     = 1;
    } else {
        work4              = (_Actor400500GrayStalkerWork*)arg0->work;
        work4->animRate    = ANIMATION_RATE_ONE;
        work4->animId      = 0x11;
        work4->animRequest = ACTOR_400500_ANIM_REQUEST_RESET;
        work->subState     = 1;
    }
}

/// State handlers `func_actor_400500_8013A700` copies onto the stack and runs
/// by `state`.
static const TaskFuncTable10 D_actor_400500_80131F7C = { {
    func_actor_400500_8013A8E4,
    func_actor_400500_8013AA98,
    func_actor_400500_8013D8CC,
    func_actor_400500_8013D958,
    func_actor_400500_8013ABE4,
    func_actor_400500_8013D9DC,
    func_actor_400500_8013D9F4,
    func_actor_400500_8013DA24,
    func_actor_400500_8013DA68,
    func_actor_400500_8013DACC,
} };

/// Relights the actor for the world position of `coord`, staged in a `VECTOR`
/// taken off the scratch stack, and sets the back colour of `obj`: a dim grey
/// in rooms 1, 3, 5 and 6 of the stage, a green tint everywhere else.
static inline void _actor400500UpdateColor(Task* arg0, GfxCoord* coord, TmdObject* obj)
{
    VECTOR* block;
    u8      room;

    block                        = (VECTOR*)(SCRATCH_STACK_CURSOR(u8) - 0x10);
    block->vx                    = coord->workm.t[0];
    block->vy                    = coord->workm.t[1];
    block->vz                    = coord->workm.t[2];
    SCRATCH_STACK_CURSOR(VECTOR) = block;
    worldCoordUpdateActorColor(arg0->spawnArg2.pointer, block, 0, 0);
    room = gGameSession->location.loc.room;
    if ((room == 1) || (room == 3) || (room == 5) || (room == 6)) {
        worldCoordSetModelAmbientColor(obj, 0x200, 0x200, 0x200);
    } else {
        worldCoordSetModelAmbientColor(obj, 0x400, 0x1000, 0x400);
    }
    SCRATCH_STACK_RELEASE_BYTES(0x10);
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
                func_actor_400500_80132AB0(arg0, -0xFA0, (u8)(work->shadowShade >> 2));
                func_actor_400500_80132AB0(arg0, -0x3E8, (u8)work->shadowShade);
            }
            return;
    }
}

static void func_actor_400500_8013A8E4(Task* arg0)
{
    _Actor400500GrayStalkerWork* work;
    _Actor400500GrayStalkerWork* work2;
    _Actor400500GrayStalkerWork* work3;
    Enemy*                       enemy;
    s32                          mapped;
    s32                          i;

    work              = (_Actor400500GrayStalkerWork*)arg0->work;
    enemy             = (Enemy*)arg0->spawnArg2.pointer;
    mapped            = D_actor_400500_80153DD4[work->animId];
    work->animRate    = ANIMATION_RATE_ONE;
    work->animRequest = ACTOR_400500_ANIM_REQUEST_RESET;
    work->animId      = mapped;
    work2             = (_Actor400500GrayStalkerWork*)arg0->work;
    if (work2->animRequest == ACTOR_400500_ANIM_REQUEST_BLEND) {
        if (work2->appliedAnim != work2->animId) {
            work2->animFrames = 0;
        } else {
            work2->animFrames = _actor400500RescaleAnimFrames(arg0, work2->animFrames);
        }
        _actor400500BlendAnimSlots(arg0);
        work2->animRequest = ACTOR_400500_ANIM_REQUEST_PLAYING;
    } else if (work2->animRequest == ACTOR_400500_ANIM_REQUEST_RESET) {
        _actor400500ResetAnimSlots(arg0);
        work2->animRequest = ACTOR_400500_ANIM_REQUEST_PLAYING;
        work2->animFrames  = 0;
    } else if (work2->animRequest == ACTOR_400500_ANIM_REQUEST_PLAYING) {
        work2->animFrames = (u16)work2->animFrames + 1;
    }
    i = 1;
    do {
        work2->rig.slots[i].rate = work2->animRate;
        animationTickSlot(&work2->rig.anim, i);
        i++;
    } while (i < ARRAY_SIZE(work2->rig.slots));
    worldTargetUnlinkNode(&enemy->node);
    sceneReleaseBattleRefWithRewards(arg0, 0);
    enemy->recs = 0;
    worldCollisionUnlinkBody(&work->body);
    worldCollisionUnlinkBody(&work->rightArmOuter);
    worldCollisionUnlinkBody(&work->leftArmOuter);
    worldCollisionUnlinkBody(&work->rightArmInner);
    worldCollisionUnlinkBody(&work->leftArmInner);
    gameFlagSetNibble(GAME_FLAG_GRAY_STALKER_DEFEATED, 1);
    if (work->lastHitReaction == ACTOR_400500_HIT_REACTION_BLAST) {
        work3           = (_Actor400500GrayStalkerWork*)arg0->work;
        work3->state    = 6;
        work3->subState = 0;
        return;
    }
    work->state = work->state + 1;
}

static void func_actor_400500_8013AA98(Task* arg0)
{
    _Actor400500GrayStalkerWork* work;
    _Actor400500GrayStalkerWork* work2;
    _Actor400500GrayStalkerWork* hit;
    s32                          i;
    s32                          cond;

    work  = (_Actor400500GrayStalkerWork*)arg0->work;
    work2 = work;
    if (work->animRequest == ACTOR_400500_ANIM_REQUEST_BLEND) {
        if (work->appliedAnim != work->animId) {
            work->animFrames = 0;
        } else {
            work->animFrames = _actor400500RescaleAnimFrames(arg0, work->animFrames);
        }
        _actor400500BlendAnimSlots(arg0);
        work2->animRequest = ACTOR_400500_ANIM_REQUEST_PLAYING;
    } else if (work->animRequest == ACTOR_400500_ANIM_REQUEST_RESET) {
        _actor400500ResetAnimSlots(arg0);
        work->animRequest = ACTOR_400500_ANIM_REQUEST_PLAYING;
        work->animFrames  = 0;
    } else if (work->animRequest == ACTOR_400500_ANIM_REQUEST_PLAYING) {
        work->animFrames = (u16)work->animFrames + 1;
    }
    i = 1;
    do {
        work2->rig.slots[i].rate = work2->animRate;
        animationTickSlot(&work2->rig.anim, i);
        i++;
    } while (i < ARRAY_SIZE(work2->rig.slots));
    hit = (_Actor400500GrayStalkerWork*)arg0->work;
    if ((hit->rig.slots[1].status.fields.flags & ANIMATION_SLOT_REACHED_BOUNDARY) ||
        (hit->rig.slots[1].status.word & (ANIMATION_SLOT_FOLLOWED_JUMP | ANIMATION_SLOT_SETTLED))) {
        cond = 1;
    } else {
        cond = 0;
    }
    if (cond) {
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

/// Sub-state handlers `func_actor_400500_8013AD60` copies onto the stack and runs by `subState`.
static const TaskFuncTable11 D_actor_400500_80131FA4 = { {
    func_actor_400500_8013AF44,
    func_actor_400500_8013B228,
    func_actor_400500_8013CB0C,
    func_actor_400500_8013CBD8,
    func_actor_400500_8013CCDC,
    func_actor_400500_8013B374,
    func_actor_400500_8013CDA8,
    func_actor_400500_8013CE9C,
    func_actor_400500_8013CF68,
    func_actor_400500_8013D078,
    func_actor_400500_8013D144,
} };

static void func_actor_400500_8013AD60(Task* arg0)
{
    _Actor400500GrayStalkerWork* work;
    TaskFuncTable11              sp;
    Enemy*                       enemy;
    _Actor400500GrayStalkerWork* work2;
    s32                          i;
    _Actor400500GrayStalkerWork* work3;
    s32                          flag;

    work  = (_Actor400500GrayStalkerWork*)arg0->work;
    enemy = (Enemy*)arg0->spawnArg2.pointer;
    sp    = D_actor_400500_80131FA4;
    if ((s16)work->subState != 0) {
        work2 = (_Actor400500GrayStalkerWork*)arg0->work;
        if (work2->animRequest == ACTOR_400500_ANIM_REQUEST_BLEND) {
            if (work2->appliedAnim != work2->animId) {
                work2->animFrames = 0;
            } else {
                work2->animFrames = _actor400500RescaleAnimFrames(arg0, work2->animFrames);
            }
            _actor400500BlendAnimSlots(arg0);
            work2->animRequest = ACTOR_400500_ANIM_REQUEST_PLAYING;
        } else if (work2->animRequest == ACTOR_400500_ANIM_REQUEST_RESET) {
            _actor400500ResetAnimSlots(arg0);
            work2->animRequest = ACTOR_400500_ANIM_REQUEST_PLAYING;
            work2->animFrames  = 0;
        } else if (work2->animRequest == ACTOR_400500_ANIM_REQUEST_PLAYING) {
            work2->animFrames = (u16)work2->animFrames + 1;
        }
        i = 1;
        do {
            work2->rig.slots[i].rate = work2->animRate;
            animationTickSlot(&work2->rig.anim, i);
            i++;
        } while (i < ARRAY_SIZE(work2->rig.slots));
    }
    if (enemy->hp > 0) {
        sp.funcs[(s16)work->subState](arg0);
    } else {
        work->deathHeld = 0;
    }
    work3 = (_Actor400500GrayStalkerWork*)arg0->work;
    if (((work3->cloakRequest >= 0) || ((u8)work3->cloakRequest & ACTOR_400500_CLOAK_KIND_MASK)) && (work3->hideCooldown == 0)) {
        flag                = ACTOR_400500_CLOAK_RUNNING | ACTOR_400500_CLOAK_HIDE;
        work3->cloakRequest = flag;
        work3->cloakPhase   = 0;
    }
}

static void func_actor_400500_8013AF44(Task* arg0)
{
    MATRIX                       local;
    _Actor400500GrayStalkerWork* work;
    _Actor400500GrayStalkerWork* work2;
    _Actor400500GrayStalkerWork* work3;
    GfxCoord*                    coord;
    GfxCoord*                    coords;
    SVECTOR3*                    pos;
    SVECTOR3*                    pos2;
    s32                          flag;
    s32                          heading;
    s32                          i;
    u16                          playerZone;

    work    = (_Actor400500GrayStalkerWork*)arg0->work;
    heading = (u16)work->yaw & 0xFFF;
    coord   = arg0->extra.tmd->coords;
    if (work->knockdownPending != 0) {
        work->knockdownPending = 0;
        _actor400500EnterState(arg0, ACTOR_400500_STATE_KNOCKDOWN);
        flag = 1;
    } else {
        flag = 0;
    }
    if ((flag == 0) && ((_actor400500TryTurnOverNearTarget(arg0) << 0x10) == 0)) {
        work2              = (_Actor400500GrayStalkerWork*)arg0->work;
        work2->animRate    = ANIMATION_RATE_ONE;
        work2->animId      = 4;
        work2->animRequest = ACTOR_400500_ANIM_REQUEST_RESET;
        work3              = (_Actor400500GrayStalkerWork*)arg0->work;
        if (work3->animRequest == ACTOR_400500_ANIM_REQUEST_BLEND) {
            if (work3->appliedAnim != work3->animId) {
                work3->animFrames = 0;
            } else {
                work3->animFrames = _actor400500RescaleAnimFrames(arg0, work3->animFrames);
            }
            _actor400500BlendAnimSlots(arg0);
            work3->animRequest = ACTOR_400500_ANIM_REQUEST_PLAYING;
        } else if (work3->animRequest == ACTOR_400500_ANIM_REQUEST_RESET) {
            _actor400500ResetAnimSlots(arg0);
            work3->animRequest = ACTOR_400500_ANIM_REQUEST_PLAYING;
            work3->animFrames  = 0;
        } else if (work3->animRequest == ACTOR_400500_ANIM_REQUEST_PLAYING) {
            work3->animFrames = (u16)work3->animFrames + 1;
        }
        i = 1;
        do {
            work3->rig.slots[i].rate = work3->animRate;
            animationTickSlot(&work3->rig.anim, i);
            i++;
        } while (i < ARRAY_SIZE(work3->rig.slots));
        switch ((s16)((u16)work->zone - 1)) {
            case 3:
                if (heading != 0x400) {
                    work->subState = 4;
                } else {
                    work->subState = 1;
                }
                break;
            case 0:
                if (work->toTarget.vx >= 0) {
                    work->subState = 3;
                } else {
                    work->subState = 1;
                }
                break;
            case 1:
                playerZone = work->playerZone;
                if ((u32)(playerZone - 1) < 2U) {
                    work->subState = 3;
                } else if (((s16)playerZone == 4) || ((s16)work->playerZone == 5)) {
                    work->subState = 3;
                } else if (((s16)playerZone == 3) && (heading == 0)) {
                    if (coord->coord.t[0] >= 0x4074) {
                        work->subState = 6;
                    } else {
                        work->subState = 1;
                    }
                } else {
                    work->subState = 1;
                }
                break;
            case 2:
                if (work->toTarget.vz < 0) {
                    work->subState = 6;
                } else {
                    work->subState = 8;
                }
                break;
            case 5:
                if (heading == 0x800) {
                    work->subState = 8;
                } else {
                    work->subState = 7;
                }
                break;
            default:
                coord->coord.t[0] = -0x3E8;
                coord->coord.t[1] = -0xFA0;
                coord->coord.t[2] = -0x2116;
                work->yaw         = 0x400;
                work->subState    = 1;
                break;
        }
        pos2   = &work->anchorPos;
        coords = arg0->extra.tmd->coords;
        actorRenderComposeCoord(&coords[8]);
        gfxMakeRelativeTransform(&gGfxViewCoord.workm, &coords[8].workm, &local);
        pos                    = pos2;
        pos->vx                = local.t[0];
        pos->vz                = local.t[2];
        coords[8].composeStamp = GRAPHICS_COORD_DIRTY;
    }
}

static void func_actor_400500_8013B228(Task* arg0)
{
    _Actor400500GrayStalkerWork* work;
    _Actor400500GrayStalkerWork* work2;
    _Actor400500GrayStalkerWork* work3;
    GfxCoord*                    coord;
    s32                          flag;
    s32                          zone;
    u16                          playerZone;

    work  = (_Actor400500GrayStalkerWork*)arg0->work;
    coord = arg0->extra.tmd->coords;
    if (work->knockdownPending != 0) {
        work->knockdownPending = 0;
        _actor400500EnterState(arg0, ACTOR_400500_STATE_KNOCKDOWN);
        flag = 1;
    } else {
        flag = 0;
    }
    if ((flag == 0) && ((_actor400500TryTurnOverNearTarget(arg0) << 0x10) == 0) &&
        ((func_actor_400500_80133460(arg0) << 0x10) == 0)) {
        if (((u16)work->yaw & 0xFFF) != 0x400) {
            work2           = (_Actor400500GrayStalkerWork*)arg0->work;
            work2->state    = ACTOR_400500_STATE_TURN_OVER;
            work2->subState = 0;
        } else {
            zone = work->zone;
            if (zone != 1) {
                if ((zone == 2) && (coord->coord.t[0] >= 0x4075)) {
                    playerZone = work->playerZone;
                    if (((u32)(playerZone - 2) < 2U) || ((s16)playerZone == 6)) {
                        work->subState = 5;
                    } else {
                        work->subState = zone;
                    }
                } else {
                    func_actor_400500_8013403C(arg0);
                }
            } else {
                if (work->toTarget.vx < -0xF9F) {
                    work3           = (_Actor400500GrayStalkerWork*)arg0->work;
                    work3->state    = ACTOR_400500_STATE_TURN_OVER;
                    work3->subState = 0;
                }
                func_actor_400500_8013403C(arg0);
            }
        }
        coord->coord.t[2] = -0x209E;
    }
}

static void func_actor_400500_8013B374(Task* arg0)
{
    _Actor400500GrayStalkerWork* work;
    _Actor400500GrayStalkerWork* work2;
    _Actor400500GrayStalkerWork* work3;
    _Actor400500GrayStalkerWork* work4;
    s32                          flag;
    u16                          playerZone;

    work = (_Actor400500GrayStalkerWork*)arg0->work;
    if (work->knockdownPending != 0) {
        work->knockdownPending = 0;
        _actor400500EnterState(arg0, ACTOR_400500_STATE_KNOCKDOWN);
        flag = 1;
    } else {
        flag = 0;
    }
    if ((flag == 0) && ((_actor400500TryTurnOverNearTarget(arg0) << 0x10) == 0) &&
        ((func_actor_400500_80133460(arg0) << 0x10) == 0)) {
        playerZone = work->playerZone;
        if (((u32)(playerZone - 2) < 2U) || ((s16)playerZone == 6)) {
            if (!((u16)work->yaw & 0xFFF)) {
                work2              = (_Actor400500GrayStalkerWork*)arg0->work;
                work2->animRate    = ANIMATION_RATE_ONE;
                work2->animId      = 4;
                work2->animRequest = ACTOR_400500_ANIM_REQUEST_RESET;
                work->subState     = 6;
                return;
            }
        } else if (((u16)work->yaw & 0xFFF) == 0xC00) {
            work3              = (_Actor400500GrayStalkerWork*)arg0->work;
            work3->animRate    = ANIMATION_RATE_ONE;
            work3->animId      = 4;
            work3->animRequest = ACTOR_400500_ANIM_REQUEST_RESET;
            work->subState     = 3;
            return;
        }
        work4           = (_Actor400500GrayStalkerWork*)arg0->work;
        work4->state    = ACTOR_400500_STATE_TURN_OVER;
        work4->subState = 0;
    }
}

static void func_actor_400500_8013B4A4(Task* arg0)
{
    _Actor400500GrayStalkerWork* work;
    Enemy*                       enemy;
    u8                           mode;

    work  = (_Actor400500GrayStalkerWork*)arg0->work;
    mode  = gGameSession->location.loc.room;
    enemy = (Enemy*)arg0->spawnArg2.pointer;
    if ((mode == 1) || (mode == 3) || (mode == 5) || (mode == 6)) {
        s16 hp;
        s32 maxHp;
        s32 quarter;

        hp      = enemy->hp;
        maxHp   = enemy->hpMax << 0x10;
        quarter = maxHp >> 0x12;
        if ((quarter + (maxHp >> 0x11)) < hp) {
            work->hideHoldFrames    = 0x10;
            work->hideCooldownReset = 0;
            return;
        }
        if (quarter < hp) {
            work->hideHoldFrames    = 0x20;
            work->hideCooldownReset = 0x40;
            return;
        }
        if ((maxHp >> 0x13) < hp) {
            work->hideHoldFrames    = 0x30;
            work->hideCooldownReset = 0x80;
            return;
        }
        if ((maxHp >> 0x14) < hp) {
            work->hideHoldFrames    = 0x40;
            work->hideCooldownReset = 0xC0;
            return;
        }
        work->hideHoldFrames    = 0x50;
        work->hideCooldownReset = 0x100;
        return;
    } else {
        s16 hp;
        s32 maxHp;
        s32 quarter;

        hp      = enemy->hp;
        maxHp   = enemy->hpMax << 0x10;
        quarter = maxHp >> 0x12;
        if ((quarter + (maxHp >> 0x11)) < hp) {
            work->hideHoldFrames    = 0x2000;
            work->hideCooldownReset = 0x20;
            return;
        }
        if (quarter < hp) {
            work->hideHoldFrames    = 0x2000;
            work->hideCooldownReset = 0x40;
            return;
        }
        if ((maxHp >> 0x13) < hp) {
            work->hideHoldFrames    = 0x2000;
            work->hideCooldownReset = 0x80;
            return;
        }
        if ((maxHp >> 0x14) < hp) {
            work->hideHoldFrames    = 0x2000;
            work->hideCooldownReset = 0xC0;
            return;
        }
        work->hideHoldFrames    = 0x2000;
        work->hideCooldownReset = 0x100;
    }
}

/// Sub-state handlers `func_actor_400500_8013B5E0` copies onto the stack and runs by `subState`.
static const TaskFuncTable5 D_actor_400500_80131FEC = { {
    func_actor_400500_8013D210,
    func_actor_400500_8013D274,
    func_actor_400500_8013D2D8,
    func_actor_400500_8013D3B8,
    func_actor_400500_8013D420,
} };

static void func_actor_400500_8013B5E0(Task* arg0)
{
    _Actor400500GrayStalkerWork* work;
    TaskFuncTable5               sp;
    _Actor400500GrayStalkerWork* work2;
    s32                          i;

    work = (_Actor400500GrayStalkerWork*)arg0->work;
    sp   = D_actor_400500_80131FEC;
    sp.funcs[(s16)work->subState](arg0);
    work2 = (_Actor400500GrayStalkerWork*)arg0->work;
    if (work2->animRequest == ACTOR_400500_ANIM_REQUEST_BLEND) {
        if (work2->appliedAnim != work2->animId) {
            work2->animFrames = 0;
        } else {
            work2->animFrames = _actor400500RescaleAnimFrames(arg0, work2->animFrames);
        }
        _actor400500BlendAnimSlots(arg0);
        work2->animRequest = ACTOR_400500_ANIM_REQUEST_PLAYING;
    } else if (work2->animRequest == ACTOR_400500_ANIM_REQUEST_RESET) {
        _actor400500ResetAnimSlots(arg0);
        work2->animRequest = ACTOR_400500_ANIM_REQUEST_PLAYING;
        work2->animFrames  = 0;
    } else if (work2->animRequest == ACTOR_400500_ANIM_REQUEST_PLAYING) {
        work2->animFrames = (u16)work2->animFrames + 1;
    }
    i = 1;
    do {
        work2->rig.slots[i].rate = work2->animRate;
        animationTickSlot(&work2->rig.anim, i);
        i++;
    } while (i < ARRAY_SIZE(work2->rig.slots));
}

static s32 func_actor_400500_8013B720(GfxCoord* arg0, MATRIX* arg1)
{
    MATRIX    matrix;
    MATRIX    parent;
    MATRIX    normal;
    MATRIX    transposed;
    GfxCoord* coord;
    GfxCoord* view;
    MATRIX*   parentp;

    coord = arg0->parent;
    if (coord == &gGfxViewCoord) {
        return 0;
    }
    view    = &gGfxViewCoord;
    parentp = &parent;
    matrix  = coord->coord;
    while (1) {
        coord = coord->parent;
        if (coord == NULL) {
            return 0;
        }
        if (coord == view) {
            break;
        }
        parent = coord->coord;
        MatrixNormal(parentp, parentp);
        gte_SetRotMatrix(parentp);
        MulRotMatrix(&matrix);
        MatrixNormal(&matrix, &normal);
        matrix = normal;
    }
    gte_TransposeMatrix(&matrix, &transposed);
    gte_SetRotMatrix(&transposed);
    MulRotMatrix(arg1);
    return 1;
}

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

static void func_actor_400500_8013BAA4(Task* arg0)
{
    _Actor400500GrayStalkerWork* work;
    _Actor400500GrayStalkerWork* work2;

    work = (_Actor400500GrayStalkerWork*)arg0->work;
    if (work->lastHitReaction == ACTOR_400500_HIT_REACTION_BLAST) {
        work->deathHeld = 0;
        return;
    }
    if (!(work->posture & ACTOR_400500_POSTURE_ON_FLOOR)) {
        work->deathHeld    = 1;
        work2              = (_Actor400500GrayStalkerWork*)arg0->work;
        work2->animRate    = ANIMATION_RATE_ONE;
        work2->animId      = 1;
        work2->animRequest = ACTOR_400500_ANIM_REQUEST_RESET;
        work->stateFrames  = 0;
        work->playerCaught = 0;
        work->moveAccel    = 0;
        work->moveSpeed    = 0;
        work->deathStep    = work->deathStep + 1;
        return;
    }
    work->deathHeld = 0;
}

static void func_actor_400500_8013BB18(Task* arg0)
{
    _Actor400500GrayStalkerWork* work;
    GfxCoord*                    coord;

    work               = (_Actor400500GrayStalkerWork*)arg0->work;
    coord              = arg0->extra.tmd->coords;
    work->moveAccel   += 2;
    work->moveSpeed   += work->moveAccel;
    coord->coord.t[1] += work->moveSpeed;
    if (coord->coord.t[1] >= -0x897) {
        coord->coord.t[1] = -0x898;
        work->deathStep++;
        _actor400500SetAnim(arg0, 0x13, ANIMATION_RATE_ONE);
        work->roll       += 0x800;
        coord->coord.t[1] = -0x3E8;
        work->stateFrames = 0;
    }
}

static void func_actor_400500_8013BBB0(Task* arg0)
{
    _Actor400500GrayStalkerWork* work;
    _Actor400500GrayStalkerWork* hit;
    s32                          soundId;
    s32                          pan;
    s32                          cond;

    work = (_Actor400500GrayStalkerWork*)arg0->work;
    if ((s16)work->stateFrames == 0) {
        soundId = ((((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x40050006;
        pan     = (s8)worldCoordGetOriginAudioPan(arg0->extra.tmd->coords);
        sndEvtRequestScriptStart(soundId, pan, (s8)worldCoordGetOriginAudioDepth(arg0->extra.tmd->coords));
        work->stateFrames = work->stateFrames + 1;
    }
    hit = (_Actor400500GrayStalkerWork*)arg0->work;
    if ((hit->rig.slots[1].status.fields.flags & ANIMATION_SLOT_REACHED_BOUNDARY) ||
        (hit->rig.slots[1].status.word & (ANIMATION_SLOT_FOLLOWED_JUMP | ANIMATION_SLOT_SETTLED))) {
        cond = 1;
    } else {
        cond = 0;
    }
    if (cond) {
        work->deathHeld = 0;
    }
}

static void func_actor_400500_8013BC9C(Task* arg0)
{
    _Actor400500GrayStalkerWork* work = (_Actor400500GrayStalkerWork*)arg0->work;

    work->deathHeld    = 1;
    work->stateFrames  = 0;
    work->playerCaught = 0;
    work->moveAccel    = 0;
    work->moveSpeed    = 0;
    work->deathStep    = work->deathStep + 1;
}

static void func_actor_400500_8013BCCC(Task* arg0)
{
    _Actor400500GrayStalkerWork* work;
    GfxCoord*                    coord;

    work               = (_Actor400500GrayStalkerWork*)arg0->work;
    coord              = arg0->extra.tmd->coords;
    work->moveAccel   += 2;
    work->moveSpeed   += work->moveAccel;
    coord->coord.t[1] += work->moveSpeed;
    if (coord->coord.t[1] >= -0x897) {
        coord->coord.t[1] = -0x898;
        work->deathStep++;
        _actor400500SetAnim(arg0, 0x13, ANIMATION_RATE_ONE);
        work->roll       += 0x800;
        coord->coord.t[1] = -0x3E8;
        work->stateFrames = 0;
    }
}

static void func_actor_400500_8013BD64(Task* arg0)
{
    _Actor400500GrayStalkerWork* work;
    _Actor400500GrayStalkerWork* hit;
    s32                          soundId;
    s32                          pan;
    s32                          cond;

    work = (_Actor400500GrayStalkerWork*)arg0->work;
    if ((s16)work->stateFrames == 0) {
        soundId = ((((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x40050006;
        pan     = (s8)worldCoordGetOriginAudioPan(arg0->extra.tmd->coords);
        sndEvtRequestScriptStart(soundId, pan, (s8)worldCoordGetOriginAudioDepth(arg0->extra.tmd->coords));
        work->stateFrames = work->stateFrames + 1;
    }
    hit = (_Actor400500GrayStalkerWork*)arg0->work;
    if ((hit->rig.slots[1].status.fields.flags & ANIMATION_SLOT_REACHED_BOUNDARY) ||
        (hit->rig.slots[1].status.word & (ANIMATION_SLOT_FOLLOWED_JUMP | ANIMATION_SLOT_SETTLED))) {
        cond = 1;
    } else {
        cond = 0;
    }
    if (cond) {
        work->deathHeld = 0;
    }
}

static void func_actor_400500_8013BE50(Task* arg0)
{
    _Actor400500GrayStalkerWork* work;
    _Actor400500GrayStalkerWork* work2;
    _Actor400500GrayStalkerWork* work3;
    s32                          flag;

    work           = (_Actor400500GrayStalkerWork*)arg0->work;
    work->animRate = ANIMATION_RATE_ONE;
    work2          = (_Actor400500GrayStalkerWork*)arg0->work;
    if ((work2->cloakRequest >= 0) || (((u8)work2->cloakRequest & ACTOR_400500_CLOAK_KIND_MASK) != ACTOR_400500_CLOAK_SHOW)) {
        flag                = ACTOR_400500_CLOAK_RUNNING | ACTOR_400500_CLOAK_SHOW;
        work2->cloakRequest = flag;
        work2->cloakPhase   = 0;
    }
    work3               = (_Actor400500GrayStalkerWork*)arg0->work;
    work3->animRate     = ANIMATION_RATE_ONE;
    work3->animId       = 1;
    work3->animRequest  = ACTOR_400500_ANIM_REQUEST_RESET;
    work->stateFrames   = 0;
    work->playerCaught  = 0;
    work->armSwingAngle = 0;
    work->subState      = work->subState + 1;
}

static void func_actor_400500_8013BEC4(Task* arg0)
{
    _Actor400500GrayStalkerWork* work;
    Task*                        child;
    _Actor400500GrayStalkerWork* work2;
    s32                          angle;

    work                    = (_Actor400500GrayStalkerWork*)arg0->work;
    work->armSwingAngle     = work->armSwingAngle + 0x80;
    work->stateFrames       = work->stateFrames + 1;
    child                   = ((_Actor400500GrayStalkerWork*)arg0->work)->armTasks[1];
    angle                   = work->armSwingAngle;
    child->extra.tmd->flags = 0;
    _actor400500SetCoordYaw(child->extra.tmd->coords, angle);
    if ((s16)work->armSwingAngle >= 0x200) {
        work2              = (_Actor400500GrayStalkerWork*)arg0->work;
        work2->animRate    = ANIMATION_RATE_ONE;
        work2->animId      = 8;
        work2->animRequest = ACTOR_400500_ANIM_REQUEST_RESET;
        work->playerCaught = 0;
        work->stateFrames  = 0;
        work->subState     = work->subState + 1;
    }
}

static void func_actor_400500_8013BFB0(Task* arg0)
{
    _Actor400500GrayStalkerWork* work;
    _Actor400500GrayStalkerWork* work2;
    s32                          flag;

    work = (_Actor400500GrayStalkerWork*)arg0->work;
    if ((work->cloakRequest >= 0) || (((u8)work->cloakRequest & ACTOR_400500_CLOAK_KIND_MASK) != ACTOR_400500_CLOAK_SHOW)) {
        flag               = ACTOR_400500_CLOAK_RUNNING | ACTOR_400500_CLOAK_SHOW;
        work->cloakRequest = flag;
        work->cloakPhase   = 0;
    }
    work2               = (_Actor400500GrayStalkerWork*)arg0->work;
    work2->animRate     = ANIMATION_RATE_ONE;
    work2->animId       = 1;
    work2->animRequest  = ACTOR_400500_ANIM_REQUEST_RESET;
    work->stateFrames   = 0;
    work->playerCaught  = 0;
    work->armSwingAngle = 0;
    work->subState      = work->subState + 1;
}

static void func_actor_400500_8013C018(Task* arg0)
{
    _Actor400500GrayStalkerWork* work;
    Task*                        child;
    _Actor400500GrayStalkerWork* work2;
    s32                          angle;

    work                    = (_Actor400500GrayStalkerWork*)arg0->work;
    work->armSwingAngle     = work->armSwingAngle + 0x80;
    work->stateFrames       = work->stateFrames + 1;
    child                   = ((_Actor400500GrayStalkerWork*)arg0->work)->armTasks[0];
    angle                   = work->armSwingAngle;
    child->extra.tmd->flags = 0;
    _actor400500SetCoordYaw(child->extra.tmd->coords, -angle);
    if ((s16)work->armSwingAngle >= 0x200) {
        work2              = (_Actor400500GrayStalkerWork*)arg0->work;
        work2->animRate    = ANIMATION_RATE_ONE;
        work2->animId      = 7;
        work2->animRequest = ACTOR_400500_ANIM_REQUEST_RESET;
        work->playerCaught = 0;
        work->stateFrames  = 0;
        work->subState     = work->subState + 1;
    }
}

static void func_actor_400500_8013C108(Task* arg0)
{
    _Actor400500GrayStalkerWork* work;
    _Actor400500GrayStalkerWork* work2;
    s32                          flag;

    work = (_Actor400500GrayStalkerWork*)arg0->work;
    if ((work->cloakRequest >= 0) || (((u8)work->cloakRequest & ACTOR_400500_CLOAK_KIND_MASK) != ACTOR_400500_CLOAK_SHOW)) {
        flag               = ACTOR_400500_CLOAK_RUNNING | ACTOR_400500_CLOAK_SHOW;
        work->cloakRequest = flag;
        work->cloakPhase   = 0;
    }
    work2              = (_Actor400500GrayStalkerWork*)arg0->work;
    work2->animRate    = ANIMATION_RATE_ONE;
    work2->animId      = 1;
    work2->animRequest = ACTOR_400500_ANIM_REQUEST_RESET;
    work->stateFrames  = 0;
    work->playerCaught = 0;
    work->moveAccel    = 0;
    work->moveSpeed    = 0;
    work->subState     = work->subState + 1;
}

static void func_actor_400500_8013C174(Task* arg0)
{
    _Actor400500GrayStalkerWork* work;
    GfxCoord*                    coord;

    work               = (_Actor400500GrayStalkerWork*)arg0->work;
    coord              = arg0->extra.tmd->coords;
    work->moveAccel   += 2;
    work->moveSpeed   += work->moveAccel;
    coord->coord.t[1] += work->moveSpeed;
    if (coord->coord.t[1] >= -0x897) {
        coord->coord.t[1] = -0x898;
        work->subState++;
        _actor400500SetAnim(arg0, 0x13, ANIMATION_RATE_ONE);
        work->roll       += 0x800;
        coord->coord.t[1] = -0x3E8;
        work->stateFrames = 0;
        work->posture    |= ACTOR_400500_POSTURE_ON_BACK;
    }
}

static void func_actor_400500_8013C218(Task* arg0)
{
    _Actor400500GrayStalkerWork* work;
    _Actor400500GrayStalkerWork* hit;
    _Actor400500GrayStalkerWork* work2;
    Enemy*                       enemy;
    s32                          soundId;
    s32                          pan;
    s32                          cond;

    work  = (_Actor400500GrayStalkerWork*)arg0->work;
    enemy = (Enemy*)arg0->spawnArg2.pointer;
    if ((s16)work->stateFrames == 0) {
        soundId = ((enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x40050006;
        pan     = (s8)worldCoordGetOriginAudioPan(arg0->extra.tmd->coords);
        sndEvtRequestScriptStart(soundId, pan, (s8)worldCoordGetOriginAudioDepth(arg0->extra.tmd->coords));
        work->stateFrames = work->stateFrames + 1;
    }
    hit = (_Actor400500GrayStalkerWork*)arg0->work;
    if ((hit->rig.slots[1].status.fields.flags & ANIMATION_SLOT_REACHED_BOUNDARY) ||
        (hit->rig.slots[1].status.word & (ANIMATION_SLOT_FOLLOWED_JUMP | ANIMATION_SLOT_SETTLED))) {
        cond = 1;
    } else {
        cond = 0;
    }
    if (cond) {
        if (enemy->hp > 0) {
            work2              = (_Actor400500GrayStalkerWork*)arg0->work;
            work2->animRate    = ANIMATION_RATE_ONE;
            work2->animId      = 0x14;
            work2->animRequest = ACTOR_400500_ANIM_REQUEST_RESET;
            work->subState     = work->subState + 1;
        } else {
            work->deathHeld = 0;
        }
    }
}

static void func_actor_400500_8013C348(Task* arg0)
{
    _Actor400500GrayStalkerWork* work;
    _Actor400500GrayStalkerWork* work2;
    Enemy*                       enemy;
    s32                          cond;

    work  = (_Actor400500GrayStalkerWork*)arg0->work;
    enemy = (Enemy*)arg0->spawnArg2.pointer;
    if ((work->rig.slots[1].status.fields.flags & ANIMATION_SLOT_REACHED_BOUNDARY) ||
        (work->rig.slots[1].status.word & (ANIMATION_SLOT_FOLLOWED_JUMP | ANIMATION_SLOT_SETTLED))) {
        cond = 1;
    } else {
        cond = 0;
    }
    if (cond) {
        if (enemy->hp > 0) {
            work2           = (_Actor400500GrayStalkerWork*)arg0->work;
            work2->state    = ACTOR_400500_STATE_CRAWL_FALLEN;
            work2->subState = 0;
            work->posture   = work->posture | ACTOR_400500_POSTURE_ON_FLOOR;
            return;
        }
        work->deathHeld = 0;
    }
}

static void func_actor_400500_8013C3C4(Task* arg0)
{
    Enemy*                       enemy;
    _Actor400500GrayStalkerWork* work;
    _Actor400500GrayStalkerWork* work2;
    _Actor400500GrayStalkerWork* work3;
    s32                          cond;

    enemy = (Enemy*)arg0->spawnArg2.pointer;
    work  = (_Actor400500GrayStalkerWork*)arg0->work;
    if (enemy->hp > 0) {
        if ((work->rig.slots[1].status.fields.flags & ANIMATION_SLOT_REACHED_BOUNDARY) ||
            (work->rig.slots[1].status.word & (ANIMATION_SLOT_FOLLOWED_JUMP | ANIMATION_SLOT_SETTLED))) {
            cond = 1;
        } else {
            cond = 0;
        }
        if (cond) {
            if (!(work->posture & ACTOR_400500_POSTURE_ON_BACK)) {
                work2              = (_Actor400500GrayStalkerWork*)arg0->work;
                work2->animRate    = ANIMATION_RATE_ONE;
                work2->animId      = 0x10;
                work2->animRequest = ACTOR_400500_ANIM_REQUEST_RESET;
            } else {
                work3              = (_Actor400500GrayStalkerWork*)arg0->work;
                work3->animRate    = ANIMATION_RATE_ONE;
                work3->animId      = 0x12;
                work3->animRequest = ACTOR_400500_ANIM_REQUEST_RESET;
            }
            work->subState = 2;
        }
    } else {
        work->deathHeld = 0;
    }
}

static void func_actor_400500_8013C474(Task* arg0)
{
    Enemy*                       enemy;
    _Actor400500GrayStalkerWork* work;
    _Actor400500GrayStalkerWork* work2;
    _Actor400500GrayStalkerWork* work3;
    s32                          cond;

    enemy = (Enemy*)arg0->spawnArg2.pointer;
    work  = (_Actor400500GrayStalkerWork*)arg0->work;
    if (enemy->hp > 0) {
        if ((work->rig.slots[1].status.fields.flags & ANIMATION_SLOT_REACHED_BOUNDARY) ||
            (work->rig.slots[1].status.word & (ANIMATION_SLOT_FOLLOWED_JUMP | ANIMATION_SLOT_SETTLED))) {
            cond = 1;
        } else {
            cond = 0;
        }
        if (cond) {
            work->knockdownPending = 0;
            if (!(work->posture & ACTOR_400500_POSTURE_ON_BACK)) {
                work2           = (_Actor400500GrayStalkerWork*)arg0->work;
                work2->state    = ACTOR_400500_STATE_CRAWL;
                work2->subState = 0;
            } else {
                work3           = (_Actor400500GrayStalkerWork*)arg0->work;
                work3->state    = ACTOR_400500_STATE_CRAWL_FALLEN;
                work3->subState = 0;
            }
        }
    } else {
        work->deathHeld = 0;
    }
}

static void func_actor_400500_8013C508(Task* arg0)
{
    _Actor400500GrayStalkerWork* hit;
    _Actor400500GrayStalkerWork* work;
    Enemy*                       enemy;
    s32                          cond;

    hit   = (_Actor400500GrayStalkerWork*)arg0->work;
    enemy = (Enemy*)arg0->spawnArg2.pointer;
    if ((hit->rig.slots[1].status.fields.flags & ANIMATION_SLOT_REACHED_BOUNDARY) ||
        (hit->rig.slots[1].status.word & (ANIMATION_SLOT_FOLLOWED_JUMP | ANIMATION_SLOT_SETTLED))) {
        cond = 1;
    } else {
        cond = 0;
    }
    if (cond || (enemy->hp <= 0)) {
        work               = hit;
        work->stateFrames  = 0;
        work->playerCaught = 0;
        work->moveAccel    = 0;
        work->moveSpeed    = 0;
        work->subState     = work->subState + 1;
    }
}

static void func_actor_400500_8013C578(Task* arg0)
{
    _Actor400500GrayStalkerWork* work;
    GfxCoord*                    coord;

    work               = (_Actor400500GrayStalkerWork*)arg0->work;
    coord              = arg0->extra.tmd->coords;
    work->moveAccel   += 2;
    work->moveSpeed   += work->moveAccel;
    coord->coord.t[1] += work->moveSpeed;
    if (coord->coord.t[1] >= -0x897) {
        coord->coord.t[1] = -0x898;
        work->subState++;
        _actor400500SetAnim(arg0, 0x13, ANIMATION_RATE_ONE);
        work->roll       += 0x800;
        coord->coord.t[1] = -0x3E8;
        work->stateFrames = 0;
        work->posture    |= ACTOR_400500_POSTURE_ON_FLOOR;
    }
}

static void func_actor_400500_8013C61C(Task* arg0)
{
    _Actor400500GrayStalkerWork* work;
    _Actor400500GrayStalkerWork* hit;
    _Actor400500GrayStalkerWork* work2;
    Enemy*                       enemy;
    s32                          soundId;
    s32                          pan;
    s32                          cond;

    work  = (_Actor400500GrayStalkerWork*)arg0->work;
    enemy = (Enemy*)arg0->spawnArg2.pointer;
    if ((s16)work->stateFrames == 0) {
        soundId = ((enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x40050006;
        pan     = (s8)worldCoordGetOriginAudioPan(arg0->extra.tmd->coords);
        sndEvtRequestScriptStart(soundId, pan, (s8)worldCoordGetOriginAudioDepth(arg0->extra.tmd->coords));
        work->stateFrames = work->stateFrames + 1;
    }
    hit = (_Actor400500GrayStalkerWork*)arg0->work;
    if ((hit->rig.slots[1].status.fields.flags & ANIMATION_SLOT_REACHED_BOUNDARY) ||
        (hit->rig.slots[1].status.word & (ANIMATION_SLOT_FOLLOWED_JUMP | ANIMATION_SLOT_SETTLED))) {
        cond = 1;
    } else {
        cond = 0;
    }
    if (cond) {
        work->knockdownPending = 0;
        if (enemy->hp > 0) {
            work2              = (_Actor400500GrayStalkerWork*)arg0->work;
            work2->animRate    = ANIMATION_RATE_ONE;
            work2->animId      = 0x14;
            work2->animRequest = ACTOR_400500_ANIM_REQUEST_RESET;
            work->subState     = work->subState + 1;
        } else {
            work->deathHeld = 0;
        }
    }
}

static void func_actor_400500_8013C750(Task* arg0)
{
    _Actor400500GrayStalkerWork* work;
    _Actor400500GrayStalkerWork* work2;
    s32                          cond;

    work = (_Actor400500GrayStalkerWork*)arg0->work;
    if ((work->rig.slots[1].status.fields.flags & ANIMATION_SLOT_REACHED_BOUNDARY) ||
        (work->rig.slots[1].status.word & (ANIMATION_SLOT_FOLLOWED_JUMP | ANIMATION_SLOT_SETTLED))) {
        cond = 1;
    } else {
        cond = 0;
    }
    if (cond) {
        work2           = (_Actor400500GrayStalkerWork*)arg0->work;
        work2->state    = ACTOR_400500_STATE_CRAWL_FALLEN;
        work2->subState = 0;
    }
}

static void func_actor_400500_8013C7A4(Task* arg0)
{
    _Actor400500GrayStalkerWork* work;
    _Actor400500GrayStalkerWork* work2;
    _Actor400500GrayStalkerWork* work3;
    s32                          flag;

    work           = (_Actor400500GrayStalkerWork*)arg0->work;
    work->animRate = ANIMATION_RATE_ONE;
    work2          = (_Actor400500GrayStalkerWork*)arg0->work;
    if (((work2->cloakRequest >= 0) || ((u8)work2->cloakRequest & ACTOR_400500_CLOAK_KIND_MASK)) && (work2->hideCooldown == 0)) {
        flag                = ACTOR_400500_CLOAK_RUNNING | ACTOR_400500_CLOAK_HIDE;
        work2->cloakRequest = flag;
        work2->cloakPhase   = 0;
    }
    work3              = (_Actor400500GrayStalkerWork*)arg0->work;
    work3->animRate    = ANIMATION_RATE_ONE;
    work3->animId      = 1;
    work3->animRequest = ACTOR_400500_ANIM_REQUEST_RESET;
    work->subState     = work->subState + 1;
}

static void func_actor_400500_8013C818(Task* task)
{
    _Actor400500GrayStalkerWork* work;
    _Actor400500GrayStalkerWork* work2;
    GfxCoord*                    coord;
    s32                          soundId;
    s32                          pan;

    coord   = task->extra.tmd->coords;
    soundId = ((((Enemy*)task->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x40050004;
    work    = (_Actor400500GrayStalkerWork*)task->work;
    pan     = (s8)worldCoordGetOriginAudioPan(coord);
    sndEvtRequestScriptStart(soundId, pan, (s8)worldCoordGetOriginAudioDepth(task->extra.tmd->coords));
    work2              = (_Actor400500GrayStalkerWork*)task->work;
    work2->animRate    = ANIMATION_RATE_ONE;
    work2->animId      = 0x20;
    work2->animRequest = ACTOR_400500_ANIM_REQUEST_RESET;
    work->stateFrames  = 0;
    work->playerCaught = 0;
    work->moveAccel    = 0x80;
    work->moveSpeed    = 0;
    work->stateFrames  = 0;
    work->subState     = work->subState + 1;
    work->dropStartX   = coord->coord.t[0];
    work->dropStartZ   = coord->coord.t[2];
}

static void func_actor_400500_8013C908(Task* arg0)
{
    _Actor400500GrayStalkerWork* work;
    _Actor400500GrayStalkerWork* work2;
    s32                          soundId;
    s32                          pan;

    work    = (_Actor400500GrayStalkerWork*)arg0->work;
    soundId = ((((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x40050004;
    pan     = (s8)worldCoordGetOriginAudioPan(arg0->extra.tmd->coords);
    sndEvtRequestScriptStart(soundId, pan, (s8)worldCoordGetOriginAudioDepth(arg0->extra.tmd->coords));
    work2              = (_Actor400500GrayStalkerWork*)arg0->work;
    work2->animRate    = ANIMATION_RATE_ONE;
    work2->animId      = 0x15;
    work2->animRequest = ACTOR_400500_ANIM_REQUEST_RESET;
    work->stateFrames  = 0;
    work->playerCaught = 0;
    work->moveAccel    = 0;
    work->moveSpeed    = 0x12C;
    work->pitch        = 0;
    work->subState     = work->subState + 1;
}

static void func_actor_400500_8013C9D4(Task* arg0)
{
    _Actor400500GrayStalkerWork* work;
    _Actor400500GrayStalkerWork* work2;
    s32                          cond;

    work = (_Actor400500GrayStalkerWork*)arg0->work;
    if ((work->rig.slots[1].status.fields.flags & ANIMATION_SLOT_REACHED_BOUNDARY) ||
        (work->rig.slots[1].status.word & (ANIMATION_SLOT_FOLLOWED_JUMP | ANIMATION_SLOT_SETTLED))) {
        cond = 1;
    } else {
        cond = 0;
    }
    if (cond) {
        work2           = (_Actor400500GrayStalkerWork*)arg0->work;
        work2->state    = ACTOR_400500_STATE_CRAWL;
        work2->subState = 0;
        work->posture  &= ~ACTOR_400500_POSTURE_ON_FLOOR;
    }
}

static void func_actor_400500_8013CA38(Task* arg0)
{
    MATRIX                       local;
    _Actor400500GrayStalkerWork* work;
    _Actor400500GrayStalkerWork* work2;
    GfxCoord*                    coords;
    SVECTOR3*                    pos;
    SVECTOR3*                    pos2;
    s32                          heading;
    s32                          masked;
    s32                          neg;

    work               = (_Actor400500GrayStalkerWork*)arg0->work;
    heading            = (u16)work->yaw;
    work->stateFrames  = 0;
    work2              = (_Actor400500GrayStalkerWork*)arg0->work;
    work2->animRate    = ANIMATION_RATE_ONE;
    work2->animId      = 0x16;
    work2->animRequest = ACTOR_400500_ANIM_REQUEST_RESET;
    masked             = heading & 0xFFF;
    if ((work->zone == 1) && ((masked == 0x400) || (masked == 0xC00))) {
        neg               = -1;
        work->stateFrames = neg;
        pos2              = &work->anchorPos;
        coords            = arg0->extra.tmd->coords;
        actorRenderComposeCoord(&coords[0xE]);
        gfxMakeRelativeTransform(&gGfxViewCoord.workm, &coords[0xE].workm, &local);
        pos                      = pos2;
        pos->vx                  = local.t[0];
        pos->vz                  = local.t[2];
        coords[0xE].composeStamp = GRAPHICS_COORD_DIRTY;
    }
    work->subState = work->subState + 1;
}

static void func_actor_400500_8013CB0C(Task* arg0)
{
    _Actor400500GrayStalkerWork* work;
    _Actor400500GrayStalkerWork* work2;
    _Actor400500GrayStalkerWork* work3;
    s32                          flag;

    work = (_Actor400500GrayStalkerWork*)arg0->work;
    if (work->knockdownPending != 0) {
        work->knockdownPending = 0;
        _actor400500EnterState(arg0, ACTOR_400500_STATE_KNOCKDOWN);
        flag = 1;
    } else {
        flag = 0;
    }
    if ((flag == 0) && ((_actor400500TryTurnOverNearTarget(arg0) << 0x10) == 0) &&
        ((func_actor_400500_80133460(arg0) << 0x10) == 0)) {
        if (((u16)work->yaw & 0xFFF) == 0xC00) {
            work2              = (_Actor400500GrayStalkerWork*)arg0->work;
            work2->animRate    = ANIMATION_RATE_ONE;
            work2->animId      = 4;
            work2->animRequest = ACTOR_400500_ANIM_REQUEST_RESET;
            work->subState     = 3;
            return;
        }
        work3           = (_Actor400500GrayStalkerWork*)arg0->work;
        work3->state    = ACTOR_400500_STATE_TURN_OVER;
        work3->subState = 0;
    }
}

static void func_actor_400500_8013CBD8(Task* arg0)
{
    _Actor400500GrayStalkerWork* work;
    _Actor400500GrayStalkerWork* work2;
    _Actor400500GrayStalkerWork* work3;
    GfxCoord*                    coord;
    s32                          flag;
    s32                          zone;

    work  = (_Actor400500GrayStalkerWork*)arg0->work;
    coord = arg0->extra.tmd->coords;
    if (work->knockdownPending != 0) {
        work->knockdownPending = 0;
        _actor400500EnterState(arg0, ACTOR_400500_STATE_KNOCKDOWN);
        flag = 1;
    } else {
        flag = 0;
    }
    if ((flag == 0) && ((_actor400500TryTurnOverNearTarget(arg0) << 0x10) == 0) &&
        ((func_actor_400500_80133460(arg0) << 0x10) == 0)) {
        if (((u16)work->yaw & 0xFFF) != 0xC00) {
            work2           = (_Actor400500GrayStalkerWork*)arg0->work;
            work2->state    = ACTOR_400500_STATE_TURN_OVER;
            work2->subState = 0;
        } else {
            zone = work->zone;
            if (zone != 1) {
                if (zone == 4) {
                    work->subState = zone;
                }
            } else if (work->toTarget.vx >= 0xFA0) {
                work3           = (_Actor400500GrayStalkerWork*)arg0->work;
                work3->state    = ACTOR_400500_STATE_TURN_OVER;
                work3->subState = 0;
            }
            func_actor_400500_8013403C(arg0);
        }
        coord->coord.t[2] = -0x209E;
    }
}

static void func_actor_400500_8013CCDC(Task* arg0)
{
    _Actor400500GrayStalkerWork* work;
    _Actor400500GrayStalkerWork* work2;
    _Actor400500GrayStalkerWork* work3;
    s32                          flag;

    work = (_Actor400500GrayStalkerWork*)arg0->work;
    if (work->knockdownPending != 0) {
        work->knockdownPending = 0;
        _actor400500EnterState(arg0, ACTOR_400500_STATE_KNOCKDOWN);
        flag = 1;
    } else {
        flag = 0;
    }
    if ((flag == 0) && ((_actor400500TryTurnOverNearTarget(arg0) << 0x10) == 0) &&
        ((func_actor_400500_80133460(arg0) << 0x10) == 0)) {
        if (((u16)work->yaw & 0xFFF) == 0x400) {
            work2              = (_Actor400500GrayStalkerWork*)arg0->work;
            work2->animRate    = ANIMATION_RATE_ONE;
            work2->animId      = 4;
            work2->animRequest = ACTOR_400500_ANIM_REQUEST_RESET;
            work->subState     = 1;
            return;
        }
        work3           = (_Actor400500GrayStalkerWork*)arg0->work;
        work3->state    = ACTOR_400500_STATE_TURN_OVER;
        work3->subState = 0;
    }
}

static void func_actor_400500_8013CDA8(Task* arg0)
{
    _Actor400500GrayStalkerWork* work;
    _Actor400500GrayStalkerWork* work2;
    GfxCoord*                    coord;
    s32                          flag;

    work  = (_Actor400500GrayStalkerWork*)arg0->work;
    coord = arg0->extra.tmd->coords;
    if (work->knockdownPending != 0) {
        work->knockdownPending = 0;
        _actor400500EnterState(arg0, ACTOR_400500_STATE_KNOCKDOWN);
        flag = 1;
    } else {
        flag = 0;
    }
    if ((flag == 0) && ((_actor400500TryTurnOverNearTarget(arg0) << 0x10) == 0) &&
        ((func_actor_400500_80133460(arg0) << 0x10) == 0)) {
        if ((u16)work->yaw & 0xFFF) {
            work2           = (_Actor400500GrayStalkerWork*)arg0->work;
            work2->state    = ACTOR_400500_STATE_TURN_OVER;
            work2->subState = 0;
        } else {
            if (work->zone != 3) {
                if (work->zone == 6) {
                    work->subState = 7;
                }
            } else if (work->toTarget.vz > 0) {
                work->subState = 7;
            }
            func_actor_400500_8013403C(arg0);
        }
        coord->coord.t[0] = 0x4074;
    }
}

static void func_actor_400500_8013CE9C(Task* arg0)
{
    _Actor400500GrayStalkerWork* work;
    _Actor400500GrayStalkerWork* work2;
    _Actor400500GrayStalkerWork* work3;
    s32                          flag;

    work = (_Actor400500GrayStalkerWork*)arg0->work;
    if (work->knockdownPending != 0) {
        work->knockdownPending = 0;
        _actor400500EnterState(arg0, ACTOR_400500_STATE_KNOCKDOWN);
        flag = 1;
    } else {
        flag = 0;
    }
    if ((flag == 0) && ((_actor400500TryTurnOverNearTarget(arg0) << 0x10) == 0) &&
        ((func_actor_400500_80133460(arg0) << 0x10) == 0)) {
        if (((u16)work->yaw & 0xFFF) == 0x800) {
            work2              = (_Actor400500GrayStalkerWork*)arg0->work;
            work2->animRate    = ANIMATION_RATE_ONE;
            work2->animId      = 4;
            work2->animRequest = ACTOR_400500_ANIM_REQUEST_RESET;
            work->subState     = 8;
            return;
        }
        work3           = (_Actor400500GrayStalkerWork*)arg0->work;
        work3->state    = ACTOR_400500_STATE_TURN_OVER;
        work3->subState = 0;
    }
}

static void func_actor_400500_8013CF68(Task* arg0)
{
    _Actor400500GrayStalkerWork* work;
    _Actor400500GrayStalkerWork* work2;
    GfxCoord*                    coord;
    s32                          flag;

    work  = (_Actor400500GrayStalkerWork*)arg0->work;
    coord = arg0->extra.tmd->coords;
    if (work->knockdownPending != 0) {
        work->knockdownPending = 0;
        _actor400500EnterState(arg0, ACTOR_400500_STATE_KNOCKDOWN);
        flag = 1;
    } else {
        flag = 0;
    }
    if ((flag == 0) && ((_actor400500TryTurnOverNearTarget(arg0) << 0x10) == 0) &&
        ((func_actor_400500_80133460(arg0) << 0x10) == 0)) {
        if (((u16)work->yaw & 0xFFF) != 0x800) {
            work2           = (_Actor400500GrayStalkerWork*)arg0->work;
            work2->state    = ACTOR_400500_STATE_TURN_OVER;
            work2->subState = 0;
        } else {
            switch (work->zone) {
                case 2:
                    if (coord->coord.t[2] < -0x209E) {
                        work->subState = 9;
                    }
                    break;
                case 3:
                    if (work->toTarget.vz < 0) {
                        work->subState = 0xA;
                    }
                    break;
            }
            func_actor_400500_8013403C(arg0);
        }
        coord->coord.t[0] = 0x4074;
    }
}

static void func_actor_400500_8013D078(Task* arg0)
{
    _Actor400500GrayStalkerWork* work;
    _Actor400500GrayStalkerWork* work2;
    _Actor400500GrayStalkerWork* work3;
    s32                          flag;

    work = (_Actor400500GrayStalkerWork*)arg0->work;
    if (work->knockdownPending != 0) {
        work->knockdownPending = 0;
        _actor400500EnterState(arg0, ACTOR_400500_STATE_KNOCKDOWN);
        flag = 1;
    } else {
        flag = 0;
    }
    if ((flag == 0) && ((_actor400500TryTurnOverNearTarget(arg0) << 0x10) == 0) &&
        ((func_actor_400500_80133460(arg0) << 0x10) == 0)) {
        if (((u16)work->yaw & 0xFFF) == 0xC00) {
            work2              = (_Actor400500GrayStalkerWork*)arg0->work;
            work2->animRate    = ANIMATION_RATE_ONE;
            work2->animId      = 4;
            work2->animRequest = ACTOR_400500_ANIM_REQUEST_RESET;
            work->subState     = 3;
            return;
        }
        work3           = (_Actor400500GrayStalkerWork*)arg0->work;
        work3->state    = ACTOR_400500_STATE_TURN_OVER;
        work3->subState = 0;
    }
}

static void func_actor_400500_8013D144(Task* arg0)
{
    _Actor400500GrayStalkerWork* work;
    _Actor400500GrayStalkerWork* work2;
    _Actor400500GrayStalkerWork* work3;
    s32                          flag;

    work = (_Actor400500GrayStalkerWork*)arg0->work;
    if (work->knockdownPending != 0) {
        work->knockdownPending = 0;
        _actor400500EnterState(arg0, ACTOR_400500_STATE_KNOCKDOWN);
        flag = 1;
    } else {
        flag = 0;
    }
    if ((flag == 0) && ((_actor400500TryTurnOverNearTarget(arg0) << 0x10) == 0) &&
        ((func_actor_400500_80133460(arg0) << 0x10) == 0)) {
        if (((u16)work->yaw & 0xFFF) == 0) {
            work2              = (_Actor400500GrayStalkerWork*)arg0->work;
            work2->animRate    = ANIMATION_RATE_ONE;
            work2->animId      = 4;
            work2->animRequest = ACTOR_400500_ANIM_REQUEST_RESET;
            work->subState     = 6;
            return;
        }
        work3           = (_Actor400500GrayStalkerWork*)arg0->work;
        work3->state    = ACTOR_400500_STATE_TURN_OVER;
        work3->subState = 0;
    }
}

static void func_actor_400500_8013D210(Task* arg0)
{
    _Actor400500GrayStalkerWork* work;
    _Actor400500GrayStalkerWork* work2;
    GfxCoord*                    coord;

    work               = (_Actor400500GrayStalkerWork*)arg0->work;
    coord              = arg0->extra.tmd->coords;
    work->roll         = 0x800;
    work->yaw          = 0;
    coord->coord.t[0]  = 0x4074;
    coord->coord.t[1]  = -0xFA0;
    coord->coord.t[2]  = -0x2710;
    work->stateFrames  = 0;
    work2              = (_Actor400500GrayStalkerWork*)arg0->work;
    work2->animRate    = 4;
    work2->animId      = 1;
    work2->animRequest = ACTOR_400500_ANIM_REQUEST_RESET;
    work->subState     = work->subState + 1;
}

static void func_actor_400500_8013D274(Task* arg0)
{
    _Actor400500GrayStalkerWork* work;
    _Actor400500GrayStalkerWork* work2;
    s32                          flag;

    work = (_Actor400500GrayStalkerWork*)arg0->work;
    if (work->roomCommand == 2) {
        work->hideHoldFrames = 0x258;
        work2                = (_Actor400500GrayStalkerWork*)arg0->work;
        if ((work2->cloakRequest >= 0) || (((u8)work2->cloakRequest & ACTOR_400500_CLOAK_KIND_MASK) != ACTOR_400500_CLOAK_SHOW)) {
            flag                = ACTOR_400500_CLOAK_RUNNING | ACTOR_400500_CLOAK_SHOW;
            work2->cloakRequest = flag;
            work2->cloakPhase   = 0;
        }
        work->stateFrames = 0;
        work->subState    = work->subState + 1;
    }
}

static void func_actor_400500_8013D2D8(Task* arg0)
{
    _Actor400500GrayStalkerWork* work;
    _Actor400500GrayStalkerWork* work2;
    _Actor400500GrayStalkerWork* work3;
    GfxCoord*                    coord;
    s32                          flag;

    work  = (_Actor400500GrayStalkerWork*)arg0->work;
    coord = arg0->extra.tmd->coords;
    if ((s16)++work->stateFrames == 1) {
        work2 = (_Actor400500GrayStalkerWork*)arg0->work;
        if (((work2->cloakRequest >= 0) || ((u8)work2->cloakRequest & ACTOR_400500_CLOAK_KIND_MASK)) && (work2->hideCooldown == 0)) {
            flag                = ACTOR_400500_CLOAK_RUNNING | ACTOR_400500_CLOAK_HIDE;
            work2->cloakRequest = flag;
            work2->cloakPhase   = 0;
        }
    }
    func_actor_400500_80133B14(arg0);
    if (coord->coord.t[2] >= -0x225F) {
        work3                  = (_Actor400500GrayStalkerWork*)arg0->work;
        work3->animBlendFrames = 0xA;
        work3->animRate        = ANIMATION_RATE_ONE;
        work3->animId          = 1;
        work3->animRequest     = ACTOR_400500_ANIM_REQUEST_BLEND;
        work->subState         = work->subState + 1;
    }
}

static void func_actor_400500_8013D3B8(Task* arg0)
{
    _Actor400500GrayStalkerWork* work;
    _Actor400500GrayStalkerWork* work2;
    s32                          flag;

    work = (_Actor400500GrayStalkerWork*)arg0->work;
    if (work->roomCommand == 3) {
        work->stateFrames    = 0;
        work->hideHoldFrames = 0x10;
        work2                = (_Actor400500GrayStalkerWork*)arg0->work;
        if ((work2->cloakRequest >= 0) || (((u8)work2->cloakRequest & ACTOR_400500_CLOAK_KIND_MASK) != ACTOR_400500_CLOAK_SHOW)) {
            flag                = ACTOR_400500_CLOAK_RUNNING | ACTOR_400500_CLOAK_SHOW;
            work2->cloakRequest = flag;
            work2->cloakPhase   = 0;
        }
        work->subState = work->subState + 1;
    }
}

static void func_actor_400500_8013D420(Task* arg0)
{
    _Actor400500GrayStalkerWork* work;
    _Actor400500GrayStalkerWork* work2;
    s32                          soundId;
    s32                          pan;

    work = (_Actor400500GrayStalkerWork*)arg0->work;
    if ((s16)++work->stateFrames == 0x1E) {
        soundId = ((((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x40050004;
        pan     = (s8)worldCoordGetOriginAudioPan(arg0->extra.tmd->coords);
        sndEvtRequestScriptStart(soundId, pan, (s8)worldCoordGetOriginAudioDepth(arg0->extra.tmd->coords));
    }
    if (work->roomCommand == 4) {
        work->commandActive = 0;
        work2               = (_Actor400500GrayStalkerWork*)arg0->work;
        work2->state        = ACTOR_400500_STATE_CRAWL;
        work2->subState     = 0;
    }
}

static void func_actor_400500_8013D4F0(Task* arg0)
{
    Enemy*                       enemy;
    _Actor400500GrayStalkerWork* work;
    _Actor400500GrayStalkerWork* work2;
    _Actor400500GrayStalkerWork* work3;

    enemy = (Enemy*)arg0->spawnArg2.pointer;
    work  = (_Actor400500GrayStalkerWork*)arg0->work;
    if (enemy->hp > 0) {
        if (damageTickEnemyBuildup(enemy) != 0) {
            if (!(work->posture & ACTOR_400500_POSTURE_ON_BACK)) {
                work2              = (_Actor400500GrayStalkerWork*)arg0->work;
                work2->animRate    = ANIMATION_RATE_ONE;
                work2->animId      = 0x10;
                work2->animRequest = ACTOR_400500_ANIM_REQUEST_RESET;
            } else {
                work3              = (_Actor400500GrayStalkerWork*)arg0->work;
                work3->animRate    = ANIMATION_RATE_ONE;
                work3->animId      = 0x12;
                work3->animRequest = ACTOR_400500_ANIM_REQUEST_RESET;
            }
            work->subState = 2;
        }
    } else {
        work->deathHeld = 0;
    }
}

static void func_actor_400500_8013D59C(Task* arg0)
{
    Enemy*                       enemy;
    _Actor400500GrayStalkerWork* work;
    _Actor400500GrayStalkerWork* work2;
    _Actor400500GrayStalkerWork* work3;
    s32                          cond;

    enemy = (Enemy*)arg0->spawnArg2.pointer;
    work  = (_Actor400500GrayStalkerWork*)arg0->work;
    if (enemy->hp > 0) {
        if ((work->rig.slots[1].status.fields.flags & ANIMATION_SLOT_REACHED_BOUNDARY) ||
            (work->rig.slots[1].status.word & (ANIMATION_SLOT_FOLLOWED_JUMP | ANIMATION_SLOT_SETTLED))) {
            cond = 1;
        } else {
            cond = 0;
        }
        if (cond) {
            work->knockdownPending = 0;
            if (!(work->posture & ACTOR_400500_POSTURE_ON_BACK)) {
                work2           = (_Actor400500GrayStalkerWork*)arg0->work;
                work2->state    = ACTOR_400500_STATE_CRAWL;
                work2->subState = 0;
            } else {
                work3           = (_Actor400500GrayStalkerWork*)arg0->work;
                work3->state    = ACTOR_400500_STATE_CRAWL_FALLEN;
                work3->subState = 0;
            }
        }
    } else {
        work->deathHeld = 0;
    }
}

static void func_actor_400500_8013D630(Task* arg0)
{
    _Actor400500GrayStalkerWork* hit;
    _Actor400500GrayStalkerWork* work;
    Enemy*                       enemy;
    s32                          cond;

    hit   = (_Actor400500GrayStalkerWork*)arg0->work;
    enemy = (Enemy*)arg0->spawnArg2.pointer;
    if ((hit->rig.slots[1].status.fields.flags & ANIMATION_SLOT_REACHED_BOUNDARY) ||
        (hit->rig.slots[1].status.word & (ANIMATION_SLOT_FOLLOWED_JUMP | ANIMATION_SLOT_SETTLED))) {
        cond = 1;
    } else {
        cond = 0;
    }
    if (cond || (enemy->hp <= 0)) {
        work               = hit;
        work->stateFrames  = 0;
        work->playerCaught = 0;
        work->moveAccel    = 0;
        work->moveSpeed    = 0;
        work->subState     = work->subState + 1;
    }
}

static void func_actor_400500_8013D6A0(Task* arg0)
{
    _Actor400500GrayStalkerWork* work;
    GfxCoord*                    coord;

    work               = (_Actor400500GrayStalkerWork*)arg0->work;
    coord              = arg0->extra.tmd->coords;
    work->moveAccel   += 2;
    work->moveSpeed   += work->moveAccel;
    coord->coord.t[1] += work->moveSpeed;
    if (coord->coord.t[1] >= -0x897) {
        coord->coord.t[1] = -0x898;
        work->subState++;
        _actor400500SetAnim(arg0, 0x13, ANIMATION_RATE_ONE);
        work->roll       += 0x800;
        coord->coord.t[1] = -0x3E8;
        work->stateFrames = 0;
        work->posture    |= ACTOR_400500_POSTURE_ON_FLOOR;
    }
}

static void func_actor_400500_8013D744(Task* arg0)
{
    _Actor400500GrayStalkerWork* work;
    _Actor400500GrayStalkerWork* hit;
    _Actor400500GrayStalkerWork* work2;
    Enemy*                       enemy;
    s32                          soundId;
    s32                          pan;
    s32                          cond;

    work  = (_Actor400500GrayStalkerWork*)arg0->work;
    enemy = (Enemy*)arg0->spawnArg2.pointer;
    if ((s16)work->stateFrames == 0) {
        soundId = ((enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x40050006;
        pan     = (s8)worldCoordGetOriginAudioPan(arg0->extra.tmd->coords);
        sndEvtRequestScriptStart(soundId, pan, (s8)worldCoordGetOriginAudioDepth(arg0->extra.tmd->coords));
        work->stateFrames = work->stateFrames + 1;
    }
    hit = (_Actor400500GrayStalkerWork*)arg0->work;
    if ((hit->rig.slots[1].status.fields.flags & ANIMATION_SLOT_REACHED_BOUNDARY) ||
        (hit->rig.slots[1].status.word & (ANIMATION_SLOT_FOLLOWED_JUMP | ANIMATION_SLOT_SETTLED))) {
        cond = 1;
    } else {
        cond = 0;
    }
    if (cond) {
        work->knockdownPending = 0;
        if (enemy->hp > 0) {
            work2              = (_Actor400500GrayStalkerWork*)arg0->work;
            work2->animRate    = ANIMATION_RATE_ONE;
            work2->animId      = 0x14;
            work2->animRequest = ACTOR_400500_ANIM_REQUEST_RESET;
            work->subState     = work->subState + 1;
        } else {
            work->deathHeld = 0;
        }
    }
}

static void func_actor_400500_8013D878(Task* arg0)
{
    _Actor400500GrayStalkerWork* work;
    _Actor400500GrayStalkerWork* work2;
    s32                          cond;

    work = (_Actor400500GrayStalkerWork*)arg0->work;
    if ((work->rig.slots[1].status.fields.flags & ANIMATION_SLOT_REACHED_BOUNDARY) ||
        (work->rig.slots[1].status.word & (ANIMATION_SLOT_FOLLOWED_JUMP | ANIMATION_SLOT_SETTLED))) {
        cond = 1;
    } else {
        cond = 0;
    }
    if (cond) {
        work2           = (_Actor400500GrayStalkerWork*)arg0->work;
        work2->state    = ACTOR_400500_STATE_CRAWL_FALLEN;
        work2->subState = 0;
    }
}

static void func_actor_400500_8013D8CC(Task* arg0)
{
    _Actor400500GrayStalkerWork* work;
    TmdObject*                   model;
    GfxCoord*                    coord;

    model              = arg0->extra.tmd;
    work               = (_Actor400500GrayStalkerWork*)arg0->work;
    coord              = model->coords;
    work->shrinkScaleY = 0x1000;
    work->savedRootMtx = coord->coord;
    worldCoordSetActorColorMode(arg0->spawnArg2.pointer, ENEMY_COLOR_WEIGHTED);
    work->stateFrames = 0;
    work->state       = work->state + 1;
}

static void func_actor_400500_8013D958(Task* arg0)
{
    _Actor400500GrayStalkerWork* work;
    TmdObject*                   model;
    u16                          frame;

    work              = (_Actor400500GrayStalkerWork*)arg0->work;
    model             = arg0->extra.tmd;
    frame             = work->stateFrames + 1;
    work->stateFrames = frame;
    if ((s16)frame >= 0x18) {
        work->cloakLevel  = 0;
        work->colorBlend  = TMD_OBJECT_COLOR_BLEND_ONE;
        work->shadowShade = 0xFF;
        modelLightingSetLayerMaterials(work->cloakLevel);
        model->shading.colorBlend = work->colorBlend;
        work->stateFrames         = 0;
        work->state               = work->state + 1;
    }
}

static void func_actor_400500_8013D9DC(Task* arg0)
{
    _Actor400500GrayStalkerWork* work = (_Actor400500GrayStalkerWork*)arg0->work;

    arg0->state    = 3;
    work->state    = 0;
    work->subState = 0;
}

static void func_actor_400500_8013D9F4(Task* arg0)
{
    TmdObject*                   model;
    _Actor400500GrayStalkerWork* work;

    model             = arg0->extra.tmd;
    work              = (_Actor400500GrayStalkerWork*)arg0->work;
    model->flags     |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
    work->stateFrames = 0;
    work->shadowShade = 0;
    work->state       = work->state + 1;
}

static void func_actor_400500_8013DA24(Task* arg0)
{
    _Actor400500GrayStalkerWork* work;
    u16                          frame;

    work              = (_Actor400500GrayStalkerWork*)arg0->work;
    frame             = work->stateFrames + 1;
    work->stateFrames = frame;
    if ((s16)frame >= 2) {
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
    func_actor_400500_80134B88(arg0);
    work->state = work->state + 1;
}

static void func_actor_400500_8013DACC(Task* arg0)
{
    _Actor400500GrayStalkerWork* work = (_Actor400500GrayStalkerWork*)arg0->work;

    arg0->state    = 3;
    work->state    = 0;
    work->subState = 0;
}

s32 func_actor_400500_8013DAE4(Task* arg0, s32 arg1, u16* arg2, s32 arg3)
{
    _Actor400500GrayStalkerWork* work;
    s32                          kind;

    work = (_Actor400500GrayStalkerWork*)arg0->work;
    kind = arg2[1];
    switch (kind) {
        case 1:
            work->commandActive  = kind;
            work->hideHoldFrames = 0x1E;
            work->roomCommand    = kind;
            _actor400500EnterState(arg0, ACTOR_400500_STATE_ROOM_SEQUENCE);
            break;
        case 2:
            work->roomCommand = kind;
            break;
        case 3:
            work->roomCommand = kind;
            break;
        case 4:
            work->roomCommand = kind;
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

static void func_actor_400500_8013DBCC(Task* arg0, s16 arg1, SVECTOR3* arg2)
{
    MATRIX    local;
    GfxCoord* coord;

    coord = &arg0->extra.tmd->coords[arg1];
    actorRenderComposeCoord(coord);
    gfxMakeRelativeTransform(&gGfxViewCoord.workm, &coord->workm, &local);
    arg2->vx            = local.t[0];
    arg2->vz            = local.t[2];
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
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

static void func_actor_400500_8013DEFC(Task* arg0)
{
    _Actor400500GrayStalkerWork* work                = (_Actor400500GrayStalkerWork*)arg0->work;
    void                         (*states[2])(Task*) = {
        func_actor_400500_8013DF74,
        func_actor_400500_8013DFE4,
    };

    states[(s16)work->state](arg0);
}

static void func_actor_400500_8013DF50(Task* arg0)
{
    _Actor400500GrayStalkerWork* work = (_Actor400500GrayStalkerWork*)arg0->work;

    work->hitTaken    = 0;
    work->hitReaction = ACTOR_400500_HIT_REACTION_NONE;
}

void func_actor_400500_8013DF64(Task* task)
{
}

void func_actor_400500_8013DF6C(Task* task)
{
}

static void func_actor_400500_8013DF74(Task* arg0)
{
    _Actor400500GrayStalkerWork* work;
    Task*                        child;
    s32                          i;

    work = (_Actor400500GrayStalkerWork*)arg0->work;
    for (i = 0; i < 2; i++) {
        child = work->armTasks[i];
        if (child != NULL) {
            taskKill(child);
        }
    }
    work->stateFrames = 0;
    work->state       = work->state + 1;
}

static void func_actor_400500_8013DFE4(Task* arg0)
{
    _Actor400500GrayStalkerWork* work;
    u16                          frame;

    work              = (_Actor400500GrayStalkerWork*)arg0->work;
    frame             = work->stateFrames + 1;
    work->stateFrames = frame;
    if ((s16)frame >= 0x12D) {
        enemyDestroy(arg0->spawnArg2.pointer, arg0);
    }
}
