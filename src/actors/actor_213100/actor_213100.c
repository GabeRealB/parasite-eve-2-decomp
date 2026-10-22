#include "actors/task_tables.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>

#include "common.h"

#include "actors/actor.h"

#include "gameplay/actor_render.h"
#include "gameplay/animation.h"
#include "gameplay/effect_tasks.h"
#include "gameplay/enemy.h"
#include "gameplay/message.h"
#include "gameplay/room_effects.h"
#include "gameplay/scene.h"
#include "gameplay/scene_runtime.h"
#include "gameplay/world_coords.h"

#include "main/coord.h"
#include "main/gfx.h"
#include "main/mem.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/task.h"
#include "main/tmd.h"
#include "main/tmd_types.h"
#include "../../shared/actor/actor_motion.h"
#include "../../shared/actor/actor_messages.h"

/// Work block of Jodie Bouquet as a room script poses her: what her body model
/// plays, the matrices it is lit with and the model she holds.
///
/// The spawn state allocates it zeroed and keeps it at `Task::work` for the
/// task's life. It opens with the nineteen-part rig and the model state, the
/// head a play request views as `ActorMotion19PlayWork`; she does not walk, so
/// the package's own state follows directly and `model.nextAnimId` is unused.
/// The model object borrows `model.light` and `model.color` for as long as the
/// block lives.
typedef struct {
    ActorAnimRig19  rig;           // Playback storage of the nineteen-part body model; slots 1 to 18 are driven
    ActorModelState model;         // Clip and bank the rig plays, and the matrices the model is lit with
    Task*           heldModelTask; // Task drawing the model hung on body part 8, a hand; it is shown and hidden with the body. Never `NULL` past the spawn state, which exits when the spawn fails
    s32             freeCountdown; // Ticks left before the body model's buffers are freed, which the tick finding 0 does (-1 no free pending)
} _Actor213100JodieBouquetWork;
STATIC_ASSERT_SIZEOF(_Actor213100JodieBouquetWork, 0x488);

static void _modelPlacementAttachPartTask(Task* childTask);
static void _actor213100UpdateBody(Task* actorTask);
static void _actor213100HeldModelIdle(Task* unusedTask);
static void _actor213100InitBody(Task* actorTask);
static void _actor213100ExitBody(Task* actorTask);
static void _actor213100InitBodyLighting(Task* actorTask);

static TmdSource _gJodieBody;
static TmdSource _gMistR18Laptop;
static s32       _actor213100SetModelDraw(Task* actorTask, s32 messageId, s32 mode, s32 unusedSecondArg);
static void      _actor213100HeldModelTask(Task* heldModelTask);
static void      _actor213100BodyTask(Task* actorTask);

#include "assets/characters/jodie/body.inc"

#include "assets/rooms/mist/r18/laptop.inc"

#include "assets/rooms/mist/parking/jodie_clips.inc"

/// Jodie Bouquet's body animation sets for bank 0: clip 0 absent, clips 1..9 loaded.
///
/// `AnimationPlayRequest::animationId` selects an entry after `source.index`
/// selects bank 0. Animation contexts borrow this table and its sets, so the
/// package must remain loaded for their playback lifetime.
#include "assets/rooms/mist/parking/jodie_bank.inc"

/// Carrier-local banks for nineteen-part playback: only bank 0, clips 1..9.
///
/// `AnimationPlayRequest::source.index` selects the bank; `animationId` selects
/// an entry of its clip table. Playback borrows that table and loaded clip data.
static AnimationSet** const _gActorMotionAnimBanks19[1] SECTION(".data") = {
    _gActor213100BodyAnimSets,
};

enum { ACTOR_213100_JODIE_TASK_PRIORITY = 192 };

TaskDesc gActor213100JodieBouquetTaskDescriptors[2] = {
    // 0: body.
    {
        .header   = { .fields = { .flags = TASK_BODY_TMD, .priority = ACTOR_213100_JODIE_TASK_PRIORITY } },
        .callback = _actor213100BodyTask,
        .data     = { .model = &_gJodieBody },
    },
    // 1: held model.
    {
        .header   = { .fields = { .flags = TASK_BODY_TMD, .priority = ACTOR_213100_JODIE_TASK_PRIORITY } },
        .callback = _actor213100HeldModelTask,
        .data     = { .model = &_gMistR18Laptop },
    },
};

/// Jodie Bouquet's body messages for animation, placement and paired model drawing.
///
/// Payloads are a borrowed play request, borrowed transform and integer draw
/// mode respectively. Drawing applies to the body and held model together.
/// Three handlers precede the end marker. The body borrows this table and its
/// loaded callbacks while it can receive messages.
static const TaskMessageEntry _gActor213100JodieBouquetBodyMessages[4] SECTION(".data") = {
    { ACTOR_MESSAGE_PLAY_ANIMATION, _actorMotionPlayAnim19 },
    { ACTOR_MESSAGE_PLACE, actorMsgPlaceEuler },
    { ACTOR_MESSAGE_SET_MODEL_DRAW, _actor213100SetModelDraw },
    { TASK_MESSAGE_TABLE_END, NULL },
};

/// Jodie's body and held-model visibility indexed by the one-based logical room view.
///
/// Zero hides both models and one shows them; only view 4 is visible. M.I.S.T.
/// parking supplies active views 1..20. The complete 24-byte table is retained;
/// its tail does not establish additional supported views.
static s8 _gActor213100JodieVisibilityByView[24] = {
    0,
    0,
    0,
    0,
    1,
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

/// Advances the body's eighteen child animation slots, leaving the root unchanged.
///
/// Requires an initialized nineteen-part rig and its borrowed animation bank.
static inline void _actor213100TickBodyAnimation(ActorAnimRig19* rig)
{
    enum { ACTOR_213100_FIRST_ANIMATED_PART = 1 };
    s32 slotIndex;

    for (slotIndex = ACTOR_213100_FIRST_ANIMATED_PART; slotIndex < ARRAY_SIZE(rig->slots); slotIndex++) {
        animationTickSlot(&rig->anim, slotIndex);
    }
}

/// Updates Jodie's animation, ground shadow, lighting and paired model visibility.
///
/// Requires initialized body work, live body and held-model tasks, and a valid
/// room view index into `_gActor213100JodieVisibilityByView` (1..20 in M.I.S.T. parking).
/// The shadow uses body part 1's cached world position before the ready-view
/// lighting/visibility update. Its half-side is 768 world-coordinate units.
/// A pending buffer release advances even when hidden or the view is not ready;
/// the update observing zero frees only the body's primitive buffers.
static void _actor213100UpdateBody(Task* actorTask)
{
    enum {
        ACTOR_213100_LIGHTING_PART           = 1,
        ACTOR_213100_GROUND_SHADOW_HALF_SIZE = 0x300,
        ACTOR_213100_LIGHT_COUNT             = 3,
    };

    _Actor213100JodieBouquetWork* work;
    TmdObject*                    model;
    TmdObject*                    heldModel;
    VECTOR3                       groundPosition;

    work  = actorTask->work;
    model = actorTask->extra.tmd;
    if (work->model.ticking != 0) {
        _actor213100TickBodyAnimation(&work->rig);
    }
    // Sample the cached body transform before refreshing it for this view.
    if (!(model->flags & TMD_OBJECT_SKIP_ACTIVE_DRAW)) {
        if (worldCollisionProjectGroundPoint(GRAPHICS_MATRIX_TRANSLATION(&actorTask->extra.tmd->coords[ACTOR_213100_LIGHTING_PART].workm), &groundPosition) != 0) {
            effectDrawGroundShadow(&groundPosition, ACTOR_213100_GROUND_SHADOW_HALF_SIZE, gRoomEffectState->groundShadowShade);
        }
    }
    if (gGameSession->viewReady != 0) {
        actorTask->extra.tmd->coords[ACTOR_213100_LIGHTING_PART].composeStamp = GRAPHICS_COORD_DIRTY;
        actorRenderComposeCoord(&actorTask->extra.tmd->coords[ACTOR_213100_LIGHTING_PART]);
        worldCoordSetModelLighting(model, actorTask->extra.tmd->coords[ACTOR_213100_LIGHTING_PART].workm.t, 0, ACTOR_213100_LIGHT_COUNT);
        heldModel = work->heldModelTask->extra.tmd;
        if (_gActor213100JodieVisibilityByView[gGameSession->location.view] != 0) {
            model->flags     &= ~TMD_OBJECT_SKIP_ACTIVE_DRAW;
            heldModel->flags &= ~TMD_OBJECT_SKIP_ACTIVE_DRAW;
        } else {
            model->flags     |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
            heldModel->flags |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
        }
    }
    // Zero is acted on before decrementing, then -1 disables further release.
    if (work->freeCountdown >= 0) {
        if (work->freeCountdown == 0) {
            tmdFreePrimitiveBuffer(model);
        }
        work->freeCountdown--;
    }
}

/// Task-state callbacks for Jodie Bouquet's held model.
///
/// `Task::state` selects 0 attach to the body part, 1 idle or 2 kill the child.
/// The body supplies borrowed coordinates and lighting until teardown.
static const TaskFuncTable3 _gActor213100HeldModelStates = {
    {
        _modelPlacementAttachPartTask,
        _actor213100HeldModelIdle,
        taskKill,
    },
};

/// Dispatches the held model's attachment, idle and teardown states.
///
/// Requires a live TMD task with state 0..2. State 0 borrows the parent TMD
/// task from `spawnArg2.pointer` and its part index from `spawnArg1.value`;
/// the coordinate and lighting matrices remain borrowed through teardown.
static void _actor213100HeldModelTask(Task* heldModelTask)
{
    TaskFuncTable3 stateCallbacks;

    stateCallbacks = _gActor213100HeldModelStates;
    stateCallbacks.funcs[heldModelTask->state](heldModelTask);
}

#include "../../shared/actor/model_placement_attach_part.inc.c"

/// Leaves the attached held model under its parent's transform and lighting.
///
/// The task argument is ignored; attachment and resource lifetime are unchanged.
static void _actor213100HeldModelIdle(Task* unusedTask)
{
}

/// Jodie Bouquet's body lifecycle: 0 initialize, 1 update, 2 release the actor.
///
/// Task state indexes a complete twelve-byte value copy, without a terminator.
/// Initialization owns body work and the held-model task; keep this package
/// and the live Enemy spawn payload available until teardown.
static const TaskFuncTable3 _gActor213100JodieBouquetBodyStates = {
    {
        _actor213100InitBody,
        _actor213100UpdateBody,
        _actor213100ExitBody,
    },
};

/// Dispatches Jodie's body initialization, frame update and teardown states.
///
/// Requires a live TMD task with state 0..2 and a live owned `Enemy` in
/// `spawnArg2.pointer`. Initialization failure tears down that enemy and task;
/// frame updates require successfully initialized body work and held model.
static void _actor213100BodyTask(Task* actorTask)
{
    TaskFuncTable3 stateCallbacks;

    stateCallbacks = _gActor213100JodieBouquetBodyStates;
    stateCallbacks.funcs[actorTask->state](actorTask);
}

/// Initializes Jodie's body playback and spawns the model held on part 8.
///
/// Requires the fresh state-0 nineteen-part TMD task and its owned `Enemy` in
/// `spawnArg2.pointer`. Allocates primary-heap work owned by the task, lends
/// its lighting matrices to the body and starts bank 0, clip 5 without blending.
/// Both models begin hidden; the held task attaches on its first update.
/// Allocation failure exits the actor. Success installs its message/exit
/// callbacks and advances to the frame-update state.
static void _actor213100InitBody(Task* actorTask)
{
    enum {
        ACTOR_213100_BUFFER_FREE_NONE       = -1,
        ACTOR_213100_HELD_MODEL_DESCRIPTOR  = 1,
        ACTOR_213100_HELD_MODEL_PART        = 8,
        ACTOR_213100_INITIAL_ANIMATION_BANK = 0,
        ACTOR_213100_INITIAL_ANIMATION_ID   = 5,
    };

    _Actor213100JodieBouquetWork* work;
    AnimationPlayRequest          initialAnimation;
    TmdObject*                    model;
    TmdObject*                    heldModel;
    Task*                         heldModelTask;

    work = memCalloc(sizeof(*work), false);
    if (work == NULL) {
        enemyTaskExit(actorTask);
        return;
    }
    actorTask->work     = work;
    work->model.animId  = ACTOR_MOTION_ANIMATION_INDEX_NONE;
    work->model.bank    = ACTOR_MOTION_ANIMATION_INDEX_NONE;
    work->freeCountdown = ACTOR_213100_BUFFER_FREE_NONE;
    heldModelTask       = taskSpawnFromTable(gActor213100JodieBouquetTaskDescriptors, ACTOR_213100_HELD_MODEL_DESCRIPTOR, ACTOR_213100_HELD_MODEL_PART, actorTask);
    work->heldModelTask = heldModelTask;
    if (heldModelTask == NULL) {
        enemyTaskExit(actorTask);
        return;
    }
    // The held task will borrow these matrices when it attaches to the body.
    _actor213100InitBodyLighting(actorTask);
    model                                 = actorTask->extra.tmd;
    model->flags                         |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
    heldModel                             = work->heldModelTask->extra.tmd;
    heldModel->flags                     |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
    initialAnimation.source.index         = ACTOR_213100_INITIAL_ANIMATION_BANK;
    initialAnimation.animationId          = ACTOR_213100_INITIAL_ANIMATION_ID;
    initialAnimation.blend                = ANIMATION_BLEND_RESET;
    initialAnimation.blendFrames          = 0;
    initialAnimation.enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE;
    _actorMotionPlayAnim19(actorTask, 0, &initialAnimation, 0);
    actorTask->msgTable     = _gActor213100JodieBouquetBodyMessages;
    actorTask->exitCallback = _actor213100ExitBody;
    actorTask->state++;
}

/// Releases the actor's enemy object and tears down its body and attached model.
///
/// Used for state 2 and as the exit callback. Requires the live owned `Enemy`
/// in `spawnArg2.pointer`; task teardown releases body work and dispatches
/// attached children before releasing model resources. Do not use the work
/// or enemy after this call.
static void _actor213100ExitBody(Task* actorTask)
{
    enemyTaskExit(actorTask);
}

/// Lends the body's work-owned light-direction and light-colour matrices to its model.
///
/// Requires live body work and a TMD model. Replaces both matrix pointers
/// without initializing their values; the work must outlive every model use.
static void _actor213100InitBodyLighting(Task* actorTask)
{
    TmdObject*                    model;
    _Actor213100JodieBouquetWork* work;

    model           = actorTask->extra.tmd;
    work            = actorTask->work;
    model->lightMtx = &work->model.light;
    model->colorMtx = &work->model.color;
}

#include "../../shared/actor/actor_motion_play19.inc.c"

#include "../../shared/actor/actor_messages_place_euler.inc.c"

/// Sets body drawing and buffer policy, then mirrors every flag to the held model.
///
/// Handles `ACTOR_MESSAGE_SET_MODEL_DRAW` after successful initialization.
/// Modes: 0 hides and permits automatic buffer recovery; 1 shows, attempts
/// body-buffer allocation and permits recovery; 2 hides, suppresses recovery
/// and schedules body-buffer release; 3 shows and suppresses recovery without
/// allocation. Mode 2 frees on the third body update, after two decrements.
/// Other modes retain body flags but still copy them to the held model.
///
/// Modes 0/1/3 retain a pending release. A ready-view body update may replace
/// either model's active-draw flag with the room's visibility policy. Returns
/// 0 for modes 0..3, including allocation failure, and 1 otherwise.
/// The message ID and second payload are ignored; no payload is retained.
static s32 _actor213100SetModelDraw(Task* actorTask, s32 messageId, s32 mode, s32 unusedSecondArg)
{
    enum {
        ACTOR_213100_DRAW_SHOW_SKIP_AUTO_BUFFER = 3,
        ACTOR_213100_BUFFER_FREE_DELAY_TICKS    = 2,
    };

    TmdObject*                    model;
    TmdObject*                    heldModel;
    _Actor213100JodieBouquetWork* work;
    s32                           result;

    model     = actorTask->extra.tmd;
    work      = actorTask->work;
    heldModel = work->heldModelTask->extra.tmd;
    result    = 0;
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
            work->freeCountdown = ACTOR_213100_BUFFER_FREE_DELAY_TICKS;
            model->flags       |= TMD_OBJECT_SKIP_AUTO_BUFFER;
            break;
        case ACTOR_213100_DRAW_SHOW_SKIP_AUTO_BUFFER:
            model->flags &= ~TMD_OBJECT_SKIP_ACTIVE_DRAW;
            model->flags |= TMD_OBJECT_SKIP_AUTO_BUFFER;
            break;
        default:
            result = 1;
            break;
    }
    heldModel->flags = model->flags;
    return result;
}
