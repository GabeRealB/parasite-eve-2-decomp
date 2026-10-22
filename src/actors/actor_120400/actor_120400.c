#include "actors/task_tables.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/abs.h>

#include "common.h"

#include "actors/actor.h"

#include "gameplay/actor_render.h"
#include "gameplay/animation.h"
#include "gameplay/area.h"
#include "gameplay/areaplace.h"
#include "gameplay/effect_tasks.h"
#include "gameplay/enemy.h"
#include "gameplay/message.h"
#include "gameplay/room_effects.h"
#include "gameplay/scene.h"
#include "gameplay/scene_combat.h"
#include "gameplay/scene_runtime.h"
#include "gameplay/world_coords.h"

#include "main/coord.h"
#include "main/gfx.h"
#include "main/gfx_types.h"
#include "main/mem.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/task.h"
#include "main/tmd.h"
#include "main/tmd_types.h"
#include "../../shared/actor/model_placement.h"
#include "../../shared/actor/actor_motion.h"

static AnimationSet** _gActorMotionAnimBanks[1];

#include "../../shared/actor/actor_motion_play_helpers.h"
#include "../../shared/actor/actor_messages.h"

/// Work block of Kyle Madigan's body, the package's scripted walker.
///
/// The task's spawn state allocates it zeroed and keeps it at `Task::work`
/// for the task's life. It opens as `ActorMotionWalkWork` does - the
/// twenty-part rig, the model state and the walk a room script sends the
/// actor on - and the model object borrows `model.light` and `model.color`
/// for as long as the block lives.
///
/// What follows `walk` is the package's own: the delayed free of the model's
/// buffers once the model has been hidden. The tasks of the two hand models
/// are not kept here: each finds the body through its own spawn argument.
typedef struct {
    ActorAnimRig20  rig;            // Playback storage of the twenty-part body model; slots 1 to 19 are driven
    ActorModelState model;          // Clip and bank the rig plays, and the matrices the model is lit with
    ActorWalkState  walk;           // Destination, closing rotation, per-frame velocity and step of the walk in progress
    byte            field_4FC[0x4]; // Allocated but never accessed; role unproven
    s16             freeCountdown;  // Ticks left before the model's buffers are freed, which the tick finding 0 does (-1 no free pending)
} _Actor120400KyleMadiganWork;
STATIC_ASSERT_SIZEOF(_Actor120400KyleMadiganWork, 0x504);

static void _actor120400SpawnKyleMadiganWalker(Task* task);
static void _modelPlacementMirrorParentDrawFlags(Task* childTask);
static void _actor120400UpdateKyleMadiganWalker(Task* task);
static void _actor120400ExitKyleMadiganWalker(Task* task);
static void _actor120400BindKyleMadiganWalkerLighting(Task* task);
static void _actor120400IdleKyleMadiganWalk(Task* task);
static void _actor120400RunKyleMadiganWalkStep(Task* task);
static void _actor120400BeginKyleMadiganWalk(Task* task);

/// Task-state callbacks shared by Kyle Madigan's left and right hand models.
///
/// `Task::state` selects 0 attach, 1 inherit the body's drawing and buffer
/// policy or 2 kill the hand. The body supplies borrowed coordinates and lighting.
static const TaskFuncTable3 _gActor120400KyleMadiganHandStates = { {
    _modelPlacementAttachChild,
    _modelPlacementMirrorParentDrawFlags,
    taskKill,
} };

/// Kyle Madigan's body lifecycle: 0 initialize, 1 update, 2 release the actor.
///
/// Task state indexes a complete twelve-byte value copy, without a terminator.
/// Dispatch requires this package loaded and initialized work after state 0.
static const TaskFuncTable3 _gActor120400KyleMadiganWalkerStates = { {
    _actor120400SpawnKyleMadiganWalker,
    _actor120400UpdateKyleMadiganWalker,
    _actor120400ExitKyleMadiganWalker,
} };

/// Kyle Madigan's four scripted-walk steps, copied before dispatch.
///
/// `walk.motionStep` must be in 0..3; the closing turn returns motion to idle.
static const TaskFuncTable4 _gActor120400KyleMadiganWalkSteps = { {
    _actorMotionFaceTarget,           // 0: face the destination
    _actor120400BeginKyleMadiganWalk, // 1: begin forward motion
    _actorMotionArrive,               // 2: stop on arrival
    _actorMotionTurnToYaw,            // 3: face the destination yaw and return to idle
} };

enum { ACTOR_120400_KYLE_WALK_VELOCITY_ONE = 0x10000 };

/// Kyle Madigan's local +Z walk velocity: 32 coordinate units per update in signed 16.16.
///
/// The root's Q12 rotation converts XYZ to parent-space velocity without
/// translation. The fourth VECTOR word is zero and is not part of the motion.
static const VECTOR _gActor120400KyleMadiganForwardWalkVelocity = { 0, 0, 32 * ACTOR_120400_KYLE_WALK_VELOCITY_ONE, 0 };

extern TmdSource gKyleBody;
extern TmdSource gKyleHandRight;
extern TmdSource gKyleHandLeft2;
static s32       _actor120400StartKyleMadiganWalk(Task* task, s32 msgId, const ActorTransform* placement,
                                                  const ActorMotionWalkAnim* walkAnim);
static s32       _actor120400SetKyleMadiganDrawMode(Task* task, s32 msgId, s32 mode, s32 unusedArg);
static s32       _actor120400IgnoreKyleMadiganCommand(Task* task, s32 msgId, s32 unusedFirstArg, s32 unusedArg);
static void      _actor120400KyleMadiganHandTask(Task* handTask);
static void      _actor120400KyleMadiganWalkerTask(Task* task);

#include "assets/characters/kyle/body.inc"

#include "assets/characters/kyle/hand_right.inc"

#include "assets/characters/kyle/hand_left_2.inc"

#include "assets/rooms/dryfield/water_tank/kyle_clips.inc"

/// Kyle Madigan's bank-0 clip table for his twenty-part body rig.
///
/// Clip IDs 1..28 borrow loaded sets; entry 0 is NULL and cannot be played.
/// The table and animation resources must remain loaded while the rig uses them.
#include "assets/rooms/dryfield/water_tank/kyle_bank.inc"

/// Actor-motion bank selection for Kyle's twenty-part body (only source.index 0).
///
/// Requests select loaded clips 1..28. This table and its borrowed sets must
/// remain loaded for playback; child hand models do not use this binding.
static AnimationSet** _gActorMotionAnimBanks[1] = {
    _gActor120400KyleMadiganAnimationBank,
};

enum { ACTOR_120400_KYLE_TASK_PRIORITY = 192 };

TaskDesc gActor120400KyleMadiganTaskDescriptors[3] = {
    // 0: body.
    {
        .header   = { .fields = { .flags = (TASK_BODY_TMD | TASK_DESC_SKIP_AUTO_MODEL_BUFFER), .priority = ACTOR_120400_KYLE_TASK_PRIORITY } },
        .callback = _actor120400KyleMadiganWalkerTask,
        .data     = { .model = &gKyleBody },
    },
    // 1: left hand.
    {
        .header   = { .fields = { .flags = TASK_BODY_TMD, .priority = ACTOR_120400_KYLE_TASK_PRIORITY } },
        .callback = _actor120400KyleMadiganHandTask,
        .data     = { .model = &gKyleHandLeft2 },
    },
    // 2: right hand.
    {
        .header   = { .fields = { .flags = TASK_BODY_TMD, .priority = ACTOR_120400_KYLE_TASK_PRIORITY } },
        .callback = _actor120400KyleMadiganHandTask,
        .data     = { .model = &gKyleHandRight },
    },
};

/// Kyle Madigan's body messages for animation, placement, drawing and scripted walking.
///
/// Animation borrows a play request; placement and walking borrow a transform,
/// with optional start/closing clips in the walk's second word. Draw mode is an
/// integer; actor commands are ignored. Five handlers precede the end marker.
/// The body borrows this table and its callback code for its active lifetime.
static const TaskMessageEntry _gActor120400KyleMadiganWalkerMessages[6] SECTION(".data") = {
    { ACTOR_MESSAGE_PLAY_ANIMATION, _actorMotionPlayAnim },
    { ACTOR_MESSAGE_PLACE, actorMsgPlaceEuler },
    { ACTOR_MESSAGE_SET_MODEL_DRAW, _actor120400SetKyleMadiganDrawMode },
    { ACTOR_MESSAGE_WALK_TO, _actor120400StartKyleMadiganWalk },
    { ACTOR_COMMAND_MESSAGE_APPLY, _actor120400IgnoreKyleMadiganCommand },
    { TASK_MESSAGE_TABLE_END, NULL },
};

/// Allocates Kyle's scripted-walker work and spawns his two attached hand models.
///
/// Requires the body model, enemy placement index and current loaded area.
/// Allocation failure exits the enemy task. On success initializes absent
/// clip/bank and buffer-release sentinels, clears 16.16 movement carry, and
/// spawns hand slots 1/2 attached to body parts 8/12. Failed child spawns are
/// skipped independently; successful children receive placement texture offsets
/// and both existing buffer halves are rebuilt. Binds the body's borrowed light
/// and colour matrices, messages and exit callback, then advances task state.
static void _actor120400SpawnKyleMadiganWalker(Task* task)
{
    enum {
        ACTOR_120400_KYLE_FREE_NOT_PENDING = -1,
        ACTOR_120400_KYLE_LEFT_HAND_TASK   = 1,
        ACTOR_120400_KYLE_RIGHT_HAND_TASK  = 2,
        ACTOR_120400_KYLE_LEFT_HAND_PART   = 8,
        ACTOR_120400_KYLE_RIGHT_HAND_PART  = 12,
    };

    _Actor120400KyleMadiganWork* work;
    Task*                        childTask;

    work = memCalloc(sizeof(_Actor120400KyleMadiganWork), false);
    if (work == NULL) {
        enemyTaskExit(task);
        return;
    }
    task->work               = work;
    work->model.animId       = ACTOR_MOTION_ANIMATION_INDEX_NONE;
    work->model.bank         = ACTOR_MOTION_ANIMATION_INDEX_NONE;
    work->freeCountdown      = ACTOR_120400_KYLE_FREE_NOT_PENDING;
    work->walk.carry[0].word = 0;
    work->walk.carry[1].word = 0;
    work->walk.carry[2].word = 0;
    // Spawn independent hand models, borrowing the body task until teardown.
    childTask = taskSpawnFromTable(gActor120400KyleMadiganTaskDescriptors, ACTOR_120400_KYLE_LEFT_HAND_TASK, ACTOR_120400_KYLE_LEFT_HAND_PART, task);
    if (childTask != NULL) {
        _actorRenderApplyTaskPlacementTextureOffsets(childTask, task->spawnArg2.pointer);
    }
    childTask = taskSpawnFromTable(gActor120400KyleMadiganTaskDescriptors, ACTOR_120400_KYLE_RIGHT_HAND_TASK, ACTOR_120400_KYLE_RIGHT_HAND_PART, task);
    if (childTask != NULL) {
        _actorRenderApplyTaskPlacementTextureOffsets(childTask, task->spawnArg2.pointer);
    }
    _actor120400BindKyleMadiganWalkerLighting(task);
    task->msgTable     = _gActor120400KyleMadiganWalkerMessages;
    task->exitCallback = _actor120400ExitKyleMadiganWalker;
    task->state       += 1;
}

/// Applies one frame's signed 16.16 velocity and retains the fractional carry.
///
/// Requires live writable work and root coordinate. Invalidates composition
/// even when stationary; the remaining fractions are zero-extended.
static inline void _actor120400IntegrateWalkVelocity(_Actor120400KyleMadiganWork* work, GfxCoord* rootCoord)
{
    work->walk.carry[0].word += work->walk.velocity.vx;
    work->walk.carry[1].word += work->walk.velocity.vy;
    work->walk.carry[2].word += work->walk.velocity.vz;
    rootCoord->coord.t[0]    += work->walk.carry[0].halves.integer;
    rootCoord->coord.t[1]    += work->walk.carry[1].halves.integer;
    rootCoord->coord.t[2]    += work->walk.carry[2].halves.integer;
    rootCoord->composeStamp   = GRAPHICS_COORD_DIRTY;
    work->walk.carry[0].word  = work->walk.carry[0].halves.fraction;
    work->walk.carry[1].word  = work->walk.carry[1].halves.fraction;
    work->walk.carry[2].word  = work->walk.carry[2].halves.fraction;
}

/// Updates Kyle's walk, animation, ground shadow, lighting and delayed buffer release.
///
/// Requires a live TMD body with its twenty-part rig and
/// `_Actor120400KyleMadiganWork`; `walk.motion` must be 0 (idle) or 1 (walking).
/// The body dispatcher runs this only while scene actors are running.
/// Slots 1..19 tick once a clip has been applied. Active-draw exclusion gates
/// the shadow; view readiness gates lighting. The update finding a release
/// counter of zero frees the body primitive buffer, then sets the counter to -1.
static void _actor120400UpdateKyleMadiganWalker(Task* task)
{
    enum {
        ACTOR_120400_KYLE_SHADOW_HALF_SIZE = 0x300, // World-coordinate units
        ACTOR_120400_KYLE_LIGHT_COUNT      = 3,
    };
    TmdObject*                   bodyModel         = task->extra.tmd;
    _Actor120400KyleMadiganWork* work              = task->work;
    TaskFunc                     motionHandlers[2] = { _actor120400IdleKyleMadiganWalk, _actor120400RunKyleMadiganWalkStep };
    VECTOR3                      groundPosition;
    GfxCoord*                    rootCoord;
    s32                          slotIndex;

    motionHandlers[work->walk.motion](task);
    rootCoord = task->extra.tmd->coords;
    _actor120400IntegrateWalkVelocity(work, rootCoord);
    if (work->model.ticking != 0) {
        for (slotIndex = 1; slotIndex < (s32)ARRAY_SIZE(work->rig.slots); slotIndex++) {
            animationTickSlot(&work->rig.anim, slotIndex);
        }
    }
    // Use the composed body part for ground projection before refreshing its lighting.
    if (!(bodyModel->flags & TMD_OBJECT_SKIP_ACTIVE_DRAW)) {
        if (worldCollisionProjectGroundPoint(GRAPHICS_MATRIX_TRANSLATION(&task->extra.tmd->coords[1].workm), &groundPosition) != 0) {
            effectDrawGroundShadow(&groundPosition, ACTOR_120400_KYLE_SHADOW_HALF_SIZE, gRoomEffectState->groundShadowShade);
        }
    }
    if (gGameSession->viewReady != 0) {
        task->extra.tmd->coords[1].composeStamp = GRAPHICS_COORD_DIRTY;
        actorRenderComposeCoord(&task->extra.tmd->coords[1]);
        worldCoordSetModelLighting(bodyModel, task->extra.tmd->coords[1].workm.t, 0, ACTOR_120400_KYLE_LIGHT_COUNT);
    }
    if (work->freeCountdown >= 0) {
        if (work->freeCountdown == 0) {
            tmdFreePrimitiveBuffer(bodyModel);
        }
        work->freeCountdown--;
    }
}

#include "../../shared/actor/actor_motion_arrive.inc.c"

/// Starts Kyle's four-step scripted walk toward a borrowed destination.
///
/// Requires a live TMD body and `_Actor120400KyleMadiganWork`. Copies XYZ
/// position in the root parent's coordinate frame and Euler angles in 4096
/// units per turn; the closing step uses yaw. Optional `walkAnim` supplies
/// bank-0 start and closing clip IDs in 1..28; NULL selects clips 16 and 1. Both borrowed
/// records need only survive this call. Plays the start clip with a five-frame
/// blend when already ticking, otherwise resets slots 1..19. The message ID
/// is ignored. Returns 0; a new request restarts an existing walk.
static s32 _actor120400StartKyleMadiganWalk(Task* task, s32 msgId, const ActorTransform* placement,
                                            const ActorMotionWalkAnim* walkAnim)
{
    enum {
        ACTOR_120400_KYLE_WALK_FACE_TARGET  = 0,
        ACTOR_120400_KYLE_ANIMATION_BANK    = 0,
        ACTOR_120400_KYLE_WALK_START_CLIP   = 0x10,
        ACTOR_120400_KYLE_WALK_CLOSING_CLIP = 1,
        ACTOR_120400_KYLE_WALK_BLEND_FRAMES = 5,
    };
    ActorMotionPlayWork*         animationWork;
    _Actor120400KyleMadiganWork* walkWork;
    AnimationPlayRequest         startRequest;

    walkWork                    = task->work;
    walkWork->walk.motion       = ACTOR_WALK_MOTION_WALKING;
    walkWork->walk.motionStep   = ACTOR_120400_KYLE_WALK_FACE_TARGET;
    walkWork->walk.target.vx    = placement->pos.vx;
    walkWork->walk.target.vy    = placement->pos.vy;
    walkWork->walk.target.vz    = placement->pos.vz;
    walkWork->walk.targetRot.vx = placement->rot.vx;
    walkWork->walk.targetRot.vy = placement->rot.vy;
    walkWork->walk.targetRot.vz = placement->rot.vz;
    startRequest.source.index   = ACTOR_120400_KYLE_ANIMATION_BANK;
    if (walkAnim != NULL) {
        startRequest.animationId   = walkAnim->animationId;
        walkWork->model.nextAnimId = walkAnim->nextAnimId;
    } else {
        startRequest.animationId   = ACTOR_120400_KYLE_WALK_START_CLIP;
        walkWork->model.nextAnimId = ACTOR_120400_KYLE_WALK_CLOSING_CLIP;
    }
    startRequest.blend                = ANIMATION_BLEND_INTERPOLATE;
    startRequest.blendFrames          = ACTOR_120400_KYLE_WALK_BLEND_FRAMES;
    startRequest.enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE;

    // Apply the request after latching the destination and closing clip.
    animationWork = task->work;
    _actorMotionApplyAnimationRequest(animationWork, task->extra.tmd, &startRequest);
    return 0;
}

/// Runs a Kyle hand model's attachment, draw-policy update or teardown state.
///
/// Requires a live TMD task with state 0..2. Setup borrows its body task from
/// `spawnArg2.pointer` and its part index from `spawnArg1.value` (8 left,
/// 12 right). The parent's coordinates and lighting storage must outlive the
/// hand's active use. Setup links teardown to the body; later ticks mirror draw and
/// automatic-buffer flags and recover missing buffers when permitted.
static void _actor120400KyleMadiganHandTask(Task* handTask)
{
    TaskFuncTable3 states;

    states = _gActor120400KyleMadiganHandStates;
    states.funcs[handTask->state](handTask);
}

#include "../../shared/actor/model_placement_attach.inc.c"

#include "../../shared/actor/model_placement_mirror_parent.inc.c"

/// Runs Kyle's body setup, update or teardown while scene actors are running.
///
/// Requires a live TMD task with state 0..2. Setup borrows its live `Enemy`
/// spawn argument and allocates the walker work; updates require that work.
/// The actor-control gate covers every state, including setup and teardown.
static void _actor120400KyleMadiganWalkerTask(Task* task)
{
    TaskFuncTable3 states;

    states = _gActor120400KyleMadiganWalkerStates;
    if (gSceneCombatState.actorControl == SCENE_COMBAT_ACTORS_RUNNING) {
        states.funcs[task->state](task);
    }
}

/// Releases Kyle's enemy tracking and tears down the body and attached tasks.
///
/// Requires the live primary-heap `Enemy` in `spawnArg2.pointer`. The common
/// teardown releases that object, invokes child exits and frees walker work.
/// TMD tasks suppress drawing and may remain alive for deferred body release;
/// the enemy and work pointers are invalid afterwards.
static void _actor120400ExitKyleMadiganWalker(Task* task)
{
    enemyTaskExit(task);
}

/// Lends the walker's light-direction and light-colour matrices to its body model.
///
/// Requires a live TMD body and `_Actor120400KyleMadiganWork`. The model borrows
/// the matrices for active drawing; keep the work live throughout that use.
static void _actor120400BindKyleMadiganWalkerLighting(Task* task)
{
    TmdObject*                   bodyModel;
    _Actor120400KyleMadiganWork* work;

    bodyModel           = task->extra.tmd;
    work                = task->work;
    bodyModel->lightMtx = &work->model.light;
    bodyModel->colorMtx = &work->model.color;
}

/// Leaves Kyle's motion idle while the body update continues animation and lighting.
static void _actor120400IdleKyleMadiganWalk(Task* task)
{
}

/// Runs the current step of Kyle's scripted walk.
///
/// Requires live body work and `walk.motionStep` in 0..3: face the destination,
/// begin forward motion, check arrival, then turn to the destination yaw.
/// The steps advance the index; the closing turn returns motion to idle.
static void _actor120400RunKyleMadiganWalkStep(Task* task)
{
    _Actor120400KyleMadiganWork* work;
    TaskFuncTable4               handlers;

    work     = task->work;
    handlers = _gActor120400KyleMadiganWalkSteps;
    handlers.funcs[work->walk.motionStep](task);
}

#include "../../shared/actor/actor_motion_face.inc.c"

/// Begins forward motion and advances Kyle's walk to the arrival check.
///
/// Requires a live TMD root and walker work in step 1. The root's Q12 rotation
/// transforms local +Z velocity of 32 units per frame, stored as signed 16.16,
/// into the parent's frame. Seeds all previous-distance components with
/// `ACTOR_MOTION_INITIAL_ARRIVAL_GAP` for the first arrival comparison.
static void _actor120400BeginKyleMadiganWalk(Task* task)
{
    _Actor120400KyleMadiganWork* work;
    GfxCoord*                    rootCoord;
    VECTOR                       localVelocity;

    rootCoord = task->extra.tmd->coords;
    work      = task->work;

    localVelocity = _gActor120400KyleMadiganForwardWalkVelocity;
    ApplyMatrixLV(&rootCoord->coord, &localVelocity, &work->walk.velocity);
    work->walk.lastDistance.vx = ACTOR_MOTION_INITIAL_ARRIVAL_GAP;
    work->walk.lastDistance.vy = ACTOR_MOTION_INITIAL_ARRIVAL_GAP;
    work->walk.lastDistance.vz = ACTOR_MOTION_INITIAL_ARRIVAL_GAP;
    work->walk.motionStep++;
}

#include "../../shared/actor/actor_motion_turn.inc.c"

#include "../../shared/actor/actor_motion_play.inc.c"

#include "../../shared/actor/actor_messages_place_euler.inc.c"

/// Sets Kyle's active-draw and automatic-buffer policy and schedules buffer release.
///
/// Requires a live TMD body and walker work. Modes 0/1 hide/show with automatic
/// buffers enabled; showing requests a missing body buffer. Mode 2 hides,
/// disables automatic buffers and sets the release counter to 2, freeing the
/// body buffer on the third subsequent running update. Mode 3 shows with
/// automatic buffers disabled. Modes 0, 1 and 3 retain a pending release.
/// Other flags remain unchanged; attached hands mirror the policy on their
/// own ticks. Returns 0 for modes 0..3, otherwise 1, even if allocation fails.
/// The message ID and second payload are ignored. `mode` is a full signed
/// word; its counter store narrows to a halfword.
static s32 _actor120400SetKyleMadiganDrawMode(Task* task, s32 msgId, s32 mode, s32 unusedArg)
{
    enum { ACTOR_120400_KYLE_DRAW_SHOW_SKIP_AUTO_BUFFER = 3 };
    TmdObject*                   bodyModel;
    _Actor120400KyleMadiganWork* work;
    s32                          result;

    bodyModel = task->extra.tmd;
    work      = task->work;
    result    = 0;
    switch (mode) {
        case ACTOR_MESSAGE_DRAW_HIDE:
            bodyModel->flags |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
            bodyModel->flags &= ~TMD_OBJECT_SKIP_AUTO_BUFFER;
            break;
        case ACTOR_MESSAGE_DRAW_SHOW:
            bodyModel->flags &= ~TMD_OBJECT_SKIP_ACTIVE_DRAW;
            tmdAllocPrimitiveBuffer(bodyModel);
            bodyModel->flags &= ~TMD_OBJECT_SKIP_AUTO_BUFFER;
            break;
        case ACTOR_MESSAGE_DRAW_HIDE_SKIP_AUTO_BUFFER:
            bodyModel->flags   |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
            work->freeCountdown = mode;
            bodyModel->flags   |= TMD_OBJECT_SKIP_AUTO_BUFFER;
            break;
        case ACTOR_120400_KYLE_DRAW_SHOW_SKIP_AUTO_BUFFER:
            bodyModel->flags &= ~TMD_OBJECT_SKIP_ACTIVE_DRAW;
            bodyModel->flags |= TMD_OBJECT_SKIP_AUTO_BUFFER;
            break;
        default:
            result = 1;
            break;
    }
    return result;
}

/// Accepts an actor command without changing Kyle's state and returns zero.
///
/// Installed for `ACTOR_COMMAND_MESSAGE_APPLY`; ignores the receiver, message
/// ID and both payload words and retains no borrowed command data.
static s32 _actor120400IgnoreKyleMadiganCommand(Task* task, s32 msgId, s32 unusedFirstArg, s32 unusedArg)
{
    return 0;
}
