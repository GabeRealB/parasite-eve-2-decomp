#include "actors/task_tables.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>

#include "common.h"

#include "actors/actor.h"

#include "gameplay/animation.h"
#include "gameplay/enemy.h"
#include "gameplay/message.h"
#include "gameplay/scene_runtime.h"
#include "gameplay/world_coords.h"

#include "main/coord.h"
#include "main/gfx.h"
#include "main/mem.h"
#include "main/session_types.h"
#include "main/task.h"
#include "main/task_types.h"
#include "main/tmd_types.h"
#include "../../shared/actor/actor_messages.h"
#include "../../shared/actor/view_figure.h"

static s32 _viewFigurePlayMessage(Task* unusedTask, s32 unusedMessageId, const AnimationPlayRequest* request, s32 unusedSecondArg);

static void _viewFigureStepAnim(Task* task);

/// Borrowed singleton work for this carrier's camera-parented scripted figure.
///
/// Spawn and dispatch publish the allocation also held by Task::work. Animation
/// and message fragments require the initialized current instance; a later spawn
/// replaces it and enemy teardown frees the work without clearing this cache.
static ViewFigureWork* _gViewFigureWork;

/// Borrowed body task of this package's active scripted actor.
///
/// Published after successful spawn for animation messages, paired visibility
/// and model attachments. The body has 20 coordinates. Only the current live
/// instance may be used: a later spawn replaces the pointer, and teardown
/// leaves it dangling rather than clearing it. This cache owns no task.
static Task* _gActor110300BodyTask;

/// Borrowed helper-model task paired with this carrier's published body task.
///
/// Successful body setup publishes the handle for paired visibility and model
/// parenting. Teardown kills the helper before releasing the body; neither
/// published handle is cleared. Consumers require the current pair to be live.
static Task* _gActorMsgHelperTask;

static void _actor110300UpdateViewFigure(Enemy* unusedEnemy, Task* task);

static TmdSource _gSwatMember2Body;
static TmdSource _gSwatMember2SubmachineGun;
static void      _actor110300ViewFigureTask(Task* task);
static void      _actor110300AttachModelTask(Task* task);

#include "assets/characters/swat_member_2/body.inc"

#include "assets/characters/swat_member_2/submachine_gun.inc"

#include "assets/rooms/akropolis/east_elevator_hall/swat_member_clips.inc"

/// Animation-request and paired model-visibility messages for the view figure.
///
/// PLAY_ANIMATION borrows an AnimationPlayRequest and restarts the current body;
/// SET_MODEL_DRAW takes a mode applied to the body and attached model. The
/// end-ID/null row stops unhandled requests. The live body borrows this table.
static TaskMessageEntry _gViewFigureMessages[3] = {
    { ACTOR_MESSAGE_PLAY_ANIMATION, _viewFigurePlayMessage },
    { ACTOR_MESSAGE_SET_MODEL_DRAW, actorMsgSetPairVisibility },
    { TASK_MESSAGE_TABLE_END, NULL },
};

TaskDesc gActor110300ViewFigureTasks[VIEW_FIGURE_TASK_COUNT] = {
    { { { TASK_BODY_TMD, VIEW_FIGURE_TASK_PRIORITY } }, _actor110300ViewFigureTask, { .model = &_gSwatMember2Body } },
    { { { TASK_BODY_TMD, VIEW_FIGURE_TASK_PRIORITY } }, _actor110300AttachModelTask, { .model = &_gSwatMember2SubmachineGun } },
};

/// Body animation-set bank for this carrier's camera-parented scripted figure.
///
/// Seven native pointers retain NULL slots 0 and 6; loaded clips 1..5 support
/// body slots 1..19. The context borrows this bank and its animation streams for
/// the body task lifetime. Message requests must select a loaded clip even though
/// the handler's signed upper-bound check also admits zero and negative IDs.
#include "assets/rooms/akropolis/east_elevator_hall/swat_bank.inc"

/// Selects the published view-figure body task for the shared fragments.
///
/// Expands to a writable `Task*` lvalue with no evaluation side effects. Spawn
/// publishes the body after work allocation succeeds; animation messages read
/// it and pair visibility dereferences its TMD model. Keep bound through those
/// fragment includes. Pair visibility also requires the matching live helper
/// in `_gActorMsgHelperTask`. The pointer borrows the current body; a later spawn
/// replaces it and teardown leaves it uncleared, so no read may follow teardown.
#define ACTOR_MESSAGE_BODY_TASK _gActor110300BodyTask

/// Selects this package's public TaskDesc array for the shared spawn state.
///
/// Requires a live two-entry array with the body at VIEW_FIGURE_TASK_BODY and
/// its attached model at VIEW_FIGURE_TASK_HELPER. Only this inclusion uses it.
#define VIEW_FIGURE_TASK_DESCS gActor110300ViewFigureTasks
#include "../../shared/actor/view_figure_spawn.inc.c"
#undef VIEW_FIGURE_TASK_DESCS

/// Dispatches spawn or animation/lighting updates for the view-parented figure.
///
/// Task state must be 0 (spawn) or 1 (update), with a live model and Enemy in
/// spawnArg2. Publishes the task work for the singleton message/animation helpers;
/// spawn owns allocation and teardown of the work and attached model task.
static void _actor110300ViewFigureTask(Task* task)
{
    EnemyTaskFunc stateHandlers[2] = {
        _viewFigureSpawnState,
        _actor110300UpdateViewFigure,
    };

    _gViewFigureWork = task->work;
    stateHandlers[task->state](task->spawnArg2.pointer, task);
}

/// Attaches the helper model's root to part 8 of this figure's body model.
///
/// Both tasks must have live TMD models; the body provides twenty coordinates.
/// The helper keeps its local transform and borrows the body's part coordinate
/// until helper teardown, which precedes body teardown in `_viewFigureExit`.
static void _actor110300AttachModelTask(Task* task)
{
    enum { ACTOR_110300_HELPER_PARENT_PART = 8 };

    GfxCoord* bodyCoords;
    GfxCoord* helperRoot;

    bodyCoords               = _gActor110300BodyTask->extra.tmd->coords;
    helperRoot               = task->extra.tmd->coords;
    helperRoot->composeStamp = GRAPHICS_COORD_DIRTY;
    helperRoot->parent       = &bodyCoords[ACTOR_110300_HELPER_PARENT_PART];
}

/// Advances the figure animation and refreshes lighting at its cached root.
///
/// Requires the live twenty-part model and its initialized singleton work.
/// The root cache supplies view-frame XYZ without a new composition here.
/// All three model-light rows are updated; unusedEnemy is ignored.
static void _actor110300UpdateViewFigure(Enemy* unusedEnemy, Task* task)
{
    enum {
        ACTOR_110300_LIGHT_COUNT = 3,
    };

    TmdObject* model;
    GfxCoord*  rootCoord;
    VECTOR     lightPosition;

    model     = task->extra.tmd;
    rootCoord = model->coords;
    _viewFigureStepAnim(task);
    lightPosition.vx = rootCoord->workm.t[0];
    lightPosition.vy = rootCoord->workm.t[1];
    lightPosition.vz = rootCoord->workm.t[2];
    worldCoordSetModelLighting(model, &lightPosition, 0, ACTOR_110300_LIGHT_COUNT);
}

#include "../../shared/actor/view_figure_exit.inc.c"

#include "../../shared/actor/view_figure.inc.c"

#include "../../shared/actor/actor_messages_pair_visibility.inc.c"
#undef ACTOR_MESSAGE_BODY_TASK
