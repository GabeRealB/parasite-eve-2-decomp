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

/// Burst selection domain and the stage at which the hidden model releases its buffer.
enum {
    ACTOR_03800_BURST_VARIANT_COUNT = 5,
    ACTOR_03800_BURST_PENDING       = 1,
    ACTOR_03800_BURST_SPAWN         = 2,
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
static void _actor03800ApplyReactions(Task* task);
static void _actor03800ProcessContacts(Task* task);
static void _actor03800TurnTowardTargetYaw(Task* task);
static void _actor03800MoveRoot(Task* task);
static void _actor03800Death(Enemy* enemy, Task* task);
static void _actor03800SpawnBurstFragments(Task* task);
static void _actor03800SpawnBurstFragment(Task* task, u32 variant);
static void _actor03800Tick(Enemy* enemy, Task* task);
static void _actor03800DispatchAction(Task* task);
static void _actor03800ActionGetUp(Task* task);
static void _actor03800ActionShrineWait(Task* task);
static void _actor03800TickFootstepSound(Task* task);
static void _actor03800Animate(Task* task);
static void _actor03800UpdateColor(Task* task);
static void _actor03800DrawGroundShadow(Task* task);
static void _actor03800SquashCorpse(Task* task);

/// State handlers `_actor03800Task` dispatches, indexed by the task's state
/// (`Task::state`): the spawn state that allocates the work block and
/// moves to state 1, the per-frame tick, and the state-2 handler the tick hands
/// over to, which carries the death sequence.
static const EnemyTaskFuncTable3 Actor03800_D00004 = {
    {
        _actor03800Spawn,
        _actor03800Tick,
        _actor03800Death,
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
static void         _actor03800Task(Task* task);

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

TaskDesc Actor03800_D05F54 = { { { TASK_BODY_TMD, 96 } }, _actor03800Task, { .model = &_gActor03800BlackBeetleBody } };

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

/// Applies pending buildup and damage-over-time reactions to the current action.
///
/// Requires live enemy parameters and spawned work. Buildup holds a floor actor
/// or makes a perched actor drop; poison HP loss is narrowed to a signed halfword
/// before choosing flinch or death. The actor owns clearing expired reactions.
static void _actor03800ApplyReactions(Task* task)
{
    enum { ACTOR_03800_REACTION_DROP_FINISHED = 4 };
    Enemy*           enemy;
    _Actor03800Work* work;
    s16              pulseDamage;
    u16              remainingHp;
    u8               reactionFlags;

    enemy         = task->spawnArg2.pointer;
    reactionFlags = enemy->reactionFlags;
    work          = task->work;
    if (reactionFlags & ENEMY_REACTION_BUILDUP) {
        if (work->mode == ACTOR_03800_MODE_FLOOR) {
            enemy->reactionFlags = reactionFlags & ENEMY_REACTION_BUILDUP_CLEAR;
            work->action         = ACTOR_03800_ACTION_BUILDUP;
            work->actionStep     = ACTOR_03800_ACTION_BEGIN;
            work->timer          = 0;
            work->inBuildup      = 1;
        } else if ((work->action != ACTOR_03800_ACTION_DROP) || (work->actionStep >= ACTOR_03800_REACTION_DROP_FINISHED)) {
            work->action     = ACTOR_03800_ACTION_DROP;
            work->actionStep = ACTOR_03800_ACTION_BEGIN;
        }
    }
    // Keep pulse and remaining HP narrowing before the fatal-hit test.
    if (enemy->reactionFlags & ENEMY_REACTION_DAMAGE_OVER_TIME_BITS) {
        pulseDamage = damageTickEnemyDamageOverTime(enemy);
        if (pulseDamage != 0) {
            worldTargetAddReadoutAmount(&enemy->node, pulseDamage, 0);
            remainingHp = enemy->hp - pulseDamage;
            enemy->hp   = remainingHp;
            if ((s16)remainingHp <= 0) {
                work->action = ACTOR_03800_ACTION_DIE;
            } else {
                work->action = ACTOR_03800_ACTION_FLINCH;
            }
            work->actionStep = ACTOR_03800_ACTION_BEGIN;
        }
        if (damageIsEnemyDamageOverTimeExpired(enemy) != 0) {
            enemy->reactionFlags &= ENEMY_REACTION_DAMAGE_OVER_TIME_CLEAR;
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

/// Consumes room, hit and attack contacts to correct position and choose reactions.
///
/// Requires the three initialized work-owned contact arrays and a composed root.
/// Positions/distances use game units; room pushback is signed 16.16 and normals
/// are Q12. Applies at most the deepest body overlap on X/Z while on the floor.
/// Attack keys must contain valid damage-table rows and a player/companion selector.
/// Clears all three arrays and releases its temporary scratch block before return.
static void _actor03800ProcessContacts(Task* task)
{
    enum {
        ACTOR_03800_FLOOR_NORMAL_Y                     = -3546,
        ACTOR_03800_CONTACT_KIND_SHIFT                 = 16,
        ACTOR_03800_ATTACKER_SELECTOR_SHIFT            = 7,
        ACTOR_03800_ATTACK_ATTACHMENT_BIT              = 0x8000,
        ACTOR_03800_HIT_RESPONSE_FLINCH                = 0,
        ACTOR_03800_HIT_RESPONSE_KNOCK_OVER            = 1,
        ACTOR_03800_HIT_RESPONSE_DIE                   = 2,
        ACTOR_03800_HIT_ATTRIBUTE_BURST                = 4,
        ACTOR_03800_HIT_ATTRIBUTE_KNOCK_OVER           = 5,
        ACTOR_03800_HIT_ATTRIBUTE_FORCE_BUILDUP        = 8,
        ACTOR_03800_HIT_ATTRIBUTE_KNOCK_OVER_ALTERNATE = 9,
        ACTOR_03800_CRITICAL_MULTIPLIER                = 4,
        ACTOR_03800_OVERTURNED_MULTIPLIER              = 3,
        ACTOR_03800_DIRECTION_FRACTION_BITS            = 12,
        ACTOR_03800_CRITICAL_EFFECT_NORMAL             = 0,
        ACTOR_03800_CRITICAL_EFFECT_OVERTURNED         = 4,
        ACTOR_03800_HIT_EFFECT_ANCHOR_PART             = 3,
    };
    _Actor03800Work*         work;
    ActorOverlapPushScratch* scratch;
    Enemy*                   enemy;
    GfxCoord*                rootCoord;
    GfxCoord*                attackerRoot;
    s32                      deepestOverlap;
    s32                      hitResponse;
    s32                      queryResult;
    s32                      contactIndex;
    s32                      overlapDepth;
    s32                      clampedDepth;
    s32                      dx;
    s32                      dy;
    s32                      dz;
    u32                      lastEffectKey;
    u32                      contactKey;
    u32                      hitKey;
    u32                      damage;

    deepestOverlap = 0;
    hitResponse    = ACTOR_03800_HIT_RESPONSE_FLINCH;
    lastEffectKey  = 0;
    work           = task->work;
    SCRATCH_STACK_RESERVE_BLOCK(ActorOverlapPushScratch);
    scratch   = SCRATCH_STACK_CURSOR(ActorOverlapPushScratch);
    rootCoord = work->rootCoord;
    enemy     = task->spawnArg2.pointer;
    // Resolve room correction before processing the previous pass's hit contacts.
    queryResult = worldCollisionResolvePushback(work->gridContacts, &scratch->delta, ARRAY_SIZE(work->gridContacts), NULL);
    if (queryResult != 0) {
        for (contactIndex = 0; contactIndex < ARRAY_SIZE(work->gridContacts); contactIndex++) {
            if ((work->gridContacts[contactIndex].key.value & WORLD_COLLISION_CONTACT_KIND_MASK) == WORLD_COLLISION_CONTACT_GRID) {
                if (work->gridContacts[contactIndex].response.direction.vy >= ACTOR_03800_FLOOR_NORMAL_Y) {
                    if (work->overturned == 0) {
                        work->hitWall = 1;
                        break;
                    }
                } else {
                    work->landed = 1;
                }
            }
        }
        switch (queryResult) {
            case WORLD_COLLISION_PUSHBACK_NO_GRID_HIT:
                break;
            case WORLD_COLLISION_PUSHBACK_GRID_HIT:
                rootCoord->coord.t[0] += scratch->delta.fixed.vx.halves.integer;
                rootCoord->coord.t[1] += scratch->delta.fixed.vy.halves.integer;
                rootCoord->coord.t[2] += scratch->delta.fixed.vz.halves.integer;
                break;
            case WORLD_COLLISION_PUSHBACK_OPPOSED:
                rootCoord->coord.t[0] = work->prevPos.vx;
                rootCoord->coord.t[1] = work->prevPos.vy;
                rootCoord->coord.t[2] = work->prevPos.vz;
                break;
        }
    }
    worldCollisionClearContacts(work->gridContacts);
    if (work->hitCooldown != 0) {
        if (--work->hitCooldown <= 0) {
            work->hitCooldown = 0;
        }
    }
    // Attack contacts select reactions; body contacts retain only the deepest push.
    for (contactIndex = 0; contactIndex < ARRAY_SIZE(work->hitContacts); contactIndex++) {
        contactKey = work->hitContacts[contactIndex].key.value;
        switch (contactKey >> ACTOR_03800_CONTACT_KIND_SHIFT) {
            case 0:
                break;
            case WORLD_COLLISION_CONTACT_ATTACK >> ACTOR_03800_CONTACT_KIND_SHIFT:
                if (work->hitCooldown == 0) {
                    attackerRoot             = gPlayerActorTasks[(contactKey >> ACTOR_03800_ATTACKER_SELECTOR_SHIFT) & 1]->extra.tmd->coords;
                    scratch->delta.vector.vx = attackerRoot->coord.t[0] - rootCoord->coord.t[0];
                    scratch->delta.vector.vy = attackerRoot->coord.t[1] - rootCoord->coord.t[1];
                    scratch->delta.vector.vz = attackerRoot->coord.t[2] - rootCoord->coord.t[2];
                    damage                   = damageComputePlayerAttack(work->hitContacts[contactIndex].key.value, SquareRoot0((scratch->delta.vector.vx * scratch->delta.vector.vx) + (scratch->delta.vector.vy * scratch->delta.vector.vy) + (scratch->delta.vector.vz * scratch->delta.vector.vz)), 0, 0);
                    if (work->overturned == 0) {
                        if (damageRollCriticalHit(enemy, work->hitContacts[contactIndex].key.value, 0) != 0) {
                            damage *= ACTOR_03800_CRITICAL_MULTIPLIER;
                            effectSpawn(EFFECT_CRITICAL_HIT, rootCoord, ACTOR_03800_CRITICAL_EFFECT_NORMAL, NULL);
                        }
                    } else if (!(work->hitContacts[contactIndex].key.value & ACTOR_03800_ATTACK_ATTACHMENT_BIT) && (damage != 0)) {
                        damage *= ACTOR_03800_OVERTURNED_MULTIPLIER;
                        effectSpawn(EFFECT_CRITICAL_HIT, rootCoord, ACTOR_03800_CRITICAL_EFFECT_OVERTURNED, NULL);
                    }
                    worldTargetAddReadoutAmount(&enemy->node, damage, 0);
                    damageAccumulateLifeDrainHp(enemy, work->hitContacts[contactIndex].key.value, damage, 0);
                    enemy->hp -= damage;
                    if (enemy->hp <= 0) {
                        hitResponse = ACTOR_03800_HIT_RESPONSE_DIE;
                    }
                    switch (damageGetPlayerAttackReaction(work->hitContacts[contactIndex].key.value) & 0xFFFF) {
                        case DAMAGE_PLAYER_REACTION_NONE:
                        default:
                            break;
                        case DAMAGE_PLAYER_REACTION_POISON:
                            damageTryStartEnemyDamageOverTime(enemy, work->hitContacts[contactIndex].key.value, 0);
                            break;
                        case ACTOR_03800_HIT_ATTRIBUTE_BURST:
                            if (enemy->hp > 0) {
                                if (work->overturned == 0) {
                                    hitResponse = ACTOR_03800_HIT_RESPONSE_KNOCK_OVER;
                                }
                            } else {
                                work->burstStage = ACTOR_03800_BURST_PENDING;
                            }
                            break;
                        case DAMAGE_PLAYER_REACTION_EXPLOSION:
                            if (enemy->hp <= 0) {
                                work->burstStage = ACTOR_03800_BURST_PENDING;
                            } else if (work->overturned == 0) {
                                hitResponse = ACTOR_03800_HIT_RESPONSE_KNOCK_OVER;
                            }
                            break;
                        case ACTOR_03800_HIT_ATTRIBUTE_FORCE_BUILDUP:
                            if (work->overturned == 0 && hitResponse == ACTOR_03800_HIT_RESPONSE_FLINCH) {
                                damageStartEnemyBuildup(enemy, work->hitContacts[contactIndex].key.value, 0);
                            }
                            break;
                        case DAMAGE_PLAYER_REACTION_STAGGER:
                        case DAMAGE_PLAYER_REACTION_BUILDUP:
                        case ACTOR_03800_HIT_ATTRIBUTE_KNOCK_OVER:
                        case ACTOR_03800_HIT_ATTRIBUTE_KNOCK_OVER_ALTERNATE:
                            if (work->overturned == 0 && hitResponse == ACTOR_03800_HIT_RESPONSE_FLINCH) {
                                hitResponse = ACTOR_03800_HIT_RESPONSE_KNOCK_OVER;
                            }
                            break;
                    }
                    switch (hitResponse) {
                        case ACTOR_03800_HIT_RESPONSE_FLINCH:
                            if (work->action != ACTOR_03800_ACTION_DROP && damage != 0) {
                                work->action     = ACTOR_03800_ACTION_FLINCH;
                                work->actionStep = ACTOR_03800_ACTION_BEGIN;
                            }
                            break;
                        case ACTOR_03800_HIT_RESPONSE_KNOCK_OVER:
                            work->action     = ACTOR_03800_ACTION_KNOCKED_OVER;
                            work->actionStep = ACTOR_03800_ACTION_BEGIN;
                            break;
                        case ACTOR_03800_HIT_RESPONSE_DIE:
                            work->action     = ACTOR_03800_ACTION_DIE;
                            work->actionStep = ACTOR_03800_ACTION_BEGIN;
                            break;
                    }
                    hitKey = work->hitContacts[contactIndex].key.value;
                    if (lastEffectKey != hitKey) {
                        lastEffectKey = hitKey;
                        effectSpawnHit(damageGetPlayerAttackEffectId(lastEffectKey), task->extra.tmd->coords + ACTOR_03800_HIT_EFFECT_ANCHOR_PART, NULL, &work->hitEffectArg);
                    }
                    queryResult = damageGetPlayerAttackHitCooldown(work->hitContacts[contactIndex].key.value);
                    if (queryResult > 0) {
                        work->hitCooldown = queryResult;
                    }
                }
                break;
            case WORLD_COLLISION_CONTACT_PLAYER_BODY >> ACTOR_03800_CONTACT_KIND_SHIFT:
                dx                       = rootCoord->workm.t[0] - work->hitContacts[contactIndex].point.vx;
                scratch->delta.vector.vx = dx;
                dy                       = rootCoord->workm.t[1] - work->hitContacts[contactIndex].point.vy;
                scratch->delta.vector.vy = dy;
                dz                       = rootCoord->workm.t[2] - work->hitContacts[contactIndex].point.vz;
                scratch->delta.vector.vz = dz;
                overlapDepth             = work->hitContacts[contactIndex].distance - SquareRoot0((dx * dx) + (dy * dy) + (dz * dz));
                clampedDepth             = overlapDepth;
                if (overlapDepth <= 0) {
                    clampedDepth = 0;
                }
                overlapDepth = clampedDepth;
                _ACTOR03800_KEEP_DEEPEST(deepestOverlap, overlapDepth, scratch);
                break;
            case WORLD_COLLISION_CONTACT_ENEMY_BODY >> ACTOR_03800_CONTACT_KIND_SHIFT:
                dx                       = rootCoord->workm.t[0] - work->hitContacts[contactIndex].point.vx;
                scratch->delta.vector.vx = dx;
                dy                       = rootCoord->workm.t[1] - work->hitContacts[contactIndex].point.vy;
                scratch->delta.vector.vy = dy;
                dz                       = rootCoord->workm.t[2] - work->hitContacts[contactIndex].point.vz;
                scratch->delta.vector.vz = dz;
                overlapDepth             = work->hitContacts[contactIndex].distance - SquareRoot0((dx * dx) + (dy * dy) + (dz * dz));
                clampedDepth             = overlapDepth;
                if (overlapDepth <= 0) {
                    clampedDepth = 0;
                }
                overlapDepth = clampedDepth;
                _ACTOR03800_KEEP_DEEPEST(deepestOverlap, overlapDepth, scratch);
                break;
        }
    }
    if (deepestOverlap > 0 && work->mode == ACTOR_03800_MODE_FLOOR) {
        rootCoord->coord.t[0] += (deepestOverlap * scratch->pushDirection.vx) >> ACTOR_03800_DIRECTION_FRACTION_BITS;
        rootCoord->coord.t[2] += (deepestOverlap * scratch->pushDirection.vz) >> ACTOR_03800_DIRECTION_FRACTION_BITS;
    }
    worldCollisionClearContacts(work->hitContacts);
    // A player or companion touch alerts the actor and selects its escape leap.
    work->attackTouched = 0;
    queryResult         = worldCollisionCountContactsByKind(work->attackContacts, WORLD_COLLISION_CONTACT_PLAYER_BODY);
    if (queryResult != 0) {
        sceneEngageBattle(1);
        work->attackTouched = 1;
        work->alerted       = 1;
        if ((work->mode == ACTOR_03800_MODE_FLOOR) && (work->overturned == 0) && (work->action != ACTOR_03800_ACTION_LEAP_AWAY)) {
            work->action     = ACTOR_03800_ACTION_LEAP_AWAY;
            work->actionStep = ACTOR_03800_ACTION_BEGIN;
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

/// Turns the model root toward the requested yaw by at most the per-tick turn rate.
///
/// Requires a live root and work; targetYaw is a 0..4095 heading and turnRate is
/// nonnegative, in 4096ths of a turn. At exactly half a turn, takes the wrapped
/// branch. Rebuilds an upright rotation without changing translation; the live
/// tick invalidates composition after movement and animation.
static void _actor03800TurnTowardTargetYaw(Task* task)
{
    _Actor03800Work* work;
    GfxCoord*        modelRoot;
    SVECTOR*         yawRotation;
    s32              matrixYaw;
    u16              targetYaw;
    s16              yawDifference;
    s32              absoluteDifference;
    s32              turnRate;
    s32              currentYaw;
    s32              nextYaw;
    s32              wrappedTurnRate;

    yawRotation        = SCRATCH_STACK_RESERVE_BYTES(sizeof(*yawRotation));
    modelRoot          = task->extra.tmd->coords;
    work               = task->work;
    matrixYaw          = ratan2(modelRoot->coord.m[0][2], modelRoot->coord.m[2][2]) & ACTOR_TRANSFORM_ANGLE_MASK;
    targetYaw          = work->targetYaw;
    yawDifference      = targetYaw - matrixYaw;
    absoluteDifference = yawDifference >= 0 ? yawDifference : -yawDifference;

    // Choose the shorter arc; a half-turn keeps the wrapped branch's direction.
    work->yaw = matrixYaw;
    if (absoluteDifference < ACTOR_TRANSFORM_ANGLE_HALF_TURN) {
        turnRate = work->turnRate;
        if (turnRate >= absoluteDifference) {
            work->yaw = targetYaw;
        } else {
            nextYaw = work->yaw;
            if (yawDifference <= 0) {
                nextYaw -= turnRate;
            } else {
                nextYaw += turnRate;
            }
            work->yaw = nextYaw;
        }
    } else {
        turnRate = work->turnRate;
        if (yawDifference > 0 ? turnRate >= ACTOR_TRANSFORM_ANGLE_TURN - yawDifference : turnRate >= ACTOR_TRANSFORM_ANGLE_TURN + yawDifference) {
            work->yaw = work->targetYaw;
        } else {
            wrappedTurnRate = work->turnRate;
            currentYaw      = work->yaw;
            if (yawDifference > 0) {
                work->yaw = currentYaw - wrappedTurnRate;
            } else {
                work->yaw = currentYaw + wrappedTurnRate;
            }
        }
    }
    yawRotation->vx = 0;
    yawRotation->vy = work->yaw;
    yawRotation->vz = 0;
    RotMatrix(yawRotation, &modelRoot->coord);
    SCRATCH_STACK_RELEASE_BYTES(sizeof(*yawRotation));
}

/// Moves the root and updates touch-attack power from the tick's speed.
///
/// Speed/fallSpeed are parent-frame game units per tick; the facing basis is Q12.
/// Acceleration is narrowed to s16 before capping positive speed at 50. Each
/// positive speed unit adds four percent of base power; nonpositive speed uses
/// base power. Saves the pre-move position for unresolved room contacts.
static void _actor03800MoveRoot(Task* task)
{
    enum {
        ACTOR_03800_MAX_ACCELERATED_SPEED   = 50,
        ACTOR_03800_ATTACK_BONUS_PER_SPEED  = 400,
        ACTOR_03800_BASIS_POINTS_ONE        = 10000,
        ACTOR_03800_DIRECTION_FRACTION_BITS = 12,
    };
    _Actor03800Work* work;
    GfxCoord*        rootCoord;
    s16              acceleratedSpeed;
    s16              speed;
    u16              attackPower;
    s32              speedBonusBasisPoints;
    _Actor03800Work* attackWork;

    work       = task->work;
    rootCoord  = work->rootCoord;
    attackWork = work;
    if (work->accel != 0) {
        acceleratedSpeed = work->speed + work->accel;
        work->speed      = acceleratedSpeed;
        if (acceleratedSpeed > ACTOR_03800_MAX_ACCELERATED_SPEED) {
            work->speed = ACTOR_03800_MAX_ACCELERATED_SPEED;
        }
    }
    // Base power 6 and positive speeds up to 125 keep the unmasked power <= 36.
    speed = attackWork->speed;
    if (speed > 0) {
        speedBonusBasisPoints = speed * ACTOR_03800_ATTACK_BONUS_PER_SPEED;
        attackPower           = Actor03800_D05F40[0].power + ((Actor03800_D05F40[0].power * speedBonusBasisPoints) / ACTOR_03800_BASIS_POINTS_ONE);
    } else {
        attackPower = Actor03800_D05F40[0].power;
    }
    attackWork->attackBody.key = (((s16)attackPower | (Actor03800_D05F40[0].reaction << DAMAGE_ATTACK_REACTION_SHIFT)) & 0xFFFF) |
                                 DAMAGE_ATTACK_CATEGORY;
    // Preserve the pre-move position for an opposed room-grid contact.
    work->prevPos.vx       = (s16)rootCoord->coord.t[0];
    work->prevPos.vy       = (s16)rootCoord->coord.t[1];
    work->prevPos.vz       = (s16)rootCoord->coord.t[2];
    rootCoord->coord.t[0] += (rootCoord->coord.m[0][2] * work->speed) >> ACTOR_03800_DIRECTION_FRACTION_BITS;
    rootCoord->coord.t[1] += work->fallSpeed;
    rootCoord->coord.t[2] += (rootCoord->coord.m[2][2] * work->speed) >> ACTOR_03800_DIRECTION_FRACTION_BITS;
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

/// Samples actor lighting and colour from the root coordinate's cached translation.
///
/// Requires a live task/work/root and Enemy with its model lighting matrices.
/// Does not compose or convert the cache; its frame and age are the caller's
/// responsibility. Copies signed 32-bit XYZ in game units to a temporary VECTOR
/// whose unused pad is untouched. The lighting call retains no sample pointer.
static inline void _actor03800UpdateColorAtRoot(Task* task)
{
    _Actor03800Work* work;
    GfxCoord*        rootCoord;
    VECTOR           samplePosition;

    work              = task->work;
    rootCoord         = work->rootCoord;
    samplePosition.vx = rootCoord->workm.t[0];
    samplePosition.vy = rootCoord->workm.t[1];
    samplePosition.vz = rootCoord->workm.t[2];
    worldCoordUpdateActorColor(task->spawnArg2.pointer, &samplePosition, 0, 0);
}

/// Retires collision and targeting, then collapses or bursts the dying actor.
///
/// Requires matching live enemy/task work on entry. Releases the battle hold
/// and credits rewards once; collapse becomes translucent at tick 10, burns at 15 and retires
/// at 60. Burst hides the model, releases its primitive buffer on burst stage 2
/// and spawns fragments. The destroy stage frees the enemy and task; no accesses
/// follow that call. Scene pause/hide controls suspend the death sequence.
static void _actor03800Death(Enemy* enemy, Task* task)
{
    enum {
        ACTOR_03800_DEATH_BEGIN              = 0,
        ACTOR_03800_DEATH_COLLAPSE           = 1,
        ACTOR_03800_DEATH_DESTROY            = 2,
        ACTOR_03800_DEATH_BURST              = 3,
        ACTOR_03800_DEATH_TRANSLUCENT_TICK   = 10,
        ACTOR_03800_DEATH_BURN_TICK          = 15,
        ACTOR_03800_DEATH_FINISH_TICK        = 60,
        ACTOR_03800_SOUND_DEATH              = 0x40260004,
        ACTOR_03800_REWARD_ACTOR_ID          = 0x26,
        ACTOR_03800_CORPSE_FLAME_BATCH_COUNT = 2,
    };
    _Actor03800Work* work;
    TmdObject*       model;
    GfxCoord*        rootCoord;
    s32              actorControl;
    s16              deathTick;
    s16              deathAnimation;
    s32              soundId;
    s32              audioPan;

    model        = task->extra.tmd;
    work         = task->work;
    actorControl = gSceneCombatState.actorControl;
    rootCoord    = work->rootCoord;
    switch (actorControl) {
        case SCENE_COMBAT_ACTORS_PAUSED:
            _actor03800UpdateColorAtRoot(task);
            return;
        case SCENE_COMBAT_ACTORS_HIDDEN:
            model->flags = TMD_OBJECT_SKIP_ACTIVE_DRAW;
            return;
        case SCENE_COMBAT_ACTORS_RUNNING:
        default:
            break;
    }
    switch (work->actionStep) {
        case ACTOR_03800_DEATH_BEGIN:
            if (work->deathAnimChosen == 0) {
                deathAnimation = ACTOR_03800_ANIM_STAND;
                if (work->overturned != 0) {
                    deathAnimation = ACTOR_03800_ANIM_DIE_OVERTURNED;
                }
                work->anim = deathAnimation;
            }
            work->timer        = 0;
            work->deathScaleY  = ONE;
            work->savedRootMtx = rootCoord->coord;
            // Remove published contacts and targeting before releasing the battle hold.
            enemy->recs = 0;
            worldTargetUnlinkNode(&enemy->node);
            worldCollisionUnlinkBody(&work->hitBody);
            worldCollisionUnlinkBody(&work->gridBody);
            worldCollisionUnlinkBody(&work->attackBody);
            worldCoordSetActorColorMode(enemy, ENEMY_COLOR_WEIGHTED);
            sceneReleaseBattleRefWithRewards(task, ACTOR_03800_REWARD_ACTOR_ID);
            work->actionStep = ACTOR_03800_DEATH_COLLAPSE;
            if (work->burstStage != 0) {
                model->flags     = TMD_OBJECT_SKIP_ACTIVE_DRAW;
                work->actionStep = ACTOR_03800_DEATH_BURST;
            }
            _actor03800TickAnim(task);
            _actor03800UpdateColorAtRoot(task);
            soundId  = ((enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << ACTOR_03800_SOUND_INSTANCE_SHIFT) | ACTOR_03800_SOUND_DEATH;
            audioPan = (s8)worldCoordGetOriginAudioPan(rootCoord);
            sndEvtRequestScriptStart(soundId, audioPan, (s8)worldCoordGetOriginAudioDepth(rootCoord));
            return;
        case ACTOR_03800_DEATH_COLLAPSE:
            _actor03800SquashCorpse(task);
            deathTick   = work->timer + 1;
            work->timer = deathTick;
            if (deathTick == ACTOR_03800_DEATH_TRANSLUCENT_TICK) {
                model->flags = TMD_OBJECT_SEMI_TRANS;
            }
            if (work->timer == ACTOR_03800_DEATH_BURN_TICK) {
                effectSpawn(EFFECT_CORPSE_BURN, rootCoord, ACTOR_03800_CORPSE_FLAME_BATCH_COUNT, NULL);
            }
            if (work->timer >= ACTOR_03800_DEATH_FINISH_TICK) {
                work->actionStep = ACTOR_03800_DEATH_DESTROY;
                model->flags     = TMD_OBJECT_SKIP_ACTIVE_DRAW;
            }
            _actor03800TickAnim(task);
            _actor03800UpdateColorAtRoot(task);
            return;
        case ACTOR_03800_DEATH_DESTROY:
            enemyDestroy(enemy, task);
            return;
        case ACTOR_03800_DEATH_BURST:
            if (work->burstStage != 0) {
                if (work->burstStage >= ACTOR_03800_BURST_SPAWN) {
                    work->burstStage = 0;
                    // Hide the parent and give up its primitives before allocating fragments.
                    tmdFreePrimitiveBuffer(model);
                    model->flags |= TMD_OBJECT_SKIP_AUTO_BUFFER;
                    _actor03800SpawnBurstFragments(task);
                } else {
                    work->burstStage++;
                }
            }
            deathTick   = work->timer + 1;
            work->timer = deathTick;
            if (deathTick >= ACTOR_03800_DEATH_FINISH_TICK) {
                work->actionStep = ACTOR_03800_DEATH_DESTROY;
            }
            return;
    }
}

/// Spawns one to three random body fragments, reducing the count for ongoing battles.
///
/// Requires live task work/model/placement and initialized effect resources.
/// One of five variants is always attempted. With zero/one battle references,
/// two/one additional effects are attempted. The third selection deliberately
/// excludes the second draw's list index, preserving the stored algorithm;
/// variants can repeat. Failed effect allocations still consume their draws.
static void _actor03800SpawnBurstFragments(Task* task)
{
    s16  remainingVariants[ACTOR_03800_BURST_VARIANT_COUNT - 1];
    s16* variantOut;
    s32  variantIndex;
    s32  extraFragmentCount;
    u32  firstVariant;
    s32  secondChoiceIndex;

    gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
    firstVariant    = (u16)((gRandomLcgState >> 16) % ACTOR_03800_BURST_VARIANT_COUNT);
    _actor03800SpawnBurstFragment(task, firstVariant);
    if (gSceneCombatState.battleRefs < ARRAY_SIZE(Actor03800_D05FA8)) {
        extraFragmentCount = Actor03800_D05FA8[gSceneCombatState.battleRefs];
        variantOut         = remainingVariants;
        for (variantIndex = 0; variantIndex < ACTOR_03800_BURST_VARIANT_COUNT; variantIndex++) {
            if (variantIndex != firstVariant) {
                *variantOut++ = variantIndex;
            }
        }
        gRandomLcgState   = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
        secondChoiceIndex = (gRandomLcgState >> 16) & (ARRAY_SIZE(remainingVariants) - 1);
        _actor03800SpawnBurstFragment(task, remainingVariants[secondChoiceIndex]);
        if (--extraFragmentCount > 0) {
            s16* thirdVariantOut = remainingVariants;

            // The exclusion uses the second draw's index, not its selected variant.
            for (variantIndex = 0; variantIndex < ACTOR_03800_BURST_VARIANT_COUNT; variantIndex++) {
                if (variantIndex != firstVariant && variantIndex != secondChoiceIndex) {
                    *thirdVariantOut++ = variantIndex;
                }
            }
            gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            _actor03800SpawnBurstFragment(task, remainingVariants[(u16)((gRandomLcgState >> 16) % (ACTOR_03800_BURST_VARIANT_COUNT - 2))]);
        }
    }
}

/// Spawns a selected body fragment with the dying actor's placement texture offsets.
///
/// `variant` is 0..4. Requires a live six-part actor, enemy placement
/// index and current area resources. Supplies the next bank-4 effect's model
/// through the task descriptor, then borrows the spawned model to rebuild both
/// buffer halves. Allocation failure returns without a texture lookup.
static void _actor03800SpawnBurstFragment(Task* task, u32 variant)
{
    enum { ACTOR_03800_BURST_ANCHOR_PART = 3,
           ACTOR_03800_BURST_PUFF_SIZE   = 256 };
    Enemy*           placementOwner;
    GameLocationKey  location;
    GameLocationKey* sessionLocation;
    u8               view;
    AreaVariant*     areaVariant;
    AreaPlacement*   placement;
    EffectWork*      fragmentEffect;
    TmdObject*       model;
    s32              placementIndex;
    u32              placeKey;

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
    fragmentEffect = effectSpawn(EFFECT_BURST_BODY_PART_BANK4, task->extra.tmd->coords + ACTOR_03800_BURST_ANCHOR_PART, ACTOR_03800_BURST_PUFF_SIZE, NULL);
    if (fragmentEffect == NULL) {
        return;
    }
    // Resolve the saved placement variant; warp is never read by these lookups.
    sessionLocation = &gGameSession->location.loc;
    placementOwner  = task->spawnArg2.pointer;
    placeKey        = placementOwner->placeKey;
    model           = fragmentEffect->task->extra.tmd;
    location.stage  = sessionLocation->stage;
    location.area   = sessionLocation->area;
    location.room   = sessionLocation->room;
    view            = sessionLocation->view;
    placementIndex  = placeKey >> ENEMY_PLACE_INDEX_SHIFT;
    location.view   = view;
    areaSyncLocationVariant(&location);
    areaVariant = areaGetVariant(&location);

    placement                = gpAreaPlaceAt(areaVariant->placements, placementIndex);
    model->texturePageOffset = placement->texturePageOffset;
    model->clutRowOffset     = placement->clutRowOffset;
    if (model->buffer != NULL) {
        tmdBuildBufferHalf(model);
        tmdBuildBufferHalf(model);
    }
}

/// Dispatches the actor's spawn, live or death task state.
///
/// Task state must be 0..2 with its Enemy in spawnArg2.pointer. Copies the
/// three-entry callback table before dispatch; a handler may destroy the task
/// and enemy, so neither is accessed afterwards.
static void _actor03800Task(Task* task)
{
    EnemyTaskFuncTable3 handlers;

    handlers = Actor03800_D00004;
    handlers.funcs[task->state](task->spawnArg2.pointer, task);
}

/// Runs reactions, contacts, action, motion and animation for one live actor tick.
///
/// Requires matching enemy/task work initialized by the spawn state. Contacts
/// describe the previous collision pass. Pause redraws colour and shadow from
/// cached coordinates; hide also disables lock-on. A normal tick composes the
/// moved root before lighting and shadow, and may hand off to the death state.
static void _actor03800Tick(Enemy* enemy, Task* task)
{
    _Actor03800Work* work;

    work = task->work;
    switch (gSceneCombatState.actorControl) {
        case SCENE_COMBAT_ACTORS_RUNNING:
            task->extra.tmd->flags        = 0;
            enemy->node.state.parts.flags = 0;
            break;
        case SCENE_COMBAT_ACTORS_HIDDEN:
            task->extra.tmd->flags        = TMD_OBJECT_SKIP_ACTIVE_DRAW;
            enemy->node.state.parts.flags = WORLD_TARGET_NOT_LOCKABLE;
            return;
        case SCENE_COMBAT_ACTORS_PAUSED:
            _actor03800UpdateColor(task);
            _actor03800DrawGroundShadow(task);
            return;
    }
    if (enemy->reactionFlags != 0) {
        _actor03800ApplyReactions(task);
    }
    // Reactions and contacts precede the action that chooses this tick's motion.
    _actor03800ProcessContacts(task);
    _actor03800DispatchAction(task);
    if (work->turnRate != 0) {
        _actor03800TurnTowardTargetYaw(task);
    }
    _actor03800MoveRoot(task);
    if (work->stepSoundTimer != 0) {
        _actor03800TickFootstepSound(task);
    }
    _actor03800Animate(task);
    // Compose only after motion and playback, then sample lighting and draw the shadow.
    work->rootCoord->composeStamp = GRAPHICS_COORD_DIRTY;
    actorRenderComposeCoord(work->rootCoord);
    _actor03800UpdateColor(task);
    _actor03800DrawGroundShadow(task);
}

/// Runs the selected action and sizes the room-collision sphere for its resulting posture.
///
/// Requires spawned task work and an ACTOR_03800_ACTION_* value. The sphere is
/// 250 game units upright or 350 overturned, with its centre one radius above
/// the root along negative Y; the post-handler posture controls the size.
static void _actor03800DispatchAction(Task* task)
{
    enum { ACTOR_03800_UPRIGHT_GRID_RADIUS    = 250,
           ACTOR_03800_OVERTURNED_GRID_RADIUS = 350 };
    _Actor03800Work* work;
    s16              action;
    s16              gridRadius;

    work   = task->work;
    action = work->action;
    switch (action) {
        case ACTOR_03800_ACTION_IDLE:
            _actor03800ActionIdle(task);
            break;
        case ACTOR_03800_ACTION_WANDER:
            _actor03800ActionWander(task);
            break;
        case ACTOR_03800_ACTION_CHARGE:
            _actor03800ActionCharge(task);
            break;
        case ACTOR_03800_ACTION_KNOCKED_OVER:
            _actor03800ActionKnockedOver(task);
            break;
        case ACTOR_03800_ACTION_GET_UP:
            _actor03800ActionGetUp(task);
            break;
        case ACTOR_03800_ACTION_FLINCH:
            _actor03800ActionFlinch(task);
            break;
        case ACTOR_03800_ACTION_BUILDUP:
            _actor03800ActionBuildup(task);
            break;
        case ACTOR_03800_ACTION_DIE:
            _actor03800ActionDie(task);
            break;
        case ACTOR_03800_ACTION_WALL_WAIT:
            _actor03800ActionWallWait(task);
            break;
        case ACTOR_03800_ACTION_CEILING_WAIT:
            _actor03800ActionCeilingWait(task);
            break;
        case ACTOR_03800_ACTION_DROP:
            _actor03800ActionDrop(task);
            break;
        case ACTOR_03800_ACTION_SHRINE_WAIT:
            _actor03800ActionShrineWait(task);
            break;
        case ACTOR_03800_ACTION_LEAP_AWAY:
            _actor03800ActionLeapAway(task);
            break;
    }
    if (work->overturned == 0) {
        work->gridBody.pos.vy = -ACTOR_03800_UPRIGHT_GRID_RADIUS;
        gridRadius            = ACTOR_03800_UPRIGHT_GRID_RADIUS;
    } else {
        work->gridBody.pos.vy = -ACTOR_03800_OVERTURNED_GRID_RADIUS;
        gridRadius            = ACTOR_03800_OVERTURNED_GRID_RADIUS;
    }
    work->gridBody.radius = gridRadius;
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

/// Counts down a running footstep cadence and queues a placement-specific sound every 12 ticks.
///
/// Requires live enemy/work/root storage and a nonzero stepSoundTimer from the
/// caller. Audio position queries are narrowed to signed bytes for the queue.
static void _actor03800TickFootstepSound(Task* task)
{
    enum { ACTOR_03800_FOOTSTEP_PERIOD_TICKS = 12,
           ACTOR_03800_SOUND_FOOTSTEP        = 0x40260001 };
    Enemy*           enemy;
    _Actor03800Work* work;
    GfxCoord*        rootCoord;
    s32              soundId;
    s32              audioPan;

    work      = task->work;
    rootCoord = work->rootCoord;
    if (--work->stepSoundTimer <= 0) {
        work->stepSoundTimer = ACTOR_03800_FOOTSTEP_PERIOD_TICKS;
        enemy                = task->spawnArg2.pointer;
        soundId              = ((enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << ACTOR_03800_SOUND_INSTANCE_SHIFT) | ACTOR_03800_SOUND_FOOTSTEP;
        audioPan             = (s8)worldCoordGetOriginAudioPan(rootCoord);
        sndEvtRequestScriptStart(soundId, audioPan, (s8)worldCoordGetOriginAudioDepth(rootCoord));
    }
}

/// Starts or advances the actor's requested animation for the live tick.
///
/// Requires the initialized five driven rig slots and the clip-index contract
/// of `_actor03800TickAnim`; the death state uses the same playback inline.
static void _actor03800Animate(Task* task)
{
    _actor03800TickAnim(task);
}

/// Updates live-state lighting from the cached root position.
///
/// Requires the live storage and composition-cache contract of
/// `_actor03800UpdateColorAtRoot`, also used by the death state.
static void _actor03800UpdateColor(Task* task)
{
    _actor03800UpdateColorAtRoot(task);
}

/// Draws the actor's floor shadow or projects a perched root onto the ground.
///
/// Requires live work and a composed root. Floor mode uses the root's cached
/// position and stored shade, including -1 to suppress drawing. Perched modes
/// project onto the room grid and shade by the signed view-Y displacement.
static void _actor03800DrawGroundShadow(Task* task)
{
    enum { ACTOR_03800_FLOOR_SHADOW_SIZE = 500,
           ACTOR_03800_PERCH_SHADOW_SIZE = 512 };
    _Actor03800Work* work;
    GfxCoord*        rootCoord;
    VECTOR3          shadowPosition;
    s16              viewYDisplacement;

    work      = task->work;
    rootCoord = work->rootCoord;
    if (work->mode == ACTOR_03800_MODE_FLOOR) {
        shadowPosition.vx = rootCoord->workm.t[0];
        shadowPosition.vy = rootCoord->workm.t[1];
        shadowPosition.vz = rootCoord->workm.t[2];
        effectDrawGroundShadow(&shadowPosition, ACTOR_03800_FLOOR_SHADOW_SIZE, work->shadowShade);
        return;
    }
    viewYDisplacement = worldCollisionProjectGroundPoint(MATRIX_TRANS(&rootCoord->workm), &shadowPosition);
    if (viewYDisplacement != 0) {
        effectDrawGroundShadow(&shadowPosition, ACTOR_03800_PERCH_SHADOW_SIZE, effectGetGroundShadowShade(ACTOR_03800_PERCH_SHADOW_SIZE, ACTOR_03800_SHADOW_SHADE, viewYDisplacement));
    }
}

/// Restores the saved death transform and applies its Q12 local Y scale.
///
/// Root, work and scratch must be live and disjoint. The caller owns scratch
/// reservation and release; restoring the full matrix prevents compounded scale.
static inline void _actor03800ApplyCorpseScale(GfxCoord* rootCoord, const _Actor03800Work* work, ActorScaleScratch* scratch)
{
    scratch->scale.vx = ONE;
    scratch->scale.vy = work->deathScaleY;
    scratch->scale.vz = ONE;
    rootCoord->coord  = work->savedRootMtx;
    gfxSetRotIdentity(&scratch->matrix);
    ScaleMatrix(&scratch->matrix, &scratch->scale);
    MulMatrix(&rootCoord->coord, &scratch->matrix);
    rootCoord->composeStamp = GRAPHICS_COORD_DIRTY;
}

/// Squashes the corpse along local Y from its saved death transform.
///
/// Requires savedRootMtx captured on death entry and live work/root storage.
/// Q12 scale decreases by 80 while above 512; a start of ONE ends at 496,
/// preserving the undershoot. Restoring the saved matrix avoids compounded
/// scaling and retains translation. Dirties composition and releases scratch.
static void _actor03800SquashCorpse(Task* task)
{
    enum { ACTOR_03800_CORPSE_SCALE_CUTOFF_Q12 = 512,
           ACTOR_03800_CORPSE_SCALE_STEP_Q12   = 80 };
    _Actor03800Work*   work;
    GfxCoord*          rootCoord;
    ActorScaleScratch* scratchHead;
    ActorScaleScratch* scratch;

    work                                    = task->work;
    scratchHead                             = SCRATCH_STACK_CURSOR(ActorScaleScratch);
    scratch                                 = scratchHead - 1;
    SCRATCH_STACK_CURSOR(ActorScaleScratch) = scratch;
    rootCoord                               = work->rootCoord;
    if (work->deathScaleY > ACTOR_03800_CORPSE_SCALE_CUTOFF_Q12) {
        work->deathScaleY = work->deathScaleY - ACTOR_03800_CORPSE_SCALE_STEP_Q12;
    }
    _actor03800ApplyCorpseScale(rootCoord, work, scratch);
    SCRATCH_STACK_RELEASE_BLOCK(ActorScaleScratch);
}
