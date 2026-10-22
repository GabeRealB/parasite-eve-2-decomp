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
#include "../../shared/actor/actor_motion.h"
#include "../../shared/actor/actor_messages.h"
#include "../../shared/walk/reversing_walker.h"

static void _modelPlacementAttachPartTask(Task* childTask);
static void _actor350700KyleMadiganWalkerInitialize(Task* task);
static void _actor350700KyleMadiganWalkerUpdate(Task* task);
static void _actor350700KyleMadiganAttachmentIdle(Task* task);
static void _actor350700KyleMadiganWalkerExit(Task* task);
static void _actor350700KyleMadiganWalkerBindLighting(Task* task);
static void _actor350700KyleMadiganWalkerIdle(Task* task);
static void _actor350700KyleMadiganWalkerRunStep(Task* task);
static void _actor350700KyleMadiganWalkerBeginMove(Task* task);

enum {
    ACTOR_350700_REVERSE_WALK_TASK_SPAWN  = 0,
    ACTOR_350700_REVERSE_WALK_TASK_UPDATE = 1,
    ACTOR_350700_REVERSE_WALK_TASK_EXIT   = 2,
};

/// Reversing walker lifecycle handlers: 0 spawn, 1 update, 2 exit.
///
/// Copied by value and indexed by `Task::state` in 0..2, without a terminator
/// or bounds check. Callbacks require a live task; update requires setup
/// work, and teardown may release both the task and its owned `Enemy`.
/// Actor-control holds suspend dispatch of all three slots.
static const TaskFuncTable3 _gActor350700ReverseWalkTaskStates = {
    .funcs = {
        [ACTOR_350700_REVERSE_WALK_TASK_SPAWN]  = _reverseWalkSpawn,
        [ACTOR_350700_REVERSE_WALK_TASK_UPDATE] = _reverseWalkUpdate,
        [ACTOR_350700_REVERSE_WALK_TASK_EXIT]   = _reverseWalkExit,
    },
};

/// Scripted walk phases indexed by `ReverseWalkWork::walk.motionStep` (0..3).
///
/// Face the target, begin moving, approach until arrival, then turn to the
/// destination yaw. Every entry is a `void(Task*)` callback; there is no terminator.
static const TaskFuncTable4 _gReverseWalkStepHandlers = { {
    _reverseWalkOrientForWalk,
    _reverseWalkBeginMove,
    _actorMotionArrive19,
    _reverseWalkTurnToYaw,
} };

/// Local +Z velocity of a forward reversing-walker step.
///
/// Signed 16.16: 32 parent-coordinate units per tick before root rotation.
/// Backward movement scales it by -0.4 before rotating into the parent frame.
static const VECTOR _gReverseWalkForwardVelocity = { 0, 0, 0x200000, 0 };

/// Task-state callbacks shared by Kyle Madigan's attached hands and gun model.
///
/// `Task::state` selects 0 attach to the body part, 1 idle under the body rig
/// or 2 kill the child. Body coordinates and lighting remain borrowed.
static const TaskFuncTable3 _gActor350700KyleMadiganAttachmentStates = { {
    _modelPlacementAttachPartTask,
    _actor350700KyleMadiganAttachmentIdle,
    taskKill,
} };

/// Kyle Madigan body states 0 initialize, 1 update and 2 exit.
///
/// `Task::state` indexes the complete three-handler copy without a bounds check
/// or terminator. Initialization owns walker work and hand/gun child tasks;
/// update requires the live twenty-part rig, and exit releases the enemy and
/// task tree. Actor-control holds suspend every slot.
static const TaskFuncTable3 _gActor350700KyleMadiganWalkerTaskStates = { {
    _actor350700KyleMadiganWalkerInitialize,
    _actor350700KyleMadiganWalkerUpdate,
    _actor350700KyleMadiganWalkerExit,
} };

/// Kyle Madigan walk steps 0 face target, 1 begin motion, 2 arrive and 3 turn to final yaw.
///
/// The dispatcher copies all four callbacks before indexing `walk.motionStep`;
/// the work's selector must remain in 0..3 and the callbacks require its live rig.
static const TaskFuncTable4 _gActor350700KyleMadiganWalkSteps = { {
    _actorMotionFaceTarget,
    _actor350700KyleMadiganWalkerBeginMove,
    _actorMotionArrive,
    _actorMotionTurnToYaw,
} };

enum { ACTOR_350700_KYLE_WALK_VELOCITY_ONE = 0x10000 };

/// Kyle Madigan's local +Z walk velocity: 32 coordinate units per update in signed 16.16.
///
/// The root's Q12 rotation converts XYZ to parent-space velocity without
/// translation. The fourth VECTOR word is zero and is not part of the motion.
static const VECTOR _gActor350700KyleMadiganForwardWalkVelocity = { 0, 0, 32 * ACTOR_350700_KYLE_WALK_VELOCITY_ONE, 0 };

extern TmdSource gKyleBody;
extern TmdSource gKyleHandRight;
extern TmdSource gKyleHandLeft;
extern TmdSource gKyleP229;
static s32       _actorMsgPlaceEuler(Task* task, s32 msgId, const ActorTransform* placement, s32 unusedArg);
static s32       _actor350700KyleMadiganWalkerSetDrawModeMsg(Task* task, s32 msgId, s32 mode, s32 unusedArg);
static s32       _actor350700KyleMadiganWalkerIgnoreCommandMsg(Task* task, s32 msgId, s32 unusedFirstArg, s32 unusedArg);
static void      _actor350700KyleMadiganAttachmentTask(Task* task);
static void      _actor350700KyleMadiganWalkerTask(Task* task);

static s32  _actor350700ReverseWalkCommandMsg(Task* task, s32 msgId, const ActorCommand* command, s32 unusedArg);
static void _actor350700ReverseWalkTask(Task* task);

#include "assets/characters/eve/masked_body.inc"

#include "assets/rooms/shelter/b6_training_room/eve_and_kyle_clips_1.inc"

/// Masked Eve's bank-0 body animation sets: clip 0 absent, clips 1..4 loaded.
///
/// These are the nineteen-part reversing walker's clips; its other body uses
/// a separate bank. `AnimationPlayRequest::animationId` selects a clip after
/// outer-bank selection. Playback borrows this table and its sets while loaded.
#include "assets/rooms/shelter/b6_training_room/eve_bank.inc"

/// Carrier-local banks for nineteen-part playback: only bank 0, clips 1..4.
///
/// `AnimationPlayRequest::source.index` selects the bank; `animationId` selects
/// an entry of its clip table. Playback borrows that table and loaded clip data.
static AnimationSet** const _gActorMotionAnimBanks19[1] SECTION(".data") = {
    _gActor350700BodyAnimSets,
};

TaskDesc gActor350700MaskedEveReverseWalkTaskDesc = { { { (TASK_BODY_TMD | TASK_DESC_SKIP_AUTO_MODEL_BUFFER), 192 } }, _actor350700ReverseWalkTask, { .model = &gEveMaskedBody } };

/// Terminated message table of this carrier's nineteen-part reversing walker.
///
/// Animation, placement, drawing and walk-to use shared handlers; the
/// direction command is package-specific. The initialized task borrows
/// this table and its callbacks until teardown.
static TaskMessageEntry _gReverseWalkMessages[6] = {
    { ACTOR_MESSAGE_PLAY_ANIMATION, _actorMotionPlayAnim19 },
    { ACTOR_MESSAGE_PLACE, actorMsgPlaceEuler },
    { ACTOR_MESSAGE_SET_MODEL_DRAW, _reverseWalkSetDrawModeMsg },
    { ACTOR_MESSAGE_WALK_TO, _reverseWalkStartWalkMsg },
    { ACTOR_COMMAND_MESSAGE_APPLY, _actor350700ReverseWalkCommandMsg },
    { TASK_MESSAGE_TABLE_END, NULL },
};

#include "assets/characters/kyle/body.inc"

#include "assets/characters/kyle/hand_right.inc"

#include "assets/characters/kyle/hand_left.inc"

#include "assets/characters/kyle/p229.inc"

#include "assets/rooms/shelter/b6_training_room/eve_and_kyle_clips_2.inc"

/// Kyle Madigan's bank-0 clip table for his twenty-part body rig.
///
/// Clip IDs 1..5 borrow loaded sets; entry 0 is NULL and cannot be played.
/// The table and animation resources must remain loaded while the rig uses them.
#include "assets/rooms/shelter/b6_training_room/kyle_bank.inc"

/// Actor-motion bank selection for Kyle's twenty-part body (only source.index 0).
///
/// Requests select loaded clips 1..5. This table and its borrowed sets must
/// remain loaded for playback; masked Eve's nineteen-part rig uses another bank.
static AnimationSet** _gActorMotionAnimBanks[1] = {
    _gActor350700KyleMadiganAnimationBank,
};

enum { ACTOR_350700_KYLE_TASK_PRIORITY = 192 };

TaskDesc gActor350700KyleMadiganTaskDescriptors[4] = {
    // 0: body.
    {
        .header   = { .fields = { .flags = (TASK_BODY_TMD | TASK_DESC_SKIP_AUTO_MODEL_BUFFER), .priority = ACTOR_350700_KYLE_TASK_PRIORITY } },
        .callback = _actor350700KyleMadiganWalkerTask,
        .data     = { .model = &gKyleBody },
    },
    // 1: left hand.
    {
        .header   = { .fields = { .flags = TASK_BODY_TMD, .priority = ACTOR_350700_KYLE_TASK_PRIORITY } },
        .callback = _actor350700KyleMadiganAttachmentTask,
        .data     = { .model = &gKyleHandLeft },
    },
    // 2: right hand.
    {
        .header   = { .fields = { .flags = TASK_BODY_TMD, .priority = ACTOR_350700_KYLE_TASK_PRIORITY } },
        .callback = _actor350700KyleMadiganAttachmentTask,
        .data     = { .model = &gKyleHandRight },
    },
    // 3: held gun.
    {
        .header   = { .fields = { .flags = TASK_BODY_TMD, .priority = ACTOR_350700_KYLE_TASK_PRIORITY } },
        .callback = _actor350700KyleMadiganAttachmentTask,
        .data     = { .model = &gKyleP229 },
    },
};

/// Kyle Madigan body messages for animation, placement, draw modes and scripted walking.
///
/// Installed on the initialized twenty-part body task, whose model and hand/gun
/// attachments must remain live for draw messages. Requests are borrowed during
/// synchronous dispatch. Actor commands are deliberately ignored with result
/// zero. The end marker makes unsupported message IDs return zero.
static TaskMessageEntry _gActor350700KyleMadiganWalkerMessageTable[6] = {
    { ACTOR_MESSAGE_PLAY_ANIMATION, _actorMotionPlayAnim },
    { ACTOR_MESSAGE_PLACE, _actorMsgPlaceEuler },
    { ACTOR_MESSAGE_SET_MODEL_DRAW, _actor350700KyleMadiganWalkerSetDrawModeMsg },
    { ACTOR_MESSAGE_WALK_TO, _actorMotionStartWalk },
    { ACTOR_COMMAND_MESSAGE_APPLY, _actor350700KyleMadiganWalkerIgnoreCommandMsg },
    { TASK_MESSAGE_TABLE_END, NULL },
};
#include "../../shared/walk/reversing_walker_update.inc.c"

#include "../../shared/actor/actor_motion_arrive19.inc.c"

#include "../../shared/walk/reversing_walker_start.inc.c"

/// Runs the nineteen-part reversing walker's task while scene actors are running.
///
/// Requires a live TMD task with state 0 (spawn), 1 (update) or 2 (exit);
/// the state index is unchecked. Spawn and exit require its owned `Enemy`
/// in `spawnArg2.pointer`; update requires initialized `ReverseWalkWork`.
/// Actor-control values other than `SCENE_COMBAT_ACTORS_RUNNING` suspend
/// all three states. Spawn failure or exit may release the task and resources.
static void _actor350700ReverseWalkTask(Task* task)
{
    const TaskFuncTable3 stateHandlers = _gActor350700ReverseWalkTaskStates;

    if (gSceneCombatState.actorControl == SCENE_COMBAT_ACTORS_RUNNING) {
        stateHandlers.funcs[task->state](task);
    }
}

#include "../../shared/walk/reversing_walker.inc.c"

#include "../../shared/actor/actor_motion_play19.inc.c"

#include "../../shared/actor/actor_messages_place_euler.inc.c"

#include "../../shared/walk/reversing_walker_visibility.inc.c"

/// Selects whether the reversing walker approaches its targets forward or backward.
///
/// Requires an initialized `ReverseWalkWork` and a borrowed, readable command.
/// Command 1 selects backing, 2 selects forward walking; other commands leave
/// the direction unchanged. The context, message ID and second payload are
/// ignored. Retains no command pointer and always returns 0.
static s32 _actor350700ReverseWalkCommandMsg(Task* task, s32 msgId, const ActorCommand* command, s32 unusedArg)
{
    enum {
        ACTOR_350700_REVERSE_WALK_COMMAND_BACKWARD = 1,
        ACTOR_350700_REVERSE_WALK_COMMAND_FORWARD  = 2,
    };
    ReverseWalkWork* work;

    work = task->work;
    switch (command->command) {
        case ACTOR_350700_REVERSE_WALK_COMMAND_BACKWARD:
            work->walksForward = 0;
            break;
        case ACTOR_350700_REVERSE_WALK_COMMAND_FORWARD:
            work->walksForward = 1;
            break;
    }
    return 0;
}

/// Initializes Kyle Madigan's walker and its hand and gun attachments.
///
/// Owns zeroed `KyleMadiganWalkerWork`; allocation failure exits immediately.
/// Left/right hands occupy slots 0/1 and attach at body parts 8/12 here. The
/// `Enemy` in `spawnArg2` supplies their placement index in the current area variant;
/// existing primitive-buffer halves are rebuilt after texture relocation. The
/// gun attaches at part 8. Child failures leave zero pointers while messages and
/// exit state are still installed; later draw messages require all three live.
/// Placement and animation are left to subsequent actor messages.
static void _actor350700KyleMadiganWalkerInitialize(Task* task)
{
    enum {
        ACTOR_350700_KYLE_FREE_DISABLED   = -1,
        ACTOR_350700_KYLE_LEFT_HAND_TASK  = 1,
        ACTOR_350700_KYLE_RIGHT_HAND_TASK = 2,
        ACTOR_350700_KYLE_HELD_TASK       = 3,
        ACTOR_350700_KYLE_LEFT_HAND_PART  = 8,
        ACTOR_350700_KYLE_RIGHT_HAND_PART = 12,
    };
    KyleMadiganWalkerWork* work;
    Task*                  childTask;

    work = memCalloc(sizeof(KyleMadiganWalkerWork), false);
    if (work == NULL) {
        enemyTaskExit(task);
        return;
    }
    task->work               = work;
    work->model.animId       = ACTOR_MOTION_ANIMATION_INDEX_NONE;
    work->model.bank         = ACTOR_MOTION_ANIMATION_INDEX_NONE;
    work->freeCountdown      = ACTOR_350700_KYLE_FREE_DISABLED;
    work->walk.carry[0].word = 0;
    work->walk.carry[1].word = 0;
    work->walk.carry[2].word = 0;
    childTask                = taskSpawnFromTable(gActor350700KyleMadiganTaskDescriptors, ACTOR_350700_KYLE_LEFT_HAND_TASK, ACTOR_350700_KYLE_LEFT_HAND_PART, task);
    if (childTask != NULL) {
        work->handTasks[0] = childTask;
        _actorRenderApplyTaskPlacementTextureOffsets(childTask, task->spawnArg2.pointer);
    }
    childTask = taskSpawnFromTable(gActor350700KyleMadiganTaskDescriptors, ACTOR_350700_KYLE_RIGHT_HAND_TASK, ACTOR_350700_KYLE_RIGHT_HAND_PART, task);
    if (childTask != NULL) {
        work->handTasks[1] = childTask;
        _actorRenderApplyTaskPlacementTextureOffsets(childTask, task->spawnArg2.pointer);
    }
    childTask = taskSpawnFromTable(gActor350700KyleMadiganTaskDescriptors, ACTOR_350700_KYLE_HELD_TASK, ACTOR_350700_KYLE_LEFT_HAND_PART, task);
    if (childTask != NULL) {
        work->heldItemTask = childTask;
    }
    _actor350700KyleMadiganWalkerBindLighting(task);
    task->msgTable     = _gActor350700KyleMadiganWalkerMessageTable;
    task->exitCallback = _actor350700KyleMadiganWalkerExit;
    task->state       += 1;
}

/// Applies Kyle Madigan's walk velocity to his root's integer translation.
///
/// Work and root must be separate, live and writable. Velocity is signed
/// 16.16 displacement in the root's parent frame, in coordinate units per
/// update. Adds the signed integer halves to XYZ and retains each unsigned
/// fractional half (0..65535) for the next update, including negative motion.
/// Marks composition dirty even for zero velocity; rotation is unchanged.
/// Neither pointer is retained, and the caller keeps ownership of both objects.
static inline void _actor350700KyleMadiganWalkerIntegrateVelocity(KyleMadiganWalkerWork* work, GfxCoord* rootCoord)
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

/// Advances Kyle Madigan's scripted walk, animation and model presentation.
///
/// Requires initialized `KyleMadiganWalkerWork`, a body with coordinates 0..19,
/// and motion 0 (idle) or 1 (walking). A ticking rig must already be bound.
/// Integrates velocity in signed 16.16 coordinate units per tick and drives
/// animation slots 1..19. Part 1 supplies the world position for the shadow and
/// lighting; it is recomposed when the view is ready. Lighting is also refreshed
/// unconditionally, including a second refresh on those frames.
/// A nonnegative buffer countdown is decremented once; a tick entering at zero
/// releases the body's primitive buffer. -1 disables the countdown.
static void _actor350700KyleMadiganWalkerUpdate(Task* task)
{
    enum { ACTOR_350700_KYLE_MADIGAN_SHADOW_HALF_SIZE = 0x300 };
    TmdObject*             bodyModel        = task->extra.tmd;
    KyleMadiganWalkerWork* work             = task->work;
    TaskFunc               motionHandlers[] = { _actor350700KyleMadiganWalkerIdle, _actor350700KyleMadiganWalkerRunStep };
    VECTOR3                groundPoint;
    GfxCoord*              rootCoord;
    s32                    slotIndex;

    // Run the walk step before consuming its velocity for this frame.
    motionHandlers[work->walk.motion](task);
    rootCoord = task->extra.tmd->coords;
    _actor350700KyleMadiganWalkerIntegrateVelocity(work, rootCoord);
    if (work->model.ticking != 0) {
        for (slotIndex = 1; slotIndex < ARRAY_SIZE(work->rig.slots); slotIndex++) {
            animationTickSlot(&work->rig.anim, slotIndex);
        }
    }
    if (!(bodyModel->flags & TMD_OBJECT_SKIP_ACTIVE_DRAW)) {
        if (worldCollisionProjectGroundPoint(GRAPHICS_MATRIX_TRANSLATION(&task->extra.tmd->coords[1].workm), &groundPoint) != 0) {
            effectDrawGroundShadow(&groundPoint, ACTOR_350700_KYLE_MADIGAN_SHADOW_HALF_SIZE, gRoomEffectState->groundShadowShade);
        }
    }
    // The presentation frame is part 1; retain both lighting queries when ready.
    if (gGameSession->viewReady != 0) {
        task->extra.tmd->coords[1].composeStamp = GRAPHICS_COORD_DIRTY;
        actorRenderComposeCoord(&task->extra.tmd->coords[1]);
        worldCoordSetModelLighting(bodyModel, task->extra.tmd->coords[1].workm.t, 0, 3);
    }
    worldCoordSetModelLighting(bodyModel, task->extra.tmd->coords[1].workm.t, 0, 3);
    // Buffer release is delayed while the hidden body stops drawing.
    if (work->freeCountdown >= 0) {
        if (work->freeCountdown == 0) {
            tmdFreePrimitiveBuffer(bodyModel);
        }
        work->freeCountdown--;
    }
}

#include "../../shared/actor/actor_motion_arrive.inc.c"

#include "../../shared/actor/actor_motion_start.inc.c"

/// Runs the attachment task for either hand or the gun on Kyle Madigan's body.
///
/// State must be 0 (attach to the parent part), 1 (idle) or 2 (kill the task).
/// Setup requires the live parent task and part index in the spawn arguments;
/// the parent owns the attachment's coordinate and lighting lifetime.
static void _actor350700KyleMadiganAttachmentTask(Task* task)
{
    TaskFuncTable3 stateHandlers;

    stateHandlers = _gActor350700KyleMadiganAttachmentStates;
    stateHandlers.funcs[task->state](task);
}

#include "../../shared/actor/model_placement_attach_part.inc.c"

/// Leaves an attached hand or gun idle while the parent's rig drives its transform.
static void _actor350700KyleMadiganAttachmentIdle(Task* task)
{
}

/// Runs Kyle Madigan's body task while actor control is running.
///
/// State must be 0 (spawn), 1 (update) or 2 (exit). Actor-control values other
/// than `SCENE_COMBAT_ACTORS_RUNNING` suspend all three states; attachment
/// dispatch is independent of that gate.
static void _actor350700KyleMadiganWalkerTask(Task* task)
{
    TaskFuncTable3 stateHandlers;

    stateHandlers = _gActor350700KyleMadiganWalkerTaskStates;
    if (gSceneCombatState.actorControl == SCENE_COMBAT_ACTORS_RUNNING) {
        stateHandlers.funcs[task->state](task);
    }
}

/// Releases Kyle Madigan's enemy record and tears down the body and its children.
///
/// Requires a live task with its owned `Enemy` in `spawnArg2.pointer`, including
/// an incomplete spawn. Task teardown releases the work and model resources.
/// The task and enemy must not be accessed after this call.
static void _actor350700KyleMadiganWalkerExit(Task* task)
{
    enemyTaskExit(task);
}

/// Lends Kyle Madigan's work-owned lighting matrices to his body model.
///
/// Requires an allocated `KyleMadiganWalkerWork` and a live TMD body. The work
/// must outlive model use and attachments that copy these matrix pointers.
static void _actor350700KyleMadiganWalkerBindLighting(Task* task)
{
    TmdObject*             bodyModel;
    KyleMadiganWalkerWork* work;

    bodyModel           = task->extra.tmd;
    work                = task->work;
    bodyModel->lightMtx = &work->model.light;
    bodyModel->colorMtx = &work->model.color;
}

/// Leaves the scripted walk idle while the body update still drives presentation.
static void _actor350700KyleMadiganWalkerIdle(Task* task)
{
}

/// Runs the current step of Kyle Madigan's scripted walk.
///
/// Requires initialized work and `walk.motionStep` in 0..3: face the target,
/// start moving, detect arrival, then turn to the requested final yaw.
static void _actor350700KyleMadiganWalkerRunStep(Task* task)
{
    KyleMadiganWalkerWork* work;
    TaskFuncTable4         handlers;

    work     = task->work;
    handlers = _gActor350700KyleMadiganWalkSteps;
    handlers.funcs[work->walk.motionStep](task);
}

#include "../../shared/actor/actor_motion_face.inc.c"

/// Starts Kyle Madigan moving along his root's local forward axis.
///
/// Requires initialized work and the root rotation set by the facing step.
/// Rotates a +Z velocity of 32 coordinate units per tick, encoded in signed
/// 16.16, into the root's parent frame. Seeds the first arrival comparison and
/// advances to walk step 2; existing fractional displacement is retained.
static void _actor350700KyleMadiganWalkerBeginMove(Task* task)
{
    KyleMadiganWalkerWork* work;
    GfxCoord*              rootCoord;
    VECTOR                 forwardVelocity;

    rootCoord = task->extra.tmd->coords;
    work      = task->work;

    // Rotate velocity only; the root's translation must not affect the step.
    forwardVelocity = _gActor350700KyleMadiganForwardWalkVelocity;
    ApplyMatrixLV(&rootCoord->coord, &forwardVelocity, &work->walk.velocity);
    work->walk.lastDistance.vx = ACTOR_MOTION_INITIAL_ARRIVAL_GAP;
    work->walk.lastDistance.vy = ACTOR_MOTION_INITIAL_ARRIVAL_GAP;
    work->walk.lastDistance.vz = ACTOR_MOTION_INITIAL_ARRIVAL_GAP;
    work->walk.motionStep++;
}

#include "../../shared/actor/actor_motion_turn.inc.c"

#include "../../shared/actor/actor_motion_play.inc.c"

/// Selects the private placement handler for the Kyle Madigan model task.
#define ACTOR_MESSAGE_PLACE_EULER_HANDLER _actorMsgPlaceEuler
#include "../../shared/actor/actor_messages_place_euler.inc.c"
#undef ACTOR_MESSAGE_PLACE_EULER_HANDLER

/// Applies Kyle Madigan's body draw mode and copies its flags to both hands and gun.
///
/// Requires initialized work and all four live TMD tasks, even for an unknown
/// mode. Mode 0 hides and enables automatic buffer recovery; 1 shows, allocates
/// the body's buffer and enables recovery; 2 hides, disables recovery and sets
/// the body buffer-release countdown to 2; 3 shows with recovery disabled.
/// Other modes return 1 without changing body flags; handled modes return 0.
/// Every mode replaces each attachment's entire flag word with the body's.
/// Showing does not cancel a pending release. The message ID and second
/// payload are ignored; no payload storage is borrowed.
static s32 _actor350700KyleMadiganWalkerSetDrawModeMsg(Task* task, s32 msgId, s32 mode, s32 unusedArg)
{
    enum { ACTOR_350700_KYLE_MADIGAN_DRAW_SHOW_SKIP_AUTO_BUFFER = 3 };
    KyleMadiganWalkerWork* work;
    TmdObject*             bodyModel;
    TmdObject*             leftHandModel;
    TmdObject*             rightHandModel;
    TmdObject*             heldItemModel;
    u16                    flags;
    s32                    result;

    work           = task->work;
    bodyModel      = task->extra.tmd;
    leftHandModel  = work->handTasks[0]->extra.tmd;
    rightHandModel = work->handTasks[1]->extra.tmd;
    heldItemModel  = work->heldItemTask->extra.tmd;
    result         = 0;
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
        case ACTOR_350700_KYLE_MADIGAN_DRAW_SHOW_SKIP_AUTO_BUFFER:
            bodyModel->flags &= ~TMD_OBJECT_SKIP_ACTIVE_DRAW;
            bodyModel->flags |= TMD_OBJECT_SKIP_AUTO_BUFFER;
            break;
        default:
            result = 1;
            break;
    }
    // Attachments inherit the whole flag word, including unrelated body flags.
    flags                 = bodyModel->flags;
    rightHandModel->flags = flags;
    leftHandModel->flags  = flags;
    heldItemModel->flags  = flags;
    return result;
}

/// Accepts actor-command messages for Kyle Madigan without changing his state.
///
/// Ignores every argument, dereferences no payload and always returns 0. This
/// consumes scene broadcasts while animation and walk messages drive the actor.
static s32 _actor350700KyleMadiganWalkerIgnoreCommandMsg(Task* task, s32 msgId, s32 unusedFirstArg, s32 unusedArg)
{
    return 0;
}
