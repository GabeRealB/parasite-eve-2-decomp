#include "actors/actor_00300.h"

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
#include "../../shared/effects/fireball.h"
#include "../../shared/actor/player_detection.h"
#include "../../shared/actor/actor_messages.h"

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

/// Pursuit-memory countdown reloaded on a clear sight line, engagement or recovery.
///
/// Counts contact-resolution ticks, not animation frames or wall-clock time.
/// A sight contact blocked by an occluder clears the countdown immediately;
/// without a sight contact it decreases towards zero.
#define ACTOR_00300_ALERT_TICKS 450

/// Initial enemy MP balance, stored in signed-halfword work after allocation.
#define ACTOR_00300_MP_AT_SPAWN 40

/// MP requested from the player and credited to the enemy on one drain contact.
///
/// The player loses at most this amount; the enemy gains the full amount even
/// when the player cannot pay. The enemy's signed-halfword balance is not capped.
#define ACTOR_00300_MP_DRAINED 20

/// Enemy MP spent when healing is selected, before the charge can be interrupted.
#define ACTOR_00300_MP_HEAL_COST 20

/// Enemy MP spent on a fireball selection, including a draw that cancels the attack.
#define ACTOR_00300_MP_FIREBALL_COST 5

/// Enemy MP credited after the recharge's 91 active wait ticks, without a cap.
#define ACTOR_00300_MP_RECHARGED 5

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

/// Indices in the enemy task's three-entry state table.
enum {
    ACTOR_00300_TASK_INITIALIZE = 0,
    ACTOR_00300_TASK_ACTIVE     = 1,
    ACTOR_00300_TASK_DEATH      = 2
};

/// Model parts used by the enemy's sensing and hit bodies.
enum {
    ACTOR_00300_SIGHT_PART = 2,
    ACTOR_00300_HIT_PART   = 3
};

/// Drain-contact centre and its touch halo offset in drain-local coordinates.
enum {
    ACTOR_00300_DRAIN_OFFSET_X = -500,
    ACTOR_00300_DRAIN_OFFSET_Y = 500
};

/// Delayed stages of a requested body burst.
enum {
    ACTOR_00300_BURST_NONE      = 0,
    ACTOR_00300_BURST_REQUESTED = 1,
    ACTOR_00300_BURST_READY     = 2
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
    WorldCollisionContact drainContacts[1];  // contact of `drainBody`; a touch requests `ACTOR_00300_MP_DRAINED` from the player and credits that full amount to `mp`
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

/// Maximum running flight ticks before a fireball bursts, including its final move.
///
/// The countdown starts after launch; paused and hidden actor control leave it
/// unchanged. Attack contacts or a blocking sweep surface can end flight earlier.
#define ACTOR_00300_FIREBALL_FLIGHT_TICKS 30

/// Teardown countdown ticks retaining a burst fireball's work and coordinate body.
///
/// Loaded on the first teardown call after unlinking both collision bodies;
/// destruction occurs on the sixtieth subsequent call, independently of actor control.
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

static SVECTOR _gActor00300MineCavernPatrolMode0Points[4];

static SVECTOR _gActor00300MineSecretPassagePatrolMode0Points[2];

static SVECTOR _gActor00300ShelterB1ElevatorHallPatrolMode0Points[2];

static SVECTOR _gActor00300ShelterB1StoreroomPatrolMode0Points[2];

static SVECTOR _gActor00300ShelterB1NorthMaintenanceWalkwayPatrolMode0Points[4];

static SVECTOR _gActor00300ShelterB1MainCorridorPatrolMode0Points[2];

static SVECTOR _gActor00300ShelterB1TransferTunnelPatrolMode0Points[2];

static SVECTOR _gActor00300ShelterB2ElevatorHallPatrolMode0Points[2];

static SVECTOR _gActor00300ShelterB2PodAccessTunnelPatrolMode0Points[4];

static SVECTOR _gActor00300ShelterB3ElevatorHallPatrolMode0Points[2];

static SVECTOR _gActor00300ShelterB4UpperSewerPatrolMode0Points[2];

static SVECTOR _gActor00300NeoArkNorthPromenadePatrolMode0Points[2];

static SVECTOR _gActor00300DryfieldMotelBalconyPatrolMode0Points[4];

static SVECTOR _gActor00300DryfieldNightMainStreetPatrolMode0Points[2];

static void _actor00300TickEvent(Enemy* enemy, Task* task);
static void _actor00300TickCombat(Enemy* enemy, Task* task);

static void _actor00300InitEnemy(Enemy* enemy, Task* task);
static void _actor00300TurnFireballTowardPlayer(Task* task);
static void _actor00300ResolveContacts(Task* task);
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
static void _actor00300TickDeath(Enemy* enemy, Task* task);
static void _actor00300TickActive(Enemy* enemy, Task* task);
static void _actor00300TickStatusReactions(Task* task);
static void _actor00300TickAction(Task* task);
static void _actor00300TickBuildup(Task* task);
static void _actor00300Move(Task* task);
static void _actor00300UpdateAnimation(Task* task);
static void _actor00300UpdateLighting(Task* task);
static void _actor00300DrawShadow(Task* task);
static void _actor00300SetRootScaleY(Task* task, const MATRIX* unscaledMatrix, s16 scaleY);
static void _actor00300InitDrainModel(Enemy* enemy, Task* task);
static void _actor00300TeardownFireball(Enemy* enemy, Task* task);

// Typed callback views for the task message dispatcher.

/// Enemy task states: 0 initialization, 1 event/combat update, 2 death.
///
/// Indexed by `Task::state` in 0..2; each callback receives the live enemy and task.
/// The complete table is copied by value before dispatch.
static const EnemyTaskFuncTable3 _gActor00300EnemyTaskStates = {
    {
        _actor00300InitEnemy,
        _actor00300TickActive,
        _actor00300TickDeath,
    },
};

static AnimationSet _gStingerIdleClip;
static AnimationSet _gStingerBrainWalkClip;
static AnimationSet _gStingerTurnClip;
static AnimationSet _gStingerChargeClip;
static AnimationSet _gStingerReleaseClip;
static AnimationSet _gStingerRecoverClip;
static AnimationSet _gStingerBrainDrainClip;
static AnimationSet _gStingerStaggerClip;
static AnimationSet _gStingerBrainFlinchBackClip;
static AnimationSet _gStingerBrainFlinchFrontClip;
static AnimationSet _gStingerBrainKnockDownBackClip;
static AnimationSet _gStingerBrainKnockDownFrontClip;
static AnimationSet _gStingerBrainGetUpBackClip;
static AnimationSet _gStingerBrainGetUpFrontClip;
static AnimationSet _gStingerStunnedClip;
static AnimationSet _gStingerBrainStingerAnim16Clip;
static AnimationSet _gStingerBrainStingerAnim17Clip;
static AnimationSet _gStingerStunRecoverClip;
static AnimationSet _gStingerBrainEvent1Clip;
static AnimationSet _gStingerBrainEvent2Clip;
static AnimationSet _gStingerBrainEvent3Clip;
static TmdSource    _gStingerBody;
static TmdSource    _gStingerBrainDrainArm;
static s32          _actor00300MsgPlayAnimation(Task* task, s32 messageId, const AnimationPlayRequest* request, s32 unusedSecondArg);
static s32          _actor00300MsgSetModelDraw(Task* task, s32 messageId, s32 drawFlags, s32 unusedSecondArg);
static s32          _actor00300MsgApplyCommand(Task* task, s32 messageId, const ActorCommand* command, s32 unusedSecondArg);
static void         _actor00300EnemyTask(Task* task);
static void         _actor00300DrainModelTask(Task* task);
static void         _actor00300FireballTask(Task* task);

#include "assets/enemies/stinger/body.inc"

#include "assets/enemies/stinger/brain/drain_arm.inc"

#include "assets/enemies/stinger/brain/burst_head.inc"

#include "assets/enemies/stinger/brain/burst_part_1.inc"

#include "assets/enemies/stinger/brain/burst_part_2.inc"

#include "assets/enemies/stinger/brain/burst_part_3.inc"

#include "assets/enemies/stinger/brain/burst_part_4.inc"

#include "assets/enemies/stinger/brain/package_1_clips.inc"

/// Indices shared by the attack properties and fireball charge durations.
enum {
    ACTOR_00300_ATTACK_DRAIN              = 0,
    ACTOR_00300_ATTACK_FIREBALL_POISON    = 1,
    ACTOR_00300_ATTACK_FIREBALL_PARALYSIS = 2,
    ACTOR_00300_ATTACK_FIREBALL_DARKNESS  = 3
};

/// Zero in the fireball draw table skips the attack after its MP cost is paid.
enum { ACTOR_00300_FIREBALL_NO_ATTACK = 0 };

/// Collision attacks: drain with silence, then poison, paralysis and darkness fireballs.
///
/// Rows are `ACTOR_00300_ATTACK_*`; powers precede the player's HP-band damage scale.
/// The enemy parameter record borrows this table; collision bodies store packed copies.
static DamageAttack _gActor00300Attacks[4] = {
    [ACTOR_00300_ATTACK_DRAIN]              = { 25, GAME_ACTOR_REACTION_SILENCE },
    [ACTOR_00300_ATTACK_FIREBALL_POISON]    = { 35, GAME_ACTOR_REACTION_POISON },
    [ACTOR_00300_ATTACK_FIREBALL_PARALYSIS] = { 35, GAME_ACTOR_REACTION_PARALYSIS },
    [ACTOR_00300_ATTACK_FIREBALL_DARKNESS]  = { 35, GAME_ACTOR_REACTION_DARKNESS },
};

/// Shared health, battle rewards and hit-reaction tuning of this enemy kind.
///
/// Each live enemy borrows the record; its maximum HP also sets the heal threshold.
static EnemyParams _gActor00300EnemyParams = {
    .attacks              = _gActor00300Attacks,
    .hpMax                = 400,
    .exp                  = 105,
    .bp                   = 158,
    .mp                   = 8,
    .critChance           = 100,
    .buildupSteps         = 10,
    .damageOverTimeChance = 100,
    .damageOverTimeTicks  = 7,
};

/// Charge-animation ticks before release, indexed by `ACTOR_00300_ATTACK_*`.
///
/// Fireballs use rows 1..3; the unused drain row is zero. The duration also
/// determines the charge halo's lifetime after its fixed lead-in.
static s16 _gActor00300FireballChargeTicks[4] = {
    [ACTOR_00300_ATTACK_DRAIN]              = 0,
    [ACTOR_00300_ATTACK_FIREBALL_POISON]    = 98,
    [ACTOR_00300_ATTACK_FIREBALL_PARALYSIS] = 150,
    [ACTOR_00300_ATTACK_FIREBALL_DARKNESS]  = 120,
};

/// Sixteen fireball draw choices: one skip, four poison, six paralysis, five darkness.
///
/// The low four bits of an upper-halfword LCG draw select a row. Nonzero
/// results are attack indices 1..3, shared with the charge-duration table.
static s16 _gActor00300FireballAttackDraws[16] = {
    ACTOR_00300_FIREBALL_NO_ATTACK,
    ACTOR_00300_ATTACK_FIREBALL_POISON,
    ACTOR_00300_ATTACK_FIREBALL_POISON,
    ACTOR_00300_ATTACK_FIREBALL_POISON,
    ACTOR_00300_ATTACK_FIREBALL_POISON,
    ACTOR_00300_ATTACK_FIREBALL_PARALYSIS,
    ACTOR_00300_ATTACK_FIREBALL_PARALYSIS,
    ACTOR_00300_ATTACK_FIREBALL_PARALYSIS,
    ACTOR_00300_ATTACK_FIREBALL_PARALYSIS,
    ACTOR_00300_ATTACK_FIREBALL_PARALYSIS,
    ACTOR_00300_ATTACK_FIREBALL_PARALYSIS,
    ACTOR_00300_ATTACK_FIREBALL_DARKNESS,
    ACTOR_00300_ATTACK_FIREBALL_DARKNESS,
    ACTOR_00300_ATTACK_FIREBALL_DARKNESS,
    ACTOR_00300_ATTACK_FIREBALL_DARKNESS,
    ACTOR_00300_ATTACK_FIREBALL_DARKNESS,
};

/// Room keys selecting a cyclic patrol route, ending at route row zero.
///
/// Each count is the full route extent in points. A room's placement mode must
/// select a loaded column of `_gActor00300PatrolRoutes`; an absent room leaves
/// the initialized enemy with no patrol route.
static _Actor00300PatrolRoom _gActor00300PatrolRooms[15] = {
    { 1, GAME_STAGE_MINE_SHELTER, GAME_AREA_MINE_CAVERN, ARRAY_SIZE(_gActor00300MineCavernPatrolMode0Points) },
    { 2, GAME_STAGE_MINE_SHELTER, GAME_AREA_MINE_SECRET_PASSAGE, ARRAY_SIZE(_gActor00300MineSecretPassagePatrolMode0Points) },
    { 3, GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B1_ELEVATOR_HALL, ARRAY_SIZE(_gActor00300ShelterB1ElevatorHallPatrolMode0Points) },
    { 4, GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B1_STOREROOM, ARRAY_SIZE(_gActor00300ShelterB1StoreroomPatrolMode0Points) },
    { 5, GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B1_NORTH_MAINTENANCE_WALKWAY, ARRAY_SIZE(_gActor00300ShelterB1NorthMaintenanceWalkwayPatrolMode0Points) },
    { 6, GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B1_MAIN_CORRIDOR, ARRAY_SIZE(_gActor00300ShelterB1MainCorridorPatrolMode0Points) },
    { 7, GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B1_TRANSFER_TUNNEL, ARRAY_SIZE(_gActor00300ShelterB1TransferTunnelPatrolMode0Points) },
    { 8, GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B2_ELEVATOR_HALL, ARRAY_SIZE(_gActor00300ShelterB2ElevatorHallPatrolMode0Points) },
    { 9, GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B2_POD_ACCESS_TUNNEL, ARRAY_SIZE(_gActor00300ShelterB2PodAccessTunnelPatrolMode0Points) },
    { 10, GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B3_ELEVATOR_HALL, ARRAY_SIZE(_gActor00300ShelterB3ElevatorHallPatrolMode0Points) },
    { 11, GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B4_UPPER_SEWER, ARRAY_SIZE(_gActor00300ShelterB4UpperSewerPatrolMode0Points) },
    { 12, GAME_STAGE_SHELTER_NEO_ARK, GAME_AREA_NEO_ARK_NORTH_PROMENADE, ARRAY_SIZE(_gActor00300NeoArkNorthPromenadePatrolMode0Points) },
    { 13, GAME_STAGE_DRYFIELD, GAME_AREA_DRYFIELD_MOTEL_BALCONY, ARRAY_SIZE(_gActor00300DryfieldMotelBalconyPatrolMode0Points) },
    { 14, GAME_STAGE_DRYFIELD_NIGHT, GAME_AREA_DRYFIELD_MAIN_STREET, ARRAY_SIZE(_gActor00300DryfieldNightMainStreetPatrolMode0Points) },
    { 0, GAME_STAGE_NONE, 0, 0 },
};

/// Cyclic patrol points for Mine Cavern placements in mode 0.
///
/// X/Z use the enemy root's parent coordinates; Y is ignored.
static SVECTOR _gActor00300MineCavernPatrolMode0Points[4] = {
    { 0x413C, 0, 1500, 0 },
    { 0x413C, 0, 7600, 0 },
    { 1800, 0, 7600, 0 },
    { 1800, 0, 1500, 0 },
};

/// Cyclic patrol points for Mine Cavern placements in mode 1.
///
/// X/Z use the enemy root's parent coordinates; Y is ignored.
static SVECTOR _gActor00300MineCavernPatrolMode1Points[4] = {
    { 0x2710, 0, 6000, 0 },
    { 7800, 0, 6000, 0 },
    { 0x2710, 0, 6000, 0 },
    { 7800, 0, 6000, 0 },
};

/// Cyclic patrol points for Mine Secret Passage placements in mode 0.
///
/// X/Z use the enemy root's parent coordinates; Y is ignored.
static SVECTOR _gActor00300MineSecretPassagePatrolMode0Points[2] = {
    { 1500, 0, 0x2710, 0 },
    { 1500, 0, 3500, 0 },
};

/// Cyclic patrol points for Mine Secret Passage placements in mode 1.
///
/// X/Z use the enemy root's parent coordinates; Y is ignored.
static SVECTOR _gActor00300MineSecretPassagePatrolMode1Points[2] = {
    { 3000, 0, 3500, 0 },
    { 0x2904, 0, 3500, 0 },
};

/// Cyclic patrol points for Shelter B1 Elevator Hall placements in mode 0.
///
/// X/Z use the enemy root's parent coordinates; Y is ignored.
static SVECTOR _gActor00300ShelterB1ElevatorHallPatrolMode0Points[2] = {
    { -9000, 0, 0, 0 },
    { -2000, 0, 0, 0 },
};

/// Cyclic patrol points for Shelter B1 Elevator Hall placements in mode 1.
///
/// X/Z use the enemy root's parent coordinates; Y is ignored.
static SVECTOR _gActor00300ShelterB1ElevatorHallPatrolMode1Points[2] = {
    { 7500, 0, 0, 0 },
    { -1000, 0, 0, 0 },
};

/// Cyclic patrol points for Shelter B1 Storeroom placements in mode 0.
///
/// X/Z use the enemy root's parent coordinates; Y is ignored.
static SVECTOR _gActor00300ShelterB1StoreroomPatrolMode0Points[2] = {
    { -1000, 0, 1300, 0 },
    { 4000, 0, 1300, 0 },
};

/// Cyclic patrol points for Shelter B1 Storeroom placements in mode 1.
///
/// X/Z use the enemy root's parent coordinates; Y is ignored.
static SVECTOR _gActor00300ShelterB1StoreroomPatrolMode1Points[2] = {
    { 4000, 0, -1300, 0 },
    { -1000, 0, -1300, 0 },
};

/// Cyclic patrol points for Shelter B1 North Maintenance Walkway placements in mode 0.
///
/// X/Z use the enemy root's parent coordinates; Y is ignored.
static SVECTOR _gActor00300ShelterB1NorthMaintenanceWalkwayPatrolMode0Points[4] = {
    { 2000, 0, -3500, 0 },
    { 2000, 0, 4000, 0 },
    { -2000, 0, 4000, 0 },
    { 2000, 0, 4000, 0 },
};

/// Cyclic patrol points for Shelter B1 Main Corridor placements in mode 0.
///
/// X/Z use the enemy root's parent coordinates; Y is ignored.
static SVECTOR _gActor00300ShelterB1MainCorridorPatrolMode0Points[2] = {
    { 0, 0, -8000, 0 },
    { 0, 0, -2000, 0 },
};

/// Cyclic patrol points for Shelter B1 Main Corridor placements in mode 1.
///
/// X/Z use the enemy root's parent coordinates; Y is ignored.
static SVECTOR _gActor00300ShelterB1MainCorridorPatrolMode1Points[2] = {
    { -2000, 0, -0x2710, 0 },
    { 2000, 0, -0x2710, 0 },
};

/// Cyclic patrol points for Shelter B1 Transfer Tunnel placements in mode 0.
///
/// X/Z use the enemy root's parent coordinates; Y is ignored.
static SVECTOR _gActor00300ShelterB1TransferTunnelPatrolMode0Points[2] = {
    { 1000, 0, 80, 0 },
    { 6000, 0, 80, 0 },
};

/// Cyclic patrol points for Shelter B2 Elevator Hall placements in mode 0.
///
/// X/Z use the enemy root's parent coordinates; Y is ignored.
static SVECTOR _gActor00300ShelterB2ElevatorHallPatrolMode0Points[2] = {
    { 6500, 0, 0, 0 },
    { 6500, 0, 3300, 0 },
};

/// Cyclic patrol points for Shelter B2 Elevator Hall placements in mode 1.
///
/// X/Z use the enemy root's parent coordinates; Y is ignored.
static SVECTOR _gActor00300ShelterB2ElevatorHallPatrolMode1Points[2] = {
    { 4000, 0, 0, 0 },
    { -4000, 0, 0, 0 },
};

/// Cyclic patrol points for Shelter B2 Pod Access Tunnel placements in mode 0.
///
/// X/Z use the enemy root's parent coordinates; Y is ignored.
static SVECTOR _gActor00300ShelterB2PodAccessTunnelPatrolMode0Points[4] = {
    { 1700, 0, -1700, 0 },
    { 4500, 0, -1700, 0 },
    { 1700, 0, -1700, 0 },
    { 1700, 0, -7000, 0 },
};

/// Cyclic patrol points for Shelter B3 Elevator Hall placements in mode 0.
///
/// X/Z use the enemy root's parent coordinates; Y is ignored.
static SVECTOR _gActor00300ShelterB3ElevatorHallPatrolMode0Points[2] = {
    { 0, 0, -2000, 0 },
    { -5000, 0, -2000, 0 },
};

/// Cyclic patrol points for Shelter B3 Elevator Hall placements in mode 1.
///
/// X/Z use the enemy root's parent coordinates; Y is ignored.
static SVECTOR _gActor00300ShelterB3ElevatorHallPatrolMode1Points[2] = {
    { 1000, 0, -2000, 0 },
    { 6000, 0, -2000, 0 },
};

/// Cyclic patrol points for Shelter B4 Upper Sewer placements in mode 0.
///
/// X/Z use the enemy root's parent coordinates; Y is ignored.
static SVECTOR _gActor00300ShelterB4UpperSewerPatrolMode0Points[2] = {
    { -6500, -2000, 9000, 0 },
    { 1000, -2000, 9000, 0 },
};

/// Cyclic patrol points for Neo Ark North Promenade placements in mode 0.
///
/// X/Z use the enemy root's parent coordinates; Y is ignored.
static SVECTOR _gActor00300NeoArkNorthPromenadePatrolMode0Points[2] = {
    { 0x2710, 0, 0x2CEC, 0 },
    { 4500, 0, 0x2CEC, 0 },
};

/// Cyclic patrol points for Neo Ark North Promenade placements in mode 1.
///
/// X/Z use the enemy root's parent coordinates; Y is ignored.
static SVECTOR _gActor00300NeoArkNorthPromenadePatrolMode1Points[2] = {
    { 0x2C24, 0, 0x2710, 0 },
    { 0x2C24, 0, 4500, 0 },
};

/// Cyclic patrol points for Dryfield Motel Balcony placements in mode 0.
///
/// X/Z use the enemy root's parent coordinates; Y is ignored.
static SVECTOR _gActor00300DryfieldMotelBalconyPatrolMode0Points[4] = {
    { -6000, 0, -4000, 0 },
    { -6000, 0, 2000, 0 },
    { -0x2CEC, 0, 2000, 0 },
    { -6000, 0, 2000, 0 },
};

/// Cyclic patrol points for Dryfield Motel Balcony placements in mode 1.
///
/// X/Z use the enemy root's parent coordinates; Y is ignored.
static SVECTOR _gActor00300DryfieldMotelBalconyPatrolMode1Points[4] = {
    { -6000, 0, 4500, 0 },
    { -6000, 0, 0x2904, 0 },
    { -1000, 0, 0x2904, 0 },
    { -6000, 0, 0x2904, 0 },
};

/// Cyclic patrol points for Dryfield Night Main Street placements in mode 0.
///
/// X/Z use the enemy root's parent coordinates; Y is ignored.
static SVECTOR _gActor00300DryfieldNightMainStreetPatrolMode0Points[2] = {
    { -2100, 0, -2100, 0 },
    { -1600, 0, 4000, 0 },
};

/// Cyclic patrol points for Dryfield Night Main Street placements in mode 1.
///
/// X/Z use the enemy root's parent coordinates; Y is ignored.
static SVECTOR _gActor00300DryfieldNightMainStreetPatrolMode1Points[2] = {
    { -4700, 0, 1750, 0 },
    { -1000, 0, 6300, 0 },
};

/// Room patrol routes indexed by route row 0..14 and placement mode 0..1.
///
/// Row zero is unavailable. Other rows belong to `_gActor00300PatrolRooms`;
/// each non-NULL array contains that room's point count. Authored placements
/// select only loaded columns. X/Z use root-parent units; Y is ignored.
static SVECTOR* _gActor00300PatrolRoutes[15][2] = {
    { NULL, NULL },
    { _gActor00300MineCavernPatrolMode0Points, _gActor00300MineCavernPatrolMode1Points },
    { _gActor00300MineSecretPassagePatrolMode0Points, _gActor00300MineSecretPassagePatrolMode1Points },
    { _gActor00300ShelterB1ElevatorHallPatrolMode0Points, _gActor00300ShelterB1ElevatorHallPatrolMode1Points },
    { _gActor00300ShelterB1StoreroomPatrolMode0Points, _gActor00300ShelterB1StoreroomPatrolMode1Points },
    { _gActor00300ShelterB1NorthMaintenanceWalkwayPatrolMode0Points, NULL },
    { _gActor00300ShelterB1MainCorridorPatrolMode0Points, _gActor00300ShelterB1MainCorridorPatrolMode1Points },
    { _gActor00300ShelterB1TransferTunnelPatrolMode0Points, NULL },
    { _gActor00300ShelterB2ElevatorHallPatrolMode0Points, _gActor00300ShelterB2ElevatorHallPatrolMode1Points },
    { _gActor00300ShelterB2PodAccessTunnelPatrolMode0Points, NULL },
    { _gActor00300ShelterB3ElevatorHallPatrolMode0Points, _gActor00300ShelterB3ElevatorHallPatrolMode1Points },
    { _gActor00300ShelterB4UpperSewerPatrolMode0Points, NULL },
    { _gActor00300NeoArkNorthPromenadePatrolMode0Points, _gActor00300NeoArkNorthPromenadePatrolMode1Points },
    { _gActor00300DryfieldMotelBalconyPatrolMode0Points, _gActor00300DryfieldMotelBalconyPatrolMode1Points },
    { _gActor00300DryfieldNightMainStreetPatrolMode0Points, _gActor00300DryfieldNightMainStreetPatrolMode1Points },
};

TaskDesc gActor00300Tasks[3] = {
    { { { TASK_BODY_TMD, TASK_PRIORITY_ACTOR } }, _actor00300EnemyTask, { .model = &_gStingerBody } },
    { { { TASK_BODY_TMD, TASK_PRIORITY_ACTOR } }, _actor00300DrainModelTask, { .model = &_gStingerBrainDrainArm } },
    { { { TASK_BODY_COORD, TASK_PRIORITY_ACTOR } }, _actor00300FireballTask, { .value = 0 } },
};

/// Enemy animation, placement, drawing and command messages with an end sentinel.
///
/// Installed for the enemy task; pointer payloads remain borrowed during dispatch.
static TaskMessageEntry _gActor00300Messages[5] = {
    { ACTOR_MESSAGE_PLAY_ANIMATION, _actor00300MsgPlayAnimation },
    { ACTOR_MESSAGE_PLACE, actorMsgPlaceRotMatrix },
    { ACTOR_MESSAGE_SET_MODEL_DRAW, _actor00300MsgSetModelDraw },
    { ACTOR_COMMAND_MESSAGE_APPLY, _actor00300MsgApplyCommand },
    { TASK_MESSAGE_TABLE_END, NULL },
};

/// Stinger body clips indexed by combat requests and the three event animations.
///
/// Entries 1..21 are loaded sets; entry 0 is unplayable. Event requests 0..2
/// select entries 19..21. The model tracks borrow these sets while this overlay lives.
#include "assets/enemies/stinger/brain/bank.inc"

/// Normal-rate blend frames for combat animations 0..18; zero starts without a transition.
///
/// Indexed by `ACTOR_00300_ANIM_*` when reaction blending is inactive.
/// Event clips 19..21 are started directly with the message's blend duration.
static s16 _gActor00300AnimationBlendFrames[19] = {
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

static TmdSource _gStingerBrainBurstHead;

static TmdSource _gStingerBrainBurstPart1;

static TmdSource _gStingerBrainBurstPart2;

static TmdSource _gStingerBrainBurstPart3;

static TmdSource _gStingerBrainBurstPart4;

static inline s16  _actor00300TiltMagnitude(s8 randomByte);
static void        _actor00300SpawnBurstFragments(Task* task);
static inline void _actor00300UpdateDrainModel(Task* task);
static void        _actor00300DrainModelActive(Enemy* enemy, Task* task);
static void        _actor00300InitFireball(Enemy* enemy, Task* task);
static void        _actor00300FlyFireball(Enemy* enemy, Task* task);

#include "../../shared/effects/fireball.inc.c"

/// Binds the drain model's texture offsets from the current area's enemy placement.
///
/// Scoped to `_actor00300InitEnemy`: captures live enemy/drainEnemy pointers and
/// the caller's drainModel, locationKey, sessionLocation, placementKey,
/// placementIndex, currentView and placement temporaries. The live placement
/// index is in 0..15; the synchronized location selects its variant. The child
/// and placement must exist. Rebuilds both primitive-buffer halves in sequence
/// if a buffer is present. No allocation, argument evaluation or early exit.
/// Invoke as a standalone compound statement inside a braced context.
#define ACTOR_00300_BIND_DRAIN_TEXTURE()                                                 \
    {                                                                                    \
        placementKey          = enemy->placeKey;                                         \
        drainModel            = drainEnemy->task->extra.tmd;                             \
        sessionLocation       = &gGameSession->location;                                 \
        locationKey.stage     = sessionLocation->stage;                                  \
        locationKey.area      = sessionLocation->area;                                   \
        locationKey.roomState = sessionLocation->roomState;                              \
        currentView           = gGameSession->location.view;                             \
        placementIndex        = placementKey >> ENEMY_PLACE_INDEX_SHIFT;                 \
        locationKey.view      = currentView;                                             \
        areaSyncLocationVariant(&locationKey);                                           \
        placement =                                                                      \
            AREA_PLACEMENT_AT(areaGetVariant(&locationKey)->placements, placementIndex); \
        drainModel->texturePageOffset = placement->texturePageOffset;                    \
        drainModel->clutRowOffset     = placement->clutRowOffset;                        \
        if (drainModel->buffer != NULL) {                                                \
            tmdBuildBufferHalf(drainModel);                                              \
            tmdBuildBufferHalf(drainModel);                                              \
        }                                                                                \
    }

/// Allocates the enemy's rig and collision work, spawns its drain attachment and enters active state.
///
/// Requires a nineteen-part TMD enemy and a live placement. A listed room uses
/// placement mode 0..1 to select its patrol route; other rooms leave the route
/// NULL. Work and the four contact tables live until enemy destruction, and the
/// drain child borrows the work's matrices. Allocation failure destroys the
/// enemy; the retained path assumes the drain-child spawn succeeds.
static void _actor00300InitEnemy(Enemy* enemy, Task* task)
{
    enum { ACTOR_00300_SPAWN_WAIT_TICKS    = 10,
           ACTOR_00300_DRAIN_DESCRIPTOR    = 1,
           ACTOR_00300_HIT_EFFECT_ARG_LOW  = 768,
           ACTOR_00300_HIT_EFFECT_ARG_HIGH = 2,
           ACTOR_00300_SIGHT_LENGTH        = 9000,
           ACTOR_00300_SIGHT_FAR_RADIUS    = 4000,
           ACTOR_00300_SIGHT_NEAR_RADIUS   = 1000,
           ACTOR_00300_BODY_KEY            = WORLD_COLLISION_CONTACT_ENEMY_BODY | 3,
           ACTOR_00300_HIT_RADIUS          = 350,
           ACTOR_00300_GRID_RADIUS         = 500,
           ACTOR_00300_DRAIN_RADIUS        = 700,
           ACTOR_00300_SCENE_MUSIC_ENTRY   = 10 };

    GameLocationKey        locationKey;
    Enemy*                 drainEnemy;
    WorldCollisionContact* sightContacts;
    WorldCollisionContact* hitContacts;
    WorldCollisionContact* gridContacts;
    WorldCollisionContact* drainContacts;
    _Actor00300Work*       work;
    TmdObject*             drainModel;
    s32                    patrolRoomIndex;
    Task*                  drainTask;
    s32                    slotIndex;
    u32                    placementIndex;
    u32                    placementKey;
    u8                     currentView;
    GameLocationKey*       sessionLocation;
    AreaPlacement*         placement;
    TmdObject*             model;
    GfxCoord*              rootCoord;
    GfxCoord*              bodyCoords;

    model     = task->extra.tmd;
    rootCoord = model->coords;
    // Own the zeroed work; an absent room route leaves combat updates inactive.
    work = memCalloc(sizeof(_Actor00300Work), 0);
    if (work == NULL) {
        enemyDestroy(enemy, task);
        return;
    }
    task->work             = work;
    work->patrolPoints     = NULL;
    work->patrolPointCount = 0;
    for (patrolRoomIndex = 0; _gActor00300PatrolRooms[patrolRoomIndex].routeRow != 0; patrolRoomIndex++) {
        if ((gGameSession->location.stage == _gActor00300PatrolRooms[patrolRoomIndex].stage) &&
            (gGameSession->location.area == _gActor00300PatrolRooms[patrolRoomIndex].area)) {
            work->patrolPoints =
                _gActor00300PatrolRoutes[_gActor00300PatrolRooms[patrolRoomIndex].routeRow]
                                        [enemy->place->mode];
            work->patrolPointCount = _gActor00300PatrolRooms[patrolRoomIndex].pointCount;
        }
    }
    work->notLockable       = enemy->place->variant;
    model->flags            = 0;
    rootCoord->composeStamp = GRAPHICS_COORD_DIRTY;
    model->lightMtx         = &work->lightMtx;
    model->colorMtx         = &work->colorMtx;
    enemy->field_4          = &rootCoord->coord;
    enemy->field_48         = 0;
    worldTargetLinkNode(&enemy->node);
    slotIndex                     = 1;
    bodyCoords                    = task->extra.tmd->coords;
    enemy->bodyPos.vx             = 0;
    enemy->bodyPos.vy             = 0;
    enemy->bodyPos.vz             = 0;
    enemy->param                  = &_gActor00300EnemyParams;
    enemy->recs                   = work->hitContacts;
    enemy->coord                  = bodyCoords + ACTOR_00300_HIT_PART;
    enemy->hp                     = (s16)_gActor00300EnemyParams.hpMax;
    work->hitEffectArg.coord      = task->extra.tmd->coords + ACTOR_00300_HIT_PART;
    work->hitEffectArg.spawnArgLo = ACTOR_00300_HIT_EFFECT_ARG_LOW;
    work->hitEffectArg.spawnArgHi = ACTOR_00300_HIT_EFFECT_ARG_HIGH;
    animationInitContext(&work->rig.anim, _gActor00300StingerAnimationSets, model,
                         work->rig.poses, work->rig.slots);
    do {
        animationResetSlot(&work->rig.anim, slotIndex, ACTOR_00300_ANIM_STAND);
        slotIndex += 1;
    } while (slotIndex < ARRAY_SIZE(work->rig.slots));
    sceneAcquireBattleRef(0);
    work->savedRootMtx = rootCoord->coord;
    work->timer        = ACTOR_00300_SPAWN_WAIT_TICKS;
    work->mp           = ACTOR_00300_MP_AT_SPAWN;
    // The drain child borrows this work and the placement texture offsets.
    drainEnemy = enemySpawnFromTable(gActor00300Tasks, ACTOR_00300_DRAIN_DESCRIPTOR, 0, enemy);
    ACTOR_00300_BIND_DRAIN_TEXTURE();
    // Register sensing, hit, floor and drain bodies before enabling their passes.
    drainTask                     = drainEnemy->task;
    work->sightCapsule.ends[0].vz = ACTOR_00300_SIGHT_LENGTH;
    work->sightCapsule.end0Radius = ACTOR_00300_SIGHT_FAR_RADIUS;
    sightContacts                 = work->sightContacts;
    work->sightCapsule.ends[0].vx = 0;
    work->sightCapsule.ends[0].vy = 0;
    work->sightCapsule.ends[1].vx = 0;
    work->sightCapsule.ends[1].vy = 0;
    work->sightCapsule.ends[1].vz = 0;
    work->sightCapsule.end1Radius = ACTOR_00300_SIGHT_NEAR_RADIUS;
    work->sightCapsule.contacts   = sightContacts;
    work->drainModelTask          = drainTask;
    work->sightBody.coord =
        task->extra.tmd->coords + ACTOR_00300_SIGHT_PART;
    work->sightBody.context.capsule = &work->sightCapsule;
    work->sightBody.pos.vx          = 0;
    work->sightBody.pos.vy          = 0;
    work->sightBody.pos.vz          = 0;
    work->sightBody.key             = 0;
    work->sightBody.radius          = 0;
    work->sightBody.flags           = WORLD_COLLISION_BODY_CAPSULE;
    worldCollisionLinkBody(WORLD_COLLISION_LIST_ENEMY_ATTACKS, &work->sightBody);
    worldCollisionInitContacts(sightContacts, ARRAY_SIZE(work->sightContacts), 0);
    hitContacts                    = work->hitContacts;
    work->sightBody.flags          = work->sightBody.flags | (WORLD_COLLISION_BODY_CLIP_TO_GRID_CONTACT | WORLD_COLLISION_BODY_SINGLE_CONTACT | WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED);
    work->hitBody.coord            = task->extra.tmd->coords + ACTOR_00300_HIT_PART;
    work->hitBody.context.contacts = hitContacts;
    work->hitBody.pos.vx           = 0;
    work->hitBody.pos.vy           = 0;
    work->hitBody.pos.vz           = 0;
    work->hitBody.key              = ACTOR_00300_BODY_KEY;
    work->hitBody.radius           = ACTOR_00300_HIT_RADIUS;
    work->hitBody.flags            = WORLD_COLLISION_BODY_SPHERE;
    worldCollisionLinkBody(WORLD_COLLISION_LIST_ENEMY_BODIES, &work->hitBody);
    worldCollisionInitContacts(hitContacts, ARRAY_SIZE(work->hitContacts), 0);
    work->hitBody.flags             = work->hitBody.flags | WORLD_COLLISION_BODY_PAIR_ENABLED;
    work->gridBody.coord            = task->extra.tmd->coords;
    gridContacts                    = work->gridContacts;
    work->gridBody.key              = ACTOR_00300_BODY_KEY;
    work->gridBody.context.contacts = gridContacts;
    work->gridBody.pos.vx           = 0;
    work->gridBody.pos.vy           = -ACTOR_00300_GRID_RADIUS;
    work->gridBody.pos.vz           = 0;
    work->gridBody.radius           = ACTOR_00300_GRID_RADIUS;
    work->gridBody.flags            = WORLD_COLLISION_BODY_SPHERE;
    worldCollisionLinkBody(WORLD_COLLISION_LIST_ENEMY_BODIES, &work->gridBody);
    worldCollisionInitContacts(gridContacts, ARRAY_SIZE(work->gridContacts), 0);
    work->gridBody.flags             = work->gridBody.flags | (WORLD_COLLISION_BODY_FLOOR_QUERY | WORLD_COLLISION_BODY_GRID_ENABLED);
    work->drainBody.coord            = drainEnemy->task->extra.tmd->coords;
    drainContacts                    = work->drainContacts;
    work->drainBody.context.contacts = drainContacts;
    work->drainBody.pos.vx           = ACTOR_00300_DRAIN_OFFSET_X;
    work->drainBody.pos.vy           = ACTOR_00300_DRAIN_OFFSET_Y;
    work->drainBody.pos.vz           = 0;
    work->drainBody.key              = damagePackAttackKey(_gActor00300Attacks, ACTOR_00300_ATTACK_DRAIN);
    work->drainBody.radius           = ACTOR_00300_DRAIN_RADIUS;
    work->drainBody.flags            = WORLD_COLLISION_BODY_SPHERE;
    worldCollisionLinkBody(WORLD_COLLISION_LIST_ENEMY_ATTACKS, &work->drainBody);
    worldCollisionInitContacts(drainContacts, ARRAY_SIZE(work->drainContacts), 0);
    gStageCountdownMusicEntry = ACTOR_00300_SCENE_MUSIC_ENTRY;
    work->drainBody.flags     = work->drainBody.flags & (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
    task->msgTable            = _gActor00300Messages;
    task->state               = ACTOR_00300_TASK_ACTIVE;
}
#undef ACTOR_00300_BIND_DRAIN_TEXTURE

/// Refreshes pursuit memory when the player-to-enemy segment has no room occluder.
///
/// start and end borrow composed GfxCoord caches in the same view frame;
/// scratch borrows writable `_Actor00300HitScratch` from/to vectors and work
/// borrows live `_Actor00300Work`. All pointers must be side-effect-free and
/// disjoint: scratch is evaluated eight times and each coordinate three times.
/// Endpoint coordinates narrow to s16; their delta must fit s16 with a nonzero
/// squared length no greater than 0x7FFFFFFF for the occlusion normalization.
/// A clear segment reloads `ACTOR_00300_ALERT_TICKS` and sets playerSeen to 1;
/// an occluder clears only the timer. The caller clears playerSeen beforehand.
/// Captures no caller locals; the occlusion query borrows both vectors for the call.
#define ACTOR_00300_TEST_SIGHT_LINE(work, scratch, start, end)                        \
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

/// Chooses pitch and yaw hit recoil from the two upper bytes of one LCG draw.
///
/// enemyWork is a side-effect-free live `_Actor00300Work*`, read repeatedly.
/// yawResult is a repeatedly read/written s32 lvalue, disjoint from enemyWork
/// and the LCG state. Use as a standalone statement. Changes the work's
/// hit-twist fields, yawResult and the shared LCG. Each angle uses 4096 units
/// per turn: even magnitudes 64..190 are negative and odd magnitudes 65..191
/// positive. The helper intentionally narrows each byte to s8 before masking.
#define ACTOR_00300_CHOOSE_HIT_TWIST(enemyWork, yawResult)                  \
    {                                                                       \
        s16 randomHalfword;                                                 \
        s32 pitchSignBit;                                                   \
        u32 randomState;                                                    \
        s32 pitchTwist;                                                     \
        s32 yawRandomByte;                                                  \
                                                                            \
        randomState     = RANDOM_LCG_NEXT(gRandomLcgState);                 \
        randomHalfword  = randomState >> 16;                                \
        pitchTwist      = _actor00300TiltMagnitude(randomHalfword);         \
        pitchSignBit    = randomHalfword & 1;                               \
        gRandomLcgState = randomState;                                      \
        if (!pitchSignBit) {                                                \
            pitchTwist = -pitchTwist;                                       \
        }                                                                   \
        (enemyWork)->hitTwist.vx = pitchTwist;                              \
        yawRandomByte            = randomHalfword >> 8;                     \
        (yawResult)              = _actor00300TiltMagnitude(yawRandomByte); \
        if (!(yawRandomByte & 1)) {                                         \
            (yawResult) = -(yawResult);                                     \
        }                                                                   \
        (enemyWork)->hitTwist.vy    = (yawResult);                          \
        (enemyWork)->hitTwistActive = 1;                                    \
    }

/// Resolves grid correction, attack damage, enemy overlaps, MP drain and player sight for one combat tick.
///
/// Requires initialized work and composed body coordinates from the previous
/// tick. Clears consumed contact tables. Grid correction
/// uses Q16 integer halves; enemy separation uses Q12 normals in the root frame.
/// Attack-key bit 7 selects player or companion; valid attack rows are required.
/// Hit direction selects front/back reactions, and a fatal burst enters death.
/// Borrows one `_Actor00300HitScratch` block, releasing it before return.
static void _actor00300ResolveContacts(Task* task)
{
    enum { ACTOR_00300_HURT_DAMAGE_THRESHOLD = 80,
           ACTOR_00300_REACTION_BURST        = 4,
           ACTOR_00300_CONTACT_FRACTION_BITS = 12,
           ACTOR_00300_DRAIN_HALO_ARG        = 0x20001, // one expansion tick, tint row 2
           ACTOR_00300_HURT_KNOCK_DOWN_STEP  = 2,
           ACTOR_00300_CRITICAL_MULTIPLIER   = 4,
           ACTOR_00300_KNOCK_DOWN_ANIM_COUNT = ACTOR_00300_ANIM_KNOCK_DOWN_FRONT - ACTOR_00300_ANIM_KNOCK_DOWN_BACK + 1 };

    _Actor00300Work*       work;
    _Actor00300HitScratch* stackTop;
    _Actor00300HitScratch* scratch;
    Enemy*                 enemy;
    GfxCoord*              rootCoord;
    GfxCoord*              contactCoord;
    s32                    maxEnemyOverlap;
    s32                    incendiarySeen;
    u32                    lastEffectKey;
    s32                    contactIndex;
    s32                    attackerDeltaZ;
    s32                    facingDotOrYawTwist;
    s32                    contactDeltaX;
    s32                    contactDeltaY;
    s32                    contactDeltaZ;
    s32                    penetration;
    s32                    positivePenetration;
    s16                    hitCooldown;

    maxEnemyOverlap = 0;
    incendiarySeen  = 0;
    lastEffectKey   = 0;
    work            = task->work;
    stackTop        = SCRATCH_STACK_CURSOR(_Actor00300HitScratch);
    rootCoord       = task->extra.tmd->coords;
    SCRATCH_STACK_RESERVE_BLOCK(_Actor00300HitScratch);
    scratch = SCRATCH_STACK_CURSOR(_Actor00300HitScratch);
    enemy   = task->spawnArg2.pointer;

    // Resolve the last movement against the grid before consuming pair contacts.
    switch (worldCollisionResolvePushback(work->gridContacts, &scratch->delta, ARRAY_SIZE(work->gridContacts), NULL)) {
        case WORLD_COLLISION_PUSHBACK_NO_GRID_HIT:
            break;
        case WORLD_COLLISION_PUSHBACK_GRID_HIT:
            rootCoord->coord.t[0] += stackTop[-1].delta.fixed.vx.halves.integer;
            rootCoord->coord.t[1] += scratch->delta.fixed.vy.halves.integer;
            rootCoord->coord.t[2] += scratch->delta.fixed.vz.halves.integer;
            break;
        case WORLD_COLLISION_PUSHBACK_OPPOSED:
            rootCoord->coord.t[0] = work->prevPos.vx;
            rootCoord->coord.t[1] = work->prevPos.vy;
            rootCoord->coord.t[2] = work->prevPos.vz;
            break;
    }
    worldCollisionClearContacts(work->gridContacts);

    if (work->hitBody.flags & WORLD_COLLISION_BODY_GRID_ENABLED) {
        switch (worldCollisionResolvePushback(work->hitContacts, &scratch->delta, ARRAY_SIZE(work->hitContacts), NULL)) {
            case WORLD_COLLISION_PUSHBACK_NO_GRID_HIT:
                break;
            case WORLD_COLLISION_PUSHBACK_GRID_HIT:
                rootCoord->coord.t[0] += scratch->delta.fixed.vx.halves.integer;
                rootCoord->coord.t[2] += scratch->delta.fixed.vz.halves.integer;
                break;
            case WORLD_COLLISION_PUSHBACK_OPPOSED:
                rootCoord->coord.t[0] = work->prevPos.vx;
                rootCoord->coord.t[2] = work->prevPos.vz;
                break;
        }
    }

    if (work->hitCooldown != 0) {
        if (--work->hitCooldown <= 0 && enemy->hp > 0) {
            work->hitCooldown = 0;
        }
    }

    // Incendiary damage reduction stays latched for the rest of this contact batch.
    for (contactIndex = 0; contactIndex < ARRAY_SIZE(work->hitContacts); contactIndex++) {
        switch ((u32)work->hitContacts[contactIndex].key.value >> 16) {
            case 0:
            case WORLD_COLLISION_CONTACT_PLAYER_BODY >> 16:
                break;
            case WORLD_COLLISION_CONTACT_ATTACK >> 16:
                if (work->hitCooldown != 0) {
                    break;
                }
                switch (damageGetPlayerAttackReaction(work->hitContacts[contactIndex].key.value) & 0xFFFF) {
                    case DAMAGE_PLAYER_REACTION_STAGGER:
                        if (work->knockDown == 0) {
                            work->knockDown = 1;
                        }
                        break;
                    case DAMAGE_PLAYER_REACTION_BUILDUP:
                        damageStartEnemyBuildup(enemy, work->hitContacts[contactIndex].key.value, 0);
                        break;
                    case DAMAGE_PLAYER_REACTION_POISON:
                        damageTryStartEnemyDamageOverTime(enemy, work->hitContacts[contactIndex].key.value, 0);
                        break;
                    case ACTOR_00300_REACTION_BURST:
                        work->burstStage = ACTOR_00300_BURST_REQUESTED;
                        break;
                    case DAMAGE_PLAYER_REACTION_EXPLOSION:
                        work->burstStage = ACTOR_00300_BURST_REQUESTED;
                        break;
                    case DAMAGE_PLAYER_REACTION_INCENDIARY:
                        incendiarySeen = 1;
                        break;
                    // These attributes add no actor-specific reaction.
                    case DAMAGE_PLAYER_REACTION_NONE:
                    case 5:
                    case 8:
                    case 9:
                        break;
                }
                contactCoord             = gPlayerActorTasks[(u8)work->hitContacts[contactIndex].key.value >> 7]->extra.tmd->coords;
                scratch->delta.vector.vx = contactCoord->coord.t[0] - rootCoord->coord.t[0];
                scratch->delta.vector.vy = contactCoord->coord.t[1] - rootCoord->coord.t[1];
                attackerDeltaZ           = contactCoord->coord.t[2] - rootCoord->coord.t[2];
                scratch->delta.vector.vz = attackerDeltaZ;
                facingDotOrYawTwist      = (scratch->delta.vector.vx * rootCoord->coord.m[0][2]) + (scratch->delta.vector.vy * rootCoord->coord.m[1][2]) + (attackerDeltaZ * rootCoord->coord.m[2][2]);
                work->hitFromFront       = facingDotOrYawTwist >= 0;
                work->hitDamage          = damageComputePlayerAttack(work->hitContacts[contactIndex].key.value,
                                                                     SquareRoot0((scratch->delta.vector.vx * scratch->delta.vector.vx) + (scratch->delta.vector.vy * scratch->delta.vector.vy) + (scratch->delta.vector.vz * scratch->delta.vector.vz)),
                                                                     0, 0);
                if (incendiarySeen != 0) {
                    work->hitDamage >>= 1;
                } else if (damageRollCriticalHit(enemy, work->hitContacts[contactIndex].key.value, 0) != 0) {
                    work->hitDamage *= ACTOR_00300_CRITICAL_MULTIPLIER;
                    effectSpawn(EFFECT_CRITICAL_HIT, &task->extra.tmd->coords[ACTOR_00300_HIT_PART], 0, NULL);
                }
                damageAccumulateLifeDrainHp(enemy, work->hitContacts[contactIndex].key.value, work->hitDamage, 0);
                worldTargetAddReadoutAmount(&enemy->node, work->hitDamage, 0);
                enemy->hp -= work->hitDamage;
                if (enemy->hp <= 0) {
                    if (work->burstStage == ACTOR_00300_BURST_NONE) {
                        work->action = ACTOR_00300_ACTION_HURT;
                        if ((u16)(work->anim - ACTOR_00300_ANIM_KNOCK_DOWN_BACK) < ACTOR_00300_KNOCK_DOWN_ANIM_COUNT) {
                            work->actionStep = ACTOR_00300_HURT_KNOCK_DOWN_STEP;
                        } else {
                            work->actionStep = ACTOR_00300_ACTION_BEGIN;
                        }
                    } else {
                        work->action     = ACTOR_00300_ACTION_DEAD;
                        work->actionStep = ACTOR_00300_ACTION_BEGIN;
                        task->state      = ACTOR_00300_TASK_DEATH;
                    }
                } else {
                    work->burstStage = ACTOR_00300_BURST_NONE;
                    if ((work->hitDamage >= ACTOR_00300_HURT_DAMAGE_THRESHOLD || work->knockDown != 0) && work->action != ACTOR_00300_ACTION_HURT) {
                        work->action     = ACTOR_00300_ACTION_HURT;
                        work->actionStep = ACTOR_00300_ACTION_BEGIN;
                    } else {
                        ACTOR_00300_CHOOSE_HIT_TWIST(work, facingDotOrYawTwist);
                    }
                }
                if (lastEffectKey != work->hitContacts[contactIndex].key.value) {
                    lastEffectKey = work->hitContacts[contactIndex].key.value;
                    effectSpawnHit(damageGetPlayerAttackEffectId(lastEffectKey), &task->extra.tmd->coords[ACTOR_00300_HIT_PART], NULL, &work->hitEffectArg);
                }
                hitCooldown = damageGetPlayerAttackHitCooldown(work->hitContacts[contactIndex].key.value);
                if (hitCooldown > 0) {
                    work->hitCooldown = hitCooldown;
                }
                break;
            case WORLD_COLLISION_CONTACT_ENEMY_BODY >> 16:
                contactCoord             = &task->extra.tmd->coords[ACTOR_00300_HIT_PART];
                contactDeltaX            = contactCoord->workm.t[0] - work->hitContacts[contactIndex].point.vx;
                scratch->delta.vector.vx = contactDeltaX;
                contactDeltaY            = contactCoord->workm.t[1] - work->hitContacts[contactIndex].point.vy;
                scratch->delta.vector.vy = contactDeltaY;
                contactDeltaZ            = contactCoord->workm.t[2] - work->hitContacts[contactIndex].point.vz;
                scratch->delta.vector.vz = contactDeltaZ;
                penetration              = work->hitContacts[contactIndex].distance - SquareRoot0((contactDeltaX * contactDeltaX) + (contactDeltaY * contactDeltaY) + (contactDeltaZ * contactDeltaZ));
                positivePenetration      = penetration;
                if (penetration <= 0) {
                    positivePenetration = 0;
                }
                penetration = positivePenetration;
                if (maxEnemyOverlap < penetration) {
                    maxEnemyOverlap = penetration;
                    VectorNormal(&scratch->delta.vector, &scratch->normal);
                    ApplyTransposeMatrixLV(&gWorldCollisionActiveGrid->viewCoord->workm, &scratch->normal, &scratch->push);
                }
                break;
        }
    }

    // Apply only the deepest enemy overlap, with its normal in the root frame.
    if (maxEnemyOverlap > 0) {
        rootCoord->coord.t[0] += (maxEnemyOverlap * scratch->push.vx) >> ACTOR_00300_CONTACT_FRACTION_BITS;
        rootCoord->coord.t[2] += (maxEnemyOverlap * scratch->push.vz) >> ACTOR_00300_CONTACT_FRACTION_BITS;
    }
    worldCollisionClearContacts(work->hitContacts);
    if (work->drainContacts[0].flags & WORLD_COLLISION_CONTACT_OCCUPIED) {
        work->drainBody.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        worldCollisionClearContacts(work->drainContacts);
        playerStateSpendMp(ACTOR_00300_MP_DRAINED);
        work->mp        += ACTOR_00300_MP_DRAINED;
        scratch->from.vx = ACTOR_00300_DRAIN_OFFSET_X;
        scratch->from.vy = ACTOR_00300_DRAIN_OFFSET_Y;
        scratch->from.vz = 0;
        effectSpawn(gRoomEffectHaloId, work->drainModelTask->extra.tmd->coords, ACTOR_00300_DRAIN_HALO_ARG, &scratch->from);
    }
    // Test the sight contact against room occluders before refreshing pursuit memory.
    work->playerSeen = 0;
    if ((work->sightContacts[0].key.value & WORLD_COLLISION_CONTACT_KIND_MASK) == WORLD_COLLISION_CONTACT_PLAYER_BODY) {
        contactCoord = &gameGetTaskSlot(GAME_TASK_SLOT_PLAYER)->extra.tmd->coords[ACTOR_00300_HIT_PART];
        ACTOR_00300_TEST_SIGHT_LINE(work, scratch, contactCoord, rootCoord);
    } else if (work->alertTimer > 0) {
        work->alertTimer--;
    }
    worldCollisionClearContacts(work->sightContacts);
    SCRATCH_STACK_RELEASE_BLOCK(_Actor00300HitScratch);
}
#undef ACTOR_00300_CHOOSE_HIT_TWIST
#undef ACTOR_00300_TEST_SIGHT_LINE

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
                randomValue     = RANDOM_LCG_NEXT(gRandomLcgState);
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
                randomValue      = RANDOM_LCG_NEXT(gRandomLcgState);
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
            randomValue       = RANDOM_LCG_NEXT(gRandomLcgState);
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
           ACTOR_00300_ATTACK_TURN_MIN_TICKS     = 60 };

    _Actor00300Work*  work;
    GfxCoord*         coord;
    ActorFaceScratch* scratch;
    s32               randomValue;

    scratch = SCRATCH_STACK_RESERVE_BLOCK(ActorFaceScratch);
    work    = task->work;
    coord   = task->extra.tmd->coords;
    if (((Enemy*)task->spawnArg2.pointer)->hp * 100 / (s32)_gActor00300EnemyParams.hpMax < ACTOR_00300_HEAL_HP_THRESHOLD_PERCENT &&
        work->mp >= ACTOR_00300_MP_HEAL_COST) {
        work->mp        -= ACTOR_00300_MP_HEAL_COST;
        work->anim       = ACTOR_00300_ANIM_CHARGE;
        work->action     = ACTOR_00300_ACTION_HEAL;
        work->actionStep = ACTOR_00300_ACTION_BEGIN;
    } else if (work->mp >= ACTOR_00300_MP_FIREBALL_COST) {
        work->mp            -= ACTOR_00300_MP_FIREBALL_COST;
        work->fireballAttack = _gActor00300FireballAttackDraws[((RANDOM_LCG_ADVANCE()) >> 16) & (ARRAY_SIZE(_gActor00300FireballAttackDraws) - 1)];
        if (work->fireballAttack == ACTOR_00300_FIREBALL_NO_ATTACK) {
            work->action     = ACTOR_00300_ACTION_PURSUE;
            work->actionStep = ACTOR_00300_ACTION_BEGIN;
            work->timer      = ((RANDOM_LCG_ADVANCE()) >> 16 & 31);
        } else {
            work->action     = ACTOR_00300_ACTION_FIREBALL;
            work->actionStep = ACTOR_00300_ACTION_BEGIN;
            work->timer      = ((RANDOM_LCG_ADVANCE()) >> 16 & 31) + ACTOR_00300_ATTACK_TURN_MIN_TICKS;
        }
    } else {
        scratch->delta.vx = gPlayerStatus.coordMtx->t[0] - coord->coord.t[0];
        scratch->delta.vy = 0;
        scratch->delta.vz = gPlayerStatus.coordMtx->t[2] - coord->coord.t[2];
        if (SquareRoot0(scratch->delta.vx * scratch->delta.vx + scratch->delta.vz * scratch->delta.vz) < ACTOR_00300_CLOSE_RANGE) {
            work->action     = ACTOR_00300_ACTION_DRAIN;
            work->actionStep = ACTOR_00300_ACTION_BEGIN;
            randomValue      = RANDOM_LCG_NEXT(gRandomLcgState);
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
                    abortDelayRandom = RANDOM_LCG_NEXT(gRandomLcgState);
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
                chargeSpawnRandom = RANDOM_LCG_NEXT(gRandomLcgState);
                gRandomLcgState   = chargeSpawnRandom;
                if (!((chargeSpawnRandom >> 0x10) & ACTOR_00300_MOTE_SPAWN_MASK)) {
                    chargeMoteRandom = RANDOM_LCG_NEXT(chargeSpawnRandom);
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
                                _gActor00300FireballChargeTicks[work->fireballAttack] - ACTOR_00300_FIREBALL_HALO_LEAD_TICKS, &scratch->rot);
                work->chargeEffect = effect;
                if (effect != NULL) {
                    taskReparent(task, effect->task);
                    work->chargeEffectTimer = _gActor00300FireballChargeTicks[work->fireballAttack] - ACTOR_00300_FIREBALL_HALO_LEAD_TICKS;
                }
                work->chargeSound =
                    ((((Enemy*)task->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | ACTOR_00300_SOUND_CHARGE;
                chargeLoopPan = (s8)worldCoordGetOriginAudioPan(coord);
                sndEvtRequestScriptStart(work->chargeSound, (s32)chargeLoopPan,
                                         (s32)(s8)worldCoordGetOriginAudioDepth(coord));
            }
            if (work->animFrame >= _gActor00300FireballChargeTicks[work->fireballAttack]) {
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
                releaseSpawnRandom = RANDOM_LCG_NEXT(gRandomLcgState);
                gRandomLcgState    = releaseSpawnRandom;
                if (!((releaseSpawnRandom >> 0x10) & ACTOR_00300_MOTE_SPAWN_MASK)) {
                    releaseMoteRandom = RANDOM_LCG_NEXT(releaseSpawnRandom);
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
                enemySpawnFromTable(gActor00300Tasks, ACTOR_00300_FIREBALL_DESCRIPTOR_INDEX, 0, task->spawnArg2.pointer);
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
                recoveryDelayRandom = RANDOM_LCG_NEXT(gRandomLcgState);
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
                    randomValue      = RANDOM_LCG_NEXT(gRandomLcgState);
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
                randomValue            = RANDOM_LCG_NEXT(gRandomLcgState);
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
/// word supplies half-extent/palette, upward speed and lifetime. The offset is
/// in coord's local X/Z plane, at radius 640 on one of 32 bearings. The Q12
/// trigonometric products are shifted unsigned then narrowed to s16, preserving
/// negative offsets; the cleared Y and fourth halfword remain zero. Offset storage
/// lasts through spawn only and must not be followed after the caller returns.
#define ACTOR_00300_SPAWN_CHARGE_MOTE(coord, packedArg, spawnRandom, angleRandom, moteAngle, radialOffset, moteOffset) \
    do {                                                                                                               \
        (spawnRandom)   = RANDOM_LCG_NEXT(gRandomLcgState);                                                            \
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
                recoveryDelayRandom = RANDOM_LCG_NEXT(gRandomLcgState);
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
                flinchDelayRandom   = RANDOM_LCG_NEXT(gRandomLcgState);
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
                getUpDelayRandom    = RANDOM_LCG_NEXT(gRandomLcgState);
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
                pursuitDelayRandom = RANDOM_LCG_NEXT(gRandomLcgState);
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
    /// angle is a signed-halfword angle in 4096-unit turns. The three temporaries
    /// are distinct s32 lvalues, disjoint from angle and the s32 latch. All
    /// arguments are side-effect-free and evaluated repeatedly. Steps by 32
    /// toward zero, snapping within that distance. Zero leaves temporaries
    /// untouched except angleValue; a snap leaves nextAngle untouched. Only
    /// raises the latch, so the caller clears it once before easing both axes.
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

/// Binds a successfully spawned fragment to the current placement's texture offsets.
///
/// Scoped to `_actor00300SpawnBurstFragments`: captures task, locationKey and
/// currentView, all live locals. fragment must be a side-effect-free EffectWork*
/// expression; it is read repeatedly. A NULL fragment performs no lookup.
/// Captures no other caller locals. The synchronized current location selects
/// the live placement at index 0..15 from the enemy's placement key. Updates
/// both texture offsets and, if allocated, both primitive-buffer halves in order.
/// The loaded placement and model resources must outlive those rebuilds.
#define ACTOR_00300_BIND_BURST_TEXTURE(fragment)                                                                            \
    do {                                                                                                                    \
        if ((fragment) != NULL) {                                                                                           \
            u32              placementKey, placementIndex;                                                                  \
            TmdObject*       fragmentModel;                                                                                 \
            AreaPlacement*   placement;                                                                                     \
            GameLocationKey* sessionLocation;                                                                               \
            sessionLocation       = &gGameSession->location;                                                                \
            placementKey          = ((Enemy*)task->spawnArg2.pointer)->placeKey;                                            \
            fragmentModel         = (fragment)->task->extra.tmd;                                                            \
            locationKey.stage     = sessionLocation->stage;                                                                 \
            locationKey.area      = sessionLocation->area;                                                                  \
            locationKey.roomState = sessionLocation->roomState;                                                             \
            currentView           = gGameSession->location.view;                                                            \
            placementIndex        = placementKey >> ENEMY_PLACE_INDEX_SHIFT;                                                \
            locationKey.view      = currentView;                                                                            \
            areaSyncLocationVariant(&locationKey);                                                                          \
            placement                        = AREA_PLACEMENT_AT(areaGetVariant(&locationKey)->placements, placementIndex); \
            fragmentModel->texturePageOffset = placement->texturePageOffset;                                                \
            fragmentModel->clutRowOffset     = placement->clutRowOffset;                                                    \
            if (fragmentModel->buffer != NULL) {                                                                            \
                tmdBuildBufferHalf(fragmentModel);                                                                          \
                tmdBuildBufferHalf(fragmentModel);                                                                          \
            }                                                                                                               \
        }                                                                                                                   \
    } while (0)

/// Spawns the five detached models of a bursting enemy with its placement texture offsets.
///
/// Requires the enemy task, model coordinate 1 and a live current-area placement
/// selected by the enemy's 0..15 placement index. Each successful effect owns
/// its new model; both primitive-buffer halves are rebuilt with the borrowed
/// area's texture offsets. Failed effect spawns are skipped independently.
/// The overlay's fragment sources must remain loaded until the effects finish.
static void _actor00300SpawnBurstFragments(Task* task)
{
    enum { ACTOR_00300_BURST_ANCHOR_PART = 1,
           ACTOR_00300_BURST_EFFECT_ARG  = 512 }; // puff-size addend passed to the flying-part task

    GameLocationKey locationKey;
    u8              currentView;
    EffectWork*     fragmentEffect;

    // The descriptor model must be selected immediately before each spawn.
    gTaskBank4Descs[TASK_BANK4_BURST_MODEL_PART_SLOT].data.model = &_gStingerBrainBurstHead;
    fragmentEffect                                               = effectSpawn(EFFECT_BURST_BODY_PART_BANK4, &task->extra.tmd->coords[ACTOR_00300_BURST_ANCHOR_PART], ACTOR_00300_BURST_EFFECT_ARG, NULL);
    ACTOR_00300_BIND_BURST_TEXTURE(fragmentEffect);

    gTaskBank4Descs[TASK_BANK4_BURST_MODEL_PART_SLOT].data.model = &_gStingerBrainBurstPart1;
    fragmentEffect                                               = effectSpawn(EFFECT_BURST_BODY_PART_BANK4, &task->extra.tmd->coords[ACTOR_00300_BURST_ANCHOR_PART], ACTOR_00300_BURST_EFFECT_ARG, NULL);
    ACTOR_00300_BIND_BURST_TEXTURE(fragmentEffect);

    gTaskBank4Descs[TASK_BANK4_BURST_MODEL_PART_SLOT].data.model = &_gStingerBrainBurstPart2;
    fragmentEffect                                               = effectSpawn(EFFECT_BURST_BODY_PART_BANK4, &task->extra.tmd->coords[ACTOR_00300_BURST_ANCHOR_PART], ACTOR_00300_BURST_EFFECT_ARG, NULL);
    ACTOR_00300_BIND_BURST_TEXTURE(fragmentEffect);

    gTaskBank4Descs[TASK_BANK4_BURST_MODEL_PART_SLOT].data.model = &_gStingerBrainBurstPart3;
    fragmentEffect                                               = effectSpawn(EFFECT_BURST_BODY_PART_BANK4, &task->extra.tmd->coords[ACTOR_00300_BURST_ANCHOR_PART], ACTOR_00300_BURST_EFFECT_ARG, NULL);
    ACTOR_00300_BIND_BURST_TEXTURE(fragmentEffect);

    gTaskBank4Descs[TASK_BANK4_BURST_MODEL_PART_SLOT].data.model = &_gStingerBrainBurstPart4;
    fragmentEffect                                               = effectSpawn(EFFECT_BURST_BODY_PART_BANK4, &task->extra.tmd->coords[ACTOR_00300_BURST_ANCHOR_PART], ACTOR_00300_BURST_EFFECT_ARG, NULL);
    ACTOR_00300_BIND_BURST_TEXTURE(fragmentEffect);
}
#undef ACTOR_00300_BIND_BURST_TEXTURE

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

/// Samples a composed coordinate for the dying enemy's lighting and colour.
///
/// Scoped to `_actor00300TickDeath`: captures live task and writable VECTOR3
/// worldPosition locals. coord is a side-effect-free composed GfxCoord* read
/// three times; its cached XYZ is copied unchanged to the lighting query. The
/// task must carry a live Enemy in spawnArg2.pointer. The last two arguments
/// are unused by the colour query, which borrows XYZ only for the call.
/// Invoke as a standalone compound statement inside a braced context.
#define ACTOR_00300_UPDATE_DEATH_COLOR(coord)                                      \
    {                                                                              \
        worldPosition.vx = (coord)->workm.t[0];                                    \
        worldPosition.vy = (coord)->workm.t[1];                                    \
        worldPosition.vz = (coord)->workm.t[2];                                    \
        worldCoordUpdateActorColor(task->spawnArg2.pointer, &worldPosition, 0, 0); \
    }

/// Retires the enemy from combat and advances its collapse or fragment-burst death.
///
/// Requires initialized enemy work and a live drain child. Paused control only
/// updates colour; hidden control suppresses both models. The running path
/// releases the battle hold and collision links once, then schedules destruction
/// after sixty ticks. Collapse rebuilds local Y from the saved matrix with a Q12 scale;
/// burst frees body packets before spawning independently owned fragment tasks.
static void _actor00300TickDeath(Enemy* enemy, Task* task)
{
    enum { ACTOR_00300_DEATH_BEGIN           = 0,
           ACTOR_00300_DEATH_COLLAPSE        = 1,
           ACTOR_00300_DEATH_DESTROY         = 2,
           ACTOR_00300_DEATH_BURST           = 3,
           ACTOR_00300_DEATH_SCALE_THRESHOLD = ONE / 8,
           ACTOR_00300_DEATH_SCALE_STEP      = 80,
           ACTOR_00300_DEATH_FADE_TICK       = 10,
           ACTOR_00300_DEATH_BURN_TICK       = 15,
           ACTOR_00300_DEATH_BURN_CYCLES     = 3,
           ACTOR_00300_DEATH_LINGER_TICKS    = 60,
           ACTOR_00300_SOUND_DEATH           = 0x40030008 };

    _Actor00300Work* work;
    TmdObject*       model;
    GfxCoord*        rootCoord;
    GfxCoord*        sampledCoord;
    VECTOR3          worldPosition;
    s32              actorControl;
    s32              soundId;
    s32              audioPan;
    s16              deathTicks;

    model        = task->extra.tmd;
    work         = task->work;
    actorControl = gSceneCombatState.actorControl;
    rootCoord    = model->coords;
    switch (actorControl) {
        case SCENE_COMBAT_ACTORS_PAUSED:
            ACTOR_00300_UPDATE_DEATH_COLOR(rootCoord);
            return;
        case SCENE_COMBAT_ACTORS_HIDDEN:
            model->flags                           = TMD_OBJECT_SKIP_ACTIVE_DRAW;
            work->drainModelTask->extra.tmd->flags = TMD_OBJECT_SKIP_ACTIVE_DRAW;
            return;
        case SCENE_COMBAT_ACTORS_RUNNING:
        default:
            break;
    }
    switch (work->actionStep) {
        case ACTOR_00300_DEATH_BEGIN:
            work->deathScaleY  = ONE;
            work->savedRootMtx = rootCoord->coord;
            // Retire target and collision links before the timed corpse phases.
            enemy->recs = NULL;
            worldTargetUnlinkNode(&enemy->node);
            worldCollisionUnlinkBody(&work->sightBody);
            worldCollisionUnlinkBody(&work->gridBody);
            worldCollisionUnlinkBody(&work->hitBody);
            worldCollisionUnlinkBody(&work->drainBody);
            worldCoordSetActorColorMode(enemy, ENEMY_COLOR_WEIGHTED);
            sceneReleaseBattleRefWithRewards(task, 3);
            work->timer      = 0;
            work->actionStep = ACTOR_00300_DEATH_COLLAPSE;
            if (work->burstStage != ACTOR_00300_BURST_NONE) {
                model->flags     = TMD_OBJECT_SKIP_ACTIVE_DRAW;
                work->actionStep = ACTOR_00300_DEATH_BURST;
            }
            sampledCoord = task->extra.tmd->coords;
            ACTOR_00300_UPDATE_DEATH_COLOR(sampledCoord);
            _actor00300StopCharge(work, work->chargeEffect);
            soundId  = ((((Enemy*)task->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | ACTOR_00300_SOUND_DEATH;
            audioPan = (s8)worldCoordGetOriginAudioPan(rootCoord);
            sndEvtRequestScriptStart(soundId, audioPan, (s8)worldCoordGetOriginAudioDepth(rootCoord));
            return;
        case ACTOR_00300_DEATH_COLLAPSE:
            if (work->deathScaleY >= ACTOR_00300_DEATH_SCALE_THRESHOLD + 1)
                work->deathScaleY -= ACTOR_00300_DEATH_SCALE_STEP;
            _actor00300SetRootScaleY(task, &work->savedRootMtx, work->deathScaleY);
            deathTicks  = work->timer + 1;
            work->timer = deathTicks;
            if (deathTicks == ACTOR_00300_DEATH_FADE_TICK)
                model->flags |= TMD_OBJECT_SEMI_TRANS;
            if (work->timer == ACTOR_00300_DEATH_BURN_TICK)
                effectSpawn(EFFECT_CORPSE_BURN, &task->extra.tmd->coords[ACTOR_00300_HIT_PART], ACTOR_00300_DEATH_BURN_CYCLES, NULL);
            if (work->timer >= ACTOR_00300_DEATH_LINGER_TICKS)
                work->actionStep = ACTOR_00300_DEATH_DESTROY;
            sampledCoord = task->extra.tmd->coords;
            ACTOR_00300_UPDATE_DEATH_COLOR(sampledCoord);
            return;
        case ACTOR_00300_DEATH_DESTROY:
            enemyDestroy(enemy, task);
            return;
        case ACTOR_00300_DEATH_BURST:
            if (work->burstStage != ACTOR_00300_BURST_NONE) {
                if (work->burstStage >= ACTOR_00300_BURST_READY) {
                    work->burstStage = ACTOR_00300_BURST_NONE;
                    // Give up the body packets before spawning independent fragments.
                    tmdFreePrimitiveBuffer(model);
                    model->flags |= TMD_OBJECT_SKIP_AUTO_BUFFER;
                    _actor00300SpawnBurstFragments(task);
                } else
                    work->burstStage++;
            }
            deathTicks  = work->timer + 1;
            work->timer = deathTicks;
            if (deathTicks >= ACTOR_00300_DEATH_LINGER_TICKS)
                work->actionStep = ACTOR_00300_DEATH_DESTROY;
            return;
    }
}
#undef ACTOR_00300_UPDATE_DEATH_COLOR

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
static inline void _actor00300UpdateDrainModel(Task* task)
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
    work->body.key              = damagePackAttackKey(_gActor00300Attacks, parentWork->fireballAttack);
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

/// Advances the homing fireball and starts burst teardown on impact or expiry.
///
/// Requires a live coordinate-body task and initialized collision work. Moves
/// 400 parent-coordinate units per running tick along the Q12 forward basis,
/// then turns toward the player and decrements the signed flight countdown.
/// A blocking sweep surface or occupied attack contact also ends flight.
/// Paused actors retain the glow; hidden actors leave flight untouched.
/// The burst borrows the coordinate through the following teardown states.
/// The enemy argument is retained for the state-table callback ABI.
static void _actor00300FlyFireball(Enemy* enemy, Task* task)
{
    enum {
        ACTOR_00300_FIREBALL_SPEED              = 400,
        ACTOR_00300_FIREBALL_STEP_FRACTION_BITS = 8,
        ACTOR_00300_FIREBALL_GLOW_SIZE          = 512,
        ACTOR_00300_FIREBALL_TASK_TEARDOWN      = 2,
    };
    _Actor00300FireballWork* work;
    GfxCoord*                coord;
    s32                      surfaceKey;
    s32                      blockedBySurface;
    s16                      remainingTicks;

    coord            = task->extra.coordBody->coord;
    work             = task->work;
    blockedBySurface = 0;
    switch (gSceneCombatState.actorControl) {
        case SCENE_COMBAT_ACTORS_PAUSED:
            _fireballDrawGlow(coord, ACTOR_00300_FIREBALL_GLOW_SIZE);
            return;
        case SCENE_COMBAT_ACTORS_RUNNING:
        default:
            // Move before steering so this tick follows the previous heading.
            coord->composeStamp = GRAPHICS_COORD_DIRTY;
            coord->coord.t[0]  += (coord->coord.m[0][2] * (ACTOR_00300_FIREBALL_SPEED >> 4)) >> ACTOR_00300_FIREBALL_STEP_FRACTION_BITS;
            coord->coord.t[2]  += (coord->coord.m[2][2] * (ACTOR_00300_FIREBALL_SPEED >> 4)) >> ACTOR_00300_FIREBALL_STEP_FRACTION_BITS;
            actorRenderComposeCoord(coord);
            _fireballDrawGlow(coord, ACTOR_00300_FIREBALL_GLOW_SIZE);
            // Consume the sweep contact before the next collision pass.
            surfaceKey = work->sweepContacts[0].key.value;
            if (surfaceKey != 0 && gWorldCollisionSurfacePropertiesTables[gGameSession->location.stage - 1]
                                                                         [gGameSession->location.area - 1][worldCollisionSurfaceClassFromKey(surfaceKey)]
                                                                             ->probePassThrough == WORLD_COLLISION_SURFACE_BLOCK_PROBES) {
                blockedBySurface = 1;
            }
            worldCollisionClearContacts(work->sweepContacts);
            _actor00300TurnFireballTowardPlayer(task);
            remainingTicks = work->timer - 1;
            work->timer    = remainingTicks;
            if (remainingTicks <= 0 || (work->contacts[0].flags & WORLD_COLLISION_CONTACT_OCCUPIED) || blockedBySurface != 0) {
                effectSpawn(gRoomEffectOrangeBurstId, coord, 0, NULL);
                task->state        = ACTOR_00300_FIREBALL_TASK_TEARDOWN;
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

#include "../../shared/effects/fireball_ember.inc.c"

/// Dispatches the enemy's initialization, active update and death state.
///
/// The descriptor supplies a nineteen-part TMD body and `spawnArg2.pointer`
/// supplies its live enemy. `Task::state` must stay within 0..2: initialize,
/// active or death. The task owns its allocated work and the drain child.
static void _actor00300EnemyTask(Task* task)
{
    EnemyTaskFuncTable3 handlers;

    handlers = _gActor00300EnemyTaskStates;
    handlers.funcs[task->state](task->spawnArg2.pointer, task);
}

/// Applies scene actor control and runs either event presentation or enemy combat.
///
/// Requires initialized enemy work and a live drain attachment. Running control
/// restores model drawing and the placement's lock-on policy; paused control
/// updates lighting and shadow only; hidden control suppresses both models and
/// lock-on. An active event selects the event presentation path.
static void _actor00300TickActive(Enemy* enemy, Task* task)
{
    _Actor00300Work* work;

    work = task->work;
    switch (gSceneCombatState.actorControl) {
        case SCENE_COMBAT_ACTORS_RUNNING:
            task->extra.tmd->flags                 = 0;
            work->drainModelTask->extra.tmd->flags = 0;
            enemy->node.state.parts.flags          = work->notLockable != 0;
            break;
        case SCENE_COMBAT_ACTORS_PAUSED:
            _actor00300UpdateLighting(task);
            _actor00300DrawShadow(task);
            return;
        case SCENE_COMBAT_ACTORS_HIDDEN:
            task->extra.tmd->flags                 = TMD_OBJECT_SKIP_ACTIVE_DRAW;
            work->drainModelTask->extra.tmd->flags = TMD_OBJECT_SKIP_ACTIVE_DRAW;
            enemy->node.state.parts.flags          = WORLD_TARGET_NOT_LOCKABLE;
            return;
    }
    if (gGameSession->eventState != 0) {
        _actor00300TickEvent(enemy, task);
        return;
    }
    _actor00300TickCombat(enemy, task);
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

/// Advances the routed enemy's contacts, actions, movement, animation and presentation in order.
///
/// Requires initialized work and a live model. A NULL patrol route suppresses
/// the entire update. Status pulses precede contact damage; movement follows
/// action and turn selection. The new root is composed before lighting and
/// shadow sampling, and coordinate 3 receives any active hit twist.
static void _actor00300TickCombat(Enemy* enemy, Task* task)
{
    GfxCoord*        rootCoord;
    _Actor00300Work* work;

    work      = task->work;
    rootCoord = task->extra.tmd->coords;
    if (work->patrolPoints != NULL) {
        if (enemy->reactionFlags != 0) {
            _actor00300TickStatusReactions(task);
        }
        // Consume last tick's contacts, then advance the action and animated pose.
        _actor00300ResolveContacts(task);
        _actor00300TickAction(task);
        if (work->turnRate != 0) {
            _actor00300TurnTowardTargetYaw(task);
        }
        _actor00300Move(task);
        _actor00300UpdateAnimation(task);
        if (work->hitTwistActive != 0) {
            _actor00300ApplyHitTwist(task);
        }
        _actor00300PlayAnimationCueSounds(task);
        // Compose the new root before sampling lighting and drawing its shadow.
        rootCoord->composeStamp = GRAPHICS_COORD_DIRTY;
        actorRenderComposeCoord(rootCoord);
        _actor00300UpdateLighting(task);
        _actor00300DrawShadow(task);
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

#include "../../shared/actor/player_detection_segment.inc.c"

/// Drain-model task states: 0 attachment setup, 1 parent-driven scaling, 2 destruction.
///
/// Indexed by `Task::state` in 0..2; each callback receives the live enemy and task.
/// The parent enemy's work and lighting matrices outlive the attachment.
static const EnemyTaskFuncTable3 _gActor00300DrainModelTaskStates = {
    {
        _actor00300InitDrainModel,
        _actor00300DrainModelActive,
        enemyDestroy,
    },
};

/// Fireball lifecycle handlers: initialization, flight and delayed teardown.
///
/// Indexed by `Task::state` in 0..2 and copied as one value before dispatch.
/// Each callback receives the live enemy and coordinate-body task.
static const EnemyTaskFuncTable3 _gActor00300FireballTaskStates = {
    {
        _actor00300InitFireball,
        _actor00300FlyFireball,
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
                randomValue      = RANDOM_LCG_NEXT(gRandomLcgState);
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
            blendFrames = _gActor00300AnimationBlendFrames[work->anim];
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

#include "../../shared/actor/actor_update_model_color.inc.c"

/// Updates the enemy lighting and colour from the composed root world position.
///
/// Requires a live enemy/model with composed coordinates and writable light and
/// colour matrices. The shared lighting routine borrows the three-component
/// position only for this call; it also updates matrices used by the drain child.
static void _actor00300UpdateLighting(Task* task)
{
    _actorUpdateModelColor(task);
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

/// Rebuilds the enemy root from a saved transform with a signed Q12 local-Y scale.
///
/// Requires a live TMD task, a readable full saved matrix and one free
/// `ActorScaleScratch` block. `ONE` means full height; X/Z and saved translation
/// remain unchanged. Repeated calls do not compound the scale. Invalidates root
/// composition, changes GTE state and releases its borrowed scratch block.
static void _actor00300SetRootScaleY(Task* task, const MATRIX* unscaledMatrix, s16 scaleY)
{
    GfxCoord*          rootCoord;
    ActorScaleScratch* stackTop;
    ActorScaleScratch* scratch;

    stackTop                                = SCRATCH_STACK_CURSOR(ActorScaleScratch);
    scratch                                 = stackTop - 1;
    SCRATCH_STACK_CURSOR(ActorScaleScratch) = scratch;
    rootCoord                               = task->extra.tmd->coords;
    scratch->scale.vx                       = ONE;
    scratch->scale.vy                       = scaleY;
    scratch->scale.vz                       = ONE;
    _actor00300ApplyDrainScale(rootCoord, unscaledMatrix, scratch);
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

    handlers = _gActor00300DrainModelTaskStates;
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

/// Dispatches the fireball's initialization, flight and delayed teardown.
///
/// The descriptor supplies a coordinate body and `spawnArg2.pointer` supplies
/// its live enemy. `Task::state` must stay within 0..2. Initialization requires
/// the launching enemy as parent and detaches before flight. Teardown retains work
/// and coordinate storage until the fireball's linger countdown ends.
static void _actor00300FireballTask(Task* task)
{
    EnemyTaskFuncTable3 handlers;

    handlers = _gActor00300FireballTaskStates;
    handlers.funcs[task->state](task->spawnArg2.pointer, task);
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

#include "../../shared/actor/actor_messages_place_rot_matrix.inc.c"

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

PACKAGE_ALIASES
