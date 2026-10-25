#include "actors/actor_323300.h"

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
#include "main/tmd.h"
#include "main/tmd_types.h"

#include "overlay.h"

#include "rooms/dryfield_toilet.h"
#include "../../shared/actor/actor_motion.h"
#include "../../shared/actor/actor_messages.h"
#include "../../shared/actor/model_morph.h"

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
    s32            bank;               // Index, in the package's animation bank table, of the bank the rig is bound to (`ACTOR_MOTION_ANIMATION_INDEX_NONE` before the first)
    s32            animId;             // Clip the slots were last seeded with, within `bank` (`ACTOR_MOTION_ANIMATION_INDEX_NONE` before the first)
    byte           unknown_448[0x4];   // Never accessed
    s32            transformCountdown; // Stage of the transformation, from `ACTOR_323300_TRANSFORM_START` down to 0 by `_STEP` a tick
    GfxCoord       unscaledParts[3];   // This tick's parts 3, 4 and 5 as they were before the scale; parts 4, 5 and 6 are parented to them in that order
    VECTOR         partBasePos[19];    // Translation of each part's coordinate as the spawn left it, entries 1 to 18; the tick shortens parts 4 and 5 from their `vy` and reads no other
    MATRIX         light;              // Light-direction matrix lent to the model object, identity
    MATRIX         color;              // Light-colour matrix lent to the model object, identity
} _Actor323300StrangerWork;
STATIC_ASSERT_SIZEOF(_Actor323300StrangerWork, 0x6B0);

/// Tuning for the toilet-event woman's turn, sound and delayed buffer release.
enum {
    ACTOR_323300_WOMAN_VIEW2_ATTENUATION = 40, // Signed sound-script attenuation; pan stays zero
    ACTOR_323300_WOMAN_TURN_STEP         = ACTOR_TRANSFORM_ANGLE_TURN / 64,
    ACTOR_323300_WOMAN_TURN_START        = 0,
    ACTOR_323300_WOMAN_TURN_RATE         = ANIMATION_RATE_ONE / 2,
    ACTOR_323300_WOMAN_IDLE_RATE         = 22, // Sixteenths of a normal-rate animation frame per tick
    ACTOR_323300_WOMAN_BUFFER_FREE_DELAY = 2,  // Active actor ticks before freeing on the next zero-count tick
};

static void _actor323300WomanSpawn(Task* task);
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

/// Toilet-event woman states 0 spawn, 1 update and 2 exit.
///
/// `Task::state` indexes a complete three-handler copy without a bounds check
/// or terminator. Update requires the initialized nineteen-part rig and work;
/// exit unlinks collision before releasing the enemy and task. Actor-control
/// holds suspend all three slots, including spawn and exit.
static const TaskFuncTable3 _gActor323300WomanTaskStates = { {
    _actor323300WomanSpawn,
    _actor323300WomanUpdate,
    _actor323300WomanExit,
} };

static s32 _actor323300WomanApplyCommand(Task* task, s32 messageId, const ActorCommand* command, const ActorTransform* placement);

static TmdSource _gAnmcWoman1Body;
static TmdSource _gStrangerLesserBody;
static void      _actor323300WomanTask(Task* task);
static void      _actor323300StrangerTask(Task* task);

static AnimationSet _gDryfieldToiletStrangerTransformClip;
static AnimationSet _gStrangerReactClip;
static AnimationSet _gStrangerOddAlertClip;

#include "assets/characters/anmc_woman_1/body.inc"

#include "assets/enemies/stranger/lesser/body.inc"

#include "assets/rooms/dryfield/toilet/anmc_woman_clips_1.inc"

/// The woman's bank-0 animation sets: clip 0 absent, clips 1..3 loaded.
///
/// The transformation's Stranger task has its own separate clip table.
/// `AnimationPlayRequest::animationId` indexes this inner table after bank
/// selection; playback borrows the table and sets while the package is loaded.
#include "assets/rooms/dryfield/toilet/woman_bank.inc"

/// Carrier-local banks for nineteen-part playback: only bank 0, clips 1..3.
///
/// `AnimationPlayRequest::source.index` selects the bank; `animationId` selects
/// an entry of its clip table. Playback borrows that table and loaded clip data.
static AnimationSet** const _gActorMotionAnimBanks19[1] SECTION(".data") = {
    _gActor323300WomanAnimSets,
};

TaskDesc gActor323300WomanStrangerTaskDescriptors[2] = {
    // 0: woman.
    {
        .header   = { .fields = { .flags = (TASK_BODY_TMD | TASK_DESC_SKIP_AUTO_MODEL_BUFFER), .priority = TASK_PRIORITY_CONTROL } },
        .callback = _actor323300WomanTask,
        .data     = { .model = &_gAnmcWoman1Body },
    },
    // 1: lesser stranger.
    {
        .header   = { .fields = { .flags = TASK_BODY_TMD, .priority = TASK_PRIORITY_CONTROL } },
        .callback = _actor323300StrangerTask,
        .data     = { .model = &_gStrangerLesserBody },
    },
};

/// Toilet-event woman's animation, placement, visibility and scene-command messages.
///
/// Installed on the nineteen-part model task after work and collision setup.
/// Requests are borrowed synchronously. Scene commands show the woman, control
/// her Lesser Stranger child, turn in place or emit spray. The final end marker
/// makes unsupported IDs return zero; keep the table live until task teardown.
static TaskMessageEntry _gActor323300WomanMessageTable[5] = {
    { ACTOR_MESSAGE_PLAY_ANIMATION, _actorMotionPlayAnim19 },
    { ACTOR_MESSAGE_PLACE, actorMsgPlaceEuler },
    { ACTOR_MESSAGE_SET_MODEL_DRAW, _actor323300WomanSetModelDraw },
    { ACTOR_COMMAND_MESSAGE_APPLY, _actor323300WomanApplyCommand },
    { TASK_MESSAGE_TABLE_END, NULL },
};

/// Initial toilet-event woman placement in the model root's parent frame.
///
/// XYZ is (-1664, 0, -1222) in integer coordinate units; Euler yaw is -1024,
/// a negative quarter turn, with zero pitch/roll. Borrowed during spawn only.
static const ActorTransform _gActor323300WomanSpawnPlacement SECTION(".data") = {
    .pos = { -1664, 0, -1222, 0 },
    .rot = { 0, -ACTOR_TRANSFORM_ANGLE_TURN / 4, 0, 0 },
};

enum { ACTOR_323300_WOMAN_SPAWN_ANIMATION = 1 };

/// Resets the toilet-event woman's bank-0 clip 1 during hidden spawn setup.
///
/// The nineteen-part animation handler reads the source index, not a set-table
/// pointer. Reset ignores the zero blend duration; this receiver does not read
/// the collision selector. The request is borrowed synchronously and its loaded
/// clip data stays live through playback.
static const AnimationPlayRequest _gActor323300WomanSpawnAnimationRequest SECTION(".data") = {
    .source               = { .index = 0 },
    .animationId          = ACTOR_323300_WOMAN_SPAWN_ANIMATION,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

enum { ACTOR_323300_WOMAN_TURN_ANIMATION    = 2,
       ACTOR_323300_WOMAN_TURN_BLEND_FRAMES = 10 };

/// Toilet-event woman's bank-0 turn clip, blended over ten normal-rate frames.
///
/// The request is borrowed synchronously. Existing playback blends into clip 2;
/// an unticked rig resets to it, and requesting the same clip preserves playback.
/// These actor handlers ignore the world-collision field; the retained value
/// disables it for handlers that interpret it.
static AnimationPlayRequest _gActor323300WomanTurnAnimationRequest = { { .index = 0 }, ACTOR_323300_WOMAN_TURN_ANIMATION, ANIMATION_BLEND_INTERPOLATE, ACTOR_323300_WOMAN_TURN_BLEND_FRAMES, ANIMATION_WORLD_COLLISION_DISABLE };

enum { ACTOR_323300_WOMAN_TURN_COMPLETE_ANIMATION = 3 };

/// Toilet-event woman's bank-0 completion clip after reaching her target yaw.
///
/// Dispatch borrows this request synchronously. Clip 3 blends over ten
/// normal-rate frames; an unticked rig resets instead. The actor retains
/// its existing clip bank and ignores the world-collision word.
static AnimationPlayRequest _gActor323300WomanTurnCompleteAnimationRequest = { { .index = 0 }, ACTOR_323300_WOMAN_TURN_COMPLETE_ANIMATION, ANIMATION_BLEND_INTERPOLATE, ACTOR_323300_WOMAN_TURN_BLEND_FRAMES, ANIMATION_WORLD_COLLISION_DISABLE };

#include "assets/rooms/dryfield/toilet/anmc_woman_clips_2.inc"

/// Lesser Stranger animation sets in bank 0: loaded clips 1..3, NULL at 0.
///
/// The transformation starts with clip 1; clips 2/3 are stored but have no established caller.
/// The animation bank borrows these descriptors for the model's lifetime.
#include "assets/rooms/dryfield/toilet/stranger_bank.inc"

/// Lesser Stranger animation banks; the only valid bank index is zero.
///
/// Borrows the loaded clip table (1..3, NULL at 0) throughout transformation playback.
static AnimationSet** _gActor323300StrangerAnimationBanks[1] = {
    _gActor323300StrangerAnimationSets,
};

/// Lesser Stranger spawn request: bank 0, clip 1, immediate reset without world collision.
///
/// Read synchronously by the spawn handler; the request owns no playback storage.
static AnimationPlayRequest _gActor323300StrangerInitialAnimation = { { .index = 0 }, 1, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

/// Retained Lesser Stranger clip-2/3 requests with no playback consumer.
///
/// Both select bank 0 with a twenty-frame blend and disabled world collision.
/// The local three-clip bank supplies these ids; only clip 1 is requested.
static AnimationPlayRequest _gActor323300UnusedStrangerAnimations2To3[2] = {
    { { .index = 0 }, 2, ANIMATION_BLEND_INTERPOLATE, 20, ANIMATION_WORLD_COLLISION_DISABLE },
    { { .index = 0 }, 3, ANIMATION_BLEND_INTERPOLATE, 20, ANIMATION_WORLD_COLLISION_DISABLE },
};

/// Lesser Stranger spawn placement in the Dryfield toilet's world frame.
///
/// Position is (-1700, 0, -1457) coordinate units and yaw is -1024 (4096 per turn).
/// Placement copies the transform synchronously; the fourth vector components are unused.
static ActorTransform _gActor323300StrangerInitialPlacement = { { -1700, 0, -1457, 0 }, { 0, -1024, 0, 0 } };

static void _actor323300StrangerSpawn(Task* task);
static void _actor323300StrangerUpdate(Task* task);
static void _actor323300StrangerTurnJoint(GfxCoord* joint, s16 yawDelta);

#include "../../shared/actor/model_morph_save_rest_shape.inc.c"

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

/// Initializes the toilet-event woman while the event has not yet been seen.
///
/// Requires the woman's nineteen-part model and the live enemy passed in the
/// task's second spawn argument. An already-seen event or primary-heap allocation
/// failure tears the task down. Success owns zeroed work until exit; the model
/// borrows its lighting matrices and the list-2 collision sphere borrows part 1
/// and a single contact entry. The model starts hidden at the event placement,
/// playing clip 1 with collision pair tests disabled. The toilet sound starts
/// at pan 0 and attenuation 40; initialization then advances to task state 1.
static void _actor323300WomanSpawn(Task* task)
{
    enum {
        ACTOR_323300_WOMAN_BODY_RADIUS    = 256, // Model-coordinate units
        ACTOR_323300_WOMAN_NO_BUFFER_FREE = -1,
    };
    _Actor323300WomanWork* work;
    TmdObject*             model;
    WorldCollisionBody*    body;

    if (gameFlagGetNibble(GAME_FLAG_TOILET_EVENT_SEEN) != 0 || (work = memCalloc(sizeof(_Actor323300WomanWork), 0)) == NULL) {
        enemyTaskExit(task);
        return;
    }
    task->work             = work;
    work->model.animId     = ACTOR_MOTION_ANIMATION_INDEX_NONE;
    work->model.bank       = ACTOR_MOTION_ANIMATION_INDEX_NONE;
    work->loopSoundEnabled = 1;
    work->freeCountdown    = ACTOR_323300_WOMAN_NO_BUFFER_FREE;
    _actor323300WomanBindLighting(task);
    // Establish the contact storage before the model's visibility controls pair tests.
    model                  = task->extra.tmd;
    body                   = &work->body;
    body->coord            = model->coords + 1;
    body->context.contacts = &work->contact;
    body->key              = WORLD_COLLISION_CONTACT_ENEMY_BODY;
    body->pos.vx           = 0;
    body->pos.vy           = 0;
    body->pos.vz           = 0;
    body->radius           = ACTOR_323300_WOMAN_BODY_RADIUS;
    body->flags            = WORLD_COLLISION_BODY_SPHERE;
    worldCollisionLinkBody(WORLD_COLLISION_LIST_ENEMY_BODIES, body);
    body->flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;
    worldCollisionInitContacts(body->context.contacts, 1, 0);
    // Install message handling, the initial pose and its looping sound.
    task->msgTable = _gActor323300WomanMessageTable;
    _actor323300WomanSetModelDraw(task, ACTOR_MESSAGE_SET_MODEL_DRAW, ACTOR_MESSAGE_DRAW_HIDE, 0);
    // The original direct placement call uses the animation tag; the callback ignores it.
    actorMsgPlaceEuler(task, ACTOR_MESSAGE_PLAY_ANIMATION, &_gActor323300WomanSpawnPlacement, 0);
    _actorMotionPlayAnim19(task, ACTOR_MESSAGE_PLAY_ANIMATION, &_gActor323300WomanSpawnAnimationRequest, 0);
    sndEvtRequestScriptStart(SOUND_AREA(GAME_STAGE_DRYFIELD, GAME_AREA_DRYFIELD_TOILET, 6), 0, ACTOR_323300_WOMAN_VIEW2_ATTENUATION);
    task->exitCallback = _actor323300WomanExit;
    task->state       += 1;
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
                if (gGameSession->location.view == 2) {
                    sndEvtRequestScriptStart(SOUND_AREA(GAME_STAGE_DRYFIELD, GAME_AREA_DRYFIELD_TOILET, 6), 0, ACTOR_323300_WOMAN_VIEW2_ATTENUATION);
                } else {
                    sndEvtRequestScriptStart(SOUND_AREA(GAME_STAGE_DRYFIELD, GAME_AREA_DRYFIELD_TOILET, 6), 0, 0);
                }
            } else if (gGameSession->viewReady != 0) {
                if (gGameSession->location.view == 2) {
                    sndEvtRequestScriptMix(SOUND_AREA(GAME_STAGE_DRYFIELD, GAME_AREA_DRYFIELD_TOILET, 6), 0, ACTOR_323300_WOMAN_VIEW2_ATTENUATION);
                } else {
                    sndEvtRequestScriptMix(SOUND_AREA(GAME_STAGE_DRYFIELD, GAME_AREA_DRYFIELD_TOILET, 6), 0, 0);
                }
            }
        }
    }
    if (!(model->flags & TMD_OBJECT_SKIP_ACTIVE_DRAW)) {
        if (worldCollisionProjectGroundPoint(GRAPHICS_MATRIX_TRANSLATION(&task->extra.tmd->coords[1].workm), &groundPoint) != 0) {
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
        case ACTOR_MESSAGE_DRAW_SHOW_SKIP_AUTO_BUFFER:
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

/// Applies a changed animation request to the woman's nineteen-part rig.
///
/// Requires initialized woman work, a live model and a borrowed request that
/// does not overlap playback storage. Bank 0 supplies the loaded clips 1..3;
/// stored IDs narrow to signed bytes. A bank change rebinds the rig and
/// invalidates its clip. Slots 1..18 blend for `blendFrames` normal-rate frames
/// (0..2047) if requested and already ticking, or reset otherwise, then tick
/// once. Repeating the clip leaves playback unchanged; its coordinates, poses
/// and clip data stay borrowed until playback ends.
static inline void _actor323300WomanApplyAnimationRequest(Task* task, const AnimationPlayRequest* request)
{
    _Actor323300WomanWork* work;
    TmdObject*             model;
    s32                    slotIndex;

    work  = task->work;
    model = task->extra.tmd;
    if (request->source.index != work->model.bank) {
        work->model.bank   = request->source.index;
        work->model.animId = ACTOR_MOTION_ANIMATION_INDEX_NONE;
        animationInitContext(&work->rig.anim, _gActorMotionAnimBanks19[work->model.bank], model,
                             work->rig.poses, work->rig.slots);
    }
    if (request->animationId != work->model.animId) {
        work->model.animId = request->animationId;
        if (request->blend != ANIMATION_BLEND_RESET && work->model.ticking != 0) {
            for (slotIndex = 1; slotIndex < (s32)ARRAY_SIZE(work->rig.slots); slotIndex++) {
                animationSeekSlotWithBlend(&work->rig.anim, slotIndex, work->model.animId, 0, request->blendFrames);
            }
        } else {
            for (slotIndex = 1; slotIndex < (s32)ARRAY_SIZE(work->rig.slots); slotIndex++) {
                animationResetSlot(&work->rig.anim, slotIndex, work->model.animId);
            }
        }
        for (slotIndex = 1; slotIndex < (s32)ARRAY_SIZE(work->rig.slots); slotIndex++) {
            animationTickSlot(&work->rig.anim, slotIndex);
        }
        work->model.ticking = 1;
    }
}

/// Applies the toilet-event woman's visibility, transformation, turn and spray commands.
///
/// Handles `ACTOR_COMMAND_MESSAGE_APPLY` with initialized woman work and a live
/// nineteen-part model: commands 0 show, 10 spawn the Lesser Stranger, copy the
/// woman's root matrix to it and disable sound retriggers, 11 kill that task and
/// stop the toilet sound, 12 turn in place to the placement yaw, and 13 emit
/// spray from part 6 for ten active effect ticks. A failed spawn still disables
/// retriggers; killing leaves the saved task pointer intact. Spawn and spray
/// require the toilet overlay's task and effect resources to remain loaded.
///
/// The command is borrowed and read only during dispatch. Command 12 also
/// requires a borrowed placement with XYZ position and rotation initialized;
/// the position is retained but does not move the actor. Angles use 4096 units
/// per turn. Other commands ignore placement. Context tags and message ID are
/// ignored, and every command, including an unknown one, returns 0.
static s32 _actor323300WomanApplyCommand(Task* task, s32 messageId, const ActorCommand* command, const ActorTransform* placement)
{
    enum {
        ACTOR_323300_WOMAN_COMMAND_SHOW              = 0,
        ACTOR_323300_WOMAN_COMMAND_SPAWN_STRANGER    = 10,
        ACTOR_323300_WOMAN_COMMAND_KILL_STRANGER     = 11,
        ACTOR_323300_WOMAN_COMMAND_TURN_TO_PLACEMENT = 12,
        ACTOR_323300_WOMAN_COMMAND_SPRAY             = 13,
        ACTOR_323300_STRANGER_TASK_INDEX             = 1,
        ACTOR_323300_WOMAN_TURN_CLIP                 = 2,
        ACTOR_323300_WOMAN_SPRAY_PART                = 6,
        ACTOR_323300_WOMAN_SPRAY_TICKS               = 10,
    };
    _Actor323300WomanWork* work;
    Task*                  strangerTask;
    GfxCoord*              womanRoot;
    GfxCoord*              strangerRoot;
    SVECTOR                sprayOffset;

    work = task->work;
    switch (command->command) {
        case ACTOR_323300_WOMAN_COMMAND_SHOW:
            _actor323300WomanSetModelDraw(task, ACTOR_MESSAGE_SET_MODEL_DRAW, ACTOR_MESSAGE_DRAW_SHOW, 0);
            break;
        case ACTOR_323300_WOMAN_COMMAND_SPAWN_STRANGER:
            strangerTask       = taskSpawnFromTable(gActor323300WomanStrangerTaskDescriptors, ACTOR_323300_STRANGER_TASK_INDEX, 0, 0);
            work->strangerTask = strangerTask;
            if (strangerTask != NULL) {
                // Seed the new root; the stranger's initialization later applies its placement preset.
                womanRoot           = task->extra.tmd->coords;
                strangerRoot        = strangerTask->extra.tmd->coords;
                strangerRoot->coord = womanRoot->coord;
            }
            work->loopSoundEnabled = 0;
            break;
        case ACTOR_323300_WOMAN_COMMAND_KILL_STRANGER:
            if (work->strangerTask != NULL) {
                taskKill(work->strangerTask);
            }
            work->loopSoundEnabled = 0;
            sndEvtRequestScriptStop(SOUND_AREA(GAME_STAGE_DRYFIELD, GAME_AREA_DRYFIELD_TOILET, 6), SOUND_SCRIPT_STOP_KEEP_RELEASE);
            break;
        case ACTOR_323300_WOMAN_COMMAND_TURN_TO_PLACEMENT:
            // Retain the complete target, but only yaw steers the in-place turn.
            work->walk.motion       = ACTOR_WALK_MOTION_WALKING;
            work->walk.motionStep   = ACTOR_323300_WOMAN_TURN_START;
            work->walk.target.vx    = placement->pos.vx;
            work->walk.target.vy    = placement->pos.vy;
            work->walk.target.vz    = placement->pos.vz;
            work->walk.targetRot.vx = placement->rot.vx;
            work->walk.targetRot.vy = placement->rot.vy;
            work->walk.targetRot.vz = placement->rot.vz;
            work->model.nextAnimId  = ACTOR_323300_WOMAN_TURN_CLIP;

            _actor323300WomanApplyAnimationRequest(task, &_gActor323300WomanTurnAnimationRequest);
            break;
        case ACTOR_323300_WOMAN_COMMAND_SPRAY:
            sprayOffset.vy = 60;
            sprayOffset.vx = 0;
            sprayOffset.vz = 200;
            effectSpawn(EFFECT_DRYFIELD_TOILET_SPRAY_EMITTER, &task->extra.tmd->coords[ACTOR_323300_WOMAN_SPRAY_PART], ACTOR_323300_WOMAN_SPRAY_TICKS, &sprayOffset);
            break;
    }
    return 0;
}

/// Dispatches the toilet-event woman's initialization, update or teardown state.
///
/// Requires `task->state` 0 (spawn), 1 (update) or 2 (exit); the table is copied
/// before selection and has no bounds check. Only `SCENE_COMBAT_ACTORS_RUNNING`
/// dispatches a state; paused or hidden actor control skips the call, including
/// the spawn and exit slots. The descriptor supplies the woman's nineteen-part TMD model;
/// update and exit require the work initialized by the spawn state.
static void _actor323300WomanTask(Task* task)
{
    TaskFuncTable3 stateHandlers;

    stateHandlers = _gActor323300WomanTaskStates;
    if (gSceneCombatState.actorControl == SCENE_COMBAT_ACTORS_RUNNING) {
        stateHandlers.funcs[task->state](task);
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
    _actorMotionPlayAnim19(task, ACTOR_MESSAGE_PLAY_ANIMATION, &_gActor323300WomanTurnAnimationRequest, 0);
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
        _actorMotionPlayAnim19(task, ACTOR_MESSAGE_PLAY_ANIMATION, &_gActor323300WomanTurnCompleteAnimationRequest, 0);
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

#include "../../shared/actor/actor_motion_play19.inc.c"

#include "../../shared/actor/actor_messages_place_euler.inc.c"

#include "../../shared/actor/model_morph_blend.inc.c"

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
    work->animId             = ACTOR_MOTION_ANIMATION_INDEX_NONE;
    work->bank               = ACTOR_MOTION_ANIMATION_INDEX_NONE;
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
    _actor323300StrangerPlayAnimation(task, ACTOR_MESSAGE_PLAY_ANIMATION, &_gActor323300StrangerInitialAnimation, 0);
    _actorMsgPlaceEuler(task, ACTOR_MESSAGE_PLAY_ANIMATION, &_gActor323300StrangerInitialPlacement, 0);

    // The room owns the shared rest snapshot for this transformation.
    _modelMorphSaveRestShape(task, &gDryfieldToiletLesserStrangerMorph);

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

    _modelMorphBlend(task, &gDryfieldToiletLesserStrangerMorph, morphRamp);
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
    MATRIX*                   light;
    MATRIX*                   color;
    GfxCoord*                 coords;
    TmdObject*                model;

    model  = task->extra.tmd;
    work   = task->work;
    coords = model->coords;

    light = &work->light;
    color = &work->color;
    gfxSetRotIdentity(light);
    gfxSetRotIdentity(color);

    model->lightMtx = light;
    model->colorMtx = color;

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
#include "../../shared/actor/actor_messages_place_euler.inc.c"
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
        work->animId = ACTOR_MOTION_ANIMATION_INDEX_NONE;
        animationInitContext(&work->rig.anim, _gActor323300StrangerAnimationBanks[work->bank], model,
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

/// Task-state callbacks for the Lesser Stranger's transforming body.
///
/// `Task::state` selects 0 spawn, 1 frame update or 2 restore model parents and kill.
/// Actor control gates all three states; the entries take the live task.
static const TaskFuncTable3 _gActor323300StrangerStates = { {
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

    stateHandlers = _gActor323300StrangerStates;
    if (gSceneCombatState.actorControl == SCENE_COMBAT_ACTORS_RUNNING) {
        stateHandlers.funcs[task->state](task);
    }
}
