#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/inline_c.h>

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
#include "gameplay/object_fields.h"
#include "gameplay/enemy_params.h"
#include "gameplay/player_actor.h"
#include "gameplay/room_effects.h"
#include "gameplay/scene_combat.h"
#include "gameplay/scene_runtime.h"
#include "gameplay/world_collision.h"
#include "gameplay/world_coords.h"
#include "gameplay/world_targets.h"

#include "main/coord.h"
#include "main/random.h"
#include "main/gfx.h"
#include "main/gfx_types.h"
#include "main/mem.h"
#include "main/scratch.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/sound.h"
#include "main/task_types.h"
#include "main/tmd.h"
#include "main/tmd_types.h"
#include "main/wipsys.h"
#include "main/wipsys_types.h"

#include "overlay.h"

/// Values of `_Actor03800Work::mode`: how the actor stands in the room.
///
/// The spawn takes it from the tens digit of the placement's mode. The two
/// perched modes hang the model under the work block's own coordinate, turned
/// about X from upright, and end with a fall to the floor; every mode becomes
/// `FLOOR` once the actor is loose in the room.
enum {
    ACTOR_03800_MODE_FLOOR   = 0, // on the floor: gravity, the grid body and the ground shadow are on
    ACTOR_03800_MODE_WALL    = 1, // perched a quarter turn about X from upright
    ACTOR_03800_MODE_CEILING = 2, // perched half a turn about X from upright
    ACTOR_03800_MODE_SHRINE  = 3  // hidden with its bodies off until the shrine's script reveals its enemies
};

/// Values of `_Actor03800Work::action`: the handler the per-frame tick runs.
///
/// A handler numbers its own stages in `actionStep`, from 0 on entry.
/// Animation numbers are `ACTOR_03800_ANIM_*`.
enum {
    ACTOR_03800_ACTION_IDLE         = 0,  // stands for a random time, then turns to a random heading; wanders once alerted, charges at a noise
    ACTOR_03800_ACTION_WANDER       = 1,  // walks with random changes of heading until `wanderTimer` runs out; a wall met slowly sends it back to idling, one met at speed into a leap away; charges at a noise
    ACTOR_03800_ACTION_CHARGE       = 2,  // turns to face the player, then runs straight for a random time and wanders
    ACTOR_03800_ACTION_KNOCKED_OVER = 3,  // thrown onto its back, sliding away from the player; lies there for `overturnedTimer`, then gets up
    ACTOR_03800_ACTION_GET_UP       = 4,  // rights itself; upright again from tick 30 and charging from tick 58
    ACTOR_03800_ACTION_FLINCH       = 5,  // recoils from a hit, then charges, returns to its buildup hold or goes on lying overturned; perched, it drops instead
    ACTOR_03800_ACTION_BUILDUP      = 6,  // held still by a buildup reaction, flinching every few ticks, until the reaction runs out
    ACTOR_03800_ACTION_DIE          = 7,  // out of hit points: hands the task to its death state, after falling to the floor if perched
    ACTOR_03800_ACTION_WALL_WAIT    = 8,  // perched in `MODE_WALL`, turning now and then; drops at a noise or on `attackTouched`
    ACTOR_03800_ACTION_CEILING_WAIT = 9,  // the same wait in `MODE_CEILING`
    ACTOR_03800_ACTION_DROP         = 10, // waits out `timer`, falls to the floor, lands overturned and gets up
    ACTOR_03800_ACTION_SHRINE_WAIT  = 11, // follows the shrine's script: hidden, shown standing once revealed, and wandering 90 ticks after its release
    ACTOR_03800_ACTION_LEAP_AWAY    = 12  // jumps along its facing for ticks 8 to 16 of the animation, away from the player, then wanders
};

/// Values of `_Actor03800Work::anim`: indices into the package's
/// animation-set table, named for the action that plays each.
enum {
    ACTOR_03800_ANIM_STAND             = 1, // idling; also the death of an upright actor
    ACTOR_03800_ANIM_WALK              = 2, // turning on the spot and wandering
    ACTOR_03800_ANIM_KNOCKED_OVER      = 3,
    ACTOR_03800_ANIM_GET_UP            = 4,
    ACTOR_03800_ANIM_DIE_OVERTURNED    = 5, // death of an overturned actor, and of one that fell from its perch
    ACTOR_03800_ANIM_FLINCH_OVERTURNED = 6,
    ACTOR_03800_ANIM_FLINCH_PERCHED    = 7,
    ACTOR_03800_ANIM_LAND              = 8, // landing after a drop
    ACTOR_03800_ANIM_CHARGE            = 9,
    ACTOR_03800_ANIM_LEAP              = 10,
    ACTOR_03800_ANIM_FLINCH            = 11
};

/// Work block of the package's enemy task.
///
/// The spawn handler allocates it zeroed and keeps it at `Task::work`. It
/// holds the animation rig, storage for the model's matrices, three collision
/// spheres with their contact tables, the coordinate a perched actor hangs
/// under, and the state the per-frame tick steers the actor with.
///
/// All three spheres ride the model's root coordinate. Angles are 4096ths of a
/// turn, and `yaw` is the heading about Y. Timers count ticks.
typedef struct {
    ActorAnimRig6         rig;               // playback of the model's parts; slots 1 to 5 are driven
    MATRIX                colorMtx;          // storage for the model's `TmdObject::colorMtx`
    MATRIX                lightMtx;          // storage for the model's `TmdObject::lightMtx`
    WorldCollisionBody    hitBody;           // sphere of radius 250, 250 above the root, that takes the hits and meets other bodies; pair tests are off in `MODE_SHRINE`
    WorldCollisionContact hitContacts[3];    // contacts of `hitBody`; also the enemy's hit records
    WorldCollisionBody    gridBody;          // sphere the room grid tests and gravity rests on the floor, on only in `MODE_FLOOR` and during a fall; radius and height 250, or 350 while `overturned`
    WorldCollisionContact gridContacts[4];   // contacts of `gridBody`, which push the root out of the room's faces
    WorldCollisionBody    attackBody;        // sphere of radius 200 ahead of the root, keyed with the package's attack scaled up by `speed`; off while knocked over, landing and leaping
    WorldCollisionContact attackContacts[1]; // contact of `attackBody`
    EffectSpawnArg        hitEffectArg;      // argument record of the effect a hit spawns, hung off the model's part 3
    MATRIX                savedRootMtx;      // root transform kept to rebuild from: the model's own while perched, turned to `yaw` on landing; at death, the pose the collapse scales
    SVECTOR               prevPos;           // root position before the tick's movement, restored when the grid cannot resolve the contact
    GfxCoord              perchCoord;        // coordinate the model's root hangs under in the perched modes: the root's own transform turned about X, parented to the view; its translation is where the actor lands
    GfxCoord*             rootCoord;         // coordinate that places the actor in the room: `perchCoord` while perched, otherwise the model's root
    s16                   anim;              // `ACTOR_03800_ANIM_*` the actions ask for
    s16                   playingAnim;       // `anim` the slots were last started on; an action sets a different value to restart the same animation
    s16                   animFrame;         // ticks since `playingAnim` was started
    s16                   hitCooldown;       // ticks left in which further attack contacts are ignored, set by the attack that landed; 0 when hits count
    s16                   mode;              // `ACTOR_03800_MODE_*`
    s16                   action;            // `ACTOR_03800_ACTION_*`
    s16                   actionStep;        // stage of the running action, 0 on entry; the death state of the task counts its own stages in it (0 begin, 1 collapse, 2 destroy, 3 burst)
    s16                   timer;             // countdown of the running action's wait; the death state counts it up instead
    s16                   wanderTimer;       // ticks left before `ACTION_WANDER` gives way to idling
    s16                   deathScaleY;       // Y scale of the collapsing body, 4096 for full height; shrinks by 80 a tick to about an eighth
    s16                   speed;             // distance moved along the facing each tick; negative backs away
    s16                   accel;             // added to `speed` each tick up to a limit of 50; 0 leaves `speed` alone
    s16                   turnRate;          // most `yaw` may change in a tick on its way to `targetYaw`; 0 leaves the rotation alone
    s16                   yaw;               // heading of the model's root, read back from its rotation before each turn
    s16                   targetYaw;         // heading `yaw` is turned toward
    s16                   fallSpeed;         // added to the root's Y each tick: 128 on the floor, 256 in a drop, 0 while perched
    s16                   burstStage;        // 0 unless the killing hit bursts the body; then 1 and 2 over the first death ticks, after which the model gives up its buffers and up to three fragment effects are spawned
    s16                   stepSoundTimer;    // ticks to the next footstep sound, one every 12; 0 keeps them silent
    s16                   attackTouched;     // 1 on a tick `attackContacts` held a category-1 contact (0 otherwise)
    s16                   overturned;        // 1 while the actor is not on its feet: perched, or on its back after a fall or a knock-over. Hits then do triple damage and do not knock it over
    s16                   hitWall;           // set when `gridBody` meets a face that is not floor while upright; cleared by the action that answers it
    s16                   shadowShade;       // vertex colour of the ground shadow in `MODE_FLOOR`; 128, or -1 for none
    s16                   landed;            // set when `gridBody` meets a floor face; a fall waits for it
    byte                  pad_376[2];        // never accessed
    s16                   deathAnimChosen;   // 1 when a perched death has already started its animation, so the death state keeps it
    s16                   alerted;           // 1 once a noise, a cast or a touch has roused the actor: idling then leads to wandering instead of another wait
    s16                   overturnedTimer;   // ticks `ACTION_KNOCKED_OVER` lies on its back: 240 plus 10 for each percent of hit points lost
    s16                   inBuildup;         // 1 from a buildup reaction on the floor until it runs out; a flinch then returns to `ACTION_BUILDUP`
    byte                  pad_380[4];        // never accessed; the allocation's last four bytes
} _Actor03800Work;
STATIC_ASSERT_SIZEOF(_Actor03800Work, 0x384);

/// Scratch-stack block of `ACTOR_03800_ACTION_KNOCKED_OVER`, which slides the
/// actor away from the player while it is thrown onto its back.
///
/// The action reserves one block each tick and releases it before returning,
/// whichever stage it is in. Only the ticks of the slide fill it: the offset
/// from the player is taken, normalised, and the root is moved along the
/// result on X and Z. Nothing carries over from one tick to the next.
typedef struct {
    VECTOR  fromPlayer; // Root's position minus the player's, world units, all three axes; `pad` is never written
    SVECTOR direction;  // `fromPlayer` normalised, 4096 = 1.0; the root moves 17/512 of its X and Z each tick of the slide, and `vy` is never read
} _Actor03800KnockedOverScratch;
STATIC_ASSERT_SIZEOF(_Actor03800KnockedOverScratch, 0x18);

extern void*     D_80067704[1];
static TmdSource _gActor03800BlackBeetleEffect1;
static TmdSource _gActor03800BlackBeetleEffect2;
static TmdSource _gActor03800BlackBeetleEffect3;
static TmdSource _gActor03800BlackBeetleEffect4;
static TmdSource _gActor03800BlackBeetleEffect5;

extern s16           Actor03800_D05F90[];
extern s16           Actor03800_D05FA8[];
extern DamageAttack  Actor03800_D05F40[1];
extern EnemyParams   Actor03800_D05F44;
extern AnimationSet* Actor03800_D05F60[12];

/* Scratchpad stack pointer, initialised by GameMain (see src/main/gamemain.c). */

static void Actor03800_Fn000B8(Enemy* arg0, Task* arg1);
static void Actor03800_Fn003B8(Task* arg0);
static void Actor03800_Fn00974(Task* arg0);
static void Actor03800_Fn00A98(Task* arg0);
static void Actor03800_Fn026F8(Task* arg0);
static void Actor03800_Fn02848(Task* arg0);
static void Actor03800_Fn02998(Enemy* arg0, Task* arg1);
static void Actor03800_Fn02E50(Task* arg0);
static void Actor03800_Fn03008(Task* actor, u32 variant);
static void Actor03800_Fn031B8(Enemy* arg0, Task* arg1);
static void Actor03800_Fn032D8(Task* arg0);
static void Actor03800_Fn03420(Task* arg0);
static void Actor03800_Fn034B0(Task* arg0);
static void Actor03800_Fn03594(Task* arg0);
static void Actor03800_Fn03628(Task* arg0);
static void Actor03800_Fn036EC(Task* arg0);
static void Actor03800_Fn03744(Task* arg0);
static void Actor03800_Fn037E0(Task* arg0);

/// State handlers `Actor03800_Fn0315C` dispatches, indexed by the task's state
/// (`Task::state`): the spawn state that allocates the work block and
/// moves to state 1, the per-frame tick, and the state-2 handler the tick hands
/// over to, which carries the death sequence.
static const EnemyTaskFuncTable3 Actor03800_D00004 = {
    {
        Actor03800_Fn000B8,
        Actor03800_Fn031B8,
        Actor03800_Fn02998,
    },
};

static AnimationSet _gActor03800Actor103800Animation04B24;
static AnimationSet _gActor03800Actor103800Animation04D20;
static AnimationSet _gActor03800Actor103800Animation04FE0;
static AnimationSet _gActor03800Actor103800Animation05534;
static AnimationSet _gActor03800Actor103800Animation055F0;
static AnimationSet _gActor03800Actor103800Animation05734;
static AnimationSet _gActor03800Actor103800Animation05824;
static AnimationSet _gActor03800Actor103800Animation05988;
static AnimationSet _gActor03800Actor103800Animation05B84;
static AnimationSet _gActor03800Actor103800Animation05E14;
static AnimationSet _gActor03800Actor103800Animation05F18;
static TmdSource    _gActor03800BlackBeetleBody;
static void         Actor03800_Fn0315C(Task*);

static TmdBone _gActor03800BlackBeetleBodySkeleton[6] = {
#include "assets/black_beetle_body_skeleton.inc"
};

static u32 _gActor03800BlackBeetleBodyPartVerts[6] = {
#include "assets/black_beetle_body_partVerts.inc"
};

static SVECTOR _gActor03800BlackBeetleBodyVerts[33] = {
#include "assets/black_beetle_body_verts.inc"
};

static SVECTOR _gActor03800BlackBeetleBodyNormals[42] = {
#include "assets/black_beetle_body_normals.inc"
};

static u32 _gActor03800BlackBeetleBodyStream[482] = {
#include "assets/black_beetle_body_stream.inc"
};

static TmdSource _gActor03800BlackBeetleBody = {
    0,
    2120,
    1072,
    6,
    _gActor03800BlackBeetleBodyPartVerts,
    _gActor03800BlackBeetleBodyVerts,
    _gActor03800BlackBeetleBodyNormals,
    _gActor03800BlackBeetleBodySkeleton,
    _gActor03800BlackBeetleBodyStream,
};

static TmdBone _gActor03800BlackBeetleEffect1Skeleton[1] = {
#include "assets/black_beetle_effect_1_skeleton.inc"
};

static u32 _gActor03800BlackBeetleEffect1PartVerts[1] = {
#include "assets/black_beetle_effect_1_partVerts.inc"
};

static SVECTOR _gActor03800BlackBeetleEffect1Verts[8] = {
#include "assets/black_beetle_effect_1_verts.inc"
};

static SVECTOR _gActor03800BlackBeetleEffect1Normals[8] = {
#include "assets/black_beetle_effect_1_normals.inc"
};

static u32 _gActor03800BlackBeetleEffect1Stream[76] = {
#include "assets/black_beetle_effect_1_stream.inc"
};

static TmdSource _gActor03800BlackBeetleEffect1 = {
    0,
    452,
    0,
    1,
    _gActor03800BlackBeetleEffect1PartVerts,
    _gActor03800BlackBeetleEffect1Verts,
    _gActor03800BlackBeetleEffect1Normals,
    _gActor03800BlackBeetleEffect1Skeleton,
    _gActor03800BlackBeetleEffect1Stream,
};

static TmdBone _gActor03800BlackBeetleEffect2Skeleton[1] = {
#include "assets/black_beetle_effect_2_skeleton.inc"
};

static u32 _gActor03800BlackBeetleEffect2PartVerts[1] = {
#include "assets/black_beetle_effect_2_partVerts.inc"
};

static SVECTOR _gActor03800BlackBeetleEffect2Verts[4] = {
#include "assets/black_beetle_effect_2_verts.inc"
};

static SVECTOR _gActor03800BlackBeetleEffect2Normals[4] = {
#include "assets/black_beetle_effect_2_normals.inc"
};

static u32 _gActor03800BlackBeetleEffect2Stream[30] = {
#include "assets/black_beetle_effect_2_stream.inc"
};

static TmdSource _gActor03800BlackBeetleEffect2 = {
    0,
    160,
    0,
    1,
    _gActor03800BlackBeetleEffect2PartVerts,
    _gActor03800BlackBeetleEffect2Verts,
    _gActor03800BlackBeetleEffect2Normals,
    _gActor03800BlackBeetleEffect2Skeleton,
    _gActor03800BlackBeetleEffect2Stream,
};

static TmdBone _gActor03800BlackBeetleEffect3Skeleton[1] = {
#include "assets/black_beetle_effect_3_skeleton.inc"
};

static u32 _gActor03800BlackBeetleEffect3PartVerts[1] = {
#include "assets/black_beetle_effect_3_partVerts.inc"
};

static SVECTOR _gActor03800BlackBeetleEffect3Verts[4] = {
#include "assets/black_beetle_effect_3_verts.inc"
};

static SVECTOR _gActor03800BlackBeetleEffect3Normals[4] = {
#include "assets/black_beetle_effect_3_normals.inc"
};

static u32 _gActor03800BlackBeetleEffect3Stream[30] = {
#include "assets/black_beetle_effect_3_stream.inc"
};

static TmdSource _gActor03800BlackBeetleEffect3 = {
    0,
    160,
    0,
    1,
    _gActor03800BlackBeetleEffect3PartVerts,
    _gActor03800BlackBeetleEffect3Verts,
    _gActor03800BlackBeetleEffect3Normals,
    _gActor03800BlackBeetleEffect3Skeleton,
    _gActor03800BlackBeetleEffect3Stream,
};

static TmdBone _gActor03800BlackBeetleEffect4Skeleton[1] = {
#include "assets/black_beetle_effect_4_skeleton.inc"
};

static u32 _gActor03800BlackBeetleEffect4PartVerts[1] = {
#include "assets/black_beetle_effect_4_partVerts.inc"
};

static SVECTOR _gActor03800BlackBeetleEffect4Verts[4] = {
#include "assets/black_beetle_effect_4_verts.inc"
};

static SVECTOR _gActor03800BlackBeetleEffect4Normals[2] = {
#include "assets/black_beetle_effect_4_normals.inc"
};

static u32 _gActor03800BlackBeetleEffect4Stream[18] = {
#include "assets/black_beetle_effect_4_stream.inc"
};

static TmdSource _gActor03800BlackBeetleEffect4 = {
    0,
    104,
    0,
    1,
    _gActor03800BlackBeetleEffect4PartVerts,
    _gActor03800BlackBeetleEffect4Verts,
    _gActor03800BlackBeetleEffect4Normals,
    _gActor03800BlackBeetleEffect4Skeleton,
    _gActor03800BlackBeetleEffect4Stream,
};

static TmdBone _gActor03800BlackBeetleEffect5Skeleton[1] = {
#include "assets/black_beetle_effect_5_skeleton.inc"
};

static u32 _gActor03800BlackBeetleEffect5PartVerts[1] = {
#include "assets/black_beetle_effect_5_partVerts.inc"
};

static SVECTOR _gActor03800BlackBeetleEffect5Verts[4] = {
#include "assets/black_beetle_effect_5_verts.inc"
};

static SVECTOR _gActor03800BlackBeetleEffect5Normals[2] = {
#include "assets/black_beetle_effect_5_normals.inc"
};

static u32 _gActor03800BlackBeetleEffect5Stream[18] = {
#include "assets/black_beetle_effect_5_stream.inc"
};

static TmdSource _gActor03800BlackBeetleEffect5 = {
    0,
    104,
    0,
    1,
    _gActor03800BlackBeetleEffect5PartVerts,
    _gActor03800BlackBeetleEffect5Verts,
    _gActor03800BlackBeetleEffect5Normals,
    _gActor03800BlackBeetleEffect5Skeleton,
    _gActor03800BlackBeetleEffect5Stream,
};

static AnimationPackedPose _gActor03800Actor103800Animation04B24Bank1[7] = {
#include "assets/actor_103800_animation_04B24_bank1.inc"
};

static AnimationPackedRotation _gActor03800Actor103800Animation04B24Bank4[24] = {
#include "assets/actor_103800_animation_04B24_bank4.inc"
};

static AnimationRecord _gActor03800Actor103800Animation04B24Records[69] = {
#include "assets/actor_103800_animation_04B24_records.inc"
};

static u16 _gActor03800Actor103800Animation04B24Indices[6] = {
#include "assets/actor_103800_animation_04B24_indices.inc"
};

static AnimationSet _gActor03800Actor103800Animation04B24 = {
    _gActor03800Actor103800Animation04B24Records,
    _gActor03800Actor103800Animation04B24Indices,
    { NULL, _gActor03800Actor103800Animation04B24Bank1, NULL, NULL, _gActor03800Actor103800Animation04B24Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor03800Actor103800Animation04D20Bank1[12] = {
#include "assets/actor_103800_animation_04D20_bank1.inc"
};

static AnimationPackedRotation _gActor03800Actor103800Animation04D20Bank4[12] = {
#include "assets/actor_103800_animation_04D20_bank4.inc"
};

static AnimationRecord _gActor03800Actor103800Animation04D20Records[66] = {
#include "assets/actor_103800_animation_04D20_records.inc"
};

static u16 _gActor03800Actor103800Animation04D20Indices[6] = {
#include "assets/actor_103800_animation_04D20_indices.inc"
};

static AnimationSet _gActor03800Actor103800Animation04D20 = {
    _gActor03800Actor103800Animation04D20Records,
    _gActor03800Actor103800Animation04D20Indices,
    { NULL, _gActor03800Actor103800Animation04D20Bank1, NULL, NULL, _gActor03800Actor103800Animation04D20Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor03800Actor103800Animation04FE0Bank1[17] = {
#include "assets/actor_103800_animation_04FE0_bank1.inc"
};

static AnimationPackedRotation _gActor03800Actor103800Animation04FE0Bank4[43] = {
#include "assets/actor_103800_animation_04FE0_bank4.inc"
};

static AnimationRecord _gActor03800Actor103800Animation04FE0Records[69] = {
#include "assets/actor_103800_animation_04FE0_records.inc"
};

static u16 _gActor03800Actor103800Animation04FE0Indices[6] = {
#include "assets/actor_103800_animation_04FE0_indices.inc"
};

static AnimationSet _gActor03800Actor103800Animation04FE0 = {
    _gActor03800Actor103800Animation04FE0Records,
    _gActor03800Actor103800Animation04FE0Indices,
    { NULL, _gActor03800Actor103800Animation04FE0Bank1, NULL, NULL, _gActor03800Actor103800Animation04FE0Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor03800Actor103800Animation05534Bank1[31] = {
#include "assets/actor_103800_animation_05534_bank1.inc"
};

static AnimationPackedRotation _gActor03800Actor103800Animation05534Bank4[95] = {
#include "assets/actor_103800_animation_05534_bank4.inc"
};

static AnimationRecord _gActor03800Actor103800Animation05534Records[140] = {
#include "assets/actor_103800_animation_05534_records.inc"
};

static u16 _gActor03800Actor103800Animation05534Indices[6] = {
#include "assets/actor_103800_animation_05534_indices.inc"
};

static AnimationSet _gActor03800Actor103800Animation05534 = {
    _gActor03800Actor103800Animation05534Records,
    _gActor03800Actor103800Animation05534Indices,
    { NULL, _gActor03800Actor103800Animation05534Bank1, NULL, NULL, _gActor03800Actor103800Animation05534Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor03800Actor103800Animation055F0Bank1[2] = {
#include "assets/actor_103800_animation_055F0_bank1.inc"
};

static AnimationPackedRotation _gActor03800Actor103800Animation055F0Bank4[4] = {
#include "assets/actor_103800_animation_055F0_bank4.inc"
};

static AnimationRecord _gActor03800Actor103800Animation055F0Records[24] = {
#include "assets/actor_103800_animation_055F0_records.inc"
};

static u16 _gActor03800Actor103800Animation055F0Indices[6] = {
#include "assets/actor_103800_animation_055F0_indices.inc"
};

static AnimationSet _gActor03800Actor103800Animation055F0 = {
    _gActor03800Actor103800Animation055F0Records,
    _gActor03800Actor103800Animation055F0Indices,
    { NULL, _gActor03800Actor103800Animation055F0Bank1, NULL, NULL, _gActor03800Actor103800Animation055F0Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor03800Actor103800Animation05734Bank1[7] = {
#include "assets/actor_103800_animation_05734_bank1.inc"
};

static AnimationPackedRotation _gActor03800Actor103800Animation05734Bank4[14] = {
#include "assets/actor_103800_animation_05734_bank4.inc"
};

static AnimationRecord _gActor03800Actor103800Animation05734Records[33] = {
#include "assets/actor_103800_animation_05734_records.inc"
};

static u16 _gActor03800Actor103800Animation05734Indices[6] = {
#include "assets/actor_103800_animation_05734_indices.inc"
};

static AnimationSet _gActor03800Actor103800Animation05734 = {
    _gActor03800Actor103800Animation05734Records,
    _gActor03800Actor103800Animation05734Indices,
    { NULL, _gActor03800Actor103800Animation05734Bank1, NULL, NULL, _gActor03800Actor103800Animation05734Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor03800Actor103800Animation05824Bank1[4] = {
#include "assets/actor_103800_animation_05824_bank1.inc"
};

static AnimationPackedRotation _gActor03800Actor103800Animation05824Bank4[12] = {
#include "assets/actor_103800_animation_05824_bank4.inc"
};

static AnimationRecord _gActor03800Actor103800Animation05824Records[23] = {
#include "assets/actor_103800_animation_05824_records.inc"
};

static u16 _gActor03800Actor103800Animation05824Indices[6] = {
#include "assets/actor_103800_animation_05824_indices.inc"
};

static AnimationSet _gActor03800Actor103800Animation05824 = {
    _gActor03800Actor103800Animation05824Records,
    _gActor03800Actor103800Animation05824Indices,
    { NULL, _gActor03800Actor103800Animation05824Bank1, NULL, NULL, _gActor03800Actor103800Animation05824Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor03800Actor103800Animation05988Bank1[7] = {
#include "assets/actor_103800_animation_05988_bank1.inc"
};

static AnimationPackedRotation _gActor03800Actor103800Animation05988Bank4[19] = {
#include "assets/actor_103800_animation_05988_bank4.inc"
};

static AnimationRecord _gActor03800Actor103800Animation05988Records[36] = {
#include "assets/actor_103800_animation_05988_records.inc"
};

static u16 _gActor03800Actor103800Animation05988Indices[6] = {
#include "assets/actor_103800_animation_05988_indices.inc"
};

static AnimationSet _gActor03800Actor103800Animation05988 = {
    _gActor03800Actor103800Animation05988Records,
    _gActor03800Actor103800Animation05988Indices,
    { NULL, _gActor03800Actor103800Animation05988Bank1, NULL, NULL, _gActor03800Actor103800Animation05988Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor03800Actor103800Animation05B84Bank1[12] = {
#include "assets/actor_103800_animation_05B84_bank1.inc"
};

static AnimationPackedRotation _gActor03800Actor103800Animation05B84Bank4[12] = {
#include "assets/actor_103800_animation_05B84_bank4.inc"
};

static AnimationRecord _gActor03800Actor103800Animation05B84Records[66] = {
#include "assets/actor_103800_animation_05B84_records.inc"
};

static u16 _gActor03800Actor103800Animation05B84Indices[6] = {
#include "assets/actor_103800_animation_05B84_indices.inc"
};

static AnimationSet _gActor03800Actor103800Animation05B84 = {
    _gActor03800Actor103800Animation05B84Records,
    _gActor03800Actor103800Animation05B84Indices,
    { NULL, _gActor03800Actor103800Animation05B84Bank1, NULL, NULL, _gActor03800Actor103800Animation05B84Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor03800Actor103800Animation05E14Bank1[16] = {
#include "assets/actor_103800_animation_05E14_bank1.inc"
};

static AnimationPackedRotation _gActor03800Actor103800Animation05E14Bank4[39] = {
#include "assets/actor_103800_animation_05E14_bank4.inc"
};

static AnimationRecord _gActor03800Actor103800Animation05E14Records[64] = {
#include "assets/actor_103800_animation_05E14_records.inc"
};

static u16 _gActor03800Actor103800Animation05E14Indices[6] = {
#include "assets/actor_103800_animation_05E14_indices.inc"
};

static AnimationSet _gActor03800Actor103800Animation05E14 = {
    _gActor03800Actor103800Animation05E14Records,
    _gActor03800Actor103800Animation05E14Indices,
    { NULL, _gActor03800Actor103800Animation05E14Bank1, NULL, NULL, _gActor03800Actor103800Animation05E14Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor03800Actor103800Animation05F18Bank1[4] = {
#include "assets/actor_103800_animation_05F18_bank1.inc"
};

static AnimationPackedRotation _gActor03800Actor103800Animation05F18Bank4[12] = {
#include "assets/actor_103800_animation_05F18_bank4.inc"
};

static AnimationRecord _gActor03800Actor103800Animation05F18Records[28] = {
#include "assets/actor_103800_animation_05F18_records.inc"
};

static u16 _gActor03800Actor103800Animation05F18Indices[6] = {
#include "assets/actor_103800_animation_05F18_indices.inc"
};

static AnimationSet _gActor03800Actor103800Animation05F18 = {
    _gActor03800Actor103800Animation05F18Records,
    _gActor03800Actor103800Animation05F18Indices,
    { NULL, _gActor03800Actor103800Animation05F18Bank1, NULL, NULL, _gActor03800Actor103800Animation05F18Bank4, NULL, NULL, NULL },
};

DamageAttack Actor03800_D05F40[1] = {
    { 6, 7 },
};

EnemyParams Actor03800_D05F44 = { Actor03800_D05F40, 280, 15, 53, 1, 0, 10, 100, 0 };

TaskDesc Actor03800_D05F54 = { { { TASK_BODY_TMD, 96 } }, Actor03800_Fn0315C, { .model = &_gActor03800BlackBeetleBody } };

AnimationSet* Actor03800_D05F60[12] = {
    NULL,
    &_gActor03800Actor103800Animation04B24,
    &_gActor03800Actor103800Animation04D20,
    &_gActor03800Actor103800Animation04FE0,
    &_gActor03800Actor103800Animation05534,
    &_gActor03800Actor103800Animation055F0,
    &_gActor03800Actor103800Animation05734,
    &_gActor03800Actor103800Animation05824,
    &_gActor03800Actor103800Animation05988,
    &_gActor03800Actor103800Animation05B84,
    &_gActor03800Actor103800Animation05E14,
    &_gActor03800Actor103800Animation05F18,
};

s16 Actor03800_D05F90[12] = {
    0,
    8,
    3,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    3,
    0,
};

s16 Actor03800_D05FA8[2] = {
    2,
    1,
};

static void        Actor03800_Fn01150(Task* arg0);
static void        Actor03800_Fn012B4(Task* arg0);
static void        Actor03800_Fn01520(Task* arg0);
static void        Actor03800_Fn0166C(Task* arg0);
static void        Actor03800_Fn01948(Task* arg0);
static void        Actor03800_Fn01AD0(Task* arg0);
static void        Actor03800_Fn01C50(Task* arg0);
static void        Actor03800_Fn01EEC(Task* arg0);
static void        Actor03800_Fn02068(Task* arg0);
static void        Actor03800_Fn021E4(Task* arg0);
static void        Actor03800_Fn02584(Task* arg0);
static inline void _actor03800TickAnim(Task* task);

static void Actor03800_Fn000B8(Enemy* arg0, Task* arg1)
{
    WorldCollisionBody*    obj;
    WorldCollisionContact* records1;
    WorldCollisionContact* records2;
    WorldCollisionContact* records3;
    _Actor03800Work*       work;
    s32                    i;
    TmdObject*             extra;

    extra = arg1->extra.tmd;
    work  = memCalloc(sizeof(_Actor03800Work), 0);
    if (work == NULL) {
        enemyDestroy(arg0, arg1);
        return;
    }
    arg1->work      = work;
    extra->lightMtx = &work->lightMtx;
    extra->flags    = 0;
    extra->colorMtx = &work->colorMtx;
    sceneAcquireBattleRef(0);
    Actor03800_Fn003B8(arg1);
    arg0->field_4  = &work->rootCoord->coord;
    arg0->field_48 = 0;
    worldTargetLinkNode(&arg0->node);
    arg0->coord                   = &arg1->extra.tmd->coords[3];
    arg0->node.state.parts.flags  = 0;
    arg0->bodyPos.vx              = 0;
    arg0->recs                    = work->hitContacts;
    arg0->bodyPos.vy              = 0;
    arg0->bodyPos.vz              = 0;
    arg0->param                   = &Actor03800_D05F44;
    arg0->hp                      = (s16)Actor03800_D05F44.hpMax;
    work->hitEffectArg.coord      = &arg1->extra.tmd->coords[3];
    work->hitEffectArg.spawnArgLo = 0x200;
    work->hitEffectArg.spawnArgHi = 1;
    animationInitContext(&work->rig.anim, Actor03800_D05F60, extra, work->rig.poses, work->rig.slots);
    for (i = 1; i < ARRAY_SIZE(work->rig.slots); i++) {
        animationResetSlot(&work->rig.anim, i, 1);
    }
    work->hitBody.coord            = arg1->extra.tmd->coords;
    records1                       = work->hitContacts;
    work->hitBody.pos.vy           = -0xFA;
    work->hitBody.context.contacts = records1;
    work->hitBody.pos.vx           = 0;
    work->hitBody.pos.vz           = 0;
    work->hitBody.key              = 0x30026;
    work->hitBody.radius           = 0xFA;
    work->hitBody.flags            = WORLD_COLLISION_BODY_SPHERE;
    worldCollisionLinkBody(WORLD_COLLISION_LIST_ENEMY_BODIES, &work->hitBody);
    worldCollisionInitContacts(records1, ARRAY_SIZE(work->hitContacts), 0);
    work->gridBody.coord            = arg1->extra.tmd->coords;
    records2                        = work->gridContacts;
    work->gridBody.pos.vy           = -0x12C;
    work->gridBody.context.contacts = records2;
    work->gridBody.pos.vx           = 0;
    work->gridBody.pos.vz           = 0;
    work->gridBody.key              = 0x30026;
    work->gridBody.radius           = 0x12C;
    work->gridBody.flags            = WORLD_COLLISION_BODY_SPHERE;
    worldCollisionLinkBody(WORLD_COLLISION_LIST_ENEMY_BODIES, &work->gridBody);
    worldCollisionInitContacts(records2, ARRAY_SIZE(work->gridContacts), 0);
    switch (work->mode) {
        case ACTOR_03800_MODE_FLOOR:
            work->hitBody.flags  |= WORLD_COLLISION_BODY_PAIR_ENABLED;
            work->gridBody.flags |= (WORLD_COLLISION_BODY_FLOOR_QUERY | WORLD_COLLISION_BODY_GRID_ENABLED);
            break;
        case ACTOR_03800_MODE_WALL:
            work->hitBody.flags  |= WORLD_COLLISION_BODY_PAIR_ENABLED;
            work->gridBody.flags &= ~(WORLD_COLLISION_BODY_FLOOR_QUERY | WORLD_COLLISION_BODY_GRID_ENABLED);
            break;
        case ACTOR_03800_MODE_CEILING:
            work->hitBody.flags  |= WORLD_COLLISION_BODY_PAIR_ENABLED;
            work->gridBody.flags &= ~(WORLD_COLLISION_BODY_FLOOR_QUERY | WORLD_COLLISION_BODY_GRID_ENABLED);
            break;
        case ACTOR_03800_MODE_SHRINE:
            work->hitBody.flags  &= ~WORLD_COLLISION_BODY_PAIR_ENABLED;
            work->gridBody.flags &= ~(WORLD_COLLISION_BODY_FLOOR_QUERY | WORLD_COLLISION_BODY_GRID_ENABLED);
            break;
    }
    obj                               = &work->attackBody;
    work->attackBody.coord            = arg1->extra.tmd->coords;
    records3                          = work->attackContacts;
    work->attackBody.pos.vy           = -0xFA;
    work->attackBody.pos.vz           = 0xFA;
    work->attackBody.radius           = 0xC8;
    work->attackBody.context.contacts = records3;
    work->attackBody.pos.vx           = 0;
    work->attackBody.key              = 0;
    work->attackBody.flags            = WORLD_COLLISION_BODY_SPHERE;
    worldCollisionLinkBody(WORLD_COLLISION_LIST_ENEMY_ATTACKS, obj);
    worldCollisionInitContacts(records3, ARRAY_SIZE(work->attackContacts), 0);
    work->attackBody.flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;
    arg1->state             = 1;
}

/// Applies the placement's mode (`AreaPlacement::mode`) to the freshly
/// allocated work block: the tens digit is `mode` and, on the floor, the units
/// digit the first action - idling after a random wait, or wandering.
/// The two perched modes hang the model instead: `perchCoord` takes the
/// root's transform, is parented to `gGfxViewCoord`, turned a quarter or half
/// turn about X and made `rootCoord`, while the model's root is reset to an
/// identity rotation at the origin and re-parented under it.
static void Actor03800_Fn003B8(Task* arg0)
{
    _Actor03800Work* work;
    Enemy*           ctx;
    GfxCoord*        src;
    GfxMatrix*       mtx;
    GfxMatrix*       srcmtx;
    GfxMatrix*       mtx2;
    GfxMatrix*       srcmtx2;
    SVECTOR          rot;
    MATRIX           mat;
    s16              mode;
    s16              kind;

    ctx  = (Enemy*)arg0->spawnArg2.pointer;
    work = arg0->work;
    src  = arg0->extra.tmd->coords;
    mode = ctx->place->mode / 10;

    work->mode = mode;
    switch (mode) {
        case ACTOR_03800_MODE_FLOOR:
            kind         = ctx->place->mode % 10;
            work->action = kind;
            switch (kind) {
                case ACTOR_03800_ACTION_IDLE:
                    work->anim      = ACTOR_03800_ANIM_STAND;
                    work->alerted   = 0;
                    gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                    work->timer     = ((gRandomLcgState >> 0x10) & 0xFF) + 0x5A;
                    break;
                case ACTOR_03800_ACTION_WANDER:
                    work->anim    = ACTOR_03800_ANIM_WALK;
                    work->timer   = 0;
                    work->alerted = 1;
                    break;
            }
            work->fallSpeed   = 0x80;
            work->shadowShade = 0x80;
            work->rootCoord   = arg0->extra.tmd->coords;
            break;
        case ACTOR_03800_MODE_WALL:
            work->action       = ACTOR_03800_ACTION_WALL_WAIT;
            work->shadowShade  = -1;
            work->fallSpeed    = 0;
            work->rootCoord    = &work->perchCoord;
            work->overturned   = 1;
            work->alerted      = 0;
            work->savedRootMtx = src->coord;

            mtx                       = (GfxMatrix*)&work->perchCoord.coord;
            mtx->rotationWords.m00M01 = ONE;
            mtx->rotationWords.m02M10 = 0;
            mtx->rotationWords.m11M12 = ONE;
            mtx->rotationWords.m20M21 = 0;
            mtx->rotationWords.m22    = ONE;

            work->perchCoord.parent     = &gGfxViewCoord;
            work->perchCoord.coord      = src->coord;
            work->perchCoord.coord.t[0] = src->coord.t[0];
            work->perchCoord.coord.t[1] = src->coord.t[1];
            work->perchCoord.coord.t[2] = src->coord.t[2];

            srcmtx                       = (GfxMatrix*)&src->coord;
            srcmtx->rotationWords.m00M01 = ONE;
            srcmtx->rotationWords.m02M10 = 0;
            srcmtx->rotationWords.m11M12 = ONE;
            srcmtx->rotationWords.m20M21 = 0;
            srcmtx->rotationWords.m22    = ONE;

            src->parent     = &work->perchCoord;
            src->coord.t[0] = 0;
            src->coord.t[1] = 0;
            src->coord.t[2] = 0;

            rot.vx = 0x400;
            rot.vy = 0;
            rot.vz = 0;
            RotMatrix(&rot, &mat);

            gte_SetRotMatrix(&work->perchCoord.coord);
            gte_ldclmv(&mat.m[0][0]);
            gte_rtir();
            gte_stclmv(&work->perchCoord.coord.m[0][0]);
            gte_ldclmv(&mat.m[0][1]);
            gte_rtir();
            gte_stclmv(&work->perchCoord.coord.m[0][1]);
            gte_ldclmv(&mat.m[0][2]);
            gte_rtir();
            gte_stclmv(&work->perchCoord.coord.m[0][2]);
            break;
        case ACTOR_03800_MODE_CEILING:
            work->action       = ACTOR_03800_ACTION_CEILING_WAIT;
            work->shadowShade  = -1;
            work->fallSpeed    = 0;
            work->rootCoord    = &work->perchCoord;
            work->overturned   = 1;
            work->alerted      = 0;
            work->savedRootMtx = src->coord;

            mtx2                       = (GfxMatrix*)&work->perchCoord.coord;
            mtx2->rotationWords.m00M01 = ONE;
            mtx2->rotationWords.m02M10 = 0;
            mtx2->rotationWords.m11M12 = ONE;
            mtx2->rotationWords.m20M21 = 0;
            mtx2->rotationWords.m22    = ONE;

            work->perchCoord.parent     = &gGfxViewCoord;
            work->perchCoord.coord      = src->coord;
            work->perchCoord.coord.t[0] = src->coord.t[0];
            work->perchCoord.coord.t[1] = src->coord.t[1];
            work->perchCoord.coord.t[2] = src->coord.t[2];

            srcmtx2                       = (GfxMatrix*)&src->coord;
            srcmtx2->rotationWords.m00M01 = ONE;
            srcmtx2->rotationWords.m02M10 = 0;
            srcmtx2->rotationWords.m11M12 = ONE;
            srcmtx2->rotationWords.m20M21 = 0;
            srcmtx2->rotationWords.m22    = ONE;

            src->parent     = &work->perchCoord;
            src->coord.t[0] = 0;
            src->coord.t[1] = 0;
            src->coord.t[2] = 0;

            rot.vx = 0x800;
            rot.vy = 0;
            rot.vz = 0;
            RotMatrix(&rot, &mat);

            gte_SetRotMatrix(&work->perchCoord.coord);
            gte_ldclmv(&mat.m[0][0]);
            gte_rtir();
            gte_stclmv(&work->perchCoord.coord.m[0][0]);
            gte_ldclmv(&mat.m[0][1]);
            gte_rtir();
            gte_stclmv(&work->perchCoord.coord.m[0][1]);
            gte_ldclmv(&mat.m[0][2]);
            gte_rtir();
            gte_stclmv(&work->perchCoord.coord.m[0][2]);
            break;
        case ACTOR_03800_MODE_SHRINE:
            work->action      = ACTOR_03800_ACTION_SHRINE_WAIT;
            work->fallSpeed   = 0;
            work->shadowShade = -1;
            work->rootCoord   = arg0->extra.tmd->coords;
            break;
    }
}

static void Actor03800_Fn00974(Task* arg0)
{
    Enemy*           ctx;
    _Actor03800Work* work;
    s16              damage;
    u16              remaining;
    u8               flags;

    ctx   = arg0->spawnArg2.pointer;
    flags = ctx->reactionFlags;
    work  = arg0->work;
    if (flags & ENEMY_REACTION_BUILDUP) {
        if (work->mode == ACTOR_03800_MODE_FLOOR) {
            ctx->reactionFlags = flags & ENEMY_REACTION_BUILDUP_CLEAR;
            work->action       = ACTOR_03800_ACTION_BUILDUP;
            work->actionStep   = 0;
            work->timer        = 0;
            work->inBuildup    = 1;
        } else if ((work->action != ACTOR_03800_ACTION_DROP) || (work->actionStep >= 4)) {
            work->action     = ACTOR_03800_ACTION_DROP;
            work->actionStep = 0;
        }
    }
    if (ctx->reactionFlags & ENEMY_REACTION_DAMAGE_OVER_TIME_BITS) {
        damage = Gp_TickObjFlag4(ctx);
        if (damage != 0) {
            func_800DA6E8(&ctx->node, (s32)damage, 0);
            remaining = ctx->hp - damage;
            ctx->hp   = remaining;
            if ((s16)remaining <= 0) {
                work->action = ACTOR_03800_ACTION_DIE;
            } else {
                work->action = ACTOR_03800_ACTION_FLINCH;
            }
            work->actionStep = 0;
        }
        if (Gp_ObjFlag4Expired(ctx) != 0) {
            ctx->reactionFlags &= ENEMY_REACTION_DAMAGE_OVER_TIME_CLEAR;
        }
    }
}

/// Keeps the deepest contact seen so far: when `depth` beats `best`, it
/// becomes the new `best`, and `frame->pushDirection` the direction to push
/// out along - the offset in `frame->delta` normalised and carried into the
/// collision grid's frame.
#define _ACTOR03800_KEEP_DEEPEST(best, depth, frame)                                                             \
    do {                                                                                                         \
        if ((best) < (depth)) {                                                                                  \
            (best) = (depth);                                                                                    \
            VectorNormal(&(frame)->delta.vector, &(frame)->normal);                                              \
            ApplyTransposeMatrixLV(&Gp_GridParams->viewCoord->workm, &(frame)->normal, &(frame)->pushDirection); \
        }                                                                                                        \
    } while (0)

static void Actor03800_Fn00A98(Task* arg0)
{
    _Actor03800Work*         work;
    ActorOverlapPushScratch* frame;
    Enemy*                   ctx;
    GfxCoord*                coord;
    GfxCoord*                sourceCoord;
    s32                      push;
    s32                      reaction;
    s32                      result;
    s32                      i;
    s32                      depth;
    s32                      boundedDepth;
    s32                      dx;
    s32                      dy;
    s32                      dz;
    u32                      lastId;
    u32                      id;
    u32                      hitId;
    u32                      damage;

    push     = 0;
    reaction = 0;
    lastId   = 0;
    work     = arg0->work;
    SCRATCH_STACK_RESERVE_BLOCK(ActorOverlapPushScratch);
    frame  = SCRATCH_STACK_CURSOR(ActorOverlapPushScratch);
    coord  = work->rootCoord;
    ctx    = arg0->spawnArg2.pointer;
    result = func_800E0C10(work->gridContacts, &frame->delta, ARRAY_SIZE(work->gridContacts), NULL);
    if (result != 0) {
        for (i = 0; i < ARRAY_SIZE(work->gridContacts); i++) {
            if ((work->gridContacts[i].key.value & WORLD_COLLISION_CONTACT_KIND_MASK) == WORLD_COLLISION_CONTACT_GRID) {
                if (work->gridContacts[i].response.direction.vy >= -0xDDA) {
                    if (work->overturned == 0) {
                        work->hitWall = 1;
                        break;
                    }
                } else {
                    work->landed = 1;
                }
            }
        }
        switch (result) {
            case 0:
                break;
            case 1:
                coord->coord.t[0] += frame->delta.fixed.vx.halves.integer;
                coord->coord.t[1] += frame->delta.fixed.vy.halves.integer;
                coord->coord.t[2] += frame->delta.fixed.vz.halves.integer;
                break;
            case 2:
                coord->coord.t[0] = work->prevPos.vx;
                coord->coord.t[1] = work->prevPos.vy;
                coord->coord.t[2] = work->prevPos.vz;
                break;
        }
    }
    worldCollisionClearContacts(work->gridContacts);
    if (work->hitCooldown != 0) {
        if (--work->hitCooldown <= 0) {
            work->hitCooldown = 0;
        }
    }
    for (i = 0; i < ARRAY_SIZE(work->hitContacts); i++) {
        id = work->hitContacts[i].key.value;
        switch (id >> 0x10) {
            case 0:
                break;
            case 2:
                if (work->hitCooldown == 0) {
                    sourceCoord            = gPlayerActorTasks[(id >> 7) & 1]->extra.tmd->coords;
                    frame->delta.vector.vx = sourceCoord->coord.t[0] - coord->coord.t[0];
                    frame->delta.vector.vy = sourceCoord->coord.t[1] - coord->coord.t[1];
                    frame->delta.vector.vz = sourceCoord->coord.t[2] - coord->coord.t[2];
                    damage                 = Gp_ComputeDamage(work->hitContacts[i].key.value, SquareRoot0((frame->delta.vector.vx * frame->delta.vector.vx) + (frame->delta.vector.vy * frame->delta.vector.vy) + (frame->delta.vector.vz * frame->delta.vector.vz)), 0, 0);
                    if (work->overturned == 0) {
                        if (Gp_RollEnemyChance(ctx, work->hitContacts[i].key.value, 0) != 0) {
                            damage *= 4;
                            Gp_SpawnEff(EFFECT_CRITICAL_HIT, coord, 0, NULL);
                        }
                    } else if (!(work->hitContacts[i].key.value & 0x8000) && (damage != 0)) {
                        damage *= 3;
                        Gp_SpawnEff(EFFECT_CRITICAL_HIT, coord, 4, NULL);
                    }
                    func_800DA6E8(&ctx->node, damage, 0);
                    func_800E2C78(ctx, work->hitContacts[i].key.value, damage, 0);
                    ctx->hp -= damage;
                    if (ctx->hp <= 0) {
                        reaction = 2;
                    }
                    switch (Gp_GetIdParam0(work->hitContacts[i].key.value) & 0xFFFF) {
                        case 0:
                        default:
                            break;
                        case 3:
                            Gp_SetObjFlag4(ctx, work->hitContacts[i].key.value, 0);
                            break;
                        case 4:
                            if (ctx->hp > 0) {
                                if (work->overturned == 0) {
                                    reaction = 1;
                                }
                            } else {
                                work->burstStage = 1;
                            }
                            break;
                        case 6:
                            if (ctx->hp <= 0) {
                                work->burstStage = 1;
                            } else if (work->overturned == 0) {
                                reaction = 1;
                            }
                            break;
                        case 8:
                            if (work->overturned == 0 && reaction == 0) {
                                Gp_SetObjFlag2(ctx, work->hitContacts[i].key.value, 0);
                            }
                            break;
                        case 1:
                        case 2:
                        case 5:
                        case 9:
                            if (work->overturned == 0 && reaction == 0) {
                                reaction = 1;
                            }
                            break;
                    }
                    switch (reaction) {
                        case 0:
                            if (work->action != ACTOR_03800_ACTION_DROP && damage != 0) {
                                work->action     = ACTOR_03800_ACTION_FLINCH;
                                work->actionStep = 0;
                            }
                            break;
                        case 1:
                            work->action     = ACTOR_03800_ACTION_KNOCKED_OVER;
                            work->actionStep = 0;
                            break;
                        case 2:
                            work->action     = ACTOR_03800_ACTION_DIE;
                            work->actionStep = 0;
                            break;
                    }
                    hitId = work->hitContacts[i].key.value;
                    if (lastId != hitId) {
                        lastId = hitId;
                        func_800FDB18(Gp_GetIdParam1(lastId) & 0xFFFF, arg0->extra.tmd->coords + 3, NULL, &work->hitEffectArg);
                    }
                    result = Gp_GetIdParam2(work->hitContacts[i].key.value);
                    if (result > 0) {
                        work->hitCooldown = result;
                    }
                }
                break;
            case 1:
                dx                     = coord->workm.t[0] - work->hitContacts[i].point.vx;
                frame->delta.vector.vx = dx;
                dy                     = coord->workm.t[1] - work->hitContacts[i].point.vy;
                frame->delta.vector.vy = dy;
                dz                     = coord->workm.t[2] - work->hitContacts[i].point.vz;
                frame->delta.vector.vz = dz;
                depth                  = work->hitContacts[i].distance - SquareRoot0((dx * dx) + (dy * dy) + (dz * dz));
                boundedDepth           = depth;
                if (depth <= 0) {
                    boundedDepth = 0;
                }
                depth = boundedDepth;
                _ACTOR03800_KEEP_DEEPEST(push, depth, frame);
                break;
            case 3:
                dx                     = coord->workm.t[0] - work->hitContacts[i].point.vx;
                frame->delta.vector.vx = dx;
                dy                     = coord->workm.t[1] - work->hitContacts[i].point.vy;
                frame->delta.vector.vy = dy;
                dz                     = coord->workm.t[2] - work->hitContacts[i].point.vz;
                frame->delta.vector.vz = dz;
                depth                  = work->hitContacts[i].distance - SquareRoot0((dx * dx) + (dy * dy) + (dz * dz));
                boundedDepth           = depth;
                if (depth <= 0) {
                    boundedDepth = 0;
                }
                depth = boundedDepth;
                _ACTOR03800_KEEP_DEEPEST(push, depth, frame);
                break;
        }
    }
    if (push > 0 && work->mode == ACTOR_03800_MODE_FLOOR) {
        coord->coord.t[0] += (push * frame->pushDirection.vx) >> 0xC;
        coord->coord.t[2] += (push * frame->pushDirection.vz) >> 0xC;
    }
    worldCollisionClearContacts(work->hitContacts);
    work->attackTouched = 0;
    result              = worldCollisionCountContactsByKind(work->attackContacts, WORLD_COLLISION_CONTACT_PLAYER_BODY);
    if (result != 0) {
        Gp_ArmStateF0(1);
        work->attackTouched = 1;
        work->alerted       = 1;
        if ((work->mode == ACTOR_03800_MODE_FLOOR) && (work->overturned == 0) && (work->action != ACTOR_03800_ACTION_LEAP_AWAY)) {
            work->action     = ACTOR_03800_ACTION_LEAP_AWAY;
            work->actionStep = 0;
        }
    }
    worldCollisionClearContacts(work->attackContacts);
    SCRATCH_STACK_RELEASE_BLOCK(ActorOverlapPushScratch);
}

static void Actor03800_Fn01150(Task* arg0)
{
    _Actor03800Work* work;
    s32              turn;

    work = arg0->work;

    switch (work->actionStep) {
        case 0:
            work->turnRate = 0;
            work->speed    = 0;
            work->accel    = 0;
            work->timer--;
            if (work->timer <= 0) {
                work->actionStep = 1;
                gRandomLcgState  = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                turn             = (gRandomLcgState >> 16) & 0x3FF;
                if (((gRandomLcgState >> 16) & 0x400) == 0) {
                    turn = -turn;
                }
                work->anim           = ACTOR_03800_ANIM_WALK;
                work->stepSoundTimer = 1;
                work->targetYaw      = (work->yaw + turn) & 0xFFF;
            }
            break;
        case 1:
            work->turnRate = 0x1E;
            work->speed    = 0;
            work->accel    = 0;
            if (work->yaw == work->targetYaw) {
                if (work->alerted == 0) {
                    work->actionStep     = 0;
                    work->anim           = ACTOR_03800_ANIM_STAND;
                    work->stepSoundTimer = 0;
                    gRandomLcgState      = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                    work->timer          = ((gRandomLcgState >> 16) & 0xFF) + 0x5A;
                } else {
                    work->action     = ACTOR_03800_ACTION_WANDER;
                    work->actionStep = 0;
                    if (work->stepSoundTimer == 0) {
                        work->stepSoundTimer = 1;
                    }
                }
            }
            break;
    }

    if (gSceneCombatState.signals.bytes.actionFlags & (SCENE_COMBAT_ACTION_NOISE | SCENE_COMBAT_ACTION_PE_CAST_OTHER)) {
        work->action     = ACTOR_03800_ACTION_CHARGE;
        work->actionStep = 0;
        if (work->stepSoundTimer == 0) {
            work->stepSoundTimer = 1;
        }
        work->alerted = 1;
    }
}

static void Actor03800_Fn012B4(Task* arg0)
{
    _Actor03800Work* work;
    s32              turn;
    s32              delta;
    s32              turn2;

    work = arg0->work;

    switch (work->actionStep) {
        case 0:
            work->turnRate  = 0;
            work->accel     = 2;
            gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            turn            = (gRandomLcgState >> 16) & 0x1FF;
            if (((gRandomLcgState >> 16) & 0x400) == 0) {
                turn = -turn;
            }
            delta = turn;
            if (work->hitWall != 0) {
                delta         = turn + 0x800;
                work->hitWall = 0;
            }
            work->anim        = ACTOR_03800_ANIM_WALK;
            work->actionStep  = 1;
            work->targetYaw   = (work->yaw + delta) & 0xFFF;
            turn              = 0; /* dead store: keeps `turn` cse-canonical over `delta` */
            gRandomLcgState   = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            work->timer       = ((gRandomLcgState >> 16) & 0xF) + 0x19;
            gRandomLcgState   = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            work->wanderTimer = ((gRandomLcgState >> 16) & 0x1F) + 0x1E;
            break;
        case 1:
            work->turnRate = 0x1E;
            work->accel    = 2;
            work->timer--;
            if (work->timer <= 0) {
                gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                work->timer     = ((gRandomLcgState >> 16) & 0xF) + 0x19;
                gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                turn2           = (gRandomLcgState >> 16) & 0x1FF;
                if (((gRandomLcgState >> 16) & 0x400) == 0) {
                    turn2 = -turn2;
                }
                work->targetYaw = (work->yaw + turn2) & 0xFFF;
            }
            if (work->hitWall != 0) {
                if (work->speed < 0x1E) {
                    work->action         = ACTOR_03800_ACTION_IDLE;
                    work->actionStep     = 0;
                    work->anim           = ACTOR_03800_ANIM_STAND;
                    work->stepSoundTimer = 0;
                    gRandomLcgState      = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                    work->timer          = ((gRandomLcgState >> 16) & 0x1F) + 0x1E;
                } else {
                    work->action     = ACTOR_03800_ACTION_LEAP_AWAY;
                    work->actionStep = 0;
                }
            }
            work->wanderTimer--;
            if (work->wanderTimer <= 0) {
                work->action         = ACTOR_03800_ACTION_IDLE;
                work->actionStep     = 0;
                work->anim           = ACTOR_03800_ANIM_STAND;
                work->stepSoundTimer = 0;
                gRandomLcgState      = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                work->timer          = ((gRandomLcgState >> 16) & 0x1F) + 0x1E;
            }
            break;
    }

    if (gSceneCombatState.signals.bytes.actionFlags & (SCENE_COMBAT_ACTION_NOISE | SCENE_COMBAT_ACTION_PE_CAST_OTHER)) {
        work->action     = ACTOR_03800_ACTION_CHARGE;
        work->actionStep = 0;
    }
}

static void Actor03800_Fn01520(Task* arg0)
{
    _Actor03800Work* work;
    GfxCoord*        coord;
    VECTOR           vec;

    work  = arg0->work;
    coord = work->rootCoord;

    switch (work->actionStep) {
        case 0:
            work->turnRate  = 0;
            work->speed     = 0;
            work->accel     = 0;
            vec.vx          = gPlayerStatus.coordMtx->t[0] - coord->coord.t[0];
            vec.vy          = 0;
            vec.vz          = gPlayerStatus.coordMtx->t[2] - coord->coord.t[2];
            work->targetYaw = ratan2((s16)vec.vx, (s16)vec.vz) & 0xFFF;
            work->anim      = ACTOR_03800_ANIM_CHARGE;
            if (work->stepSoundTimer == 0) {
                work->stepSoundTimer = 1;
            }
            work->actionStep = 1;
            break;
        case 1:
            work->turnRate = 0x28;
            work->speed    = 0;
            work->accel    = 0;
            if (work->yaw == work->targetYaw) {
                work->actionStep = 2;
                gRandomLcgState  = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                work->timer      = ((gRandomLcgState >> 16) & 0x1F) + 0x78;
            }
            break;
        case 2:
            work->turnRate = 0;
            work->accel    = 2;
            work->timer--;
            if (work->timer <= 0) {
                work->action     = ACTOR_03800_ACTION_WANDER;
                work->actionStep = 0;
            }
            break;
    }
}

static void Actor03800_Fn0166C(Task* arg0)
{
    _Actor03800KnockedOverScratch* scratch;
    _Actor03800Work*               work;
    Enemy*                         ctx;
    GfxCoord*                      coord;
    s16                            state;
    s32                            snd;
    s32                            pan;
    s32                            pan2;

    scratch = SCRATCH_STACK_RESERVE_BLOCK(_Actor03800KnockedOverScratch);
    work    = arg0->work;
    ctx     = arg0->spawnArg2.pointer;
    state   = work->actionStep;
    coord   = work->rootCoord;
    switch (state) {
        case 0:
            work->anim              = ACTOR_03800_ANIM_KNOCKED_OVER;
            work->actionStep        = 1;
            work->stepSoundTimer    = 0;
            work->overturned        = 1;
            work->attackBody.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
            snd                     = ((((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x40260003;
            pan                     = (s8)worldCoordGetOriginAudioPan(coord);
            sndEvtRequestScriptStart(snd, pan, (s8)worldCoordGetOriginAudioDepth(coord));
            break;
        case 1:
            if (work->animFrame >= 2 && work->animFrame < 14) {
                // Slide along the ground, straight away from the player.
                scratch->fromPlayer.vx = coord->coord.t[0] - gPlayerStatus.coordMtx->t[0];
                scratch->fromPlayer.vy = coord->coord.t[1] - gPlayerStatus.coordMtx->t[1];
                scratch->fromPlayer.vz = coord->coord.t[2] - gPlayerStatus.coordMtx->t[2];
                VectorNormalS(&scratch->fromPlayer, &scratch->direction);
                coord->coord.t[0] += (scratch->direction.vx * 17) >> 9;
                coord->coord.t[2] += (scratch->direction.vz * 17) >> 9;
            } else {
                work->speed = 0;
                work->accel = 0;
            }
            if (work->animFrame == 12) {
                snd  = ((((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x40260002;
                pan2 = (s8)worldCoordGetOriginAudioPan(coord);
                sndEvtRequestScriptStart(snd, pan2, (s8)worldCoordGetOriginAudioDepth(coord));
            }
            if (work->animFrame >= 29) {
                work->actionStep      = 2;
                work->overturnedTimer = ((Actor03800_D05F44.hpMax - ctx->hp) * 100 / Actor03800_D05F44.hpMax) * 10 + 240;
            }
            break;
        case 2:
            work->overturnedTimer--;
            if (work->overturnedTimer <= 0) {
                work->action     = ACTOR_03800_ACTION_GET_UP;
                work->actionStep = 0;
            }
            break;
    }
    SCRATCH_STACK_RELEASE_BLOCK(_Actor03800KnockedOverScratch);
}

static void Actor03800_Fn01948(Task* arg0)
{
    _Actor03800Work* work = arg0->work;
    GfxCoord*        coord;
    s16              state;
    s32              snd;
    s32              pan;

    state = work->actionStep;
    coord = work->rootCoord;
    switch (state) {
        case 0:
            if (work->mode == ACTOR_03800_MODE_FLOOR) {
                if (work->overturned == 0) {
                    work->anim  = ACTOR_03800_ANIM_FLINCH;
                    work->timer = 0xC;
                } else {
                    work->anim  = ACTOR_03800_ANIM_FLINCH_OVERTURNED;
                    work->timer = 0x13;
                }
            } else {
                work->anim   = ACTOR_03800_ANIM_FLINCH_PERCHED;
                work->timer  = 0;
                work->action = ACTOR_03800_ACTION_DROP;
            }
            work->actionStep     = 1;
            work->playingAnim    = ACTOR_03800_ANIM_STAND; // not the animation asked for, so it starts afresh
            work->stepSoundTimer = 0;
            work->speed          = 0;
            work->accel          = 0;
            work->turnRate       = 0;
            snd                  = ((((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x40260003;
            pan                  = (s8)worldCoordGetOriginAudioPan(coord);
            sndEvtRequestScriptStart(snd, pan, (s8)worldCoordGetOriginAudioDepth(coord));
            break;
        case 1:
            work->timer -= 1;
            if (work->timer > 0) {
                break;
            }
            if (work->overturned == 0) {
                if (work->inBuildup == 0) {
                    work->action     = ACTOR_03800_ACTION_CHARGE;
                    work->actionStep = 0;
                } else {
                    work->action     = ACTOR_03800_ACTION_BUILDUP;
                    work->actionStep = 0;
                    work->timer      = 0;
                }
                if (work->stepSoundTimer == 0) {
                    work->stepSoundTimer = 1;
                }
            } else {
                work->action     = ACTOR_03800_ACTION_KNOCKED_OVER;
                work->actionStep = 2;
            }
            break;
    }
}

static void Actor03800_Fn01AD0(Task* arg0)
{
    Enemy*           ctx;
    _Actor03800Work* work;

    work           = arg0->work;
    ctx            = arg0->spawnArg2.pointer;
    work->turnRate = 0;
    work->speed    = 0;
    work->accel    = 0;
    work->timer--;
    if (work->timer <= 0) {
        if (work->overturned == 0) {
            work->anim = ACTOR_03800_ANIM_FLINCH;
        } else {
            work->anim = ACTOR_03800_ANIM_FLINCH_OVERTURNED;
        }
        work->playingAnim = ACTOR_03800_ANIM_STAND; // not the animation asked for, so it starts afresh
        gRandomLcgState   = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
        work->timer       = ((gRandomLcgState >> 16) & 7) + 3;
    }
    if (Gp_TickObjFlag2(arg0->spawnArg2.pointer) != 0) {
        work->inBuildup = 0;
        if (work->mode == ACTOR_03800_MODE_FLOOR) {
            if (work->overturned == 0) {
                work->action     = ACTOR_03800_ACTION_CHARGE;
                work->actionStep = 0;
                if (work->stepSoundTimer == 0) {
                    work->stepSoundTimer = 1;
                }
            } else {
                work->action          = ACTOR_03800_ACTION_KNOCKED_OVER;
                work->actionStep      = 2;
                work->overturnedTimer = ((Actor03800_D05F44.hpMax - ctx->hp) * 100 / Actor03800_D05F44.hpMax) * 10 + 240;
            }
        } else {
            work->action     = ACTOR_03800_ACTION_DROP;
            work->actionStep = 0;
        }
    }
}

static void Actor03800_Fn01C50(Task* arg0)
{
    SVECTOR          rotation;
    MATRIX           matrix;
    _Actor03800Work* work;
    GfxCoord*        coord;
    s16              state;

    work  = arg0->work;
    state = work->actionStep;
    coord = arg0->extra.tmd->coords;
    switch (state) {
        case 0:
            if (work->mode == ACTOR_03800_MODE_FLOOR) {
                arg0->state      = 2;
                work->actionStep = 0;
                return;
            }
            work->fallSpeed       = 0x80;
            work->shadowShade     = 0x80;
            work->actionStep      = 1;
            work->gridBody.flags |= (WORLD_COLLISION_BODY_FLOOR_QUERY | WORLD_COLLISION_BODY_GRID_ENABLED);
            return;
        case 1:
            if (work->landed != 0) {
                work->actionStep = 2;
                return;
            }
        default:
            return;
        case 2:
            work->anim       = ACTOR_03800_ANIM_DIE_OVERTURNED;
            work->actionStep = 3;
            return;
        case 3:
            rotation.vx = 0;
            rotation.vy = work->yaw;
            rotation.vz = 0;
            RotMatrix(&rotation, &matrix);
            gte_SetRotMatrix(&work->savedRootMtx);
            gte_ldclmv(&matrix.m[0][0]);
            gte_rtir();
            gte_stclmv(&work->savedRootMtx.m[0][0]);
            gte_ldclmv(&matrix.m[0][1]);
            gte_rtir();
            gte_stclmv(&work->savedRootMtx.m[0][1]);
            gte_ldclmv(&matrix.m[0][2]);
            gte_rtir();
            gte_stclmv(&work->savedRootMtx.m[0][2]);
            coord->parent       = &gGfxViewCoord;
            coord->coord        = work->savedRootMtx;
            coord->coord.t[0]   = work->perchCoord.coord.t[0];
            coord->coord.t[1]   = work->perchCoord.coord.t[1];
            coord->coord.t[2]   = work->perchCoord.coord.t[2];
            coord->composeStamp = GRAPHICS_COORD_DIRTY;
            actorRenderComposeCoord(coord);
            work->timer      = 0xF;
            work->rootCoord  = coord;
            work->actionStep = 4;
            return;
        case 4:
            work->mode = ACTOR_03800_MODE_FLOOR;
            work->timer--;
            if (work->timer <= 0) {
                work->deathAnimChosen = 1;
                work->actionStep      = 0;
                arg0->state           = 2;
            }
            break;
    }
}

/// `ACTOR_03800_ACTION_WALL_WAIT`: the same body as `Actor03800_Fn02068`,
/// which the overlay carries twice.
static void Actor03800_Fn01EEC(Task* arg0)
{
    _Actor03800Work* work;
    s32              rand;
    s32              delta;

    work = arg0->work;

    switch (work->actionStep) {
        case 0:
            work->turnRate = 0;
            work->speed    = 0;
            work->accel    = 0;
            work->timer--;
            if (work->timer > 0) {
                break;
            }

            gRandomLcgState  = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            work->actionStep = 1;
            rand             = gRandomLcgState >> 16;
            delta            = rand & 0x3FF;
            if (!(rand & 0x400)) {
                delta = -delta;
            }

            work->anim           = ACTOR_03800_ANIM_WALK;
            work->stepSoundTimer = 1;
            work->targetYaw      = (work->yaw + delta) & 0xFFF;
            break;

        case 1:
            work->turnRate = 0x1E;
            work->speed    = 0;
            work->accel    = 0;
            if (work->yaw == work->targetYaw) {
                gRandomLcgState      = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                work->actionStep     = 0;
                work->anim           = ACTOR_03800_ANIM_STAND;
                work->stepSoundTimer = 0;
                work->timer          = (gRandomLcgState >> 16 & 0xFF) + 0x5A;
            }
            break;
    }

    if ((gSceneCombatState.signals.bytes.actionFlags & (SCENE_COMBAT_ACTION_NOISE | SCENE_COMBAT_ACTION_PE_CAST_OTHER)) || work->attackTouched != 0) {
        gRandomLcgState  = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
        work->action     = ACTOR_03800_ACTION_DROP;
        work->actionStep = 0;
        work->alerted    = 1;
        work->turnRate   = 0;
        work->speed      = 0;
        work->accel      = 0;
        work->timer      = (gRandomLcgState >> 16 & 0xF) + 0xF;
    }
}

/// `ACTOR_03800_ACTION_CEILING_WAIT`: the perched look-around. Step 0 counts
/// `timer` down and then picks a `targetYaw` within 0x3FF of `yaw`; step 1
/// waits for the turn to finish and re-arms the countdown. A noise, a cast of
/// the other kind or `attackTouched` ends the wait: the actor is alerted and
/// drops after 15 to 30 ticks.
static void Actor03800_Fn02068(Task* arg0)
{
    _Actor03800Work* work;
    s32              rand;
    s32              delta;

    work = arg0->work;

    switch (work->actionStep) {
        case 0:
            work->turnRate = 0;
            work->speed    = 0;
            work->accel    = 0;
            work->timer--;
            if (work->timer > 0) {
                break;
            }

            gRandomLcgState  = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            work->actionStep = 1;
            rand             = gRandomLcgState >> 16;
            delta            = rand & 0x3FF;
            if (!(rand & 0x400)) {
                delta = -delta;
            }

            work->anim           = ACTOR_03800_ANIM_WALK;
            work->stepSoundTimer = 1;
            work->targetYaw      = (work->yaw + delta) & 0xFFF;
            break;

        case 1:
            work->turnRate = 0x1E;
            work->speed    = 0;
            work->accel    = 0;
            if (work->yaw == work->targetYaw) {
                gRandomLcgState      = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                work->actionStep     = 0;
                work->anim           = ACTOR_03800_ANIM_STAND;
                work->stepSoundTimer = 0;
                work->timer          = (gRandomLcgState >> 16 & 0xFF) + 0x5A;
            }
            break;
    }

    if ((gSceneCombatState.signals.bytes.actionFlags & (SCENE_COMBAT_ACTION_NOISE | SCENE_COMBAT_ACTION_PE_CAST_OTHER)) || work->attackTouched != 0) {
        gRandomLcgState  = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
        work->action     = ACTOR_03800_ACTION_DROP;
        work->actionStep = 0;
        work->alerted    = 1;
        work->turnRate   = 0;
        work->speed      = 0;
        work->accel      = 0;
        work->timer      = (gRandomLcgState >> 16 & 0xF) + 0xF;
    }
}

static void Actor03800_Fn021E4(Task* arg0)
{
    Enemy*                 ctx;
    _Actor03800Work*       work;
    GfxCoord*              coord;
    ActorEulerTurnScratch* scratch;
    s32                    sound;
    s32                    pan;

    scratch = (ActorEulerTurnScratch*)SCRATCH_STACK_RESERVE_BYTES(sizeof(*scratch));
    work    = arg0->work;
    coord   = arg0->extra.tmd->coords;
    ctx     = arg0->spawnArg2.pointer;
    switch (work->actionStep) {
        case 0:
            work->timer--;
            if (work->timer <= 0) {
                work->actionStep = 1;
            }
            break;
        case 1:
            work->fallSpeed       = 0x100;
            work->shadowShade     = 0x80;
            work->gridBody.flags |= (WORLD_COLLISION_BODY_FLOOR_QUERY | WORLD_COLLISION_BODY_GRID_ENABLED);
            if (work->landed != 0) {
                work->landed            = 0;
                work->actionStep        = 2;
                work->fallSpeed         = 0x80;
                work->attackBody.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
                sound                   = ((((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x40260002;
                pan                     = (s8)worldCoordGetOriginAudioPan(coord);
                sndEvtRequestScriptStart(sound, (s32)pan, (s32)(s8)worldCoordGetOriginAudioDepth(coord));
            }
            break;
        case 2:
            work->anim       = ACTOR_03800_ANIM_LAND;
            work->actionStep = 3;
            break;
        case 3:
            scratch->angles.vx = 0;
            scratch->angles.vy = work->yaw;
            scratch->angles.vz = 0;
            RotMatrix(&scratch->angles, &scratch->rotation);
            gte_SetRotMatrix(&work->savedRootMtx);
            gte_ldclmv(&scratch->rotation.m[0][0]);
            gte_rtir();
            gte_stclmv(&work->savedRootMtx.m[0][0]);
            gte_ldclmv(&scratch->rotation.m[0][1]);
            gte_rtir();
            gte_stclmv(&work->savedRootMtx.m[0][1]);
            gte_ldclmv(&scratch->rotation.m[0][2]);
            gte_rtir();
            gte_stclmv(&work->savedRootMtx.m[0][2]);
            coord->coord        = work->savedRootMtx;
            coord->coord.t[0]   = work->perchCoord.coord.t[0];
            coord->coord.t[1]   = work->perchCoord.coord.t[1];
            coord->coord.t[2]   = work->perchCoord.coord.t[2];
            coord->parent       = &gGfxViewCoord;
            coord->composeStamp = GRAPHICS_COORD_DIRTY;
            actorRenderComposeCoord(coord);
            work->actionStep      = 4;
            work->rootCoord       = coord;
            work->overturnedTimer = ((Actor03800_D05F44.hpMax - ctx->hp) * 100 / Actor03800_D05F44.hpMax) * 10 + 240;
            break;
        case 4:
            work->mode = ACTOR_03800_MODE_FLOOR;
            work->timer--;
            if (work->timer <= 0) {
                work->action     = ACTOR_03800_ACTION_GET_UP;
                work->actionStep = 0;
            }
            break;
    }
    SCRATCH_STACK_RELEASE_BYTES(0x28);
}

/// `ACTOR_03800_ACTION_LEAP_AWAY`: a jump along the facing, away from the
/// player. Step 0 starts the leap animation and picks the direction: backward
/// (step 1) when the player is ahead of the actor or `hitWall` is set, forward
/// (step 2) otherwise. Those steps hold `speed` at -/+125 while `animFrame`
/// is 8 to 16 and stop the actor outside that, moving to step 3 from frame
/// 17; step 3 returns to wandering at frame 20. `attackBody` is off for the
/// whole leap.
static void Actor03800_Fn02584(Task* arg0)
{
    _Actor03800Work* work;
    GfxCoord*        coord;
    VECTOR           vec;

    work  = arg0->work;
    coord = work->rootCoord;

    switch (work->actionStep) {
        case 0:
            vec.vx     = gPlayerStatus.coordMtx->t[0] - coord->coord.t[0];
            vec.vy     = 0;
            vec.vz     = gPlayerStatus.coordMtx->t[2] - coord->coord.t[2];
            work->anim = ACTOR_03800_ANIM_LEAP;
            if (work->hitWall != 0) {
                work->hitWall    = 0;
                work->actionStep = 1;
            } else {
                work->actionStep =
                    ((vec.vx * coord->coord.m[0][2]) + (vec.vz * coord->coord.m[2][2]) > 0) ? 1 : 2;
            }
            work->attackBody.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
            break;

        case 1:
            if (work->animFrame >= 8 && work->animFrame <= 0x10) {
                work->speed = -0x7D;
                break;
            }
            work->speed = 0;
            if (work->animFrame >= 0x11) {
                work->actionStep = 3;
            }
            break;

        case 2:
            if (work->animFrame >= 8 && work->animFrame <= 0x10) {
                work->speed = 0x7D;
                break;
            }
            work->speed = 0;
            if (work->animFrame >= 0x11) {
                work->actionStep = 3;
            }
            break;

        case 3:
            if (work->animFrame >= 0x14) {
                work->action            = ACTOR_03800_ACTION_WANDER;
                work->actionStep        = 0;
                work->shadowShade       = 0x80;
                work->attackBody.flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;
            }
            break;
    }
}

static void Actor03800_Fn026F8(Task* arg0)
{
    _Actor03800Work* work;
    GfxCoord*        coord;
    SVECTOR*         rot;
    s32              ang;
    u16              want;
    s16              diff;
    s32              adiff;
    s32              step;
    s32              cur;
    s32              next;
    s32              wrapStep;

    rot   = (SVECTOR*)SCRATCH_STACK_RESERVE_BYTES(8);
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
        if (diff > 0 ? step >= 0x1000 - diff : step >= 0x1000 + diff) {
            work->yaw = work->targetYaw;
        } else {
            wrapStep = work->turnRate;
            cur      = work->yaw;
            if (diff > 0) {
                work->yaw = cur - wrapStep;
            } else {
                work->yaw = cur + wrapStep;
            }
        }
    }
    rot->vx = 0;
    rot->vy = work->yaw;
    rot->vz = 0;
    RotMatrix(rot, &coord->coord);
    SCRATCH_STACK_RELEASE_BYTES(8);
}

static void Actor03800_Fn02848(Task* arg0)
{
    _Actor03800Work* work;
    GfxCoord*        coord;
    s16              next;
    s16              speed;
    u16              value;
    s32              scale;
    _Actor03800Work* work2;

    work  = arg0->work;
    coord = work->rootCoord;
    work2 = work;
    if (work->accel != 0) {
        next        = work->speed + work->accel;
        work->speed = next;
        if (next >= 0x33) {
            work->speed = 0x32;
        }
    }
    speed = work2->speed;
    if (speed > 0) {
        scale = speed * 0x190;
        value = Actor03800_D05F40[0].power + ((Actor03800_D05F40[0].power * scale) / 10000);
    } else {
        value = Actor03800_D05F40[0].power;
    }
    work2->attackBody.key = (((s16)value | (Actor03800_D05F40[0].reaction << DAMAGE_ATTACK_REACTION_SHIFT)) & 0xFFFF) |
                            DAMAGE_ATTACK_CATEGORY;
    work->prevPos.vx   = (s16)coord->coord.t[0];
    work->prevPos.vy   = (s16)coord->coord.t[1];
    work->prevPos.vz   = (s16)coord->coord.t[2];
    coord->coord.t[0] += (s32)(coord->coord.m[0][2] * work->speed) >> 0xC;
    coord->coord.t[1] += work->fallSpeed;
    coord->coord.t[2] += (s32)(coord->coord.m[2][2] * work->speed) >> 0xC;
}

/// Switches the work's animation id, resetting the slots to the blend value the
/// table gives for the new id; otherwise ticks every slot one frame.
static inline void _actor03800TickAnim(Task* task)
{
    _Actor03800Work* work;
    s32              i;
    s32              value;

    work = task->work;
    if (work->anim != work->playingAnim) {
        work->playingAnim = work->anim;
        work->animFrame   = 0;
        value             = Actor03800_D05F90[work->anim];
        for (i = 1; i < ARRAY_SIZE(work->rig.slots); i++) {
            animationSeekSlotWithBlend(&work->rig.anim, i, work->anim, 0, value);
        }
    } else {
        work->animFrame++;
        for (i = 1; i < ARRAY_SIZE(work->rig.slots); i++) {
            animationTickSlot(&work->rig.anim, i);
        }
    }
}

/// Recolours the actor from the lighting at its root coordinate's world position.
static inline void _actor03800UpdateColorAtRoot(Task* arg0)
{
    GfxCoord* coord;
    VECTOR    vec;

    coord  = ((_Actor03800Work*)arg0->work)->rootCoord;
    vec.vx = coord->workm.t[0];
    vec.vy = coord->workm.t[1];
    vec.vz = coord->workm.t[2];
    Gp_UpdateActorColor(arg0->spawnArg2.pointer, &vec, 0, 0);
}

static void Actor03800_Fn02998(Enemy* arg0, Task* arg1)
{
    _Actor03800Work* work;
    TmdObject*       obj;
    GfxCoord*        coord;
    s32              state;
    s16              phase;
    s16              anim;
    s32              snd;
    s32              pan;

    obj   = arg1->extra.tmd;
    work  = arg1->work;
    state = gSceneCombatState.actorControl;
    coord = work->rootCoord;
    switch (state) {
        case 1:
            _actor03800UpdateColorAtRoot(arg1);
            return;
        case 2:
            obj->flags = TMD_OBJECT_SKIP_ACTIVE_DRAW;
            return;
        case 0:
        default:
            break;
    }
    switch (work->actionStep) {
        case 0:
            if (work->deathAnimChosen == 0) {
                anim = ACTOR_03800_ANIM_STAND;
                if (work->overturned != 0) {
                    anim = ACTOR_03800_ANIM_DIE_OVERTURNED;
                }
                work->anim = anim;
            }
            work->timer        = 0;
            work->deathScaleY  = 0x1000;
            work->savedRootMtx = coord->coord;
            arg0->recs         = 0;
            worldTargetUnlinkNode(&arg0->node);
            worldCollisionUnlinkBody(&work->hitBody);
            worldCollisionUnlinkBody(&work->gridBody);
            worldCollisionUnlinkBody(&work->attackBody);
            Gp_SetLightMode(arg0, ENEMY_COLOR_WEIGHTED);
            Gp_ReleaseStateF0Add(arg1, 0x26);
            work->actionStep = 1;
            if (work->burstStage != 0) {
                obj->flags       = TMD_OBJECT_SKIP_ACTIVE_DRAW;
                work->actionStep = 3;
            }
            _actor03800TickAnim(arg1);
            _actor03800UpdateColorAtRoot(arg1);
            snd = ((arg0->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x40260004;
            pan = (s8)worldCoordGetOriginAudioPan(coord);
            sndEvtRequestScriptStart(snd, pan, (s8)worldCoordGetOriginAudioDepth(coord));
            return;
        case 1:
            Actor03800_Fn037E0(arg1);
            phase       = work->timer + 1;
            work->timer = phase;
            if (phase == 10) {
                obj->flags = TMD_OBJECT_SEMI_TRANS;
            }
            if (work->timer == 15) {
                Gp_SpawnEff(EFFECT_CORPSE_BURN, coord, 2, NULL);
            }
            if (work->timer >= 0x3C) {
                work->actionStep = 2;
                obj->flags       = TMD_OBJECT_SKIP_ACTIVE_DRAW;
            }
            _actor03800TickAnim(arg1);
            _actor03800UpdateColorAtRoot(arg1);
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
                    Actor03800_Fn02E50(arg1);
                } else {
                    work->burstStage++;
                }
            }
            phase       = work->timer + 1;
            work->timer = phase;
            if (phase >= 0x3C) {
                work->actionStep = 2;
            }
            return;
    }
}

static void Actor03800_Fn02E50(Task* actor)
{
    s16  variants[4];
    s16* out;
    s32  i;
    s32  count;
    u32  first;
    s32  second;

    gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
    first           = (u16)((gRandomLcgState >> 16) % 5);
    Actor03800_Fn03008(actor, first);
    if (gSceneCombatState.battleRefs < 2) {
        count = Actor03800_D05FA8[gSceneCombatState.battleRefs];
        out   = variants;
        for (i = 0; i < 5; i++) {
            if (i != first) {
                *out++ = i;
            }
        }
        gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
        second          = (gRandomLcgState >> 16) & 3;
        Actor03800_Fn03008(actor, variants[second]);
        if (--count > 0) {
            s16* next = variants;

            for (i = 0; i < 5; i++) {
                if (i != first && i != second) {
                    *next++ = i;
                }
            }
            gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            Actor03800_Fn03008(actor, variants[(u16)((gRandomLcgState >> 16) % 3)]);
        }
    }
}

static void Actor03800_Fn03008(Task* actor, u32 variant)
{
    GameLocationKey  key;
    GameLocationKey* sessionKey;
    u8               areaByte0;
    AreaVariant*     layout;
    AreaPlacement*   entry;
    EffectWork*      eff;
    TmdObject*       model;
    s32              idx;
    u32              raw;

    switch (variant) {
        case 0:
            D_80067704[0] = &_gActor03800BlackBeetleEffect1;
            break;
        case 1:
            D_80067704[0] = &_gActor03800BlackBeetleEffect2;
            break;
        case 2:
            D_80067704[0] = &_gActor03800BlackBeetleEffect3;
            break;
        case 3:
            D_80067704[0] = &_gActor03800BlackBeetleEffect4;
            break;
        case 4:
            D_80067704[0] = &_gActor03800BlackBeetleEffect5;
            break;
    }
    eff = Gp_SpawnEff(EFFECT_BURST_BODY_PART_BANK4, actor->extra.tmd->coords + 3, 0x100, NULL);
    if (eff == NULL) {
        return;
    }
    sessionKey = &gGameSession->location.loc;
    raw        = ((Enemy*)actor->spawnArg2.pointer)->placeKey;
    model      = eff->task->extra.tmd;
    key.stage  = sessionKey->stage;
    key.area   = sessionKey->area;
    key.room   = sessionKey->room;
    areaByte0  = sessionKey->view;
    idx        = raw >> 12;
    key.view   = areaByte0;
    areaSyncLocationVariant(&key);
    layout = Gp_GetNestedAreaRec(&key);

    entry                    = gpAreaPlaceAt(layout->placements, idx);
    model->texturePageOffset = entry->texturePageOffset;
    model->clutRowOffset     = entry->clutRowOffset;
    if (model->buffer != NULL) {
        tmdBuildBufferHalf(model);
        tmdBuildBufferHalf(model);
    }
}

static void Actor03800_Fn0315C(Task* arg0)
{
    EnemyTaskFuncTable3 sp;

    sp = Actor03800_D00004;
    sp.funcs[arg0->state](((Enemy*)arg0->spawnArg2.pointer), arg0);
}

static void Actor03800_Fn031B8(Enemy* arg0, Task* arg1)
{
    _Actor03800Work* work;

    work = arg1->work;
    switch (gSceneCombatState.actorControl) {
        case 0:
            arg1->extra.tmd->flags       = 0;
            arg0->node.state.parts.flags = 0;
            break;
        case 2:
            arg1->extra.tmd->flags       = TMD_OBJECT_SKIP_ACTIVE_DRAW;
            arg0->node.state.parts.flags = 1;
            return;
        case 1:
            Actor03800_Fn036EC(arg1);
            Actor03800_Fn03744(arg1);
            return;
    }
    if (arg0->reactionFlags != 0) {
        Actor03800_Fn00974(arg1);
    }
    Actor03800_Fn00A98(arg1);
    Actor03800_Fn032D8(arg1);
    if (work->turnRate != 0) {
        Actor03800_Fn026F8(arg1);
    }
    Actor03800_Fn02848(arg1);
    if (work->stepSoundTimer != 0) {
        Actor03800_Fn03594(arg1);
    }
    Actor03800_Fn03628(arg1);
    work->rootCoord->composeStamp = GRAPHICS_COORD_DIRTY;
    actorRenderComposeCoord(work->rootCoord);
    Actor03800_Fn036EC(arg1);
    Actor03800_Fn03744(arg1);
}

static void Actor03800_Fn032D8(Task* arg0)
{
    _Actor03800Work* work;
    s16              state;
    s16              mag;

    work  = arg0->work;
    state = work->action;
    switch (state) {
        case ACTOR_03800_ACTION_IDLE:
            Actor03800_Fn01150(arg0);
            break;
        case ACTOR_03800_ACTION_WANDER:
            Actor03800_Fn012B4(arg0);
            break;
        case ACTOR_03800_ACTION_CHARGE:
            Actor03800_Fn01520(arg0);
            break;
        case ACTOR_03800_ACTION_KNOCKED_OVER:
            Actor03800_Fn0166C(arg0);
            break;
        case ACTOR_03800_ACTION_GET_UP:
            Actor03800_Fn03420(arg0);
            break;
        case ACTOR_03800_ACTION_FLINCH:
            Actor03800_Fn01948(arg0);
            break;
        case ACTOR_03800_ACTION_BUILDUP:
            Actor03800_Fn01AD0(arg0);
            break;
        case ACTOR_03800_ACTION_DIE:
            Actor03800_Fn01C50(arg0);
            break;
        case ACTOR_03800_ACTION_WALL_WAIT:
            Actor03800_Fn01EEC(arg0);
            break;
        case ACTOR_03800_ACTION_CEILING_WAIT:
            Actor03800_Fn02068(arg0);
            break;
        case ACTOR_03800_ACTION_DROP:
            Actor03800_Fn021E4(arg0);
            break;
        case ACTOR_03800_ACTION_SHRINE_WAIT:
            Actor03800_Fn034B0(arg0);
            break;
        case ACTOR_03800_ACTION_LEAP_AWAY:
            Actor03800_Fn02584(arg0);
            break;
    }
    if (work->overturned == 0) {
        work->gridBody.pos.vy = -0xFA;
        mag                   = 0xFA;
    } else {
        work->gridBody.pos.vy = -0x15E;
        mag                   = 0x15E;
    }
    work->gridBody.radius = mag;
}

static void Actor03800_Fn03420(Task* arg0)
{
    _Actor03800Work* work = arg0->work;
    s32              state;

    state = work->actionStep;
    switch (state) {
        case 0:
            work->anim       = ACTOR_03800_ANIM_GET_UP;
            work->actionStep = 1;
            break;
        case 1:
            if (work->animFrame == 0x1E) {
                work->overturned = 0;
            }
            if (work->animFrame >= 0x3A) {
                work->action     = ACTOR_03800_ACTION_CHARGE;
                work->actionStep = 0;
                if (work->stepSoundTimer == 0) {
                    work->stepSoundTimer = state;
                }
                work->attackBody.flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;
            }
            break;
    }
}

static void Actor03800_Fn034B0(Task* arg0)
{
    TmdObject*       obj;
    Enemy*           ctx;
    _Actor03800Work* work;

    work = arg0->work;
    obj  = arg0->extra.tmd;
    ctx  = arg0->spawnArg2.pointer;
    switch (gSceneCombatState.shrineEnemyPhase) {
        case SCENE_COMBAT_SHRINE_HIDDEN:
            obj->flags                  = (TMD_OBJECT_SKIP_ACTIVE_DRAW | TMD_OBJECT_SKIP_AUTO_BUFFER);
            ctx->node.state.parts.flags = WORLD_TARGET_NOT_LOCKABLE;
            return;
        case SCENE_COMBAT_SHRINE_REVEALED:
            obj->flags              = 0;
            work->hitBody.flags    |= WORLD_COLLISION_BODY_PAIR_ENABLED;
            work->gridBody.flags   |= (WORLD_COLLISION_BODY_FLOOR_QUERY | WORLD_COLLISION_BODY_GRID_ENABLED);
            work->attackBody.flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;
            Gp_ArmStateF0(1);
            work->fallSpeed   = 0x80;
            work->timer       = 0x5A;
            work->mode        = ACTOR_03800_MODE_FLOOR;
            work->shadowShade = 0x80;
            return;
        case SCENE_COMBAT_SHRINE_RELEASED:
            if (--work->timer <= 0) {
                work->action     = ACTOR_03800_ACTION_WANDER;
                work->actionStep = 0;
            }
            return;
    }
}

static void Actor03800_Fn03594(Task* arg0)
{
    _Actor03800Work* work;
    GfxCoord*        coord;
    s32              soundId;
    s32              pan;

    work  = arg0->work;
    coord = work->rootCoord;
    if (--work->stepSoundTimer <= 0) {
        work->stepSoundTimer = 0xC;
        soundId              = ((((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x40260001;
        pan                  = (s8)worldCoordGetOriginAudioPan(coord);
        sndEvtRequestScriptStart(soundId, pan, (s8)worldCoordGetOriginAudioDepth(coord));
    }
}

static void Actor03800_Fn03628(Task* arg0)
{
    _actor03800TickAnim(arg0);
}

static void Actor03800_Fn036EC(Task* arg0)
{
    GfxCoord* coord;
    VECTOR    vec;

    coord  = ((_Actor03800Work*)arg0->work)->rootCoord;
    vec.vx = coord->workm.t[0];
    vec.vy = coord->workm.t[1];
    vec.vz = coord->workm.t[2];
    Gp_UpdateActorColor(arg0->spawnArg2.pointer, &vec, 0, 0);
}

static void Actor03800_Fn03744(Task* arg0)
{
    _Actor03800Work* work;
    GfxCoord*        coord;
    VECTOR3          vec;
    s16              hit;

    work  = arg0->work;
    coord = work->rootCoord;
    if (work->mode == ACTOR_03800_MODE_FLOOR) {
        vec.vx = coord->workm.t[0];
        vec.vy = coord->workm.t[1];
        vec.vz = coord->workm.t[2];
        effectDrawGroundShadow(&vec, 0x1F4, work->shadowShade);
        return;
    }
    hit = worldCollisionProjectGroundPoint(MATRIX_TRANS(&coord->workm), &vec);
    if (hit != 0) {
        effectDrawGroundShadow(&vec, 0x200, effectGetGroundShadowShade(0x200, 0x80, hit));
    }
}

static void Actor03800_Fn037E0(Task* arg0)
{
    _Actor03800Work*   work;
    GfxCoord*          coord;
    ActorScaleScratch* head;
    ActorScaleScratch* scratch;

    work                                    = arg0->work;
    head                                    = SCRATCH_STACK_CURSOR(ActorScaleScratch);
    scratch                                 = head - 1;
    SCRATCH_STACK_CURSOR(ActorScaleScratch) = scratch;
    coord                                   = work->rootCoord;
    if (work->deathScaleY >= 0x201) {
        work->deathScaleY = work->deathScaleY - 0x50;
    }
    scratch->scale.vx                    = ONE;
    scratch->scale.vy                    = work->deathScaleY;
    scratch->scale.vz                    = ONE;
    coord->coord                         = work->savedRootMtx;
    scratch->matrix.rotationWords.m00M01 = ONE;
    scratch->matrix.rotationWords.m02M10 = 0;
    scratch->matrix.rotationWords.m11M12 = ONE;
    scratch->matrix.rotationWords.m20M21 = 0;
    scratch->matrix.rotationWords.m22    = ONE;
    ScaleMatrix(&scratch->matrix.mat, &scratch->scale);
    MulMatrix(&coord->coord, &scratch->matrix.mat);
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
    SCRATCH_STACK_RELEASE_BLOCK(ActorScaleScratch);
}
