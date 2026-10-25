#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/abs.h>

#include "common.h"

#include "actors/actor.h"

#include "gameplay/actor_render.h"
#include "gameplay/animation.h"
#include "gameplay/enemy.h"
#include "gameplay/message.h"
#include "gameplay/scene_runtime.h"
#include "gameplay/world_coords.h"

#include "main/coord.h"
#include "main/gfx.h"
#include "main/mem.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/sound.h"
#include "main/task_types.h"
#include "main/tmd.h"
#include "main/tmd_types.h"
#include "../../../../shared/actor/actor_motion.h"
#include "../../../../shared/actor/actor_messages.h"

#include "assets/rooms/akropolis/patio/stranger_schema.h"

/// Work block of the overlay's actor, allocated zeroed by its spawn state and kept at `Task::work` for the task's
/// life.
typedef struct {
    ActorAnimRig19  rig;              // Playback storage of the nineteen-part body model; slots 1 to 18 are driven
    ActorModelState model;            // Clip and bank the rig plays, and the matrices the model is lit with
    ActorWalkState  walk;             // Leap in progress: the yaw it launches along, the per-frame velocity and the step; `target` and `lastDistance` are not read
    s8              airborne;         // The leap is in flight, so each tick adds gravity to `walk.velocity.vy` (0 on the floor, 1 from launch to landing)
    s8              turnWeightRising; // Direction `turnWeight` ramps, set by an actor command (0 falls by 0x80 a tick, 1 rises by 0x40)
    s16             turnWeight;       // Weight handed to the per-frame head turn toward the player, 0 to `ONE`; the package's turn takes it and does not read it
    s16             freeCountdown;    // Ticks left before the model's buffers are freed, which the tick finding 0 does (-1 no free pending)
} _Work;
STATIC_ASSERT_SIZEOF(_Work, 0x4CC);

/// Leap tuning of this actor package.
enum {
    YAW_STEP     = 0x40, // 4096 angle units per turn
    BLEND_FRAMES = 5,
};

/// The body part the lighting follows.
enum { PELVIS_PART = 1 };

static void _patioLeapingStrangersUpdate(Task* task);
static void _patioLeapingStrangersFacePlayerAfterLeap(Task* task);
static void _patioLeapingStrangersTurnHeadToTarget(Task* task, Task* targetTask, s32 maxYaw, s32 maxPitch, s32 blendWeight);
static void _patioLeapingStrangersInit(Task* task);
static void _patioLeapingStrangersExit(Task* task);
static void _patioLeapingStrangersBindLighting(Task* task);
static void _patioLeapingStrangersIdle(Task* task);
static void _patioLeapingStrangersRunLeapStep(Task* task);
static void _patioLeapingStrangersTurnToLaunchYaw(Task* task);
static void _patioLeapingStrangersLaunchLeap(Task* task);
static void _patioLeapingStrangersLandLeap(Task* task);
static s32  _patioLeapingStrangersSetDrawMode(Task* task, s32 messageId, s32 mode, s32 unusedArg);

enum {
    TASK_INITIALIZE = 0,
    TASK_UPDATE     = 1,
    TASK_EXIT       = 2,
};

/// The leaping Stranger's task states: initialize, update, exit.
static const TaskFuncTable3 _gPatioLeapingStrangersStates = {
    .funcs = {
        [TASK_INITIALIZE] = _patioLeapingStrangersInit,
        [TASK_UPDATE]     = _patioLeapingStrangersUpdate,
        [TASK_EXIT]       = _patioLeapingStrangersExit,
    },
};

/// Grinning Stranger leap phases: turn to launch yaw, launch, land, face the player.
static const TaskFuncTable4 _gPatioLeapingStrangersLeapStepHandlers = { {
    _patioLeapingStrangersTurnToLaunchYaw,
    _patioLeapingStrangersLaunchLeap,
    _patioLeapingStrangersLandLeap,
    _patioLeapingStrangersFacePlayerAfterLeap,
} };

enum { VELOCITY_ONE = 0x10000 };

/// Leap launch velocity in local model axes, in signed 16.16 units per tick.
static const VECTOR _gPatioLeapingStrangersLeapLaunchVelocity = { 0, -(128 * VELOCITY_ONE), 64 * VELOCITY_ONE, 0 };

static TmdSource _gStrangerGrinningBody;
static s32       _patioLeapingStrangersBeginLeap(Task* task, s32 messageId, const ActorTransform* launchPlacement, const ActorMotionWalkAnim* animationRequest);
static s32       _patioLeapingStrangersApplyCommand(Task* task, s32 messageId, const ActorCommand* commandRequest, s32 unusedArg);
static void      _patioLeapingStrangersTask(Task* task);

#include "assets/enemies/stranger/grinning/body.inc"

#include "assets/rooms/akropolis/patio/leaping_strangers_clips.inc"

/// Grinning Stranger body animation sets for bank 0: clip 0 absent, clips 1..8 loaded.
#include "assets/rooms/akropolis/patio/stranger_bank.inc"

/// Carrier-local banks for nineteen-part playback: only bank 0, clips 1..8.
static AnimationSet** const _gActorMotionAnimBanks19[1] SECTION(".data") = {
    _gPatioLeapingStrangersBodyAnimSets,
};

TaskDesc gPatioLeapingStrangersTaskDesc = { { { (TASK_BODY_TMD | TASK_DESC_SKIP_AUTO_MODEL_BUFFER), TASK_PRIORITY_CONTROL } }, _patioLeapingStrangersTask, { .model = &_gStrangerGrinningBody } };

/// Message map for the script-controlled Grinning Stranger and its leap sequence.
static const TaskMessageEntry _gPatioLeapingStrangersMessages[6] __attribute__((section(".data"))) = {
    { ACTOR_MESSAGE_PLAY_ANIMATION, _actorMotionPlayAnim19 },
    { ACTOR_MESSAGE_PLACE, actorMsgPlaceEuler },
    { ACTOR_MESSAGE_SET_MODEL_DRAW, _patioLeapingStrangersSetDrawMode },
    { ACTOR_MESSAGE_WALK_TO, _patioLeapingStrangersBeginLeap },
    { ACTOR_COMMAND_MESSAGE_APPLY, _patioLeapingStrangersApplyCommand },
    { TASK_MESSAGE_TABLE_END, NULL },
};

/// Applies one frame of leap displacement and advances the airborne velocity.
static inline void _patioLeapingStrangersIntegrateLeapVelocity(_Work* work, GfxCoord* rootCoord)
{
    enum { LEAP_GRAVITY = 18 << 16 }; // Signed 16.16 units per tick squared

    // Accumulate this frame's displacement before gravity changes the velocity.
    work->walk.carry[0].word += work->walk.velocity.vx;
    work->walk.carry[1].word += work->walk.velocity.vy;
    work->walk.carry[2].word += work->walk.velocity.vz;
    if (work->airborne != 0) {
        work->walk.velocity.vy += LEAP_GRAVITY;
    }
    rootCoord->coord.t[0]  += work->walk.carry[0].halves.integer;
    rootCoord->coord.t[1]  += work->walk.carry[1].halves.integer;
    rootCoord->coord.t[2]  += work->walk.carry[2].halves.integer;
    rootCoord->composeStamp = GRAPHICS_COORD_DIRTY;
    // Retain the unsigned fractions after consuming the signed integer steps.
    work->walk.carry[0].word = work->walk.carry[0].halves.fraction;
    work->walk.carry[1].word = work->walk.carry[1].halves.fraction;
    work->walk.carry[2].word = work->walk.carry[2].halves.fraction;
}

/// Updates the leap, animation, head turn, lighting and delayed buffer release.
static void _patioLeapingStrangersUpdate(Task* task)
{
    enum { HEAD_WEIGHT_RISE = 0x40,
           HEAD_WEIGHT_FALL = 0x80,
           LIGHT_COUNT      = 3 };
    TmdObject* bodyModel         = task->extra.tmd;
    _Work*     work              = task->work;
    TaskFunc   motionHandlers[2] = { _patioLeapingStrangersIdle, _patioLeapingStrangersRunLeapStep };
    GfxCoord*  rootCoord;
    s32        slotIndex;

    motionHandlers[work->walk.motion](task);

    // Integrate this tick before gravity changes the next tick's velocity.
    rootCoord = task->extra.tmd->coords;
    _patioLeapingStrangersIntegrateLeapVelocity(work, rootCoord);
    if (work->model.ticking != 0) {
        for (slotIndex = 1; slotIndex < (s32)ARRAY_SIZE(work->rig.slots); slotIndex++) {
            animationTickSlot(&work->rig.anim, slotIndex);
        }
    }
    if (gGameSession->viewReady != 0) {
        task->extra.tmd->coords[PELVIS_PART].composeStamp = GRAPHICS_COORD_DIRTY;
        actorRenderComposeCoord(&task->extra.tmd->coords[PELVIS_PART]);
        worldCoordSetModelLighting(bodyModel, task->extra.tmd->coords[PELVIS_PART].workm.t, 0, LIGHT_COUNT);
    }
    if (work->turnWeightRising != 0) {
        work->turnWeight += HEAD_WEIGHT_RISE;
        if (work->turnWeight > ONE) {
            work->turnWeight = ONE;
        }
    } else {
        work->turnWeight -= HEAD_WEIGHT_FALL;
        if (work->turnWeight < 0) {
            work->turnWeight = 0;
        }
    }
    _patioLeapingStrangersTurnHeadToTarget(task, gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), ACTOR_TRANSFORM_ANGLE_TURN / 4, ACTOR_TRANSFORM_ANGLE_TURN / 8, work->turnWeight);
    // Delay release until queued model primitives have left the frame buffers.
    if (work->freeCountdown >= 0) {
        if (work->freeCountdown == 0) {
            tmdFreePrimitiveBuffer(bodyModel);
        }
        work->freeCountdown--;
    }
}

/// Ends the leap by turning the root toward the player's position.
static void _patioLeapingStrangersFacePlayerAfterLeap(Task* task)
{
    _Work*    work;
    GfxCoord* rootCoord;
    GfxCoord* playerCoord;
    VECTOR    toPlayer;
    SVECTOR   direction;
    SVECTOR   targetAngles;
    SVECTOR   rootAngles;
    s16       yawDelta;
    s32       yawMagnitude;
    s32       currentYaw;

    rootCoord   = task->extra.tmd->coords;
    playerCoord = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER)->extra.tmd->coords;
    work        = task->work;

    toPlayer.vx = playerCoord->coord.t[0] - rootCoord->coord.t[0];
    toPlayer.vy = playerCoord->coord.t[1] - rootCoord->coord.t[1];
    toPlayer.vz = playerCoord->coord.t[2] - rootCoord->coord.t[2];
    VectorNormalS(&toPlayer, &direction);

    targetAngles.vx = 0;
    targetAngles.vy = ratan2(direction.vx, direction.vz);
    targetAngles.vz = 0;

    rootCoord->param.rot.vx = targetAngles.vx;
    rootCoord->param.rot.vy = targetAngles.vy;
    rootCoord->param.rot.vz = targetAngles.vz;

    gfxExtractSmallestEuler(&rootAngles, &rootCoord->coord);
    yawDelta     = ratan2(direction.vx, direction.vz) - rootAngles.vy;
    yawMagnitude = abs(yawDelta);
    if (yawMagnitude >= YAW_STEP + 1) {
        currentYaw = rootAngles.vy;
        if (yawDelta < 0) {
            rootAngles.vy = currentYaw - YAW_STEP;
        } else {
            rootAngles.vy = currentYaw + YAW_STEP;
        }
    } else {
        work->walk.motion     = ACTOR_WALK_MOTION_IDLE;
        work->walk.motionStep = 0;
    }
    RotMatrix(&rootAngles, &rootCoord->coord);
    rootCoord->composeStamp = GRAPHICS_COORD_DIRTY;
}

/// Turns the head toward a target's head and gives the lower neck an eighth of the turn.
static void _patioLeapingStrangersTurnHeadToTarget(Task* task, Task* targetTask, s32 maxYaw, s32 maxPitch, s32 blendWeight)
{
    enum {
        HEAD_PART        = 5, // Of the Stranger rig
        LOWER_NECK_PART  = 3,
        CHEST_PART       = 2, // The frame the turn is measured in
        TARGET_HEAD_PART = 4, // Of Aya's rig
    };
    GfxCoord* subjectCoords;
    GfxCoord* targetCoords;
    GfxCoord* headCoord;
    GfxCoord* targetHeadCoord;
    MATRIX*   lowerNeckRotation;
    VECTOR    toTarget;
    VECTOR    direction;
    SVECTOR   headAngles;
    SVECTOR   jointAngles;
    SVECTOR   turnAngles;

    subjectCoords   = task->extra.tmd->coords;
    targetCoords    = targetTask->extra.tmd->coords;
    headCoord       = &subjectCoords[HEAD_PART];
    targetHeadCoord = &targetCoords[TARGET_HEAD_PART];

    // Express the world separation in the chest's frame.
    toTarget.vx = targetHeadCoord->workm.t[0] - headCoord->workm.t[0];
    toTarget.vy = targetHeadCoord->workm.t[1] - headCoord->workm.t[1];
    toTarget.vz = targetHeadCoord->workm.t[2] - headCoord->workm.t[2];
    VectorNormal(&toTarget, &toTarget);
    ApplyTransposeMatrixLV(&task->extra.tmd->coords[CHEST_PART].workm, &toTarget, &toTarget);
    VectorNormal(&toTarget, &direction);

    turnAngles.vx = ratan2(direction.vz, direction.vy);
    turnAngles.vy = ratan2(direction.vx, -ABS(direction.vz));
    turnAngles.vz = ratan2(direction.vx, direction.vy);

    gfxExtractSmallestEuler(&headAngles, &subjectCoords[HEAD_PART].coord);
    headAngles.vx = (u16)headAngles.vx + (u16)turnAngles.vx;
    headAngles.vy = (u16)headAngles.vy + (u16)turnAngles.vy;
    headAngles.vz = (u16)headAngles.vz + (u16)turnAngles.vz;
    RotMatrix(&headAngles, &subjectCoords[HEAD_PART].coord);

    // Share the resulting head rotation with the lower neck before reducing the head pose.
    gfxExtractSmallestEuler(&turnAngles, &subjectCoords[HEAD_PART].coord);
    lowerNeckRotation = &task->extra.tmd->coords[LOWER_NECK_PART].coord;
    gfxExtractSmallestEuler(&jointAngles, lowerNeckRotation);
    jointAngles.vx = (u16)jointAngles.vx + turnAngles.vx / 8;
    jointAngles.vy = (u16)jointAngles.vy + turnAngles.vy / 8;
    jointAngles.vz = (u16)jointAngles.vz + turnAngles.vz / 8;
    RotMatrix(&jointAngles, lowerNeckRotation);

    gfxExtractSmallestEuler(&jointAngles, &subjectCoords[HEAD_PART].coord);
    jointAngles.vx = jointAngles.vx * 5 / 8;
    jointAngles.vy = jointAngles.vy * 5 / 8;
    jointAngles.vz = jointAngles.vz * 5 / 8;

    gfxSetRotIdentity(&subjectCoords[HEAD_PART].coord);
    RotMatrix(&jointAngles, &subjectCoords[HEAD_PART].coord);
}

/// Applies the leap's requested start clip without restarting repeated clips.
static inline void _patioLeapingStrangersApplyLeapStartAnimation(Task* task, const AnimationPlayRequest* request)
{
    enum { FIRST_DRIVEN_SLOT = 1 };
    _Work*     playbackWork;
    TmdObject* bodyModel;
    s32        slotIndex;

    playbackWork = task->work;
    bodyModel    = task->extra.tmd;
    // Changing banks invalidates the previous clip even when its ID agrees.
    if (request->source.index != playbackWork->model.bank) {
        playbackWork->model.bank   = request->source.index;
        playbackWork->model.animId = ACTOR_MOTION_ANIMATION_INDEX_NONE;
        animationInitContext(&playbackWork->rig.anim, _gActorMotionAnimBanks19[playbackWork->model.bank], bodyModel, playbackWork->rig.poses,
                             playbackWork->rig.slots);
    }
    if (request->animationId != playbackWork->model.animId) {
        playbackWork->model.animId = request->animationId;
        if (request->blend != ANIMATION_BLEND_RESET && playbackWork->model.ticking != 0) {
            for (slotIndex = FIRST_DRIVEN_SLOT; slotIndex < (s32)ARRAY_SIZE(playbackWork->rig.slots); slotIndex++) {
                animationSeekSlotWithBlend(&playbackWork->rig.anim, slotIndex, playbackWork->model.animId, 0, request->blendFrames);
            }
        } else {
            for (slotIndex = FIRST_DRIVEN_SLOT; slotIndex < (s32)ARRAY_SIZE(playbackWork->rig.slots); slotIndex++) {
                animationResetSlot(&playbackWork->rig.anim, slotIndex, playbackWork->model.animId);
            }
        }
        // Apply the selected pose before the normal frame update resumes ticking.
        for (slotIndex = FIRST_DRIVEN_SLOT; slotIndex < (s32)ARRAY_SIZE(playbackWork->rig.slots); slotIndex++) {
            animationTickSlot(&playbackWork->rig.anim, slotIndex);
        }
        playbackWork->model.ticking = true;
    }
}

/// Handles a walk-to message by starting this actor's scripted leap.
static s32 _patioLeapingStrangersBeginLeap(Task* task, s32 messageId, const ActorTransform* launchPlacement, const ActorMotionWalkAnim* animationRequest)
{
    _Work*               leapWork;
    AnimationPlayRequest startRequest;

    leapWork                    = task->work;
    leapWork->walk.motion       = ACTOR_WALK_MOTION_WALKING;
    leapWork->walk.target.vx    = launchPlacement->pos.vx;
    leapWork->walk.target.vy    = launchPlacement->pos.vy;
    leapWork->walk.target.vz    = launchPlacement->pos.vz;
    leapWork->walk.targetRot.vx = launchPlacement->rot.vx;
    leapWork->walk.targetRot.vy = launchPlacement->rot.vy;
    leapWork->walk.targetRot.vz = launchPlacement->rot.vz;
    startRequest.source.index   = 0;
    if (animationRequest != NULL) {
        startRequest.animationId   = animationRequest->animationId;
        leapWork->model.nextAnimId = animationRequest->nextAnimId;
    } else {
        startRequest.animationId   = STRANGER_ANIMATION_LEAP;
        leapWork->model.nextAnimId = STRANGER_ANIMATION_REACT;
    }
    startRequest.blend                = ANIMATION_BLEND_INTERPOLATE;
    startRequest.blendFrames          = BLEND_FRAMES;
    startRequest.enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE;

    // Apply a changed clip immediately; an unchanged clip keeps its current cursor.
    _patioLeapingStrangersApplyLeapStartAnimation(task, &startRequest);
    return 0;
}

/// Runs this actor's initialization, update or exit state.
static void _patioLeapingStrangersTask(Task* task)
{
    TaskFuncTable3 stateHandlers;

    stateHandlers = _gPatioLeapingStrangersStates;
    stateHandlers.funcs[task->state](task);
}

/// Allocates the actor's playback and leap work and installs its callbacks.
static void _patioLeapingStrangersInit(Task* task)
{
    enum { BUFFER_FREE_NONE = -1 };
    _Work* work;

    work = memCalloc(sizeof(_Work), false);
    if (work == NULL) {
        enemyTaskExit(task);
        return;
    }

    task->work               = work;
    work->model.animId       = ACTOR_MOTION_ANIMATION_INDEX_NONE;
    work->model.bank         = ACTOR_MOTION_ANIMATION_INDEX_NONE;
    work->freeCountdown      = BUFFER_FREE_NONE;
    work->walk.carry[0].word = 0;
    work->walk.carry[1].word = 0;
    work->walk.carry[2].word = 0;

    _patioLeapingStrangersBindLighting(task);
    _patioLeapingStrangersSetDrawMode(task, ACTOR_MESSAGE_SET_MODEL_DRAW, ACTOR_MESSAGE_DRAW_HIDE, 0);

    task->msgTable     = _gPatioLeapingStrangersMessages;
    task->exitCallback = _patioLeapingStrangersExit;
    task->state++;
}

/// Tears down the actor through the enemy task lifecycle.
static void _patioLeapingStrangersExit(Task* task)
{
    enemyTaskExit(task);
}

/// Lends the work-owned light and colour matrices to the actor's TMD object.
static void _patioLeapingStrangersBindLighting(Task* task)
{
    TmdObject* bodyModel;
    _Work*     work;

    bodyModel           = task->extra.tmd;
    work                = task->work;
    bodyModel->lightMtx = &work->model.light;
    bodyModel->colorMtx = &work->model.color;
}

/// Leaves an idle actor's motion unchanged while its animation and head turn continue.
static void _patioLeapingStrangersIdle(Task* task)
{
}

/// Dispatches the current leap phase from a stack copy of the package's table.
static void _patioLeapingStrangersRunLeapStep(Task* task)
{
    TaskFuncTable4 handlers;
    _Work*         work;

    work     = task->work;
    handlers = _gPatioLeapingStrangersLeapStepHandlers;
    handlers.funcs[work->walk.motionStep](task);
}

/// Turns to the requested launch yaw and selects the leap animation.
static void _patioLeapingStrangersTurnToLaunchYaw(Task* task)
{
    _Work*               work;
    GfxCoord*            rootCoord;
    SVECTOR              rootAngles;
    AnimationPlayRequest launchRequest;
    s32                  currentYaw;
    s16                  yawDelta;

    rootCoord = task->extra.tmd->coords;
    work      = task->work;

    gfxExtractSmallestEuler(&rootAngles, &rootCoord->coord);
    yawDelta = (u16)work->walk.targetRot.vy - (u16)rootAngles.vy;
    if (ABS(yawDelta) >= YAW_STEP + 1) {
        currentYaw = rootAngles.vy;
        if (yawDelta < 0) {
            rootAngles.vy = currentYaw - YAW_STEP;
        } else {
            rootAngles.vy = currentYaw + YAW_STEP;
        }
    } else {
        rootAngles.vy                      = work->walk.targetRot.vy;
        launchRequest.source.index         = 0;
        launchRequest.animationId          = STRANGER_ANIMATION_LEAP;
        launchRequest.blend                = ANIMATION_BLEND_INTERPOLATE;
        launchRequest.blendFrames          = BLEND_FRAMES;
        launchRequest.enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE;
        _actorMotionPlayAnim19(task, ACTOR_MESSAGE_PLAY_ANIMATION, &launchRequest, 0);
        work->airborne = 0;
        work->walk.motionStep++;
    }

    gfxSetRotIdentity(&rootCoord->coord);
    RotMatrix(&rootAngles, &rootCoord->coord);
    rootCoord->composeStamp = GRAPHICS_COORD_DIRTY;
}

/// Launches the actor by rotating its signed 16.16 local velocity into the root frame.
static void _patioLeapingStrangersLaunchLeap(Task* task)
{
    _Work*    work;
    GfxCoord* rootCoord;
    VECTOR    launchVelocity;

    rootCoord = task->extra.tmd->coords;
    work      = task->work;

    launchVelocity = _gPatioLeapingStrangersLeapLaunchVelocity;
    ApplyMatrixLV(&rootCoord->coord, &launchVelocity, &work->walk.velocity);
    work->airborne = 1;
    work->walk.motionStep++;
}

/// Stops the leap near floor height, plays the queued clip and sounds the landing.
static void _patioLeapingStrangersLandLeap(Task* task)
{
    enum {
        LANDING_Y     = -0x30, // Root-coordinate units; positive Y points down
        LANDING_SOUND = SOUND_CHARACTER(SOUND_BANK_LESSER_STRANGER, 0x0B),
    };
    GfxCoord*            rootCoord;
    _Work*               work;
    AnimationPlayRequest landingRequest;
    s32                  audioPan;

    rootCoord = task->extra.tmd->coords;
    work      = task->work;
    if (rootCoord->coord.t[1] < LANDING_Y) {
        return;
    }
    landingRequest.source.index         = 0;
    landingRequest.animationId          = work->model.nextAnimId;
    landingRequest.blend                = ANIMATION_BLEND_INTERPOLATE;
    landingRequest.blendFrames          = BLEND_FRAMES;
    landingRequest.enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE;
    _actorMotionPlayAnim19(task, ACTOR_MESSAGE_PLAY_ANIMATION, &landingRequest, 0);
    audioPan = (s8)worldCoordGetOriginAudioPan(rootCoord);
    sndEvtRequestScriptStart(LANDING_SOUND, audioPan, (s8)worldCoordGetOriginAudioDepth(rootCoord));

    work->walk.velocity.vx = 0;
    work->walk.velocity.vy = 0;
    work->walk.velocity.vz = 0;
    work->airborne         = 0;
    work->walk.motionStep++;
}

#include "../../../../shared/actor/actor_motion_play19.inc.c"

#include "../../../../shared/actor/actor_messages_place_euler.inc.c"

/// Handles the actor's draw mode and primitive-buffer ownership choice.
static s32 _patioLeapingStrangersSetDrawMode(Task* task, s32 messageId, s32 mode, s32 unusedArg)
{
    TmdObject* bodyModel;
    _Work*     work;
    s32        result;

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
        case ACTOR_MESSAGE_DRAW_SHOW_SKIP_AUTO_BUFFER:
            bodyModel->flags &= ~TMD_OBJECT_SKIP_ACTIVE_DRAW;
            bodyModel->flags |= TMD_OBJECT_SKIP_AUTO_BUFFER;
            break;
        default:
            result = 1;
            break;
    }
    return result;
}

/// Selects the head-turn weight ramp or prints the actor's root transform.
static s32 _patioLeapingStrangersApplyCommand(Task* task, s32 messageId, const ActorCommand* commandRequest, s32 unusedArg)
{
    enum { LOWER_HEAD_WEIGHT = 0,
           RAISE_HEAD_WEIGHT = 1 };
    _Work*    work;
    GfxCoord* rootCoord;
    SVECTOR   rootAngles;
    s32       command;

    work      = task->work;
    rootCoord = task->extra.tmd->coords;
    command   = commandRequest->command;
    switch (command) {
        case LOWER_HEAD_WEIGHT:
            work->turnWeightRising = 0;
            break;
        case RAISE_HEAD_WEIGHT:
            work->turnWeightRising = command;
            break;
        default:
            GPU_printf("%s=(%d,%d,%d)\n", "pos", rootCoord->coord.t[0], rootCoord->coord.t[1], rootCoord->coord.t[2]);
            gfxExtractSmallestEuler(&rootAngles, &rootCoord->coord);
            GPU_printf("%s=(%d,%d,%d)\n", "rot", rootAngles.vx, rootAngles.vy, rootAngles.vz);
            break;
    }
    return 0;
}
