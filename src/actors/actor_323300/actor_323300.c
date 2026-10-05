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
/// `actorMotionPlayAnim19` index by `_Actor323300WomanWork::model.bank`.
extern AnimationSet*  D_actor_323300_80172548[4];
extern AnimationSet** gActorMotionAnimBanks19[1];

/// Descriptor table the 0x7DB handler spawns its child from; index 1 is the
/// task kept in `_Actor323300WomanWork::strangerTask`.
extern TaskDesc D_actor_323300_8017255C[];

/// Placement `func_actor_323300_80161E78` hands `actorMsgPlaceEuler`.
extern ActorTransform D_actor_323300_8017259C;

/// Animation presets the spawn handler, the 0x7DB handler and the two states
/// hand `actorMotionPlayAnim19`.
extern AnimationPlayRequest D_actor_323300_801725B4;
extern AnimationPlayRequest D_actor_323300_801725C8;
extern AnimationPlayRequest D_actor_323300_801725DC;

/// Animation source table `func_actor_323300_80163718` indexes by
/// `_Actor323300StrangerWork::bank`. `func_actor_323300_80162BE4`
/// applies the preset `D_actor_323300_80174A74` through it and places the
/// actor at `D_actor_323300_80174AB0`.
extern AnimationSet*        D_actor_323300_80174A60[4];
extern AnimationSet**       D_actor_323300_80174A70[1];
extern AnimationPlayRequest D_actor_323300_80174A74;
extern ActorTransform       D_actor_323300_80174AB0;

/// Psy-Q `RotMatrixY` (it sits right after `RotMatrixX`).

static void func_actor_323300_80161E78(Task* arg0);
static void func_actor_323300_80161FE8(Task* arg0);
s32         func_actor_323300_80162208(Task* arg0, s32 arg1, s32 mode, s32 arg3);
static void func_actor_323300_8016269C(Task* arg0);
static void func_actor_323300_801626D0(Task* arg0);
static void func_actor_323300_801626EC(Task* arg0);
static void func_actor_323300_801626F4(Task* arg0);
static void func_actor_323300_80162748(Task* arg0);
static void func_actor_323300_801627B4(Task* arg0);
static void func_actor_323300_801634B0(Task* arg0);
static void func_actor_323300_80163510(Task* arg0);
static void func_actor_323300_8016359C(Task* arg0, s16 arg1);
static s32  func_actor_323300_8016369C(Task* arg0, s32 arg1, ActorTransform* transform, s32 arg3);
static s32  func_actor_323300_80163718(Task* arg0, s32 arg1, AnimationPlayRequest* arg2, s32 arg3);

/// State table `func_actor_323300_80162630` copies onto the stack and indexes
/// by `Task::state`: spawn, per-frame runner and exit of the woman.
static const TaskFuncTable3 D_actor_323300_80161E24 = { {
    func_actor_323300_80161E78,
    func_actor_323300_80161FE8,
    func_actor_323300_8016269C,
} };

s32 func_actor_323300_80162208(Task*, s32, s32, s32);
s32 func_actor_323300_80162360(Task* task, s32 msgId, ActorCommand* msg, ActorTransform* place);

static TmdSource _gActor323300AnmcWoman1Body;
static TmdSource _gActor323300LesserStrangerBody;
void             func_actor_323300_80162630(Task*);
void             func_actor_323300_80163840(Task*);

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
    { { { TASK_BODY_TMD, 192 } }, func_actor_323300_80163840, { .model = &_gActor323300LesserStrangerBody } },
};

TaskMessageEntry D_actor_323300_80172574[5] = {
    { ACTOR_MESSAGE_PLAY_ANIMATION, actorMotionPlayAnim19 },
    { ACTOR_MESSAGE_PLACE, actorMsgPlaceEuler },
    { ACTOR_MESSAGE_SET_MODEL_DRAW, func_actor_323300_80162208 },
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

static void func_actor_323300_80162BE4(Task* arg0);
static void func_actor_323300_80162DF0(Task* arg0);
static void func_actor_323300_80163188(GfxCoord* coord, s16 angle);

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
    func_actor_323300_801626D0(arg0);
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
    func_actor_323300_80162208(arg0, ACTOR_MESSAGE_SET_MODEL_DRAW, 0, 0);
    actorMsgPlaceEuler(arg0, 0x7D3, &D_actor_323300_8017259C, 0);
    actorMotionPlayAnim19(arg0, ACTOR_MESSAGE_PLAY_ANIMATION, &D_actor_323300_801725B4, 0);
    sndEvtRequestScriptStart(SOUND_AREA(GAME_STAGE_DRYFIELD, GAME_AREA_DRYFIELD_TOILET, 6), 0, 0x28);
    arg0->exitCallback = func_actor_323300_8016269C;
    arg0->state       += 1;
}

/// Per-frame runner of the woman: dispatches on `walk.motion` through the
/// two-entry handler table it builds on the stack, ticks the 18 animation
/// slots and, while `loopSoundEnabled` is set, posts the room's sound. Slot
/// 1's `ANIMATION_SLOT_FOLLOWED_JUMP` retriggers the script; otherwise a ready
/// view queues `sndEvtRequestScriptMix` for its first matching instance.
/// Both requests use pan 0 and attenuation 40 in view 2, 0 elsewhere.
/// The pan/attenuation pair `location.loc.view` picks between is built
/// twice so the two calls cross-jump into a shared `jal`. Then, unless
/// `TmdObject::flags` says the model is hidden, draws the ground shadow under
/// coordinate 1, empties `contact`, refreshes that coordinate's matrix and
/// colour, and ticks the `freeCountdown` that frees the model's buffers when
/// it reaches zero.
static void func_actor_323300_80161FE8(Task* arg0)
{
    TmdObject*             extra               = arg0->extra.tmd;
    _Actor323300WomanWork* work                = arg0->work;
    void                   (*states[2])(Task*) = {
        func_actor_323300_801626EC,
        func_actor_323300_801626F4,
    };
    VECTOR vec;
    s32    i;

    states[work->walk.motion](arg0);
    if (work->model.ticking != 0) {
        for (i = 1; i < 0x13; i++) {
            animationTickSlot(&work->rig.anim, i);
        }
        if (work->loopSoundEnabled != 0) {
            if (work->rig.slots[1].status.fields.flags & ANIMATION_SLOT_FOLLOWED_JUMP) {
                if (gGameSession->location.loc.view == 2) {
                    sndEvtRequestScriptStart(SOUND_AREA(GAME_STAGE_DRYFIELD, GAME_AREA_DRYFIELD_TOILET, 6), 0, 0x28);
                } else {
                    sndEvtRequestScriptStart(SOUND_AREA(GAME_STAGE_DRYFIELD, GAME_AREA_DRYFIELD_TOILET, 6), 0, 0);
                }
            } else if (gGameSession->viewReady != 0) {
                if (gGameSession->location.loc.view == 2) {
                    sndEvtRequestScriptMix(SOUND_AREA(GAME_STAGE_DRYFIELD, GAME_AREA_DRYFIELD_TOILET, 6), 0, 0x28);
                } else {
                    sndEvtRequestScriptMix(SOUND_AREA(GAME_STAGE_DRYFIELD, GAME_AREA_DRYFIELD_TOILET, 6), 0, 0);
                }
            }
        }
    }
    if (!(extra->flags & TMD_OBJECT_SKIP_ACTIVE_DRAW)) {
        if (func_800EA1A8(MATRIX_TRANS(&arg0->extra.tmd->coords[1].workm), (VECTOR3*)&vec) != 0) {
            effectDrawGroundShadow((VECTOR3*)&vec, 0x200, gRoomEffectState->groundShadowShade);
        }
        worldCollisionClearContacts(&work->contact);
        arg0->extra.tmd->coords[1].composeStamp = GRAPHICS_COORD_DIRTY;
        actorRenderComposeCoord(&arg0->extra.tmd->coords[1]);
        worldCoordSetModelLighting(extra, arg0->extra.tmd->coords[1].workm.t, 0, 3);
    }
    if (work->freeCountdown >= 0) {
        if (work->freeCountdown == 0) {
            tmdFreePrimitiveBuffer(extra);
        }
        work->freeCountdown--;
    }
}

/// Message-0x7D5 handler: the four-way visibility/mode switch on the message's
/// mode word, plus the pair-test toggles `_Actor323300WomanWork::body` needs.
///
/// Mode 0 hides the model -- `TmdObject::flags` bit 0x80, the bit
/// `func_actor_323300_80161FE8` tests before drawing the ground shadow -- and
/// clears `TMD_OBJECT_SKIP_AUTO_BUFFER` without allocating. Mode 1 shows it,
/// puts the node back in the pair walk, allocates the buffers and clears
/// `TMD_OBJECT_SKIP_AUTO_BUFFER`. Mode 2 hides it, arms the `freeCountdown`
/// the the per-frame runner frees the buffers with, and sets
/// `TMD_OBJECT_SKIP_AUTO_BUFFER` so the missing-buffer sweep does not refill
/// them. Mode 3 shows it and sets `TMD_OBJECT_SKIP_AUTO_BUFFER`. The countdown
/// itself does not test that bit. Anything else returns 1 and leaves the
/// object alone; the handled modes return 0.
///
/// The node's `WorldCollisionBody::flags` halfword is the induction variable, strided by one
/// `WorldCollisionBody` per step: the block owns a single node, so the walk covers one
/// element, but retail keeps the array shape. `WORLD_COLLISION_BODY_PAIR_ENABLED`
/// takes the node in and out of body-pair tests without unlinking it.
s32 func_actor_323300_80162208(Task* arg0, s32 arg1, s32 mode, s32 arg3)
{
    _Actor323300WomanWork* work;
    TmdObject*             extra;
    u16*                   flags;
    s32                    i;
    s32                    ret;

    extra = arg0->extra.tmd;
    work  = arg0->work;
    ret   = 0;

    switch (mode) {
        case 0:
            extra->flags |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
            flags         = &work->body.flags;
            for (i = 0; i < 1; i++) {
                flags[i * (sizeof(WorldCollisionBody) / sizeof(*flags))] &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
            }
            extra->flags &= ~TMD_OBJECT_SKIP_AUTO_BUFFER;
            break;
        case 1:
            extra->flags &= ~TMD_OBJECT_SKIP_ACTIVE_DRAW;
            flags         = &work->body.flags;
            for (i = 0; i < 1; i++) {
                flags[i * (sizeof(WorldCollisionBody) / sizeof(*flags))] |= WORLD_COLLISION_BODY_PAIR_ENABLED;
            }
            tmdAllocPrimitiveBuffer(extra);
            extra->flags &= ~TMD_OBJECT_SKIP_AUTO_BUFFER;
            break;
        case 2:
            extra->flags |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
            flags         = &work->body.flags;
            for (i = 0; i < 1; i++) {
                flags[i * (sizeof(WorldCollisionBody) / sizeof(*flags))] &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
            }
            work->freeCountdown = 2;
            extra->flags       |= TMD_OBJECT_SKIP_AUTO_BUFFER;
            break;
        case 3:
            extra->flags &= ~TMD_OBJECT_SKIP_ACTIVE_DRAW;
            flags         = &work->body.flags;
            for (i = 0; i < 1; i++) {
                flags[i * (sizeof(WorldCollisionBody) / sizeof(*flags))] |= WORLD_COLLISION_BODY_PAIR_ENABLED;
            }
            extra->flags |= TMD_OBJECT_SKIP_AUTO_BUFFER;
            break;
        default:
            ret = 1;
            break;
    }

    return ret;
}

/// Message 0x7DB handler, listed in `D_actor_323300_80172574` after the 0x7D3 /
/// 0x7D4 / 0x7D5 ones. The payload halfword selects one of five actions: 0
/// shows the model through the 0x7D5 visibility switch, 10/11 spawn and kill
/// the Lesser Stranger at `strangerTask`, 12 latches a placement and starts preset
/// `D_actor_323300_801725C8` (inlining the 0x7D3 preset body of
/// `actorMotionPlayAnim19`), 13 posts effect 0x600A2 on part 6.
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
            func_actor_323300_80162208(arg0, ACTOR_MESSAGE_SET_MODEL_DRAW, 1, 0);
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
            Gp_SpawnEff(EFFECT_DRYFIELD_TOILET_SPRAY_EMITTER, &arg0->extra.tmd->coords[6], 0xA, &vec);
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

static void func_actor_323300_8016269C(Task* arg0)
{
    worldCollisionUnlinkBody(&((_Actor323300WomanWork*)arg0->work)->body);
    enemyTaskExit(arg0);
}

static void func_actor_323300_801626D0(Task* arg0)
{
    TmdObject*             ext;
    _Actor323300WomanWork* work;

    ext           = arg0->extra.tmd;
    work          = arg0->work;
    ext->lightMtx = &work->model.light;
    ext->colorMtx = &work->model.color;
}

/// Index 0 of the two-entry table `func_actor_323300_80161FE8` builds on its
/// stack: the empty "hold" state.
static void func_actor_323300_801626EC(Task* arg0)
{
}

/// Index 1 of that table: re-dispatches on `walk.motionStep` through a second
/// two-entry table, the preset start and the turn-to-face step.
static void func_actor_323300_801626F4(Task* arg0)
{
    _Actor323300WomanWork* work                = arg0->work;
    void                   (*states[2])(Task*) = {
        func_actor_323300_80162748,
        func_actor_323300_801627B4,
    };

    states[work->walk.motionStep](arg0);
}

static void func_actor_323300_80162748(Task* arg0)
{
    _Actor323300WomanWork* work;
    s32                    i;

    work = arg0->work;
    actorMotionPlayAnim19(arg0, ACTOR_MESSAGE_PLAY_ANIMATION, &D_actor_323300_801725C8, 0);
    for (i = 1; i < 0x13; i++) {
        work->rig.slots[i].rate = 8;
    }
    work->walk.velocity.vx = 0;
    work->walk.velocity.vy = 0;
    work->walk.velocity.vz = 0;
    work->walk.motionStep++;
}

/// State handler at index 1 of the two-entry table `func_actor_323300_801626F4`
/// dispatches, the turn-to-face body. Euler-extracts the root coordinate into `vec`
/// and, while the yaw gap to the target `work->walk.targetRot.vy` stays under 0x41,
/// snaps `vec.vy` to that target, plays anim 0x7D3 through
/// `actorMotionPlayAnim19` and parks all 18 animation slots at 0x16 --
/// `walk.motion` and `walk.motionStep` go back to zero, so the handler re-runs. A wider
/// gap steps `vec.vy` toward the target by 0x40 instead. Either way the root
/// coordinate is rebuilt as the identity matrix rotated by `vec`, with `composeStamp`
/// cleared so the next `actorRenderComposeCoord` recomputes it.
static void func_actor_323300_801627B4(Task* arg0)
{
    _Actor323300WomanWork* work;
    GfxMatrix*             words;
    GfxCoord*              coord;
    SVECTOR                vec;
    s16                    diff;
    s32                    vy;
    s32                    i;

    coord = arg0->extra.tmd->coords;
    work  = arg0->work;

    gfxExtractSmallestEuler(&vec, &coord->coord);
    diff = (u16)work->walk.targetRot.vy - (u16)vec.vy;
    if (ABS(diff) >= 0x41) {
        vy = vec.vy;
        if (diff < 0) {
            vec.vy = vy - 0x40;
        } else {
            vec.vy = vy + 0x40;
        }
    } else {
        vec.vy = work->walk.targetRot.vy;
        actorMotionPlayAnim19(arg0, ACTOR_MESSAGE_PLAY_ANIMATION, &D_actor_323300_801725DC, 0);
        for (i = 1; i < 0x13; i++) {
            work->rig.slots[i].rate = 0x16;
        }
        work->walk.motion     = ACTOR_WALK_MOTION_IDLE;
        work->walk.motionStep = 0;
    }

    words                       = (GfxMatrix*)&coord->coord;
    words->rotationWords.m00M01 = ONE;
    words->rotationWords.m02M10 = 0;
    words->rotationWords.m11M12 = ONE;
    words->rotationWords.m20M21 = 0;
    words->rotationWords.m22    = ONE;
    RotMatrix(&vec, &coord->coord);
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
}

#include "../../shared/actor_motion_play19.inc.c"

#include "../../shared/actor_messages_place_euler.inc.c"

#include "../../shared/model_morph_blend.inc.c"

static void func_actor_323300_80162BE4(Task* arg0)
{
    _Actor323300StrangerWork* work;
    TmdObject*                extra;
    TmdSource*                src;
    GfxCoord*                 coord;
    ModelMorph*               morph;
    SVECTOR*                  dst;
    SVECTOR*                  nrm;
    SVECTOR*                  verts;
    SVECTOR*                  normals;
    s32                       i;
    s32                       part;

    extra              = arg0->extra.tmd;
    arg0->exitCallback = func_actor_323300_801634B0;
    work               = memCalloc(sizeof(_Actor323300StrangerWork), 0);
    if (work == NULL) {
        taskKill(arg0);
        return;
    }
    arg0->work                    = work;
    work->animId                  = ACTOR_MODEL_STATE_NONE;
    work->bank                    = ACTOR_MODEL_STATE_NONE;
    work->transformCountdown      = ACTOR_323300_TRANSFORM_START;
    extra->clutRowOffset          = 2;
    extra->layerTexturePageOffset = 2;
    extra->layerClutRowOffset     = 4;
    extra->texturePageOffset      = 0;
    extra->shading.colorBlend     = TMD_OBJECT_COLOR_BLEND_ONE - 1;
    extra->flags                 &= (u16)~TMD_OBJECT_SKIP_ACTIVE_DRAW;
    tmdBuildBufferHalf(extra);
    tmdBuildBufferHalf(extra);
    func_actor_323300_80163718(arg0, 0x7D3, &D_actor_323300_80174A74, 0);
    func_actor_323300_8016369C(arg0, 0x7D3, &D_actor_323300_80174AB0, 0);

    // Snapshot the model's rest shape into the room's morph record.
    morph = &D_dryfield_toilet_801865D0;
    src   = arg0->extra.tmd->source;
    dst   = morph->savedVertices;
    nrm   = morph->savedNormals;
    verts = src->verts;
    for (i = 0; i < morph->savedVertexCount; i++) {
        dst[i].vx = verts[i].vx;
        dst[i].vy = verts[i].vy;
        dst[i].vz = verts[i].vz;
    }
    if (morph->targetNormals != NULL) {
        normals = src->normals;
        for (i = 0; i < morph->normalCount; i++) {
            nrm[i].vx = normals[i].vx;
            nrm[i].vy = normals[i].vy;
            nrm[i].vz = normals[i].vz;
        }
    }

    func_actor_323300_80163510(arg0);
    // Record where the first clip's pose leaves each part, for the tick to shorten parts 4 and 5 from.
    for (part = 1; part < 0x13; part++) {
        coord = &arg0->extra.tmd->coords[part];
        setVector(&work->partBasePos[part], coord->coord.t[0], coord->coord.t[1], coord->coord.t[2]);
    }
    arg0->state += 1;
}

/// Per-frame runner of the Lesser Stranger, which carries its transformation
/// one step on: it ticks the 18 slots once a clip has been applied, turns
/// `_Actor323300StrangerWork::transformCountdown` into the 0..0xFFF ramp
/// `modelMorphBlend` deforms the model's vertices by, and stores that ramp in
/// `TmdObject::shading.colorBlend`, the colour weight with 12 fractional bits
/// used by the shading handlers. Once the countdown is below
/// `ACTOR_323300_TRANSFORM_TURN` the turn angle handed to
/// `func_actor_323300_8016359C` is a quarter of what has elapsed since.
///
/// The three coordinate nodes at parts 3..5 are then scaled: each is copied
/// off into `unscaledParts[0..2]` first, then scaled in place through
/// `ScaleMatrix` -- parts 3 and 4 to 0.2 on Y, part 5 by one -- and the
/// *copies* become the parents of parts 4, 5 and 6, so the scale does not
/// compound down the part chain. Parts 4 and 5 are drawn in toward their
/// parents on Y by the ramp: each keeps its recorded `partBasePos` height at
/// a zero ramp and a fifth of it at a full one. Part 6's shading is rebound to
/// its own translation before the countdown drops.
static void func_actor_323300_80162DF0(Task* arg0)
{
    _Actor323300StrangerWork* work;
    TmdObject*                extra;
    GfxCoord*                 coord;
    VECTOR                    vec;
    s32                       blend;
    s32                       i;

    work  = arg0->work;
    extra = arg0->extra.tmd;

    if (work->ticking != 0) {
        for (i = 1; i < 0x13; i++) {
            animationTickSlot(&work->rig.anim, i);
        }
    }

    // Morph ramp: full until the countdown reaches the morph stage, zero from the turn stage on.
    blend = work->transformCountdown;
    if (blend >= ACTOR_323300_TRANSFORM_MORPH) {
        blend = TMD_OBJECT_COLOR_BLEND_ONE - 1;
    } else if (blend > ACTOR_323300_TRANSFORM_TURN) {
        blend -= ACTOR_323300_TRANSFORM_TURN;
    } else {
        blend = 0;
    }

    modelMorphBlend(arg0, &D_dryfield_toilet_801865D0, blend);
    extra->shading.colorBlend = blend;

    if (work->transformCountdown < ACTOR_323300_TRANSFORM_TURN) {
        func_actor_323300_8016359C(arg0, (s16)(((ACTOR_323300_TRANSFORM_TURN - work->transformCountdown) << 14) >> 16));
    }

    // Scale parts 3 to 5 in place, hanging each next part from an unscaled copy.

    coord                  = &arg0->extra.tmd->coords[3];
    work->unscaledParts[0] = *coord;
    vec.vx                 = 0x1000;
    vec.vy                 = 0x333;
    vec.vz                 = 0x1000;
    ScaleMatrix(&coord->coord, &vec);

    coord                  = &arg0->extra.tmd->coords[4];
    work->unscaledParts[1] = *coord;
    coord->parent          = &work->unscaledParts[0];
    vec.vx                 = 0x1000;
    vec.vy                 = 0x333;
    vec.vz                 = 0x1000;
    ScaleMatrix(&coord->coord, &vec);
    coord->coord.t[1] = work->partBasePos[4].vy - work->partBasePos[4].vy * 0.8 * blend / 4096.0;

    coord                  = &arg0->extra.tmd->coords[5];
    work->unscaledParts[2] = *coord;
    coord->parent          = &work->unscaledParts[1];
    vec.vx                 = 0x1000;
    vec.vy                 = 0x1000;
    vec.vz                 = 0x1000;
    ScaleMatrix(&coord->coord, &vec);
    coord->coord.t[1] = work->partBasePos[5].vy - work->partBasePos[5].vy * 0.8 * blend / 4096.0;

    coord         = &arg0->extra.tmd->coords[6];
    coord->parent = &work->unscaledParts[2];
    worldCoordSetModelLighting(extra, coord->workm.t, 0, 3);

    work->transformCountdown -= ACTOR_323300_TRANSFORM_STEP;
    if (work->transformCountdown < 0) {
        work->transformCountdown = 0;
    }
}

/// Turns joint `coord` by `angle` about the world Y axis and then by half of it
/// about X, so the joint is pitched as well as turned: builds its world
/// rotation in a matrix carved off the scratchpad head, applies both turns,
/// converts the result back into the parent's frame, writes the 3x3 into the
/// joint and refreshes it.
static void func_actor_323300_80163188(GfxCoord* coord, s16 angle)
{
    MATRIX*   rotation;
    GfxCoord* out;

    SCRATCH_STACK_RESERVE_BLOCK(MATRIX);
    rotation = SCRATCH_STACK_CURSOR(MATRIX);
    actorAccumulateRotation(coord, rotation, &gGfxViewCoord);
    RotMatrixY(angle, rotation);
    RotMatrixX(angle / 2, rotation);
    out = actorLocalizeRotation(coord, rotation);
    memcpy(out->coord.m, rotation->m, sizeof(out->coord.m));
    out->composeStamp = GRAPHICS_COORD_DIRTY;
    actorRenderComposeCoord(out);
    SCRATCH_STACK_RELEASE_BLOCK(MATRIX);
}

/// Exit callback of the Lesser Stranger: puts parts 4 to 6 back under the
/// model's own parts 3 to 5, off the `_Actor323300StrangerWork::unscaledParts`
/// copies that go away with the work block, and kills the task.
static void func_actor_323300_801634B0(Task* arg0)
{
    GfxCoord* base;
    GfxCoord* node;
    GfxCoord* sub;

    do {
        base         = arg0->extra.tmd->coords;
        sub          = base + 3;
        node         = base + 4;
        node->parent = sub;
    } while (0);
    sub                               = arg0->extra.tmd->coords + 5;
    sub->parent                       = node;
    arg0->extra.tmd->coords[6].parent = sub;
    taskKill(arg0);
}

/// Sets `_Actor323300StrangerWork::light` and `color` to identity, lends them
/// to the model object as `TmdObject::lightMtx` / `colorMtx`, then re-derives model part 1's world
/// matrix -- clearing its dirty flag, rebuilding it from its parent and
/// rebinding the actor's shading to the part's translation.
static void func_actor_323300_80163510(Task* arg0)
{
    _Actor323300StrangerWork* work;
    GfxMatrix*                light;
    GfxMatrix*                color;
    GfxCoord*                 coords;
    TmdObject*                extra;

    extra  = arg0->extra.tmd;
    work   = arg0->work;
    coords = extra->coords;

    work->light.rotationWords.m00M01 = ONE;
    light                            = &work->light;
    light->rotationWords.m02M10      = 0;
    light->rotationWords.m11M12      = ONE;
    light->rotationWords.m20M21      = 0;
    light->rotationWords.m22         = ONE;

    work->color.rotationWords.m00M01 = ONE;
    color                            = &work->color;
    color->rotationWords.m02M10      = 0;
    color->rotationWords.m11M12      = ONE;
    color->rotationWords.m20M21      = 0;
    color->rotationWords.m22         = ONE;

    extra->lightMtx = &light->mat;
    extra->colorMtx = &color->mat;

    coords[1].composeStamp = GRAPHICS_COORD_DIRTY;
    actorRenderComposeCoord(&coords[1]);
    worldCoordSetModelLighting(extra, coords[1].workm.t, 0, 3);
}

/// Re-aims the per-part coordinate nodes at index 5 and index 2 from one turn
/// angle: the angle is clamped to +-0x400 -- a quarter turn either way -- then
/// `func_actor_323300_80163188` rebuilds node 5 from two thirds of it and node
/// 2 from half, and nodes 5 down to 2 have their dirty flag cleared so the next
/// `actorRenderComposeCoord` re-derives them. The lower clamp tests `arg1` rather than
/// the clamped copy; that is the same test, because the upper clamp has already
/// pinned the copy to 0x400 whenever the angle was out of range upwards.
static void func_actor_323300_8016359C(Task* arg0, s16 arg1)
{
    s16 var;

    var = arg1;
    if (var > 0x400) {
        var = 0x400;
    }
    if (arg1 < -0x400) {
        var = -0x400;
    }

    func_actor_323300_80163188(&arg0->extra.tmd->coords[5], (var * 2) / 3);
    func_actor_323300_80163188(&arg0->extra.tmd->coords[2], var / 2);

    arg0->extra.tmd->coords[5].composeStamp = GRAPHICS_COORD_DIRTY;
    arg0->extra.tmd->coords[4].composeStamp = GRAPHICS_COORD_DIRTY;
    arg0->extra.tmd->coords[3].composeStamp = GRAPHICS_COORD_DIRTY;
    arg0->extra.tmd->coords[2].composeStamp = GRAPHICS_COORD_DIRTY;
}

/// A second copy of the handler, under this file's own name.
#define actorMsgPlaceEuler func_actor_323300_8016369C
#include "../../shared/actor_messages_place_euler.inc.c"
#undef actorMsgPlaceEuler

/// Play-animation handler of the Lesser Stranger, the twin of
/// `actorMotionPlayAnim19` for a block that keeps its playback values as the
/// words of `_Actor323300StrangerWork`. A request for a bank the rig is not
/// bound to rebinds it: the bank is stored, the clip id is reset to
/// `ACTOR_MODEL_STATE_NONE` and the bank's sets go to `animationInitContext`
/// with the rig's context, pose buffer and slots. A different clip id then
/// restarts every slot 1..0x12 -- through `animationSeekSlotWithBlend` when
/// the request asks for a blend and a clip has been applied before, through
/// `animationResetSlot` otherwise -- ticks them once and sets `ticking`.
static s32 func_actor_323300_80163718(Task* arg0, s32 arg1, AnimationPlayRequest* arg2, s32 arg3)
{
    _Actor323300StrangerWork* work;
    TmdObject*                ext;
    s32                       i;

    work = arg0->work;
    ext  = arg0->extra.tmd;
    if (arg2->source.index != work->bank) {
        work->bank   = arg2->source.index;
        work->animId = ACTOR_MODEL_STATE_NONE;
        animationInitContext(&work->rig.anim, D_actor_323300_80174A70[work->bank], ext,
                             work->rig.poses, work->rig.slots);
    }
    if (arg2->animationId != work->animId) {
        work->animId = arg2->animationId;
        if (arg2->blend != ANIMATION_BLEND_RESET && work->ticking != 0) {
            for (i = 1; i < 0x13; i++) {
                animationSeekSlotWithBlend(&work->rig.anim, i, work->animId, 0, arg2->blendFrames);
            }
        } else {
            for (i = 1; i < 0x13; i++) {
                animationResetSlot(&work->rig.anim, i, work->animId);
            }
        }
        for (i = 1; i < 0x13; i++) {
            animationTickSlot(&work->rig.anim, i);
        }
        work->ticking = 1;
    }
    return 0;
}

static const TaskFuncTable3 D_actor_323300_80161E6C = { {
    func_actor_323300_80162BE4,
    func_actor_323300_80162DF0,
    func_actor_323300_801634B0,
} };

/// Per-frame dispatcher of the Lesser Stranger: runs its spawn, tick or exit
/// state from `D_actor_323300_80161E6C` by `Task::state`, skipping the frame
/// while the global freeze byte is set.
void func_actor_323300_80163840(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_actor_323300_80161E6C;
    if (gSceneCombatState.actorControl == SCENE_COMBAT_ACTORS_RUNNING) {
        sp.funcs[task->state](task);
    }
}
