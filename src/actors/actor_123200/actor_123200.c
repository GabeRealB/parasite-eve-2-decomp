#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/abs.h>
#include <psyq/inline_c.h>
#include <psyq/memory.h>

#include "common.h"
#include "gte.h"

#include "actors/actor.h"

#include "gameplay/actor_render.h"
#include "gameplay/animation.h"
#include "gameplay/collision.h"
#include "gameplay/effect_tasks.h"
#include "gameplay/effects.h"
#include "gameplay/enemy.h"
#include "gameplay/geometry.h"
#include "gameplay/message.h"
#include "gameplay/enemy_params.h"
#include "gameplay/room_effects.h"
#include "gameplay/scene.h"
#include "gameplay/scene_combat.h"
#include "gameplay/scene_runtime.h"
#include "gameplay/world_collision.h"
#include "gameplay/world_coords.h"
#include "gameplay/world_targets.h"

#include "main/coord.h"
#include "main/gfx.h"
#include "main/mc.h"
#include "main/mc_types.h"
#include "main/mem.h"
#include "main/scratch.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/sound.h"
#include "main/task_types.h"
#include "main/tmd.h"
#include "main/tmd_types.h"

#include "overlay.h"
#include "../../shared/coord_math.h"
#include "../../shared/actor_messages.h"
#include "../../shared/anim_driver.h"

/// Values of `_Actor123200Work::state`: the index of the handler the per-frame
/// tick runs.
///
/// Animation numbers are indices into the package's animation-set table. The
/// two walking states do the same thing; a change from one to the other is
/// what makes the walk start over.
enum {
    ACTOR_123200_STATE_HIDDEN             = 0, // not drawn, not lockable, no ground shadow
    ACTOR_123200_STATE_WALK               = 1, // restarts animation 2, then steps 5 units along the facing every tick; entered by actor command 2 and by the model-draw message
    ACTOR_123200_STATE_FIRST_STAGING_WALK = 2  // the same walk; entered only by actor command 1
};

/// Values of `_Actor123200Work::lightScale`, in 4096ths.
enum {
    ACTOR_123200_LIGHT_SCALE_QUARTER = 0x400,
    ACTOR_123200_LIGHT_SCALE_FULL    = 0x1000 // the tick leaves the light matrix as built
};

/// Work block of the package's enemy task.
///
/// The spawn handler allocates it zeroed and keeps it at `Task::work`. The
/// actor is a bone suckler staged for the Dryfield motel room 1 event: it has
/// no collision bodies and no hit points, and only walks straight ahead while
/// the event's actor commands pick its state and how brightly it is lit.
///
/// Everything up to and including `driver` is laid out as `AnimDriverWork`,
/// through which the shared animation driver reaches the block: its
/// `carrierState` bytes are the five words and the gap ahead of `rig` here. A
/// cue index is the low ten bits of the record a slot's current pose names
/// (`ANIMATION_POSE_CUE_INDEX_MASK`).
typedef struct {
    s16           state;        // `ACTOR_123200_STATE_*`
    s16           prevState;    // `state` the tick last ran; -1 from spawn, so the first tick enters `state` afresh
    s16           stateEntered; // 1 on the first tick of a state, when the handler sets itself up (0 otherwise)
    s16           stateFrame;   // ticks a walking state has run since it was entered; never read
    s16           field_8;      // cleared at spawn and never accessed again; role unproven
    byte          pad_A[2];     // never accessed
    ActorAnimRig6 rig;          // playback of the model's parts; slot 1's status and cue index time the sound cues
    struct {
        s16 state;              // `ANIM_DRIVER_STATE_*` (0 idle, 1 or 2 restart requested, 3 playing)
        s16 playingSet;         // animation the slots were last restarted on
        s16 requestedSet;       // animation the next restart plays; also what the sound cues are keyed on
        s16 rate;               // playback rate in sixteenths of a frame per tick; 16 plus the placement index (odd placements) or minus half of it (even)
        s16 rateBias;           // added to `rate`; always 0 here
        s16 tickCount;          // advancing ticks since the last restart
        s16 jumpCount;          // of those, ticks on which slot 1 followed a control jump: loops of a looping animation
    } driver;                   // the animation driver's state, member for member `AnimDriverWork`'s
    s16     field_17E;          // cleared at spawn and never accessed again; role unproven
    byte    pad_180[0x14];      // never accessed
    u8      lastCommandStage;   // stage tag of the last actor command received, whatever its namespace; never read
    u8      lastCommandArea;    // area tag of that command; never read
    u8      lastCommand;        // low byte of that command's selector; never read
    byte    pad_197[1];         // never accessed
    u16     field_198;          // 5 at spawn, offset by the placement index the way `driver.rate` is; never accessed again; role unproven
    u16     field_19A;          // 20 at spawn, offset the same way; never accessed again; role unproven
    byte    pad_19C[0xC];       // never accessed
    SVECTOR spawnPos;           // root position at spawn; never read
    SVECTOR walkTarget;         // world point stored on entering a walking state; never read, since the walk follows the facing instead of steering
    byte    pad_1B8[4];         // never accessed
    MATRIX  lightMtx;           // storage for the model's `TmdObject::lightMtx`
    MATRIX  colorMtx;           // storage for the model's `TmdObject::colorMtx`
    byte    pad_1FC[0x20];      // never accessed
    s16     lightScale;         // `ACTOR_123200_LIGHT_SCALE_*`: scales `lightMtx` after every tick's relighting, dimming the model; 0 from spawn until an actor command sets it
    byte    pad_21E[2];         // never accessed
    u16     lastSoundCueIndex;  // slot 1's cue index when the walk cue sound last fired, so a held cue sounds once; 0 on any other cue
    byte    pad_222[0xA];       // never accessed
} _Actor123200Work;
STATIC_ASSERT_SIZEOF(_Actor123200Work, 0x22C);
STATIC_ASSERT(OFFSET_OF(_Actor123200Work, rig) == OFFSET_OF(AnimDriverWork, rig), Actor123200Work_rig);
STATIC_ASSERT(OFFSET_OF(_Actor123200Work, driver) == OFFSET_OF(AnimDriverWork, state), Actor123200Work_driver);

/// Enemy parameters the spawn handler installs at `Enemy::param`.
extern EnemyParams D_actor_123200_80134208;

/// Resource whose animation-set table is bound by `animationInitContext`.
extern u8 D_actor_123200_80137154[];

/// Message table the spawn handler publishes as `Task::msgTable`.
// Message-table callbacks use the argument views required by this TU.

extern TaskMessageEntry D_actor_123200_80137214[4];

/// Integer part of the last movement step `func_actor_123200_801329F0`
/// applied.
static SVECTOR ActorContact_ScratchPosition;

/// Overlay-wide spawn record the spawn handler fills for the instance's own
/// coordinate, with the 0x100 / 1 argument pair. Each overlay that spawns this
/// way keeps one, and they differ only in the coordinate and the argument.
extern EffectSpawnArg D_actor_123200_80137248;

/// Psy-Q `RotMatrixY` (it sits right after `RotMatrixX`).

MATRIX* ScaleMatrix(MATRIX* m, VECTOR* v);

static void func_actor_123200_80134178(Enemy* arg0, Task* arg1);

static TmdSource _gActor123200BoneSucklerBody;
void             func_actor_123200_801341A8(Task*);

s32 func_actor_123200_80133E30(Task*, s32, s32, s32);
s32 func_actor_123200_80133EDC(Task* task, s32 msgId, ActorCommand* msg, s32 arg3);

#include "../../shared/actor_contacts.h"

DamageAttack D_actor_123200_80134204[1] = {
    { 24, 7 },
};

EnemyParams D_actor_123200_80134208 = { D_actor_123200_80134204, 1, 6, 20, 3, 100, 0, 100, 0 };

static TmdBone _gActor123200BoneSucklerBodySkeleton[6] = {
#include "assets/bone_suckler_body_skeleton.inc"
};

static u32 _gActor123200BoneSucklerBodyPartVerts[6] = {
#include "assets/bone_suckler_body_partVerts.inc"
};

static SVECTOR _gActor123200BoneSucklerBodyVerts[102] = {
#include "assets/bone_suckler_body_verts.inc"
};

static SVECTOR _gActor123200BoneSucklerBodyNormals[139] = {
#include "assets/bone_suckler_body_normals.inc"
};

static u32 _gActor123200BoneSucklerBodyStream[1048] = {
#include "assets/bone_suckler_body_stream.inc"
};

static TmdSource _gActor123200BoneSucklerBody = {
    0,
    5984,
    1264,
    6,
    _gActor123200BoneSucklerBodyPartVerts,
    _gActor123200BoneSucklerBodyVerts,
    _gActor123200BoneSucklerBodyNormals,
    _gActor123200BoneSucklerBodySkeleton,
    _gActor123200BoneSucklerBodyStream,
};

static AnimationPackedPose _gActor123200Animation03E30Bank1[4] = {
#include "assets/actor_123200_animation_03E30_bank1.inc"
};

static AnimationPackedRotation _gActor123200Animation03E30Bank4[21] = {
#include "assets/actor_123200_animation_03E30_bank4.inc"
};

static AnimationRecord _gActor123200Animation03E30Records[43] = {
#include "assets/actor_123200_animation_03E30_records.inc"
};

static u16 _gActor123200Animation03E30Indices[6] = {
#include "assets/actor_123200_animation_03E30_indices.inc"
};

static AnimationSet _gActor123200Animation03E30 = {
    _gActor123200Animation03E30Records,
    _gActor123200Animation03E30Indices,
    { NULL, _gActor123200Animation03E30Bank1, NULL, NULL, _gActor123200Animation03E30Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor123200Animation040F4Bank1[19] = {
#include "assets/actor_123200_animation_040F4_bank1.inc"
};

static AnimationPackedRotation _gActor123200Animation040F4Bank4[36] = {
#include "assets/actor_123200_animation_040F4_bank4.inc"
};

static AnimationRecord _gActor123200Animation040F4Records[71] = {
#include "assets/actor_123200_animation_040F4_records.inc"
};

static u16 _gActor123200Animation040F4Indices[6] = {
#include "assets/actor_123200_animation_040F4_indices.inc"
};

static AnimationSet _gActor123200Animation040F4 = {
    _gActor123200Animation040F4Records,
    _gActor123200Animation040F4Indices,
    { NULL, _gActor123200Animation040F4Bank1, NULL, NULL, _gActor123200Animation040F4Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor123200Animation04398Bank1[20] = {
#include "assets/actor_123200_animation_04398_bank1.inc"
};

static AnimationPackedRotation _gActor123200Animation04398Bank4[31] = {
#include "assets/actor_123200_animation_04398_bank4.inc"
};

static AnimationRecord _gActor123200Animation04398Records[65] = {
#include "assets/actor_123200_animation_04398_records.inc"
};

static u16 _gActor123200Animation04398Indices[6] = {
#include "assets/actor_123200_animation_04398_indices.inc"
};

static AnimationSet _gActor123200Animation04398 = {
    _gActor123200Animation04398Records,
    _gActor123200Animation04398Indices,
    { NULL, _gActor123200Animation04398Bank1, NULL, NULL, _gActor123200Animation04398Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor123200Animation04578Bank1[6] = {
#include "assets/actor_123200_animation_04578_bank1.inc"
};

static AnimationPackedRotation _gActor123200Animation04578Bank4[38] = {
#include "assets/actor_123200_animation_04578_bank4.inc"
};

static AnimationRecord _gActor123200Animation04578Records[51] = {
#include "assets/actor_123200_animation_04578_records.inc"
};

static u16 _gActor123200Animation04578Indices[6] = {
#include "assets/actor_123200_animation_04578_indices.inc"
};

static AnimationSet _gActor123200Animation04578 = {
    _gActor123200Animation04578Records,
    _gActor123200Animation04578Indices,
    { NULL, _gActor123200Animation04578Bank1, NULL, NULL, _gActor123200Animation04578Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor123200Animation046B0Bank1[4] = {
#include "assets/actor_123200_animation_046B0_bank1.inc"
};

static AnimationPackedRotation _gActor123200Animation046B0Bank4[13] = {
#include "assets/actor_123200_animation_046B0_bank4.inc"
};

static AnimationRecord _gActor123200Animation046B0Records[40] = {
#include "assets/actor_123200_animation_046B0_records.inc"
};

static u16 _gActor123200Animation046B0Indices[6] = {
#include "assets/actor_123200_animation_046B0_indices.inc"
};

static AnimationSet _gActor123200Animation046B0 = {
    _gActor123200Animation046B0Records,
    _gActor123200Animation046B0Indices,
    { NULL, _gActor123200Animation046B0Bank1, NULL, NULL, _gActor123200Animation046B0Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor123200Animation04850Bank1[7] = {
#include "assets/actor_123200_animation_04850_bank1.inc"
};

static AnimationPackedRotation _gActor123200Animation04850Bank4[28] = {
#include "assets/actor_123200_animation_04850_bank4.inc"
};

static AnimationRecord _gActor123200Animation04850Records[42] = {
#include "assets/actor_123200_animation_04850_records.inc"
};

static u16 _gActor123200Animation04850Indices[6] = {
#include "assets/actor_123200_animation_04850_indices.inc"
};

static AnimationSet _gActor123200Animation04850 = {
    _gActor123200Animation04850Records,
    _gActor123200Animation04850Indices,
    { NULL, _gActor123200Animation04850Bank1, NULL, NULL, _gActor123200Animation04850Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor123200Animation04A64Bank1[6] = {
#include "assets/actor_123200_animation_04A64_bank1.inc"
};

static AnimationPackedRotation _gActor123200Animation04A64Bank4[42] = {
#include "assets/actor_123200_animation_04A64_bank4.inc"
};

static AnimationRecord _gActor123200Animation04A64Records[60] = {
#include "assets/actor_123200_animation_04A64_records.inc"
};

static u16 _gActor123200Animation04A64Indices[6] = {
#include "assets/actor_123200_animation_04A64_indices.inc"
};

static AnimationSet _gActor123200Animation04A64 = {
    _gActor123200Animation04A64Records,
    _gActor123200Animation04A64Indices,
    { NULL, _gActor123200Animation04A64Bank1, NULL, NULL, _gActor123200Animation04A64Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor123200Animation04E74Bank1[17] = {
#include "assets/actor_123200_animation_04E74_bank1.inc"
};

static AnimationPackedRotation _gActor123200Animation04E74Bank4[85] = {
#include "assets/actor_123200_animation_04E74_bank4.inc"
};

static AnimationRecord _gActor123200Animation04E74Records[111] = {
#include "assets/actor_123200_animation_04E74_records.inc"
};

static u16 _gActor123200Animation04E74Indices[6] = {
#include "assets/actor_123200_animation_04E74_indices.inc"
};

static AnimationSet _gActor123200Animation04E74 = {
    _gActor123200Animation04E74Records,
    _gActor123200Animation04E74Indices,
    { NULL, _gActor123200Animation04E74Bank1, NULL, NULL, _gActor123200Animation04E74Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor123200Animation05134Bank1[12] = {
#include "assets/actor_123200_animation_05134_bank1.inc"
};

static AnimationPackedRotation _gActor123200Animation05134Bank4[52] = {
#include "assets/actor_123200_animation_05134_bank4.inc"
};

static AnimationRecord _gActor123200Animation05134Records[75] = {
#include "assets/actor_123200_animation_05134_records.inc"
};

static u16 _gActor123200Animation05134Indices[6] = {
#include "assets/actor_123200_animation_05134_indices.inc"
};

static AnimationSet _gActor123200Animation05134 = {
    _gActor123200Animation05134Records,
    _gActor123200Animation05134Indices,
    { NULL, _gActor123200Animation05134Bank1, NULL, NULL, _gActor123200Animation05134Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor123200Animation0530CBank1[5] = {
#include "assets/actor_123200_animation_0530C_bank1.inc"
};

static AnimationPackedRotation _gActor123200Animation0530CBank4[19] = {
#include "assets/actor_123200_animation_0530C_bank4.inc"
};

static AnimationRecord _gActor123200Animation0530CRecords[71] = {
#include "assets/actor_123200_animation_0530C_records.inc"
};

static u16 _gActor123200Animation0530CIndices[6] = {
#include "assets/actor_123200_animation_0530C_indices.inc"
};

static AnimationSet _gActor123200Animation0530C = {
    _gActor123200Animation0530CRecords,
    _gActor123200Animation0530CIndices,
    { NULL, _gActor123200Animation0530CBank1, NULL, NULL, _gActor123200Animation0530CBank4, NULL, NULL, NULL },
};

u8 D_actor_123200_80137154[192] = {
    0,
    0,
    0,
    0,
    80,
    92,
    19,
    128,
    20,
    95,
    19,
    128,
    184,
    97,
    19,
    128,
    152,
    99,
    19,
    128,
    208,
    100,
    19,
    128,
    112,
    102,
    19,
    128,
    132,
    104,
    19,
    128,
    148,
    108,
    19,
    128,
    84,
    111,
    19,
    128,
    44,
    113,
    19,
    128,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    6,
    0,
    0,
    0,
    0,
    0,
    9,
    0,
    0,
    0,
    0,
    0,
    6,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    6,
    6,
    6,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
};

TaskMessageEntry D_actor_123200_80137214[4] = {
    { ACTOR_MESSAGE_SET_MODEL_DRAW, func_actor_123200_80133E30 },
    { ACTOR_COMMAND_MESSAGE_APPLY, func_actor_123200_80133EDC },
    { ACTOR_MESSAGE_PLACE, actorMsgPlace },
    { TASK_MESSAGE_TABLE_END, NULL },
};

TaskDesc D_actor_123200_80137234 = { { { TASK_BODY_TMD, 96 } }, func_actor_123200_801341A8, { .model = &_gActor123200BoneSucklerBody } };

static SVECTOR ActorContact_ScratchPosition;

static inline SVECTOR* ActorContact_GetScratchPosition(void)
{
    return &ActorContact_ScratchPosition;
}

EffectSpawnArg D_actor_123200_80137248;

static s32             func_actor_123200_80133450(_Actor123200Work* arg0);
static __inline__ void Actor123200_ScaleForward(SVECTOR* dir);
static void            func_actor_123200_8013352C(Enemy* enemy, Task* task);
static __inline__ void Actor123200_StepForward(GfxCoord* coord);
static void            func_actor_123200_80133820(Enemy* enemy, Task* task);
static __inline__ void Actor123200_MoveForward(GfxCoord* coord);
static void            func_actor_123200_801339F0(Enemy* enemy, Task* task);
static void            func_actor_123200_80133BA0(Enemy* enemy, Task* arg1);

#include "../../shared/actor_contacts.h"

#include "../../shared/actor_contacts.inc.c"

#include "../../shared/anim_driver_tick.inc.c"

/// On animations 2 and 3 (`driver.requestedSet`), reports 0x400C0001 the
/// first time rig slot 1's cue index reaches one of that animation's trigger
/// ids (latched in `lastSoundCueIndex`); on animation 5, 0x400C0005 while
/// slot 1 reports `ANIMATION_SLOT_FOLLOWED_JUMP`.
/// Returns 0 otherwise.
static s32 func_actor_123200_80133450(_Actor123200Work* arg0)
{
    u16 id;
    s32 v;

    switch (arg0->driver.requestedSet) {
        case 2:
            id = arg0->rig.slots[ANIM_DRIVER_FIRST_SLOT].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK;
            v  = id;
            if (v != 0x15) {
                goto not15;
            }
        check:
            if (arg0->lastSoundCueIndex == v) {
                goto same;
            }
            arg0->lastSoundCueIndex = id;
            return 0x400C0001;
        not15:
            if (v == 0x11) {
                goto check;
            }
        clear:
            arg0->lastSoundCueIndex = 0;
            break;
        case 3:
            id = arg0->rig.slots[ANIM_DRIVER_FIRST_SLOT].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK;
            v  = id;
            if (v != 0xD && v != 0x12) {
                goto clear;
            }
            goto check;
        same:
            arg0->lastSoundCueIndex = id;
            break;
        case 5:
            if (arg0->rig.slots[ANIM_DRIVER_FIRST_SLOT].status.fields.flags & ANIMATION_SLOT_FOLLOWED_JUMP) {
                return 0x400C0005;
            }
            break;
    }
    return 0;
}

/// Normalises `dir` in place and scales it to 0x3E8/0x1000 of unit length on
/// the GTE. The pointer stays in one register across `VectorNormalSS` because
/// the GTE loads read it back afterwards.
static __inline__ void Actor123200_ScaleForward(SVECTOR* dir)
{
    VectorNormalSS(dir, dir);
    gte_lddp(0x3E8);
    gte_ldsv(dir);
    gte_gpf12();
    gte_stsv(dir);
}

/// Spawn state of this enemy: allocates the work block, publishes it as
/// `Task::work`, reparents the model to `gGfxViewCoord`, seeds its animation
/// slots from `D_actor_123200_80137154` and hangs the enemy's display node off
/// part 2 of the model's coordinate array. The placement index in
/// `Enemy::placeKey` biases the initial values in `driver.rate`, `field_198`
/// and `field_19A`: odd indices add the index, even indices subtract half of it.
static void func_actor_123200_8013352C(Enemy* enemy, Task* task)
{
    SVECTOR           dir;
    _Actor123200Work* work;
    TmdObject*        obj;
    GfxCoord*         coord;
    u32               placementIndex;
    u32               placementParity;

    obj        = task->extra.tmd;
    coord      = obj->coords;
    work       = memCalloc(sizeof(_Actor123200Work), false);
    task->work = work;
    if (work == NULL) {
        enemyDestroy(enemy, task);
        return;
    }
    task->msgTable = D_actor_123200_80137214;
    coord->parent  = &gGfxViewCoord;
    obj->flags     = 0;
    animationInitContext(&work->rig.anim, (AnimationSet**)D_actor_123200_80137154, obj, work->rig.poses, work->rig.slots);

    enemy->field_4    = &coord->coord;
    enemy->field_48   = 0;
    enemy->bodyPos.vx = 0;
    enemy->bodyPos.vy = 0;
    enemy->bodyPos.vz = 0;
    enemy->coord      = &task->extra.tmd->coords[2];
    worldTargetLinkNode(&enemy->node);
    enemy->node.state.parts.flags = WORLD_TARGET_NOT_LOCKABLE;
    enemy->param                  = &D_actor_123200_80134208;
    enemy->reactionFlags          = 0;
    enemy->hpMax                  = 0;
    enemy->hp                     = 0;
    enemy->recs                   = 0;

    work->driver.requestedSet = 1;
    work->driver.state        = ANIM_DRIVER_STATE_RESTART_2;
    work->driver.rate         = ANIMATION_RATE_ONE;
    work->driver.rateBias     = 0;
    animDriverTick(task);
    work->field_17E     = 0;
    work->field_8       = 0;
    obj->lightMtx       = &work->lightMtx;
    obj->colorMtx       = &work->colorMtx;
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
    work->field_198     = 5;
    work->field_19A     = 0x14;

    placementIndex  = (u16)(enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT);
    placementParity = placementIndex & 1;
    if (placementParity == 1) {
        work->driver.rate += enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT;
        work->field_19A   += enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT;
        work->field_198   += enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT;
    } else {
        work->driver.rate -= placementIndex >> 1;
        work->field_19A   -= (enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) / 2;
        work->field_198   -= (enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) / 2;
    }

    work->spawnPos.vx = task->extra.tmd->coords->coord.t[0];
    work->spawnPos.vy = task->extra.tmd->coords->coord.t[1];
    work->spawnPos.vz = task->extra.tmd->coords->coord.t[2];

    gfxReadMatrixZAxis(&task->extra.tmd->coords->coord, &dir);
    dir.vy = 0;
    Actor123200_ScaleForward(&dir);

    work->state                        = ACTOR_123200_STATE_HIDDEN;
    work->prevState                    = -1;
    D_actor_123200_80137248.coord      = task->extra.tmd->coords;
    D_actor_123200_80137248.spawnArgLo = 0x100;
    D_actor_123200_80137248.spawnArgHi = 1;
    task->state++;
}

/// Steps `coord` 5/0x1000 of the way along its own forward axis (column 2 of
/// its rotation, normalised and GPF-scaled) and flags it for rebuild. The
/// direction vector lives in an `SVECTOR` carved off the scratch head and
/// handed straight back.
static __inline__ void Actor123200_StepForward(GfxCoord* coord)
{
    u8*      head;
    SVECTOR* dir;

    head                       = SCRATCH_STACK_CURSOR(u8);
    dir                        = (SVECTOR*)(head - sizeof(SVECTOR));
    SCRATCH_STACK_CURSOR(void) = dir;

    gfxReadMatrixZAxis(&coord->coord, dir);
    VectorNormalSS(dir, dir);
    gte_lddp(5);
    gte_ldsv(dir);
    gte_gpf12();
    gte_stsv(dir);

    coord->coord.t[0]  += dir->vx;
    coord->coord.t[1]  += dir->vy;
    coord->coord.t[2]  += dir->vz;
    coord->composeStamp = GRAPHICS_COORD_DIRTY;

    SCRATCH_STACK_RELEASE_BYTES(sizeof(SVECTOR));
}

/// `ACTOR_123200_STATE_WALK` handler (entry 1 of `D_actor_123200_80131E24`). On
/// the frame the state is entered (`stateEntered` set) it re-arms the model --
/// clearing `TmdObject.flags` and reinstating its buffers, storing
/// `walkTarget`, restarting the driver on animation 2 and the frame counter
/// `stateFrame`, and marking the enemy's lock-on node not lockable -- and
/// returns. Otherwise the frame counter runs, 0xC bytes are reserved off the
/// scratch head, and unless the game is frozen the model is stepped forward
/// along its facing; the reservation is released after the animation update
/// and the model's coordinate is flagged for rebuild.
static void func_actor_123200_80133820(Enemy* enemy, Task* task)
{
    _Actor123200Work* work;
    TmdObject*        obj;
    GfxCoord*         coord;

    work = task->work;
    if (work->stateEntered != 0) {
        obj                           = task->extra.tmd;
        enemy->node.state.parts.flags = WORLD_TARGET_NOT_LOCKABLE;
        obj->flags                    = 0;
        tmdAllocPrimitiveBuffer(obj);
        work->walkTarget.vx       = 0x115D;
        work->walkTarget.vy       = 1;
        work->walkTarget.vz       = 0x12D5;
        work->driver.requestedSet = 2;
        work->driver.state        = ANIM_DRIVER_STATE_RESTART_2;
        animDriverTick(task);
        task->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
        work->stateFrame                      = 0;
        return;
    }
    work->stateFrame++;
    SCRATCH_STACK_RESERVE_BYTES(0xC);
    coord = task->extra.tmd->coords;
    if (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.actorsFrozen != 1) {
        Actor123200_StepForward(coord);
    }
    animDriverTick(task);
    SCRATCH_STACK_RELEASE_BYTES(0xC);
    task->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
}

/// Unless the game is frozen, steps `coord` 5/0x1000 of the way along its own
/// forward axis through an `SVECTOR` carved off the scratch head, and flags it
/// for rebuild.
static __inline__ void Actor123200_MoveForward(GfxCoord* coord)
{
    SVECTOR* head;
    SVECTOR* vec;

    if (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.actorsFrozen != 1) {
        head                          = SCRATCH_STACK_CURSOR(SVECTOR);
        vec                           = head - 1;
        SCRATCH_STACK_CURSOR(SVECTOR) = vec;
        gfxReadMatrixZAxis(&coord->coord, vec);
        VectorNormalSS(vec, vec);
        gte_lddp(5);
        gte_ldsv(vec);
        gte_gpf12();
        gte_stsv(vec);
        coord->coord.t[0]  += head[-1].vx;
        coord->coord.t[1]  += vec->vy;
        coord->coord.t[2]  += vec->vz;
        coord->composeStamp = GRAPHICS_COORD_DIRTY;
        SCRATCH_STACK_RELEASE_BLOCK(SVECTOR);
    }
}

/// `ACTOR_123200_STATE_FIRST_STAGING_WALK` handler (entry 2 of `D_actor_123200_80131E24`): the same
/// re-arm on entry as `func_actor_123200_80133820`; on later frames it counts
/// the frame, steps the model along its facing unless the game is frozen, and
/// updates its animation, without the extra scratch reservation.
static void func_actor_123200_801339F0(Enemy* enemy, Task* task)
{
    _Actor123200Work* work;
    TmdObject*        obj;
    GfxCoord*         coord;

    work = task->work;
    if (work->stateEntered != 0) {
        obj                           = task->extra.tmd;
        enemy->node.state.parts.flags = WORLD_TARGET_NOT_LOCKABLE;
        obj->flags                    = 0;
        tmdAllocPrimitiveBuffer(obj);
        work->walkTarget.vx       = 0x115D;
        work->walkTarget.vy       = 1;
        work->walkTarget.vz       = 0x12D5;
        work->driver.requestedSet = 2;
        work->driver.state        = ANIM_DRIVER_STATE_RESTART_2;
        animDriverTick(task);
        task->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
        work->stateFrame                      = 0;
        return;
    }
    work->stateFrame++;
    coord = task->extra.tmd->coords;
    Actor123200_MoveForward(coord);
    animDriverTick(task);
    task->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
}

/// The three handlers `func_actor_123200_80133BA0` picks between by the work
/// block's `state` (`ACTOR_123200_STATE_*`), copied onto its stack before the
/// call: 0 the hidden state, 1 and 2 the two walking handlers.
static const EnemyTaskFuncTable3 D_actor_123200_80131E24 = {
    {
        func_actor_123200_80134178,
        func_actor_123200_80133820,
        func_actor_123200_801339F0,
    },
};

/// Per-frame tick: flags the model's coordinate for rebuild, relights it at
/// the part matrix's translation, then scales the light matrix in the work
/// block by its `lightScale`. The render mode in `gSceneCombatState.actorControl` runs next -- modes
/// 0 and 1 draw the ground quad unless the state is `ACTOR_123200_STATE_HIDDEN`, and 1 and 2
/// return without ticking. The rest re-records the state in `prevState`
/// (`stateEntered` re-arming the model when it changed), dispatches the
/// state's handler from `D_actor_123200_80131E24`, and plays the sound that
/// handler reports, panned and depth-tagged from the model's coordinate. A
/// raised `gGameSession->viewReady` flags the coordinate for rebuild again.
static void func_actor_123200_80133BA0(Enemy* enemy, Task* arg1)
{
    VECTOR              pos;
    EnemyTaskFuncTable3 table;
    _Actor123200Work*   work;
    s32                 snd;
    s32                 pan;
    s32                 id;

    work                                  = arg1->work;
    table                                 = D_actor_123200_80131E24;
    arg1->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    actorRenderComposeCoord(arg1->extra.tmd->coords);
    pos.vx = arg1->extra.tmd->coords->workm.t[0];
    pos.vy = arg1->extra.tmd->coords->workm.t[1];
    pos.vz = arg1->extra.tmd->coords->workm.t[2];
    Gp_UpdateActorColor(enemy, &pos, 0, 0);
    if (work->lightScale != ACTOR_123200_LIGHT_SCALE_FULL) {
        pos.vx = pos.vy = pos.vz = work->lightScale;
        ScaleMatrix(&work->lightMtx, &pos);
    }
    switch (gSceneCombatState.actorControl) {
        case SCENE_COMBAT_ACTORS_RUNNING:
            if (work->state != ACTOR_123200_STATE_HIDDEN) {
                arg1->extra.tmd->flags = 0;
                effectDrawGroundShadow(MATRIX_TRANS(&arg1->extra.tmd->coords->workm), 0x180, gRoomEffectState->groundShadowShade);
            }
            break;
        case SCENE_COMBAT_ACTORS_PAUSED:
            if (work->state != ACTOR_123200_STATE_HIDDEN) {
                arg1->extra.tmd->flags = 0;
                effectDrawGroundShadow(MATRIX_TRANS(&arg1->extra.tmd->coords->workm), 0x180, gRoomEffectState->groundShadowShade);
            }
            return;
        case SCENE_COMBAT_ACTORS_HIDDEN:
            arg1->extra.tmd->flags = TMD_OBJECT_SKIP_ACTIVE_DRAW;
            return;
    }
    if (work->prevState != work->state) {
        work->stateEntered = 1;
    } else {
        work->stateEntered = 0;
    }
    work->prevState = work->state;
    table.funcs[work->state](enemy, arg1);
    id = func_actor_123200_80133450(work);
    if (id != 0) {
        snd = id | ((enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8);
        pan = (s8)worldCoordGetOriginAudioPan(arg1->extra.tmd->coords);
        sndEvtRequestScriptStart(snd, pan, (s8)worldCoordGetOriginAudioDepth(arg1->extra.tmd->coords));
    }
    if (gGameSession->viewReady != 0) {
        arg1->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    }
}

/// The enemy's three task states -- spawn, per-frame tick and teardown -- which
/// `func_actor_123200_801341A8` runs by `Task::state`.
static const EnemyTaskFuncTable3 D_actor_123200_80131E30 = {
    {
        func_actor_123200_8013352C,
        func_actor_123200_80133BA0,
        enemyDestroy,
    },
};

/// Message handler (id 0x7D5 in `D_actor_123200_80137214`). `arg2` selects the
/// mode: 0 hides the model (`TmdObject.flags` bit 0x80), 1 clears its flags and
/// so shows it, 2 sets `TMD_OBJECT_SKIP_AUTO_BUFFER`, and 3 and 4 both clear
/// the flags and then set `TMD_OBJECT_SKIP_AUTO_BUFFER`. Modes 0 and 1 reinstate the model's buffers through
/// `tmdAllocPrimitiveBuffer` and set the work block's `state` to `ACTOR_123200_STATE_WALK`;
/// modes 2, 3 and 4 set it to `ACTOR_123200_STATE_HIDDEN`. `arg1` is unused. Always returns 0.
s32 func_actor_123200_80133E30(Task* task, s32 arg1, s32 arg2, s32 arg3)
{
    TmdObject*        obj;
    _Actor123200Work* work;

    obj  = task->extra.tmd;
    work = task->work;
    switch (arg2) {
        case 0:
            obj->flags = TMD_OBJECT_SKIP_ACTIVE_DRAW;
            tmdAllocPrimitiveBuffer(obj);
            work->state = ACTOR_123200_STATE_WALK;
            break;
        case 1:
            obj->flags = 0;
            tmdAllocPrimitiveBuffer(obj);
            work->state = ACTOR_123200_STATE_WALK;
            break;
        case 2:
            obj->flags |= TMD_OBJECT_SKIP_AUTO_BUFFER;
            work->state = ACTOR_123200_STATE_HIDDEN;
            break;
        case 3:
        case 4:
            obj->flags  = 0;
            work->state = ACTOR_123200_STATE_HIDDEN;
            obj->flags |= TMD_OBJECT_SKIP_AUTO_BUFFER;
            break;
    }
    return 0;
}

/// Message handler (id 0x7DB in `D_actor_123200_80137214`). Copies the
/// message's stage, area and low command byte into the work block and handles
/// type 0xB02 commands: 1 selects `ACTOR_123200_STATE_FIRST_STAGING_WALK`, lit
/// in full when the top nibble of the enemy's `placeKey` is 1 and at a quarter
/// otherwise; 2 selects `ACTOR_123200_STATE_WALK` lit in full; 3 selects
/// `ACTOR_123200_STATE_HIDDEN`. Always returns 0.
s32 func_actor_123200_80133EDC(Task* task, s32 arg1, ActorCommand* msg, s32 arg3)
{
    _Actor123200Work* work;
    Enemy*            enemy;

    work                   = task->work;
    enemy                  = (Enemy*)task->spawnArg2.pointer;
    work->lastCommandStage = msg->context.loc.stage;
    work->lastCommandArea  = msg->context.loc.area;
    work->lastCommand      = msg->command;
    if (msg->context.key == 0xB02) {
        switch (msg->command) {
            case 1:
                if ((enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) == 1) {
                    work->lightScale = ACTOR_123200_LIGHT_SCALE_FULL;
                } else {
                    work->lightScale = ACTOR_123200_LIGHT_SCALE_QUARTER;
                }
                work->state = ACTOR_123200_STATE_FIRST_STAGING_WALK;
                break;
            case 2:
                work->lightScale = ACTOR_123200_LIGHT_SCALE_FULL;
                work->state      = ACTOR_123200_STATE_WALK;
                break;
            case 3:
                work->state = ACTOR_123200_STATE_HIDDEN;
                break;
            case 0:
                break;
        }
    }
    return 0;
}

#include "../../shared/actor_messages_place.inc.c"

#include "../../shared/coord_math_yaw_scale.inc.c"

/// `ACTOR_123200_STATE_HIDDEN` handler (entry 0 of `D_actor_123200_80131E24`). On the
/// frame the state is entered (`stateEntered` set) it marks the enemy not lockable
/// and sets the model's flags to 0x80; it does nothing on later frames.
static void func_actor_123200_80134178(Enemy* arg0, Task* arg1)
{
    TmdObject* model;

    if (((_Actor123200Work*)arg1->work)->stateEntered != 0) {
        model                        = arg1->extra.tmd;
        arg0->node.state.parts.flags = WORLD_TARGET_NOT_LOCKABLE;
        model->flags                 = TMD_OBJECT_SKIP_ACTIVE_DRAW;
    }
}

/// Runs the handler of `D_actor_123200_80131E30` that `Task::state` selects --
/// spawn, per-frame tick or teardown -- on the enemy in `Task::spawnArg2`,
/// copying the table onto the stack before the call.
void func_actor_123200_801341A8(Task* task)
{
    EnemyTaskFuncTable3 sp;

    sp = D_actor_123200_80131E30;
    sp.funcs[task->state](task->spawnArg2.pointer, task);
}
