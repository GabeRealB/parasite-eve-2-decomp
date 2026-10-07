#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/abs.h>
#include <psyq/inline_c.h>
#include <psyq/memory.h>

#include "common.h"
#include "gte.h"

#include "actors/actor.h"

#include "gameplay/actor.h"
#include "gameplay/actor_render.h"
#include "gameplay/animation.h"
#include "gameplay/effect_tasks.h"
#include "gameplay/enemy.h"
#include "gameplay/message.h"
#include "gameplay/room_effects.h"
#include "gameplay/scene.h"
#include "gameplay/scene_combat.h"
#include "gameplay/scene_runtime.h"
#include "gameplay/world_collision.h"
#include "gameplay/world_coords.h"

#include "main/coord.h"
#include "main/gameflag.h"
#include "main/gfx.h"
#include "main/gfx_types.h"
#include "main/mem.h"
#include "main/scratch.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/task_types.h"
#include "main/tmd.h"
#include "main/tmd_types.h"

#include "overlay.h"

#include "rooms/dryfield_toilet.h"
#include "../../shared/actor_motion.h"
#include "../../shared/actor_messages.h"
#include "../../shared/model_morph.h"

/// Work block of the woman of the Dryfield toilet event, the package's first
/// actor.
///
/// The task's spawn state allocates it zeroed and keeps it at `Task::work`
/// for the task's life. It opens with the two members the nineteen-part play
/// handler runs on (`ActorMotion19PlayWork`); the model object borrows
/// `model.light` and `model.color` for as long as the block lives. The
/// collision sphere and its contact table come next, ahead of the walk state,
/// so the block is not laid out as a library walker's and the walk is the
/// package's own: an actor command latches a placement and the actor turns in
/// place to the placement's yaw, without moving. `walk.target`,
/// `walk.lastDistance` and `model.nextAnimId` are written or left zero and
/// never read.
///
/// The tail is the task of the Lesser Stranger an actor command spawns over
/// this actor, the switch of the sound the clip loops, and the delayed free
/// of the model's buffers once the model has been hidden.
typedef struct {
    ActorAnimRig19        rig;              // Playback storage of the nineteen-part body model; slots 1 to 18 are driven
    ActorModelState       model;            // Clip and bank the rig plays, and the matrices the model is lit with
    WorldCollisionBody    body;             // Sphere of radius 0x100 at part 1 on list 2, key category 3; its pair tests follow the model's visibility
    WorldCollisionContact contact;          // One-entry contact table of `body`, emptied each tick the model is drawn; nothing reads what it records
    Task*                 strangerTask;     // Task of the package's Lesser Stranger, spawned and killed by actor commands; NULL before the spawn or when it failed, and not cleared by the kill
    ActorWalkState        walk;             // Turn in progress: `targetRot.vy` is the yaw it steers to, `motionStep` 0 starts the turn clip and 1 turns; `velocity` is only zeroed
    s16                   loopSoundEnabled; // 1 to retrigger sound on clip loops and update its mix between loops (pan 0; attenuation 40 in view 2, 0 elsewhere); 0 after stranger spawn or death
    s16                   freeCountdown;    // Ticks left before the model's buffers are freed, which the tick finding 0 does (-1 no free pending)
} _Actor323300WomanWork;
STATIC_ASSERT_SIZEOF(_Actor323300WomanWork, 0x504);

/// `_Actor323300StrangerWork::transformCountdown`: the stages of the Lesser
/// Stranger's transformation, which the countdown passes through from
/// `_START` down to 0.
enum {
    ACTOR_323300_TRANSFORM_START = 0x3000, // Seeded by the spawn; down to `_MORPH` the model holds the full morph
    ACTOR_323300_TRANSFORM_MORPH = 0x2000, // Below this the morph ramp is the countdown less `_TURN`, falling to the rest shape
    ACTOR_323300_TRANSFORM_TURN  = 0x1000, // Below this the model is at rest and the body turn grows by a quarter of what has elapsed
    ACTOR_323300_TRANSFORM_STEP  = 0x40,   // Taken off each tick
};

/// Work block of the Lesser Stranger the woman turns into, the package's
/// second actor.
///
/// The task's spawn state allocates it zeroed and keeps it at `Task::work`
/// for the task's life. It opens with the nineteen-part rig and what the rig
/// plays, the values an `ActorModelState` holds but kept as words and in
/// another order, so the package carries a play handler of its own for it.
/// The model object borrows `light` and `color` for as long as the block
/// lives.
///
/// The actor exists to transform: its model starts fully morphed by the
/// Dryfield toilet's `ModelMorph` record and `transformCountdown` carries it
/// to its rest shape and then through a turn of the body. Throughout, parts 3
/// and 4 are drawn flattened to a fifth of their height. So that the scale
/// does not reach the parts below them, each tick copies parts 3 to 5 into
/// `unscaledParts` before scaling them and parents parts 4 to 6 to the
/// copies; the exit callback puts those parts back under the model's own
/// coordinates before the block is freed.
///
/// Nothing in the package reads or writes `unknown_448`; the block is
/// allocated at its full size, so the bytes are its own, but nothing shows
/// what they hold.
typedef struct {
    ActorAnimRig19 rig;                // Playback storage of the nineteen-part body model; slots 1 to 18 are driven
    s32            ticking;            // Set once a clip has been applied, never cleared: the slots are ticked each frame and a request may blend from their pose
    s32            bank;               // Index, in the package's animation bank table, of the bank the rig is bound to (`ACTOR_MODEL_STATE_NONE` before the first)
    s32            animId;             // Clip the slots were last seeded with, within `bank` (`ACTOR_MODEL_STATE_NONE` before the first)
    byte           unknown_448[0x4];   // Never accessed
    s32            transformCountdown; // Stage of the transformation, from `ACTOR_323300_TRANSFORM_START` down to 0 by `_STEP` a tick
    GfxCoord       unscaledParts[3];   // This tick's parts 3, 4 and 5 as they were before the scale; parts 4, 5 and 6 are parented to them in that order
    VECTOR         partBasePos[19];    // Translation of each part's coordinate as the spawn left it, entries 1 to 18; the tick shortens parts 4 and 5 from their `vy` and reads no other
    GfxMatrix      light;              // Light-direction matrix lent to the model object, identity
    GfxMatrix      color;              // Light-colour matrix lent to the model object, identity
} _Actor323300StrangerWork;
STATIC_ASSERT_SIZEOF(_Actor323300StrangerWork, 0x6B0);

extern TaskMessageEntry D_actor_323300_80172574[];

/// Animation source table `func_actor_323300_80162360` and
/// `_actorMotionPlayAnim19` index by `_Actor323300WomanWork::model.bank`.
extern AnimationSet*  D_actor_323300_80172548[4];
extern AnimationSet** gActorMotionAnimBanks19[1];

/// Descriptor table the 0x7DB handler spawns its child from; index 1 is the
/// task kept in `_Actor323300WomanWork::strangerTask`.
extern TaskDesc D_actor_323300_8017255C[];

/// Placement `func_actor_323300_80161E78` hands `actorMsgPlaceEuler`.
extern ActorTransform D_actor_323300_8017259C;

/// Animation presets the spawn handler, the 0x7DB handler and the two states
/// hand `_actorMotionPlayAnim19`.
extern AnimationPlayRequest D_actor_323300_801725B4;
extern AnimationPlayRequest D_actor_323300_801725C8;
extern AnimationPlayRequest D_actor_323300_801725DC;

/// Animation source table `_actor323300StrangerPlayAnimation` indexes by
/// `_Actor323300StrangerWork::bank`. `_actor323300StrangerSpawn`
/// applies the preset `D_actor_323300_80174A74` through it and places the
/// actor at `D_actor_323300_80174AB0`.
extern AnimationSet*        D_actor_323300_80174A60[4];
extern AnimationSet**       D_actor_323300_80174A70[1];
extern AnimationPlayRequest D_actor_323300_80174A74;
extern ActorTransform       D_actor_323300_80174AB0;

/// Tuning for the toilet-event woman's turn, sound and delayed buffer release.
enum {
    ACTOR_323300_WOMAN_VIEW2_ATTENUATION          = 40, // Signed sound-script attenuation; pan stays zero
    ACTOR_323300_WOMAN_TURN_STEP                  = ACTOR_TRANSFORM_ANGLE_TURN / 64,
    ACTOR_323300_WOMAN_TURN_START                 = 0,
    ACTOR_323300_WOMAN_TURN_RATE                  = ANIMATION_RATE_ONE / 2,
    ACTOR_323300_WOMAN_IDLE_RATE                  = 22, // Sixteenths of a normal-rate animation frame per tick
    ACTOR_323300_WOMAN_BUFFER_FREE_DELAY          = 2,  // Active actor ticks before freeing on the next zero-count tick
    ACTOR_323300_WOMAN_DRAW_SHOW_SKIP_AUTO_BUFFER = 3,
};

static void func_actor_323300_80161E78(Task* arg0);
static void _actor323300WomanUpdate(Task* task);
static s32  _actor323300WomanSetModelDraw(Task* task, s32 messageId, s32 mode, s32 unusedArg);
static void _actor323300WomanExit(Task* task);
static void _actor323300WomanBindLighting(Task* task);
static void _actor323300WomanIdle(Task* task);
static void _actor323300WomanDispatchTurn(Task* task);
static void _actor323300WomanStartTurn(Task* task);
static void _actor323300WomanTurnTowardTarget(Task* task);
static void _actor323300StrangerExit(Task* task);
static void _actor323300StrangerInitLighting(Task* task);
static void _actor323300StrangerTurnBody(Task* task, s16 turnAngle);
static s32  _actorMsgPlaceEuler(Task* task, s32 msgId, const ActorTransform* placement, s32 unusedArg);
static s32  _actor323300StrangerPlayAnimation(Task* task, s32 messageId, const AnimationPlayRequest* request, s32 unusedArg);

/// State table `func_actor_323300_80162630` copies onto the stack and indexes
/// by `Task::state`: spawn, per-frame runner and exit of the woman.
static const TaskFuncTable3 D_actor_323300_80161E24 = { {
    func_actor_323300_80161E78,
    _actor323300WomanUpdate,
    _actor323300WomanExit,
} };

s32 func_actor_323300_80162360(Task* task, s32 msgId, ActorCommand* msg, ActorTransform* place);

static TmdSource _gActor323300AnmcWoman1Body;
static TmdSource _gActor323300LesserStrangerBody;
void             func_actor_323300_80162630(Task*);
static void      _actor323300StrangerTask(Task* task);

static AnimationSet _gActor323300Animation11FC4;
static AnimationSet _gActor323300Animation123E0;
static AnimationSet _gActor323300Animation12C18;

extern AnimationSet* D_actor_323300_80174A60[4];

static TmdBone _gActor323300AnmcWoman1BodySkeleton[19] = {
#include "assets/anmc_woman_1_body_skeleton.inc"
};

static u32 _gActor323300AnmcWoman1BodyPartVerts[19] = {
#include "assets/anmc_woman_1_body_partVerts.inc"
};

static SVECTOR _gActor323300AnmcWoman1BodyVerts[325] = {
#include "assets/anmc_woman_1_body_verts.inc"
};

static SVECTOR _gActor323300AnmcWoman1BodyNormals[384] = {
#include "assets/anmc_woman_1_body_normals.inc"
};

static u32 _gActor323300AnmcWoman1BodyStream[4109] = {
#include "assets/anmc_woman_1_body_stream.inc"
};

static TmdSource _gActor323300AnmcWoman1Body = {
    0,
    21132,
    7444,
    19,
    _gActor323300AnmcWoman1BodyPartVerts,
    _gActor323300AnmcWoman1BodyVerts,
    _gActor323300AnmcWoman1BodyNormals,
    _gActor323300AnmcWoman1BodySkeleton,
    _gActor323300AnmcWoman1BodyStream,
};

static TmdBone _gActor323300LesserStrangerBodySkeleton[19] = {
#include "assets/lesser_stranger_body_skeleton.inc"
};

static u32 _gActor323300LesserStrangerBodyPartVerts[19] = {
#include "assets/lesser_stranger_body_partVerts.inc"
};

static SVECTOR _gActor323300LesserStrangerBodyVerts[325] = {
#include "assets/lesser_stranger_body_verts.inc"
};

static SVECTOR _gActor323300LesserStrangerBodyNormals[1604] = {
#include "assets/lesser_stranger_body_normals.inc"
};

static u32 _gActor323300LesserStrangerBodyStream[4170] = {
#include "assets/lesser_stranger_body_stream.inc"
};

static TmdSource _gActor323300LesserStrangerBody = {
    0,
    42312,
    15928,
    19,
    _gActor323300LesserStrangerBodyPartVerts,
    _gActor323300LesserStrangerBodyVerts,
    _gActor323300LesserStrangerBodyNormals,
    _gActor323300LesserStrangerBodySkeleton,
    _gActor323300LesserStrangerBodyStream,
};

static AnimationPackedPose _gActor323300Animation0FFC4Bank1[18] = {
#include "assets/actor_323300_animation_0FFC4_bank1.inc"
};

static AnimationPackedRotation _gActor323300Animation0FFC4Bank4[274] = {
#include "assets/actor_323300_animation_0FFC4_bank4.inc"
};

static AnimationRecord _gActor323300Animation0FFC4Records[379] = {
#include "assets/actor_323300_animation_0FFC4_records.inc"
};

static u16 _gActor323300Animation0FFC4Indices[20] = {
#include "assets/actor_323300_animation_0FFC4_indices.inc"
};

static AnimationSet _gActor323300Animation0FFC4 = {
    _gActor323300Animation0FFC4Records,
    _gActor323300Animation0FFC4Indices,
    { NULL, _gActor323300Animation0FFC4Bank1, NULL, NULL, _gActor323300Animation0FFC4Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor323300Animation102C8Bank1[6] = {
#include "assets/actor_323300_animation_102C8_bank1.inc"
};

static AnimationPackedRotation _gActor323300Animation102C8Bank4[46] = {
#include "assets/actor_323300_animation_102C8_bank4.inc"
};

static AnimationRecord _gActor323300Animation102C8Records[109] = {
#include "assets/actor_323300_animation_102C8_records.inc"
};

static u16 _gActor323300Animation102C8Indices[20] = {
#include "assets/actor_323300_animation_102C8_indices.inc"
};

static AnimationSet _gActor323300Animation102C8 = {
    _gActor323300Animation102C8Records,
    _gActor323300Animation102C8Indices,
    { NULL, _gActor323300Animation102C8Bank1, NULL, NULL, _gActor323300Animation102C8Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor323300Animation10700Bank1[5] = {
#include "assets/actor_323300_animation_10700_bank1.inc"
};

static AnimationPackedRotation _gActor323300Animation10700Bank4[92] = {
#include "assets/actor_323300_animation_10700_bank4.inc"
};

static AnimationRecord _gActor323300Animation10700Records[143] = {
#include "assets/actor_323300_animation_10700_records.inc"
};

static u16 _gActor323300Animation10700Indices[20] = {
#include "assets/actor_323300_animation_10700_indices.inc"
};

static AnimationSet _gActor323300Animation10700 = {
    _gActor323300Animation10700Records,
    _gActor323300Animation10700Indices,
    { NULL, _gActor323300Animation10700Bank1, NULL, NULL, _gActor323300Animation10700Bank4, NULL, NULL, NULL },
};

AnimationSet* D_actor_323300_80172548[4] = {
    NULL,
    &_gActor323300Animation0FFC4,
    &_gActor323300Animation10700,
    &_gActor323300Animation102C8,
};

AnimationSet** gActorMotionAnimBanks19[1] = {
    D_actor_323300_80172548,
};

TaskDesc D_actor_323300_8017255C[2] = {
    { { { (TASK_BODY_TMD | TASK_DESC_SKIP_AUTO_MODEL_BUFFER), 192 } }, func_actor_323300_80162630, { .model = &_gActor323300AnmcWoman1Body } },
    { { { TASK_BODY_TMD, 192 } }, _actor323300StrangerTask, { .model = &_gActor323300LesserStrangerBody } },
};

TaskMessageEntry D_actor_323300_80172574[5] = {
    { ACTOR_MESSAGE_PLAY_ANIMATION, _actorMotionPlayAnim19 },
    { ACTOR_MESSAGE_PLACE, actorMsgPlaceEuler },
    { ACTOR_MESSAGE_SET_MODEL_DRAW, _actor323300WomanSetModelDraw },
    { ACTOR_COMMAND_MESSAGE_APPLY, func_actor_323300_80162360 },
    { TASK_MESSAGE_TABLE_END, NULL },
};

ActorTransform D_actor_323300_8017259C = { { -1664, 0, -1222, 0 }, { 0, -1024, 0, 0 } };

AnimationPlayRequest D_actor_323300_801725B4 = { { .sets = NULL }, 1, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_323300_801725C8 = { { .sets = NULL }, 2, ANIMATION_BLEND_INTERPOLATE, 10, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_323300_801725DC = { { .sets = NULL }, 3, ANIMATION_BLEND_INTERPOLATE, 10, ANIMATION_WORLD_COLLISION_DISABLE };

static AnimationPackedPose _gActor323300Animation11FC4Bank1[58] = {
#include "assets/actor_323300_animation_11FC4_bank1.inc"
};

static AnimationPackedRotation _gActor323300Animation11FC4Bank4[324] = {
#include "assets/actor_323300_animation_11FC4_bank4.inc"
};

static AnimationRecord _gActor323300Animation11FC4Records[1025] = {
#include "assets/actor_323300_animation_11FC4_records.inc"
};

static u16 _gActor323300Animation11FC4Indices[20] = {
#include "assets/actor_323300_animation_11FC4_indices.inc"
};

static AnimationSet _gActor323300Animation11FC4 = {
    _gActor323300Animation11FC4Records,
    _gActor323300Animation11FC4Indices,
    { NULL, _gActor323300Animation11FC4Bank1, NULL, NULL, _gActor323300Animation11FC4Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor323300Animation123E0Bank1[4] = {
#include "assets/actor_323300_animation_123E0_bank1.inc"
};

static AnimationPackedRotation _gActor323300Animation123E0Bank4[69] = {
#include "assets/actor_323300_animation_123E0_bank4.inc"
};

static AnimationRecord _gActor323300Animation123E0Records[162] = {
#include "assets/actor_323300_animation_123E0_records.inc"
};

static u16 _gActor323300Animation123E0Indices[20] = {
#include "assets/actor_323300_animation_123E0_indices.inc"
};

static AnimationSet _gActor323300Animation123E0 = {
    _gActor323300Animation123E0Records,
    _gActor323300Animation123E0Indices,
    { NULL, _gActor323300Animation123E0Bank1, NULL, NULL, _gActor323300Animation123E0Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor323300Animation12C18Bank1[16] = {
#include "assets/actor_323300_animation_12C18_bank1.inc"
};

static AnimationPackedRotation _gActor323300Animation12C18Bank4[199] = {
#include "assets/actor_323300_animation_12C18_bank4.inc"
};

static AnimationRecord _gActor323300Animation12C18Records[259] = {
#include "assets/actor_323300_animation_12C18_records.inc"
};

static u16 _gActor323300Animation12C18Indices[20] = {
#include "assets/actor_323300_animation_12C18_indices.inc"
};

static AnimationSet _gActor323300Animation12C18 = {
    _gActor323300Animation12C18Records,
    _gActor323300Animation12C18Indices,
    { NULL, _gActor323300Animation12C18Bank1, NULL, NULL, _gActor323300Animation12C18Bank4, NULL, NULL, NULL },
};

AnimationSet* D_actor_323300_80174A60[4] = {
    NULL,
    &_gActor323300Animation11FC4,
    &_gActor323300Animation123E0,
    &_gActor323300Animation12C18,
};

AnimationSet** D_actor_323300_80174A70[1] = {
    D_actor_323300_80174A60,
};

AnimationPlayRequest D_actor_323300_80174A74 = { { .index = 0 }, 1, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_323300_80174A88[2] = {
    { { .index = 0 }, 2, ANIMATION_BLEND_INTERPOLATE, 20, ANIMATION_WORLD_COLLISION_DISABLE },
    { { .index = 0 }, 3, ANIMATION_BLEND_INTERPOLATE, 20, ANIMATION_WORLD_COLLISION_DISABLE },
};

ActorTransform D_actor_323300_80174AB0 = { { -1700, 0, -1457, 0 }, { 0, -1024, 0, 0 } };

static void _actor323300StrangerSpawn(Task* task);
static void _actor323300StrangerUpdate(Task* task);
static void _actor323300StrangerTurnJoint(GfxCoord* joint, s16 yawDelta);

/// Saves a model's rest XYZ components into a morph's borrowed snapshot buffers.
///
/// The source and buffers must cover the record's vertex and normal counts.
/// Normals are saved only when target normals exist; fourth halfwords are
/// untouched. The record and snapshot remain owned by the loaded room, and
/// the saved shape must remain unchanged until its transformation finishes.
static inline void _modelMorphSaveRestShape(const Task* task, const ModelMorph* morph)
{
    const TmdSource* source;
    SVECTOR*         savedVertices;
    SVECTOR*         savedNormals;
    const SVECTOR*   vertices;
    const SVECTOR*   normals;
    s32              elementIndex;

    source        = task->extra.tmd->source;
    savedVertices = morph->savedVertices;
    savedNormals  = morph->savedNormals;
    vertices      = source->verts;
    for (elementIndex = 0; elementIndex < morph->savedVertexCount; elementIndex++) {
        savedVertices[elementIndex].vx = vertices[elementIndex].vx;
        savedVertices[elementIndex].vy = vertices[elementIndex].vy;
        savedVertices[elementIndex].vz = vertices[elementIndex].vz;
    }
    if (morph->targetNormals != NULL) {
        normals = source->normals;
        for (elementIndex = 0; elementIndex < morph->normalCount; elementIndex++) {
            savedNormals[elementIndex].vx = normals[elementIndex].vx;
            savedNormals[elementIndex].vy = normals[elementIndex].vy;
            savedNormals[elementIndex].vz = normals[elementIndex].vz;
        }
    }
}

/// Installs nine local rotation coefficients and refreshes a joint's composed transform.
///
/// Source rotation must be separate, readable and word-aligned. Translation,
/// alignment bytes and parent links stay intact. The joint's acyclic ancestor
/// chain remains live; composition may update ancestor caches and GTE registers.
static inline void _actorRenderInstallJointRotation(GfxCoord* joint, const MATRIX* localRotation)
{
    memcpy(joint->coord.m, localRotation->m, sizeof(joint->coord.m));
    joint->composeStamp = GRAPHICS_COORD_DIRTY;
    actorRenderComposeCoord(joint);
}

/// Allocates the `_Actor323300WomanWork` this actor's whole lifetime runs on,
/// links `body` with its one-entry contact table, then binds the message
/// handlers and the animation presets the state functions drive. Bails out
/// through `enemyTaskExit` when the room flag 0x60 is already set (the actor
/// already spawned) or the allocation fails.
static void func_actor_323300_80161E78(Task* arg0)
{
    _Actor323300WomanWork* work;
    TmdObject*             extra;
    WorldCollisionBody*    body;

    if (gameFlagGetNibble(GAME_FLAG_TOILET_EVENT_SEEN) != 0 || (work = memCalloc(sizeof(_Actor323300WomanWork), 0)) == NULL) {
        enemyTaskExit(arg0);
        return;
    }
    arg0->work             = work;
    work->model.animId     = ACTOR_MODEL_STATE_NONE;
    work->model.bank       = ACTOR_MODEL_STATE_NONE;
    work->loopSoundEnabled = 1;
    work->freeCountdown    = -1;
    _actor323300WomanBindLighting(arg0);
    extra                  = arg0->extra.tmd;
    body                   = &work->body;
    body->coord            = extra->coords + 1;
    body->context.contacts = &work->contact;
    body->key              = 0x30000;
    body->pos.vx           = 0;
    body->pos.vy           = 0;
    body->pos.vz           = 0;
    body->radius           = 0x100;
    body->flags            = WORLD_COLLISION_BODY_SPHERE;
    worldCollisionLinkBody(WORLD_COLLISION_LIST_ENEMY_BODIES, body);
    body->flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;
    worldCollisionInitContacts(body->context.contacts, 1, 0);
    arg0->msgTable = D_actor_323300_80172574;
    _actor323300WomanSetModelDraw(arg0, ACTOR_MESSAGE_SET_MODEL_DRAW, 0, 0);
    actorMsgPlaceEuler(arg0, ACTOR_MESSAGE_PLAY_ANIMATION, &D_actor_323300_8017259C, 0);
    _actorMotionPlayAnim19(arg0, ACTOR_MESSAGE_PLAY_ANIMATION, &D_actor_323300_801725B4, 0);
    sndEvtRequestScriptStart(SOUND_AREA(GAME_STAGE_DRYFIELD, GAME_AREA_DRYFIELD_TOILET, 6), 0, 0x28);
    arg0->exitCallback = _actor323300WomanExit;
    arg0->state       += 1;
}

/// Advances the toilet-event woman's motion, animation, sound and buffer-release delay.
///
/// Requires a live model and woman work with motion 0 (idle) or 1 (turning).
/// Tracks 1..18 advance when playback is initialized. While loop sound is
/// active, a control jump on track 1 retriggers the toilet script; a ready view
/// otherwise updates its pan and attenuation (pan 0; attenuation 40 in view 2,
/// 0 elsewhere). Visible frames draw the ground shadow, clear contacts and
/// refresh lighting. The pending buffer release counts active actor ticks;
/// the tick entering with zero frees the buffers and changes the count to -1.
static void _actor323300WomanUpdate(Task* task)
{
    TmdObject*             model             = task->extra.tmd;
    _Actor323300WomanWork* work              = task->work;
    TaskFunc               motionHandlers[2] = {
        _actor323300WomanIdle,
        _actor323300WomanDispatchTurn,
    };
    VECTOR3 groundPoint;
    s32     slotIndex;

    motionHandlers[work->walk.motion](task);
    if (work->model.ticking != 0) {
        for (slotIndex = 1; slotIndex < (s32)ARRAY_SIZE(work->rig.slots); slotIndex++) {
            animationTickSlot(&work->rig.anim, slotIndex);
        }
        if (work->loopSoundEnabled != 0) {
            // Clip jumps retrigger the script; a ready view otherwise updates the current mix.
            if (work->rig.slots[1].status.fields.flags & ANIMATION_SLOT_FOLLOWED_JUMP) {
                if (gGameSession->location.loc.view == 2) {
                    sndEvtRequestScriptStart(SOUND_AREA(GAME_STAGE_DRYFIELD, GAME_AREA_DRYFIELD_TOILET, 6), 0, ACTOR_323300_WOMAN_VIEW2_ATTENUATION);
                } else {
                    sndEvtRequestScriptStart(SOUND_AREA(GAME_STAGE_DRYFIELD, GAME_AREA_DRYFIELD_TOILET, 6), 0, 0);
                }
            } else if (gGameSession->viewReady != 0) {
                if (gGameSession->location.loc.view == 2) {
                    sndEvtRequestScriptMix(SOUND_AREA(GAME_STAGE_DRYFIELD, GAME_AREA_DRYFIELD_TOILET, 6), 0, ACTOR_323300_WOMAN_VIEW2_ATTENUATION);
                } else {
                    sndEvtRequestScriptMix(SOUND_AREA(GAME_STAGE_DRYFIELD, GAME_AREA_DRYFIELD_TOILET, 6), 0, 0);
                }
            }
        }
    }
    if (!(model->flags & TMD_OBJECT_SKIP_ACTIVE_DRAW)) {
        if (worldCollisionProjectGroundPoint(MATRIX_TRANS(&task->extra.tmd->coords[1].workm), &groundPoint) != 0) {
            effectDrawGroundShadow(&groundPoint, 0x200, gRoomEffectState->groundShadowShade);
        }
        worldCollisionClearContacts(&work->contact);
        task->extra.tmd->coords[1].composeStamp = GRAPHICS_COORD_DIRTY;
        actorRenderComposeCoord(&task->extra.tmd->coords[1]);
        worldCoordSetModelLighting(model, task->extra.tmd->coords[1].workm.t, 0, 3);
    }
    if (work->freeCountdown >= 0) {
        if (work->freeCountdown == 0) {
            tmdFreePrimitiveBuffer(model);
        }
        work->freeCountdown--;
    }
}

/// Sets the woman's visibility, collision pair tests and model-buffer policy.
///
/// Requires a live TMD model and woman work. Modes 0/1 hide/show and enable
/// automatic buffer recovery; mode 1 also allocates both buffer halves.
/// Mode 2 hides, disables recovery and sets the two-tick buffer-free delay;
/// mode 3 shows and disables recovery. Showing enables body-pair tests and
/// hiding disables them without unlinking the body. No mode cancels an
/// already pending free. Returns 0 for modes 0..3, 1 otherwise. The message
/// ID and second payload are ignored.
static s32 _actor323300WomanSetModelDraw(Task* task, s32 messageId, s32 mode, s32 unusedArg)
{
    _Actor323300WomanWork* work;
    TmdObject*             model;
    u16*                   bodyFlags;
    s32                    bodyIndex;
    s32                    result;

    model  = task->extra.tmd;
    work   = task->work;
    result = 0;

    switch (mode) {
        case ACTOR_MESSAGE_DRAW_HIDE:
            model->flags |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
            bodyFlags     = &work->body.flags;
            for (bodyIndex = 0; bodyIndex < 1; bodyIndex++) {
                bodyFlags[bodyIndex * (sizeof(WorldCollisionBody) / sizeof(*bodyFlags))] &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
            }
            model->flags &= ~TMD_OBJECT_SKIP_AUTO_BUFFER;
            break;
        case ACTOR_MESSAGE_DRAW_SHOW:
            model->flags &= ~TMD_OBJECT_SKIP_ACTIVE_DRAW;
            bodyFlags     = &work->body.flags;
            for (bodyIndex = 0; bodyIndex < 1; bodyIndex++) {
                bodyFlags[bodyIndex * (sizeof(WorldCollisionBody) / sizeof(*bodyFlags))] |= WORLD_COLLISION_BODY_PAIR_ENABLED;
            }
            tmdAllocPrimitiveBuffer(model);
            model->flags &= ~TMD_OBJECT_SKIP_AUTO_BUFFER;
            break;
        case ACTOR_MESSAGE_DRAW_HIDE_SKIP_AUTO_BUFFER:
            model->flags |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
            bodyFlags     = &work->body.flags;
            for (bodyIndex = 0; bodyIndex < 1; bodyIndex++) {
                bodyFlags[bodyIndex * (sizeof(WorldCollisionBody) / sizeof(*bodyFlags))] &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
            }
            work->freeCountdown = ACTOR_323300_WOMAN_BUFFER_FREE_DELAY;
            model->flags       |= TMD_OBJECT_SKIP_AUTO_BUFFER;
            break;
        case ACTOR_323300_WOMAN_DRAW_SHOW_SKIP_AUTO_BUFFER:
            model->flags &= ~TMD_OBJECT_SKIP_ACTIVE_DRAW;
            bodyFlags     = &work->body.flags;
            for (bodyIndex = 0; bodyIndex < 1; bodyIndex++) {
                bodyFlags[bodyIndex * (sizeof(WorldCollisionBody) / sizeof(*bodyFlags))] |= WORLD_COLLISION_BODY_PAIR_ENABLED;
            }
            model->flags |= TMD_OBJECT_SKIP_AUTO_BUFFER;
            break;
        default:
            result = 1;
            break;
    }

    return result;
}

/// Message 0x7DB handler, listed in `D_actor_323300_80172574` after the 0x7D3 /
/// 0x7D4 / 0x7D5 ones. The payload halfword selects one of five actions: 0
/// shows the model through the 0x7D5 visibility switch, 10/11 spawn and kill
/// the Lesser Stranger at `strangerTask`, 12 latches a placement and starts preset
/// `D_actor_323300_801725C8` (inlining the 0x7D3 preset body of
/// `_actorMotionPlayAnim19`), 13 posts effect 0x600A2 on part 6.
s32 func_actor_323300_80162360(Task* arg0, s32 arg1, ActorCommand* msg, ActorTransform* place)
{
    _Actor323300WomanWork* w;
    _Actor323300WomanWork* work;
    AnimationPlayRequest*  preset;
    TmdObject*             extra;
    Task*                  spawned;
    GfxCoord*              src;
    GfxCoord*              dst;
    SVECTOR                vec;
    s32                    i;

    w = arg0->work;
    switch (msg->command) {
        case 0:
            _actor323300WomanSetModelDraw(arg0, ACTOR_MESSAGE_SET_MODEL_DRAW, 1, 0);
            break;
        case 10:
            spawned         = taskSpawnFromTable(D_actor_323300_8017255C, 1, 0, 0);
            w->strangerTask = spawned;
            if (spawned != NULL) {
                // The stranger appears where the woman stands.
                src        = arg0->extra.tmd->coords;
                dst        = spawned->extra.tmd->coords;
                dst->coord = src->coord;
            }
            w->loopSoundEnabled = 0;
            break;
        case 11:
            if (w->strangerTask != NULL) {
                taskKill(w->strangerTask);
            }
            w->loopSoundEnabled = 0;
            sndEvtRequestScriptStop(SOUND_AREA(GAME_STAGE_DRYFIELD, GAME_AREA_DRYFIELD_TOILET, 6), SOUND_SCRIPT_STOP_KEEP_RELEASE);
            break;
        case 12:
            w->walk.motion       = ACTOR_WALK_MOTION_WALKING;
            w->walk.motionStep   = 0;
            w->walk.target.vx    = place->pos.vx;
            w->walk.target.vy    = place->pos.vy;
            w->walk.target.vz    = place->pos.vz;
            w->walk.targetRot.vx = place->rot.vx;
            w->walk.targetRot.vy = place->rot.vy;
            w->walk.targetRot.vz = place->rot.vz;
            w->model.nextAnimId  = 2;

            preset = &D_actor_323300_801725C8;
            work   = arg0->work;
            extra  = arg0->extra.tmd;
            if (preset->source.index != work->model.bank) {
                work->model.bank   = preset->source.index;
                work->model.animId = ACTOR_MODEL_STATE_NONE;
                animationInitContext(&work->rig.anim, gActorMotionAnimBanks19[work->model.bank], extra,
                                     work->rig.poses, work->rig.slots);
            }
            if (preset->animationId != work->model.animId) {
                work->model.animId = preset->animationId;
                if (preset->blend != ANIMATION_BLEND_RESET && work->model.ticking != 0) {
                    for (i = 1; i < 0x13; i++) {
                        animationSeekSlotWithBlend(&work->rig.anim, i, work->model.animId, 0, preset->blendFrames);
                    }
                } else {
                    for (i = 1; i < 0x13; i++) {
                        animationResetSlot(&work->rig.anim, i, work->model.animId);
                    }
                }
                for (i = 1; i < 0x13; i++) {
                    animationTickSlot(&work->rig.anim, i);
                }
                work->model.ticking = 1;
            }
            break;
        case 13:
            vec.vy = 0x3C;
            vec.vx = 0;
            vec.vz = 0xC8;
            effectSpawn(EFFECT_DRYFIELD_TOILET_SPRAY_EMITTER, &arg0->extra.tmd->coords[6], 0xA, &vec);
            break;
    }
    return 0;
}

/// Per-frame dispatcher of the woman: runs its spawn, tick or exit
/// state from `D_actor_323300_80161E24` by `Task::state`, skipping the frame
/// while the global freeze byte is set.
void func_actor_323300_80162630(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_actor_323300_80161E24;
    if (gSceneCombatState.actorControl == SCENE_COMBAT_ACTORS_RUNNING) {
        sp.funcs[task->state](task);
    }
}

/// Unlinks the woman's collision body before releasing her enemy and task storage.
///
/// Requires initialized woman work and a live enemy in the task's second
/// spawn argument. Teardown releases the work and its borrowed lighting
/// matrices; the stopped model's remaining storage follows task teardown.
static void _actor323300WomanExit(Task* task)
{
    _Actor323300WomanWork* work = task->work;

    worldCollisionUnlinkBody(&work->body);
    enemyTaskExit(task);
}

/// Binds the woman's model to the lighting matrices in her work block.
///
/// The live model borrows both matrices until teardown; this does not initialize
/// their coefficients. Woman work must remain allocated while the model draws.
static void _actor323300WomanBindLighting(Task* task)
{
    TmdObject*             model;
    _Actor323300WomanWork* work;

    model           = task->extra.tmd;
    work            = task->work;
    model->lightMtx = &work->model.light;
    model->colorMtx = &work->model.color;
}

/// Leaves the woman's idle motion unchanged; animation and sound run in the frame update.
static void _actor323300WomanIdle(Task* task)
{
}

/// Dispatches the woman's in-place turn to its start or tracking step.
///
/// Requires initialized woman work with `walk.motionStep` 0 (start) or 1
/// (tracking). Each handler receives the live task; the table has no bounds check.
static void _actor323300WomanDispatchTurn(Task* task)
{
    _Actor323300WomanWork* work            = task->work;
    TaskFunc               turnHandlers[2] = {
        _actor323300WomanStartTurn,
        _actor323300WomanTurnTowardTarget,
    };

    turnHandlers[work->walk.motionStep](task);
}

/// Starts the woman's turn clip at half a normal animation frame per tick.
///
/// Requires turn step 0 and initialized woman work. Tracks 1..18 are set to
/// rate 8 in sixteenths of a frame, the unused walk velocity is cleared, and
/// the turn advances to step 1. The target position does not move the model.
static void _actor323300WomanStartTurn(Task* task)
{
    _Actor323300WomanWork* work;
    s32                    slotIndex;

    work = task->work;
    _actorMotionPlayAnim19(task, ACTOR_MESSAGE_PLAY_ANIMATION, &D_actor_323300_801725C8, 0);
    for (slotIndex = 1; slotIndex < (s32)ARRAY_SIZE(work->rig.slots); slotIndex++) {
        work->rig.slots[slotIndex].rate = ACTOR_323300_WOMAN_TURN_RATE;
    }
    work->walk.velocity.vx = 0;
    work->walk.velocity.vy = 0;
    work->walk.velocity.vz = 0;
    work->walk.motionStep++;
}

/// Turns the woman's root toward the requested yaw without changing its translation.
///
/// Requires turn step 1 and a live nineteen-part model. Angles use 4096 units
/// per turn. The yaw difference is narrowed to signed 16 bits; each tick steps
/// 64 units or snaps when the gap is at most 64. Arrival selects its completion clip,
/// sets tracks 1..18 to rate 22 sixteenths of a frame and returns to idle motion.
/// The root rotation is rebuilt from the extracted Euler angles, discarding
/// scale and marking its composed transform dirty.
static void _actor323300WomanTurnTowardTarget(Task* task)
{
    _Actor323300WomanWork* work;
    GfxCoord*              root;
    SVECTOR                rootAngles;
    s16                    yawGap;
    s32                    currentYaw;
    s32                    slotIndex;

    root = task->extra.tmd->coords;
    work = task->work;

    gfxExtractSmallestEuler(&rootAngles, &root->coord);
    yawGap = (u16)work->walk.targetRot.vy - (u16)rootAngles.vy;
    if (ABS(yawGap) >= ACTOR_323300_WOMAN_TURN_STEP + 1) {
        currentYaw = rootAngles.vy;
        if (yawGap < 0) {
            rootAngles.vy = currentYaw - ACTOR_323300_WOMAN_TURN_STEP;
        } else {
            rootAngles.vy = currentYaw + ACTOR_323300_WOMAN_TURN_STEP;
        }
    } else {
        rootAngles.vy = work->walk.targetRot.vy;
        _actorMotionPlayAnim19(task, ACTOR_MESSAGE_PLAY_ANIMATION, &D_actor_323300_801725DC, 0);
        for (slotIndex = 1; slotIndex < (s32)ARRAY_SIZE(work->rig.slots); slotIndex++) {
            work->rig.slots[slotIndex].rate = ACTOR_323300_WOMAN_IDLE_RATE;
        }
        work->walk.motion     = ACTOR_WALK_MOTION_IDLE;
        work->walk.motionStep = ACTOR_323300_WOMAN_TURN_START;
    }

    gfxSetRotIdentity(&root->coord);
    RotMatrix(&rootAngles, &root->coord);
    root->composeStamp = GRAPHICS_COORD_DIRTY;
}

#include "../../shared/actor_motion_play19.inc.c"

#include "../../shared/actor_messages_place_euler.inc.c"

#include "../../shared/model_morph_blend.inc.c"

/// Initializes the Lesser Stranger's transformation task and rest-shape snapshot.
///
/// Allocates zeroed work from the primary heap; failure tears the task down.
/// Requires its nineteen-part model and the loaded toilet overlay's morph
/// record and writable snapshot buffers (325 vertices and 1604 normals).
/// Only XYZ components are saved, leaving each snapshot's fourth halfword
/// intact. The room's snapshot is shared, so another instance must not overwrite
/// it during this transformation. Model lighting borrows the allocated work;
/// part translations come from the first clip's initialized pose.
static void _actor323300StrangerSpawn(Task* task)
{
    _Actor323300StrangerWork* work;
    TmdObject*                model;
    GfxCoord*                 coord;
    s32                       partIndex;

    model              = task->extra.tmd;
    task->exitCallback = _actor323300StrangerExit;
    work               = memCalloc(sizeof(_Actor323300StrangerWork), 0);
    if (work == NULL) {
        taskKill(task);
        return;
    }
    task->work               = work;
    work->animId             = ACTOR_MODEL_STATE_NONE;
    work->bank               = ACTOR_MODEL_STATE_NONE;
    work->transformCountdown = ACTOR_323300_TRANSFORM_START;
    // Select the transformation palettes and build both primitive-buffer halves.
    model->clutRowOffset          = 2;
    model->layerTexturePageOffset = 2;
    model->layerClutRowOffset     = 4;
    model->texturePageOffset      = 0;
    model->shading.colorBlend     = TMD_OBJECT_COLOR_BLEND_ONE - 1;
    model->flags                 &= (u16)~TMD_OBJECT_SKIP_ACTIVE_DRAW;
    tmdBuildBufferHalf(model);
    tmdBuildBufferHalf(model);
    _actor323300StrangerPlayAnimation(task, ACTOR_MESSAGE_PLAY_ANIMATION, &D_actor_323300_80174A74, 0);
    _actorMsgPlaceEuler(task, ACTOR_MESSAGE_PLAY_ANIMATION, &D_actor_323300_80174AB0, 0);

    // The room owns the shared rest snapshot for this transformation.
    _modelMorphSaveRestShape(task, &D_dryfield_toilet_801865D0);

    _actor323300StrangerInitLighting(task);
    // Save the first clip's part translations before the transformation shortens parts 4 and 5.
    for (partIndex = 1; partIndex < (s32)ARRAY_SIZE(work->partBasePos); partIndex++) {
        coord = &task->extra.tmd->coords[partIndex];
        setVector(&work->partBasePos[partIndex], coord->coord.t[0], coord->coord.t[1], coord->coord.t[2]);
    }
    task->state += 1;
}

/// Advances the Lesser Stranger from the room's full morph to its rest shape, then turns its body.
///
/// Requires initialized Stranger work, tracks 1..18, live model coordinates and
/// the unchanged room-owned rest snapshot. The countdown starts at 0x3000 and
/// falls by 64 per active actor tick. It holds ramp 4095 through 0x2000, falls
/// to zero at 0x1000, then grows the body turn by a quarter of elapsed units.
/// Ramp coefficients have 12 fractional bits. Parts 3 and 4 are scaled to
/// 819/4096 on Y; parts 4 and 5 shorten toward one fifth of their saved Y
/// translation at full ramp. Parts 4..6 borrow unscaled coordinate copies in
/// the work block to prevent the scale propagating down the chain. Their parent
/// links must be restored before that block is released. The full-morph shape
/// and the reason for the constant Y scale remain unproven.
static void _actor323300StrangerUpdate(Task* task)
{
    _Actor323300StrangerWork* work;
    TmdObject*                model;
    GfxCoord*                 coord;
    VECTOR                    scale;
    s32                       morphRamp;
    s32                       slotIndex;

    work  = task->work;
    model = task->extra.tmd;

    if (work->ticking != 0) {
        for (slotIndex = 1; slotIndex < (s32)ARRAY_SIZE(work->rig.slots); slotIndex++) {
            animationTickSlot(&work->rig.anim, slotIndex);
        }
    }

    // Morph ramp: full until the countdown reaches the morph stage, zero from the turn stage on.
    morphRamp = work->transformCountdown;
    if (morphRamp >= ACTOR_323300_TRANSFORM_MORPH) {
        morphRamp = TMD_OBJECT_COLOR_BLEND_ONE - 1;
    } else if (morphRamp > ACTOR_323300_TRANSFORM_TURN) {
        morphRamp -= ACTOR_323300_TRANSFORM_TURN;
    } else {
        morphRamp = 0;
    }

    _modelMorphBlend(task, &D_dryfield_toilet_801865D0, morphRamp);
    model->shading.colorBlend = morphRamp;

    if (work->transformCountdown < ACTOR_323300_TRANSFORM_TURN) {
        _actor323300StrangerTurnBody(task, (s16)(((ACTOR_323300_TRANSFORM_TURN - work->transformCountdown) << 14) >> 16));
    }

    // Scale parts 3 to 5 in place, hanging each next part from an unscaled copy.

    coord                  = &task->extra.tmd->coords[3];
    work->unscaledParts[0] = *coord;
    scale.vx               = ONE;
    scale.vy               = ONE / 5;
    scale.vz               = ONE;
    ScaleMatrix(&coord->coord, &scale);

    coord                  = &task->extra.tmd->coords[4];
    work->unscaledParts[1] = *coord;
    coord->parent          = &work->unscaledParts[0];
    scale.vx               = ONE;
    scale.vy               = ONE / 5;
    scale.vz               = ONE;
    ScaleMatrix(&coord->coord, &scale);
    coord->coord.t[1] = work->partBasePos[4].vy - work->partBasePos[4].vy * 0.8 * morphRamp / (double)ONE;

    coord                  = &task->extra.tmd->coords[5];
    work->unscaledParts[2] = *coord;
    coord->parent          = &work->unscaledParts[1];
    scale.vx               = ONE;
    scale.vy               = ONE;
    scale.vz               = ONE;
    ScaleMatrix(&coord->coord, &scale);
    coord->coord.t[1] = work->partBasePos[5].vy - work->partBasePos[5].vy * 0.8 * morphRamp / (double)ONE;

    coord         = &task->extra.tmd->coords[6];
    coord->parent = &work->unscaledParts[2];
    worldCoordSetModelLighting(model, coord->workm.t, 0, 3);

    work->transformCountdown -= ACTOR_323300_TRANSFORM_STEP;
    if (work->transformCountdown < 0) {
        work->transformCountdown = 0;
    }
}

/// Adds world-space yaw and half as much pitch to a Stranger joint, then refreshes it.
///
/// `yawDelta` is signed in 4096 units per turn; pitch uses division toward zero.
/// Requires a writable joint with a non-NULL parent and a live acyclic chain
/// reaching `gGfxViewCoord`, plus word-aligned scratch-stack space for one
/// `MATRIX`. Only the nine rotation coefficients change; translation and parent
/// links stay intact. The scratch block is released before return and GTE
/// working registers change.
static void _actor323300StrangerTurnJoint(GfxCoord* joint, s16 yawDelta)
{
    MATRIX* worldRotation;

    worldRotation = SCRATCH_STACK_RESERVE_BLOCK(MATRIX);
    // Apply the extra yaw and pitch in world space, then return to the parent frame.
    _actorRenderAccumulateRotation(joint, worldRotation, &gGfxViewCoord);
    RotMatrixY(yawDelta, worldRotation);
    RotMatrixX(yawDelta / 2, worldRotation);
    _actorRenderLocalizeRotation(joint, worldRotation);
    _actorRenderInstallJointRotation(joint, worldRotation);
    SCRATCH_STACK_RELEASE_BLOCK(MATRIX);
}

/// Restores the Stranger's model parent links before task teardown releases its work.
///
/// Parts 4, 5 and 6 are returned to model parts 3, 4 and 5 respectively, ending
/// their borrowing of `unscaledParts`. Requires the live model coordinates;
/// `taskKill` then releases work and schedules or performs body teardown.
static void _actor323300StrangerExit(Task* task)
{
    GfxCoord* coords;
    GfxCoord* part4;
    GfxCoord* parent;

    // Restore links before taskKill releases the work-owned parent copies.
    do {
        coords        = task->extra.tmd->coords;
        parent        = coords + 3;
        part4         = coords + 4;
        part4->parent = parent;
    } while (0);
    parent                            = task->extra.tmd->coords + 5;
    parent->parent                    = part4;
    task->extra.tmd->coords[6].parent = parent;
    taskKill(task);
}

/// Initializes identity lighting matrices for the Stranger and samples lighting at part 1.
///
/// Requires the live model and allocated Stranger work. The model borrows the
/// work's light and colour matrices through teardown. Only their nine rotation
/// coefficients are initialized; translation stays intact. Part 1's composed
/// transform is refreshed before sampling the world's first three lights.
static void _actor323300StrangerInitLighting(Task* task)
{
    _Actor323300StrangerWork* work;
    GfxMatrix*                light;
    GfxMatrix*                color;
    GfxCoord*                 coords;
    TmdObject*                model;

    model  = task->extra.tmd;
    work   = task->work;
    coords = model->coords;

    light = &work->light;
    color = &work->color;
    gfxSetRotIdentity(&light->mat);
    gfxSetRotIdentity(&color->mat);

    model->lightMtx = &light->mat;
    model->colorMtx = &color->mat;

    coords[1].composeStamp = GRAPHICS_COORD_DIRTY;
    actorRenderComposeCoord(&coords[1]);
    worldCoordSetModelLighting(model, coords[1].workm.t, 0, 3);
}

/// Turns Stranger joints 5 and 2 by linked yaw-and-pitch angles.
///
/// `turnAngle` uses 4096 units per turn and is clamped to +/-1024. Joint 5
/// receives two thirds of the clamped angle; joint 2 receives half. Both receive
/// half their yaw as world pitch. Requires live model coordinates and the
/// joint helper's parent-chain and scratch-stack contract. Parts 2..5 are
/// marked dirty afterwards because the joint turns affect descendant caches.
static void _actor323300StrangerTurnBody(Task* task, s16 turnAngle)
{
    s16 clampedAngle;

    clampedAngle = turnAngle;
    if (clampedAngle > (ACTOR_TRANSFORM_ANGLE_TURN / 4)) {
        clampedAngle = (ACTOR_TRANSFORM_ANGLE_TURN / 4);
    }
    if (turnAngle < -(ACTOR_TRANSFORM_ANGLE_TURN / 4)) {
        clampedAngle = -(ACTOR_TRANSFORM_ANGLE_TURN / 4);
    }

    _actor323300StrangerTurnJoint(&task->extra.tmd->coords[5], (clampedAngle * 2) / 3);
    _actor323300StrangerTurnJoint(&task->extra.tmd->coords[2], clampedAngle / 2);

    task->extra.tmd->coords[5].composeStamp = GRAPHICS_COORD_DIRTY;
    task->extra.tmd->coords[4].composeStamp = GRAPHICS_COORD_DIRTY;
    task->extra.tmd->coords[3].composeStamp = GRAPHICS_COORD_DIRTY;
    task->extra.tmd->coords[2].composeStamp = GRAPHICS_COORD_DIRTY;
}

/// Selects the private placement handler for the Lesser Stranger model task.
#define ACTOR_MESSAGE_PLACE_EULER_HANDLER _actorMsgPlaceEuler
#include "../../shared/actor_messages_place_euler.inc.c"
#undef ACTOR_MESSAGE_PLACE_EULER_HANDLER

/// Applies a borrowed animation request to the Lesser Stranger's nineteen-part rig.
///
/// The request's bank index must be 0 and its clip index 1..3 in that bank's
/// loaded table. A bank change rebinds the rig and invalidates the selected clip.
/// A changed clip resets tracks 1..18, or blends from the existing pose when
/// requested and playback was initialized; blend duration is in normal-rate
/// frames. The tracks are ticked once to apply the pose. Repeating the selected
/// clip does nothing, including ignoring new blend settings. Request storage
/// is read only during this call; bank data and model storage remain borrowed.
/// Returns 0; the message ID and second payload are ignored.
static s32 _actor323300StrangerPlayAnimation(Task* task, s32 messageId, const AnimationPlayRequest* request, s32 unusedArg)
{
    _Actor323300StrangerWork* work;
    TmdObject*                model;
    s32                       slotIndex;

    work  = task->work;
    model = task->extra.tmd;
    if (request->source.index != work->bank) {
        work->bank   = request->source.index;
        work->animId = ACTOR_MODEL_STATE_NONE;
        animationInitContext(&work->rig.anim, D_actor_323300_80174A70[work->bank], model,
                             work->rig.poses, work->rig.slots);
    }
    if (request->animationId != work->animId) {
        work->animId = request->animationId;
        if (request->blend != ANIMATION_BLEND_RESET && work->ticking != 0) {
            for (slotIndex = 1; slotIndex < (s32)ARRAY_SIZE(work->rig.slots); slotIndex++) {
                animationSeekSlotWithBlend(&work->rig.anim, slotIndex, work->animId, 0, request->blendFrames);
            }
        } else {
            for (slotIndex = 1; slotIndex < (s32)ARRAY_SIZE(work->rig.slots); slotIndex++) {
                animationResetSlot(&work->rig.anim, slotIndex, work->animId);
            }
        }
        for (slotIndex = 1; slotIndex < (s32)ARRAY_SIZE(work->rig.slots); slotIndex++) {
            animationTickSlot(&work->rig.anim, slotIndex);
        }
        work->ticking = 1;
    }
    return 0;
}

static const TaskFuncTable3 D_actor_323300_80161E6C = { {
    _actor323300StrangerSpawn,
    _actor323300StrangerUpdate,
    _actor323300StrangerExit,
} };

/// Dispatches the Lesser Stranger's spawn, transformation or exit state each active frame.
///
/// `Task::state` must be 0 (spawn), 1 (update) or 2 (exit), with a live model
/// and the work expected by that state. The global actor-control gate pauses
/// all three states. The state table is copied by value and has no bounds check.
static void _actor323300StrangerTask(Task* task)
{
    TaskFuncTable3 stateHandlers;

    stateHandlers = D_actor_323300_80161E6C;
    if (gSceneCombatState.actorControl == SCENE_COMBAT_ACTORS_RUNNING) {
        stateHandlers.funcs[task->state](task);
    }
}
