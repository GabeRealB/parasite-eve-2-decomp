#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/abs.h>
#include <psyq/gtemac.h>
#include <psyq/inline_c.h>
#include <psyq/libgs.h>
#include <psyq/memory.h>

#include "common.h"
#include "gte.h"

#include "actors/actor.h"

#include "gameplay/actor.h"
#include "gameplay/actor_render.h"
#include "gameplay/animation.h"
#include "gameplay/area.h"
#include "gameplay/area_entry.h"
#include "gameplay/areaplace.h"
#include "gameplay/collision.h"
#include "gameplay/damage.h"
#include "gameplay/display.h"
#include "gameplay/effect_tasks.h"
#include "gameplay/effects.h"
#include "gameplay/enemy.h"
#include "gameplay/geometry.h"
#include "gameplay/hud_sprites.h"
#include "gameplay/player_state.h"
#include "gameplay/light.h"
#include "gameplay/loading.h"
#include "gameplay/message.h"
#include "gameplay/enemy_params.h"
#include "gameplay/player_actor.h"
#include "gameplay/room.h"
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
#include "../../shared/fireball.h"
#include "../../shared/player_detection.h"
#include "../../shared/actor_messages.h"

/// One room the actor patrols in: an entry of the table the spawn handler
/// searches for the current location.
///
/// A placement's `mode` picks one of the row's two routes. The table ends at
/// the entry whose `routeRow` is 0.
typedef struct {
    s16 routeRow;   // row of the package's patrol-route table holding this room's routes (1..14); 0 ends the table
    s16 stage;      // `GAME_STAGE_*` of the room
    s16 area;       // `GAME_AREA_*` of the room within `stage`
    u16 pointCount; // points in each of the row's routes
} _Actor00300PatrolRoom;
STATIC_ASSERT_SIZEOF(_Actor00300PatrolRoom, 8);

/// Ticks `_Actor00300Work::alertTimer` is reloaded with: how long the actor
/// keeps after the player once it has lost sight of them.
#define ACTOR_00300_ALERT_TICKS 450

/// Amounts `_Actor00300Work::mp` changes by.
#define ACTOR_00300_MP_AT_SPAWN      40
#define ACTOR_00300_MP_DRAINED       20 // taken from the player by a touch of the drain attack
#define ACTOR_00300_MP_HEAL_COST     20
#define ACTOR_00300_MP_FIREBALL_COST 5
#define ACTOR_00300_MP_RECHARGED     5 // gained by `ACTOR_00300_ACTION_RECHARGE`

/// Values of `_Actor00300Work::action`: the handler the per-frame tick runs.
///
/// A handler numbers its own stages in `actionStep`, from 0 on entry.
/// Animation numbers are `ACTOR_00300_ANIM_*`.
enum {
    ACTOR_00300_ACTION_PATROL   = 0, // goes round the room's patrol points; a sighting, the group's attack alert or a hit sends it after the player
    ACTOR_00300_ACTION_PURSUE   = 1, // by turns waits and closes on the player while `alertTimer` runs, and picks an attack each time `attackTimer` runs out with the player in sight; patrols again once the alert has run out
    ACTOR_00300_ACTION_FIREBALL = 2, // turns to the player, charges, launches the fireball and recovers
    ACTOR_00300_ACTION_DRAIN    = 3, // turns to the player, then stretches the drain model out and back
    ACTOR_00300_ACTION_HEAL     = 4, // charges, then gains 100 hit points
    ACTOR_00300_ACTION_HURT     = 5, // flinches from a hit, or is knocked down and gets up; knocked down with no hit points left, it hands the task to its death state instead
    ACTOR_00300_ACTION_BUILDUP  = 6, // held still by a buildup reaction until the reaction runs out
    ACTOR_00300_ACTION_RECHARGE = 7, // stands for 91 ticks, then gains `ACTOR_00300_MP_RECHARGED`
    ACTOR_00300_ACTION_DEAD     = 8  // no handler; set as the task moves to its death state
};

/// Values of `_Actor00300Work::anim`: indices into the package's
/// animation-set table, named for the action that plays each.
///
/// A `BACK` animation answers a hit from behind and a `FRONT` one a hit from
/// ahead (`hitFromFront`). An animation an event asks for by message is stored
/// as its id plus `ACTOR_00300_ANIM_EVENT_BASE`.
enum {
    ACTOR_00300_ANIM_STAND            = 1,
    ACTOR_00300_ANIM_MOVE             = 2,
    ACTOR_00300_ANIM_TURN             = 3, // turning on the spot; also the recharge
    ACTOR_00300_ANIM_CHARGE           = 4, // building up the fireball or the heal
    ACTOR_00300_ANIM_RELEASE          = 5, // letting the charge go
    ACTOR_00300_ANIM_RECOVER          = 6, // settling after the release
    ACTOR_00300_ANIM_DRAIN            = 7,
    ACTOR_00300_ANIM_FLINCH_BACK      = 9,
    ACTOR_00300_ANIM_FLINCH_FRONT     = 10,
    ACTOR_00300_ANIM_KNOCK_DOWN_BACK  = 11, // also the death
    ACTOR_00300_ANIM_KNOCK_DOWN_FRONT = 12, // also the death
    ACTOR_00300_ANIM_GET_UP_BACK      = 13,
    ACTOR_00300_ANIM_GET_UP_FRONT     = 14,
    ACTOR_00300_ANIM_BUILDUP          = 15,
    ACTOR_00300_ANIM_BUILDUP_END      = 18,
    ACTOR_00300_ANIM_EVENT_BASE       = 19
};

/// Combat thresholds and sound selectors of this actor's action handlers.
///
/// Sound selectors leave the instance byte clear; callers insert the placement index.
/// The halo release state requests teardown by the room's halo task.
enum {
    ACTOR_00300_ACTION_BEGIN       = 0,
    ACTOR_00300_PURSUIT_WAIT       = 0,
    ACTOR_00300_PURSUIT_MOVE       = 1,
    ACTOR_00300_CLOSE_RANGE        = 3000,
    ACTOR_00300_HEAL_AMOUNT        = 100,
    ACTOR_00300_SOUND_CHARGE       = 0x40030009,
    ACTOR_00300_SOUND_RESTORE      = 0x4003000B,
    ACTOR_00300_HALO_RELEASE_STATE = 3,
    ACTOR_00300_MOTE_ANGLE_MASK    = 0xF80, // 32 bearings, 128 angle units apart
    ACTOR_00300_MOTE_SPAWN_MASK    = 3      // Emit on one of four upper-halfword draws
};

/// Draw-request bits retained for the enemy and its drain attachment during events.
enum {
    ACTOR_00300_EVENT_DRAW_SHOW             = 1 << 0,
    ACTOR_00300_EVENT_DRAW_SKIP_AUTO_BUFFER = 1 << 1
};

/// Work block of the package's enemy task.
///
/// The spawn handler allocates it zeroed and keeps it at `Task::work`. It
/// holds the animation rig, storage for the models' matrices, four collision
/// bodies with their contact tables, and the state the per-frame tick steers
/// the actor with. The drain model's task and the fireball's task read it
/// through their parent task.
///
/// Angles are 4096ths of a turn, and `yaw` is the heading about Y. Timers
/// count ticks.
typedef struct {
    ActorAnimRig19        rig;               // playback of the body model's nineteen parts; slots 1 to 18 are driven
    Task*                 drainModelTask;    // task of the one-part model hung under body part 7, which the drain attack stretches; `drainBody` rides its coordinate
    MATRIX                colorMtx;          // storage for `TmdObject::colorMtx` of the body and of the drain model
    MATRIX                lightMtx;          // storage for their `TmdObject::lightMtx`
    WorldCollisionBody    sightBody;         // capsule on body part 2 that harms nothing and only reports the player being inside it
    WorldCollisionCapsule sightCapsule;      // shape of `sightBody`: from the part to 9000 ahead of it, radius 1000 at the part and 4000 at the far end
    WorldCollisionContact sightContacts[1];  // contact of `sightBody`; while it holds a category-1 key, a wall test between player and actor decides `playerSeen`
    WorldCollisionBody    hitBody;           // sphere of radius 350 on body part 3 that takes the hits; the grid tests it only from the fatal knock-down on
    WorldCollisionContact hitContacts[3];    // contacts of `hitBody`; also the enemy's hit records
    WorldCollisionBody    gridBody;          // sphere of radius 500, 500 above the root, that the room grid tests and that rests on the floor; the grid leaves it alone from the fatal knock-down on
    WorldCollisionContact gridContacts[4];   // contacts of `gridBody`, which push the root out of the room's faces
    WorldCollisionBody    drainBody;         // sphere of radius 700 on the drain model, keyed with the package's first attack; pair tests are on only while the drain attack reaches out
    WorldCollisionContact drainContacts[1];  // contact of `drainBody`; a touch takes `ACTOR_00300_MP_DRAINED` from the player into `mp`
    EffectSpawnArg        hitEffectArg;      // argument record of the effect a hit spawns, hung off body part 3
    VECTOR3               prevPos;           // root position before the tick's movement, restored when the grid cannot resolve a contact
    byte                  pad_604[4];        // never accessed
    MATRIX                savedRootMtx;      // root transform taken at spawn and again at death, where it is the pose the collapse scales
    MATRIX                drainModelMtx;     // the drain model's own transform as spawned; each tick rebuilds the model from it, stretched by `drainScaleY`
    SVECTOR*              patrolPoints;      // route of `patrolPointCount` points (X and Z are used) the room gives this placement's mode; NULL in a room with none, where the tick does nothing
    byte                  pad_64C[8];        // never accessed
    EffectWork*           chargeEffect;      // halo effect of the charge in progress, kept so an interruption can end it; NULL when there is none or `chargeEffectTimer` has run out
    s32                   chargeSound;       // sound started with `chargeEffect`, stopped together with it
    SVECTOR               hitTwist;          // rotation a light hit knocks body part 3 by (`vx` and `vy` only), walked back 0x20 a tick towards zero
    s16                   hitTwistActive;    // nonzero while `hitTwist` still has to be applied
    s16                   mp;                // points the attacks are paid from (`ACTOR_00300_MP_*`); too few for a fireball leaves the drain attack or a recharge
    byte                  pad_668[2];        // never accessed
    s16                   fireballAttack;    // attack of the package's damage table the next fireball is keyed with (1..3), which also sets how long it charges; a draw of 0 calls the attack off
    s16                   hitCooldown;       // ticks left in which further attack contacts are ignored, set by the attack that landed; 0 when hits count
    s16                   anim;              // `ACTOR_00300_ANIM_*` the actions ask for
    s16                   playingAnim;       // `anim` the slots were last started on; a difference from `anim` restarts them
    s16                   animFrame;         // ticks since `playingAnim` was started
    s16                   deathScaleY;       // Y scale of the collapsing body, 4096 for full height; shrinks by 80 a tick to about an eighth
    s16                   drainScaleY;       // Y scale of the drain model, 4096 for full length; grows 256 a tick as the drain attack reaches out and shrinks back after it; at 0 or below the model is not drawn
    u16                   eventDrawFlags;    // argument of the last `ACTOR_MESSAGE_SET_MODEL_DRAW`, applied to the models again each tick while an event runs (bit 0 draw, bit 1 `TMD_OBJECT_SKIP_AUTO_BUFFER`)
    s16                   speed;             // distance moved along the facing each tick; a patrol also snaps to a point once it is this near
    s16                   turnRate;          // most `yaw` may change in a tick on its way to `targetYaw`; 0 leaves the rotation alone
    s16                   yaw;               // heading of the model's root, read back from its rotation before each turn
    u16                   targetYaw;         // heading `yaw` is turned toward
    s16                   burstStage;        // 0 unless the last hit bursts the body; then 1 and 2 over the first death ticks, after which the model gives up its buffers and five fragment effects are spawned
    s16                   action;            // `ACTOR_00300_ACTION_*`
    s16                   actionStep;        // stage of the running action, 0 on entry; the death state of the task counts its own stages in it (0 begin, 1 collapse, 2 destroy, 3 burst)
    s16                   timer;             // countdown of the running action's wait; `ACTION_HURT` keeps the animation tick its reaction ends on here, and the recharge and the death state count it up instead
    s16                   attackTimer;       // ticks of pursuit left before the next attack is picked
    s16                   patrolPointCount;  // points in `patrolPoints`
    s16                   patrolPoint;       // index of the point the patrol is heading for
    s16                   hitDamage;         // damage of the last hit or damage-over-time pulse; nonzero also rouses a patrol, and the return to patrol clears it
    s16                   hitFromFront;      // 1 when the last attacker stood ahead of the actor, 0 when behind; picks the reaction's animation
    s16                   knockDown;         // set by a hit of attack kind 1, which knocks the actor down whatever its damage; cleared when the reaction ends
    u16                   lastCueFlags;      // `ANIMATION_RECORD_CUE_MASK` bits of slot 1's record on the previous tick; a sound plays when one drops
    s16                   notLockable;       // the placement's variant; nonzero keeps the player from locking on to the actor
    s16                   deathStage;        // 0 alive; 1 on the tick the fatal knock-down begins and 2 from the next, when the root stops being pulled down onto the floor
    s16                   chargeEffectTimer; // ticks until `chargeEffect` has ended by itself and is forgotten
    s16                   reactionBlend;     // nonzero during a flinch and while getting up: animations then start over 8 blend ticks instead of their own count
    s16                   alertTimer;        // ticks of pursuit left; reloaded with `ACTOR_00300_ALERT_TICKS` while the player is in sight, and 0 on patrol
    s16                   playerSeen;        // 1 on a tick `sightBody` held the player with no wall between (0 otherwise)
} _Actor00300Work;
STATIC_ASSERT_SIZEOF(_Actor00300Work, 0x6A4);

/// Ticks a fireball flies before it bursts by itself.
#define ACTOR_00300_FIREBALL_FLIGHT_TICKS 30

/// Ticks a burst fireball's task is kept before it is destroyed.
#define ACTOR_00300_FIREBALL_LINGER_TICKS 60

/// Values of `_Actor00300FireballWork::teardownStep`.
enum {
    ACTOR_00300_FIREBALL_TEARDOWN_UNLINK = 0, // unlinks both bodies and starts the `ACTOR_00300_FIREBALL_LINGER_TICKS` wait
    ACTOR_00300_FIREBALL_TEARDOWN_LINGER = 1  // counts `timer` down, then destroys the task
};

/// Work block of the fireball the enemy launches.
///
/// The fireball is a task of its own with a coordinate for a body. Its spawn
/// handler allocates the block zeroed and keeps it at `Task::work`, places the
/// coordinate 1500 above and 800 ahead of the enemy's root with the enemy's
/// rotation, and detaches the task from the enemy. Each tick the fireball then
/// moves 400 along its Z, turns toward the player and draws its glow. It
/// bursts when `body` has touched something, when the path it covered crossed
/// a room surface that blocks probes, or when
/// `ACTOR_00300_FIREBALL_FLIGHT_TICKS` have passed; the task then goes through
/// `teardownStep`.
typedef struct {
    WorldCollisionBody    body;             // sphere of radius 450 on the coordinate, keyed with the attack the enemy drew (`_Actor00300Work::fireballAttack`); pair tests are on until the teardown unlinks it
    WorldCollisionContact contacts[1];      // contact of `body`; an occupied one ends the flight
    WorldCollisionBody    sweepBody;        // keyless capsule body on the same coordinate that only the room grid tests, clipped to its contact
    WorldCollisionCapsule sweepCapsule;     // shape of `sweepBody`: from the coordinate's origin to 420 back along its Z, radius 1 at both ends, which covers the tick's movement
    WorldCollisionContact sweepContacts[1]; // contact of `sweepBody`: the room face the path crossed, whose surface parameters decide whether the flight ends
    s16                   timer;            // ticks left: of the flight, from `ACTOR_00300_FIREBALL_FLIGHT_TICKS`; then of the wait before the task is destroyed, from `ACTOR_00300_FIREBALL_LINGER_TICKS`
    s16                   teardownStep;     // `ACTOR_00300_FIREBALL_TEARDOWN_*`; set to the first as the fireball bursts
} _Actor00300FireballWork;
STATIC_ASSERT_SIZEOF(_Actor00300FireballWork, 0x8C);

extern DamageAttack Actor00300_D15FD8[4];

extern s16 Actor00300_D16394[];
extern s16 Actor00300_D16000[];
extern s16 Actor00300_D15FF8[];

static void _actor00300TickEvent(Enemy* enemy, Task* task);
static void Actor00300_Fn04958(Enemy* arg0, Task* arg1);

static void Actor00300_Fn00970(Enemy* enemy, Task* task);
static void _actor00300TurnFireballTowardPlayer(Task* task);
static void Actor00300_Fn00E54(Task* arg0);
static void _actor00300TickPatrol(Task* task);
static void _actor00300TickPursuit(Task* task);
static void _actor00300SelectAttack(Task* task);
static void _actor00300TickFireballAttack(Task* task);
static void _actor00300TickDrainAttack(Task* task);
static void _actor00300TickHeal(Task* task);
static void _actor00300TickHurt(Task* task);
static void _actor00300TickRecharge(Task* task);
static void _actor00300TurnTowardTargetYaw(Task* task);
static void _actor00300ApplyHitTwist(Task* task);
static void _actor00300PlayAnimationCueSounds(Task* task);
static void Actor00300_Fn03B70(Enemy* arg0, Task* arg1);
static void Actor00300_Fn047CC(Enemy* arg0, Task* arg1);
static void _actor00300TickStatusReactions(Task* task);
static void _actor00300TickAction(Task* task);
static void _actor00300TickBuildup(Task* task);
static void _actor00300Move(Task* task);
static void _actor00300UpdateAnimation(Task* task);
static void _actor00300UpdateLighting(Task* task);
static void _actor00300DrawShadow(Task* task);
static void Actor00300_Fn0505C(Task* arg0, MATRIX* arg1, s16 arg2);
static void _actor00300InitDrainModel(Enemy* enemy, Task* task);
static void _actor00300TeardownFireball(Enemy* enemy, Task* task);

extern EnemyParams           Actor00300_D15FE8;
extern _Actor00300PatrolRoom Actor00300_D16020[15];
extern SVECTOR*              Actor00300_D16278[15][2];
extern TaskDesc              Actor00300_D162F0[];
// Typed callback views for the task message dispatcher.

extern TaskMessageEntry Actor00300_D16314[5];
extern AnimationSet*    Actor00300_D1633C[22];

/// State handlers of the task `Actor00300_Fn04770` dispatches, indexed by
/// `Task::state`. The first sets the task up and moves it to state 1.
static const EnemyTaskFuncTable3 Actor00300_D00004 = {
    {
        Actor00300_Fn00970,
        Actor00300_Fn047CC,
        Actor00300_Fn03B70,
    },
};

static AnimationSet _gActor00300Actor100300Animation0CAD4;
static AnimationSet _gActor00300Actor100300Animation0D8A8;
static AnimationSet _gActor00300Actor100300Animation0E028;
static AnimationSet _gActor00300Actor100300Animation0ECF8;
static AnimationSet _gActor00300Actor100300Animation0F1A8;
static AnimationSet _gActor00300Actor100300Animation0F5E4;
static AnimationSet _gActor00300Actor100300Animation0FF4C;
static AnimationSet _gActor00300Actor100300Animation1065C;
static AnimationSet _gActor00300Actor100300Animation10FBC;
static AnimationSet _gActor00300Actor100300Animation118C4;
static AnimationSet _gActor00300Actor100300Animation11F30;
static AnimationSet _gActor00300Actor100300Animation12754;
static AnimationSet _gActor00300Actor100300Animation12E24;
static AnimationSet _gActor00300Actor100300Animation1369C;
static AnimationSet _gActor00300Actor100300Animation13C00;
static AnimationSet _gActor00300Actor100300Animation13DE4;
static AnimationSet _gActor00300Actor100300Animation13FC0;
static AnimationSet _gActor00300Actor100300Animation14268;
static AnimationSet _gActor00300Actor100300Animation14C8C;
static AnimationSet _gActor00300Actor100300Animation15594;
static AnimationSet _gActor00300Actor100300Animation15FB0;
static TmdSource    _gActor00300StingerBody;
static TmdSource    _gActor00300Actor100300Model09FA0;
static s32          _actor00300MsgPlayAnimation(Task* task, s32 messageId, const AnimationPlayRequest* request, s32 unusedSecondArg);
static s32          _actor00300MsgSetModelDraw(Task* task, s32 messageId, s32 drawFlags, s32 unusedSecondArg);
static s32          _actor00300MsgApplyCommand(Task* task, s32 messageId, const ActorCommand* command, s32 unusedSecondArg);
void                Actor00300_Fn04770(Task*);
static void         _actor00300DrainModelTask(Task* task);
void                Actor00300_Fn0521C(Task*);

static TmdBone _gActor00300StingerBodySkeleton[19] = {
#include "assets/stinger_body_skeleton.inc"
};

static u32 _gActor00300StingerBodyPartVerts[19] = {
#include "assets/stinger_body_partVerts.inc"
};

static SVECTOR _gActor00300StingerBodyVerts[250] = {
#include "assets/stinger_body_verts.inc"
};

static SVECTOR _gActor00300StingerBodyNormals[257] = {
#include "assets/stinger_body_normals.inc"
};

static u32 _gActor00300StingerBodyStream[3520] = {
#include "assets/stinger_body_stream.inc"
};

static TmdSource _gActor00300StingerBody = {
    0,
    17784,
    6072,
    19,
    _gActor00300StingerBodyPartVerts,
    _gActor00300StingerBodyVerts,
    _gActor00300StingerBodyNormals,
    _gActor00300StingerBodySkeleton,
    _gActor00300StingerBodyStream,
};

static TmdBone _gActor00300Actor100300Model09FA0Skeleton[1] = {
#include "assets/actor_100300_model_09FA0_skeleton.inc"
};

static u32 _gActor00300Actor100300Model09FA0PartVerts[1] = {
#include "assets/actor_100300_model_09FA0_partVerts.inc"
};

static SVECTOR _gActor00300Actor100300Model09FA0Verts[13] = {
#include "assets/actor_100300_model_09FA0_verts.inc"
};

static SVECTOR _gActor00300Actor100300Model09FA0Normals[13] = {
#include "assets/actor_100300_model_09FA0_normals.inc"
};

static u32 _gActor00300Actor100300Model09FA0Stream[96] = {
#include "assets/actor_100300_model_09FA0_stream.inc"
};

static TmdSource _gActor00300Actor100300Model09FA0 = {
    0,
    628,
    0,
    1,
    _gActor00300Actor100300Model09FA0PartVerts,
    _gActor00300Actor100300Model09FA0Verts,
    _gActor00300Actor100300Model09FA0Normals,
    _gActor00300Actor100300Model09FA0Skeleton,
    _gActor00300Actor100300Model09FA0Stream,
};

static TmdBone _gActor00300BrainStingerBurstHeadSkeleton[1] = {
#include "assets/brain_stinger_burst_head_skeleton.inc"
};

static u32 _gActor00300BrainStingerBurstHeadPartVerts[1] = {
#include "assets/brain_stinger_burst_head_partVerts.inc"
};

static SVECTOR _gActor00300BrainStingerBurstHeadVerts[40] = {
#include "assets/brain_stinger_burst_head_verts.inc"
};

static SVECTOR _gActor00300BrainStingerBurstHeadNormals[40] = {
#include "assets/brain_stinger_burst_head_normals.inc"
};

static u32 _gActor00300BrainStingerBurstHeadStream[395] = {
#include "assets/brain_stinger_burst_head_stream.inc"
};

static TmdSource _gActor00300BrainStingerBurstHead = {
    0,
    2648,
    0,
    1,
    _gActor00300BrainStingerBurstHeadPartVerts,
    _gActor00300BrainStingerBurstHeadVerts,
    _gActor00300BrainStingerBurstHeadNormals,
    _gActor00300BrainStingerBurstHeadSkeleton,
    _gActor00300BrainStingerBurstHeadStream,
};

static TmdBone _gActor00300Actor100300Model0ABC4Skeleton[1] = {
#include "assets/actor_100300_model_0ABC4_skeleton.inc"
};

static u32 _gActor00300Actor100300Model0ABC4PartVerts[1] = {
#include "assets/actor_100300_model_0ABC4_partVerts.inc"
};

static SVECTOR _gActor00300Actor100300Model0ABC4Verts[22] = {
#include "assets/actor_100300_model_0ABC4_verts.inc"
};

static SVECTOR _gActor00300Actor100300Model0ABC4Normals[22] = {
#include "assets/actor_100300_model_0ABC4_normals.inc"
};

static u32 _gActor00300Actor100300Model0ABC4Stream[194] = {
#include "assets/actor_100300_model_0ABC4_stream.inc"
};

static TmdSource _gActor00300Actor100300Model0ABC4 = {
    0,
    1292,
    0,
    1,
    _gActor00300Actor100300Model0ABC4PartVerts,
    _gActor00300Actor100300Model0ABC4Verts,
    _gActor00300Actor100300Model0ABC4Normals,
    _gActor00300Actor100300Model0ABC4Skeleton,
    _gActor00300Actor100300Model0ABC4Stream,
};

static TmdBone _gActor00300Actor100300Model0B128Skeleton[1] = {
#include "assets/actor_100300_model_0B128_skeleton.inc"
};

static u32 _gActor00300Actor100300Model0B128PartVerts[1] = {
#include "assets/actor_100300_model_0B128_partVerts.inc"
};

static SVECTOR _gActor00300Actor100300Model0B128Verts[33] = {
#include "assets/actor_100300_model_0B128_verts.inc"
};

static SVECTOR _gActor00300Actor100300Model0B128Normals[33] = {
#include "assets/actor_100300_model_0B128_normals.inc"
};

static u32 _gActor00300Actor100300Model0B128Stream[326] = {
#include "assets/actor_100300_model_0B128_stream.inc"
};

static TmdSource _gActor00300Actor100300Model0B128 = {
    0,
    2172,
    0,
    1,
    _gActor00300Actor100300Model0B128PartVerts,
    _gActor00300Actor100300Model0B128Verts,
    _gActor00300Actor100300Model0B128Normals,
    _gActor00300Actor100300Model0B128Skeleton,
    _gActor00300Actor100300Model0B128Stream,
};

static TmdBone _gActor00300Actor100300Model0B8ACSkeleton[1] = {
#include "assets/actor_100300_model_0B8AC_skeleton.inc"
};

static u32 _gActor00300Actor100300Model0B8ACPartVerts[1] = {
#include "assets/actor_100300_model_0B8AC_partVerts.inc"
};

static SVECTOR _gActor00300Actor100300Model0B8ACVerts[34] = {
#include "assets/actor_100300_model_0B8AC_verts.inc"
};

static SVECTOR _gActor00300Actor100300Model0B8ACNormals[34] = {
#include "assets/actor_100300_model_0B8AC_normals.inc"
};

static u32 _gActor00300Actor100300Model0B8ACStream[358] = {
#include "assets/actor_100300_model_0B8AC_stream.inc"
};

static TmdSource _gActor00300Actor100300Model0B8AC = {
    0,
    2364,
    0,
    1,
    _gActor00300Actor100300Model0B8ACPartVerts,
    _gActor00300Actor100300Model0B8ACVerts,
    _gActor00300Actor100300Model0B8ACNormals,
    _gActor00300Actor100300Model0B8ACSkeleton,
    _gActor00300Actor100300Model0B8ACStream,
};

static TmdBone _gActor00300Actor100300Model0BFC0Skeleton[1] = {
#include "assets/actor_100300_model_0BFC0_skeleton.inc"
};

static u32 _gActor00300Actor100300Model0BFC0PartVerts[1] = {
#include "assets/actor_100300_model_0BFC0_partVerts.inc"
};

static SVECTOR _gActor00300Actor100300Model0BFC0Verts[19] = {
#include "assets/actor_100300_model_0BFC0_verts.inc"
};

static SVECTOR _gActor00300Actor100300Model0BFC0Normals[19] = {
#include "assets/actor_100300_model_0BFC0_normals.inc"
};

static u32 _gActor00300Actor100300Model0BFC0Stream[193] = {
#include "assets/actor_100300_model_0BFC0_stream.inc"
};

static TmdSource _gActor00300Actor100300Model0BFC0 = {
    0,
    1248,
    0,
    1,
    _gActor00300Actor100300Model0BFC0PartVerts,
    _gActor00300Actor100300Model0BFC0Verts,
    _gActor00300Actor100300Model0BFC0Normals,
    _gActor00300Actor100300Model0BFC0Skeleton,
    _gActor00300Actor100300Model0BFC0Stream,
};

static AnimationPackedPose _gActor00300Actor100300Animation0CAD4Bank1[10] = {
#include "assets/actor_100300_animation_0CAD4_bank1.inc"
};

static AnimationPackedRotation _gActor00300Actor100300Animation0CAD4Bank4[167] = {
#include "assets/actor_100300_animation_0CAD4_bank4.inc"
};

static AnimationRecord _gActor00300Actor100300Animation0CAD4Records[300] = {
#include "assets/actor_100300_animation_0CAD4_records.inc"
};

static u16 _gActor00300Actor100300Animation0CAD4Indices[20] = {
#include "assets/actor_100300_animation_0CAD4_indices.inc"
};

static AnimationSet _gActor00300Actor100300Animation0CAD4 = {
    _gActor00300Actor100300Animation0CAD4Records,
    _gActor00300Actor100300Animation0CAD4Indices,
    { NULL, _gActor00300Actor100300Animation0CAD4Bank1, NULL, NULL, _gActor00300Actor100300Animation0CAD4Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor00300Actor100300Animation0D8A8Bank1[48] = {
#include "assets/actor_100300_animation_0D8A8_bank1.inc"
};

static AnimationPackedRotation _gActor00300Actor100300Animation0D8A8Bank4[294] = {
#include "assets/actor_100300_animation_0D8A8_bank4.inc"
};

static AnimationRecord _gActor00300Actor100300Animation0D8A8Records[427] = {
#include "assets/actor_100300_animation_0D8A8_records.inc"
};

static u16 _gActor00300Actor100300Animation0D8A8Indices[20] = {
#include "assets/actor_100300_animation_0D8A8_indices.inc"
};

static AnimationSet _gActor00300Actor100300Animation0D8A8 = {
    _gActor00300Actor100300Animation0D8A8Records,
    _gActor00300Actor100300Animation0D8A8Indices,
    { NULL, _gActor00300Actor100300Animation0D8A8Bank1, NULL, NULL, _gActor00300Actor100300Animation0D8A8Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor00300Actor100300Animation0E028Bank1[14] = {
#include "assets/actor_100300_animation_0E028_bank1.inc"
};

static AnimationPackedRotation _gActor00300Actor100300Animation0E028Bank4[170] = {
#include "assets/actor_100300_animation_0E028_bank4.inc"
};

static AnimationRecord _gActor00300Actor100300Animation0E028Records[248] = {
#include "assets/actor_100300_animation_0E028_records.inc"
};

static u16 _gActor00300Actor100300Animation0E028Indices[20] = {
#include "assets/actor_100300_animation_0E028_indices.inc"
};

static AnimationSet _gActor00300Actor100300Animation0E028 = {
    _gActor00300Actor100300Animation0E028Records,
    _gActor00300Actor100300Animation0E028Indices,
    { NULL, _gActor00300Actor100300Animation0E028Bank1, NULL, NULL, _gActor00300Actor100300Animation0E028Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor00300Actor100300Animation0ECF8Bank1[16] = {
#include "assets/actor_100300_animation_0ECF8_bank1.inc"
};

static AnimationPackedRotation _gActor00300Actor100300Animation0ECF8Bank4[298] = {
#include "assets/actor_100300_animation_0ECF8_bank4.inc"
};

static AnimationRecord _gActor00300Actor100300Animation0ECF8Records[454] = {
#include "assets/actor_100300_animation_0ECF8_records.inc"
};

static u16 _gActor00300Actor100300Animation0ECF8Indices[20] = {
#include "assets/actor_100300_animation_0ECF8_indices.inc"
};

static AnimationSet _gActor00300Actor100300Animation0ECF8 = {
    _gActor00300Actor100300Animation0ECF8Records,
    _gActor00300Actor100300Animation0ECF8Indices,
    { NULL, _gActor00300Actor100300Animation0ECF8Bank1, NULL, NULL, _gActor00300Actor100300Animation0ECF8Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor00300Actor100300Animation0F1A8Bank1[7] = {
#include "assets/actor_100300_animation_0F1A8_bank1.inc"
};

static AnimationPackedRotation _gActor00300Actor100300Animation0F1A8Bank4[114] = {
#include "assets/actor_100300_animation_0F1A8_bank4.inc"
};

static AnimationRecord _gActor00300Actor100300Animation0F1A8Records[145] = {
#include "assets/actor_100300_animation_0F1A8_records.inc"
};

static u16 _gActor00300Actor100300Animation0F1A8Indices[20] = {
#include "assets/actor_100300_animation_0F1A8_indices.inc"
};

static AnimationSet _gActor00300Actor100300Animation0F1A8 = {
    _gActor00300Actor100300Animation0F1A8Records,
    _gActor00300Actor100300Animation0F1A8Indices,
    { NULL, _gActor00300Actor100300Animation0F1A8Bank1, NULL, NULL, _gActor00300Actor100300Animation0F1A8Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor00300Actor100300Animation0F5E4Bank1[8] = {
#include "assets/actor_100300_animation_0F5E4_bank1.inc"
};

static AnimationPackedRotation _gActor00300Actor100300Animation0F5E4Bank4[97] = {
#include "assets/actor_100300_animation_0F5E4_bank4.inc"
};

static AnimationRecord _gActor00300Actor100300Animation0F5E4Records[130] = {
#include "assets/actor_100300_animation_0F5E4_records.inc"
};

static u16 _gActor00300Actor100300Animation0F5E4Indices[20] = {
#include "assets/actor_100300_animation_0F5E4_indices.inc"
};

static AnimationSet _gActor00300Actor100300Animation0F5E4 = {
    _gActor00300Actor100300Animation0F5E4Records,
    _gActor00300Actor100300Animation0F5E4Indices,
    { NULL, _gActor00300Actor100300Animation0F5E4Bank1, NULL, NULL, _gActor00300Actor100300Animation0F5E4Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor00300Actor100300Animation0FF4CBank1[17] = {
#include "assets/actor_100300_animation_0FF4C_bank1.inc"
};

static AnimationPackedRotation _gActor00300Actor100300Animation0FF4CBank4[232] = {
#include "assets/actor_100300_animation_0FF4C_bank4.inc"
};

static AnimationRecord _gActor00300Actor100300Animation0FF4CRecords[299] = {
#include "assets/actor_100300_animation_0FF4C_records.inc"
};

static u16 _gActor00300Actor100300Animation0FF4CIndices[20] = {
#include "assets/actor_100300_animation_0FF4C_indices.inc"
};

static AnimationSet _gActor00300Actor100300Animation0FF4C = {
    _gActor00300Actor100300Animation0FF4CRecords,
    _gActor00300Actor100300Animation0FF4CIndices,
    { NULL, _gActor00300Actor100300Animation0FF4CBank1, NULL, NULL, _gActor00300Actor100300Animation0FF4CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor00300Actor100300Animation1065CBank1[12] = {
#include "assets/actor_100300_animation_1065C_bank1.inc"
};

static AnimationPackedRotation _gActor00300Actor100300Animation1065CBank4[171] = {
#include "assets/actor_100300_animation_1065C_bank4.inc"
};

static AnimationRecord _gActor00300Actor100300Animation1065CRecords[225] = {
#include "assets/actor_100300_animation_1065C_records.inc"
};

static u16 _gActor00300Actor100300Animation1065CIndices[20] = {
#include "assets/actor_100300_animation_1065C_indices.inc"
};

static AnimationSet _gActor00300Actor100300Animation1065C = {
    _gActor00300Actor100300Animation1065CRecords,
    _gActor00300Actor100300Animation1065CIndices,
    { NULL, _gActor00300Actor100300Animation1065CBank1, NULL, NULL, _gActor00300Actor100300Animation1065CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor00300Actor100300Animation10FBCBank1[16] = {
#include "assets/actor_100300_animation_10FBC_bank1.inc"
};

static AnimationPackedRotation _gActor00300Actor100300Animation10FBCBank4[228] = {
#include "assets/actor_100300_animation_10FBC_bank4.inc"
};

static AnimationRecord _gActor00300Actor100300Animation10FBCRecords[304] = {
#include "assets/actor_100300_animation_10FBC_records.inc"
};

static u16 _gActor00300Actor100300Animation10FBCIndices[20] = {
#include "assets/actor_100300_animation_10FBC_indices.inc"
};

static AnimationSet _gActor00300Actor100300Animation10FBC = {
    _gActor00300Actor100300Animation10FBCRecords,
    _gActor00300Actor100300Animation10FBCIndices,
    { NULL, _gActor00300Actor100300Animation10FBCBank1, NULL, NULL, _gActor00300Actor100300Animation10FBCBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor00300Actor100300Animation118C4Bank1[16] = {
#include "assets/actor_100300_animation_118C4_bank1.inc"
};

static AnimationPackedRotation _gActor00300Actor100300Animation118C4Bank4[224] = {
#include "assets/actor_100300_animation_118C4_bank4.inc"
};

static AnimationRecord _gActor00300Actor100300Animation118C4Records[286] = {
#include "assets/actor_100300_animation_118C4_records.inc"
};

static u16 _gActor00300Actor100300Animation118C4Indices[20] = {
#include "assets/actor_100300_animation_118C4_indices.inc"
};

static AnimationSet _gActor00300Actor100300Animation118C4 = {
    _gActor00300Actor100300Animation118C4Records,
    _gActor00300Actor100300Animation118C4Indices,
    { NULL, _gActor00300Actor100300Animation118C4Bank1, NULL, NULL, _gActor00300Actor100300Animation118C4Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor00300Actor100300Animation11F30Bank1[11] = {
#include "assets/actor_100300_animation_11F30_bank1.inc"
};

static AnimationPackedRotation _gActor00300Actor100300Animation11F30Bank4[158] = {
#include "assets/actor_100300_animation_11F30_bank4.inc"
};

static AnimationRecord _gActor00300Actor100300Animation11F30Records[200] = {
#include "assets/actor_100300_animation_11F30_records.inc"
};

static u16 _gActor00300Actor100300Animation11F30Indices[20] = {
#include "assets/actor_100300_animation_11F30_indices.inc"
};

static AnimationSet _gActor00300Actor100300Animation11F30 = {
    _gActor00300Actor100300Animation11F30Records,
    _gActor00300Actor100300Animation11F30Indices,
    { NULL, _gActor00300Actor100300Animation11F30Bank1, NULL, NULL, _gActor00300Actor100300Animation11F30Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor00300Actor100300Animation12754Bank1[14] = {
#include "assets/actor_100300_animation_12754_bank1.inc"
};

static AnimationPackedRotation _gActor00300Actor100300Animation12754Bank4[199] = {
#include "assets/actor_100300_animation_12754_bank4.inc"
};

static AnimationRecord _gActor00300Actor100300Animation12754Records[260] = {
#include "assets/actor_100300_animation_12754_records.inc"
};

static u16 _gActor00300Actor100300Animation12754Indices[20] = {
#include "assets/actor_100300_animation_12754_indices.inc"
};

static AnimationSet _gActor00300Actor100300Animation12754 = {
    _gActor00300Actor100300Animation12754Records,
    _gActor00300Actor100300Animation12754Indices,
    { NULL, _gActor00300Actor100300Animation12754Bank1, NULL, NULL, _gActor00300Actor100300Animation12754Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor00300Actor100300Animation12E24Bank1[12] = {
#include "assets/actor_100300_animation_12E24_bank1.inc"
};

static AnimationPackedRotation _gActor00300Actor100300Animation12E24Bank4[169] = {
#include "assets/actor_100300_animation_12E24_bank4.inc"
};

static AnimationRecord _gActor00300Actor100300Animation12E24Records[211] = {
#include "assets/actor_100300_animation_12E24_records.inc"
};

static u16 _gActor00300Actor100300Animation12E24Indices[20] = {
#include "assets/actor_100300_animation_12E24_indices.inc"
};

static AnimationSet _gActor00300Actor100300Animation12E24 = {
    _gActor00300Actor100300Animation12E24Records,
    _gActor00300Actor100300Animation12E24Indices,
    { NULL, _gActor00300Actor100300Animation12E24Bank1, NULL, NULL, _gActor00300Actor100300Animation12E24Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor00300Actor100300Animation1369CBank1[16] = {
#include "assets/actor_100300_animation_1369C_bank1.inc"
};

static AnimationPackedRotation _gActor00300Actor100300Animation1369CBank4[212] = {
#include "assets/actor_100300_animation_1369C_bank4.inc"
};

static AnimationRecord _gActor00300Actor100300Animation1369CRecords[262] = {
#include "assets/actor_100300_animation_1369C_records.inc"
};

static u16 _gActor00300Actor100300Animation1369CIndices[20] = {
#include "assets/actor_100300_animation_1369C_indices.inc"
};

static AnimationSet _gActor00300Actor100300Animation1369C = {
    _gActor00300Actor100300Animation1369CRecords,
    _gActor00300Actor100300Animation1369CIndices,
    { NULL, _gActor00300Actor100300Animation1369CBank1, NULL, NULL, _gActor00300Actor100300Animation1369CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor00300Actor100300Animation13C00Bank1[7] = {
#include "assets/actor_100300_animation_13C00_bank1.inc"
};

static AnimationPackedRotation _gActor00300Actor100300Animation13C00Bank4[108] = {
#include "assets/actor_100300_animation_13C00_bank4.inc"
};

static AnimationRecord _gActor00300Actor100300Animation13C00Records[196] = {
#include "assets/actor_100300_animation_13C00_records.inc"
};

static u16 _gActor00300Actor100300Animation13C00Indices[20] = {
#include "assets/actor_100300_animation_13C00_indices.inc"
};

static AnimationSet _gActor00300Actor100300Animation13C00 = {
    _gActor00300Actor100300Animation13C00Records,
    _gActor00300Actor100300Animation13C00Indices,
    { NULL, _gActor00300Actor100300Animation13C00Bank1, NULL, NULL, _gActor00300Actor100300Animation13C00Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor00300Actor100300Animation13DE4Bank1[2] = {
#include "assets/actor_100300_animation_13DE4_bank1.inc"
};

static AnimationPackedRotation _gActor00300Actor100300Animation13DE4Bank4[19] = {
#include "assets/actor_100300_animation_13DE4_bank4.inc"
};

static AnimationRecord _gActor00300Actor100300Animation13DE4Records[76] = {
#include "assets/actor_100300_animation_13DE4_records.inc"
};

static u16 _gActor00300Actor100300Animation13DE4Indices[20] = {
#include "assets/actor_100300_animation_13DE4_indices.inc"
};

static AnimationSet _gActor00300Actor100300Animation13DE4 = {
    _gActor00300Actor100300Animation13DE4Records,
    _gActor00300Actor100300Animation13DE4Indices,
    { NULL, _gActor00300Actor100300Animation13DE4Bank1, NULL, NULL, _gActor00300Actor100300Animation13DE4Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor00300Actor100300Animation13FC0Bank1[2] = {
#include "assets/actor_100300_animation_13FC0_bank1.inc"
};

static AnimationPackedRotation _gActor00300Actor100300Animation13FC0Bank4[17] = {
#include "assets/actor_100300_animation_13FC0_bank4.inc"
};

static AnimationRecord _gActor00300Actor100300Animation13FC0Records[76] = {
#include "assets/actor_100300_animation_13FC0_records.inc"
};

static u16 _gActor00300Actor100300Animation13FC0Indices[20] = {
#include "assets/actor_100300_animation_13FC0_indices.inc"
};

static AnimationSet _gActor00300Actor100300Animation13FC0 = {
    _gActor00300Actor100300Animation13FC0Records,
    _gActor00300Actor100300Animation13FC0Indices,
    { NULL, _gActor00300Actor100300Animation13FC0Bank1, NULL, NULL, _gActor00300Actor100300Animation13FC0Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor00300Actor100300Animation14268Bank1[4] = {
#include "assets/actor_100300_animation_14268_bank1.inc"
};

static AnimationPackedRotation _gActor00300Actor100300Animation14268Bank4[55] = {
#include "assets/actor_100300_animation_14268_bank4.inc"
};

static AnimationRecord _gActor00300Actor100300Animation14268Records[83] = {
#include "assets/actor_100300_animation_14268_records.inc"
};

static u16 _gActor00300Actor100300Animation14268Indices[20] = {
#include "assets/actor_100300_animation_14268_indices.inc"
};

static AnimationSet _gActor00300Actor100300Animation14268 = {
    _gActor00300Actor100300Animation14268Records,
    _gActor00300Actor100300Animation14268Indices,
    { NULL, _gActor00300Actor100300Animation14268Bank1, NULL, NULL, _gActor00300Actor100300Animation14268Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor00300Actor100300Animation14C8CBank1[2] = {
#include "assets/actor_100300_animation_14C8C_bank1.inc"
};

static AnimationPackedRotation _gActor00300Actor100300Animation14C8CBank4[281] = {
#include "assets/actor_100300_animation_14C8C_bank4.inc"
};

static AnimationRecord _gActor00300Actor100300Animation14C8CRecords[342] = {
#include "assets/actor_100300_animation_14C8C_records.inc"
};

static u16 _gActor00300Actor100300Animation14C8CIndices[20] = {
#include "assets/actor_100300_animation_14C8C_indices.inc"
};

static AnimationSet _gActor00300Actor100300Animation14C8C = {
    _gActor00300Actor100300Animation14C8CRecords,
    _gActor00300Actor100300Animation14C8CIndices,
    { NULL, _gActor00300Actor100300Animation14C8CBank1, NULL, NULL, _gActor00300Actor100300Animation14C8CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor00300Actor100300Animation15594Bank1[20] = {
#include "assets/actor_100300_animation_15594_bank1.inc"
};

static AnimationPackedRotation _gActor00300Actor100300Animation15594Bank4[219] = {
#include "assets/actor_100300_animation_15594_bank4.inc"
};

static AnimationRecord _gActor00300Actor100300Animation15594Records[279] = {
#include "assets/actor_100300_animation_15594_records.inc"
};

static u16 _gActor00300Actor100300Animation15594Indices[20] = {
#include "assets/actor_100300_animation_15594_indices.inc"
};

static AnimationSet _gActor00300Actor100300Animation15594 = {
    _gActor00300Actor100300Animation15594Records,
    _gActor00300Actor100300Animation15594Indices,
    { NULL, _gActor00300Actor100300Animation15594Bank1, NULL, NULL, _gActor00300Actor100300Animation15594Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor00300Actor100300Animation15FB0Bank1[19] = {
#include "assets/actor_100300_animation_15FB0_bank1.inc"
};

static AnimationPackedRotation _gActor00300Actor100300Animation15FB0Bank4[253] = {
#include "assets/actor_100300_animation_15FB0_bank4.inc"
};

static AnimationRecord _gActor00300Actor100300Animation15FB0Records[317] = {
#include "assets/actor_100300_animation_15FB0_records.inc"
};

static u16 _gActor00300Actor100300Animation15FB0Indices[20] = {
#include "assets/actor_100300_animation_15FB0_indices.inc"
};

static AnimationSet _gActor00300Actor100300Animation15FB0 = {
    _gActor00300Actor100300Animation15FB0Records,
    _gActor00300Actor100300Animation15FB0Indices,
    { NULL, _gActor00300Actor100300Animation15FB0Bank1, NULL, NULL, _gActor00300Actor100300Animation15FB0Bank4, NULL, NULL, NULL },
};

DamageAttack Actor00300_D15FD8[4] = {
    { 25, 8 },
    { 35, 3 },
    { 35, 2 },
    { 35, 1 },
};

EnemyParams Actor00300_D15FE8 = { Actor00300_D15FD8, 400, 105, 158, 8, 100, 10, 100, 7 };

s16 Actor00300_D15FF8[4] = {
    0,
    98,
    150,
    120,
};

s16 Actor00300_D16000[16] = {
    0,
    1,
    1,
    1,
    1,
    2,
    2,
    2,
    2,
    2,
    2,
    3,
    3,
    3,
    3,
    3,
};

_Actor00300PatrolRoom Actor00300_D16020[15] = {
    { 1, 4, 2, 4 },
    { 2, 4, 8, 2 },
    { 3, 4, 9, 2 },
    { 4, 4, 11, 2 },
    { 5, 4, 12, 4 },
    { 6, 4, 15, 2 },
    { 7, 4, 24, 2 },
    { 8, 4, 27, 2 },
    { 9, 4, 35, 4 },
    { 10, 4, 42, 2 },
    { 11, 4, 44, 2 },
    { 12, 5, 10, 2 },
    { 13, 2, 29, 4 },
    { 14, 3, 2, 2 },
    { 0, 0, 0, 0 },
};

SVECTOR Actor00300_D16098[4] = {
    { 0x413C, 0, 1500, 0 },
    { 0x413C, 0, 7600, 0 },
    { 1800, 0, 7600, 0 },
    { 1800, 0, 1500, 0 },
};

SVECTOR Actor00300_D160B8[4] = {
    { 0x2710, 0, 6000, 0 },
    { 7800, 0, 6000, 0 },
    { 0x2710, 0, 6000, 0 },
    { 7800, 0, 6000, 0 },
};

SVECTOR Actor00300_D160D8[2] = {
    { 1500, 0, 0x2710, 0 },
    { 1500, 0, 3500, 0 },
};

SVECTOR Actor00300_D160E8[2] = {
    { 3000, 0, 3500, 0 },
    { 0x2904, 0, 3500, 0 },
};

SVECTOR Actor00300_D160F8[2] = {
    { -9000, 0, 0, 0 },
    { -2000, 0, 0, 0 },
};

SVECTOR Actor00300_D16108[2] = {
    { 7500, 0, 0, 0 },
    { -1000, 0, 0, 0 },
};

SVECTOR Actor00300_D16118[2] = {
    { -1000, 0, 1300, 0 },
    { 4000, 0, 1300, 0 },
};

SVECTOR Actor00300_D16128[2] = {
    { 4000, 0, -1300, 0 },
    { -1000, 0, -1300, 0 },
};

SVECTOR Actor00300_D16138[4] = {
    { 2000, 0, -3500, 0 },
    { 2000, 0, 4000, 0 },
    { -2000, 0, 4000, 0 },
    { 2000, 0, 4000, 0 },
};

SVECTOR Actor00300_D16158[2] = {
    { 0, 0, -8000, 0 },
    { 0, 0, -2000, 0 },
};

SVECTOR Actor00300_D16168[2] = {
    { -2000, 0, -0x2710, 0 },
    { 2000, 0, -0x2710, 0 },
};

SVECTOR Actor00300_D16178[2] = {
    { 1000, 0, 80, 0 },
    { 6000, 0, 80, 0 },
};

SVECTOR Actor00300_D16188[2] = {
    { 6500, 0, 0, 0 },
    { 6500, 0, 3300, 0 },
};

SVECTOR Actor00300_D16198[2] = {
    { 4000, 0, 0, 0 },
    { -4000, 0, 0, 0 },
};

SVECTOR Actor00300_D161A8[4] = {
    { 1700, 0, -1700, 0 },
    { 4500, 0, -1700, 0 },
    { 1700, 0, -1700, 0 },
    { 1700, 0, -7000, 0 },
};

SVECTOR Actor00300_D161C8[2] = {
    { 0, 0, -2000, 0 },
    { -5000, 0, -2000, 0 },
};

SVECTOR Actor00300_D161D8[2] = {
    { 1000, 0, -2000, 0 },
    { 6000, 0, -2000, 0 },
};

SVECTOR Actor00300_D161E8[2] = {
    { -6500, -2000, 9000, 0 },
    { 1000, -2000, 9000, 0 },
};

SVECTOR Actor00300_D161F8[2] = {
    { 0x2710, 0, 0x2CEC, 0 },
    { 4500, 0, 0x2CEC, 0 },
};

SVECTOR Actor00300_D16208[2] = {
    { 0x2C24, 0, 0x2710, 0 },
    { 0x2C24, 0, 4500, 0 },
};

SVECTOR Actor00300_D16218[4] = {
    { -6000, 0, -4000, 0 },
    { -6000, 0, 2000, 0 },
    { -0x2CEC, 0, 2000, 0 },
    { -6000, 0, 2000, 0 },
};

SVECTOR Actor00300_D16238[4] = {
    { -6000, 0, 4500, 0 },
    { -6000, 0, 0x2904, 0 },
    { -1000, 0, 0x2904, 0 },
    { -6000, 0, 0x2904, 0 },
};

SVECTOR Actor00300_D16258[2] = {
    { -2100, 0, -2100, 0 },
    { -1600, 0, 4000, 0 },
};

SVECTOR Actor00300_D16268[2] = {
    { -4700, 0, 1750, 0 },
    { -1000, 0, 6300, 0 },
};

SVECTOR* Actor00300_D16278[15][2] = {
    { NULL, NULL },
    { Actor00300_D16098, Actor00300_D160B8 },
    { Actor00300_D160D8, Actor00300_D160E8 },
    { Actor00300_D160F8, Actor00300_D16108 },
    { Actor00300_D16118, Actor00300_D16128 },
    { Actor00300_D16138, NULL },
    { Actor00300_D16158, Actor00300_D16168 },
    { Actor00300_D16178, NULL },
    { Actor00300_D16188, Actor00300_D16198 },
    { Actor00300_D161A8, NULL },
    { Actor00300_D161C8, Actor00300_D161D8 },
    { Actor00300_D161E8, NULL },
    { Actor00300_D161F8, Actor00300_D16208 },
    { Actor00300_D16218, Actor00300_D16238 },
    { Actor00300_D16258, Actor00300_D16268 },
};

TaskDesc Actor00300_D162F0[3] = {
    { { { TASK_BODY_TMD, 96 } }, Actor00300_Fn04770, { .model = &_gActor00300StingerBody } },
    { { { TASK_BODY_TMD, 96 } }, _actor00300DrainModelTask, { .model = &_gActor00300Actor100300Model09FA0 } },
    { { { TASK_BODY_COORD, 96 } }, Actor00300_Fn0521C, { .value = 0 } },
};

TaskMessageEntry Actor00300_D16314[5] = {
    { ACTOR_MESSAGE_PLAY_ANIMATION, _actor00300MsgPlayAnimation },
    { ACTOR_MESSAGE_PLACE, actorMsgPlaceRotMatrix },
    { ACTOR_MESSAGE_SET_MODEL_DRAW, _actor00300MsgSetModelDraw },
    { ACTOR_COMMAND_MESSAGE_APPLY, _actor00300MsgApplyCommand },
    { TASK_MESSAGE_TABLE_END, NULL },
};

AnimationSet* Actor00300_D1633C[22] = {
    NULL,
    &_gActor00300Actor100300Animation0CAD4,
    &_gActor00300Actor100300Animation0D8A8,
    &_gActor00300Actor100300Animation0E028,
    &_gActor00300Actor100300Animation0ECF8,
    &_gActor00300Actor100300Animation0F1A8,
    &_gActor00300Actor100300Animation0F5E4,
    &_gActor00300Actor100300Animation0FF4C,
    &_gActor00300Actor100300Animation1065C,
    &_gActor00300Actor100300Animation10FBC,
    &_gActor00300Actor100300Animation118C4,
    &_gActor00300Actor100300Animation11F30,
    &_gActor00300Actor100300Animation12754,
    &_gActor00300Actor100300Animation12E24,
    &_gActor00300Actor100300Animation1369C,
    &_gActor00300Actor100300Animation13C00,
    &_gActor00300Actor100300Animation13DE4,
    &_gActor00300Actor100300Animation13FC0,
    &_gActor00300Actor100300Animation14268,
    &_gActor00300Actor100300Animation14C8C,
    &_gActor00300Actor100300Animation15594,
    &_gActor00300Actor100300Animation15FB0,
};

s16 Actor00300_D16394[18] = {
    0,
    8,
    8,
    4,
    4,
    0,
    0,
    3,
    2,
    2,
    2,
    0,
    0,
    0,
    0,
    8,
    0,
    0,
};

/// Scratch-stack block of the tick that resolves the enemy's contact tables.
typedef struct {
    WorldCollisionDelta delta;  // push-out of the grid and hit contacts; its `vector` view then holds the offset to an attacker, and the offset of body part 3 from a pushing contact's point
    VECTOR              normal; // `delta.vector` of the deepest pushing contact, normalized to 4096
    SVECTOR             from;   // start of the sight segment: the player's part 3; before that, the offset the drain's halo effect is spawned at
    SVECTOR             to;     // end of the sight segment: the enemy's root
    VECTOR              push;   // `normal` taken out of view space into the space of the root's translation
} _Actor00300HitScratch;
STATIC_ASSERT_SIZEOF(_Actor00300HitScratch, 0x40);

static TmdSource _gActor00300BrainStingerBurstHead;

static TmdSource _gActor00300Actor100300Model0ABC4;

static TmdSource _gActor00300Actor100300Model0B128;

static TmdSource _gActor00300Actor100300Model0B8AC;

static TmdSource _gActor00300Actor100300Model0BFC0;

extern void* D_80067704[1];

static inline s16      _actor00300TiltMagnitude(s8 randomByte);
static void            Actor00300_Fn03618(Task* arg0);
static __inline__ void _actor00300UpdateDrainModel(Task* task);
static void            _actor00300DrainModelActive(Enemy* enemy, Task* task);
static void            _actor00300InitFireball(Enemy* enemy, Task* task);
static void            Actor00300_Fn04370(Enemy* arg0, Task* arg1);

#include "../../shared/fireball_glow.inc.c"

#include "../../shared/fireball_ground_glow.inc.c"

static void Actor00300_Fn00970(Enemy* enemy, Task* task)
{
    GameLocationKey        key;
    Enemy*                 child;
    WorldCollisionContact* sightContacts;
    WorldCollisionContact* hitContacts;
    WorldCollisionContact* gridContacts;
    WorldCollisionContact* drainContacts;
    _Actor00300Work*       work;
    TmdObject*             model;
    s32                    areaIndex;
    Task*                  childTask;
    s32                    slot;
    u32                    index;
    u32                    rawId;
    u8                     areaByte0;
    GameLocationKey*       sessionKey;
    AreaPlacement*         entry;
    TmdObject*             obj;
    GfxCoord*              coord;
    GfxCoord*              parts;

    obj   = task->extra.tmd;
    coord = obj->coords;
    work  = memCalloc(sizeof(_Actor00300Work), 0);
    if (work == NULL) {
        enemyDestroy(enemy, task);
        return;
    }
    task->work             = work;
    work->patrolPoints     = NULL;
    work->patrolPointCount = 0;
    for (areaIndex = 0; Actor00300_D16020[areaIndex].routeRow != 0; areaIndex++) {
        if ((gGameSession->location.loc.stage == Actor00300_D16020[areaIndex].stage) &&
            (gGameSession->location.loc.area == Actor00300_D16020[areaIndex].area)) {
            work->patrolPoints =
                Actor00300_D16278[Actor00300_D16020[areaIndex].routeRow]
                                 [enemy->place->mode];
            work->patrolPointCount = Actor00300_D16020[areaIndex].pointCount;
        }
    }
    work->notLockable   = enemy->place->variant;
    obj->flags          = 0;
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
    obj->lightMtx       = &work->lightMtx;
    obj->colorMtx       = &work->colorMtx;
    enemy->field_4      = &coord->coord;
    enemy->field_48     = 0;
    worldTargetLinkNode(&enemy->node);
    slot                          = 1;
    parts                         = task->extra.tmd->coords;
    enemy->bodyPos.vx             = 0;
    enemy->bodyPos.vy             = 0;
    enemy->bodyPos.vz             = 0;
    enemy->param                  = &Actor00300_D15FE8;
    enemy->recs                   = work->hitContacts;
    enemy->coord                  = parts + 3;
    enemy->hp                     = (s16)Actor00300_D15FE8.hpMax;
    work->hitEffectArg.coord      = task->extra.tmd->coords + 3;
    work->hitEffectArg.spawnArgLo = 0x300;
    work->hitEffectArg.spawnArgHi = 2;
    animationInitContext(&work->rig.anim, Actor00300_D1633C, obj,
                         work->rig.poses, work->rig.slots);
    do {
        animationResetSlot(&work->rig.anim, slot, 1);
        slot += 1;
    } while (slot < ARRAY_SIZE(work->rig.slots));
    (sceneAcquireBattleRef)(0);
    work->savedRootMtx = coord->coord;
    work->timer        = 0xA;
    work->mp           = ACTOR_00300_MP_AT_SPAWN;
    child              = enemySpawnFromTable(Actor00300_D162F0, 1, 0, enemy);
    rawId              = enemy->placeKey;
    model              = child->task->extra.tmd;
    sessionKey         = &gGameSession->location.loc;
    key.stage          = sessionKey->stage;
    key.area           = sessionKey->area;
    key.room           = sessionKey->room;
    areaByte0          = gGameSession->location.loc.view;
    index              = rawId >> 12;
    key.view           = areaByte0;
    areaSyncLocationVariant(&key);
    entry =
        gpAreaPlaceAt(Gp_GetNestedAreaRec(&key)->placements, index);
    model->texturePageOffset = entry->texturePageOffset;
    model->clutRowOffset     = entry->clutRowOffset;
    if (model->buffer != NULL) {
        tmdBuildBufferHalf(model);
        tmdBuildBufferHalf(model);
    }
    childTask                     = child->task;
    work->sightCapsule.ends[0].vz = 0x2328;
    work->sightCapsule.end0Radius = 0xFA0;
    sightContacts                 = work->sightContacts;
    work->sightCapsule.ends[0].vx = 0;
    work->sightCapsule.ends[0].vy = 0;
    work->sightCapsule.ends[1].vx = 0;
    work->sightCapsule.ends[1].vy = 0;
    work->sightCapsule.ends[1].vz = 0;
    work->sightCapsule.end1Radius = 0x3E8;
    work->sightCapsule.contacts   = sightContacts;
    work->drainModelTask          = childTask;
    work->sightBody.coord =
        task->extra.tmd->coords + 2;
    work->sightBody.context.capsule = &work->sightCapsule;
    work->sightBody.pos.vx          = 0;
    work->sightBody.pos.vy          = 0;
    work->sightBody.pos.vz          = 0;
    work->sightBody.key             = 0;
    work->sightBody.radius          = 0;
    work->sightBody.flags           = (u32)WORLD_COLLISION_BODY_CAPSULE;
    worldCollisionLinkBody(WORLD_COLLISION_LIST_ENEMY_ATTACKS, &work->sightBody);
    worldCollisionInitContacts(sightContacts, ARRAY_SIZE(work->sightContacts), 0);
    hitContacts                    = work->hitContacts;
    work->sightBody.flags          = (u16)(work->sightBody.flags | (WORLD_COLLISION_BODY_CLIP_TO_GRID_CONTACT | WORLD_COLLISION_BODY_SINGLE_CONTACT | WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED));
    work->hitBody.coord            = task->extra.tmd->coords + 3;
    work->hitBody.context.contacts = hitContacts;
    work->hitBody.pos.vx           = 0;
    work->hitBody.pos.vy           = 0;
    work->hitBody.pos.vz           = 0;
    work->hitBody.key              = 0x30003;
    work->hitBody.radius           = 0x15E;
    work->hitBody.flags            = (u32)WORLD_COLLISION_BODY_SPHERE;
    worldCollisionLinkBody(WORLD_COLLISION_LIST_ENEMY_BODIES, &work->hitBody);
    worldCollisionInitContacts(hitContacts, ARRAY_SIZE(work->hitContacts), 0);
    work->hitBody.flags             = (u16)(work->hitBody.flags | WORLD_COLLISION_BODY_PAIR_ENABLED);
    work->gridBody.coord            = task->extra.tmd->coords;
    gridContacts                    = work->gridContacts;
    work->gridBody.key              = 0x30003;
    work->gridBody.context.contacts = gridContacts;
    work->gridBody.pos.vx           = 0;
    work->gridBody.pos.vy           = -0x1F4;
    work->gridBody.pos.vz           = 0;
    work->gridBody.radius           = 0x1F4;
    work->gridBody.flags            = (u32)WORLD_COLLISION_BODY_SPHERE;
    worldCollisionLinkBody(WORLD_COLLISION_LIST_ENEMY_BODIES, &work->gridBody);
    worldCollisionInitContacts(gridContacts, ARRAY_SIZE(work->gridContacts), 0);
    work->gridBody.flags             = (u16)(work->gridBody.flags | (WORLD_COLLISION_BODY_FLOOR_QUERY | WORLD_COLLISION_BODY_GRID_ENABLED));
    work->drainBody.coord            = child->task->extra.tmd->coords;
    drainContacts                    = work->drainContacts;
    work->drainBody.context.contacts = drainContacts;
    work->drainBody.pos.vx           = -0x1F4;
    work->drainBody.pos.vy           = 0x1F4;
    work->drainBody.pos.vz           = 0;
    work->drainBody.key              = damagePackAttackKey(Actor00300_D15FD8, 0);
    work->drainBody.radius           = 0x2BC;
    work->drainBody.flags            = (u32)WORLD_COLLISION_BODY_SPHERE;
    worldCollisionLinkBody(WORLD_COLLISION_LIST_ENEMY_ATTACKS, &work->drainBody);
    worldCollisionInitContacts(drainContacts, ARRAY_SIZE(work->drainContacts), 0);
    gStageSceneMusicEntry = 0xA;
    work->drainBody.flags = (u16)(work->drainBody.flags & (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED));
    task->msgTable        = Actor00300_D16314;
    task->state           = 1;
}

/// Copies the world positions of two coordinates into the scratch block and
/// runs `_playerDetectionSegmentOccluded` on the segment between them. When it reports no
/// hit, `alertTimer` is rearmed to `ACTOR_00300_ALERT_TICKS` and `playerSeen`
/// raised; otherwise `alertTimer` is cleared.
#define _ACTOR00300_TEST_SIGHT_LINE(work, scratch, start, end)                        \
    do {                                                                              \
        (scratch)->from.vx = (start)->workm.t[0];                                     \
        (scratch)->from.vy = (start)->workm.t[1];                                     \
        (scratch)->from.vz = (start)->workm.t[2];                                     \
        (scratch)->to.vx   = (end)->workm.t[0];                                       \
        (scratch)->to.vy   = (end)->workm.t[1];                                       \
        (scratch)->to.vz   = (end)->workm.t[2];                                       \
        if (_playerDetectionSegmentOccluded(&(scratch)->from, &(scratch)->to) != 0) { \
            (work)->alertTimer = 0;                                                   \
        } else {                                                                      \
            (work)->alertTimer = ACTOR_00300_ALERT_TICKS;                             \
            (work)->playerSeen = 1;                                                   \
        }                                                                             \
    } while (0)

/// Maps a random byte to a hit-twist magnitude of 64..191 angle units.
///
/// Uses the low seven bits even for a signed input (4096 units per turn).
/// The hit reaction chooses the sign separately from the same random byte.
static inline s16 _actor00300TiltMagnitude(s8 randomByte)
{
    enum { ACTOR_00300_HIT_TWIST_RANDOM_MASK = 0x7F,
           ACTOR_00300_HIT_TWIST_MIN         = 64 };

    return (randomByte & ACTOR_00300_HIT_TWIST_RANDOM_MASK) + ACTOR_00300_HIT_TWIST_MIN;
}

static void Actor00300_Fn00E54(Task* arg0)
{
    _Actor00300Work*       work;
    _Actor00300HitScratch* head;
    _Actor00300HitScratch* scratch;
    Enemy*                 enemy;
    GfxCoord*              self;
    GfxCoord*              other;
    s32                    maxPush;
    s32                    critical;
    u32                    lastId;
    s32                    i;
    s32                    dz;
    s32                    val;
    s32                    x;
    s32                    y;
    s32                    z;
    s32                    push;
    s32                    clamped;
    s16                    cooldown;
    s16                    rng;
    s32                    bit;
    u32                    random;
    s32                    tilt;
    s32                    byte1;

    maxPush  = 0;
    critical = 0;
    lastId   = 0;
    work     = arg0->work;
    head     = SCRATCH_STACK_CURSOR(_Actor00300HitScratch);
    self     = arg0->extra.tmd->coords;
    SCRATCH_STACK_RESERVE_BLOCK(_Actor00300HitScratch);
    scratch = SCRATCH_STACK_CURSOR(_Actor00300HitScratch);
    enemy   = arg0->spawnArg2.pointer;

    switch (worldCollisionResolvePushback(work->gridContacts, &scratch->delta, ARRAY_SIZE(work->gridContacts), NULL)) {
        case WORLD_COLLISION_PUSHBACK_NO_GRID_HIT:
            break;
        case WORLD_COLLISION_PUSHBACK_GRID_HIT:
            self->coord.t[0] += head[-1].delta.fixed.vx.halves.integer;
            self->coord.t[1] += scratch->delta.fixed.vy.halves.integer;
            self->coord.t[2] += scratch->delta.fixed.vz.halves.integer;
            break;
        case WORLD_COLLISION_PUSHBACK_OPPOSED:
            self->coord.t[0] = work->prevPos.vx;
            self->coord.t[1] = work->prevPos.vy;
            self->coord.t[2] = work->prevPos.vz;
            break;
    }
    worldCollisionClearContacts(work->gridContacts);

    if (work->hitBody.flags & WORLD_COLLISION_BODY_GRID_ENABLED) {
        switch (worldCollisionResolvePushback(work->hitContacts, &scratch->delta, ARRAY_SIZE(work->hitContacts), NULL)) {
            case WORLD_COLLISION_PUSHBACK_NO_GRID_HIT:
                break;
            case WORLD_COLLISION_PUSHBACK_GRID_HIT:
                self->coord.t[0] += scratch->delta.fixed.vx.halves.integer;
                self->coord.t[2] += scratch->delta.fixed.vz.halves.integer;
                break;
            case WORLD_COLLISION_PUSHBACK_OPPOSED:
                self->coord.t[0] = work->prevPos.vx;
                self->coord.t[2] = work->prevPos.vz;
                break;
        }
    }

    if (work->hitCooldown != 0) {
        if (--work->hitCooldown <= 0 && enemy->hp > 0) {
            work->hitCooldown = 0;
        }
    }

    for (i = 0; i < ARRAY_SIZE(work->hitContacts); i++) {
        switch ((u32)work->hitContacts[i].key.value >> 16) {
            case 0:
            case 1:
                break;
            case 2:
                if (work->hitCooldown != 0) {
                    break;
                }
                switch (damageGetPlayerAttackReaction(work->hitContacts[i].key.value) & 0xFFFF) {
                    case DAMAGE_PLAYER_REACTION_STAGGER:
                        if (work->knockDown == 0) {
                            work->knockDown = 1;
                        }
                        break;
                    case DAMAGE_PLAYER_REACTION_BUILDUP:
                        damageStartEnemyBuildup(enemy, work->hitContacts[i].key.value, 0);
                        break;
                    case DAMAGE_PLAYER_REACTION_POISON:
                        damageTryStartEnemyDamageOverTime(enemy, work->hitContacts[i].key.value, 0);
                        break;
                    case 4:
                        work->burstStage = 1;
                        break;
                    case DAMAGE_PLAYER_REACTION_EXPLOSION:
                        work->burstStage = 1;
                        break;
                    case DAMAGE_PLAYER_REACTION_INCENDIARY:
                        critical = 1;
                        break;
                    case DAMAGE_PLAYER_REACTION_NONE:
                    case 5:
                    case 8:
                    case 9:
                        break;
                }
                other                    = gPlayerActorTasks[(u8)work->hitContacts[i].key.value >> 7]->extra.tmd->coords;
                scratch->delta.vector.vx = other->coord.t[0] - self->coord.t[0];
                scratch->delta.vector.vy = other->coord.t[1] - self->coord.t[1];
                dz                       = other->coord.t[2] - self->coord.t[2];
                scratch->delta.vector.vz = dz;
                val                      = (scratch->delta.vector.vx * self->coord.m[0][2]) + (scratch->delta.vector.vy * self->coord.m[1][2]) + (dz * self->coord.m[2][2]);
                work->hitFromFront       = val >= 0;
                work->hitDamage          = damageComputePlayerAttack(work->hitContacts[i].key.value,
                                                                     SquareRoot0((scratch->delta.vector.vx * scratch->delta.vector.vx) + (scratch->delta.vector.vy * scratch->delta.vector.vy) + (scratch->delta.vector.vz * scratch->delta.vector.vz)),
                                                                     0, 0);
                if (critical != 0) {
                    work->hitDamage >>= 1;
                } else if (damageRollCriticalHit(enemy, work->hitContacts[i].key.value, 0) != 0) {
                    work->hitDamage *= 4;
                    effectSpawn(EFFECT_CRITICAL_HIT, &arg0->extra.tmd->coords[3], 0, NULL);
                }
                damageAccumulateLifeDrainHp(enemy, work->hitContacts[i].key.value, work->hitDamage, 0);
                worldTargetAddReadoutAmount(&enemy->node, work->hitDamage, 0);
                enemy->hp -= work->hitDamage;
                if (enemy->hp <= 0) {
                    if (work->burstStage == 0) {
                        work->action = ACTOR_00300_ACTION_HURT;
                        if ((u16)(work->anim - ACTOR_00300_ANIM_KNOCK_DOWN_BACK) < 2) {
                            work->actionStep = 2;
                        } else {
                            work->actionStep = 0;
                        }
                    } else {
                        work->action     = ACTOR_00300_ACTION_DEAD;
                        work->actionStep = 0;
                        arg0->state      = 2;
                    }
                } else {
                    work->burstStage = 0;
                    if ((work->hitDamage >= 0x50 || work->knockDown != 0) && work->action != ACTOR_00300_ACTION_HURT) {
                        work->action     = ACTOR_00300_ACTION_HURT;
                        work->actionStep = 0;
                    } else {
                        random          = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                        rng             = random >> 16;
                        tilt            = _actor00300TiltMagnitude(rng);
                        bit             = rng & 1;
                        gRandomLcgState = random;
                        if (!bit) {
                            tilt = -tilt;
                        }
                        work->hitTwist.vx = tilt;
                        byte1             = rng >> 8;
                        val               = _actor00300TiltMagnitude(byte1);
                        if (!(byte1 & 1)) {
                            val = -val;
                        }
                        work->hitTwist.vy    = val;
                        work->hitTwistActive = 1;
                    }
                }
                if (lastId != work->hitContacts[i].key.value) {
                    lastId = work->hitContacts[i].key.value;
                    effectSpawnHit(damageGetPlayerAttackEffectId(lastId), &arg0->extra.tmd->coords[3], NULL, &work->hitEffectArg);
                }
                cooldown = damageGetPlayerAttackHitCooldown(work->hitContacts[i].key.value);
                if (cooldown > 0) {
                    work->hitCooldown = cooldown;
                }
                break;
            case 3:
                other                    = &arg0->extra.tmd->coords[3];
                x                        = other->workm.t[0] - work->hitContacts[i].point.vx;
                scratch->delta.vector.vx = x;
                y                        = other->workm.t[1] - work->hitContacts[i].point.vy;
                scratch->delta.vector.vy = y;
                z                        = other->workm.t[2] - work->hitContacts[i].point.vz;
                scratch->delta.vector.vz = z;
                push                     = work->hitContacts[i].distance - SquareRoot0((x * x) + (y * y) + (z * z));
                clamped                  = push;
                if (push <= 0) {
                    clamped = 0;
                }
                push = clamped;
                if (maxPush < push) {
                    maxPush = push;
                    VectorNormal(&scratch->delta.vector, &scratch->normal);
                    ApplyTransposeMatrixLV(&Gp_GridParams->viewCoord->workm, &scratch->normal, &scratch->push);
                }
                break;
        }
    }

    if (maxPush > 0) {
        self->coord.t[0] += (maxPush * scratch->push.vx) >> 12;
        self->coord.t[2] += (maxPush * scratch->push.vz) >> 12;
    }
    worldCollisionClearContacts(work->hitContacts);
    if (work->drainContacts[0].flags & WORLD_COLLISION_CONTACT_OCCUPIED) {
        work->drainBody.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        worldCollisionClearContacts(work->drainContacts);
        playerStateSpendMp(ACTOR_00300_MP_DRAINED);
        work->mp        += ACTOR_00300_MP_DRAINED;
        scratch->from.vx = -0x1F4;
        scratch->from.vy = 0x1F4;
        scratch->from.vz = 0;
        effectSpawn(gRoomEffectHaloId, work->drainModelTask->extra.tmd->coords, 0x20001, &scratch->from);
    }
    work->playerSeen = 0;
    if ((work->sightContacts[0].key.value & 0xFFFF0000) == 0x10000) {
        other = &gameGetTaskSlot(GAME_TASK_SLOT_PLAYER)->extra.tmd->coords[3];
        _ACTOR00300_TEST_SIGHT_LINE(work, scratch, other, self);
    } else if (work->alertTimer > 0) {
        work->alertTimer--;
    }
    worldCollisionClearContacts(work->sightContacts);
    SCRATCH_STACK_RELEASE_BLOCK(_Actor00300HitScratch);
}

/// Advances the room patrol and switches to pursuit when the actor is alerted.
///
/// Requires a live enemy model and a nonempty route with `patrolPoint` in range.
/// The wait, turn and move stages use parent-coordinate X/Z; headings use 4096
/// units per turn. Borrows one `VECTOR` scratch block for the point delta.
static void _actor00300TickPatrol(Task* task)
{
    enum { ACTOR_00300_PATROL_WAIT      = 0,
           ACTOR_00300_PATROL_TURN      = 1,
           ACTOR_00300_PATROL_MOVE      = 2,
           ACTOR_00300_PATROL_TURN_RATE = 40,
           ACTOR_00300_PATROL_SPEED     = 25 };

    _Actor00300Work* work;
    GfxCoord*        coord;
    s16              timer;
    s16              nextPoint;
    s32              step;
    s16              nextStep;
    s32              deltaX;
    s32              deltaZ;
    u16              nextYaw;
    u16              turnYaw;
    VECTOR*          routeDelta;
    VECTOR*          stackTop;

    stackTop                     = SCRATCH_STACK_CURSOR(VECTOR);
    routeDelta                   = stackTop - 1;
    SCRATCH_STACK_CURSOR(VECTOR) = routeDelta;
    work                         = task->work;
    step                         = work->actionStep;
    coord                        = task->extra.tmd->coords;
    switch (step) {
        case ACTOR_00300_PATROL_WAIT:
            work->turnRate = 0;
            work->speed    = 0;
            work->anim     = ACTOR_00300_ANIM_STAND;
            timer          = (u16)work->timer - 1;
            work->timer    = timer;
            if (timer <= 0) {
                routeDelta->vx  = (s32)(work->patrolPoints[work->patrolPoint].vx - coord->coord.t[0]);
                routeDelta->vy  = 0;
                routeDelta->vz  = (s32)(work->patrolPoints[work->patrolPoint].vz - coord->coord.t[2]);
                nextYaw         = ratan2((s16)routeDelta->vx, (s16)routeDelta->vz) & ACTOR_TRANSFORM_ANGLE_MASK;
                nextStep        = ACTOR_00300_PATROL_TURN;
                work->targetYaw = nextYaw;
                if (work->yaw == nextYaw) {
                    nextStep = ACTOR_00300_PATROL_MOVE;
                }
                work->actionStep = nextStep;
                work->timer      = 0;
            }
            break;
        case ACTOR_00300_PATROL_TURN:
            work->turnRate  = ACTOR_00300_PATROL_TURN_RATE;
            work->anim      = ACTOR_00300_ANIM_TURN;
            work->speed     = 0;
            routeDelta->vx  = (s32)(work->patrolPoints[work->patrolPoint].vx - coord->coord.t[0]);
            routeDelta->vy  = 0;
            routeDelta->vz  = (s32)(work->patrolPoints[work->patrolPoint].vz - coord->coord.t[2]);
            turnYaw         = ratan2((s16)routeDelta->vx, (s16)routeDelta->vz) & ACTOR_TRANSFORM_ANGLE_MASK;
            work->targetYaw = turnYaw;
            if (work->yaw == turnYaw) {
                work->actionStep = ACTOR_00300_PATROL_MOVE;
            }
            break;
        case ACTOR_00300_PATROL_MOVE:
            work->turnRate  = ACTOR_00300_PATROL_TURN_RATE;
            work->speed     = ACTOR_00300_PATROL_SPEED;
            work->anim      = ACTOR_00300_ANIM_MOVE;
            routeDelta->vx  = (s32)(work->patrolPoints[work->patrolPoint].vx - coord->coord.t[0]);
            routeDelta->vy  = 0;
            routeDelta->vz  = (s32)(work->patrolPoints[work->patrolPoint].vz - coord->coord.t[2]);
            work->targetYaw = ratan2((s16)routeDelta->vx, (s16)routeDelta->vz) & ACTOR_TRANSFORM_ANGLE_MASK;
            deltaX          = routeDelta->vx;
            deltaZ          = routeDelta->vz;
            if (SquareRoot0((deltaX * deltaX) + (deltaZ * deltaZ)) <= work->speed) {
                coord->coord.t[0] = (s32)work->patrolPoints[work->patrolPoint].vx;
                coord->coord.t[2] = (s32)work->patrolPoints[work->patrolPoint].vz;
                work->speed       = 0;
                nextPoint         = (u16)work->patrolPoint + 1;
                work->patrolPoint = nextPoint;
                if (nextPoint >= work->patrolPointCount) {
                    work->patrolPoint = 0;
                }
                work->actionStep = ACTOR_00300_PATROL_TURN;
            }
            break;
    }
    // A sighting, group alert or hit interrupts the current patrol stage.
    if ((work->alertTimer != 0) || (gSceneCombatState.actor00300AttackAlert != 0) || (work->hitDamage != 0)) {
        work->action     = ACTOR_00300_ACTION_PURSUE;
        work->actionStep = ACTOR_00300_ACTION_BEGIN;
        work->alertTimer = ACTOR_00300_ALERT_TICKS;
        sceneEngageBattle(1);
        gSceneCombatState.actor00300AttackAlert = 0;
    }
    SCRATCH_STACK_RELEASE_BLOCK(VECTOR);
}

/// Alternates waiting and closing on the player, then selects an action when its attack timer expires.
///
/// Requires initialized enemy work and model coordinates. Losing the alert returns
/// to patrol; attack selection requires `playerSeen`. Distances are in root-parent
/// coordinate units and headings in 4096 units per turn. Borrows one `VECTOR`.
static void _actor00300TickPursuit(Task* task)
{
    enum { ACTOR_00300_PURSUIT_YAW_TOLERANCE    = 0x100,
           ACTOR_00300_PURSUIT_TURN_RATE        = 60,
           ACTOR_00300_PURSUIT_SPEED            = 25,
           ACTOR_00300_PURSUIT_MOVE_MIN_TICKS   = 30,
           ACTOR_00300_ATTACK_MIN_TICKS         = 60,
           ACTOR_00300_RETURN_PATROL_WAIT_TICKS = 10 };

    _Actor00300Work* work;
    GfxCoord*        coord;
    VECTOR*          stackTop;
    VECTOR*          playerDelta;
    s16              yawDelta;
    s16              targetYaw;
    s16              yawError;
    s16              timer;
    s16              wrappedError;
    s32              idleYawMagnitude;
    s32              movingYawMagnitude;
    s32              playerDistance;
    s32              randomValue;

    stackTop                     = SCRATCH_STACK_CURSOR(VECTOR);
    playerDelta                  = stackTop - 1;
    SCRATCH_STACK_CURSOR(VECTOR) = playerDelta;
    work                         = task->work;
    coord                        = task->extra.tmd->coords;
    switch (work->actionStep) {
        case ACTOR_00300_PURSUIT_WAIT:
            work->turnRate = 0;
            work->speed    = 0;
            work->anim     = ACTOR_00300_ANIM_STAND;
            timer          = (u16)work->timer - 1;
            work->timer    = timer;
            if (timer <= 0) {
                playerDelta->vx  = gPlayerStatus.coordMtx->t[0] - coord->coord.t[0];
                playerDelta->vy  = 0;
                playerDelta->vz  = gPlayerStatus.coordMtx->t[2] - coord->coord.t[2];
                playerDistance   = SquareRoot0(playerDelta->vx * playerDelta->vx + playerDelta->vz * playerDelta->vz);
                targetYaw        = ratan2((s16)playerDelta->vx, (s16)playerDelta->vz) & ACTOR_TRANSFORM_ANGLE_MASK;
                work->targetYaw  = targetYaw;
                yawDelta         = (u16)targetYaw - (u16)work->yaw;
                idleYawMagnitude = abs(yawDelta);
                if (idleYawMagnitude < ACTOR_TRANSFORM_ANGLE_HALF_TURN) {
                    yawError = idleYawMagnitude;
                } else {
                    if (yawDelta > 0) {
                        wrappedError = ACTOR_TRANSFORM_ANGLE_TURN - yawDelta;
                    } else {
                        wrappedError = yawDelta + ACTOR_TRANSFORM_ANGLE_TURN;
                    }
                    yawError = wrappedError;
                }
                if (playerDistance >= ACTOR_00300_CLOSE_RANGE || yawError >= ACTOR_00300_PURSUIT_YAW_TOLERANCE) {
                    work->actionStep = ACTOR_00300_PURSUIT_MOVE;
                }
                randomValue     = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                gRandomLcgState = randomValue;
                work->timer     = (((u32)randomValue >> 16) & 31) + ACTOR_00300_PURSUIT_MOVE_MIN_TICKS;
            }
            break;
        case ACTOR_00300_PURSUIT_MOVE:
            work->turnRate     = ACTOR_00300_PURSUIT_TURN_RATE;
            work->speed        = ACTOR_00300_PURSUIT_SPEED;
            work->anim         = ACTOR_00300_ANIM_MOVE;
            playerDelta->vx    = gPlayerStatus.coordMtx->t[0] - coord->coord.t[0];
            playerDelta->vy    = 0;
            playerDelta->vz    = gPlayerStatus.coordMtx->t[2] - coord->coord.t[2];
            playerDistance     = SquareRoot0(playerDelta->vx * playerDelta->vx + playerDelta->vz * playerDelta->vz);
            targetYaw          = ratan2((s16)playerDelta->vx, (s16)playerDelta->vz) & ACTOR_TRANSFORM_ANGLE_MASK;
            work->targetYaw    = targetYaw;
            yawDelta           = (u16)targetYaw - (u16)work->yaw;
            movingYawMagnitude = abs(yawDelta);
            if (movingYawMagnitude < ACTOR_TRANSFORM_ANGLE_HALF_TURN) {
                yawError = movingYawMagnitude;
            } else {
                if (yawDelta > 0) {
                    wrappedError = ACTOR_TRANSFORM_ANGLE_TURN - yawDelta;
                } else {
                    wrappedError = yawDelta + ACTOR_TRANSFORM_ANGLE_TURN;
                }
                yawError = wrappedError;
            }
            timer       = (u16)work->timer - 1;
            work->timer = timer;
            if (timer <= 0 || (playerDistance < ACTOR_00300_CLOSE_RANGE && yawError < ACTOR_00300_PURSUIT_YAW_TOLERANCE)) {
                work->actionStep = ACTOR_00300_PURSUIT_WAIT;
                randomValue      = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                gRandomLcgState  = randomValue;
                work->timer      = ((u32)randomValue >> 16) & 31;
            }
            break;
    }
    if (work->alertTimer == 0) {
        work->action                            = ACTOR_00300_ACTION_PATROL;
        work->actionStep                        = ACTOR_00300_ACTION_BEGIN;
        work->hitDamage                         = 0;
        gSceneCombatState.actor00300AttackAlert = 0;
        work->timer                             = ACTOR_00300_RETURN_PATROL_WAIT_TICKS;
    } else {
        timer             = (u16)work->attackTimer - 1;
        work->attackTimer = timer;
        if (timer <= 0) {
            randomValue       = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            gRandomLcgState   = randomValue;
            work->attackTimer = (((u32)randomValue >> 16) & 63) + ACTOR_00300_ATTACK_MIN_TICKS;
            if (work->playerSeen == 1) {
                _actor00300SelectAttack(task);
            }
        }
    }
    SCRATCH_STACK_RELEASE_BLOCK(VECTOR);
}

/// Selects healing, a fireball, MP drain or recharge from current health, MP and player distance.
///
/// Requires initialized enemy work, a positive package maximum HP and live player
/// and root matrices in the same parent space. Spends the heal or fireball cost
/// immediately; a zero fireball draw still consumes its cost and resumes pursuit.
/// The sixteen-entry draw contains 0..3, bounding the four-entry charge table.
static void _actor00300SelectAttack(Task* task)
{
    enum { ACTOR_00300_HEAL_HP_THRESHOLD_PERCENT = 50,
           ACTOR_00300_FIREBALL_NO_ATTACK        = 0,
           ACTOR_00300_ATTACK_TURN_MIN_TICKS     = 60 };

    _Actor00300Work*  work;
    GfxCoord*         coord;
    ActorFaceScratch* scratch;
    s32               randomValue;

    scratch = SCRATCH_STACK_RESERVE_BLOCK(ActorFaceScratch);
    work    = task->work;
    coord   = task->extra.tmd->coords;
    if (((Enemy*)task->spawnArg2.pointer)->hp * 100 / (s32)Actor00300_D15FE8.hpMax < ACTOR_00300_HEAL_HP_THRESHOLD_PERCENT &&
        work->mp >= ACTOR_00300_MP_HEAL_COST) {
        work->mp        -= ACTOR_00300_MP_HEAL_COST;
        work->anim       = ACTOR_00300_ANIM_CHARGE;
        work->action     = ACTOR_00300_ACTION_HEAL;
        work->actionStep = ACTOR_00300_ACTION_BEGIN;
    } else if (work->mp >= ACTOR_00300_MP_FIREBALL_COST) {
        work->mp            -= ACTOR_00300_MP_FIREBALL_COST;
        work->fireballAttack = Actor00300_D16000[((gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT) >> 16) & 15];
        if (work->fireballAttack == ACTOR_00300_FIREBALL_NO_ATTACK) {
            work->action     = ACTOR_00300_ACTION_PURSUE;
            work->actionStep = ACTOR_00300_ACTION_BEGIN;
            work->timer      = ((gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT) >> 16 & 31);
        } else {
            work->action     = ACTOR_00300_ACTION_FIREBALL;
            work->actionStep = ACTOR_00300_ACTION_BEGIN;
            work->timer      = ((gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT) >> 16 & 31) + ACTOR_00300_ATTACK_TURN_MIN_TICKS;
        }
    } else {
        scratch->delta.vx = gPlayerStatus.coordMtx->t[0] - coord->coord.t[0];
        scratch->delta.vy = 0;
        scratch->delta.vz = gPlayerStatus.coordMtx->t[2] - coord->coord.t[2];
        if (SquareRoot0(scratch->delta.vx * scratch->delta.vx + scratch->delta.vz * scratch->delta.vz) < ACTOR_00300_CLOSE_RANGE) {
            work->action     = ACTOR_00300_ACTION_DRAIN;
            work->actionStep = ACTOR_00300_ACTION_BEGIN;
            randomValue      = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            gRandomLcgState  = randomValue;
            work->timer      = (((u32)randomValue >> 16) & 31) + ACTOR_00300_ATTACK_TURN_MIN_TICKS;
        } else {
            work->action     = ACTOR_00300_ACTION_RECHARGE;
            work->actionStep = ACTOR_00300_ACTION_BEGIN;
        }
    }
    SCRATCH_STACK_RELEASE_BLOCK(ActorFaceScratch);
}

/// Turns toward the player, charges and releases a fireball, then recovers to pursuit.
///
/// Requires attack index 1..3 and initialized parent work/model coordinates.
/// Animation ticks schedule the effects and projectile; the charge halo belongs
/// to the enemy task tree. Spawn offsets are read during `effectSpawn`; temporary
/// offset storage cannot be followed after this call returns.
static void _actor00300TickFireballAttack(Task* task)
{
    enum { ACTOR_00300_FIREBALL_AIM               = 0,
           ACTOR_00300_FIREBALL_CHARGE            = 1,
           ACTOR_00300_FIREBALL_RELEASE           = 2,
           ACTOR_00300_FIREBALL_RECOVER           = 3,
           ACTOR_00300_FIREBALL_YAW_TOLERANCE     = 0x100,
           ACTOR_00300_FIREBALL_AIM_RATE          = 60,
           ACTOR_00300_FIREBALL_CHARGE_TURN_RATE  = 15,
           ACTOR_00300_FIREBALL_HALO_FRAME        = 51,
           ACTOR_00300_FIREBALL_HALO_LEAD_TICKS   = 50,
           ACTOR_00300_FIREBALL_SPAWN_FRAME       = 14,
           ACTOR_00300_FIREBALL_RELEASE_END_FRAME = 19,
           ACTOR_00300_FIREBALL_RECOVER_END_FRAME = 20,
           ACTOR_00300_FIREBALL_DESCRIPTOR_INDEX  = 2,
           ACTOR_00300_SOUND_CHARGE_START         = 0x40030006,
           ACTOR_00300_SOUND_FIREBALL_RELEASE     = 0x40030005,
           ACTOR_00300_FIREBALL_MOTE_ARG          = 0x20101200, // 512-unit half-extent, palette 1, 16-unit base rise, 32 ticks
    };

    SVECTOR           moteOffset;
    SVECTOR           radialOffset;
    _Actor00300Work*  work;
    EffectWork*       effect;
    GfxCoord*         coord;
    s16               turnTimer;
    s16               effectTimer2;
    s16               effectTimer1;
    s16               step;
    s16               yawDelta;
    s32               yawMagnitude;
    s16               yawError;
    s16               pursuitDelay;
    s32               releaseMoteRandom;
    s32               releaseMoteAngle;
    s32               chargeMoteRandom;
    s32               chargeMoteAngle;
    s32               soundId;
    s32               abortDelayRandom;
    s32               recoveryDelayRandom;
    s32               releasePan;
    s32               chargeStartPan;
    s32               chargeLoopPan;
    u16               targetYaw;
    u32               releaseSpawnRandom;
    u32               chargeSpawnRandom;
    ActorFaceScratch* stackTop;
    ActorFaceScratch* scratch;

    stackTop = SCRATCH_STACK_CURSOR(ActorFaceScratch);
    scratch  = (SCRATCH_STACK_CURSOR(ActorFaceScratch) = stackTop - 1);
    work     = task->work;
    step     = work->actionStep;
    coord    = task->extra.tmd->coords;
    switch (step) {
        case ACTOR_00300_FIREBALL_AIM:
            work->turnRate = ACTOR_00300_FIREBALL_AIM_RATE;
            work->speed    = 0;
            work->anim     = ACTOR_00300_ANIM_TURN;
            stackTop[-1].delta.vx =
                (s32)(gPlayerStatus.coordMtx->t[0] - coord->coord.t[0]);
            scratch->delta.vy = 0;
            scratch->delta.vz = (s32)(gPlayerStatus.coordMtx->t[2] - coord->coord.t[2]);
            targetYaw         = ratan2((s16)stackTop[-1].delta.vx, (s16)scratch->delta.vz) &
                        ACTOR_TRANSFORM_ANGLE_MASK;
            work->targetYaw = targetYaw;
            yawDelta        = targetYaw - (u16)work->yaw;
            yawMagnitude    = abs(yawDelta);
            yawError        = yawMagnitude >= ACTOR_TRANSFORM_ANGLE_HALF_TURN ? (yawDelta > 0 ? ACTOR_TRANSFORM_ANGLE_TURN - yawDelta : yawDelta + ACTOR_TRANSFORM_ANGLE_TURN)
                                                                              : yawMagnitude;
            if (yawError < ACTOR_00300_FIREBALL_YAW_TOLERANCE) {
                work->actionStep                        = ACTOR_00300_FIREBALL_CHARGE;
                work->anim                              = ACTOR_00300_ANIM_CHARGE;
                gSceneCombatState.actor00300AttackAlert = 1;
            } else {
                turnTimer   = (u16)work->timer - 1;
                work->timer = turnTimer;
                if (turnTimer <= 0) {
                    work->action     = ACTOR_00300_ACTION_PURSUE;
                    work->actionStep = ACTOR_00300_ACTION_BEGIN;
                    abortDelayRandom = (gRandomLcgState * RANDOM_LCG_MULTIPLIER) + RANDOM_LCG_INCREMENT;
                    gRandomLcgState  = abortDelayRandom;
                    pursuitDelay     = ((u32)abortDelayRandom >> 0x10) & 0xF;
                    work->timer      = pursuitDelay;
                }
            }
            break;
        case ACTOR_00300_FIREBALL_CHARGE:
            gSceneCombatState.actor00300AttackAlert = 0;
            work->turnRate                          = ACTOR_00300_FIREBALL_CHARGE_TURN_RATE;
            stackTop[-1].delta.vx =
                (s32)(gPlayerStatus.coordMtx->t[0] - coord->coord.t[0]);
            scratch->delta.vy = 0;
            scratch->delta.vz = (s32)(gPlayerStatus.coordMtx->t[2] - coord->coord.t[2]);
            work->targetYaw =
                ratan2((s16)stackTop[-1].delta.vx, (s16)scratch->delta.vz) &
                ACTOR_TRANSFORM_ANGLE_MASK;
            if (gRoomEffectState->effectControl == ROOM_EFFECT_CONTROL_RUNNING) {
                chargeSpawnRandom = (gRandomLcgState * RANDOM_LCG_MULTIPLIER) + RANDOM_LCG_INCREMENT;
                gRandomLcgState   = chargeSpawnRandom;
                if (!((chargeSpawnRandom >> 0x10) & ACTOR_00300_MOTE_SPAWN_MASK)) {
                    chargeMoteRandom = (chargeSpawnRandom * RANDOM_LCG_MULTIPLIER) + RANDOM_LCG_INCREMENT;
                    gRandomLcgState  = chargeMoteRandom;
                    chargeMoteAngle  = ((u32)chargeMoteRandom >> 0x10) & ACTOR_00300_MOTE_ANGLE_MASK;
                    memset(&radialOffset, 0, sizeof(radialOffset));
                    radialOffset.vx = (s16)((u32)(rcos(chargeMoteAngle) * 5) >> 5);
                    radialOffset.vz = (s16)((u32)(rsin(chargeMoteAngle) * 5) >> 5);
                    moteOffset      = radialOffset;
                    effectSpawn(gRoomEffectMoteId, coord, ACTOR_00300_FIREBALL_MOTE_ARG, &moteOffset);
                }
            }
            // Keep the halo alive until the chosen attack charge completes.
            if (work->animFrame == ACTOR_00300_FIREBALL_HALO_FRAME) {
                soundId        = ((((Enemy*)task->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | ACTOR_00300_SOUND_CHARGE_START;
                chargeStartPan = (s8)worldCoordGetOriginAudioPan(coord);
                sndEvtRequestScriptStart(soundId, (s32)chargeStartPan,
                                         (s32)(s8)worldCoordGetOriginAudioDepth(coord));
                scratch->rot.vy = -0x5DC;
                scratch->rot.vx = 0;
                scratch->rot.vz = 0x320;
                effect =
                    effectSpawn(gRoomEffectHaloId, coord,
                                Actor00300_D15FF8[work->fireballAttack] - ACTOR_00300_FIREBALL_HALO_LEAD_TICKS, &scratch->rot);
                work->chargeEffect = effect;
                if (effect != NULL) {
                    taskReparent(task, effect->task);
                    work->chargeEffectTimer = Actor00300_D15FF8[work->fireballAttack] - ACTOR_00300_FIREBALL_HALO_LEAD_TICKS;
                }
                work->chargeSound =
                    ((((Enemy*)task->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | ACTOR_00300_SOUND_CHARGE;
                chargeLoopPan = (s8)worldCoordGetOriginAudioPan(coord);
                sndEvtRequestScriptStart(work->chargeSound, (s32)chargeLoopPan,
                                         (s32)(s8)worldCoordGetOriginAudioDepth(coord));
            }
            if (work->animFrame >= Actor00300_D15FF8[work->fireballAttack]) {
                work->actionStep = ACTOR_00300_FIREBALL_RELEASE;
                work->anim       = ACTOR_00300_ANIM_RELEASE;
                work->turnRate   = 0;
            }
            if (work->chargeEffectTimer > 0) {
                effectTimer1            = (u16)work->chargeEffectTimer - 1;
                work->chargeEffectTimer = effectTimer1;
                if (effectTimer1 <= 0) {
                    work->chargeEffect = NULL;
                }
            }
            break;
        case ACTOR_00300_FIREBALL_RELEASE:
            if (work->chargeEffectTimer > 0) {
                effectTimer2            = (u16)work->chargeEffectTimer - 1;
                work->chargeEffectTimer = effectTimer2;
                if (effectTimer2 <= 0) {
                    work->chargeEffect = NULL;
                }
            }
            if ((work->animFrame < ACTOR_00300_FIREBALL_SPAWN_FRAME) && (gRoomEffectState->effectControl == ROOM_EFFECT_CONTROL_RUNNING)) {
                releaseSpawnRandom = (gRandomLcgState * RANDOM_LCG_MULTIPLIER) + RANDOM_LCG_INCREMENT;
                gRandomLcgState    = releaseSpawnRandom;
                if (!((releaseSpawnRandom >> 0x10) & ACTOR_00300_MOTE_SPAWN_MASK)) {
                    releaseMoteRandom = (releaseSpawnRandom * RANDOM_LCG_MULTIPLIER) + RANDOM_LCG_INCREMENT;
                    gRandomLcgState   = releaseMoteRandom;
                    releaseMoteAngle  = ((u32)releaseMoteRandom >> 0x10) & ACTOR_00300_MOTE_ANGLE_MASK;
                    memset(&radialOffset, 0, sizeof(radialOffset));
                    radialOffset.vx = (s16)((u32)(rcos(releaseMoteAngle) * 5) >> 5);
                    radialOffset.vz = (s16)((u32)(rsin(releaseMoteAngle) * 5) >> 5);
                    moteOffset      = radialOffset;
                    effectSpawn(gRoomEffectMoteId, coord, ACTOR_00300_FIREBALL_MOTE_ARG, &moteOffset);
                }
            }
            // Release exactly one projectile on the animation cue.
            if (work->animFrame == ACTOR_00300_FIREBALL_SPAWN_FRAME) {
                enemySpawnFromTable(Actor00300_D162F0, ACTOR_00300_FIREBALL_DESCRIPTOR_INDEX, 0, task->spawnArg2.pointer);
                soundId    = ((((Enemy*)task->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | ACTOR_00300_SOUND_FIREBALL_RELEASE;
                releasePan = (s8)worldCoordGetOriginAudioPan(coord);
                sndEvtRequestScriptStart(soundId, (s32)releasePan,
                                         (s32)(s8)worldCoordGetOriginAudioDepth(coord));
            }
            if (work->animFrame >= ACTOR_00300_FIREBALL_RELEASE_END_FRAME) {
                work->actionStep = ACTOR_00300_FIREBALL_RECOVER;
                work->anim       = ACTOR_00300_ANIM_RECOVER;
            }
            break;
        case ACTOR_00300_FIREBALL_RECOVER:
            if (work->animFrame >= ACTOR_00300_FIREBALL_RECOVER_END_FRAME) {
                work->action        = ACTOR_00300_ACTION_PURSUE;
                work->actionStep    = ACTOR_00300_ACTION_BEGIN;
                recoveryDelayRandom = (gRandomLcgState * RANDOM_LCG_MULTIPLIER) + RANDOM_LCG_INCREMENT;
                gRandomLcgState     = recoveryDelayRandom;
                pursuitDelay        = ((u32)recoveryDelayRandom >> 0x10) & 0x1F;
                work->timer         = pursuitDelay;
            }
            break;
    }
    SCRATCH_STACK_RELEASE_BLOCK(ActorFaceScratch);
}

/// Turns toward the player and extends the drain model and its MP-draining collision sphere.
///
/// Requires a live drain child and parent work. The model grows by 1/16 in Q12
/// scale per animation tick during frames 16..35, retracts from frame 51 and
/// finishes at frame 55. Pair contacts begin at frame 32 and end on contact or
/// completion. Borrows one `VECTOR`; angles use 4096 units per turn.
static void _actor00300TickDrainAttack(Task* task)
{
    enum { ACTOR_00300_DRAIN_AIM                = 0,
           ACTOR_00300_DRAIN_EXTEND             = 1,
           ACTOR_00300_DRAIN_YAW_TOLERANCE      = 0x80,
           ACTOR_00300_DRAIN_AIM_RATE           = 60,
           ACTOR_00300_DRAIN_EXTEND_START_FRAME = 16,
           ACTOR_00300_DRAIN_EXTEND_TICKS       = 20U,
           ACTOR_00300_DRAIN_RETRACT_FRAME      = 51,
           ACTOR_00300_DRAIN_CONTACT_FRAME      = 32,
           ACTOR_00300_DRAIN_END_FRAME          = 55,
           ACTOR_00300_DRAIN_SCALE_STEP         = ONE / 16,
           ACTOR_00300_SOUND_DRAIN              = 0x4003000A };

    _Actor00300Work* work;
    GfxCoord*        coord;
    VECTOR*          stackTop;
    VECTOR*          playerDelta;
    s16              yawDelta;
    s16              targetYaw;
    s16              yawError;
    s16              timer;
    s32              yawMagnitude;
    s32              soundId;
    s32              pan;
    s32              randomValue;

    stackTop                     = SCRATCH_STACK_CURSOR(VECTOR);
    playerDelta                  = stackTop - 1;
    SCRATCH_STACK_CURSOR(VECTOR) = playerDelta;
    work                         = task->work;
    coord                        = task->extra.tmd->coords;
    switch (work->actionStep) {
        case ACTOR_00300_DRAIN_AIM:
            work->turnRate  = ACTOR_00300_DRAIN_AIM_RATE;
            work->speed     = 0;
            work->anim      = ACTOR_00300_ANIM_TURN;
            playerDelta->vx = gPlayerStatus.coordMtx->t[0] - coord->coord.t[0];
            playerDelta->vy = 0;
            playerDelta->vz = gPlayerStatus.coordMtx->t[2] - coord->coord.t[2];
            targetYaw       = ratan2((s16)playerDelta->vx, (s16)playerDelta->vz) & ACTOR_TRANSFORM_ANGLE_MASK;
            work->targetYaw = targetYaw;
            yawDelta        = (u16)targetYaw - (u16)work->yaw;
            yawMagnitude    = abs(yawDelta);
            yawError        = yawMagnitude >= ACTOR_TRANSFORM_ANGLE_HALF_TURN ? (yawDelta > 0 ? ACTOR_TRANSFORM_ANGLE_TURN - yawDelta : yawDelta + ACTOR_TRANSFORM_ANGLE_TURN) : yawMagnitude;
            if (yawError < ACTOR_00300_DRAIN_YAW_TOLERANCE) {
                work->actionStep = ACTOR_00300_DRAIN_EXTEND;
                work->turnRate   = 0;
                work->anim       = ACTOR_00300_ANIM_DRAIN;
            } else {
                timer       = (u16)work->timer - 1;
                work->timer = timer;
                if (timer <= 0) {
                    work->action     = ACTOR_00300_ACTION_PURSUE;
                    work->actionStep = ACTOR_00300_ACTION_BEGIN;
                    randomValue      = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                    gRandomLcgState  = randomValue;
                    work->timer      = ((u32)randomValue >> 16) & 15;
                }
            }
            break;
        case ACTOR_00300_DRAIN_EXTEND:
            // The unsigned range test selects only the outward-extension frames.
            if ((u32)((u16)work->animFrame - ACTOR_00300_DRAIN_EXTEND_START_FRAME) < (u32)ACTOR_00300_DRAIN_EXTEND_TICKS) {
                work->drainScaleY += ACTOR_00300_DRAIN_SCALE_STEP;
            } else if (work->animFrame >= ACTOR_00300_DRAIN_RETRACT_FRAME) {
                work->drainScaleY -= ACTOR_00300_DRAIN_SCALE_STEP;
            }
            if (work->animFrame == ACTOR_00300_DRAIN_CONTACT_FRAME) {
                work->drainBody.flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;
                soundId                = ((((Enemy*)task->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | ACTOR_00300_SOUND_DRAIN;
                pan                    = (s8)worldCoordGetOriginAudioPan(coord);
                sndEvtRequestScriptStart(soundId, pan, (s8)worldCoordGetOriginAudioDepth(coord));
            }
            if (work->animFrame >= ACTOR_00300_DRAIN_END_FRAME) {
                work->drainScaleY      = 0;
                work->action           = ACTOR_00300_ACTION_PURSUE;
                work->actionStep       = ACTOR_00300_ACTION_BEGIN;
                randomValue            = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                gRandomLcgState        = randomValue;
                work->timer            = ((u32)randomValue >> 16) & 31;
                work->drainBody.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
            }
            break;
    }
    SCRATCH_STACK_RELEASE_BLOCK(VECTOR);
}

/// Randomly emits a rising charge mote using caller-owned temporary offsets.
///
/// coord and packedArg are evaluated once if emitted; all other arguments are
/// side-effect-free writable scalar/vector lvalues evaluated repeatedly.
/// spawnRandom is u32, angleRandom and moteAngle are s32, and both offsets
/// are distinct SVECTOR objects. Temporaries must not alias one another or the
/// shared LCG state. Captures the room mote id and shared
/// LCG state, advancing it once for the decision and again for the angle on a
/// one-in-four success. The caller must gate room effect control. The packed
/// word supplies half-extent/palette, upward speed and lifetime. Offset storage
/// lasts through spawn only and must not be followed after the caller returns.
#define ACTOR_00300_SPAWN_CHARGE_MOTE(coord, packedArg, spawnRandom, angleRandom, moteAngle, radialOffset, moteOffset) \
    do {                                                                                                               \
        (spawnRandom)   = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;                              \
        gRandomLcgState = (spawnRandom);                                                                               \
        if (!(((spawnRandom) >> 16) & ACTOR_00300_MOTE_SPAWN_MASK)) {                                                  \
            (angleRandom)   = (spawnRandom) * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;                            \
            gRandomLcgState = (angleRandom);                                                                           \
            (moteAngle)     = ((u32)(angleRandom) >> 16) & ACTOR_00300_MOTE_ANGLE_MASK;                                \
            memset(&(radialOffset), 0, sizeof(radialOffset));                                                          \
            (radialOffset).vx = (s16)((u32)(rcos((moteAngle)) * 5) >> 5);                                              \
            (radialOffset).vz = (s16)((u32)(rsin((moteAngle)) * 5) >> 5);                                              \
            (moteOffset)      = (radialOffset);                                                                        \
            effectSpawn(gRoomEffectMoteId, (coord), (packedArg), &(moteOffset));                                       \
        }                                                                                                              \
    } while (0)

/// Charges and restores 100 HP, emits the healing effects, then resumes pursuit.
///
/// Requires the charge animation already selected and the heal MP cost already
/// paid. HP addition retains 16-bit wrap and is not clamped to maximum HP.
/// Halo and spark-emitter tasks join the enemy teardown tree; mote offsets are
/// temporary spawn-time values. Animation ticks gate the three stages.
static void _actor00300TickHeal(Task* task)
{
    enum { ACTOR_00300_HEAL_CHARGE            = 0,
           ACTOR_00300_HEAL_RELEASE           = 1,
           ACTOR_00300_HEAL_RECOVER           = 2,
           ACTOR_00300_HEAL_HALO_FRAME        = 51,
           ACTOR_00300_HEAL_RELEASE_END_FRAME = 19,
           ACTOR_00300_HEAL_RECOVER_END_FRAME = 15,
           ACTOR_00300_HEAL_HALO_ARG          = 0x10014,    // tint row 1, 20 expansion ticks
           ACTOR_00300_HEAL_MOTE_ARG          = 0x20100200, // 512-unit half-extent, palette 0, 16-unit base rise, 32 ticks
    };

    SVECTOR          haloOffset;
    SVECTOR          moteOffset;
    SVECTOR          radialOffset;
    _Actor00300Work* work;
    EffectWork*      halo;
    EffectWork*      sparkEmitter;
    Enemy*           enemy;
    TmdObject*       model;
    Enemy*           healedEnemy;
    GfxCoord*        coord;
    s16              timer;
    s16              step;
    s32              chargeMoteRandom;
    s32              chargeMoteAngle;
    s32              releaseMoteRandom;
    s32              releaseMoteAngle;
    s32              soundId;
    s32              recoveryDelayRandom;
    s32              chargePan;
    s32              restorePan;
    u32              chargeSpawnRandom;
    u32              releaseSpawnRandom;

    model = task->extra.tmd;
    work  = task->work;
    enemy = task->spawnArg2.pointer;
    step  = work->actionStep;
    coord = model->coords;
    switch (step) {
        case ACTOR_00300_HEAL_CHARGE:
            if (gRoomEffectState->effectControl == ROOM_EFFECT_CONTROL_RUNNING) {
                ACTOR_00300_SPAWN_CHARGE_MOTE(coord, ACTOR_00300_HEAL_MOTE_ARG, chargeSpawnRandom, chargeMoteRandom, chargeMoteAngle, radialOffset, moteOffset);
            }
            work->turnRate = 0;
            work->speed    = 0;
            if (work->animFrame >= ACTOR_00300_HEAL_HALO_FRAME) {
                work->actionStep   = ACTOR_00300_HEAL_RELEASE;
                work->anim         = ACTOR_00300_ANIM_RELEASE;
                haloOffset.vy      = -0x5DC;
                haloOffset.vx      = 0;
                haloOffset.vz      = 0x320;
                halo               = effectSpawn(gRoomEffectHaloId, coord, ACTOR_00300_HEAL_HALO_ARG, &haloOffset);
                work->chargeEffect = halo;
                if (halo != NULL) {
                    taskReparent(task, halo->task);
                    work->chargeEffectTimer = ACTOR_00300_HEAL_RELEASE_END_FRAME;
                }
                work->chargeSound = ((((Enemy*)task->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | ACTOR_00300_SOUND_CHARGE;
                chargePan         = (s8)worldCoordGetOriginAudioPan(coord);
                sndEvtRequestScriptStart(work->chargeSound, chargePan, (s8)worldCoordGetOriginAudioDepth(coord));
                return;
            }
            return;
        case ACTOR_00300_HEAL_RELEASE:
            if (work->chargeEffectTimer > 0) {
                timer                   = (u16)work->chargeEffectTimer - 1;
                work->chargeEffectTimer = timer;
                if (timer <= 0) {
                    work->chargeEffect = NULL;
                }
            }
            if (gRoomEffectState->effectControl == ROOM_EFFECT_CONTROL_RUNNING) {
                ACTOR_00300_SPAWN_CHARGE_MOTE(coord, ACTOR_00300_HEAL_MOTE_ARG, releaseSpawnRandom, releaseMoteRandom, releaseMoteAngle, radialOffset, moteOffset);
            }
            if (work->animFrame >= ACTOR_00300_HEAL_RELEASE_END_FRAME) {
                work->actionStep   = ACTOR_00300_HEAL_RECOVER;
                work->chargeEffect = NULL;
                work->anim         = ACTOR_00300_ANIM_RECOVER;
                // Preserve the original halfword addition, including overflow.
                healedEnemy     = task->spawnArg2.pointer;
                healedEnemy->hp = (u16)healedEnemy->hp + ACTOR_00300_HEAL_AMOUNT;
                worldTargetAddReadoutAmount(&enemy->node, -ACTOR_00300_HEAL_AMOUNT, 0);
                sparkEmitter = effectSpawn(gRoomEffectSparkEmitterId, coord, 0, NULL);
                if (sparkEmitter != NULL) {
                    taskReparent(task, sparkEmitter->task);
                }
                soundId    = ((((Enemy*)task->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | ACTOR_00300_SOUND_RESTORE;
                restorePan = (s8)worldCoordGetOriginAudioPan(coord);
                sndEvtRequestScriptStart(soundId, restorePan, (s8)worldCoordGetOriginAudioDepth(coord));
            }
            break;
        case ACTOR_00300_HEAL_RECOVER:
            if (work->animFrame >= ACTOR_00300_HEAL_RECOVER_END_FRAME) {
                work->action        = ACTOR_00300_ACTION_PURSUE;
                work->actionStep    = ACTOR_00300_ACTION_BEGIN;
                recoveryDelayRandom = (gRandomLcgState * RANDOM_LCG_MULTIPLIER) + RANDOM_LCG_INCREMENT;
                gRandomLcgState     = recoveryDelayRandom;
                work->timer         = ((u32)recoveryDelayRandom >> 0x10) & 0x1F;
            }
            break;
    }
}

/// Releases a live charge halo and stops the sound instance held by the enemy.
///
/// Borrows initialized enemy work and an optional live halo. A non-NULL halo
/// receives its release task state, then the stored pointer and timer clear and
/// the sound stop keeps the current ADSR release policy. NULL changes nothing;
/// the halo task owns its eventual release.
static inline void _actor00300StopCharge(_Actor00300Work* work, EffectWork* chargeEffect)
{
    if (chargeEffect != NULL) {
        chargeEffect->task->state = ACTOR_00300_HALO_RELEASE_STATE;
        work->chargeEffect        = NULL;
        work->chargeEffectTimer   = 0;
        sndEvtRequestScriptStop(work->chargeSound, SOUND_SCRIPT_STOP_KEEP_RELEASE);
    }
}

/// Runs a flinch or knock-down/get-up reaction and hands a fatal knock-down to the death state.
///
/// Requires the last hit damage, direction and knock-down flag in initialized
/// enemy work. Entry cancels charge and drain contacts. Animation ticks select
/// reaction sounds and end frames; fatal entry moves floor/grid testing from the
/// root sphere to the hit sphere. A surviving reaction re-arms pursuit.
static void _actor00300TickHurt(Task* task)
{
    enum { ACTOR_00300_HURT_BEGIN               = 0,
           ACTOR_00300_HURT_FLINCH              = 1,
           ACTOR_00300_HURT_KNOCK_DOWN          = 2,
           ACTOR_00300_HURT_GET_UP              = 3,
           ACTOR_00300_KNOCK_DOWN_DAMAGE        = 120,
           ACTOR_00300_BACK_FALL_END_FRAME      = 32,
           ACTOR_00300_FRONT_FALL_END_FRAME     = 34,
           ACTOR_00300_BACK_FLINCH_END_FRAME    = 53,
           ACTOR_00300_FRONT_FLINCH_END_FRAME   = 51,
           ACTOR_00300_BACK_FLINCH_SOUND_FRAME  = 10,
           ACTOR_00300_FRONT_FLINCH_SOUND_FRAME = 12,
           ACTOR_00300_BACK_FALL_SOUND_FRAME    = 13,
           ACTOR_00300_FRONT_FALL_SOUND_FRAME   = 18,
           ACTOR_00300_GET_UP_END_FRAME         = 43,
           ACTOR_00300_SOUND_HIT                = 0x40030007,
           ACTOR_00300_SOUND_FLINCH             = 0x40030003,
           ACTOR_00300_SOUND_FALL               = 0x40030004 };

    _Actor00300Work* work;
    EffectWork*      chargeEffect;
    Enemy*           enemy;
    GfxCoord*        coord;
    s32              step;
    s16              reactionAnim;
    s16              deathEndFrame;
    s16              knockDownEndFrame;
    s16              flinchEndFrame;
    s32              soundId;
    s32              flinchDelayRandom;
    s32              getUpDelayRandom;
    s32              soundEntry;
    s32              hitPan, backFlinchPan, frontFlinchPan, backFallPan, frontFallPan;

    work  = task->work;
    enemy = task->spawnArg2.pointer;
    step  = work->actionStep;
    coord = task->extra.tmd->coords;
    switch (step) {
        case ACTOR_00300_HURT_BEGIN:
            chargeEffect           = work->chargeEffect;
            work->speed            = 0;
            work->turnRate         = 0;
            work->drainBody.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
            _actor00300StopCharge(work, chargeEffect);
            // Fatal falls move floor collision from the root to the falling body.
            if (enemy->hp <= 0) {
                if (work->hitFromFront == 0) {
                    work->anim    = ACTOR_00300_ANIM_KNOCK_DOWN_BACK;
                    deathEndFrame = ACTOR_00300_BACK_FALL_END_FRAME;
                } else {
                    work->anim    = ACTOR_00300_ANIM_KNOCK_DOWN_FRONT;
                    deathEndFrame = ACTOR_00300_FRONT_FALL_END_FRAME;
                }
                work->timer           = deathEndFrame;
                work->deathStage      = 1;
                enemy->reactionFlags  = 0;
                work->hitBody.pos.vz  = 0x190;
                work->actionStep      = ACTOR_00300_HURT_KNOCK_DOWN;
                work->hitBody.flags  |= WORLD_COLLISION_BODY_GRID_ENABLED;
                work->gridBody.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_GRID_ENABLED);
                return;
            }
            if ((work->hitDamage >= ACTOR_00300_KNOCK_DOWN_DAMAGE) || (work->knockDown != 0)) {
                if (work->hitFromFront == 0) {
                    work->anim        = ACTOR_00300_ANIM_KNOCK_DOWN_BACK;
                    knockDownEndFrame = ACTOR_00300_BACK_FALL_END_FRAME;
                } else {
                    work->anim        = ACTOR_00300_ANIM_KNOCK_DOWN_FRONT;
                    knockDownEndFrame = ACTOR_00300_FRONT_FALL_END_FRAME;
                }
                work->timer      = knockDownEndFrame;
                work->actionStep = ACTOR_00300_HURT_KNOCK_DOWN;
            } else {
                if (work->hitFromFront == 0) {
                    work->anim     = ACTOR_00300_ANIM_FLINCH_BACK;
                    flinchEndFrame = ACTOR_00300_BACK_FLINCH_END_FRAME;
                } else {
                    work->anim     = ACTOR_00300_ANIM_FLINCH_FRONT;
                    flinchEndFrame = ACTOR_00300_FRONT_FLINCH_END_FRAME;
                }
                work->timer         = flinchEndFrame;
                work->actionStep    = ACTOR_00300_HURT_FLINCH;
                work->reactionBlend = 1;
            }
            soundEntry = ACTOR_00300_SOUND_HIT;
            soundId    = ((((Enemy*)task->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | soundEntry;
            hitPan     = (s8)worldCoordGetOriginAudioPan(coord);
            sndEvtRequestScriptStart(soundId, (s32)hitPan, (s8)worldCoordGetOriginAudioDepth(coord));
            return;
        case ACTOR_00300_HURT_FLINCH:
            if (work->animFrame >= work->timer) {
                work->knockDown     = 0;
                work->action        = ACTOR_00300_ACTION_PURSUE;
                work->actionStep    = ACTOR_00300_PURSUIT_MOVE;
                work->alertTimer    = ACTOR_00300_ALERT_TICKS;
                work->reactionBlend = 0;
                flinchDelayRandom   = (gRandomLcgState * RANDOM_LCG_MULTIPLIER) + RANDOM_LCG_INCREMENT;
                gRandomLcgState     = flinchDelayRandom;
                work->timer         = ((u32)flinchDelayRandom >> 0x10) & 0x1F;
            }
            reactionAnim = work->anim;
            if (reactionAnim == ACTOR_00300_ANIM_FLINCH_BACK) {
                if (work->animFrame == ACTOR_00300_BACK_FLINCH_SOUND_FRAME) {
                    soundEntry    = ACTOR_00300_SOUND_FLINCH;
                    soundId       = ((((Enemy*)task->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | soundEntry;
                    backFlinchPan = (s8)worldCoordGetOriginAudioPan(coord);
                    sndEvtRequestScriptStart(soundId, (s32)backFlinchPan, (s8)worldCoordGetOriginAudioDepth(coord));
                }
            } else if (reactionAnim == ACTOR_00300_ANIM_FLINCH_FRONT) {
                if (work->animFrame == ACTOR_00300_FRONT_FLINCH_SOUND_FRAME) {
                    soundEntry     = ACTOR_00300_SOUND_FLINCH;
                    soundId        = ((((Enemy*)task->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | soundEntry;
                    frontFlinchPan = (s8)worldCoordGetOriginAudioPan(coord);
                    sndEvtRequestScriptStart(soundId, (s32)frontFlinchPan, (s8)worldCoordGetOriginAudioDepth(coord));
                }
            }
            break;
        case ACTOR_00300_HURT_KNOCK_DOWN:
            if (work->deathStage == 1) {
                work->deathStage = step;
            }
            if (work->animFrame >= work->timer) {
                if (enemy->hp <= 0) {
                    work->action     = ACTOR_00300_ACTION_DEAD;
                    work->actionStep = ACTOR_00300_ACTION_BEGIN;
                    task->state      = (s32)step;
                } else {
                    work->actionStep = ACTOR_00300_HURT_GET_UP;
                    if (work->anim == ACTOR_00300_ANIM_KNOCK_DOWN_BACK) {
                        work->anim = ACTOR_00300_ANIM_GET_UP_BACK;
                    } else {
                        work->anim = ACTOR_00300_ANIM_GET_UP_FRONT;
                    }
                    work->reactionBlend = 1;
                }
            }
            if (work->anim == ACTOR_00300_ANIM_KNOCK_DOWN_BACK) {
                if (work->animFrame == ACTOR_00300_BACK_FALL_SOUND_FRAME) {
                    soundEntry  = ACTOR_00300_SOUND_FALL;
                    soundId     = ((((Enemy*)task->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | soundEntry;
                    backFallPan = (s8)worldCoordGetOriginAudioPan(coord);
                    sndEvtRequestScriptStart(soundId, (s32)backFallPan, (s8)worldCoordGetOriginAudioDepth(coord));
                }
            } else if (work->anim == ACTOR_00300_ANIM_KNOCK_DOWN_FRONT) {
                if (work->animFrame == ACTOR_00300_FRONT_FALL_SOUND_FRAME) {
                    soundEntry   = ACTOR_00300_SOUND_FALL;
                    soundId      = ((((Enemy*)task->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | soundEntry;
                    frontFallPan = (s8)worldCoordGetOriginAudioPan(coord);
                    sndEvtRequestScriptStart(soundId, (s32)frontFallPan, (s8)worldCoordGetOriginAudioDepth(coord));
                }
            }
            break;
        case ACTOR_00300_HURT_GET_UP:
            if (work->animFrame >= ACTOR_00300_GET_UP_END_FRAME) {
                work->knockDown     = 0;
                work->action        = ACTOR_00300_ACTION_PURSUE;
                work->actionStep    = ACTOR_00300_PURSUIT_MOVE;
                work->alertTimer    = ACTOR_00300_ALERT_TICKS;
                work->reactionBlend = 0;
                getUpDelayRandom    = (gRandomLcgState * RANDOM_LCG_MULTIPLIER) + RANDOM_LCG_INCREMENT;
                gRandomLcgState     = getUpDelayRandom;
                work->timer         = ((u32)getUpDelayRandom >> 0x10) & 0x1F;
            }
            break;
    }
}

/// Stands for 91 active ticks to regain five MP, then resumes pursuit.
///
/// Requires initialized enemy work/model coordinates. Its timer counts upward
/// from zero, while the mote effects run under room effect control. The final
/// MP addition and random pursuit wait preserve their original halfword narrowing.
static void _actor00300TickRecharge(Task* task)
{
    enum { ACTOR_00300_RECHARGE_BEGIN    = 0,
           ACTOR_00300_RECHARGE_WAIT     = 1,
           ACTOR_00300_RECHARGE_TICKS    = 91,
           ACTOR_00300_RECHARGE_MOTE_ARG = 0x20103200, // 512-unit half-extent, palette 3, 16-unit base rise, 32 ticks
    };

    SVECTOR          moteOffset;
    SVECTOR          radialOffset;
    _Actor00300Work* work;
    GfxCoord*        coord;
    s16              timer;
    s16              step;
    s32              moteRandom;
    s32              moteAngle;
    s32              soundId;
    s32              pan;
    u32              spawnRandom;
    u32              pursuitDelayRandom;

    work  = task->work;
    step  = work->actionStep;
    coord = task->extra.tmd->coords;
    switch (step) {
        case ACTOR_00300_RECHARGE_BEGIN:
            work->actionStep = ACTOR_00300_RECHARGE_WAIT;
            work->turnRate   = 0;
            work->speed      = 0;
            work->timer      = 0;
            work->anim       = ACTOR_00300_ANIM_TURN;
            return;
        case ACTOR_00300_RECHARGE_WAIT:
            if (gRoomEffectState->effectControl == ROOM_EFFECT_CONTROL_RUNNING) {
                ACTOR_00300_SPAWN_CHARGE_MOTE(coord, ACTOR_00300_RECHARGE_MOTE_ARG, spawnRandom, moteRandom, moteAngle, radialOffset, moteOffset);
            }
            timer       = (u16)work->timer + 1;
            work->timer = timer;
            if (timer >= ACTOR_00300_RECHARGE_TICKS) {
                work->action       = ACTOR_00300_ACTION_PURSUE;
                work->timer        = 0;
                work->actionStep   = ACTOR_00300_ACTION_BEGIN;
                pursuitDelayRandom = (gRandomLcgState * RANDOM_LCG_MULTIPLIER) + RANDOM_LCG_INCREMENT;
                work->mp           = work->mp + ACTOR_00300_MP_RECHARGED;
                work->timer        = (pursuitDelayRandom >> 0x10) & 0x1F;
                gRandomLcgState    = pursuitDelayRandom;
                soundId            = ((((Enemy*)task->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | ACTOR_00300_SOUND_RESTORE;
                pan                = (s8)worldCoordGetOriginAudioPan(coord);
                sndEvtRequestScriptStart(soundId, pan, (s8)worldCoordGetOriginAudioDepth(coord));
            }
            return;
    }
}
#undef ACTOR_00300_SPAWN_CHARGE_MOTE

/// Turns the enemy root toward its requested heading by at most `turnRate`.
///
/// Requires initialized TMD work, a target yaw in 0..4095 and a nonnegative
/// turn rate, all in 4096 units per turn. Reads the current yaw from the matrix,
/// chooses the shorter arc and rebuilds pure Y rotation, retaining translation.
/// At exactly half a turn it takes the wrapped arc. The caller dirties and
/// composes the root afterward. Borrows one `ActorFaceScratch`.
static void _actor00300TurnTowardTargetYaw(Task* task)
{
    _Actor00300Work*  work;
    GfxCoord*         coord;
    ActorFaceScratch* scratch;
    s32               matrixYaw;
    u16               targetYaw;
    s16               yawDelta;
    s32               yawMagnitude;
    s32               turnRate;
    s32               currentYaw;
    s32               nextYaw;
    s32               wrappedTurnRate;

    scratch      = SCRATCH_STACK_RESERVE_BLOCK(ActorFaceScratch);
    coord        = task->extra.tmd->coords;
    work         = task->work;
    matrixYaw    = ratan2(coord->coord.m[0][2], coord->coord.m[2][2]) & ACTOR_TRANSFORM_ANGLE_MASK;
    targetYaw    = work->targetYaw;
    yawDelta     = targetYaw - matrixYaw;
    yawMagnitude = yawDelta >= 0 ? yawDelta : -yawDelta;

    work->yaw = matrixYaw;
    if (yawMagnitude < ACTOR_TRANSFORM_ANGLE_HALF_TURN) {
        turnRate = work->turnRate;
        if (turnRate >= yawMagnitude) {
            work->yaw = targetYaw;
        } else {
            nextYaw = work->yaw;
            if (yawDelta <= 0) {
                nextYaw -= turnRate;
            } else {
                nextYaw += turnRate;
            }
            work->yaw = nextYaw;
        }
    } else {
        turnRate = work->turnRate;
        if (yawDelta > 0 ? turnRate >= ACTOR_TRANSFORM_ANGLE_TURN - yawDelta : turnRate >= ACTOR_TRANSFORM_ANGLE_TURN + yawDelta) {
            work->yaw = work->targetYaw;
        } else {
            wrappedTurnRate = work->turnRate;
            currentYaw      = work->yaw;
            if (yawDelta > 0) {
                work->yaw = currentYaw - wrappedTurnRate;
            } else {
                work->yaw = currentYaw + wrappedTurnRate;
            }
        }
    }
    scratch->rot.vx = 0;
    scratch->rot.vy = work->yaw;
    scratch->rot.vz = 0;
    RotMatrix(&scratch->rot, &coord->coord);
    SCRATCH_STACK_RELEASE_BLOCK(ActorFaceScratch);
}

/// Applies the hit recoil to body part 3 and eases its two angles toward zero.
///
/// Requires the initialized nineteen-part TMD rig and active `hitTwist`.
/// Postmultiplies that part's animated rotation, retaining translation, then
/// reduces each nonzero angle by up to 32 units (4096 per turn). Clears the
/// active flag once both settle. Borrows a scratch matrix and changes GTE state.
static void _actor00300ApplyHitTwist(Task* task)
{
    enum { ACTOR_00300_HIT_TWIST_RETURN_STEP = 32 };

    /// Eases one hit-twist angle, raising the shared latch while another tick is needed.
    ///
    /// All arguments must be side-effect-free lvalues and the temporaries must
    /// be distinct signed words. Uses this function's angle-unit return step;
    /// the caller initializes the shared latch to zero before easing both axes.
#define ACTOR_00300_EASE_HIT_TWIST(angle, angleValue, magnitude, nextAngle, activeLatch) \
    {                                                                                    \
        (angleValue) = (angle);                                                          \
        if ((angleValue) != 0) {                                                         \
            (magnitude) = __builtin_abs(angleValue);                                     \
            if ((magnitude) <= ACTOR_00300_HIT_TWIST_RETURN_STEP) {                      \
                (angle) = 0;                                                             \
            } else {                                                                     \
                (nextAngle) = (angleValue) - ACTOR_00300_HIT_TWIST_RETURN_STEP;          \
                if ((angleValue) <= 0) {                                                 \
                    (nextAngle) = (angleValue) + ACTOR_00300_HIT_TWIST_RETURN_STEP;      \
                }                                                                        \
                (angle)       = (nextAngle);                                             \
                (activeLatch) = 1;                                                       \
            }                                                                            \
        }                                                                                \
    }

    _Actor00300Work* work;
    GfxCoord*        parts;
    MATRIX*          twistMatrix;
    s32              angleX;
    s32              angleY;
    s32              absX;
    s32              nextX;
    s32              absY;
    s32              nextY;
    s32              stillTwisting;

    twistMatrix   = SCRATCH_STACK_RESERVE_BLOCK(MATRIX);
    stillTwisting = 0;
    work          = task->work;
    parts         = task->extra.tmd->coords;
    // Apply recoil to the animated part before easing the angles for the next tick.
    RotMatrix(&work->hitTwist, twistMatrix);
    gte_MulMatrix0(&parts[3].coord, twistMatrix, &parts[3].coord);
    ACTOR_00300_EASE_HIT_TWIST(work->hitTwist.vx, angleX, absX, nextX, stillTwisting);
    ACTOR_00300_EASE_HIT_TWIST(work->hitTwist.vy, angleY, absY, nextY, stillTwisting);
    if (stillTwisting == 0) {
        work->hitTwistActive = 0;
    }
    SCRATCH_STACK_RELEASE_BLOCK(MATRIX);
}
#undef ACTOR_00300_EASE_HIT_TWIST

static void Actor00300_Fn03618(Task* arg0)
{
    GameLocationKey  key;
    u8               areaByte0;
    u32              raw1, index1;
    EffectWork*      effect1;
    TmdObject*       model1;
    AreaPlacement*   entry1;
    GameLocationKey* sessionKey1;
    u32              raw2, index2;
    EffectWork*      effect2;
    TmdObject*       model2;
    AreaPlacement*   entry2;
    GameLocationKey* sessionKey2;
    u32              raw3, index3;
    EffectWork*      effect3;
    TmdObject*       model3;
    AreaPlacement*   entry3;
    GameLocationKey* sessionKey3;
    u32              raw4, index4;
    EffectWork*      effect4;
    TmdObject*       model4;
    AreaPlacement*   entry4;
    GameLocationKey* sessionKey4;
    u32              raw5, index5;
    EffectWork*      effect5;
    TmdObject*       model5;
    AreaPlacement*   entry5;
    GameLocationKey* sessionKey5;

    D_80067704[0] = &_gActor00300BrainStingerBurstHead;
    effect1       = effectSpawn(EFFECT_BURST_BODY_PART_BANK4, &arg0->extra.tmd->coords[1], 0x200, NULL);
    if (effect1 != NULL) {
        sessionKey1 = &gGameSession->location.loc;
        raw1        = ((Enemy*)arg0->spawnArg2.pointer)->placeKey;
        model1      = effect1->task->extra.tmd;
        key.stage   = sessionKey1->stage;
        key.area    = sessionKey1->area;
        key.room    = sessionKey1->room;
        areaByte0   = gGameSession->location.loc.view;
        index1      = raw1 >> 12;
        key.view    = areaByte0;
        areaSyncLocationVariant(&key);
        entry1                    = gpAreaPlaceAt(Gp_GetNestedAreaRec(&key)->placements, index1);
        model1->texturePageOffset = entry1->texturePageOffset;
        model1->clutRowOffset     = entry1->clutRowOffset;
        if (model1->buffer != NULL) {
            tmdBuildBufferHalf(model1);
            tmdBuildBufferHalf(model1);
        }
    }

    D_80067704[0] = &_gActor00300Actor100300Model0ABC4;
    effect2       = effectSpawn(EFFECT_BURST_BODY_PART_BANK4, &arg0->extra.tmd->coords[1], 0x200, NULL);
    if (effect2 != NULL) {
        sessionKey2 = &gGameSession->location.loc;
        raw2        = ((Enemy*)arg0->spawnArg2.pointer)->placeKey;
        model2      = effect2->task->extra.tmd;
        key.stage   = sessionKey2->stage;
        key.area    = sessionKey2->area;
        key.room    = sessionKey2->room;
        areaByte0   = gGameSession->location.loc.view;
        index2      = raw2 >> 12;
        key.view    = areaByte0;
        areaSyncLocationVariant(&key);
        entry2                    = gpAreaPlaceAt(Gp_GetNestedAreaRec(&key)->placements, index2);
        model2->texturePageOffset = entry2->texturePageOffset;
        model2->clutRowOffset     = entry2->clutRowOffset;
        if (model2->buffer != NULL) {
            tmdBuildBufferHalf(model2);
            tmdBuildBufferHalf(model2);
        }
    }

    D_80067704[0] = &_gActor00300Actor100300Model0B128;
    effect3       = effectSpawn(EFFECT_BURST_BODY_PART_BANK4, &arg0->extra.tmd->coords[1], 0x200, NULL);
    if (effect3 != NULL) {
        sessionKey3 = &gGameSession->location.loc;
        raw3        = ((Enemy*)arg0->spawnArg2.pointer)->placeKey;
        model3      = effect3->task->extra.tmd;
        key.stage   = sessionKey3->stage;
        key.area    = sessionKey3->area;
        key.room    = sessionKey3->room;
        areaByte0   = gGameSession->location.loc.view;
        index3      = raw3 >> 12;
        key.view    = areaByte0;
        areaSyncLocationVariant(&key);
        entry3                    = gpAreaPlaceAt(Gp_GetNestedAreaRec(&key)->placements, index3);
        model3->texturePageOffset = entry3->texturePageOffset;
        model3->clutRowOffset     = entry3->clutRowOffset;
        if (model3->buffer != NULL) {
            tmdBuildBufferHalf(model3);
            tmdBuildBufferHalf(model3);
        }
    }

    D_80067704[0] = &_gActor00300Actor100300Model0B8AC;
    effect4       = effectSpawn(EFFECT_BURST_BODY_PART_BANK4, &arg0->extra.tmd->coords[1], 0x200, NULL);
    if (effect4 != NULL) {
        sessionKey4 = &gGameSession->location.loc;
        raw4        = ((Enemy*)arg0->spawnArg2.pointer)->placeKey;
        model4      = effect4->task->extra.tmd;
        key.stage   = sessionKey4->stage;
        key.area    = sessionKey4->area;
        key.room    = sessionKey4->room;
        areaByte0   = gGameSession->location.loc.view;
        index4      = raw4 >> 12;
        key.view    = areaByte0;
        areaSyncLocationVariant(&key);
        entry4                    = gpAreaPlaceAt(Gp_GetNestedAreaRec(&key)->placements, index4);
        model4->texturePageOffset = entry4->texturePageOffset;
        model4->clutRowOffset     = entry4->clutRowOffset;
        if (model4->buffer != NULL) {
            tmdBuildBufferHalf(model4);
            tmdBuildBufferHalf(model4);
        }
    }

    D_80067704[0] = &_gActor00300Actor100300Model0BFC0;
    effect5       = effectSpawn(EFFECT_BURST_BODY_PART_BANK4, &arg0->extra.tmd->coords[1], 0x200, NULL);
    if (effect5 != NULL) {
        sessionKey5 = &gGameSession->location.loc;
        raw5        = ((Enemy*)arg0->spawnArg2.pointer)->placeKey;
        model5      = effect5->task->extra.tmd;
        key.stage   = sessionKey5->stage;
        key.area    = sessionKey5->area;
        key.room    = sessionKey5->room;
        areaByte0   = gGameSession->location.loc.view;
        index5      = raw5 >> 12;
        key.view    = areaByte0;
        areaSyncLocationVariant(&key);
        entry5                    = gpAreaPlaceAt(Gp_GetNestedAreaRec(&key)->placements, index5);
        model5->texturePageOffset = entry5->texturePageOffset;
        model5->clutRowOffset     = entry5->clutRowOffset;
        if (model5->buffer != NULL) {
            tmdBuildBufferHalf(model5);
            tmdBuildBufferHalf(model5);
        }
    }
}

/// Plays the actor's two cue sounds when their slot-1 record bits fall.
///
/// Requires initialized rig, root coordinate and enemy placement key. Reads
/// the current record without advancing playback; NULL leaves the saved cue
/// bits intact. Sound instances use the placement index and root audio position.
/// Only cue bits are retained for the next tick.
static void _actor00300PlayAnimationCueSounds(Task* task)
{
    enum { ACTOR_00300_SOUND_CUE_2 = 0x40030001,
           ACTOR_00300_SOUND_CUE_1 = 0x40030002 };

    _Actor00300Work*       work;
    const AnimationRecord* record;
    GfxCoord*              rootCoord;
    s32                    soundId;
    s32                    cue2Pan;
    s32                    cue1Pan;

    work      = task->work;
    rootCoord = task->extra.tmd->coords;
    record    = animationGetCurrentRecord(&work->rig.anim, &work->rig.slots[1]);
    if (record != NULL) {
        if (!(record->flags & ANIMATION_RECORD_CUE_2) && (work->lastCueFlags & ANIMATION_RECORD_CUE_2)) {
            soundId = ((((Enemy*)task->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | ACTOR_00300_SOUND_CUE_2;
            cue2Pan = (s8)worldCoordGetOriginAudioPan(rootCoord);
            sndEvtRequestScriptStart(soundId, cue2Pan, (s8)worldCoordGetOriginAudioDepth(rootCoord));
        }
        if (!(record->flags & ANIMATION_RECORD_CUE_1) && (work->lastCueFlags & ANIMATION_RECORD_CUE_1)) {
            soundId = ((((Enemy*)task->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | ACTOR_00300_SOUND_CUE_1;
            cue1Pan = (s8)worldCoordGetOriginAudioPan(rootCoord);
            sndEvtRequestScriptStart(soundId, cue1Pan, (s8)worldCoordGetOriginAudioDepth(rootCoord));
        }
        work->lastCueFlags = (u16)(record->flags & ANIMATION_RECORD_CUE_MASK);
    }
}

static void Actor00300_Fn03B70(Enemy* arg0, Task* arg1)
{
    _Actor00300Work* work;
    TmdObject*       obj;
    GfxCoord*        coord;
    GfxCoord*        c;
    VECTOR           vec;
    s32              mode;
    s32              sound;
    s32              pan;
    s16              phase;

    obj   = arg1->extra.tmd;
    work  = arg1->work;
    mode  = gSceneCombatState.actorControl;
    coord = obj->coords;
    switch (mode) {
        case 1:
            vec.vx = coord->workm.t[0];
            vec.vy = coord->workm.t[1];
            vec.vz = coord->workm.t[2];
            worldCoordUpdateActorColor(arg1->spawnArg2.pointer, &vec, 0, 0);
            return;
        case 2:
            obj->flags                             = TMD_OBJECT_SKIP_ACTIVE_DRAW;
            work->drainModelTask->extra.tmd->flags = TMD_OBJECT_SKIP_ACTIVE_DRAW;
            return;
        case 0:
        default:
            break;
    }
    switch (work->actionStep) {
        case 0:
            work->deathScaleY  = 0x1000;
            work->savedRootMtx = coord->coord;
            arg0->recs         = NULL;
            worldTargetUnlinkNode(&arg0->node);
            worldCollisionUnlinkBody(&work->sightBody);
            worldCollisionUnlinkBody(&work->gridBody);
            worldCollisionUnlinkBody(&work->hitBody);
            worldCollisionUnlinkBody(&work->drainBody);
            worldCoordSetActorColorMode(arg0, ENEMY_COLOR_WEIGHTED);
            sceneReleaseBattleRefWithRewards(arg1, 3);
            work->timer      = 0;
            work->actionStep = 1;
            if (work->burstStage != 0) {
                obj->flags       = TMD_OBJECT_SKIP_ACTIVE_DRAW;
                work->actionStep = 3;
            }
            c      = arg1->extra.tmd->coords;
            vec.vx = c->workm.t[0];
            vec.vy = c->workm.t[1];
            vec.vz = c->workm.t[2];
            worldCoordUpdateActorColor(arg1->spawnArg2.pointer, &vec, 0, 0);
            if (work->chargeEffect != NULL) {
                work->chargeEffect->task->state = 3;
                work->chargeEffect              = NULL;
                work->chargeEffectTimer         = 0;
                sndEvtRequestScriptStop(work->chargeSound, SOUND_SCRIPT_STOP_KEEP_RELEASE);
            }
            sound = ((((Enemy*)arg1->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x40030008;
            pan   = (s8)worldCoordGetOriginAudioPan(coord);
            sndEvtRequestScriptStart(sound, pan, (s8)worldCoordGetOriginAudioDepth(coord));
            return;
        case 1:
            if (work->deathScaleY >= 0x201)
                work->deathScaleY -= 0x50;
            Actor00300_Fn0505C(arg1, &work->savedRootMtx, work->deathScaleY);
            phase       = work->timer + 1;
            work->timer = phase;
            if (phase == 10)
                obj->flags |= TMD_OBJECT_SEMI_TRANS;
            if (work->timer == 15)
                effectSpawn(EFFECT_CORPSE_BURN, &arg1->extra.tmd->coords[3], 3, NULL);
            if (work->timer >= 0x3C)
                work->actionStep = 2;
            c      = arg1->extra.tmd->coords;
            vec.vx = c->workm.t[0];
            vec.vy = c->workm.t[1];
            vec.vz = c->workm.t[2];
            worldCoordUpdateActorColor(arg1->spawnArg2.pointer, &vec, 0, 0);
            return;
        case 2:
            enemyDestroy(arg0, arg1);
            return;
        case 3:
            if (work->burstStage != 0) {
                if (work->burstStage >= 2) {
                    work->burstStage = 0;
                    tmdFreePrimitiveBuffer(obj);
                    obj->flags |= TMD_OBJECT_SKIP_AUTO_BUFFER;
                    Actor00300_Fn03618(arg1);
                } else
                    work->burstStage++;
            }
            phase       = work->timer + 1;
            work->timer = phase;
            if (phase >= 0x3C)
                work->actionStep = 2;
            return;
    }
}

/// Rebuilds the drain coordinate with a prepared local-axis Q12 scale from its rest matrix.
///
/// Borrows live coordinate, saved matrix and disjoint scratch storage. The scale
/// vector is prepared by the caller (4096 means full scale); this operation
/// initializes only the scratch rotation, postmultiplies the saved rotation and
/// preserves saved translation. Invalidates composition and changes GTE state.
static inline void _actor00300ApplyDrainScale(GfxCoord* coord, const MATRIX* restMatrix, ActorScaleScratch* scratch)
{
    coord->coord = *restMatrix;
    gfxSetRotIdentity(&scratch->matrix);
    ScaleMatrix(&scratch->matrix, &scratch->scale);
    MulMatrix(&coord->coord, &scratch->matrix);
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
}

/// Rebuilds the drain attachment from its rest matrix and the parent's Q12 Y scale.
///
/// Requires a live TMD child whose parent owns `_Actor00300Work`. A nonpositive
/// scale hides the model; positive scale multiplies local Y without scaling the
/// saved translation. Only running actor control updates it. Event draw flags
/// apply before the scale test. Borrows one `ActorScaleScratch` and dirties the
/// composition cache; the matrix multiply changes GTE state.
static __inline__ void _actor00300UpdateDrainModel(Task* task)
{
    TmdObject*         model;
    GfxCoord*          rootCoord;
    _Actor00300Work*   parentWork;
    s32                actorControl;
    s16                drawFlags;
    s16                scaleY;
    ActorScaleScratch* stackTop;
    ActorScaleScratch* scratch;
    GfxCoord*          scaledCoord;

    rootCoord    = task->extra.tmd->coords;
    model        = task->extra.tmd;
    actorControl = gSceneCombatState.actorControl;
    parentWork   = task->parent->work;
    if (actorControl == SCENE_COMBAT_ACTORS_RUNNING) {
        if (gGameSession->eventState != 0) {
            drawFlags    = ((parentWork->eventDrawFlags & 1) == 0) * TMD_OBJECT_SKIP_ACTIVE_DRAW;
            model->flags = drawFlags;
            if (parentWork->eventDrawFlags & 2) {
                model->flags = drawFlags | TMD_OBJECT_SKIP_AUTO_BUFFER;
            }
        }
        scaleY = parentWork->drainScaleY;
        if (scaleY <= 0) {
            model->flags = TMD_OBJECT_SKIP_ACTIVE_DRAW;
            return;
        }
        stackTop                                = SCRATCH_STACK_CURSOR(ActorScaleScratch);
        scratch                                 = stackTop - 1;
        scaledCoord                             = task->extra.tmd->coords;
        SCRATCH_STACK_CURSOR(ActorScaleScratch) = scratch;
        scratch->scale.vx                       = ONE;
        scratch->scale.vy                       = scaleY;
        scratch->scale.vz                       = ONE;
        // Rebuild from rest every tick so the Y scale never compounds.
        _actor00300ApplyDrainScale(scaledCoord, &parentWork->drainModelMtx, scratch);
        rootCoord->composeStamp = GRAPHICS_COORD_DIRTY;
        SCRATCH_STACK_RELEASE_BLOCK(ActorScaleScratch);
    }
}

/// Updates the active drain attachment with its parent's current scale.
///
/// The enemy argument is unused but retained for the `EnemyTaskFunc` contract.
/// Requires the attachment initialized and its parent work still live.
static void _actor00300DrainModelActive(Enemy* enemy, Task* task)
{
    _actor00300UpdateDrainModel(task);
}

/// Initializes and links the fireball's attack sphere before pair tests are enabled.
///
/// Borrows a live coordinate-body task, zeroed fireball work and the launching
/// enemy's work. The attack index must be 1..3; the body borrows the task's
/// coordinate and its own one-entry contact table until teardown unlinks it.
static inline void _actor00300InitFireballAttackBody(Task* task, _Actor00300FireballWork* work, const _Actor00300Work* parentWork)
{
    enum { ACTOR_00300_FIREBALL_HIT_RADIUS = 450 };
    GfxCoord* coord;

    coord                       = task->extra.coordBody->coord;
    work->body.context.contacts = work->contacts;
    work->body.pos.vx           = 0;
    work->body.pos.vy           = 0;
    work->body.pos.vz           = 0;
    work->body.coord            = coord;
    work->body.key              = damagePackAttackKey(Actor00300_D15FD8, parentWork->fireballAttack);
    work->body.radius           = ACTOR_00300_FIREBALL_HIT_RADIUS;
    work->body.flags            = WORLD_COLLISION_BODY_SPHERE;
    worldCollisionLinkBody(WORLD_COLLISION_LIST_ENEMY_ATTACKS, &work->body);
    worldCollisionInitContacts(work->contacts, ARRAY_SIZE(work->contacts), 0);
}

/// Links the trailing grid-probe capsule and enables the attack sphere's pair tests.
///
/// Requires a linked attack sphere and zeroed sweep storage. The 420-unit
/// capsule covers one 400-unit flight step with radius 1 at both ends.
/// Borrows the live task coordinate and the work's one-entry sweep-contact table;
/// the caller enables grid tests after starting the flight countdown.
static inline void _actor00300InitFireballSweepBody(Task* task, _Actor00300FireballWork* work)
{
    enum { ACTOR_00300_FIREBALL_SWEEP_LENGTH = 420,
           ACTOR_00300_FIREBALL_SWEEP_RADIUS = 1 };
    GfxCoord* coord;

    work->sweepCapsule.ends[1].vz   = -ACTOR_00300_FIREBALL_SWEEP_LENGTH;
    work->sweepCapsule.end0Radius   = ACTOR_00300_FIREBALL_SWEEP_RADIUS;
    work->sweepCapsule.end1Radius   = ACTOR_00300_FIREBALL_SWEEP_RADIUS;
    work->sweepCapsule.ends[0].vx   = 0;
    work->sweepCapsule.ends[0].vy   = 0;
    work->sweepCapsule.ends[0].vz   = 0;
    work->sweepCapsule.ends[1].vx   = 0;
    work->sweepCapsule.ends[1].vy   = 0;
    work->sweepCapsule.contacts     = work->sweepContacts;
    work->body.flags               |= WORLD_COLLISION_BODY_PAIR_ENABLED;
    coord                           = task->extra.coordBody->coord;
    work->sweepBody.context.capsule = &work->sweepCapsule;
    work->sweepBody.pos.vx          = 0;
    work->sweepBody.pos.vy          = 0;
    work->sweepBody.pos.vz          = 0;
    work->sweepBody.key             = 0;
    work->sweepBody.radius          = 0;
    work->sweepBody.flags           = WORLD_COLLISION_BODY_CAPSULE;
    work->sweepBody.coord           = coord;
    worldCollisionLinkBody(WORLD_COLLISION_LIST_ENEMY_ATTACKS, &work->sweepBody);
    worldCollisionInitContacts(work->sweepContacts, ARRAY_SIZE(work->sweepContacts), 0);
}

/// Places and arms a fireball, then detaches it from the launching enemy.
///
/// Requires a coordinate-body child of a live TMD enemy with initialized work
/// and attack index 1..3. Launches 1500 units above and 800 forward in the
/// parent's frame, borrowing `gGfxViewCoord` thereafter. Owns a zeroed fireball
/// work allocation and links its attack sphere and grid sweep capsule. Success
/// starts the flight countdown and task state 1. Allocation failure destroys
/// the child and retains the reserved scratch block, as in the binary.
static void _actor00300InitFireball(Enemy* enemy, Task* task)
{
    enum { ACTOR_00300_FIREBALL_LAUNCH_Y = -1500,
           ACTOR_00300_FIREBALL_LAUNCH_Z = 800,
           ACTOR_00300_FIREBALL_TASK_FLY = 1 };

    _Actor00300Work*         parentWork;
    Task*                    parent;
    _Actor00300FireballWork* work;
    ActorOffsetScratch*      scratch;
    SVECTOR*                 offset;
    GfxCoord*                coord;
    GfxCoord*                parentCoord;

    scratch     = SCRATCH_STACK_RESERVE_BLOCK(ActorOffsetScratch);
    offset      = &scratch->offset;
    parent      = task->parent;
    coord       = task->extra.coordBody->coord;
    parentCoord = parent->extra.tmd->coords;
    parentWork  = parent->work;
    work        = memCalloc(sizeof(_Actor00300FireballWork), 0);
    if (work == NULL) {
        // Retained failure path leaves the scratch block reserved.
        enemyDestroy(enemy, task);
        return;
    }
    task->work         = work;
    scratch->offset.vx = 0;
    scratch->offset.vy = ACTOR_00300_FIREBALL_LAUNCH_Y;
    scratch->offset.vz = ACTOR_00300_FIREBALL_LAUNCH_Z;
    // Carry the local launch offset through the parent rotation before detaching.
    gte_SetRotMatrix(&parentCoord->coord);
    gte_ldv0(offset);
    gte_rtv0();
    gte_stlvnl(&scratch->rotated);
    coord->parent       = &gGfxViewCoord;
    coord->coord        = parentCoord->coord;
    coord->coord.t[0]   = parentCoord->coord.t[0] + scratch->rotated.vx;
    coord->coord.t[1]   = parentCoord->coord.t[1] + scratch->rotated.vy;
    coord->coord.t[2]   = parentCoord->coord.t[2] + scratch->rotated.vz;
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
    _actor00300InitFireballAttackBody(task, work, parentWork);
    _actor00300InitFireballSweepBody(task, work);
    work->timer            = ACTOR_00300_FIREBALL_FLIGHT_TICKS;
    work->sweepBody.flags |= (WORLD_COLLISION_BODY_CLIP_TO_GRID_CONTACT | WORLD_COLLISION_BODY_GRID_ENABLED);
    taskDetachFromParent(task);
    task->state = ACTOR_00300_FIREBALL_TASK_FLY;
    SCRATCH_STACK_RELEASE_BLOCK(ActorOffsetScratch);
}

static void Actor00300_Fn04370(Enemy* arg0, Task* arg1)
{
    _Actor00300FireballWork* work;
    GfxCoord*                coord;
    s32                      id;
    s32                      expired;
    s16                      timer;

    coord   = arg1->extra.tmd->coords;
    work    = arg1->work;
    expired = 0;
    switch (gSceneCombatState.actorControl) {
        case SCENE_COMBAT_ACTORS_PAUSED:
            fireballDrawGlow(coord, 0x200);
            return;
        case SCENE_COMBAT_ACTORS_RUNNING:
        default:
            coord->composeStamp = GRAPHICS_COORD_DIRTY;
            coord->coord.t[0]  += (coord->coord.m[0][2] * 0x19) >> 8;
            coord->coord.t[2]  += (coord->coord.m[2][2] * 0x19) >> 8;
            actorRenderComposeCoord(coord);
            fireballDrawGlow(coord, 0x200);
            id = work->sweepContacts[0].key.value;
            if (id != 0 && Gp_RoomParamTables[gGameSession->location.loc.stage - 1]
                                             [gGameSession->location.loc.area - 1][worldCollisionSurfaceClassFromKey(id)]
                                                 ->probePassThrough == WORLD_COLLISION_SURFACE_BLOCK_PROBES) {
                expired = 1;
            }
            worldCollisionClearContacts(work->sweepContacts);
            _actor00300TurnFireballTowardPlayer(arg1);
            timer       = work->timer - 1;
            work->timer = timer;
            if (timer <= 0 || (work->contacts[0].flags & WORLD_COLLISION_CONTACT_OCCUPIED) || expired != 0) {
                effectSpawn(gRoomEffectOrangeBurstId, coord, 0, NULL);
                arg1->state        = 2;
                work->teardownStep = ACTOR_00300_FIREBALL_TEARDOWN_UNLINK;
            }
        case SCENE_COMBAT_ACTORS_HIDDEN:
            return;
    }
}

/// Steers the fireball toward the player's X/Z position by up to 12 angle units.
///
/// Requires a live coordinate-body task and player matrix. Angles use 4096
/// units per turn; the position difference narrows to signed 16 bits before
/// the bearing calculation. Uses the shorter arc, retaining the delta's sign
/// at exactly half a turn. Replaces pitch and roll while retaining translation;
/// the flight caller owns composition invalidation. Borrows `ActorFaceScratch`.
static void _actor00300TurnFireballTowardPlayer(Task* task)
{
    enum { ACTOR_00300_FIREBALL_TURN_RATE = 12 };

    s32               targetYaw;
    GfxCoord*         coord;
    ActorFaceScratch* scratch;
    s16               nextYaw;
    s32               matrixYaw;
    s32               currentYaw;
    s16               yawDelta;
    s32               yawMagnitude;
    s16               turnDirection;
    s16               wrappedDelta;

    coord             = task->extra.coordBody->coord;
    scratch           = SCRATCH_STACK_RESERVE_BLOCK(ActorFaceScratch);
    scratch->delta.vx = gPlayerStatus.coordMtx->t[0] - coord->coord.t[0];
    scratch->delta.vy = 0;
    scratch->delta.vz = gPlayerStatus.coordMtx->t[2] - coord->coord.t[2];
    targetYaw         = ratan2((s16)scratch->delta.vx, (s16)scratch->delta.vz) & ACTOR_TRANSFORM_ANGLE_MASK;
    matrixYaw         = ratan2(coord->coord.m[0][2], coord->coord.m[2][2]) & ACTOR_TRANSFORM_ANGLE_MASK;
    nextYaw           = matrixYaw;
    yawDelta          = targetYaw - matrixYaw;
    yawMagnitude      = yawDelta >= 0 ? yawDelta : -yawDelta;
    turnDirection     = yawDelta;
    if (yawMagnitude <= ACTOR_00300_FIREBALL_TURN_RATE) {
        nextYaw = targetYaw;
    } else {
        if (yawMagnitude > ACTOR_TRANSFORM_ANGLE_HALF_TURN) {
            wrappedDelta = yawDelta - ACTOR_TRANSFORM_ANGLE_TURN;
            if (yawDelta <= 0) {
                wrappedDelta = ACTOR_TRANSFORM_ANGLE_TURN - yawDelta;
            }
            turnDirection = wrappedDelta;
        }
        currentYaw = nextYaw;
        nextYaw    = currentYaw + ACTOR_00300_FIREBALL_TURN_RATE;
        if (turnDirection <= 0) {
            nextYaw = currentYaw - ACTOR_00300_FIREBALL_TURN_RATE;
        }
    }
    scratch->rot.vx = 0;
    scratch->rot.vy = nextYaw;
    scratch->rot.vz = 0;
    RotMatrix(&scratch->rot, &coord->coord);
    SCRATCH_STACK_RELEASE_BLOCK(ActorFaceScratch);
}

#include "../../shared/fireball_ember.inc.c"

void Actor00300_Fn04770(Task* arg0)
{
    EnemyTaskFuncTable3 sp;

    sp = Actor00300_D00004;
    sp.funcs[arg0->state](((Enemy*)arg0->spawnArg2.pointer), arg0);
}

static void Actor00300_Fn047CC(Enemy* arg0, Task* arg1)
{
    _Actor00300Work* work;

    work = arg1->work;
    switch (gSceneCombatState.actorControl) {
        case SCENE_COMBAT_ACTORS_RUNNING:
            arg1->extra.tmd->flags                 = 0;
            work->drainModelTask->extra.tmd->flags = 0;
            arg0->node.state.parts.flags           = work->notLockable != 0;
            break;
        case SCENE_COMBAT_ACTORS_PAUSED:
            _actor00300UpdateLighting(arg1);
            _actor00300DrawShadow(arg1);
            return;
        case SCENE_COMBAT_ACTORS_HIDDEN:
            arg1->extra.tmd->flags                 = TMD_OBJECT_SKIP_ACTIVE_DRAW;
            work->drainModelTask->extra.tmd->flags = TMD_OBJECT_SKIP_ACTIVE_DRAW;
            arg0->node.state.parts.flags           = WORLD_TARGET_NOT_LOCKABLE;
            return;
    }
    if (gGameSession->eventState != 0) {
        _actor00300TickEvent(arg0, arg1);
        return;
    }
    Actor00300_Fn04958(arg0, arg1);
}

/// Updates event-controlled model drawing, animation, lighting and shadow.
///
/// Requires initialized TMD work. While an event is active, the cached draw
/// request replaces the body model flags; other request bits have no effect.
/// The enemy parameter is unused. The drain task applies its own cached flags.
static void _actor00300TickEvent(Enemy* enemy, Task* task)
{
    TmdObject*       model;
    _Actor00300Work* work;
    s16              modelFlags;

    work  = task->work;
    model = task->extra.tmd;
    if (gGameSession->eventState != 0) {
        modelFlags   = ((work->eventDrawFlags & ACTOR_00300_EVENT_DRAW_SHOW) == 0) * TMD_OBJECT_SKIP_ACTIVE_DRAW;
        model->flags = modelFlags;
        if (work->eventDrawFlags & ACTOR_00300_EVENT_DRAW_SKIP_AUTO_BUFFER) {
            model->flags = modelFlags | TMD_OBJECT_SKIP_AUTO_BUFFER;
        }
    }
    _actor00300UpdateAnimation(task);
    _actor00300UpdateLighting(task);
    _actor00300DrawShadow(task);
}

static void Actor00300_Fn04958(Enemy* arg0, Task* arg1)
{
    GfxCoord*        coord;
    _Actor00300Work* work;

    work  = arg1->work;
    coord = arg1->extra.tmd->coords;
    if (work->patrolPoints != NULL) {
        if (arg0->reactionFlags != 0) {
            _actor00300TickStatusReactions(arg1);
        }
        Actor00300_Fn00E54(arg1);
        _actor00300TickAction(arg1);
        if (work->turnRate != 0) {
            _actor00300TurnTowardTargetYaw(arg1);
        }
        _actor00300Move(arg1);
        _actor00300UpdateAnimation(arg1);
        if (work->hitTwistActive != 0) {
            _actor00300ApplyHitTwist(arg1);
        }
        _actor00300PlayAnimationCueSounds(arg1);
        coord->composeStamp = GRAPHICS_COORD_DIRTY;
        actorRenderComposeCoord(coord);
        _actor00300UpdateLighting(arg1);
        _actor00300DrawShadow(arg1);
    }
}

/// Applies pending buildup and damage-over-time reactions to the enemy action.
///
/// Requires initialized work and a live enemy in the second spawn argument.
/// Clears stagger, starts buildup unless already hurt, and reports each damage
/// pulse before subtracting it and entering hurt. An expired status clears its
/// damage-over-time reaction bits. Hit direction and knock-down state are retained.
static void _actor00300TickStatusReactions(Task* task)
{
    _Actor00300Work* work;
    Enemy*           enemy;
    s16              pulseDamage;
    u8               reactionFlags;

    enemy         = task->spawnArg2.pointer;
    reactionFlags = enemy->reactionFlags;
    work          = task->work;
    if (reactionFlags & ENEMY_REACTION_STAGGER) {
        enemy->reactionFlags = reactionFlags & ENEMY_REACTION_STAGGER_CLEAR;
    }
    if ((enemy->reactionFlags & ENEMY_REACTION_BUILDUP) && (work->action != ACTOR_00300_ACTION_HURT)) {
        work->action     = ACTOR_00300_ACTION_BUILDUP;
        work->actionStep = ACTOR_00300_ACTION_BEGIN;
    }
    if (enemy->reactionFlags & ENEMY_REACTION_DAMAGE_OVER_TIME_BITS) {
        pulseDamage     = damageTickEnemyDamageOverTime(enemy);
        work->hitDamage = pulseDamage;
        if (pulseDamage != 0) {
            worldTargetAddReadoutAmount(&enemy->node, (s32)pulseDamage, 0);
            enemy->hp        = (u16)enemy->hp - (u16)work->hitDamage;
            work->action     = ACTOR_00300_ACTION_HURT;
            work->actionStep = ACTOR_00300_ACTION_BEGIN;
        }
        if (damageIsEnemyDamageOverTimeExpired(enemy) != 0) {
            enemy->reactionFlags &= ENEMY_REACTION_DAMAGE_OVER_TIME_CLEAR;
        }
    }
}

#include "../../shared/player_detection_segment.inc.c"

/// State handlers of the task `_actor00300DrainModelTask` dispatches, indexed by
/// `Task::state`: a setup that attaches the coordinate to the parent's and
/// moves to state 1, a scaled drain-model update, and `enemyDestroy`.
static const EnemyTaskFuncTable3 Actor00300_D0003C = {
    {
        _actor00300InitDrainModel,
        _actor00300DrainModelActive,
        enemyDestroy,
    },
};

/// State handlers of the task `Actor00300_Fn0521C` dispatches, indexed by
/// `Task::state`. The first sets the task up and moves it to state 1, the
/// second moves it on to state 2, and the third destroys it once its timer
/// has run out.
static const EnemyTaskFuncTable3 Actor00300_D00048 = {
    {
        _actor00300InitFireball,
        Actor00300_Fn04370,
        _actor00300TeardownFireball,
    },
};

/// Runs the current combat action and retracts a drain interrupted by another action.
///
/// Requires initialized enemy work. `ACTOR_00300_ACTION_*` selects one handler;
/// dead or unrecognized actions run none. Outside drain, positive local-Y scale
/// decreases by 1/16 of full scale per tick; a negative overshoot clears next tick.
static void _actor00300TickAction(Task* task)
{
    enum { ACTOR_00300_DRAIN_SCALE_STEP = ONE / 16 };

    _Actor00300Work* work;

    work = task->work;
    switch (work->action) {
        case ACTOR_00300_ACTION_PATROL:
            _actor00300TickPatrol(task);
            break;
        case ACTOR_00300_ACTION_PURSUE:
            _actor00300TickPursuit(task);
            break;
        case ACTOR_00300_ACTION_FIREBALL:
            _actor00300TickFireballAttack(task);
            break;
        case ACTOR_00300_ACTION_DRAIN:
            _actor00300TickDrainAttack(task);
            break;
        case ACTOR_00300_ACTION_HEAL:
            _actor00300TickHeal(task);
            break;
        case ACTOR_00300_ACTION_HURT:
            _actor00300TickHurt(task);
            break;
        case ACTOR_00300_ACTION_BUILDUP:
            _actor00300TickBuildup(task);
            break;
        case ACTOR_00300_ACTION_RECHARGE:
            _actor00300TickRecharge(task);
            break;
        case ACTOR_00300_ACTION_DEAD:
            break;
    }

    // Retract an interrupted drain, clamping an overshoot on the next tick.
    if (work->action != ACTOR_00300_ACTION_DRAIN) {
        if (work->drainScaleY != 0) {
            if (work->drainScaleY > 0) {
                work->drainScaleY -= ACTOR_00300_DRAIN_SCALE_STEP;
            } else if (work->drainScaleY < 0) {
                work->drainScaleY = 0;
            }
        }
    }
}

/// Holds the actor through a buildup reaction, then resumes moving pursuit after its release animation.
///
/// Requires initialized work and enemy reaction state. Cancels any charge halo
/// and sound, ticks the buildup status, and clears its flag when that status ends.
/// The end animation is selected by the package animation table.
static void _actor00300TickBuildup(Task* task)
{
    enum { ACTOR_00300_BUILDUP_HOLD      = 0,
           ACTOR_00300_BUILDUP_RELEASE   = 1,
           ACTOR_00300_BUILDUP_END_FRAME = 11 };

    _Actor00300Work* work;
    Enemy*           enemy;
    s32              step;
    s32              randomValue;
    EffectWork*      chargeEffect;

    work = task->work;
    step = work->actionStep;
    switch (step) {
        case ACTOR_00300_BUILDUP_HOLD:
            chargeEffect = work->chargeEffect;
            work->speed  = 0;
            work->anim   = ACTOR_00300_ANIM_BUILDUP;
            _actor00300StopCharge(work, chargeEffect);
            if (damageTickEnemyBuildup(task->spawnArg2.pointer) != 0) {
                enemy                 = task->spawnArg2.pointer;
                enemy->reactionFlags &= ENEMY_REACTION_BUILDUP_CLEAR;
                work->anim            = ACTOR_00300_ANIM_BUILDUP_END;
                work->actionStep      = ACTOR_00300_BUILDUP_RELEASE;
            }
            break;
        case ACTOR_00300_BUILDUP_RELEASE:
            if (work->animFrame >= ACTOR_00300_BUILDUP_END_FRAME) {
                work->action     = ACTOR_00300_ACTION_PURSUE;
                work->actionStep = ACTOR_00300_PURSUIT_MOVE;
                randomValue      = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                gRandomLcgState  = randomValue;
                work->timer      = ((u32)randomValue >> 16) & 0x1F;
            }
            break;
    }
}

/// Saves the root position and moves along its facing with a downward floor step.
///
/// Requires initialized TMD work. `speed` is distance per tick in the root
/// parent's coordinate units; matrix axes are Q12. Positive Y advances 128
/// units until the fatal fall's second stage. Collision resolution can restore
/// the saved position. The caller invalidates and composes the root afterward.
static void _actor00300Move(Task* task)
{
    enum { ACTOR_00300_COORD_FRACTION_BITS  = 12,
           ACTOR_00300_FLOOR_STEP           = 128,
           ACTOR_00300_DEATH_FLOOR_RELEASED = 2 };

    _Actor00300Work* work;
    GfxCoord*        coord;

    coord              = task->extra.tmd->coords;
    work               = task->work;
    work->prevPos.vx   = coord->coord.t[0];
    work->prevPos.vy   = coord->coord.t[1];
    work->prevPos.vz   = coord->coord.t[2];
    coord->coord.t[0] += (coord->coord.m[0][2] * work->speed) >> ACTOR_00300_COORD_FRACTION_BITS;
    if (work->deathStage < ACTOR_00300_DEATH_FLOOR_RELEASED) {
        coord->coord.t[1] += ACTOR_00300_FLOOR_STEP;
    }
    coord->coord.t[2] += (coord->coord.m[2][2] * work->speed) >> ACTOR_00300_COORD_FRACTION_BITS;
}

/// Starts a requested body animation or advances the eighteen non-root slots.
///
/// Requires a bound nineteen-part rig and a loaded animation id. A new request
/// resets `animFrame` and blends from each current pose; reaction blending takes
/// eight normal-rate frames. An unchanged request increments the tick counter.
/// The caller must supply a readable package blend-table entry when not reacting.
static void _actor00300UpdateAnimation(Task* task)
{
    enum { ACTOR_00300_REACTION_BLEND_FRAMES = 8 };

    _Actor00300Work* work;
    s32              slotIndex;
    s32              blendFrames;

    work = task->work;
    if (work->anim != work->playingAnim) {
        work->playingAnim = work->anim;
        work->animFrame   = 0;
        if (work->reactionBlend == 0) {
            blendFrames = Actor00300_D16394[work->anim];
        } else {
            blendFrames = ACTOR_00300_REACTION_BLEND_FRAMES;
        }
        for (slotIndex = 1; slotIndex < ARRAY_SIZE(work->rig.slots); slotIndex++) {
            animationSeekSlotWithBlend(&work->rig.anim, slotIndex, work->anim, 0, blendFrames);
        }
    } else {
        work->animFrame++;
        for (slotIndex = 1; slotIndex < ARRAY_SIZE(work->rig.slots); slotIndex++) {
            animationTickSlot(&work->rig.anim, slotIndex);
        }
    }
}

/// Updates the enemy lighting and colour from the composed root world position.
///
/// Requires a live enemy/model with composed coordinates and writable light and
/// colour matrices. The shared lighting routine borrows the three-component
/// position only for this call; it also updates matrices used by the drain child.
static void _actor00300UpdateLighting(Task* task)
{
    GfxCoord* coord;
    VECTOR3   worldPosition;

    coord            = task->extra.tmd->coords;
    worldPosition.vx = coord->workm.t[0];
    worldPosition.vy = coord->workm.t[1];
    worldPosition.vz = coord->workm.t[2];
    worldCoordUpdateActorColor(task->spawnArg2.pointer, &worldPosition, 0, 0);
}

/// Draws the actor ground shadow below body part 3 at the root height.
///
/// Requires composed model coordinates including part 3. X/Z follow that part,
/// while Y follows the root; the centre uses world-coordinate units.
static void _actor00300DrawShadow(Task* task)
{
    enum { ACTOR_00300_SHADOW_HALF_SIZE = 768,
           ACTOR_00300_SHADOW_SHADE     = 128 };

    VECTOR3   shadowCentre;
    GfxCoord* coord;
    GfxCoord* bodyCoord;

    coord           = task->extra.tmd->coords;
    bodyCoord       = coord + 3;
    shadowCentre.vx = bodyCoord->workm.t[0];
    shadowCentre.vy = coord->workm.t[1];
    shadowCentre.vz = bodyCoord->workm.t[2];
    effectDrawGroundShadow(&shadowCentre, ACTOR_00300_SHADOW_HALF_SIZE, ACTOR_00300_SHADOW_SHADE);
}

static void Actor00300_Fn0505C(Task* arg0, MATRIX* arg1, s16 arg2)
{
    GfxCoord*          coord;
    ActorScaleScratch* head;
    ActorScaleScratch* scratch;

    head                                    = SCRATCH_STACK_CURSOR(ActorScaleScratch);
    scratch                                 = head - 1;
    SCRATCH_STACK_CURSOR(ActorScaleScratch) = scratch;
    coord                                   = arg0->extra.tmd->coords;
    scratch->scale.vx                       = ONE;
    scratch->scale.vy                       = arg2;
    scratch->scale.vz                       = ONE;
    coord->coord                            = *arg1;
    gfxSetRotIdentity(&scratch->matrix);
    ScaleMatrix(&scratch->matrix, &scratch->scale);
    MulMatrix(&coord->coord, &scratch->matrix);
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
    SCRATCH_STACK_RELEASE_BLOCK(ActorScaleScratch);
}

/// Dispatches initialization, active scaling and destruction of the drain model.
///
/// The descriptor supplies a one-part TMD child and `spawnArg2.pointer` supplies
/// a live enemy. `Task::state` is 0 setup, 1 active or 2 destroy; it must remain
/// within this three-entry handler table. The parent work outlives the child.
static void _actor00300DrainModelTask(Task* task)
{
    EnemyTaskFuncTable3 handlers;

    handlers = Actor00300_D0003C;
    handlers.funcs[task->state](task->spawnArg2.pointer, task);
}

/// Attaches the drain model to body part 7 and captures its unscaled rest matrix.
///
/// Requires a one-part TMD child of the initialized nineteen-part enemy model.
/// Borrows the parent's light/colour matrices and work for the child's lifetime;
/// starts at zero Y scale and advances the task to active state 1. The enemy
/// argument is unused but retained for the `EnemyTaskFunc` contract.
static void _actor00300InitDrainModel(Enemy* enemy, Task* task)
{
    enum { ACTOR_00300_DRAIN_PARENT_PART  = 7,
           ACTOR_00300_DRAIN_MODEL_ACTIVE = 1 };

    Task*            parent;
    TmdObject*       model;
    GfxCoord*        coord;
    GfxCoord*        parentCoord;
    _Actor00300Work* parentWork;

    parent                    = task->parent;
    model                     = task->extra.tmd;
    parentCoord               = parent->extra.tmd->coords;
    coord                     = model->coords;
    parentWork                = parent->work;
    model->flags              = 0;
    coord->composeStamp       = GRAPHICS_COORD_DIRTY;
    coord->parent             = parentCoord + ACTOR_00300_DRAIN_PARENT_PART;
    model->lightMtx           = &parentWork->lightMtx;
    model->colorMtx           = &parentWork->colorMtx;
    parentWork->drainModelMtx = coord->coord;
    parentWork->drainScaleY   = 0;
    task->state               = ACTOR_00300_DRAIN_MODEL_ACTIVE;
}

void Actor00300_Fn0521C(Task* arg0)
{
    EnemyTaskFuncTable3 sp;

    sp = Actor00300_D00048;
    sp.funcs[arg0->state](((Enemy*)arg0->spawnArg2.pointer), arg0);
}

/// Unlinks a burst fireball's bodies, waits sixty task ticks and destroys it.
///
/// Requires live enemy/task and initialized fireball work. The first teardown
/// step disables both bodies and starts the linger countdown; the second
/// counts it down through signed 16-bit storage. Work and the coordinate body
/// stay owned by the task until `enemyDestroy` releases them. Other steps do nothing.
static void _actor00300TeardownFireball(Enemy* enemy, Task* task)
{
    _Actor00300FireballWork* work;
    u16                      timer;

    work = task->work;
    switch (work->teardownStep) {
        case ACTOR_00300_FIREBALL_TEARDOWN_UNLINK:
            worldCollisionUnlinkBody(&work->body);
            worldCollisionUnlinkBody(&work->sweepBody);
            work->timer        = ACTOR_00300_FIREBALL_LINGER_TICKS;
            work->teardownStep = ACTOR_00300_FIREBALL_TEARDOWN_LINGER;
            return;
        case ACTOR_00300_FIREBALL_TEARDOWN_LINGER:
            timer       = work->timer - 1;
            work->timer = timer;
            if ((s16)timer <= 0) {
                enemyDestroy(enemy, task);
            }
            return;
    }
}

/// Starts an event animation on the enemy's eighteen non-root playback slots.
///
/// Requires initialized rig and a borrowed request with animation id 0..2,
/// mapped to package clips 19..21. A zero blend choice seeks without a transition;
/// otherwise `blendFrames` is passed through in normal-rate frames. Sets both
/// requested and playing ids without resetting the actor's animation-tick counter.
/// Ignores the request's bank and collision fields, message id and second argument.
/// Retains no request pointer and returns 0; ids and frame counts are unchecked.
static s32 _actor00300MsgPlayAnimation(Task* task, s32 messageId, const AnimationPlayRequest* request, s32 unusedSecondArg)
{
    _Actor00300Work* work;
    s32              slotIndex;
    s32              blendFrames;
    s16              animationId;

    work              = task->work;
    animationId       = request->animationId + ACTOR_00300_ANIM_EVENT_BASE;
    work->anim        = animationId;
    work->playingAnim = animationId;
    blendFrames       = 0;
    if (request->blend != ANIMATION_BLEND_RESET) {
        blendFrames = request->blendFrames;
    }
    for (slotIndex = 1; slotIndex < ARRAY_SIZE(work->rig.slots); slotIndex++) {
        animationSeekSlotWithBlend(&work->rig.anim, slotIndex, work->anim, 0, blendFrames);
    }
    return 0;
}

#include "../../shared/actor_messages_place_rot_matrix.inc.c"

/// Replaces model draw flags and caches the request for event ticks and the drain child.
///
/// Requires initialized TMD work. Bit 0 permits active drawing; bit 1 disables
/// automatic buffer allocation. Other model flags are cleared and no buffers
/// are allocated or freed here. The low 16 request bits are retained, including
/// unused bits. Ignores the message id and second argument; returns 0.
static s32 _actor00300MsgSetModelDraw(Task* task, s32 messageId, s32 drawFlags, s32 unusedSecondArg)
{
    TmdObject*       model;
    _Actor00300Work* work;

    model = task->extra.tmd;
    work  = task->work;
    if (!(drawFlags & ACTOR_00300_EVENT_DRAW_SHOW)) {
        model->flags = TMD_OBJECT_SKIP_ACTIVE_DRAW;
    } else {
        model->flags = 0;
    }
    if (drawFlags & ACTOR_00300_EVENT_DRAW_SKIP_AUTO_BUFFER) {
        model->flags |= TMD_OBJECT_SKIP_AUTO_BUFFER;
    }
    work->eventDrawFlags = drawFlags;
    return 0;
}

/// Removes the enemy for any nonzero actor command, leaving command zero alone.
///
/// Requires initialized enemy work and a borrowed command through dispatch.
/// Clears hit-record borrowing, unlinks the target and all four collision bodies,
/// then destroys the enemy and its task. Ignores command context, message id and
/// second argument. Retains no command pointer and returns 0 on either path.
static s32 _actor00300MsgApplyCommand(Task* task, s32 messageId, const ActorCommand* command, s32 unusedSecondArg)
{
    _Actor00300Work* work;
    Enemy*           enemy;

    work  = task->work;
    enemy = task->spawnArg2.pointer;
    if (command->command != 0) {
        enemy->recs = NULL;
        worldTargetUnlinkNode(&enemy->node);
        worldCollisionUnlinkBody(&work->sightBody);
        worldCollisionUnlinkBody(&work->gridBody);
        worldCollisionUnlinkBody(&work->hitBody);
        worldCollisionUnlinkBody(&work->drainBody);
        enemyDestroy(enemy, task);
    }
    return 0;
}
