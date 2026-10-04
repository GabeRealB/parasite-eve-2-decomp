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
#include "gameplay/effect_tasks.h"
#include "gameplay/effects.h"
#include "gameplay/enemy.h"
#include "gameplay/geometry.h"
#include "gameplay/hud_sprites.h"
#include "gameplay/light.h"
#include "gameplay/loading.h"
#include "gameplay/message.h"
#include "gameplay/object_fields.h"
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

/// Main-executable counter whose lowest bit the flicker alternates on.

typedef struct Actor00300InitWork {
    /* 0x00 */ WorldCollisionBody    obj0;
    /* 0x20 */ WorldCollisionContact rec20;
    /* 0x38 */ WorldCollisionBody    obj38;
    /* 0x58 */ WorldCollisionCapsule pose;
    /* 0x70 */ WorldCollisionContact rec70;
    /* 0x88 */ s16                   timer;
    /* 0x8A */ s16                   pad8A;
} Actor00300InitWork;
STATIC_ASSERT_SIZEOF(Actor00300InitWork, 0x8C);

typedef struct Actor00300AreaConfig {
    /* 0x0 */ s16 id;
    /* 0x2 */ s16 area;
    /* 0x4 */ s16 room;
    /* 0x6 */ u16 value;
} Actor00300AreaConfig;
STATIC_ASSERT_SIZEOF(Actor00300AreaConfig, 8);

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

extern DamageAttack Actor00300_D15FD8[4];

extern s16 Actor00300_D16394[];
extern s16 Actor00300_D16000[];
extern s16 Actor00300_D15FF8[];

static void Actor00300_Fn048D4(Enemy* arg0, Task* arg1);
static void Actor00300_Fn04958(Enemy* arg0, Task* arg1);

static void Actor00300_Fn00970(Enemy* enemy, Task* task);
static void Actor00300_Fn04528(Task* arg0);
static void Actor00300_Fn00E54(Task* arg0);
static void Actor00300_Fn01678(Task* arg0);
static void Actor00300_Fn019C0(Task* arg0);
static void Actor00300_Fn01D60(Task* arg0);
static void Actor00300_Fn01F9C(Task* arg0);
static void Actor00300_Fn02620(Task* arg0);
static void Actor00300_Fn028D0(Task* arg0);
static void Actor00300_Fn02CE8(Task* arg0);
static void Actor00300_Fn030B8(Task* arg0);
static void Actor00300_Fn032BC(Task* arg0);
static void Actor00300_Fn0340C(Task* arg0);
static void Actor00300_Fn03A1C(Task* arg0);
static void Actor00300_Fn03B70(Enemy* arg0, Task* arg1);
static void Actor00300_Fn047CC(Enemy* arg0, Task* arg1);
static void Actor00300_Fn04A2C(Task* arg0);
static void Actor00300_Fn04C20(Task* arg0);
static void Actor00300_Fn04D28(Task* arg0);
static void Actor00300_Fn04E30(Task* arg0);
static void Actor00300_Fn04ED4(Task* arg0);
static void Actor00300_Fn04FB0(Task* arg0);
static void Actor00300_Fn05008(Task* arg0);
static void Actor00300_Fn0505C(Task* arg0, MATRIX* arg1, s16 arg2);
static void Actor00300_Fn05194(Enemy* arg0, Task* arg1);
static void Actor00300_Fn05278(Enemy* arg0, Task* arg1);

extern EnemyParams          Actor00300_D15FE8;
extern Actor00300AreaConfig Actor00300_D16020[15];
extern SVECTOR*             Actor00300_D16278[15][2];
extern TaskDesc             Actor00300_D162F0[];
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
s32                 Actor00300_Fn05304(Task*, s32, AnimationPlayRequest*, s32);
s32                 Actor00300_Fn053EC(Task*, s32, s32, s32);
s32                 Actor00300_Fn05434(Task* task, s32 msgId, ActorCommand* args, s32 arg3);
void                Actor00300_Fn04770(Task*);
void                Actor00300_Fn05138(Task*);
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

Actor00300AreaConfig Actor00300_D16020[15] = {
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
    { { { TASK_BODY_TMD, 96 } }, Actor00300_Fn05138, { .model = &_gActor00300Actor100300Model09FA0 } },
    { { { TASK_BODY_COORD, 96 } }, Actor00300_Fn0521C, { .value = 0 } },
};

TaskMessageEntry Actor00300_D16314[5] = {
    { ACTOR_MESSAGE_PLAY_ANIMATION, Actor00300_Fn05304 },
    { ACTOR_MESSAGE_PLACE, actorMsgPlaceRotMatrix },
    { ACTOR_MESSAGE_SET_MODEL_DRAW, Actor00300_Fn053EC },
    { ACTOR_COMMAND_MESSAGE_APPLY, Actor00300_Fn05434 },
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

/// Scratchpad block the hit and push tick works in: the push-out delta, its
/// normal, the two points of the sight test (the first also serves as the
/// effect offset), and the normal rotated into the grid's frame.
typedef struct _Actor00300HitScratch {
    WorldCollisionDelta delta;
    VECTOR              normal;
    SVECTOR             from;
    SVECTOR             to;
    VECTOR              push;
} _Actor00300HitScratch;

static TmdSource _gActor00300BrainStingerBurstHead;

static TmdSource _gActor00300Actor100300Model0ABC4;

static TmdSource _gActor00300Actor100300Model0B128;

static TmdSource _gActor00300Actor100300Model0B8AC;

static TmdSource _gActor00300Actor100300Model0BFC0;

extern void* D_80067704[1];

static inline s16      _actor00300TiltMagnitude(s8 value);
static void            Actor00300_Fn03618(Task* arg0);
static __inline__ void Actor00300_UpdateTransform(Enemy* arg0, Task* arg1);
static void            Actor00300_Fn03F40(Enemy* arg0, Task* arg1);
static void            Actor00300_Fn040A4(Enemy* arg0, Task* arg1);
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
    for (areaIndex = 0; Actor00300_D16020[areaIndex].id != 0; areaIndex++) {
        if ((gGameSession->location.loc.stage == Actor00300_D16020[areaIndex].area) &&
            (gGameSession->location.loc.area == Actor00300_D16020[areaIndex].room)) {
            work->patrolPoints =
                Actor00300_D16278[Actor00300_D16020[areaIndex].id]
                                 [enemy->place->mode];
            work->patrolPointCount = Actor00300_D16020[areaIndex].value;
        }
    }
    work->notLockable   = enemy->place->variant;
    obj->flags          = 0;
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
    obj->lightMtx       = &work->lightMtx;
    obj->colorMtx       = &work->colorMtx;
    enemy->field_4      = &coord->coord;
    enemy->field_48     = 0;
    Gp_LinkNode(&enemy->node);
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
    (Gp_IncStateF0Ref)(0);
    work->savedRootMtx = coord->coord;
    work->timer        = 0xA;
    work->mp           = ACTOR_00300_MP_AT_SPAWN;
    child              = Gp_SpawnEnemyFromTable(Actor00300_D162F0, 1, 0, enemy);
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
        tmdProcessStream(model);
        tmdProcessStream(model);
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
    Gp_LinkObj(3, &work->sightBody);
    Gp_InitRec18Table(sightContacts, ARRAY_SIZE(work->sightContacts), 0);
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
    Gp_LinkObj(2, &work->hitBody);
    Gp_InitRec18Table(hitContacts, ARRAY_SIZE(work->hitContacts), 0);
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
    Gp_LinkObj(2, &work->gridBody);
    Gp_InitRec18Table(gridContacts, ARRAY_SIZE(work->gridContacts), 0);
    work->gridBody.flags             = (u16)(work->gridBody.flags | (WORLD_COLLISION_BODY_FLOOR_QUERY | WORLD_COLLISION_BODY_GRID_ENABLED));
    work->drainBody.coord            = child->task->extra.tmd->coords;
    drainContacts                    = work->drainContacts;
    work->drainBody.context.contacts = drainContacts;
    work->drainBody.pos.vx           = -0x1F4;
    work->drainBody.pos.vy           = 0x1F4;
    work->drainBody.pos.vz           = 0;
    work->drainBody.key              = Gp_PackPair(Actor00300_D15FD8, 0);
    work->drainBody.radius           = 0x2BC;
    work->drainBody.flags            = (u32)WORLD_COLLISION_BODY_SPHERE;
    Gp_LinkObj(3, &work->drainBody);
    Gp_InitRec18Table(drainContacts, ARRAY_SIZE(work->drainContacts), 0);
    gStageSceneMusicEntry = 0xA;
    work->drainBody.flags = (u16)(work->drainBody.flags & (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED));
    task->msgTable        = Actor00300_D16314;
    task->state           = 1;
}

/// Copies the world positions of two coordinates into the scratch block and
/// runs `detectSegmentHitsWall` on the segment between them. When it reports no
/// hit, `alertTimer` is rearmed to `ACTOR_00300_ALERT_TICKS` and `playerSeen`
/// raised; otherwise `alertTimer` is cleared.
#define _ACTOR00300_TEST_SIGHT_LINE(work, scratch, start, end)              \
    do {                                                                    \
        (scratch)->from.vx = (start)->workm.t[0];                           \
        (scratch)->from.vy = (start)->workm.t[1];                           \
        (scratch)->from.vz = (start)->workm.t[2];                           \
        (scratch)->to.vx   = (end)->workm.t[0];                             \
        (scratch)->to.vy   = (end)->workm.t[1];                             \
        (scratch)->to.vz   = (end)->workm.t[2];                             \
        if (detectSegmentHitsWall(&(scratch)->from, &(scratch)->to) != 0) { \
            (work)->alertTimer = 0;                                         \
        } else {                                                            \
            (work)->alertTimer = ACTOR_00300_ALERT_TICKS;                   \
            (work)->playerSeen = 1;                                         \
        }                                                                   \
    } while (0)

/// Maps a random byte's low seven bits to a tilt magnitude of 0x40..0xBF.
static inline s16 _actor00300TiltMagnitude(s8 value)
{
    return (value & 0x7F) + 0x40;
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

    switch (func_800E0C10(work->gridContacts, &scratch->delta, ARRAY_SIZE(work->gridContacts), NULL)) {
        case 0:
            break;
        case 1:
            self->coord.t[0] += head[-1].delta.fixed.vx.halves.integer;
            self->coord.t[1] += scratch->delta.fixed.vy.halves.integer;
            self->coord.t[2] += scratch->delta.fixed.vz.halves.integer;
            break;
        case 2:
            self->coord.t[0] = work->prevPos.vx;
            self->coord.t[1] = work->prevPos.vy;
            self->coord.t[2] = work->prevPos.vz;
            break;
    }
    Gp_ClearRec18Occupied(work->gridContacts);

    if (work->hitBody.flags & WORLD_COLLISION_BODY_GRID_ENABLED) {
        switch (func_800E0C10(work->hitContacts, &scratch->delta, ARRAY_SIZE(work->hitContacts), NULL)) {
            case 0:
                break;
            case 1:
                self->coord.t[0] += scratch->delta.fixed.vx.halves.integer;
                self->coord.t[2] += scratch->delta.fixed.vz.halves.integer;
                break;
            case 2:
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
                switch (Gp_GetIdParam0(work->hitContacts[i].key.value) & 0xFFFF) {
                    case 1:
                        if (work->knockDown == 0) {
                            work->knockDown = 1;
                        }
                        break;
                    case 2:
                        Gp_SetObjFlag2(enemy, work->hitContacts[i].key.value, 0);
                        break;
                    case 3:
                        Gp_SetObjFlag4(enemy, work->hitContacts[i].key.value, 0);
                        break;
                    case 4:
                        work->burstStage = 1;
                        break;
                    case 6:
                        work->burstStage = 1;
                        break;
                    case 7:
                        critical = 1;
                        break;
                    case 0:
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
                work->hitDamage          = Gp_ComputeDamage(work->hitContacts[i].key.value,
                                                            SquareRoot0((scratch->delta.vector.vx * scratch->delta.vector.vx) + (scratch->delta.vector.vy * scratch->delta.vector.vy) + (scratch->delta.vector.vz * scratch->delta.vector.vz)),
                                                            0, 0);
                if (critical != 0) {
                    work->hitDamage >>= 1;
                } else if (Gp_RollEnemyChance(enemy, work->hitContacts[i].key.value, 0) != 0) {
                    work->hitDamage *= 4;
                    Gp_SpawnEff(EFFECT_CRITICAL_HIT, &arg0->extra.tmd->coords[3], 0, NULL);
                }
                func_800E2C78(enemy, work->hitContacts[i].key.value, work->hitDamage, 0);
                func_800DA6E8(&enemy->node, work->hitDamage, 0);
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
                    func_800FDB18(Gp_GetIdParam1(lastId) & 0xFFFF, &arg0->extra.tmd->coords[3], NULL, &work->hitEffectArg);
                }
                cooldown = Gp_GetIdParam2(work->hitContacts[i].key.value);
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
    Gp_ClearRec18Occupied(work->hitContacts);
    if (work->drainContacts[0].flags & WORLD_COLLISION_CONTACT_OCCUPIED) {
        work->drainBody.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        Gp_ClearRec18Occupied(work->drainContacts);
        Gp_SpendMp(ACTOR_00300_MP_DRAINED);
        work->mp        += ACTOR_00300_MP_DRAINED;
        scratch->from.vx = -0x1F4;
        scratch->from.vy = 0x1F4;
        scratch->from.vz = 0;
        Gp_SpawnEff(gRoomEffectHaloId, work->drainModelTask->extra.tmd->coords, 0x20001, &scratch->from);
    }
    work->playerSeen = 0;
    if ((work->sightContacts[0].key.value & 0xFFFF0000) == 0x10000) {
        other = &gameGetTaskSlot(GAME_TASK_SLOT_PLAYER)->extra.tmd->coords[3];
        _ACTOR00300_TEST_SIGHT_LINE(work, scratch, other, self);
    } else if (work->alertTimer > 0) {
        work->alertTimer--;
    }
    Gp_ClearRec18Occupied(work->sightContacts);
    SCRATCH_STACK_RELEASE_BYTES(0x40);
}

static void Actor00300_Fn01678(Task* arg0)
{
    _Actor00300Work* work;
    GfxCoord*        coord;
    s16              timer;
    s16              nextPoint;
    s32              state;
    s16              nextState;
    s32              dx;
    s32              dz;
    u16              angle0;
    u16              angle1;
    VECTOR*          vec;
    VECTOR*          scratchEnd;

    scratchEnd                 = SCRATCH_STACK_CURSOR(void);
    vec                        = scratchEnd - 1;
    SCRATCH_STACK_CURSOR(void) = vec;
    work                       = arg0->work;
    state                      = work->actionStep;
    coord                      = arg0->extra.tmd->coords;
    switch (state) {
        case 0:
            work->turnRate = 0;
            work->speed    = 0;
            work->anim     = ACTOR_00300_ANIM_STAND;
            timer          = (u16)work->timer - 1;
            work->timer    = timer;
            if ((timer << 0x10) <= 0) {
                scratchEnd[-1].vx = (s32)(work->patrolPoints[work->patrolPoint].vx - coord->coord.t[0]);
                vec->vy           = 0;
                vec->vz           = (s32)(work->patrolPoints[work->patrolPoint].vz - coord->coord.t[2]);
                angle0            = ratan2((s32)(s16)scratchEnd[-1].vx, (s32)(s16)vec->vz) & 0xFFF;
                nextState         = 1;
                work->targetYaw   = angle0;
                if (work->yaw == angle0) {
                    nextState = 2;
                }
                work->actionStep = nextState;
                work->timer      = 0;
            }
            break;
        case 1:
            work->turnRate    = 0x28;
            work->anim        = ACTOR_00300_ANIM_TURN;
            work->speed       = 0;
            scratchEnd[-1].vx = (s32)(work->patrolPoints[work->patrolPoint].vx - coord->coord.t[0]);
            vec->vy           = 0;
            vec->vz           = (s32)(work->patrolPoints[work->patrolPoint].vz - coord->coord.t[2]);
            angle1            = ratan2((s32)(s16)scratchEnd[-1].vx, (s32)(s16)vec->vz) & 0xFFF;
            work->targetYaw   = angle1;
            if (work->yaw == angle1) {
                work->actionStep = 2;
            }
            break;
        case 2:
            work->turnRate    = 0x28;
            work->speed       = 0x19;
            work->anim        = state;
            scratchEnd[-1].vx = (s32)(work->patrolPoints[work->patrolPoint].vx - coord->coord.t[0]);
            vec->vy           = 0;
            vec->vz           = (s32)(work->patrolPoints[work->patrolPoint].vz - coord->coord.t[2]);
            work->targetYaw   = ratan2((s32)(s16)scratchEnd[-1].vx, (s32)(s16)vec->vz) & 0xFFF;
            dx                = scratchEnd[-1].vx;
            dz                = vec->vz;
            if (SquareRoot0((dx * dx) + (dz * dz)) <= work->speed) {
                coord->coord.t[0] = (s32)work->patrolPoints[work->patrolPoint].vx;
                coord->coord.t[2] = (s32)work->patrolPoints[work->patrolPoint].vz;
                work->speed       = 0;
                nextPoint         = (u16)work->patrolPoint + 1;
                work->patrolPoint = nextPoint;
                if (nextPoint >= work->patrolPointCount) {
                    work->patrolPoint = 0;
                }
                work->actionStep = 1;
            }
            break;
    }
    if ((work->alertTimer != 0) || (gSceneCombatState.actor00300AttackAlert != 0) || (work->hitDamage != 0)) {
        work->action     = ACTOR_00300_ACTION_PURSUE;
        work->actionStep = 0;
        work->alertTimer = ACTOR_00300_ALERT_TICKS;
        Gp_ArmStateF0(1);
        gSceneCombatState.actor00300AttackAlert = 0;
    }
    SCRATCH_STACK_RELEASE_BYTES(0x10);
}

static void Actor00300_Fn019C0(Task* arg0)
{
    _Actor00300Work* work;
    GfxCoord*        coord;
    VECTOR*          scratchEnd;
    VECTOR*          vec;
    s16              delta;
    s16              angle;
    s16              timer;
    s16              wrapped;
    s32              magnitude;
    s32              moveMagnitude;
    s32              distance;
    s32              random;

    scratchEnd                   = SCRATCH_STACK_CURSOR(VECTOR);
    vec                          = scratchEnd - 1;
    SCRATCH_STACK_CURSOR(VECTOR) = vec;
    work                         = arg0->work;
    coord                        = arg0->extra.tmd->coords;
    switch (work->actionStep) {
        case 0:
            work->turnRate = 0;
            work->speed    = 0;
            work->anim     = ACTOR_00300_ANIM_STAND;
            timer          = (u16)work->timer - 1;
            work->timer    = timer;
            if (timer <= 0) {
                scratchEnd[-1].vx = gPlayerStatus.coordMtx->t[0] - coord->coord.t[0];
                vec->vy           = 0;
                vec->vz           = gPlayerStatus.coordMtx->t[2] - coord->coord.t[2];
                distance          = SquareRoot0(scratchEnd[-1].vx * scratchEnd[-1].vx + vec->vz * vec->vz);
                angle             = ratan2((s16)scratchEnd[-1].vx, (s16)vec->vz) & 0xFFF;
                work->targetYaw   = angle;
                delta             = (u16)angle - (u16)work->yaw;
                magnitude         = abs(delta);
                if (magnitude < 0x800) {
                    angle = magnitude;
                } else {
                    if (delta > 0) {
                        wrapped = 0x1000 - delta;
                    } else {
                        wrapped = delta + 0x1000;
                    }
                    angle = wrapped;
                }
                if (distance >= 0xBB8 || angle >= 0x100) {
                    work->actionStep = 1;
                }
                random          = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                gRandomLcgState = random;
                work->timer     = (((u32)random >> 16) & 31) + 30;
            }
            break;
        case 1:
            work->turnRate    = 0x3C;
            work->speed       = 0x19;
            work->anim        = ACTOR_00300_ANIM_MOVE;
            scratchEnd[-1].vx = gPlayerStatus.coordMtx->t[0] - coord->coord.t[0];
            vec->vy           = 0;
            vec->vz           = gPlayerStatus.coordMtx->t[2] - coord->coord.t[2];
            distance          = SquareRoot0(scratchEnd[-1].vx * scratchEnd[-1].vx + vec->vz * vec->vz);
            angle             = ratan2((s16)scratchEnd[-1].vx, (s16)vec->vz) & 0xFFF;
            work->targetYaw   = angle;
            delta             = (u16)angle - (u16)work->yaw;
            moveMagnitude     = abs(delta);
            if (moveMagnitude < 0x800) {
                angle = moveMagnitude;
            } else {
                if (delta > 0) {
                    wrapped = 0x1000 - delta;
                } else {
                    wrapped = delta + 0x1000;
                }
                angle = wrapped;
            }
            timer       = (u16)work->timer - 1;
            work->timer = timer;
            if (timer <= 0 || (distance < 0xBB8 && angle < 0x100)) {
                work->actionStep = 0;
                random           = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                gRandomLcgState  = random;
                work->timer      = ((u32)random >> 16) & 31;
            }
            break;
    }
    if (work->alertTimer == 0) {
        work->action                            = ACTOR_00300_ACTION_PATROL;
        work->actionStep                        = 0;
        work->hitDamage                         = 0;
        gSceneCombatState.actor00300AttackAlert = 0;
        work->timer                             = 10;
    } else {
        timer             = (u16)work->attackTimer - 1;
        work->attackTimer = timer;
        if (timer <= 0) {
            random            = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            gRandomLcgState   = random;
            work->attackTimer = (((u32)random >> 16) & 63) + 60;
            if (work->playerSeen == 1) {
                Actor00300_Fn01D60(arg0);
            }
        }
    }
    SCRATCH_STACK_RELEASE_BYTES(0x10);
}

static void Actor00300_Fn01D60(Task* arg0)
{
    _Actor00300Work*  work;
    GfxCoord*         coord;
    ActorFaceScratch* sc;
    s32               random;

    sc    = (ActorFaceScratch*)SCRATCH_STACK_RESERVE_BYTES(0x18);
    work  = arg0->work;
    coord = arg0->extra.tmd->coords;
    if (((Enemy*)arg0->spawnArg2.pointer)->hp * 100 / (s32)Actor00300_D15FE8.hpMax < 50 &&
        work->mp >= ACTOR_00300_MP_HEAL_COST) {
        work->mp        -= ACTOR_00300_MP_HEAL_COST;
        work->anim       = ACTOR_00300_ANIM_CHARGE;
        work->action     = ACTOR_00300_ACTION_HEAL;
        work->actionStep = 0;
    } else if (work->mp >= ACTOR_00300_MP_FIREBALL_COST) {
        work->mp            -= ACTOR_00300_MP_FIREBALL_COST;
        work->fireballAttack = Actor00300_D16000[((gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT) >> 16) & 15];
        if (work->fireballAttack == 0) {
            work->action     = ACTOR_00300_ACTION_PURSUE;
            work->actionStep = 0;
            work->timer      = ((gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT) >> 16 & 31);
        } else {
            work->action     = ACTOR_00300_ACTION_FIREBALL;
            work->actionStep = 0;
            work->timer      = ((gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT) >> 16 & 31) + 60;
        }
    } else {
        sc->delta.vx = gPlayerStatus.coordMtx->t[0] - coord->coord.t[0];
        sc->delta.vy = 0;
        sc->delta.vz = gPlayerStatus.coordMtx->t[2] - coord->coord.t[2];
        if (SquareRoot0(sc->delta.vx * sc->delta.vx + sc->delta.vz * sc->delta.vz) < 3000) {
            work->action     = ACTOR_00300_ACTION_DRAIN;
            work->actionStep = 0;
            random           = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            gRandomLcgState  = random;
            work->timer      = (((u32)random >> 16) & 31) + 60;
        } else {
            work->action     = ACTOR_00300_ACTION_RECHARGE;
            work->actionStep = 0;
        }
    }
    SCRATCH_STACK_RELEASE_BYTES(0x18);
}

static void Actor00300_Fn01F9C(Task* arg0)
{
    SVECTOR           sp10;
    SVECTOR           sp18;
    _Actor00300Work*  work;
    EffectWork*       effect;
    GfxCoord*         coord;
    s16               turnTimer;
    s16               effectTimer2;
    s16               effectTimer1;
    s16               state;
    s16               delta;
    s32               magnitude;
    s16               angle;
    s16               delay;
    s32               random2;
    s32               effectAngle2;
    s32               random1;
    s32               effectAngle1;
    s32               sound;
    s32               delayRandom0;
    s32               delayRandom3;
    s32               pan2;
    s32               pan1;
    s32               effectPan;
    u16               yaw;
    u32               effectRandom2;
    u32               effectRandom1;
    ActorFaceScratch* scratchEnd;
    ActorFaceScratch* scratch;

    scratchEnd = SCRATCH_STACK_CURSOR(ActorFaceScratch);
    scratch =
        (ActorFaceScratch*)(SCRATCH_STACK_CURSOR(u8) = (u8*)scratchEnd - 0x18);
    work  = arg0->work;
    state = work->actionStep;
    coord = arg0->extra.tmd->coords;
    switch (state) {
        case 0:
            work->turnRate = 0x3C;
            work->speed    = 0;
            work->anim     = ACTOR_00300_ANIM_TURN;
            scratchEnd[-1].delta.vx =
                (s32)(gPlayerStatus.coordMtx->t[0] - coord->coord.t[0]);
            scratch->delta.vy = 0;
            scratch->delta.vz = (s32)(gPlayerStatus.coordMtx->t[2] - coord->coord.t[2]);
            yaw               = ratan2((s32)(s16)scratchEnd[-1].delta.vx, (s32)(s16)scratch->delta.vz) &
                  0xFFF;
            work->targetYaw = yaw;
            delta           = yaw - (u16)work->yaw;
            magnitude       = abs(delta);
            angle           = magnitude >= 0x800 ? (delta > 0 ? 0x1000 - delta : delta + 0x1000)
                                                 : magnitude;
            if (angle < 0x100) {
                work->actionStep                        = 1;
                work->anim                              = ACTOR_00300_ANIM_CHARGE;
                gSceneCombatState.actor00300AttackAlert = 1;
            } else {
                turnTimer   = (u16)work->timer - 1;
                work->timer = turnTimer;
                if ((turnTimer << 0x10) <= 0) {
                    work->action     = ACTOR_00300_ACTION_PURSUE;
                    work->actionStep = 0;
                    delayRandom0     = (gRandomLcgState * RANDOM_LCG_MULTIPLIER) + RANDOM_LCG_INCREMENT;
                    gRandomLcgState  = delayRandom0;
                    delay            = ((u32)delayRandom0 >> 0x10) & 0xF;
                    work->timer      = delay;
                }
            }
            break;
        case 1:
            gSceneCombatState.actor00300AttackAlert = 0;
            work->turnRate                          = 0xF;
            scratchEnd[-1].delta.vx =
                (s32)(gPlayerStatus.coordMtx->t[0] - coord->coord.t[0]);
            scratch->delta.vy = 0;
            scratch->delta.vz = (s32)(gPlayerStatus.coordMtx->t[2] - coord->coord.t[2]);
            work->targetYaw =
                ratan2((s32)(s16)scratchEnd[-1].delta.vx, (s32)(s16)scratch->delta.vz) &
                0xFFF;
            if (gRoomEffectState->effectControl == ROOM_EFFECT_CONTROL_RUNNING) {
                effectRandom1   = (gRandomLcgState * RANDOM_LCG_MULTIPLIER) + RANDOM_LCG_INCREMENT;
                gRandomLcgState = effectRandom1;
                if (!((effectRandom1 >> 0x10) & 3)) {
                    random1         = (effectRandom1 * RANDOM_LCG_MULTIPLIER) + RANDOM_LCG_INCREMENT;
                    gRandomLcgState = random1;
                    effectAngle1    = ((u32)random1 >> 0x10) & 0xF80;
                    memset(&sp18, 0, 8);
                    sp18.vx = (s16)((u32)(rcos(effectAngle1) * 5) >> 5);
                    sp18.vz = (s16)((u32)(rsin(effectAngle1) * 5) >> 5);
                    sp10    = sp18;
                    Gp_SpawnEff(gRoomEffectMoteId, coord, 0x20101200, &sp10);
                }
            }
            if (work->animFrame == 0x33) {
                sound = ((((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x40030006;
                pan1  = (s8)worldCoordGetOriginAudioPan(coord);
                SndEvt_EnqueueType6(sound, (s32)pan1,
                                    (s32)(s8)worldCoordGetOriginAudioDepth(coord));
                scratch->rot.vy = -0x5DC;
                scratch->rot.vx = 0;
                scratch->rot.vz = 0x320;
                effect =
                    Gp_SpawnEff(gRoomEffectHaloId, coord,
                                Actor00300_D15FF8[work->fireballAttack] - 0x32, &scratch->rot);
                work->chargeEffect = effect;
                if (effect != NULL) {
                    taskReparent(arg0, effect->task);
                    work->chargeEffectTimer = Actor00300_D15FF8[work->fireballAttack] - 0x32;
                }
                work->chargeSound =
                    ((((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x40030009;
                effectPan = (s8)worldCoordGetOriginAudioPan(coord);
                SndEvt_EnqueueType6(work->chargeSound, (s32)effectPan,
                                    (s32)(s8)worldCoordGetOriginAudioDepth(coord));
            }
            if (work->animFrame >= Actor00300_D15FF8[work->fireballAttack]) {
                work->actionStep = 2;
                work->anim       = ACTOR_00300_ANIM_RELEASE;
                work->turnRate   = 0;
            }
            if (work->chargeEffectTimer > 0) {
                effectTimer1            = (u16)work->chargeEffectTimer - 1;
                work->chargeEffectTimer = effectTimer1;
                if ((effectTimer1 << 0x10) <= 0) {
                    work->chargeEffect = NULL;
                }
            }
            break;
        case 2:
            if (work->chargeEffectTimer > 0) {
                effectTimer2            = (u16)work->chargeEffectTimer - 1;
                work->chargeEffectTimer = effectTimer2;
                if ((effectTimer2 << 0x10) <= 0) {
                    work->chargeEffect = NULL;
                }
            }
            if ((work->animFrame < 0xE) && (gRoomEffectState->effectControl == ROOM_EFFECT_CONTROL_RUNNING)) {
                effectRandom2   = (gRandomLcgState * RANDOM_LCG_MULTIPLIER) + RANDOM_LCG_INCREMENT;
                gRandomLcgState = effectRandom2;
                if (!((effectRandom2 >> 0x10) & 3)) {
                    random2         = (effectRandom2 * RANDOM_LCG_MULTIPLIER) + RANDOM_LCG_INCREMENT;
                    gRandomLcgState = random2;
                    effectAngle2    = ((u32)random2 >> 0x10) & 0xF80;
                    memset(&sp18, 0, 8);
                    sp18.vx = (s16)((u32)(rcos(effectAngle2) * 5) >> 5);
                    sp18.vz = (s16)((u32)(rsin(effectAngle2) * 5) >> 5);
                    sp10    = sp18;
                    Gp_SpawnEff(gRoomEffectMoteId, coord, 0x20101200, &sp10);
                }
            }
            if (work->animFrame == 0xE) {
                Gp_SpawnEnemyFromTable(Actor00300_D162F0, 2, 0, arg0->spawnArg2.pointer);
                sound = ((((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x40030005;
                pan2  = (s8)worldCoordGetOriginAudioPan(coord);
                SndEvt_EnqueueType6(sound, (s32)pan2,
                                    (s32)(s8)worldCoordGetOriginAudioDepth(coord));
            }
            if (work->animFrame >= 0x13) {
                work->actionStep = 3;
                work->anim       = ACTOR_00300_ANIM_RECOVER;
            }
            break;
        case 3:
            if (work->animFrame >= 0x14) {
                work->action     = ACTOR_00300_ACTION_PURSUE;
                work->actionStep = 0;
                delayRandom3     = (gRandomLcgState * RANDOM_LCG_MULTIPLIER) + RANDOM_LCG_INCREMENT;
                gRandomLcgState  = delayRandom3;
                delay            = ((u32)delayRandom3 >> 0x10) & 0x1F;
                work->timer      = delay;
            }
            break;
    }
    SCRATCH_STACK_RELEASE_BYTES(0x18);
}

static void Actor00300_Fn02620(Task* arg0)
{
    _Actor00300Work* work;
    GfxCoord*        coord;
    VECTOR*          scratchEnd;
    VECTOR*          vec;
    s16              delta;
    s16              angle;
    s16              timer;
    s32              magnitude;
    s32              sound;
    s32              pan;
    s32              random;

    scratchEnd                   = SCRATCH_STACK_CURSOR(VECTOR);
    vec                          = scratchEnd - 1;
    SCRATCH_STACK_CURSOR(VECTOR) = vec;
    work                         = arg0->work;
    coord                        = arg0->extra.tmd->coords;
    switch (work->actionStep) {
        case 0:
            work->turnRate    = 0x3C;
            work->speed       = 0;
            work->anim        = ACTOR_00300_ANIM_TURN;
            scratchEnd[-1].vx = gPlayerStatus.coordMtx->t[0] - coord->coord.t[0];
            vec->vy           = 0;
            vec->vz           = gPlayerStatus.coordMtx->t[2] - coord->coord.t[2];
            angle             = ratan2((s16)scratchEnd[-1].vx, (s16)vec->vz) & 0xFFF;
            work->targetYaw   = angle;
            delta             = (u16)angle - (u16)work->yaw;
            magnitude         = abs(delta);
            angle             = magnitude >= 0x800 ? (delta > 0 ? 0x1000 - delta : delta + 0x1000) : magnitude;
            if (angle < 0x80) {
                work->actionStep = 1;
                work->turnRate   = 0;
                work->anim       = ACTOR_00300_ANIM_DRAIN;
            } else {
                timer       = (u16)work->timer - 1;
                work->timer = timer;
                if (timer <= 0) {
                    work->action     = ACTOR_00300_ACTION_PURSUE;
                    work->actionStep = 0;
                    random           = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                    gRandomLcgState  = random;
                    work->timer      = ((u32)random >> 16) & 15;
                }
            }
            break;
        case 1:
            if ((u32)((u16)work->animFrame - 0x10) < 0x14U) {
                work->drainScaleY += 0x100;
            } else if (work->animFrame >= 0x33) {
                work->drainScaleY -= 0x100;
            }
            if (work->animFrame == 0x20) {
                work->drainBody.flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;
                sound                  = ((((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x4003000A;
                pan                    = (s8)worldCoordGetOriginAudioPan(coord);
                SndEvt_EnqueueType6(sound, pan, (s8)worldCoordGetOriginAudioDepth(coord));
            }
            if (work->animFrame >= 0x37) {
                work->drainScaleY      = 0;
                work->action           = ACTOR_00300_ACTION_PURSUE;
                work->actionStep       = 0;
                random                 = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                gRandomLcgState        = random;
                work->timer            = ((u32)random >> 16) & 31;
                work->drainBody.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
            }
            break;
    }
    SCRATCH_STACK_RELEASE_BYTES(0x10);
}

static void Actor00300_Fn028D0(Task* arg0)
{
    SVECTOR          sp10;
    SVECTOR          sp18;
    SVECTOR          sp20;
    _Actor00300Work* work;
    EffectWork*      effect;
    EffectWork*      burst;
    Enemy*           enemy;
    TmdObject*       obj;
    Enemy*           currentEnemy;
    GfxCoord*        coord;
    s16              timer;
    s16              state;
    s32              random0;
    s32              angle0;
    s32              random1;
    s32              angle1;
    s32              sound;
    s32              random2;
    s32              pan0;
    s32              pan1;
    u32              effectRandom0;
    u32              effectRandom1;

    obj   = arg0->extra.tmd;
    work  = arg0->work;
    enemy = arg0->spawnArg2.pointer;
    state = work->actionStep;
    coord = obj->coords;
    switch (state) {
        case 0:
            if (gRoomEffectState->effectControl == ROOM_EFFECT_CONTROL_RUNNING) {
                effectRandom0   = (gRandomLcgState * RANDOM_LCG_MULTIPLIER) + RANDOM_LCG_INCREMENT;
                gRandomLcgState = effectRandom0;
                if (!((effectRandom0 >> 0x10) & 3)) {
                    random0         = (effectRandom0 * RANDOM_LCG_MULTIPLIER) + RANDOM_LCG_INCREMENT;
                    gRandomLcgState = random0;
                    angle0          = ((u32)random0 >> 0x10) & 0xF80;
                    memset(&sp20, 0, sizeof(sp20));
                    sp20.vx = (s16)((u32)(rcos(angle0) * 5) >> 5);
                    sp20.vz = (s16)((u32)(rsin(angle0) * 5) >> 5);
                    sp18    = sp20;
                    Gp_SpawnEff(gRoomEffectMoteId, coord, 0x20100200, &sp18);
                }
            }
            work->turnRate = 0;
            work->speed    = 0;
            if (work->animFrame >= 0x33) {
                work->actionStep   = 1;
                work->anim         = ACTOR_00300_ANIM_RELEASE;
                sp10.vy            = -0x5DC;
                sp10.vx            = 0;
                sp10.vz            = 0x320;
                effect             = Gp_SpawnEff(gRoomEffectHaloId, coord, 0x10014, &sp10);
                work->chargeEffect = effect;
                if (effect != NULL) {
                    taskReparent(arg0, effect->task);
                    work->chargeEffectTimer = 0x13;
                }
                work->chargeSound = ((((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x40030009;
                pan0              = (s8)worldCoordGetOriginAudioPan(coord);
                SndEvt_EnqueueType6(work->chargeSound, pan0, (s8)worldCoordGetOriginAudioDepth(coord));
                return;
            }
            return;
        case 1:
            if (work->chargeEffectTimer > 0) {
                timer                   = (u16)work->chargeEffectTimer - 1;
                work->chargeEffectTimer = timer;
                if (timer <= 0) {
                    work->chargeEffect = NULL;
                }
            }
            if (gRoomEffectState->effectControl == ROOM_EFFECT_CONTROL_RUNNING) {
                effectRandom1   = (gRandomLcgState * RANDOM_LCG_MULTIPLIER) + RANDOM_LCG_INCREMENT;
                gRandomLcgState = effectRandom1;
                if (!((effectRandom1 >> 0x10) & 3)) {
                    random1         = (effectRandom1 * RANDOM_LCG_MULTIPLIER) + RANDOM_LCG_INCREMENT;
                    gRandomLcgState = random1;
                    angle1          = ((u32)random1 >> 0x10) & 0xF80;
                    memset(&sp20, 0, sizeof(sp20));
                    sp20.vx = (s16)((u32)(rcos(angle1) * 5) >> 5);
                    sp20.vz = (s16)((u32)(rsin(angle1) * 5) >> 5);
                    sp18    = sp20;
                    Gp_SpawnEff(gRoomEffectMoteId, coord, 0x20100200, &sp18);
                }
            }
            if (work->animFrame >= 0x13) {
                work->actionStep   = 2;
                work->chargeEffect = NULL;
                work->anim         = ACTOR_00300_ANIM_RECOVER;
                currentEnemy       = arg0->spawnArg2.pointer;
                currentEnemy->hp   = (u16)currentEnemy->hp + 0x64;
                func_800DA6E8(&enemy->node, -0x64, 0);
                burst = Gp_SpawnEff(gRoomEffectSparkEmitterId, coord, 0, NULL);
                if (burst != NULL) {
                    taskReparent(arg0, burst->task);
                }
                sound = ((((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x4003000B;
                pan1  = (s8)worldCoordGetOriginAudioPan(coord);
                SndEvt_EnqueueType6(sound, pan1, (s8)worldCoordGetOriginAudioDepth(coord));
            }
            break;
        case 2:
            if (work->animFrame >= 0xF) {
                work->action     = ACTOR_00300_ACTION_PURSUE;
                work->actionStep = 0;
                random2          = (gRandomLcgState * RANDOM_LCG_MULTIPLIER) + RANDOM_LCG_INCREMENT;
                gRandomLcgState  = random2;
                work->timer      = ((u32)random2 >> 0x10) & 0x1F;
            }
            break;
    }
}

static void Actor00300_Fn02CE8(Task* arg0)
{
    _Actor00300Work* work;
    EffectWork*      effect;
    Enemy*           enemy;
    GfxCoord*        coord;
    s32              state;
    s16              animation;
    s16              deathEnd;
    s16              heavyEnd;
    s16              lightEnd;
    s32              sound;
    s32              random0;
    s32              random1;
    s32              soundBase;
    s32              pan0, pan1, pan2, pan3, pan4;

    work  = arg0->work;
    enemy = arg0->spawnArg2.pointer;
    state = work->actionStep;
    coord = arg0->extra.tmd->coords;
    switch (state) {
        case 0:
            effect                 = work->chargeEffect;
            work->speed            = 0;
            work->turnRate         = 0;
            work->drainBody.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
            if (effect != NULL) {
                effect->task->state     = 3;
                work->chargeEffect      = NULL;
                work->chargeEffectTimer = 0;
                SndEvt_EnqueueType7(work->chargeSound, 1);
            }
            if (enemy->hp <= 0) {
                if (work->hitFromFront == 0) {
                    work->anim = ACTOR_00300_ANIM_KNOCK_DOWN_BACK;
                    deathEnd   = 0x20;
                } else {
                    work->anim = ACTOR_00300_ANIM_KNOCK_DOWN_FRONT;
                    deathEnd   = 0x22;
                }
                work->timer           = deathEnd;
                work->deathStage      = 1;
                enemy->reactionFlags  = 0;
                work->hitBody.pos.vz  = 0x190;
                work->actionStep      = 2;
                work->hitBody.flags  |= WORLD_COLLISION_BODY_GRID_ENABLED;
                work->gridBody.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_GRID_ENABLED);
                return;
            }
            if ((work->hitDamage >= 0x78) || (work->knockDown != 0)) {
                if (work->hitFromFront == 0) {
                    work->anim = ACTOR_00300_ANIM_KNOCK_DOWN_BACK;
                    heavyEnd   = 0x20;
                } else {
                    work->anim = ACTOR_00300_ANIM_KNOCK_DOWN_FRONT;
                    heavyEnd   = 0x22;
                }
                work->timer      = heavyEnd;
                work->actionStep = 2;
            } else {
                if (work->hitFromFront == 0) {
                    work->anim = ACTOR_00300_ANIM_FLINCH_BACK;
                    lightEnd   = 0x35;
                } else {
                    work->anim = ACTOR_00300_ANIM_FLINCH_FRONT;
                    lightEnd   = 0x33;
                }
                work->timer         = lightEnd;
                work->actionStep    = 1;
                work->reactionBlend = 1;
            }
            soundBase = 0x40030007;
            sound     = ((((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | soundBase;
            pan0      = (s8)worldCoordGetOriginAudioPan(coord);
            SndEvt_EnqueueType6(sound, (s32)pan0, (s8)worldCoordGetOriginAudioDepth(coord));
            return;
        case 1:
            if (work->animFrame >= work->timer) {
                work->knockDown     = 0;
                work->action        = ACTOR_00300_ACTION_PURSUE;
                work->actionStep    = 1;
                work->alertTimer    = ACTOR_00300_ALERT_TICKS;
                work->reactionBlend = 0;
                random0             = (gRandomLcgState * RANDOM_LCG_MULTIPLIER) + RANDOM_LCG_INCREMENT;
                gRandomLcgState     = random0;
                work->timer         = ((u32)random0 >> 0x10) & 0x1F;
            }
            animation = work->anim;
            if (animation == ACTOR_00300_ANIM_FLINCH_BACK) {
                if (work->animFrame == 0xA) {
                    soundBase = 0x40030003;
                    sound     = ((((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | soundBase;
                    pan1      = (s8)worldCoordGetOriginAudioPan(coord);
                    SndEvt_EnqueueType6(sound, (s32)pan1, (s8)worldCoordGetOriginAudioDepth(coord));
                }
            } else if (animation == ACTOR_00300_ANIM_FLINCH_FRONT) {
                if (work->animFrame == 0xC) {
                    soundBase = 0x40030003;
                    sound     = ((((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | soundBase;
                    pan2      = (s8)worldCoordGetOriginAudioPan(coord);
                    SndEvt_EnqueueType6(sound, (s32)pan2, (s8)worldCoordGetOriginAudioDepth(coord));
                }
            }
            break;
        case 2:
            if (work->deathStage == 1) {
                work->deathStage = state;
            }
            if (work->animFrame >= work->timer) {
                if (enemy->hp <= 0) {
                    work->action     = ACTOR_00300_ACTION_DEAD;
                    work->actionStep = 0;
                    arg0->state      = (s32)state;
                } else {
                    work->actionStep = 3;
                    if (work->anim == ACTOR_00300_ANIM_KNOCK_DOWN_BACK) {
                        work->anim = ACTOR_00300_ANIM_GET_UP_BACK;
                    } else {
                        work->anim = ACTOR_00300_ANIM_GET_UP_FRONT;
                    }
                    work->reactionBlend = 1;
                }
            }
            if (work->anim == ACTOR_00300_ANIM_KNOCK_DOWN_BACK) {
                if (work->animFrame == 0xD) {
                    soundBase = 0x40030004;
                    sound     = ((((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | soundBase;
                    pan3      = (s8)worldCoordGetOriginAudioPan(coord);
                    SndEvt_EnqueueType6(sound, (s32)pan3, (s8)worldCoordGetOriginAudioDepth(coord));
                }
            } else if (work->anim == ACTOR_00300_ANIM_KNOCK_DOWN_FRONT) {
                if (work->animFrame == 0x12) {
                    soundBase = 0x40030004;
                    sound     = ((((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | soundBase;
                    pan4      = (s8)worldCoordGetOriginAudioPan(coord);
                    SndEvt_EnqueueType6(sound, (s32)pan4, (s8)worldCoordGetOriginAudioDepth(coord));
                }
            }
            break;
        case 3:
            if (work->animFrame >= 0x2B) {
                work->knockDown     = 0;
                work->action        = ACTOR_00300_ACTION_PURSUE;
                work->actionStep    = 1;
                work->alertTimer    = ACTOR_00300_ALERT_TICKS;
                work->reactionBlend = 0;
                random1             = (gRandomLcgState * RANDOM_LCG_MULTIPLIER) + RANDOM_LCG_INCREMENT;
                gRandomLcgState     = random1;
                work->timer         = ((u32)random1 >> 0x10) & 0x1F;
            }
            break;
    }
}

static void Actor00300_Fn030B8(Task* arg0)
{
    SVECTOR          sp10;
    SVECTOR          sp18;
    _Actor00300Work* work;
    GfxCoord*        coord;
    s16              timer;
    s16              state;
    s32              random;
    s32              angle;
    s32              sound;
    s32              pan;
    u32              effectRandom;
    u32              nextRandom;

    work  = arg0->work;
    state = work->actionStep;
    coord = arg0->extra.tmd->coords;
    switch (state) {
        case 0:
            work->actionStep = 1;
            work->turnRate   = 0;
            work->speed      = 0;
            work->timer      = 0;
            work->anim       = ACTOR_00300_ANIM_TURN;
            return;
        case 1:
            if (gRoomEffectState->effectControl == ROOM_EFFECT_CONTROL_RUNNING) {
                effectRandom    = (gRandomLcgState * RANDOM_LCG_MULTIPLIER) + RANDOM_LCG_INCREMENT;
                gRandomLcgState = effectRandom;
                if (!((effectRandom >> 0x10) & 3)) {
                    random          = (effectRandom * RANDOM_LCG_MULTIPLIER) + RANDOM_LCG_INCREMENT;
                    gRandomLcgState = random;
                    angle           = ((u32)random >> 0x10) & 0xF80;
                    memset(&sp18, 0, sizeof(sp18));
                    sp18.vx = (u32)(rcos(angle) * 5) >> 5;
                    sp18.vz = (u32)(rsin(angle) * 5) >> 5;
                    sp10    = sp18;
                    Gp_SpawnEff(gRoomEffectMoteId, coord, 0x20103200, &sp10);
                }
            }
            timer       = (u16)work->timer + 1;
            work->timer = timer;
            if (timer >= 0x5B) {
                work->action     = ACTOR_00300_ACTION_PURSUE;
                work->timer      = 0;
                work->actionStep = 0;
                nextRandom       = (gRandomLcgState * RANDOM_LCG_MULTIPLIER) + RANDOM_LCG_INCREMENT;
                work->mp         = work->mp + ACTOR_00300_MP_RECHARGED;
                work->timer      = (nextRandom >> 0x10) & 0x1F;
                gRandomLcgState  = nextRandom;
                sound            = ((((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x4003000B;
                pan              = (s8)worldCoordGetOriginAudioPan(coord);
                SndEvt_EnqueueType6(sound, pan, (s8)worldCoordGetOriginAudioDepth(coord));
            }
            return;
    }
}

static void Actor00300_Fn032BC(Task* arg0)
{
    _Actor00300Work*  work;
    GfxCoord*         coord;
    ActorFaceScratch* sc;
    s32               ang;
    u16               want;
    s16               diff;
    s32               adiff;
    s32               step;
    s32               cur;
    s32               next;
    s32               wrapStep;

    sc    = (ActorFaceScratch*)SCRATCH_STACK_RESERVE_BYTES(0x18);
    coord = arg0->extra.tmd->coords;
    work  = arg0->work;
    ang   = ratan2(coord->coord.m[0][2], coord->coord.m[2][2]) & 0xFFF;
    want  = work->targetYaw;
    diff  = want - ang;
    adiff = diff >= 0 ? diff : -diff;

    work->yaw = ang;
    if (adiff < 0x800) {
        step = work->turnRate;
        if (step >= adiff) {
            work->yaw = want;
        } else {
            next = work->yaw;
            if (diff <= 0) {
                next -= step;
            } else {
                next += step;
            }
            work->yaw = next;
        }
    } else {
        step = work->turnRate;
        if (diff > 0) {
            if (step >= 0x1000 - diff) {
                goto snap;
            } else {
                goto turn;
            }
        } else if (step >= 0x1000 + diff) {
            goto snap;
        } else {
            goto turn;
        }
    snap:
        work->yaw = work->targetYaw;
        goto done;
    turn:
        wrapStep = work->turnRate;
        cur      = work->yaw;
        if (diff > 0) {
            work->yaw = cur - wrapStep;
        } else {
            work->yaw = cur + wrapStep;
        }
    }
done:
    sc->rot.vx = 0;
    sc->rot.vy = work->yaw;
    sc->rot.vz = 0;
    RotMatrix(&sc->rot, &coord->coord);
    SCRATCH_STACK_RELEASE_BYTES(0x18);
}

/// Turns the model's fourth coordinate by the angles in `hitTwist`, then
/// eases the x and y angles back toward zero by 0x20 a call; once both have
/// settled, clears `hitTwistActive`.
static void Actor00300_Fn0340C(Task* arg0)
{
    _Actor00300Work* work;
    GfxCoord*        coord;
    MATRIX*          matrix;
    s32              angleX;
    s32              angleY;
    s32              absX;
    s32              nextX;
    s32              absY;
    s32              nextY;
    s32              active;

    matrix = SCRATCH_STACK_RESERVE_BLOCK(MATRIX);
    active = 0;
    work   = arg0->work;
    coord  = arg0->extra.tmd->coords;
    RotMatrix(&work->hitTwist, matrix);
    gte_MulMatrix0(&coord[3].coord, matrix, &coord[3].coord);
    angleX = work->hitTwist.vx;
    if (angleX != 0) {
        absX = __builtin_abs(angleX);
        if (absX < 0x21) {
            work->hitTwist.vx = 0;
        } else {
            nextX = angleX - 0x20;
            if (angleX <= 0) {
                nextX = angleX + 0x20;
            }
            work->hitTwist.vx = nextX;
            active            = 1;
        }
    }
    angleY = work->hitTwist.vy;
    if (angleY != 0) {
        absY = __builtin_abs(angleY);
        if (absY < 0x21) {
            work->hitTwist.vy = 0;
        } else {
            nextY = angleY - 0x20;
            if (angleY <= 0) {
                nextY = angleY + 0x20;
            }
            work->hitTwist.vy = nextY;
            active            = 1;
        }
    }
    if (active == 0) {
        work->hitTwistActive = 0;
    }
    SCRATCH_STACK_RELEASE_BLOCK(MATRIX);
}

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
    effect1       = Gp_SpawnEff(EFFECT_BURST_BODY_PART_BANK4, &arg0->extra.tmd->coords[1], 0x200, NULL);
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
            tmdProcessStream(model1);
            tmdProcessStream(model1);
        }
    }

    D_80067704[0] = &_gActor00300Actor100300Model0ABC4;
    effect2       = Gp_SpawnEff(EFFECT_BURST_BODY_PART_BANK4, &arg0->extra.tmd->coords[1], 0x200, NULL);
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
            tmdProcessStream(model2);
            tmdProcessStream(model2);
        }
    }

    D_80067704[0] = &_gActor00300Actor100300Model0B128;
    effect3       = Gp_SpawnEff(EFFECT_BURST_BODY_PART_BANK4, &arg0->extra.tmd->coords[1], 0x200, NULL);
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
            tmdProcessStream(model3);
            tmdProcessStream(model3);
        }
    }

    D_80067704[0] = &_gActor00300Actor100300Model0B8AC;
    effect4       = Gp_SpawnEff(EFFECT_BURST_BODY_PART_BANK4, &arg0->extra.tmd->coords[1], 0x200, NULL);
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
            tmdProcessStream(model4);
            tmdProcessStream(model4);
        }
    }

    D_80067704[0] = &_gActor00300Actor100300Model0BFC0;
    effect5       = Gp_SpawnEff(EFFECT_BURST_BODY_PART_BANK4, &arg0->extra.tmd->coords[1], 0x200, NULL);
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
            tmdProcessStream(model5);
            tmdProcessStream(model5);
        }
    }
}

static void Actor00300_Fn03A1C(Task* arg0)
{
    _Actor00300Work*       work;
    const AnimationRecord* rec;
    GfxCoord*              coord;
    s32                    sound;
    s32                    pan;
    s32                    pan2;

    work  = arg0->work;
    coord = arg0->extra.tmd->coords;
    rec   = Gp_AnimGetRec(&work->rig.anim, &work->rig.slots[1]);
    if (rec != NULL) {
        if (!(rec->flags & ANIMATION_RECORD_CUE_2) && (work->lastCueFlags & ANIMATION_RECORD_CUE_2)) {
            sound = ((((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x40030001;
            pan   = (s8)worldCoordGetOriginAudioPan(coord);
            SndEvt_EnqueueType6(sound, pan, (s8)worldCoordGetOriginAudioDepth(coord));
        }
        if (!(rec->flags & ANIMATION_RECORD_CUE_1) && (work->lastCueFlags & ANIMATION_RECORD_CUE_1)) {
            sound = ((((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x40030002;
            pan2  = (s8)worldCoordGetOriginAudioPan(coord);
            SndEvt_EnqueueType6(sound, pan2, (s8)worldCoordGetOriginAudioDepth(coord));
        }
        work->lastCueFlags = (u16)(rec->flags & ANIMATION_RECORD_CUE_MASK);
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
    if (mode == 1)
        goto case1;
    if (mode < 2)
        goto common;
    if (mode == 2)
        goto case2;
    goto common;
case1:
    vec.vx = coord->workm.t[0];
    vec.vy = coord->workm.t[1];
    vec.vz = coord->workm.t[2];
    Gp_UpdateActorColor(arg1->spawnArg2.pointer, &vec, 0, 0);
    return;
case2:
    obj->flags                             = TMD_OBJECT_SKIP_ACTIVE_DRAW;
    work->drainModelTask->extra.tmd->flags = TMD_OBJECT_SKIP_ACTIVE_DRAW;
    return;
common:
    switch (work->actionStep) {
        case 0:
            work->deathScaleY  = 0x1000;
            work->savedRootMtx = coord->coord;
            arg0->recs         = NULL;
            worldTargetUnlinkNode(&arg0->node);
            Gp_UnlinkObj(&work->sightBody);
            Gp_UnlinkObj(&work->gridBody);
            Gp_UnlinkObj(&work->hitBody);
            Gp_UnlinkObj(&work->drainBody);
            Gp_SetLightMode(arg0, ENEMY_COLOR_WEIGHTED);
            Gp_ReleaseStateF0Add(arg1, 3);
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
            Gp_UpdateActorColor(arg1->spawnArg2.pointer, &vec, 0, 0);
            if (work->chargeEffect != NULL) {
                work->chargeEffect->task->state = 3;
                work->chargeEffect              = NULL;
                work->chargeEffectTimer         = 0;
                SndEvt_EnqueueType7(work->chargeSound, 1);
            }
            sound = ((((Enemy*)arg1->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x40030008;
            pan   = (s8)worldCoordGetOriginAudioPan(coord);
            SndEvt_EnqueueType6(sound, pan, (s8)worldCoordGetOriginAudioDepth(coord));
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
                Gp_SpawnEff(EFFECT_CORPSE_BURN, &arg1->extra.tmd->coords[3], 3, NULL);
            if (work->timer >= 0x3C)
                work->actionStep = 2;
            c      = arg1->extra.tmd->coords;
            vec.vx = c->workm.t[0];
            vec.vy = c->workm.t[1];
            vec.vz = c->workm.t[2];
            Gp_UpdateActorColor(arg1->spawnArg2.pointer, &vec, 0, 0);
            return;
        case 2:
            enemyDestroy(arg0, arg1);
            return;
        case 3:
            if (work->burstStage != 0) {
                if (work->burstStage >= 2) {
                    work->burstStage = 0;
                    Tmd_FreeBuffers(obj);
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

static __inline__ void Actor00300_UpdateTransform(Enemy* arg0, Task* arg1)
{
    TmdObject*         obj;
    GfxCoord*          saved;
    _Actor00300Work*   work;
    s32                disabled;
    s16                flags;
    s16                scale;
    ActorScaleScratch* head;
    ActorScaleScratch* scratch;
    GfxCoord*          coord;

    saved    = arg1->extra.tmd->coords;
    obj      = arg1->extra.tmd;
    disabled = gSceneCombatState.actorControl;
    work     = arg1->parent->work;
    if (disabled == 0) {
        if (gGameSession->eventState != 0) {
            flags      = ((work->eventDrawFlags & 1) == 0) * TMD_OBJECT_SKIP_ACTIVE_DRAW;
            obj->flags = flags;
            if (work->eventDrawFlags & 2) {
                obj->flags = flags | TMD_OBJECT_SKIP_AUTO_BUFFER;
            }
        }
        scale = work->drainScaleY;
        if (scale <= 0) {
            obj->flags = TMD_OBJECT_SKIP_ACTIVE_DRAW;
            return;
        }
        head                                    = SCRATCH_STACK_CURSOR(ActorScaleScratch);
        scratch                                 = head - 1;
        coord                                   = arg1->extra.tmd->coords;
        SCRATCH_STACK_CURSOR(ActorScaleScratch) = scratch;
        scratch->scale.vx                       = ONE;
        scratch->scale.vy                       = scale;
        scratch->scale.vz                       = ONE;
        coord->coord                            = work->drainModelMtx;
        scratch->matrix.rotationWords.m00M01    = ONE;
        scratch->matrix.rotationWords.m02M10    = 0;
        scratch->matrix.rotationWords.m11M12    = ONE;
        scratch->matrix.rotationWords.m20M21    = 0;
        scratch->matrix.rotationWords.m22       = ONE;
        ScaleMatrix(&scratch->matrix.mat, &scratch->scale);
        MulMatrix(&coord->coord, &scratch->matrix.mat);
        coord->composeStamp = GRAPHICS_COORD_DIRTY;
        saved->composeStamp = GRAPHICS_COORD_DIRTY;
        SCRATCH_STACK_RELEASE_BLOCK(ActorScaleScratch);
    }
}

static void Actor00300_Fn03F40(Enemy* arg0, Task* arg1)
{
    Actor00300_UpdateTransform(arg0, arg1);
}

static void Actor00300_Fn040A4(Enemy* arg0, Task* arg1)
{
    _Actor00300Work*    parentWork;
    Task*               parent;
    Actor00300InitWork* work;
    ActorOffsetScratch* scratch;
    ActorOffsetScratch* head;
    SVECTOR*            offset;
    GfxCoord*           coord;
    GfxCoord*           parentCoord;
    GfxCoord*           objCoord;
    GfxCoord*           objCoord2;

    head                                     = SCRATCH_STACK_CURSOR(ActorOffsetScratch);
    scratch                                  = head - 1;
    SCRATCH_STACK_CURSOR(ActorOffsetScratch) = scratch;
    offset                                   = &scratch->offset;
    parent                                   = arg1->parent;
    coord                                    = arg1->extra.tmd->coords;
    parentCoord                              = parent->extra.tmd->coords;
    parentWork                               = parent->work;
    work                                     = memCalloc(0x8C, 0);
    if (work == NULL) {
        enemyDestroy(arg0, arg1);
        return;
    }
    arg1->work         = work;
    scratch->offset.vx = 0;
    scratch->offset.vy = -0x5DC;
    scratch->offset.vz = 0x320;
    gte_SetRotMatrix(&parentCoord->coord);
    gte_ldv0(offset);
    gte_rtv0();
    gte_stlvnl(&scratch->result);
    coord->parent               = &gGfxViewCoord;
    coord->coord                = parentCoord->coord;
    coord->coord.t[0]           = parentCoord->coord.t[0] + scratch->result.vx;
    coord->coord.t[1]           = parentCoord->coord.t[1] + scratch->result.vy;
    coord->coord.t[2]           = parentCoord->coord.t[2] + scratch->result.vz;
    coord->composeStamp         = GRAPHICS_COORD_DIRTY;
    objCoord                    = arg1->extra.tmd->coords;
    work->obj0.context.contacts = &work->rec20;
    work->obj0.pos.vx           = 0;
    work->obj0.pos.vy           = 0;
    work->obj0.pos.vz           = 0;
    work->obj0.coord            = objCoord;
    work->obj0.key              = Gp_PackPair(Actor00300_D15FD8, parentWork->fireballAttack);
    work->obj0.radius           = ACTOR_00300_ALERT_TICKS;
    work->obj0.flags            = WORLD_COLLISION_BODY_SPHERE;
    Gp_LinkObj(3, &work->obj0);
    Gp_InitRec18Table(&work->rec20, 1, 0);
    work->pose.ends[1].vz       = -0x1A4;
    work->pose.end0Radius       = 1;
    work->pose.end1Radius       = 1;
    work->pose.ends[0].vx       = 0;
    work->pose.ends[0].vy       = 0;
    work->pose.ends[0].vz       = 0;
    work->pose.ends[1].vx       = 0;
    work->pose.ends[1].vy       = 0;
    work->pose.contacts         = &work->rec70;
    work->obj0.flags           |= WORLD_COLLISION_BODY_PAIR_ENABLED;
    objCoord2                   = arg1->extra.tmd->coords;
    work->obj38.context.capsule = &work->pose;
    work->obj38.pos.vx          = 0;
    work->obj38.pos.vy          = 0;
    work->obj38.pos.vz          = 0;
    work->obj38.key             = 0;
    work->obj38.radius          = 0;
    work->obj38.flags           = WORLD_COLLISION_BODY_CAPSULE;
    work->obj38.coord           = objCoord2;
    Gp_LinkObj(3, &work->obj38);
    Gp_InitRec18Table(&work->rec70, 1, 0);
    work->timer        = 0x1E;
    work->obj38.flags |= (WORLD_COLLISION_BODY_CLIP_TO_GRID_CONTACT | WORLD_COLLISION_BODY_GRID_ENABLED);
    taskDetachFromParent(arg1);
    arg1->state = 1;
    SCRATCH_STACK_RELEASE_BLOCK(ActorOffsetScratch);
}

static void Actor00300_Fn04370(Enemy* arg0, Task* arg1)
{
    Actor00300InitWork* work;
    GfxCoord*           coord;
    s32                 id;
    s32                 expired;
    s16                 timer;

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
            Gp_UpdateCoord(coord);
            fireballDrawGlow(coord, 0x200);
            id = work->rec70.key.value;
            if (id != 0 && Gp_RoomParamTables[gGameSession->location.loc.stage - 1]
                                             [gGameSession->location.loc.area - 1][func_800E1B24(id)]
                                                 ->probePassThrough == WORLD_COLLISION_SURFACE_BLOCK_PROBES) {
                expired = 1;
            }
            Gp_ClearRec18Occupied(&work->rec70);
            Actor00300_Fn04528(arg1);
            timer       = work->timer - 1;
            work->timer = timer;
            if (timer <= 0 || (work->rec20.flags & WORLD_COLLISION_CONTACT_OCCUPIED) || expired != 0) {
                Gp_SpawnEff(gRoomEffectOrangeBurstId, coord, 0, NULL);
                arg1->state = 2;
                work->pad8A = 0;
            }
        case SCENE_COMBAT_ACTORS_HIDDEN:
            return;
    }
}

static void Actor00300_Fn04528(Task* arg0)
{
    s32               want;
    GfxCoord*         coord;
    ActorFaceScratch* sc;
    s16               cur;
    s32               ang;
    s32               current;
    s16               diff;
    s32               adiff;
    s16               turn;
    s16               wrap;

    coord        = arg0->extra.tmd->coords;
    sc           = (ActorFaceScratch*)SCRATCH_STACK_RESERVE_BYTES(0x18);
    sc->delta.vx = gPlayerStatus.coordMtx->t[0] - coord->coord.t[0];
    sc->delta.vy = 0;
    sc->delta.vz = gPlayerStatus.coordMtx->t[2] - coord->coord.t[2];
    want         = ratan2((s16)sc->delta.vx, (s16)sc->delta.vz) & 0xFFF;
    ang          = ratan2(coord->coord.m[0][2], coord->coord.m[2][2]) & 0xFFF;
    cur          = ang;
    diff         = want - ang;
    adiff        = diff >= 0 ? diff : -diff;
    turn         = diff;
    if (adiff < 0xD) {
        cur = want;
    } else {
        if (adiff >= 0x801) {
            wrap = diff - 0x1000;
            if (diff <= 0) {
                wrap = 0x1000 - diff;
            }
            turn = wrap;
        }
        current = cur;
        cur     = current + 0xC;
        if (turn <= 0) {
            cur = current - 0xC;
        }
    }
    sc->rot.vx = 0;
    sc->rot.vy = cur;
    sc->rot.vz = 0;
    RotMatrix(&sc->rot, &coord->coord);
    SCRATCH_STACK_RELEASE_BYTES(0x18);
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
            Actor00300_Fn04FB0(arg1);
            Actor00300_Fn05008(arg1);
            return;
        case SCENE_COMBAT_ACTORS_HIDDEN:
            arg1->extra.tmd->flags                 = TMD_OBJECT_SKIP_ACTIVE_DRAW;
            work->drainModelTask->extra.tmd->flags = TMD_OBJECT_SKIP_ACTIVE_DRAW;
            arg0->node.state.parts.flags           = WORLD_TARGET_NOT_LOCKABLE;
            return;
    }
    if (gGameSession->eventState != 0) {
        Actor00300_Fn048D4(arg0, arg1);
        return;
    }
    Actor00300_Fn04958(arg0, arg1);
}

static void Actor00300_Fn048D4(Enemy* arg0, Task* arg1)
{
    TmdObject*       obj;
    _Actor00300Work* work;
    s16              flags;

    work = arg1->work;
    obj  = arg1->extra.tmd;
    if (gGameSession->eventState != 0) {
        flags      = ((work->eventDrawFlags & 1) == 0) * TMD_OBJECT_SKIP_ACTIVE_DRAW;
        obj->flags = flags;
        if (work->eventDrawFlags & 2) {
            obj->flags = flags | TMD_OBJECT_SKIP_AUTO_BUFFER;
        }
    }
    Actor00300_Fn04ED4(arg1);
    Actor00300_Fn04FB0(arg1);
    Actor00300_Fn05008(arg1);
}

static void Actor00300_Fn04958(Enemy* arg0, Task* arg1)
{
    GfxCoord*        coord;
    _Actor00300Work* work;

    work  = arg1->work;
    coord = arg1->extra.tmd->coords;
    if (work->patrolPoints != NULL) {
        if (arg0->reactionFlags != 0) {
            Actor00300_Fn04A2C(arg1);
        }
        Actor00300_Fn00E54(arg1);
        Actor00300_Fn04C20(arg1);
        if (work->turnRate != 0) {
            Actor00300_Fn032BC(arg1);
        }
        Actor00300_Fn04E30(arg1);
        Actor00300_Fn04ED4(arg1);
        if (work->hitTwistActive != 0) {
            Actor00300_Fn0340C(arg1);
        }
        Actor00300_Fn03A1C(arg1);
        coord->composeStamp = GRAPHICS_COORD_DIRTY;
        Gp_UpdateCoord(coord);
        Actor00300_Fn04FB0(arg1);
        Actor00300_Fn05008(arg1);
    }
}

static void Actor00300_Fn04A2C(Task* arg0)
{
    _Actor00300Work* work;
    Enemy*           enemy;
    s16              damage;
    u8               flags;

    enemy = arg0->spawnArg2.pointer;
    flags = enemy->reactionFlags;
    work  = arg0->work;
    if (flags & ENEMY_REACTION_STAGGER) {
        enemy->reactionFlags = flags & ENEMY_REACTION_STAGGER_CLEAR;
    }
    if ((enemy->reactionFlags & ENEMY_REACTION_BUILDUP) && (work->action != ACTOR_00300_ACTION_HURT)) {
        work->action     = ACTOR_00300_ACTION_BUILDUP;
        work->actionStep = 0;
    }
    if (enemy->reactionFlags & ENEMY_REACTION_DAMAGE_OVER_TIME_BITS) {
        damage          = Gp_TickObjFlag4(enemy);
        work->hitDamage = damage;
        if (damage != 0) {
            func_800DA6E8(&enemy->node, (s32)damage, 0);
            enemy->hp        = (u16)enemy->hp - (u16)work->hitDamage;
            work->action     = ACTOR_00300_ACTION_HURT;
            work->actionStep = 0;
        }
        if (Gp_ObjFlag4Expired(enemy) != 0) {
            enemy->reactionFlags &= ENEMY_REACTION_DAMAGE_OVER_TIME_CLEAR;
        }
    }
}

#include "../../shared/player_detection_segment.inc.c"

/// State handlers of the task `Actor00300_Fn05138` dispatches, indexed by
/// `Task::state`: a setup that attaches the coordinate to the parent's and
/// moves to state 1, an empty state, and `enemyDestroy`.
static const EnemyTaskFuncTable3 Actor00300_D0003C = {
    {
        Actor00300_Fn05194,
        Actor00300_Fn03F40,
        enemyDestroy,
    },
};

/// State handlers of the task `Actor00300_Fn0521C` dispatches, indexed by
/// `Task::state`. The first sets the task up and moves it to state 1, the
/// second moves it on to state 2, and the third destroys it once its timer
/// has run out.
static const EnemyTaskFuncTable3 Actor00300_D00048 = {
    {
        Actor00300_Fn040A4,
        Actor00300_Fn04370,
        Actor00300_Fn05278,
    },
};

static void Actor00300_Fn04C20(Task* arg0)
{
    _Actor00300Work* work;

    work = arg0->work;
    switch (work->action) {
        case ACTOR_00300_ACTION_PATROL:
            Actor00300_Fn01678(arg0);
            break;
        case ACTOR_00300_ACTION_PURSUE:
            Actor00300_Fn019C0(arg0);
            break;
        case ACTOR_00300_ACTION_FIREBALL:
            Actor00300_Fn01F9C(arg0);
            break;
        case ACTOR_00300_ACTION_DRAIN:
            Actor00300_Fn02620(arg0);
            break;
        case ACTOR_00300_ACTION_HEAL:
            Actor00300_Fn028D0(arg0);
            break;
        case ACTOR_00300_ACTION_HURT:
            Actor00300_Fn02CE8(arg0);
            break;
        case ACTOR_00300_ACTION_BUILDUP:
            Actor00300_Fn04D28(arg0);
            break;
        case ACTOR_00300_ACTION_RECHARGE:
            Actor00300_Fn030B8(arg0);
            break;
        case ACTOR_00300_ACTION_DEAD:
            break;
    }

    if (work->action != ACTOR_00300_ACTION_DRAIN) {
        if (work->drainScaleY != 0) {
            if (work->drainScaleY > 0) {
                work->drainScaleY -= 0x100;
            } else if (work->drainScaleY < 0) {
                work->drainScaleY = 0;
            }
        }
    }
}

static void Actor00300_Fn04D28(Task* arg0)
{
    _Actor00300Work* work;
    Enemy*           enemy;
    s32              state;
    s32              value;
    EffectWork*      effect;

    work  = arg0->work;
    state = work->actionStep;
    switch (state) {
        case 0:
            effect      = work->chargeEffect;
            work->speed = 0;
            work->anim  = ACTOR_00300_ANIM_BUILDUP;
            if (effect != NULL) {
                effect->task->state     = 3;
                work->chargeEffect      = NULL;
                work->chargeEffectTimer = 0;
                SndEvt_EnqueueType7(work->chargeSound, 1);
            }
            if (Gp_TickObjFlag2(arg0->spawnArg2.pointer) != 0) {
                enemy                 = arg0->spawnArg2.pointer;
                enemy->reactionFlags &= ENEMY_REACTION_BUILDUP_CLEAR;
                work->anim            = ACTOR_00300_ANIM_BUILDUP_END;
                work->actionStep      = 1;
            }
            break;
        case 1:
            if (work->animFrame >= 0xB) {
                work->action     = state;
                work->actionStep = state;
                value            = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                gRandomLcgState  = value;
                work->timer      = ((u32)value >> 16) & 0x1F;
            }
            break;
    }
}

static void Actor00300_Fn04E30(Task* arg0)
{
    _Actor00300Work* work;
    GfxCoord*        coord;

    coord              = arg0->extra.tmd->coords;
    work               = arg0->work;
    work->prevPos.vx   = coord->coord.t[0];
    work->prevPos.vy   = coord->coord.t[1];
    work->prevPos.vz   = coord->coord.t[2];
    coord->coord.t[0] += (coord->coord.m[0][2] * work->speed) >> 12;
    if (work->deathStage < 2) {
        coord->coord.t[1] += 0x80;
    }
    coord->coord.t[2] += (coord->coord.m[2][2] * work->speed) >> 12;
}

static void Actor00300_Fn04ED4(Task* arg0)
{
    _Actor00300Work* work;
    s32              i;
    s32              val;

    work = arg0->work;
    if (work->anim != work->playingAnim) {
        work->playingAnim = work->anim;
        work->animFrame   = 0;
        if (work->reactionBlend == 0) {
            val = Actor00300_D16394[work->anim];
        } else {
            val = 8;
        }
        for (i = 1; i < ARRAY_SIZE(work->rig.slots); i++) {
            animationSeekSlotWithBlend(&work->rig.anim, i, work->anim, 0, val);
        }
    } else {
        work->animFrame++;
        for (i = 1; i < ARRAY_SIZE(work->rig.slots); i++) {
            animationTickSlot(&work->rig.anim, i);
        }
    }
}

static void Actor00300_Fn04FB0(Task* arg0)
{
    GfxCoord* coord;
    VECTOR    vec;

    coord  = arg0->extra.tmd->coords;
    vec.vx = coord->workm.t[0];
    vec.vy = coord->workm.t[1];
    vec.vz = coord->workm.t[2];
    Gp_UpdateActorColor(arg0->spawnArg2.pointer, &vec, 0, 0);
}

static void Actor00300_Fn05008(Task* arg0)
{
    VECTOR3   vec;
    GfxCoord* coord;
    GfxCoord* part;

    coord  = arg0->extra.tmd->coords;
    part   = coord + 3;
    vec.vx = part->workm.t[0];
    vec.vy = coord->workm.t[1];
    vec.vz = part->workm.t[2];
    Gp_DrawEffGroundQuad(&vec, 0x300, 0x80);
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
    scratch->matrix.rotationWords.m00M01    = ONE;
    scratch->matrix.rotationWords.m02M10    = 0;
    scratch->matrix.rotationWords.m11M12    = ONE;
    scratch->matrix.rotationWords.m20M21    = 0;
    scratch->matrix.rotationWords.m22       = ONE;
    ScaleMatrix(&scratch->matrix.mat, &scratch->scale);
    MulMatrix(&coord->coord, &scratch->matrix.mat);
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
    SCRATCH_STACK_RELEASE_BLOCK(ActorScaleScratch);
}

void Actor00300_Fn05138(Task* arg0)
{
    EnemyTaskFuncTable3 sp;

    sp = Actor00300_D0003C;
    sp.funcs[arg0->state](((Enemy*)arg0->spawnArg2.pointer), arg0);
}

static void Actor00300_Fn05194(Enemy* arg0, Task* arg1)
{
    Task*            parent;
    TmdObject*       obj;
    GfxCoord*        coord;
    GfxCoord*        parentCoord;
    _Actor00300Work* work;

    parent              = arg1->parent;
    obj                 = arg1->extra.tmd;
    parentCoord         = parent->extra.tmd->coords;
    coord               = obj->coords;
    work                = parent->work;
    obj->flags          = 0;
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
    coord->parent       = parentCoord + 7;
    obj->lightMtx       = &work->lightMtx;
    obj->colorMtx       = &work->colorMtx;
    work->drainModelMtx = coord->coord;
    work->drainScaleY   = 0;
    arg1->state         = 1;
}

void Actor00300_Fn0521C(Task* arg0)
{
    EnemyTaskFuncTable3 sp;

    sp = Actor00300_D00048;
    sp.funcs[arg0->state](((Enemy*)arg0->spawnArg2.pointer), arg0);
}

static void Actor00300_Fn05278(Enemy* arg0, Task* arg1)
{
    Actor00300InitWork* work;
    u16                 timer;

    work = arg1->work;
    switch (work->pad8A) {
        case 0:
            Gp_UnlinkObj(&work->obj0);
            Gp_UnlinkObj(&work->obj38);
            work->timer = 0x3C;
            work->pad8A = 1;
            return;
        case 1:
            timer       = work->timer - 1;
            work->timer = timer;
            if ((s16)timer <= 0) {
                enemyDestroy(arg0, arg1);
            }
            return;
    }
}

s32 Actor00300_Fn05304(Task* arg0, s32 arg1, AnimationPlayRequest* args, s32 arg3)
{
    _Actor00300Work* work;
    s32              i;
    s32              frames;
    s16              anim;

    work              = arg0->work;
    anim              = args->animationId + ACTOR_00300_ANIM_EVENT_BASE;
    work->anim        = anim;
    work->playingAnim = anim;
    frames            = 0;
    if (args->blend != ANIMATION_BLEND_RESET) {
        frames = args->blendFrames;
    }
    for (i = 1; i < ARRAY_SIZE(work->rig.slots); i++) {
        animationSeekSlotWithBlend(&work->rig.anim, i, work->anim, 0, frames);
    }
    return 0;
}

#include "../../shared/actor_messages_place_rot_matrix.inc.c"

s32 Actor00300_Fn053EC(Task* arg0, s32 arg1, s32 arg2, s32 arg3)
{
    TmdObject*       obj;
    _Actor00300Work* work;

    obj  = arg0->extra.tmd;
    work = arg0->work;
    if (!(arg2 & 1)) {
        obj->flags = TMD_OBJECT_SKIP_ACTIVE_DRAW;
    } else {
        obj->flags = 0;
    }
    if (arg2 & 2) {
        obj->flags |= TMD_OBJECT_SKIP_AUTO_BUFFER;
    }
    work->eventDrawFlags = arg2;
    return 0;
}

s32 Actor00300_Fn05434(Task* arg0, s32 arg1, ActorCommand* args, s32 arg3)
{
    _Actor00300Work* work;
    Enemy*           enemy;

    work  = arg0->work;
    enemy = arg0->spawnArg2.pointer;
    if (args->command != 0) {
        enemy->recs = 0;
        worldTargetUnlinkNode(&enemy->node);
        Gp_UnlinkObj(&work->sightBody);
        Gp_UnlinkObj(&work->gridBody);
        Gp_UnlinkObj(&work->hitBody);
        Gp_UnlinkObj(&work->drainBody);
        enemyDestroy(enemy, arg0);
    }
    return 0;
}
