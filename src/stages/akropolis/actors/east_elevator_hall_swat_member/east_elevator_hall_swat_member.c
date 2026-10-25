#include "stages/akropolis/actors/east_elevator_hall_swat_member.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>

#include "common.h"

#include "actors/actor.h"

#include "gameplay/animation.h"
#include "gameplay/enemy.h"
#include "gameplay/message.h"
#include "gameplay/world_coords.h"

#include "main/coord.h"
#include "main/gfx.h"
#include "main/mem.h"
#include "main/session_types.h"
#include "main/task.h"
#include "main/task_types.h"
#include "main/tmd_types.h"
#include "../../../../shared/actor/actor_messages.h"
#include "../../../../shared/actor/view_figure.h"

static s32 _viewFigurePlayMessage(Task* unusedTask, s32 unusedMessageId, const AnimationPlayRequest* request, s32 unusedSecondArg);

static void _viewFigureStepAnim(Task* task);

/// Borrowed singleton work for this carrier's camera-parented scripted figure.
static ViewFigureWork* _gViewFigureWork;

/// Borrowed body task of this package's active scripted actor.
static Task* _gEastElevatorHallSwatMemberBodyTask;

/// The submachine gun's task, attached to the body.
static Task* _gActorMsgHelperTask;

static void _eastElevatorHallSwatMemberUpdateViewFigure(Enemy* unusedEnemy, Task* task);

static TmdSource _gSwatMember2Body;
static TmdSource _gSwatMember2SubmachineGun;
static void      _eastElevatorHallSwatMemberViewFigureTask(Task* task);
static void      _eastElevatorHallSwatMemberAttachGunTask(Task* task);

#include "assets/characters/swat_member_2/body.inc"

#include "assets/characters/swat_member_2/submachine_gun.inc"

#include "assets/rooms/akropolis/east_elevator_hall/swat_member_clips.inc"

/// Animation-request and paired model-visibility messages for the view figure.
static TaskMessageEntry _gViewFigureMessages[3] = {
    { ACTOR_MESSAGE_PLAY_ANIMATION, _viewFigurePlayMessage },
    { ACTOR_MESSAGE_SET_MODEL_DRAW, actorMsgSetPairVisibility },
    { TASK_MESSAGE_TABLE_END, NULL },
};

TaskDesc gEastElevatorHallSwatMemberViewFigureTasks[VIEW_FIGURE_TASK_COUNT] = {
    { { { TASK_BODY_TMD, TASK_PRIORITY_CONTROL } }, _eastElevatorHallSwatMemberViewFigureTask, { .model = &_gSwatMember2Body } },
    { { { TASK_BODY_TMD, TASK_PRIORITY_CONTROL } }, _eastElevatorHallSwatMemberAttachGunTask, { .model = &_gSwatMember2SubmachineGun } },
};

/// The SWAT member's clips by slot.
#include "assets/rooms/akropolis/east_elevator_hall/swat_bank.inc"

/// Selects the published view-figure body task for the shared fragments.
#define ACTOR_MESSAGE_BODY_TASK _gEastElevatorHallSwatMemberBodyTask

/// Selects this package's public TaskDesc array for the shared spawn state.
#define VIEW_FIGURE_TASK_DESCS gEastElevatorHallSwatMemberViewFigureTasks
#include "../../../../shared/actor/view_figure_spawn.inc.c"
#undef VIEW_FIGURE_TASK_DESCS

/// Dispatches spawn or animation/lighting updates for the view-parented figure.
static void _eastElevatorHallSwatMemberViewFigureTask(Task* task)
{
    EnemyTaskFunc stateHandlers[2] = {
        _viewFigureSpawnState,
        _eastElevatorHallSwatMemberUpdateViewFigure,
    };

    _gViewFigureWork = task->work;
    stateHandlers[task->state](task->spawnArg2.pointer, task);
}

/// Attaches the submachine gun to the right hand.
static void _eastElevatorHallSwatMemberAttachGunTask(Task* task)
{
    enum { RIGHT_HAND_PART = 8 };

    GfxCoord* bodyCoords;
    GfxCoord* gunRoot;

    bodyCoords            = _gEastElevatorHallSwatMemberBodyTask->extra.tmd->coords;
    gunRoot               = task->extra.tmd->coords;
    gunRoot->composeStamp = GRAPHICS_COORD_DIRTY;
    gunRoot->parent       = &bodyCoords[RIGHT_HAND_PART];
}

/// Advances the figure animation and refreshes lighting at its cached root.
static void _eastElevatorHallSwatMemberUpdateViewFigure(Enemy* unusedEnemy, Task* task)
{
    enum {
        LIGHT_COUNT = 3,
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
    worldCoordSetModelLighting(model, &lightPosition, 0, LIGHT_COUNT);
}

#include "../../../../shared/actor/view_figure_exit.inc.c"

#include "../../../../shared/actor/view_figure.inc.c"

#include "../../../../shared/actor/actor_messages_pair_visibility.inc.c"
#undef ACTOR_MESSAGE_BODY_TASK
