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

/// Stage values shared by actions that hand control to one another.
enum {
    ACTOR_03800_ACTION_BEGIN      = 0,
    ACTOR_03800_KNOCKED_OVER_WAIT = 2,
};

/// Per-tick distances in the root's parent frame and ground-shadow vertex shades.
enum {
    ACTOR_03800_FLOOR_FALL_SPEED = 128,
    ACTOR_03800_SHADOW_SHADE     = 128,
};

/// Bank 0x4026 script entries; the placement index supplies the instance byte.
enum {
    ACTOR_03800_SOUND_IMPACT         = 0x40260002,
    ACTOR_03800_SOUND_REACTION       = 0x40260003,
    ACTOR_03800_SOUND_INSTANCE_SHIFT = 8,
};

/// Bit selecting a positive random turn; a clear bit selects its negative.
enum { ACTOR_03800_RANDOM_TURN_POSITIVE_BIT = 0x400 };

/// Minimum idle/perch wait and its inclusive random extension, in task ticks.
enum {
    ACTOR_03800_IDLE_WAIT_BASE        = 90,
    ACTOR_03800_IDLE_WAIT_JITTER_MASK = 255,
};

/// Overturned wait in task ticks, extended for each integer percent of HP lost.
enum {
    ACTOR_03800_OVERTURNED_WAIT_BASE           = 240,
    ACTOR_03800_OVERTURNED_WAIT_PER_HP_PERCENT = 10,
};

/// Random delay before a perched actor drops, in task ticks.
enum {
    ACTOR_03800_DROP_DELAY_BASE        = 15,
    ACTOR_03800_DROP_DELAY_JITTER_MASK = 15,
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

static void        _actor03800ActionIdle(Task* task);
static void        _actor03800ActionWander(Task* task);
static void        _actor03800ActionCharge(Task* task);
static void        _actor03800ActionKnockedOver(Task* task);
static void        _actor03800ActionFlinch(Task* task);
static void        _actor03800ActionBuildup(Task* task);
static void        _actor03800ActionDie(Task* task);
static void        _actor03800ActionWallWait(Task* task);
static void        _actor03800ActionCeilingWait(Task* task);
static void        _actor03800ActionDrop(Task* task);
static void        _actor03800ActionLeapAway(Task* task);
static inline void _actor03800TickAnim(Task* task);

static void _actor03800Spawn(Enemy* enemy, Task* task);
static void _actor03800ApplyPlacementMode(Task* task);
static void Actor03800_Fn00974(Task* arg0);
static void Actor03800_Fn00A98(Task* arg0);
static void Actor03800_Fn026F8(Task* arg0);
static void Actor03800_Fn02848(Task* arg0);
static void Actor03800_Fn02998(Enemy* arg0, Task* arg1);
static void Actor03800_Fn02E50(Task* arg0);
static void Actor03800_Fn03008(Task* actor, u32 variant);
static void Actor03800_Fn031B8(Enemy* arg0, Task* arg1);
static void Actor03800_Fn032D8(Task* arg0);
static void _actor03800ActionGetUp(Task* task);
static void _actor03800ActionShrineWait(Task* task);
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
        _actor03800Spawn,
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

/// Allocates and binds the actor task's rig, target and collision bodies.
///
/// Requires a live enemy in `task->spawnArg2.pointer`, a six-part TMD and a
/// placement mode supported by `_actor03800ApplyPlacementMode`. The task owns
/// the zeroed work block; allocation failure destroys the enemy and task.
/// Successful setup links three bodies and advances to the active task state.
static void _actor03800Spawn(Enemy* enemy, Task* task)
{
    enum {
        ACTOR_03800_BODY_KEY    = WORLD_COLLISION_CONTACT_ENEMY_BODY | 0x26,
        ACTOR_03800_TASK_ACTIVE = 1,
    };
    WorldCollisionBody*    attackBody;
    WorldCollisionContact* hitContacts;
    WorldCollisionContact* gridContacts;
    WorldCollisionContact* attackContacts;
    _Actor03800Work*       work;
    s32                    slotIndex;
    TmdObject*             model;

    // Bind task-owned storage before publishing the enemy and collision bodies.
    model = task->extra.tmd;
    work  = memCalloc(sizeof(_Actor03800Work), 0);
    if (work == NULL) {
        enemyDestroy(enemy, task);
        return;
    }
    task->work      = work;
    model->lightMtx = &work->lightMtx;
    model->flags    = 0;
    model->colorMtx = &work->colorMtx;
    sceneAcquireBattleRef(0);
    _actor03800ApplyPlacementMode(task);
    enemy->field_4  = &work->rootCoord->coord;
    enemy->field_48 = 0;
    worldTargetLinkNode(&enemy->node);
    enemy->coord                  = &task->extra.tmd->coords[3];
    enemy->node.state.parts.flags = 0;
    enemy->bodyPos.vx             = 0;
    enemy->recs                   = work->hitContacts;
    enemy->bodyPos.vy             = 0;
    enemy->bodyPos.vz             = 0;
    enemy->param                  = &Actor03800_D05F44;
    enemy->hp                     = (s16)Actor03800_D05F44.hpMax;
    work->hitEffectArg.coord      = &task->extra.tmd->coords[3];
    work->hitEffectArg.spawnArgLo = 0x200;
    work->hitEffectArg.spawnArgHi = 1;
    animationInitContext(&work->rig.anim, Actor03800_D05F60, model, work->rig.poses, work->rig.slots);
    for (slotIndex = 1; slotIndex < ARRAY_SIZE(work->rig.slots); slotIndex++) {
        animationResetSlot(&work->rig.anim, slotIndex, ACTOR_03800_ANIM_STAND);
    }
    // Contact arrays stay live with the work block until the bodies are unlinked.
    work->hitBody.coord            = task->extra.tmd->coords;
    hitContacts                    = work->hitContacts;
    work->hitBody.pos.vy           = -0xFA;
    work->hitBody.context.contacts = hitContacts;
    work->hitBody.pos.vx           = 0;
    work->hitBody.pos.vz           = 0;
    work->hitBody.key              = ACTOR_03800_BODY_KEY;
    work->hitBody.radius           = 0xFA;
    work->hitBody.flags            = WORLD_COLLISION_BODY_SPHERE;
    worldCollisionLinkBody(WORLD_COLLISION_LIST_ENEMY_BODIES, &work->hitBody);
    worldCollisionInitContacts(hitContacts, ARRAY_SIZE(work->hitContacts), 0);
    work->gridBody.coord            = task->extra.tmd->coords;
    gridContacts                    = work->gridContacts;
    work->gridBody.pos.vy           = -0x12C;
    work->gridBody.context.contacts = gridContacts;
    work->gridBody.pos.vx           = 0;
    work->gridBody.pos.vz           = 0;
    work->gridBody.key              = ACTOR_03800_BODY_KEY;
    work->gridBody.radius           = 0x12C;
    work->gridBody.flags            = WORLD_COLLISION_BODY_SPHERE;
    worldCollisionLinkBody(WORLD_COLLISION_LIST_ENEMY_BODIES, &work->gridBody);
    worldCollisionInitContacts(gridContacts, ARRAY_SIZE(work->gridContacts), 0);
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
    attackBody                        = &work->attackBody;
    work->attackBody.coord            = task->extra.tmd->coords;
    attackContacts                    = work->attackContacts;
    work->attackBody.pos.vy           = -0xFA;
    work->attackBody.pos.vz           = 0xFA;
    work->attackBody.radius           = 0xC8;
    work->attackBody.context.contacts = attackContacts;
    work->attackBody.pos.vx           = 0;
    work->attackBody.key              = 0;
    work->attackBody.flags            = WORLD_COLLISION_BODY_SPHERE;
    worldCollisionLinkBody(WORLD_COLLISION_LIST_ENEMY_ATTACKS, attackBody);
    worldCollisionInitContacts(attackContacts, ARRAY_SIZE(work->attackContacts), 0);
    work->attackBody.flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;
    task->state             = ACTOR_03800_TASK_ACTIVE;
}

/// Sets the initial action and coordinate hierarchy from the placement mode.
///
/// Requires a fresh zeroed work block and a live six-part TMD. Decimal tens
/// select floor, wall, ceiling or shrine (0..3); floor units select idle or
/// wander (0..1). Perched roots attach beneath a work-owned coordinate turned
/// about X by a quarter or half turn; its translation becomes the landing point.
static void _actor03800ApplyPlacementMode(Task* task)
{
    enum { ACTOR_03800_SHADOW_NONE = -1 };
    _Actor03800Work* work;
    Enemy*           enemy;
    GfxCoord*        modelRoot;
    SVECTOR          perchAngles;
    MATRIX           perchRotation;
    s16              mode;
    s16              initialAction;

    /// Attaches a fresh model root beneath its pitched perch, retaining the landing position.
    ///
    /// Use as a standalone compound statement in the placement cases. Operands
    /// must be stable pointer locals; each occurs more than once. Captures
    /// `perchAngles`/`perchRotation` and clobbers the GTE; pitch is evaluated once
    /// and narrowed to a signed halfword in 4096-per-turn units.
#define ACTOR_03800_ATTACH_PERCH(workBlock, root, pitch)         \
    {                                                            \
        (workBlock)->savedRootMtx = (root)->coord;               \
                                                                 \
        gfxSetRotIdentity(&(workBlock)->perchCoord.coord);       \
                                                                 \
        (workBlock)->perchCoord.parent     = &gGfxViewCoord;     \
        (workBlock)->perchCoord.coord      = (root)->coord;      \
        (workBlock)->perchCoord.coord.t[0] = (root)->coord.t[0]; \
        (workBlock)->perchCoord.coord.t[1] = (root)->coord.t[1]; \
        (workBlock)->perchCoord.coord.t[2] = (root)->coord.t[2]; \
                                                                 \
        gfxSetRotIdentity(&(root)->coord);                       \
                                                                 \
        (root)->parent     = &(workBlock)->perchCoord;           \
        (root)->coord.t[0] = 0;                                  \
        (root)->coord.t[1] = 0;                                  \
        (root)->coord.t[2] = 0;                                  \
                                                                 \
        perchAngles.vx = (pitch);                                \
        perchAngles.vy = 0;                                      \
        perchAngles.vz = 0;                                      \
        RotMatrix(&perchAngles, &perchRotation);                 \
                                                                 \
        gte_SetRotMatrix(&(workBlock)->perchCoord.coord);        \
        gte_ldclmv(&perchRotation.m[0][0]);                      \
        gte_rtir();                                              \
        gte_stclmv(&(workBlock)->perchCoord.coord.m[0][0]);      \
        gte_ldclmv(&perchRotation.m[0][1]);                      \
        gte_rtir();                                              \
        gte_stclmv(&(workBlock)->perchCoord.coord.m[0][1]);      \
        gte_ldclmv(&perchRotation.m[0][2]);                      \
        gte_rtir();                                              \
        gte_stclmv(&(workBlock)->perchCoord.coord.m[0][2]);      \
    }

    enemy     = task->spawnArg2.pointer;
    work      = task->work;
    modelRoot = task->extra.tmd->coords;
    mode      = enemy->place->mode / 10;

    work->mode = mode;
    switch (mode) {
        case ACTOR_03800_MODE_FLOOR:
            initialAction = enemy->place->mode % 10;
            work->action  = initialAction;
            switch (initialAction) {
                case ACTOR_03800_ACTION_IDLE:
                    work->anim      = ACTOR_03800_ANIM_STAND;
                    work->alerted   = 0;
                    gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                    work->timer     = ((gRandomLcgState >> 0x10) & ACTOR_03800_IDLE_WAIT_JITTER_MASK) + ACTOR_03800_IDLE_WAIT_BASE;
                    break;
                case ACTOR_03800_ACTION_WANDER:
                    work->anim    = ACTOR_03800_ANIM_WALK;
                    work->timer   = 0;
                    work->alerted = 1;
                    break;
            }
            work->fallSpeed   = ACTOR_03800_FLOOR_FALL_SPEED;
            work->shadowShade = ACTOR_03800_SHADOW_SHADE;
            work->rootCoord   = task->extra.tmd->coords;
            break;
        case ACTOR_03800_MODE_WALL:
            work->action      = ACTOR_03800_ACTION_WALL_WAIT;
            work->shadowShade = ACTOR_03800_SHADOW_NONE;
            work->fallSpeed   = 0;
            work->rootCoord   = &work->perchCoord;
            work->overturned  = 1;
            work->alerted     = 0;
            ACTOR_03800_ATTACH_PERCH(work, modelRoot, ACTOR_TRANSFORM_ANGLE_TURN / 4);
            break;
        case ACTOR_03800_MODE_CEILING:
            work->action      = ACTOR_03800_ACTION_CEILING_WAIT;
            work->shadowShade = ACTOR_03800_SHADOW_NONE;
            work->fallSpeed   = 0;
            work->rootCoord   = &work->perchCoord;
            work->overturned  = 1;
            work->alerted     = 0;
            ACTOR_03800_ATTACH_PERCH(work, modelRoot, ACTOR_TRANSFORM_ANGLE_HALF_TURN);
            break;
        case ACTOR_03800_MODE_SHRINE:
            work->action      = ACTOR_03800_ACTION_SHRINE_WAIT;
            work->fallSpeed   = 0;
            work->shadowShade = ACTOR_03800_SHADOW_NONE;
            work->rootCoord   = task->extra.tmd->coords;
            break;
    }
#undef ACTOR_03800_ATTACH_PERCH
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
        damage = damageTickEnemyDamageOverTime(ctx);
        if (damage != 0) {
            worldTargetAddReadoutAmount(&ctx->node, (s32)damage, 0);
            remaining = ctx->hp - damage;
            ctx->hp   = remaining;
            if ((s16)remaining <= 0) {
                work->action = ACTOR_03800_ACTION_DIE;
            } else {
                work->action = ACTOR_03800_ACTION_FLINCH;
            }
            work->actionStep = 0;
        }
        if (damageIsEnemyDamageOverTimeExpired(ctx) != 0) {
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
    result = worldCollisionResolvePushback(work->gridContacts, &frame->delta, ARRAY_SIZE(work->gridContacts), NULL);
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
                    damage                 = damageComputePlayerAttack(work->hitContacts[i].key.value, SquareRoot0((frame->delta.vector.vx * frame->delta.vector.vx) + (frame->delta.vector.vy * frame->delta.vector.vy) + (frame->delta.vector.vz * frame->delta.vector.vz)), 0, 0);
                    if (work->overturned == 0) {
                        if (damageRollCriticalHit(ctx, work->hitContacts[i].key.value, 0) != 0) {
                            damage *= 4;
                            effectSpawn(EFFECT_CRITICAL_HIT, coord, 0, NULL);
                        }
                    } else if (!(work->hitContacts[i].key.value & 0x8000) && (damage != 0)) {
                        damage *= 3;
                        effectSpawn(EFFECT_CRITICAL_HIT, coord, 4, NULL);
                    }
                    worldTargetAddReadoutAmount(&ctx->node, damage, 0);
                    damageAccumulateLifeDrainHp(ctx, work->hitContacts[i].key.value, damage, 0);
                    ctx->hp -= damage;
                    if (ctx->hp <= 0) {
                        reaction = 2;
                    }
                    switch (damageGetPlayerAttackReaction(work->hitContacts[i].key.value) & 0xFFFF) {
                        case DAMAGE_PLAYER_REACTION_NONE:
                        default:
                            break;
                        case DAMAGE_PLAYER_REACTION_POISON:
                            damageTryStartEnemyDamageOverTime(ctx, work->hitContacts[i].key.value, 0);
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
                        case DAMAGE_PLAYER_REACTION_EXPLOSION:
                            if (ctx->hp <= 0) {
                                work->burstStage = 1;
                            } else if (work->overturned == 0) {
                                reaction = 1;
                            }
                            break;
                        case 8:
                            if (work->overturned == 0 && reaction == 0) {
                                damageStartEnemyBuildup(ctx, work->hitContacts[i].key.value, 0);
                            }
                            break;
                        case DAMAGE_PLAYER_REACTION_STAGGER:
                        case DAMAGE_PLAYER_REACTION_BUILDUP:
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
                        effectSpawnHit(damageGetPlayerAttackEffectId(lastId), arg0->extra.tmd->coords + 3, NULL, &work->hitEffectArg);
                    }
                    result = damageGetPlayerAttackHitCooldown(work->hitContacts[i].key.value);
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
        sceneEngageBattle(1);
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

/// Waits and turns on the spot, wandering once alerted or charging at a combat signal.
static void _actor03800ActionIdle(Task* task)
{
    enum {
        ACTOR_03800_IDLE_TURN = 1,
    };
    _Actor03800Work* work;
    s32              yawOffset;

    work = task->work;

    switch (work->actionStep) {
        case ACTOR_03800_ACTION_BEGIN:
            work->turnRate = 0;
            work->speed    = 0;
            work->accel    = 0;
            work->timer--;
            if (work->timer <= 0) {
                work->actionStep = ACTOR_03800_IDLE_TURN;
                gRandomLcgState  = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                yawOffset        = (gRandomLcgState >> 16) & (ACTOR_TRANSFORM_ANGLE_TURN / 4 - 1);
                if (((gRandomLcgState >> 16) & ACTOR_03800_RANDOM_TURN_POSITIVE_BIT) == 0) {
                    yawOffset = -yawOffset;
                }
                work->anim           = ACTOR_03800_ANIM_WALK;
                work->stepSoundTimer = 1;
                work->targetYaw      = (work->yaw + yawOffset) & ACTOR_TRANSFORM_ANGLE_MASK;
            }
            break;
        case ACTOR_03800_IDLE_TURN:
            work->turnRate = 0x1E;
            work->speed    = 0;
            work->accel    = 0;
            if (work->yaw == work->targetYaw) {
                if (work->alerted == 0) {
                    work->actionStep     = ACTOR_03800_ACTION_BEGIN;
                    work->anim           = ACTOR_03800_ANIM_STAND;
                    work->stepSoundTimer = 0;
                    gRandomLcgState      = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                    work->timer          = ((gRandomLcgState >> 16) & ACTOR_03800_IDLE_WAIT_JITTER_MASK) + ACTOR_03800_IDLE_WAIT_BASE;
                } else {
                    work->action     = ACTOR_03800_ACTION_WANDER;
                    work->actionStep = ACTOR_03800_ACTION_BEGIN;
                    if (work->stepSoundTimer == 0) {
                        work->stepSoundTimer = 1;
                    }
                }
            }
            break;
    }

    if (gSceneCombatState.signals.bytes.actionFlags & (SCENE_COMBAT_ACTION_NOISE | SCENE_COMBAT_ACTION_PE_CAST_OTHER)) {
        work->action     = ACTOR_03800_ACTION_CHARGE;
        work->actionStep = ACTOR_03800_ACTION_BEGIN;
        if (work->stepSoundTimer == 0) {
            work->stepSoundTimer = 1;
        }
        work->alerted = 1;
    }
}

/// Walks with changing headings, answering walls and combat signals until its wander ends.
static void _actor03800ActionWander(Task* task)
{
    enum {
        ACTOR_03800_WANDER_HEADING_DELAY_BASE        = 25,
        ACTOR_03800_WANDER_HEADING_DELAY_JITTER_MASK = 15,
        ACTOR_03800_WANDER_DURATION_BASE             = 30,
        ACTOR_03800_WANDER_DURATION_JITTER_MASK      = 31,
        ACTOR_03800_WANDER_WALK                      = 1,
    };
    _Actor03800Work* work;
    s32              initialYawOffset;
    s32              entryYawOffset;
    s32              nextYawOffset;

    work = task->work;

    switch (work->actionStep) {
        case ACTOR_03800_ACTION_BEGIN:
            work->turnRate   = 0;
            work->accel      = 2;
            gRandomLcgState  = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            initialYawOffset = (gRandomLcgState >> 16) & (ACTOR_TRANSFORM_ANGLE_TURN / 8 - 1);
            if (((gRandomLcgState >> 16) & ACTOR_03800_RANDOM_TURN_POSITIVE_BIT) == 0) {
                initialYawOffset = -initialYawOffset;
            }
            entryYawOffset = initialYawOffset;
            if (work->hitWall != 0) {
                entryYawOffset = initialYawOffset + ACTOR_TRANSFORM_ANGLE_HALF_TURN;
                work->hitWall  = 0;
            }
            work->anim        = ACTOR_03800_ANIM_WALK;
            work->actionStep  = ACTOR_03800_WANDER_WALK;
            work->targetYaw   = (work->yaw + entryYawOffset) & ACTOR_TRANSFORM_ANGLE_MASK;
            initialYawOffset  = 0; /* dead store: keeps `initialYawOffset` cse-canonical over `entryYawOffset` */
            gRandomLcgState   = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            work->timer       = ((gRandomLcgState >> 16) & ACTOR_03800_WANDER_HEADING_DELAY_JITTER_MASK) + ACTOR_03800_WANDER_HEADING_DELAY_BASE;
            gRandomLcgState   = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            work->wanderTimer = ((gRandomLcgState >> 16) & ACTOR_03800_WANDER_DURATION_JITTER_MASK) + ACTOR_03800_WANDER_DURATION_BASE;
            break;
        case ACTOR_03800_WANDER_WALK:
            work->turnRate = 0x1E;
            work->accel    = 2;
            work->timer--;
            if (work->timer <= 0) {
                gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                work->timer     = ((gRandomLcgState >> 16) & ACTOR_03800_WANDER_HEADING_DELAY_JITTER_MASK) + ACTOR_03800_WANDER_HEADING_DELAY_BASE;
                gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                nextYawOffset   = (gRandomLcgState >> 16) & (ACTOR_TRANSFORM_ANGLE_TURN / 8 - 1);
                if (((gRandomLcgState >> 16) & ACTOR_03800_RANDOM_TURN_POSITIVE_BIT) == 0) {
                    nextYawOffset = -nextYawOffset;
                }
                work->targetYaw = (work->yaw + nextYawOffset) & ACTOR_TRANSFORM_ANGLE_MASK;
            }
            if (work->hitWall != 0) {
                if (work->speed < 0x1E) {
                    work->action         = ACTOR_03800_ACTION_IDLE;
                    work->actionStep     = ACTOR_03800_ACTION_BEGIN;
                    work->anim           = ACTOR_03800_ANIM_STAND;
                    work->stepSoundTimer = 0;
                    gRandomLcgState      = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                    work->timer          = ((gRandomLcgState >> 16) & ACTOR_03800_WANDER_DURATION_JITTER_MASK) + ACTOR_03800_WANDER_DURATION_BASE;
                } else {
                    work->action     = ACTOR_03800_ACTION_LEAP_AWAY;
                    work->actionStep = ACTOR_03800_ACTION_BEGIN;
                }
            }
            work->wanderTimer--;
            if (work->wanderTimer <= 0) {
                work->action         = ACTOR_03800_ACTION_IDLE;
                work->actionStep     = ACTOR_03800_ACTION_BEGIN;
                work->anim           = ACTOR_03800_ANIM_STAND;
                work->stepSoundTimer = 0;
                gRandomLcgState      = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                work->timer          = ((gRandomLcgState >> 16) & ACTOR_03800_WANDER_DURATION_JITTER_MASK) + ACTOR_03800_WANDER_DURATION_BASE;
            }
            break;
    }

    if (gSceneCombatState.signals.bytes.actionFlags & (SCENE_COMBAT_ACTION_NOISE | SCENE_COMBAT_ACTION_PE_CAST_OTHER)) {
        work->action     = ACTOR_03800_ACTION_CHARGE;
        work->actionStep = ACTOR_03800_ACTION_BEGIN;
    }
}

/// Faces the player once, then accelerates straight ahead for 120..151 ticks.
///
/// Both roots must share their parent frame. The X/Z bearing narrows each
/// coordinate difference to a signed halfword before `ratan2`; yaw wraps
/// at 4096 units per turn. Turning happens before forward acceleration.
static void _actor03800ActionCharge(Task* task)
{
    enum {
        ACTOR_03800_CHARGE_DURATION_BASE        = 120,
        ACTOR_03800_CHARGE_DURATION_JITTER_MASK = 31,
        ACTOR_03800_CHARGE_TURN                 = 1,
        ACTOR_03800_CHARGE_RUN                  = 2,
    };
    _Actor03800Work* work;
    GfxCoord*        rootCoord;
    VECTOR           toPlayer;

    work      = task->work;
    rootCoord = work->rootCoord;

    switch (work->actionStep) {
        case ACTOR_03800_ACTION_BEGIN:
            work->turnRate  = 0;
            work->speed     = 0;
            work->accel     = 0;
            toPlayer.vx     = gPlayerStatus.coordMtx->t[0] - rootCoord->coord.t[0];
            toPlayer.vy     = 0;
            toPlayer.vz     = gPlayerStatus.coordMtx->t[2] - rootCoord->coord.t[2];
            work->targetYaw = ratan2((s16)toPlayer.vx, (s16)toPlayer.vz) & ACTOR_TRANSFORM_ANGLE_MASK;
            work->anim      = ACTOR_03800_ANIM_CHARGE;
            if (work->stepSoundTimer == 0) {
                work->stepSoundTimer = 1;
            }
            work->actionStep = ACTOR_03800_CHARGE_TURN;
            break;
        case ACTOR_03800_CHARGE_TURN:
            work->turnRate = 0x28;
            work->speed    = 0;
            work->accel    = 0;
            if (work->yaw == work->targetYaw) {
                work->actionStep = ACTOR_03800_CHARGE_RUN;
                gRandomLcgState  = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                work->timer      = ((gRandomLcgState >> 16) & ACTOR_03800_CHARGE_DURATION_JITTER_MASK) + ACTOR_03800_CHARGE_DURATION_BASE;
            }
            break;
        case ACTOR_03800_CHARGE_RUN:
            work->turnRate = 0;
            work->accel    = 2;
            work->timer--;
            if (work->timer <= 0) {
                work->action     = ACTOR_03800_ACTION_WANDER;
                work->actionStep = ACTOR_03800_ACTION_BEGIN;
            }
            break;
    }
}

/// Slides away from the player while overturning, then waits before getting up.
///
/// Reserves and releases one scratch block per call. Slide direction uses
/// all three axes, but only X/Z move, by 17/512 of the Q12 unit direction.
/// The wait is 240 ticks plus 10 for each integer percent of hit points lost.
static void _actor03800ActionKnockedOver(Task* task)
{
    enum {
        ACTOR_03800_KNOCKED_OVER_SLIDE            = 1,
        ACTOR_03800_KNOCKED_OVER_SLIDE_FIRST_TICK = 2,
        ACTOR_03800_KNOCKED_OVER_SLIDE_END_TICK   = 14,
        ACTOR_03800_KNOCKED_OVER_IMPACT_TICK      = 12,
        ACTOR_03800_KNOCKED_OVER_WAIT_TICK        = 29,
        ACTOR_03800_KNOCKED_OVER_DIRECTION_SHIFT  = 9,
    };
    _Actor03800KnockedOverScratch* scratch;
    _Actor03800Work*               work;
    Enemy*                         enemy;
    GfxCoord*                      rootCoord;
    s16                            actionStep;
    Enemy*                         reactionEnemy;
    Enemy*                         impactEnemy;
    s32                            soundId;
    s32                            reactionPan;
    s32                            impactPan;

    scratch    = SCRATCH_STACK_RESERVE_BLOCK(_Actor03800KnockedOverScratch);
    work       = task->work;
    enemy      = task->spawnArg2.pointer;
    actionStep = work->actionStep;
    rootCoord  = work->rootCoord;
    switch (actionStep) {
        case ACTOR_03800_ACTION_BEGIN:
            work->anim              = ACTOR_03800_ANIM_KNOCKED_OVER;
            work->actionStep        = ACTOR_03800_KNOCKED_OVER_SLIDE;
            work->stepSoundTimer    = 0;
            work->overturned        = 1;
            work->attackBody.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
            reactionEnemy           = task->spawnArg2.pointer;
            soundId                 = ((reactionEnemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << ACTOR_03800_SOUND_INSTANCE_SHIFT) | ACTOR_03800_SOUND_REACTION;
            reactionPan             = (s8)worldCoordGetOriginAudioPan(rootCoord);
            sndEvtRequestScriptStart(soundId, reactionPan, (s8)worldCoordGetOriginAudioDepth(rootCoord));
            break;
        case ACTOR_03800_KNOCKED_OVER_SLIDE:
            if (work->animFrame >= ACTOR_03800_KNOCKED_OVER_SLIDE_FIRST_TICK && work->animFrame < ACTOR_03800_KNOCKED_OVER_SLIDE_END_TICK) {
                // Slide along the ground, straight away from the player.
                scratch->fromPlayer.vx = rootCoord->coord.t[0] - gPlayerStatus.coordMtx->t[0];
                scratch->fromPlayer.vy = rootCoord->coord.t[1] - gPlayerStatus.coordMtx->t[1];
                scratch->fromPlayer.vz = rootCoord->coord.t[2] - gPlayerStatus.coordMtx->t[2];
                VectorNormalS(&scratch->fromPlayer, &scratch->direction);
                rootCoord->coord.t[0] += (scratch->direction.vx * 17) >> ACTOR_03800_KNOCKED_OVER_DIRECTION_SHIFT;
                rootCoord->coord.t[2] += (scratch->direction.vz * 17) >> ACTOR_03800_KNOCKED_OVER_DIRECTION_SHIFT;
            } else {
                work->speed = 0;
                work->accel = 0;
            }
            if (work->animFrame == ACTOR_03800_KNOCKED_OVER_IMPACT_TICK) {
                impactEnemy = task->spawnArg2.pointer;
                soundId     = ((impactEnemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << ACTOR_03800_SOUND_INSTANCE_SHIFT) | ACTOR_03800_SOUND_IMPACT;
                impactPan   = (s8)worldCoordGetOriginAudioPan(rootCoord);
                sndEvtRequestScriptStart(soundId, impactPan, (s8)worldCoordGetOriginAudioDepth(rootCoord));
            }
            if (work->animFrame >= ACTOR_03800_KNOCKED_OVER_WAIT_TICK) {
                work->actionStep      = ACTOR_03800_KNOCKED_OVER_WAIT;
                work->overturnedTimer = ((Actor03800_D05F44.hpMax - enemy->hp) * 100 / Actor03800_D05F44.hpMax) * ACTOR_03800_OVERTURNED_WAIT_PER_HP_PERCENT + ACTOR_03800_OVERTURNED_WAIT_BASE;
            }
            break;
        case ACTOR_03800_KNOCKED_OVER_WAIT:
            work->overturnedTimer--;
            if (work->overturnedTimer <= 0) {
                work->action     = ACTOR_03800_ACTION_GET_UP;
                work->actionStep = ACTOR_03800_ACTION_BEGIN;
            }
            break;
    }
    SCRATCH_STACK_RELEASE_BLOCK(_Actor03800KnockedOverScratch);
}

/// Recoils from a hit, then resumes charge, buildup or the overturned wait.
///
/// A perched hit starts the drop directly at its falling stage. Animation
/// requests deliberately differ from `playingAnim` to restart the reaction.
static void _actor03800ActionFlinch(Task* task)
{
    enum {
        ACTOR_03800_FLINCH_WAIT             = 1,
        ACTOR_03800_FLINCH_UPRIGHT_TICKS    = 12,
        ACTOR_03800_FLINCH_OVERTURNED_TICKS = 19,
    };
    _Actor03800Work* work = task->work;
    GfxCoord*        rootCoord;
    Enemy*           reactionEnemy;
    s16              actionStep;
    s32              soundId;
    s32              reactionPan;

    actionStep = work->actionStep;
    rootCoord  = work->rootCoord;
    switch (actionStep) {
        case ACTOR_03800_ACTION_BEGIN:
            if (work->mode == ACTOR_03800_MODE_FLOOR) {
                if (work->overturned == 0) {
                    work->anim  = ACTOR_03800_ANIM_FLINCH;
                    work->timer = ACTOR_03800_FLINCH_UPRIGHT_TICKS;
                } else {
                    work->anim  = ACTOR_03800_ANIM_FLINCH_OVERTURNED;
                    work->timer = ACTOR_03800_FLINCH_OVERTURNED_TICKS;
                }
            } else {
                work->anim   = ACTOR_03800_ANIM_FLINCH_PERCHED;
                work->timer  = 0;
                work->action = ACTOR_03800_ACTION_DROP;
            }
            work->actionStep     = ACTOR_03800_FLINCH_WAIT;
            work->playingAnim    = ACTOR_03800_ANIM_STAND; // not the animation asked for, so it starts afresh
            work->stepSoundTimer = 0;
            work->speed          = 0;
            work->accel          = 0;
            work->turnRate       = 0;
            reactionEnemy        = task->spawnArg2.pointer;
            soundId              = ((reactionEnemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << ACTOR_03800_SOUND_INSTANCE_SHIFT) | ACTOR_03800_SOUND_REACTION;
            reactionPan          = (s8)worldCoordGetOriginAudioPan(rootCoord);
            sndEvtRequestScriptStart(soundId, reactionPan, (s8)worldCoordGetOriginAudioDepth(rootCoord));
            break;
        case ACTOR_03800_FLINCH_WAIT:
            work->timer -= 1;
            if (work->timer > 0) {
                break;
            }
            if (work->overturned == 0) {
                if (work->inBuildup == 0) {
                    work->action     = ACTOR_03800_ACTION_CHARGE;
                    work->actionStep = ACTOR_03800_ACTION_BEGIN;
                } else {
                    work->action     = ACTOR_03800_ACTION_BUILDUP;
                    work->actionStep = ACTOR_03800_ACTION_BEGIN;
                    work->timer      = 0;
                }
                if (work->stepSoundTimer == 0) {
                    work->stepSoundTimer = 1;
                }
            } else {
                work->action     = ACTOR_03800_ACTION_KNOCKED_OVER;
                work->actionStep = ACTOR_03800_KNOCKED_OVER_WAIT;
            }
            break;
    }
}

/// Holds movement during buildup, restarting a flinch every 3..10 ticks.
///
/// On expiry, resumes charge or the overturned wait on the floor, or drops
/// from a perch. The overturned wait is recalculated from the remaining HP.
static void _actor03800ActionBuildup(Task* task)
{
    enum {
        ACTOR_03800_BUILDUP_FLINCH_DELAY_BASE        = 3,
        ACTOR_03800_BUILDUP_FLINCH_DELAY_JITTER_MASK = 7,
    };
    Enemy*           enemy;
    Enemy*           buildupEnemy;
    _Actor03800Work* work;

    work           = task->work;
    enemy          = task->spawnArg2.pointer;
    work->turnRate = 0;
    work->speed    = 0;
    work->accel    = 0;
    work->timer--;
    if (work->timer <= 0) {
        // Restart the flinch periodically while the shared buildup reaction remains active.
        if (work->overturned == 0) {
            work->anim = ACTOR_03800_ANIM_FLINCH;
        } else {
            work->anim = ACTOR_03800_ANIM_FLINCH_OVERTURNED;
        }
        work->playingAnim = ACTOR_03800_ANIM_STAND; // not the animation asked for, so it starts afresh
        gRandomLcgState   = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
        work->timer       = ((gRandomLcgState >> 16) & ACTOR_03800_BUILDUP_FLINCH_DELAY_JITTER_MASK) + ACTOR_03800_BUILDUP_FLINCH_DELAY_BASE;
    }
    buildupEnemy = task->spawnArg2.pointer;
    if (damageTickEnemyBuildup(buildupEnemy) != 0) {
        work->inBuildup = 0;
        if (work->mode == ACTOR_03800_MODE_FLOOR) {
            if (work->overturned == 0) {
                work->action     = ACTOR_03800_ACTION_CHARGE;
                work->actionStep = ACTOR_03800_ACTION_BEGIN;
                if (work->stepSoundTimer == 0) {
                    work->stepSoundTimer = 1;
                }
            } else {
                work->action          = ACTOR_03800_ACTION_KNOCKED_OVER;
                work->actionStep      = ACTOR_03800_KNOCKED_OVER_WAIT;
                work->overturnedTimer = ((Actor03800_D05F44.hpMax - enemy->hp) * 100 / Actor03800_D05F44.hpMax) * ACTOR_03800_OVERTURNED_WAIT_PER_HP_PERCENT + ACTOR_03800_OVERTURNED_WAIT_BASE;
            }
        } else {
            work->action     = ACTOR_03800_ACTION_DROP;
            work->actionStep = ACTOR_03800_ACTION_BEGIN;
        }
    }
}

/// Hands a floor death to the death task state, or lands a perched corpse first.
///
/// After landing, restores the saved model root beneath the view coordinate
/// and starts the overturned death animation before handing over.
static void _actor03800ActionDie(Task* task)
{
    enum {
        ACTOR_03800_DIE_FALL                 = 1,
        ACTOR_03800_DIE_START_ANIM           = 2,
        ACTOR_03800_DIE_RESTORE_ROOT         = 3,
        ACTOR_03800_DIE_WAIT                 = 4,
        ACTOR_03800_PERCHED_DEATH_WAIT_TICKS = 15,
        ACTOR_03800_TASK_DEATH               = 2,
    };
    SVECTOR          landingAngles;
    MATRIX           landingRotation;
    _Actor03800Work* work;
    GfxCoord*        modelRoot;
    s16              actionStep;

    work       = task->work;
    actionStep = work->actionStep;
    modelRoot  = task->extra.tmd->coords;
    switch (actionStep) {
        case ACTOR_03800_ACTION_BEGIN:
            if (work->mode == ACTOR_03800_MODE_FLOOR) {
                task->state      = ACTOR_03800_TASK_DEATH;
                work->actionStep = ACTOR_03800_ACTION_BEGIN;
                return;
            }
            work->fallSpeed       = ACTOR_03800_FLOOR_FALL_SPEED;
            work->shadowShade     = ACTOR_03800_SHADOW_SHADE;
            work->actionStep      = ACTOR_03800_DIE_FALL;
            work->gridBody.flags |= (WORLD_COLLISION_BODY_FLOOR_QUERY | WORLD_COLLISION_BODY_GRID_ENABLED);
            return;
        case ACTOR_03800_DIE_FALL:
            if (work->landed != 0) {
                work->actionStep = ACTOR_03800_DIE_START_ANIM;
                return;
            }
        default:
            return;
        case ACTOR_03800_DIE_START_ANIM:
            work->anim       = ACTOR_03800_ANIM_DIE_OVERTURNED;
            work->actionStep = ACTOR_03800_DIE_RESTORE_ROOT;
            return;
        // Reattach at the fallen perch position without applying the perch pitch.
        case ACTOR_03800_DIE_RESTORE_ROOT:
            landingAngles.vx = 0;
            landingAngles.vy = work->yaw;
            landingAngles.vz = 0;
            RotMatrix(&landingAngles, &landingRotation);
            gte_SetRotMatrix(&work->savedRootMtx);
            gte_ldclmv(&landingRotation.m[0][0]);
            gte_rtir();
            gte_stclmv(&work->savedRootMtx.m[0][0]);
            gte_ldclmv(&landingRotation.m[0][1]);
            gte_rtir();
            gte_stclmv(&work->savedRootMtx.m[0][1]);
            gte_ldclmv(&landingRotation.m[0][2]);
            gte_rtir();
            gte_stclmv(&work->savedRootMtx.m[0][2]);
            modelRoot->parent       = &gGfxViewCoord;
            modelRoot->coord        = work->savedRootMtx;
            modelRoot->coord.t[0]   = work->perchCoord.coord.t[0];
            modelRoot->coord.t[1]   = work->perchCoord.coord.t[1];
            modelRoot->coord.t[2]   = work->perchCoord.coord.t[2];
            modelRoot->composeStamp = GRAPHICS_COORD_DIRTY;
            actorRenderComposeCoord(modelRoot);
            work->timer      = ACTOR_03800_PERCHED_DEATH_WAIT_TICKS;
            work->rootCoord  = modelRoot;
            work->actionStep = ACTOR_03800_DIE_WAIT;
            return;
        case ACTOR_03800_DIE_WAIT:
            work->mode = ACTOR_03800_MODE_FLOOR;
            work->timer--;
            if (work->timer <= 0) {
                work->deathAnimChosen = 1;
                work->actionStep      = ACTOR_03800_ACTION_BEGIN;
                task->state           = ACTOR_03800_TASK_DEATH;
            }
            break;
    }
}

/// Looks around on a wall perch and drops 15..30 ticks after a combat signal or touch.
static void _actor03800ActionWallWait(Task* task)
{
    enum {
        ACTOR_03800_WALL_WAIT_TURN = 1,
    };
    _Actor03800Work* work;
    s32              randomBits;
    s32              yawOffset;

    work = task->work;

    switch (work->actionStep) {
        case ACTOR_03800_ACTION_BEGIN:
            work->turnRate = 0;
            work->speed    = 0;
            work->accel    = 0;
            work->timer--;
            if (work->timer > 0) {
                break;
            }

            gRandomLcgState  = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            work->actionStep = ACTOR_03800_WALL_WAIT_TURN;
            randomBits       = gRandomLcgState >> 16;
            yawOffset        = randomBits & (ACTOR_TRANSFORM_ANGLE_TURN / 4 - 1);
            if (!(randomBits & ACTOR_03800_RANDOM_TURN_POSITIVE_BIT)) {
                yawOffset = -yawOffset;
            }

            work->anim           = ACTOR_03800_ANIM_WALK;
            work->stepSoundTimer = 1;
            work->targetYaw      = (work->yaw + yawOffset) & ACTOR_TRANSFORM_ANGLE_MASK;
            break;

        case ACTOR_03800_WALL_WAIT_TURN:
            work->turnRate = 0x1E;
            work->speed    = 0;
            work->accel    = 0;
            if (work->yaw == work->targetYaw) {
                gRandomLcgState      = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                work->actionStep     = ACTOR_03800_ACTION_BEGIN;
                work->anim           = ACTOR_03800_ANIM_STAND;
                work->stepSoundTimer = 0;
                work->timer          = (gRandomLcgState >> 16 & ACTOR_03800_IDLE_WAIT_JITTER_MASK) + ACTOR_03800_IDLE_WAIT_BASE;
            }
            break;
    }

    if ((gSceneCombatState.signals.bytes.actionFlags & (SCENE_COMBAT_ACTION_NOISE | SCENE_COMBAT_ACTION_PE_CAST_OTHER)) || work->attackTouched != 0) {
        gRandomLcgState  = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
        work->action     = ACTOR_03800_ACTION_DROP;
        work->actionStep = ACTOR_03800_ACTION_BEGIN;
        work->alerted    = 1;
        work->turnRate   = 0;
        work->speed      = 0;
        work->accel      = 0;
        work->timer      = (gRandomLcgState >> 16 & ACTOR_03800_DROP_DELAY_JITTER_MASK) + ACTOR_03800_DROP_DELAY_BASE;
    }
}

/// Looks around on a ceiling perch and drops 15..30 ticks after a combat signal or touch.
static void _actor03800ActionCeilingWait(Task* task)
{
    enum {
        ACTOR_03800_CEILING_WAIT_TURN = 1,
    };
    _Actor03800Work* work;
    s32              randomBits;
    s32              yawOffset;

    work = task->work;

    switch (work->actionStep) {
        case ACTOR_03800_ACTION_BEGIN:
            work->turnRate = 0;
            work->speed    = 0;
            work->accel    = 0;
            work->timer--;
            if (work->timer > 0) {
                break;
            }

            gRandomLcgState  = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            work->actionStep = ACTOR_03800_CEILING_WAIT_TURN;
            randomBits       = gRandomLcgState >> 16;
            yawOffset        = randomBits & (ACTOR_TRANSFORM_ANGLE_TURN / 4 - 1);
            if (!(randomBits & ACTOR_03800_RANDOM_TURN_POSITIVE_BIT)) {
                yawOffset = -yawOffset;
            }

            work->anim           = ACTOR_03800_ANIM_WALK;
            work->stepSoundTimer = 1;
            work->targetYaw      = (work->yaw + yawOffset) & ACTOR_TRANSFORM_ANGLE_MASK;
            break;

        case ACTOR_03800_CEILING_WAIT_TURN:
            work->turnRate = 0x1E;
            work->speed    = 0;
            work->accel    = 0;
            if (work->yaw == work->targetYaw) {
                gRandomLcgState      = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                work->actionStep     = ACTOR_03800_ACTION_BEGIN;
                work->anim           = ACTOR_03800_ANIM_STAND;
                work->stepSoundTimer = 0;
                work->timer          = (gRandomLcgState >> 16 & ACTOR_03800_IDLE_WAIT_JITTER_MASK) + ACTOR_03800_IDLE_WAIT_BASE;
            }
            break;
    }

    if ((gSceneCombatState.signals.bytes.actionFlags & (SCENE_COMBAT_ACTION_NOISE | SCENE_COMBAT_ACTION_PE_CAST_OTHER)) || work->attackTouched != 0) {
        gRandomLcgState  = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
        work->action     = ACTOR_03800_ACTION_DROP;
        work->actionStep = ACTOR_03800_ACTION_BEGIN;
        work->alerted    = 1;
        work->turnRate   = 0;
        work->speed      = 0;
        work->accel      = 0;
        work->timer      = (gRandomLcgState >> 16 & ACTOR_03800_DROP_DELAY_JITTER_MASK) + ACTOR_03800_DROP_DELAY_BASE;
    }
}

/// Waits, falls from a perch and restores the floor root before starting to get up.
///
/// Requires the saved model transform and perch coordinate from placement.
/// Each call reserves and releases one Euler-turn scratch block; landing
/// disables attack pairing until the get-up action restores it.
static void _actor03800ActionDrop(Task* task)
{
    enum {
        ACTOR_03800_DROP_FALL         = 1,
        ACTOR_03800_DROP_START_ANIM   = 2,
        ACTOR_03800_DROP_RESTORE_ROOT = 3,
        ACTOR_03800_DROP_FINISH       = 4,
        ACTOR_03800_DROP_FALL_SPEED   = 256,
    };
    Enemy*                 enemy;
    _Actor03800Work*       work;
    GfxCoord*              modelRoot;
    ActorEulerTurnScratch* scratch;
    Enemy*                 soundEnemy;
    s32                    soundId;
    s32                    impactPan;

    scratch   = SCRATCH_STACK_RESERVE_BLOCK(ActorEulerTurnScratch);
    work      = task->work;
    modelRoot = task->extra.tmd->coords;
    enemy     = task->spawnArg2.pointer;
    switch (work->actionStep) {
        case ACTOR_03800_ACTION_BEGIN:
            work->timer--;
            if (work->timer <= 0) {
                work->actionStep = ACTOR_03800_DROP_FALL;
            }
            break;
        case ACTOR_03800_DROP_FALL:
            work->fallSpeed       = ACTOR_03800_DROP_FALL_SPEED;
            work->shadowShade     = ACTOR_03800_SHADOW_SHADE;
            work->gridBody.flags |= (WORLD_COLLISION_BODY_FLOOR_QUERY | WORLD_COLLISION_BODY_GRID_ENABLED);
            if (work->landed != 0) {
                work->landed            = 0;
                work->actionStep        = ACTOR_03800_DROP_START_ANIM;
                work->fallSpeed         = ACTOR_03800_FLOOR_FALL_SPEED;
                work->attackBody.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
                soundEnemy              = task->spawnArg2.pointer;
                soundId                 = ((soundEnemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << ACTOR_03800_SOUND_INSTANCE_SHIFT) | ACTOR_03800_SOUND_IMPACT;
                impactPan               = (s8)worldCoordGetOriginAudioPan(modelRoot);
                sndEvtRequestScriptStart(soundId, impactPan, (s8)worldCoordGetOriginAudioDepth(modelRoot));
            }
            break;
        case ACTOR_03800_DROP_START_ANIM:
            work->anim       = ACTOR_03800_ANIM_LAND;
            work->actionStep = ACTOR_03800_DROP_RESTORE_ROOT;
            break;
        // Move the animated root back beneath the view at the landing position.
        case ACTOR_03800_DROP_RESTORE_ROOT:
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
            modelRoot->coord        = work->savedRootMtx;
            modelRoot->coord.t[0]   = work->perchCoord.coord.t[0];
            modelRoot->coord.t[1]   = work->perchCoord.coord.t[1];
            modelRoot->coord.t[2]   = work->perchCoord.coord.t[2];
            modelRoot->parent       = &gGfxViewCoord;
            modelRoot->composeStamp = GRAPHICS_COORD_DIRTY;
            actorRenderComposeCoord(modelRoot);
            work->actionStep      = ACTOR_03800_DROP_FINISH;
            work->rootCoord       = modelRoot;
            work->overturnedTimer = ((Actor03800_D05F44.hpMax - enemy->hp) * 100 / Actor03800_D05F44.hpMax) * ACTOR_03800_OVERTURNED_WAIT_PER_HP_PERCENT + ACTOR_03800_OVERTURNED_WAIT_BASE;
            break;
        case ACTOR_03800_DROP_FINISH:
            work->mode = ACTOR_03800_MODE_FLOOR;
            work->timer--;
            if (work->timer <= 0) {
                work->action     = ACTOR_03800_ACTION_GET_UP;
                work->actionStep = ACTOR_03800_ACTION_BEGIN;
            }
            break;
    }
    SCRATCH_STACK_RELEASE_BLOCK(ActorEulerTurnScratch);
}

/// Leaps along the facing away from the player or wall, then resumes wandering.
///
/// The player offset and root must share their parent frame. A dot product
/// against the root's +Z axis selects backward or forward motion. Attack
/// pairing stays disabled until the leap animation reaches its finish tick.
static void _actor03800ActionLeapAway(Task* task)
{
    enum {
        ACTOR_03800_LEAP_BACKWARD      = 1,
        ACTOR_03800_LEAP_FORWARD       = 2,
        ACTOR_03800_LEAP_RECOVER       = 3,
        ACTOR_03800_LEAP_FIRST_TICK    = 8,
        ACTOR_03800_LEAP_LAST_TICK     = 16,
        ACTOR_03800_LEAP_RECOVERY_TICK = 17,
        ACTOR_03800_LEAP_FINISH_TICK   = 20,
        ACTOR_03800_LEAP_SPEED         = 125,
    };
    _Actor03800Work* work;
    GfxCoord*        rootCoord;
    VECTOR           toPlayer;

    work      = task->work;
    rootCoord = work->rootCoord;

    switch (work->actionStep) {
        case ACTOR_03800_ACTION_BEGIN:
            toPlayer.vx = gPlayerStatus.coordMtx->t[0] - rootCoord->coord.t[0];
            toPlayer.vy = 0;
            toPlayer.vz = gPlayerStatus.coordMtx->t[2] - rootCoord->coord.t[2];
            work->anim  = ACTOR_03800_ANIM_LEAP;
            if (work->hitWall != 0) {
                work->hitWall    = 0;
                work->actionStep = ACTOR_03800_LEAP_BACKWARD;
            } else {
                work->actionStep =
                    ((toPlayer.vx * rootCoord->coord.m[0][2]) + (toPlayer.vz * rootCoord->coord.m[2][2]) > 0) ? ACTOR_03800_LEAP_BACKWARD : ACTOR_03800_LEAP_FORWARD;
            }
            work->attackBody.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
            break;

        case ACTOR_03800_LEAP_BACKWARD:
            if (work->animFrame >= ACTOR_03800_LEAP_FIRST_TICK && work->animFrame <= ACTOR_03800_LEAP_LAST_TICK) {
                work->speed = -ACTOR_03800_LEAP_SPEED;
                break;
            }
            work->speed = 0;
            if (work->animFrame >= ACTOR_03800_LEAP_RECOVERY_TICK) {
                work->actionStep = ACTOR_03800_LEAP_RECOVER;
            }
            break;

        case ACTOR_03800_LEAP_FORWARD:
            if (work->animFrame >= ACTOR_03800_LEAP_FIRST_TICK && work->animFrame <= ACTOR_03800_LEAP_LAST_TICK) {
                work->speed = ACTOR_03800_LEAP_SPEED;
                break;
            }
            work->speed = 0;
            if (work->animFrame >= ACTOR_03800_LEAP_RECOVERY_TICK) {
                work->actionStep = ACTOR_03800_LEAP_RECOVER;
            }
            break;

        case ACTOR_03800_LEAP_RECOVER:
            if (work->animFrame >= ACTOR_03800_LEAP_FINISH_TICK) {
                work->action            = ACTOR_03800_ACTION_WANDER;
                work->actionStep        = ACTOR_03800_ACTION_BEGIN;
                work->shadowShade       = ACTOR_03800_SHADOW_SHADE;
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

/// Starts a requested clip with its table-defined blend, or advances the five driven parts.
///
/// Requires a live rig initialized by `_actor03800Spawn`. Changed requests
/// must name a clip in 1..11; the initial unchanged zero keeps the spawn's
/// stand playback. Slots 1..5 map to model parts and tracks; slot 0 is untouched.
/// A changed request resets `animFrame` to zero and captures each old pose for
/// the blend. Unchanged requests advance playback and count one task tick.
static inline void _actor03800TickAnim(Task* task)
{
    _Actor03800Work* work;
    s32              slotIndex;
    s32              blendFrames;

    work = task->work;
    if (work->anim != work->playingAnim) {
        work->playingAnim = work->anim;
        work->animFrame   = 0;
        blendFrames       = Actor03800_D05F90[work->anim];
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

/// Recolours the actor from the lighting at its root coordinate's world position.
static inline void _actor03800UpdateColorAtRoot(Task* arg0)
{
    GfxCoord* coord;
    VECTOR    vec;

    coord  = ((_Actor03800Work*)arg0->work)->rootCoord;
    vec.vx = coord->workm.t[0];
    vec.vy = coord->workm.t[1];
    vec.vz = coord->workm.t[2];
    worldCoordUpdateActorColor(arg0->spawnArg2.pointer, &vec, 0, 0);
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
            worldCoordSetActorColorMode(arg0, ENEMY_COLOR_WEIGHTED);
            sceneReleaseBattleRefWithRewards(arg1, 0x26);
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
                effectSpawn(EFFECT_CORPSE_BURN, coord, 2, NULL);
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
    eff = effectSpawn(EFFECT_BURST_BODY_PART_BANK4, actor->extra.tmd->coords + 3, 0x100, NULL);
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
    layout = areaGetVariant(&key);

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
            _actor03800ActionIdle(arg0);
            break;
        case ACTOR_03800_ACTION_WANDER:
            _actor03800ActionWander(arg0);
            break;
        case ACTOR_03800_ACTION_CHARGE:
            _actor03800ActionCharge(arg0);
            break;
        case ACTOR_03800_ACTION_KNOCKED_OVER:
            _actor03800ActionKnockedOver(arg0);
            break;
        case ACTOR_03800_ACTION_GET_UP:
            _actor03800ActionGetUp(arg0);
            break;
        case ACTOR_03800_ACTION_FLINCH:
            _actor03800ActionFlinch(arg0);
            break;
        case ACTOR_03800_ACTION_BUILDUP:
            _actor03800ActionBuildup(arg0);
            break;
        case ACTOR_03800_ACTION_DIE:
            _actor03800ActionDie(arg0);
            break;
        case ACTOR_03800_ACTION_WALL_WAIT:
            _actor03800ActionWallWait(arg0);
            break;
        case ACTOR_03800_ACTION_CEILING_WAIT:
            _actor03800ActionCeilingWait(arg0);
            break;
        case ACTOR_03800_ACTION_DROP:
            _actor03800ActionDrop(arg0);
            break;
        case ACTOR_03800_ACTION_SHRINE_WAIT:
            _actor03800ActionShrineWait(arg0);
            break;
        case ACTOR_03800_ACTION_LEAP_AWAY:
            _actor03800ActionLeapAway(arg0);
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

/// Rights the actor at animation tick 30 and resumes charging from tick 58.
static void _actor03800ActionGetUp(Task* task)
{
    enum {
        ACTOR_03800_GET_UP_PLAY         = 1,
        ACTOR_03800_GET_UP_UPRIGHT_TICK = 30,
        ACTOR_03800_GET_UP_CHARGE_TICK  = 58,
    };
    _Actor03800Work* work = task->work;
    s32              actionStep;

    actionStep = work->actionStep;
    switch (actionStep) {
        case ACTOR_03800_ACTION_BEGIN:
            work->anim       = ACTOR_03800_ANIM_GET_UP;
            work->actionStep = ACTOR_03800_GET_UP_PLAY;
            break;
        case ACTOR_03800_GET_UP_PLAY:
            if (work->animFrame == ACTOR_03800_GET_UP_UPRIGHT_TICK) {
                work->overturned = 0;
            }
            if (work->animFrame >= ACTOR_03800_GET_UP_CHARGE_TICK) {
                work->action     = ACTOR_03800_ACTION_CHARGE;
                work->actionStep = ACTOR_03800_ACTION_BEGIN;
                if (work->stepSoundTimer == 0) {
                    work->stepSoundTimer = 1;
                }
                work->attackBody.flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;
            }
            break;
    }
}

/// Follows shrine visibility and release phases, wandering after a 90-tick release delay.
static void _actor03800ActionShrineWait(Task* task)
{
    enum {
        ACTOR_03800_SHRINE_RELEASE_TICKS = 90,
    };
    TmdObject*       model;
    Enemy*           enemy;
    _Actor03800Work* work;

    work  = task->work;
    model = task->extra.tmd;
    enemy = task->spawnArg2.pointer;
    switch (gSceneCombatState.shrineEnemyPhase) {
        case SCENE_COMBAT_SHRINE_HIDDEN:
            model->flags                  = (TMD_OBJECT_SKIP_ACTIVE_DRAW | TMD_OBJECT_SKIP_AUTO_BUFFER);
            enemy->node.state.parts.flags = WORLD_TARGET_NOT_LOCKABLE;
            return;
        case SCENE_COMBAT_SHRINE_REVEALED:
            model->flags            = 0;
            work->hitBody.flags    |= WORLD_COLLISION_BODY_PAIR_ENABLED;
            work->gridBody.flags   |= (WORLD_COLLISION_BODY_FLOOR_QUERY | WORLD_COLLISION_BODY_GRID_ENABLED);
            work->attackBody.flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;
            sceneEngageBattle(1);
            work->fallSpeed   = ACTOR_03800_FLOOR_FALL_SPEED;
            work->timer       = ACTOR_03800_SHRINE_RELEASE_TICKS;
            work->mode        = ACTOR_03800_MODE_FLOOR;
            work->shadowShade = ACTOR_03800_SHADOW_SHADE;
            return;
        case SCENE_COMBAT_SHRINE_RELEASED:
            if (--work->timer <= 0) {
                work->action     = ACTOR_03800_ACTION_WANDER;
                work->actionStep = ACTOR_03800_ACTION_BEGIN;
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
    worldCoordUpdateActorColor(arg0->spawnArg2.pointer, &vec, 0, 0);
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
    scratch->scale.vx = ONE;
    scratch->scale.vy = work->deathScaleY;
    scratch->scale.vz = ONE;
    coord->coord      = work->savedRootMtx;
    gfxSetRotIdentity(&scratch->matrix);
    ScaleMatrix(&scratch->matrix, &scratch->scale);
    MulMatrix(&coord->coord, &scratch->matrix);
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
    SCRATCH_STACK_RELEASE_BLOCK(ActorScaleScratch);
}
