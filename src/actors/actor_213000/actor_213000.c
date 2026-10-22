#include "actors/task_tables.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>

#include "common.h"

#include "actors/actor.h"

#include "gameplay/actor_render.h"
#include "gameplay/animation.h"
#include "gameplay/area.h"
#include "gameplay/areaplace.h"
#include "gameplay/enemy.h"
#include "gameplay/message.h"
#include "gameplay/scene_runtime.h"
#include "gameplay/world_coords.h"

#include "main/coord.h"
#include "main/mem.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/task.h"
#include "main/tmd.h"
#include "main/tmd_types.h"
#include "../../shared/actor/model_placement.h"
#include "../../shared/actor/actor_messages.h"

/// Work block of Eric Baldwin as a room script poses him: what his body model
/// plays, the matrices it is lit with and the models he holds.
///
/// The spawn state allocates it zeroed and keeps it at `Task::work` for the
/// task's life. It opens with the twenty-part rig and the bytes that say what
/// the rig is playing; the model object borrows `light` and `color` for as
/// long as the block lives.
///
/// The head is not `ActorModelState`: the byte that type gives a walk's next
/// clip is the free countdown here, and a word separates the bytes from the
/// matrices.
typedef struct {
    ActorAnimRig20 rig;               // Playback storage of the twenty-part body model; slots 1 to 19 are driven
    s8             ticking;           // Set once a clip has been applied, never cleared: the slots are ticked each frame
    s8             animId;            // Clip the slots were last seeded with, within `bank` (`ACTOR_MOTION_ANIMATION_INDEX_NONE` before the first request)
    s8             bank;              // Index, in the package's animation bank table, of the bank the rig is bound to (`ACTOR_MOTION_ANIMATION_INDEX_NONE` before the first request)
    s8             freeCountdown;     // Ticks left before the body model's buffers are freed, which the tick finding 0 does (-1 no free pending)
    s32            field_478;         // Cleared by the spawn state and never read; role unproven
    MATRIX         light;             // Light-direction matrix lent to the model object
    MATRIX         color;             // Light-colour matrix lent to the model object
    Task*          heldModelTasks[2]; // Tasks drawing the two models hung on body part 8, a hand; an actor command shows or hides each. `NULL` where the spawn failed
} _Actor213000EricBaldwinWork;
STATIC_ASSERT_SIZEOF(_Actor213000EricBaldwinWork, 0x4C4);

static void _modelPlacementAttachPartTask(Task* childTask);
static void _modelPlacementMirrorParentDrawFlags(Task* childTask);
static void _modelPlacementMirrorParentDrawFlagsTask(Task* childTask);
static void _actor213000HeldModelIdle(Task* unusedTask);
static void _actor213000AttachThreeRootModel(Task* childTask);
static void _actor213000UpdateBody(Task* actorTask);
static void _actor213000InitBodyLighting(Task* actorTask);

static TmdSource _gEricBody;
static TmdSource _gEricArmRight;
static TmdSource _gEricHandRight;
static TmdSource _gMistR18BriefingFile;
static TmdSource _gMistR18Laptop;
static s32       _actor213000PlayAnimation(Task* actorTask, s32 messageId, const AnimationPlayRequest* request, s32 unusedSecondArg);
static s32       _actor213000SetModelDraw(Task* actorTask, s32 messageId, s32 mode, s32 unusedSecondArg);
static s32       _actor213000ApplyHeldModelCommand(Task* actorTask, s32 messageId, const ActorCommand* request, s32 unusedSecondArg);
static void      _actor213000HeldModelTask(Task* heldModelTask);
static void      _actor213000RightHandTask(Task* handTask);
static void      _actor213000ThreeRootModelTask(Task* childTask);
static void      _actor213000BodyTask(Task* actorTask);

/// Body part whose world translation supplies the lighting sample.
enum { ACTOR_213000_LIGHTING_PART = 1 };

#include "assets/characters/eric/body.inc"

#include "assets/characters/eric/arm_right.inc"

#include "assets/characters/eric/hand_right.inc"

#include "assets/rooms/mist/r18/briefing_file.inc"

#include "assets/rooms/mist/r18/laptop.inc"

#include "assets/rooms/mist/r18/eric_clips.inc"

/// Eric Baldwin's bank-0 clip sets, with loaded clips at indices 1..10.
///
/// Slot 0 is NULL. Requests select bank 0 through the separate bank collection
/// and then select a clip here by animationId. The rig borrows the table and
/// its sets; the package must stay loaded throughout scripted playback.
#include "assets/rooms/mist/r18/eric_bank.inc"

/// Eric Baldwin's animation-bank directory, selected by a playback request's source index.
///
/// Bank 0 is the sole supported selector and contains clips 1..10 with slot 0
/// empty. The rig borrows its clip table and sets until playback ends; the
/// directory and its resources must remain loaded throughout the body task.
static AnimationSet** _gActor213000EricBaldwinAnimationBanks[1] = {
    _gActor213000EricBaldwinAnimationSets,
};

enum { ACTOR_213000_ERIC_TASK_PRIORITY = 192 };

TaskDesc gActor213000EricBaldwinTaskDescriptors[5] = {
    // 0: body.
    {
        .header   = { .fields = { .flags = TASK_BODY_TMD, .priority = ACTOR_213000_ERIC_TASK_PRIORITY } },
        .callback = _actor213000BodyTask,
        .data     = { .model = &_gEricBody },
    },
    // 1: first held model.
    {
        .header   = { .fields = { .flags = TASK_BODY_TMD, .priority = ACTOR_213000_ERIC_TASK_PRIORITY } },
        .callback = _actor213000HeldModelTask,
        .data     = { .model = &_gMistR18BriefingFile },
    },
    // 2: second held model.
    {
        .header   = { .fields = { .flags = TASK_BODY_TMD, .priority = ACTOR_213000_ERIC_TASK_PRIORITY } },
        .callback = _actor213000HeldModelTask,
        .data     = { .model = &_gMistR18Laptop },
    },
    // 3: three-root attachment.
    {
        .header   = { .fields = { .flags = TASK_BODY_TMD, .priority = ACTOR_213000_ERIC_TASK_PRIORITY } },
        .callback = _actor213000ThreeRootModelTask,
        .data     = { .model = &_gEricArmRight },
    },
    // 4: right hand.
    {
        .header   = { .fields = { .flags = TASK_BODY_TMD, .priority = ACTOR_213000_ERIC_TASK_PRIORITY } },
        .callback = _actor213000RightHandTask,
        .data     = { .model = &_gEricHandRight },
    },
};

/// Eric Baldwin's body messages for animation, placement, drawing and held-model commands.
///
/// Payloads are borrowed play requests, transforms, integer draw modes and
/// ActorCommand records respectively. Four handlers precede the end marker.
/// The body borrows this table and callback code while it can receive messages.
static const TaskMessageEntry _gActor213000EricBaldwinBodyMessages[5] SECTION(".data") = {
    { ACTOR_MESSAGE_PLAY_ANIMATION, _actor213000PlayAnimation },
    { ACTOR_MESSAGE_PLACE, actorMsgPlaceEuler },
    { ACTOR_MESSAGE_SET_MODEL_DRAW, _actor213000SetModelDraw },
    { ACTOR_COMMAND_MESSAGE_APPLY, _actor213000ApplyHeldModelCommand },
    { TASK_MESSAGE_TABLE_END, NULL },
};

/// Initializes Eric Baldwin's hidden body and four attached models.
///
/// Owns zeroed body work; allocation failure exits immediately. The two held
/// models attach at part 8, the three-root model at part 9 and the right hand
/// at part 12. Their texture offsets use the placement owner in spawnArg2 and
/// the current area variant; existing packet-buffer halves are both rebuilt.
/// Each child may fail independently. Installs lighting, messages and common
/// enemy teardown before advancing to the update state.
static void _actor213000InitializeEricBaldwin(Task* task)
{
    enum {
        ACTOR_213000_FREE_DISABLED    = -1,
        ACTOR_213000_FIRST_HELD_TASK  = 1,
        ACTOR_213000_SECOND_HELD_TASK = 2,
        ACTOR_213000_THREE_ROOT_TASK  = 3,
        ACTOR_213000_RIGHT_HAND_TASK  = 4,
        ACTOR_213000_HELD_PART        = 8,
        ACTOR_213000_THREE_ROOT_PART  = 9,
        ACTOR_213000_RIGHT_HAND_PART  = 12,
    };
    _Actor213000EricBaldwinWork* work;
    TmdObject*                   bodyModel;
    Task*                        threeRootTask;
    Task*                        rightHandTask;

    bodyModel = task->extra.tmd;
    work      = memCalloc(sizeof(*work), false);
    if (work == NULL) {
        enemyTaskExit(task);
        return;
    }
    task->work              = work;
    work->animId            = ACTOR_MOTION_ANIMATION_INDEX_NONE;
    work->bank              = ACTOR_MOTION_ANIMATION_INDEX_NONE;
    work->field_478         = 0;
    work->freeCountdown     = ACTOR_213000_FREE_DISABLED;
    bodyModel->flags       |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
    work->heldModelTasks[0] = taskSpawnFromTable(gActor213000EricBaldwinTaskDescriptors, ACTOR_213000_FIRST_HELD_TASK, ACTOR_213000_HELD_PART, task);
    work->heldModelTasks[1] = taskSpawnFromTable(gActor213000EricBaldwinTaskDescriptors, ACTOR_213000_SECOND_HELD_TASK, ACTOR_213000_HELD_PART, task);
    threeRootTask           = taskSpawnFromTable(gActor213000EricBaldwinTaskDescriptors, ACTOR_213000_THREE_ROOT_TASK, ACTOR_213000_THREE_ROOT_PART, task);
    rightHandTask           = taskSpawnFromTable(gActor213000EricBaldwinTaskDescriptors, ACTOR_213000_RIGHT_HAND_TASK, ACTOR_213000_RIGHT_HAND_PART, task);
    if (threeRootTask != NULL) {
        _actorRenderApplyTaskPlacementTextureOffsets(threeRootTask, task->spawnArg2.pointer);
    }
    if (rightHandTask != NULL) {
        _actorRenderApplyTaskPlacementTextureOffsets(rightHandTask, task->spawnArg2.pointer);
    }
    _actor213000InitBodyLighting(task);
    task->msgTable     = _gActor213000EricBaldwinBodyMessages;
    task->exitCallback = enemyTaskExit;
    task->state++;
}

/// Task-state callbacks shared by Eric Baldwin's two held models.
///
/// `Task::state` selects 0 attach to the body part, 1 idle or 2 kill the child.
/// The body supplies borrowed coordinates and lighting; drawing is controlled separately.
static const TaskFuncTable3 _gActor213000HeldModelStates = {
    {
        _modelPlacementAttachPartTask,
        _actor213000HeldModelIdle,
        taskKill,
    },
};

/// Runs a held model's attachment, idle or teardown state.
///
/// Requires a live TMD task and state 0..2. Setup borrows the parent task in
/// `spawnArg2.pointer` and the body-part index in `spawnArg1.value` (8 here),
/// retaining the model's local pose and draw flags. Commands control its active
/// drawing independently of the body. State 2 may release the task.
static void _actor213000HeldModelTask(Task* heldModelTask)
{
    TaskFuncTable3 states;

    states = _gActor213000HeldModelStates;
    states.funcs[heldModelTask->state](heldModelTask);
}

#include "../../shared/actor/model_placement_attach_part.inc.c"

/// Keeps a held model attached without changing its pose, flags or task state.
///
/// The task argument is unused; rendering follows the borrowed parent coordinate.
static void _actor213000HeldModelIdle(Task* unusedTask)
{
}

/// Task-state callbacks for Eric Baldwin's right-hand model.
///
/// `Task::state` selects 0 attach, 1 inherit the body's drawing and buffer
/// policy or 2 kill the hand. Body coordinates and lighting remain borrowed.
static const TaskFuncTable3 _gActor213000RightHandStates = {
    {
        _modelPlacementAttachChild,
        _modelPlacementMirrorParentDrawFlags,
        taskKill,
    },
};

/// Runs the right-hand model's attachment, draw-policy update or teardown state.
///
/// Requires a live one-part TMD task and state 0..2. Setup attaches its retained
/// local pose to the parent in `spawnArg2.pointer` at `spawnArg1.value` (part
/// 12 here), borrows lighting and joins the parent's teardown tree. The update
/// inherits active-draw exclusion and automatic-buffer policy. State 2 may
/// release the task; the parent and its borrowed resources must remain live
/// while the hand uses them.
static void _actor213000RightHandTask(Task* handTask)
{
    TaskFuncTable3 states;

    states = _gActor213000RightHandStates;
    states.funcs[handTask->state](handTask);
}

#include "../../shared/actor/model_placement_attach.inc.c"

#include "../../shared/actor/model_placement_mirror_parent.inc.c"

/// Attaches a fresh child-model part at the origin of a parent-model part.
///
/// Clears local translation and stored Euler angles (0x1000 units per turn),
/// retaining the existing local rotation matrix. Composition uses that matrix,
/// so the caller must supply the intended rotation.
///
/// Both coordinates must be live and distinct; the parent's ancestry must exclude
/// `childRoot` and remain acyclic. The parent link is borrowed and must remain live
/// while the child uses it. Marks the cached `workm` stale; compose the child before
/// using that matrix.
static inline void _actor213000LinkAttachmentRoot(GfxCoord* childRoot, GfxCoord* parentPart)
{
    childRoot->parent       = parentPart;
    childRoot->coord.t[0]   = 0;
    childRoot->coord.t[1]   = 0;
    childRoot->coord.t[2]   = 0;
    childRoot->param.rot.vx = 0;
    childRoot->param.rot.vy = 0;
    childRoot->param.rot.vz = 0;
    childRoot->composeStamp = GRAPHICS_COORD_DIRTY;
}

/// Attaches three child model parts independently to body parts 9..11.
///
/// Requires the fresh three-part TMD task and a live twenty-part body task in
/// `spawnArg2.pointer`. Replaces the child's original coordinate hierarchy,
/// zeroes local translations and stored Euler angles, and retains the source's
/// identity rotation matrices. Borrows the body's lighting, inherits active-draw
/// exclusion and automatic-buffer policy, and allocates a missing buffer when
/// permitted. Other flags, including flagged-pass selection, are retained.
///
/// The parent coordinates and lighting must outlive the child's uses. Joins
/// the parent's teardown tree and advances state 0 to 1 even if buffer allocation
/// fails; the update can retry. Draws four OT entries earlier, which must remain
/// within the selected ordering table. Spawn arguments are retained.
static void _actor213000AttachThreeRootModel(Task* childTask)
{
    enum {
        ACTOR_213000_ATTACHMENT_PART_COUNT        = 3,
        ACTOR_213000_ATTACHMENT_FIRST_PARENT_PART = 9,
        ACTOR_213000_ATTACHMENT_OT_OFFSET         = -4,
    };

    Task*      parentTask;
    TmdObject* childModel;
    TmdObject* parentModel;
    GfxCoord*  parentPart;
    GfxCoord*  childRoot;
    s32        partIndex;
    u16        initialFlags;

    parentTask  = childTask->spawnArg2.pointer;
    childModel  = childTask->extra.tmd;
    parentModel = parentTask->extra.tmd;
    // Each child part follows its corresponding body part instead of another child part.
    for (partIndex = 0; partIndex < ACTOR_213000_ATTACHMENT_PART_COUNT; partIndex++) {
        parentPart = &parentTask->extra.tmd->coords[partIndex + ACTOR_213000_ATTACHMENT_FIRST_PARENT_PART];
        childRoot  = &childTask->extra.tmd->coords[partIndex];
        _actor213000LinkAttachmentRoot(childRoot, parentPart);
    }
    childModel->lightMtx = parentModel->lightMtx;
    childModel->colorMtx = parentModel->colorMtx;
    initialFlags         = childModel->flags | TMD_OBJECT_SKIP_ACTIVE_DRAW;
    childModel->flags    = initialFlags;
    if (!(parentModel->flags & TMD_OBJECT_SKIP_ACTIVE_DRAW)) {
        childModel->flags = initialFlags & (u16)~TMD_OBJECT_SKIP_ACTIVE_DRAW;
    }
    // Recover missing primitive buffers even while active drawing is excluded.
    if (!(parentModel->flags & TMD_OBJECT_SKIP_AUTO_BUFFER)) {
        childModel->flags &= (u16)~TMD_OBJECT_SKIP_AUTO_BUFFER;
        tmdAllocPrimitiveBuffer(childModel);
    } else {
        childModel->flags |= TMD_OBJECT_SKIP_AUTO_BUFFER;
    }
    childModel->otOffset = ACTOR_213000_ATTACHMENT_OT_OFFSET;
    taskReparent(parentTask, childTask);
    childTask->state += 1;
}

/// Selects the private flag-mirroring tick for the three-root attached model.
///
/// The value is the `static void(Task*)` callback declared in this carrier's
/// prologue. The following fragment consumes and undefines the binding.
#define MODEL_PLACEMENT_MIRROR_PARENT_DRAW_FLAGS_TASK _modelPlacementMirrorParentDrawFlagsTask
#include "../../shared/actor/model_placement_mirror_parent.inc.c"

/// Task-state callbacks for Eric Baldwin's three-root attached model.
///
/// `Task::state` selects 0 attach roots to body parts 9..11, 1 inherit drawing
/// and buffer policy or 2 kill the child. Body coordinates and lighting remain borrowed.
static const TaskFuncTable3 _gActor213000ThreeRootModelStates = {
    {
        _actor213000AttachThreeRootModel,
        _modelPlacementMirrorParentDrawFlagsTask,
        taskKill,
    },
};

/// Runs the three-part model's body attachment, draw-policy update or teardown state.
///
/// Requires a live TMD task and state 0..2. Setup uses
/// `_actor213000AttachThreeRootModel`; subsequent updates inherit the body's
/// active-draw and buffer policy. State 2 may release the task.
static void _actor213000ThreeRootModelTask(Task* childTask)
{
    TaskFuncTable3 states;

    states = _gActor213000ThreeRootModelStates;
    states.funcs[childTask->state](childTask);
}

/// Eric Baldwin's body lifecycle: 0 initialize, 1 update, 2 release the actor.
///
/// Task state indexes a complete twelve-byte value copy, without a terminator.
/// Initialization owns body work and four child tasks; keep this package and
/// the live Enemy spawn payload available until teardown.
static const TaskFuncTable3 _gActor213000EricBaldwinBodyStates = {
    {
        _actor213000InitializeEricBaldwin,
        _actor213000UpdateBody,
        enemyTaskExit,
    },
};

/// Runs Eric Baldwin's body spawn, update or teardown state.
///
/// Requires a live twenty-part TMD task and state 0..2. State 0 owns allocation
/// of the animation/lighting work and four child tasks; state 1 consumes that
/// initialized work. `spawnArg2.pointer` borrows the scene's `Enemy` record.
/// Allocation failure or state 2 may release the task and its resources.
static void _actor213000BodyTask(Task* actorTask)
{
    TaskFuncTable3 states;

    states = _gActor213000EricBaldwinBodyStates;
    states.funcs[actorTask->state](actorTask);
}

/// Advances body animation, refreshes lighting and services delayed buffer release.
///
/// Requires the initialized body task and its live work/model. Advances slots
/// 1..19 after playback starts, retaining the root pose. When the view is ready,
/// composes part 1 and samples room/transient lights at its world translation.
/// A nonnegative buffer countdown is decremented once per call; a call finding
/// zero releases both primitive halves and leaves -1 (no release pending).
/// GPU use must have finished before that release. Model and work remain owned
/// by the task; this does not end playback or change draw flags.
static void _actor213000UpdateBody(Task* actorTask)
{
    _Actor213000EricBaldwinWork* work;
    TmdObject*                   model;
    GfxCoord*                    lightingPart;
    s32                          slotIndex;

    model        = actorTask->extra.tmd;
    work         = actorTask->work;
    lightingPart = &model->coords[ACTOR_213000_LIGHTING_PART];
    if (work->ticking != 0) {
        for (slotIndex = 1; slotIndex < ARRAY_SIZE(work->rig.slots); slotIndex++) {
            animationTickSlot(&work->rig.anim, slotIndex);
        }
    }
    // Sample lighting after animation has changed the model-part transforms.
    if (gGameSession->viewReady != 0) {
        lightingPart->composeStamp = GRAPHICS_COORD_DIRTY;
        actorRenderComposeCoord(lightingPart);
        worldCoordSetModelLighting(model, lightingPart->workm.t, 0, ARRAY_SIZE(work->light.m));
    }
    if (work->freeCountdown >= 0) {
        if (work->freeCountdown == 0) {
            tmdFreePrimitiveBuffer(model);
        }
        work->freeCountdown--;
    }
}

/// Binds the body's work-owned lighting matrices and samples lights at part 1.
///
/// Requires the allocated work, twenty-part model and current view transform.
/// Composes part 1 before using its world-unit translation. The model borrows
/// the work's matrices while it uses lighting; neither allocation is transferred
/// or released. This initial sample is performed without a `viewReady` check.
static void _actor213000InitBodyLighting(Task* actorTask)
{
    _Actor213000EricBaldwinWork* work;
    GfxCoord*                    modelCoords;
    TmdObject*                   model;

    work            = actorTask->work;
    model           = actorTask->extra.tmd;
    modelCoords     = model->coords;
    model->lightMtx = &work->light;
    model->colorMtx = &work->color;

    modelCoords[ACTOR_213000_LIGHTING_PART].composeStamp = GRAPHICS_COORD_DIRTY;
    actorRenderComposeCoord(&modelCoords[ACTOR_213000_LIGHTING_PART]);
    worldCoordSetModelLighting(model, modelCoords[ACTOR_213000_LIGHTING_PART].workm.t, 0, ARRAY_SIZE(work->light.m));
}

/// Handles `ACTOR_MESSAGE_PLAY_ANIMATION` for the body's nineteen animated parts.
///
/// Requires initialized body work and a request borrowed through this call.
/// The package has bank 0 and loaded clips 1..10; indices are unchecked and
/// stored in signed bytes. The first request must use `ANIMATION_BLEND_RESET`,
/// since blending captures existing slot state. A later nonzero blend selects
/// six normal-rate frames, ignoring `blendFrames` and `enableWorldCollision`.
/// Restarts or blends slots 1..19, ticks each immediately and enables future
/// per-frame ticking. The root slot is retained. No request pointer is kept;
/// the package's clip data and task-owned playback storage must remain live.
/// The message ID and second argument are ignored. Returns 0.
static s32 _actor213000PlayAnimation(Task* actorTask, s32 messageId, const AnimationPlayRequest* request, s32 unusedSecondArg)
{
    enum { ACTOR_213000_ANIMATION_BLEND_FRAMES = 6 };

    _Actor213000EricBaldwinWork* work;
    TmdObject*                   model;
    s32                          slotIndex;

    work  = actorTask->work;
    model = actorTask->extra.tmd;
    if (request->source.index != work->bank) {
        work->bank   = request->source.index;
        work->animId = ACTOR_MOTION_ANIMATION_INDEX_NONE;
        animationInitContext(&work->rig.anim, _gActor213000EricBaldwinAnimationBanks[work->bank], model, work->rig.poses,
                             work->rig.slots);
    }
    work->animId = request->animationId;
    if (request->blend != ANIMATION_BLEND_RESET) {
        for (slotIndex = 1; slotIndex < ARRAY_SIZE(work->rig.slots); slotIndex++) {
            animationSeekSlotWithBlend(&work->rig.anim, slotIndex, work->animId, 0, ACTOR_213000_ANIMATION_BLEND_FRAMES);
        }
    } else {
        for (slotIndex = 1; slotIndex < ARRAY_SIZE(work->rig.slots); slotIndex++) {
            animationResetSlot(&work->rig.anim, slotIndex, work->animId);
        }
    }
    // Apply a pose during dispatch, before the next body update.
    for (slotIndex = 1; slotIndex < ARRAY_SIZE(work->rig.slots); slotIndex++) {
        animationTickSlot(&work->rig.anim, slotIndex);
    }
    work->ticking = 1;
    return 0;
}

#include "../../shared/actor/actor_messages_place_euler.inc.c"

/// Handles `ACTOR_MESSAGE_SET_MODEL_DRAW` for the body and its primitive buffers.
///
/// Requires live body work/model. Modes 0/1 exclude/permit active drawing and
/// permit automatic buffer recovery; 1 also requests a missing buffer now.
/// Mode 2 excludes active drawing, suppresses recovery and schedules release
/// after two countdown ticks followed by the tick finding zero. Mode 3 permits
/// drawing and suppresses automatic recovery without allocating a buffer.
/// Other flags and any pending release in modes 0/1/3 are retained, so showing
/// the model does not cancel an earlier release. Flagged-pass selection is
/// independent. Returns 0 for modes 0..3 and 1 otherwise, even if allocation
/// fails. The message ID and second argument are ignored.
static s32 _actor213000SetModelDraw(Task* actorTask, s32 messageId, s32 mode, s32 unusedSecondArg)
{
    enum {
        ACTOR_213000_DRAW_SHOW_SKIP_AUTO_BUFFER = 3,
        ACTOR_213000_BUFFER_FREE_DELAY_TICKS    = 2,
    };

    TmdObject*                   model;
    _Actor213000EricBaldwinWork* work;
    s32                          result;

    model  = actorTask->extra.tmd;
    work   = actorTask->work;
    result = 0;

    switch (mode) {
        case ACTOR_MESSAGE_DRAW_HIDE:
            model->flags |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
            model->flags &= ~TMD_OBJECT_SKIP_AUTO_BUFFER;
            break;
        case ACTOR_MESSAGE_DRAW_SHOW:
            model->flags &= ~TMD_OBJECT_SKIP_ACTIVE_DRAW;
            tmdAllocPrimitiveBuffer(model);
            model->flags &= ~TMD_OBJECT_SKIP_AUTO_BUFFER;
            break;
        case ACTOR_MESSAGE_DRAW_HIDE_SKIP_AUTO_BUFFER:
            model->flags       |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
            work->freeCountdown = ACTOR_213000_BUFFER_FREE_DELAY_TICKS;
            model->flags       |= TMD_OBJECT_SKIP_AUTO_BUFFER;
            break;
        case ACTOR_213000_DRAW_SHOW_SKIP_AUTO_BUFFER:
            model->flags &= ~TMD_OBJECT_SKIP_ACTIVE_DRAW;
            model->flags |= TMD_OBJECT_SKIP_AUTO_BUFFER;
            break;
        default:
            result = 1;
            break;
    }
    return result;
}

/// Handles `ACTOR_COMMAND_MESSAGE_APPLY` by showing or hiding either held model.
///
/// Requires live body work and a readable command borrowed through the call.
/// Commands 0/1 permit/exclude active drawing of `heldModelTasks[0]`; 2/3 do the
/// same for `heldModelTasks[1]`. The context tag is ignored. Missing child tasks
/// and other commands change nothing. Only active-draw exclusion changes;
/// buffer policy, flagged-pass selection and the body are retained. No payload
/// pointer is kept. The message ID and second argument are ignored. Returns 0.
static s32 _actor213000ApplyHeldModelCommand(Task* actorTask, s32 messageId, const ActorCommand* request, s32 unusedSecondArg)
{
    enum {
        ACTOR_213000_COMMAND_SHOW_FIRST_HELD_MODEL  = 0,
        ACTOR_213000_COMMAND_HIDE_FIRST_HELD_MODEL  = 1,
        ACTOR_213000_COMMAND_SHOW_SECOND_HELD_MODEL = 2,
        ACTOR_213000_COMMAND_HIDE_SECOND_HELD_MODEL = 3,
    };

    _Actor213000EricBaldwinWork* work;
    Task*                        heldModelTask;
    u16                          command;

    command = request->command;
    work    = actorTask->work;

    switch (command) {
        case ACTOR_213000_COMMAND_SHOW_FIRST_HELD_MODEL:
            heldModelTask = work->heldModelTasks[0];
            if (heldModelTask != NULL) {
                heldModelTask->extra.tmd->flags &= (u16)~TMD_OBJECT_SKIP_ACTIVE_DRAW;
            }
            break;
        case ACTOR_213000_COMMAND_HIDE_FIRST_HELD_MODEL:
            heldModelTask = work->heldModelTasks[0];
            if (heldModelTask != NULL) {
                heldModelTask->extra.tmd->flags |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
            }
            break;
        case ACTOR_213000_COMMAND_SHOW_SECOND_HELD_MODEL:
            heldModelTask = work->heldModelTasks[1];
            if (heldModelTask != NULL) {
                heldModelTask->extra.tmd->flags &= (u16)~TMD_OBJECT_SKIP_ACTIVE_DRAW;
            }
            break;
        case ACTOR_213000_COMMAND_HIDE_SECOND_HELD_MODEL:
            heldModelTask = work->heldModelTasks[1];
            if (heldModelTask != NULL) {
                heldModelTask->extra.tmd->flags |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
            }
            break;
    }
    return 0;
}
