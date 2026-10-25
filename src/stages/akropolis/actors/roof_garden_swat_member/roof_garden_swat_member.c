#include "stages/akropolis/actors/roof_garden_swat_member.h"

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
#include "main/sound.h"
#include "main/task.h"
#include "main/task_types.h"
#include "main/tmd_types.h"

#include "../../rooms/roof_garden/config.h"

#include "assets/rooms/akropolis/roof_garden/swat_schema.h"

#include "../../../../shared/actor/actor_messages.h"
#include "../../../../shared/actor/view_figure.h"

static s32 _viewFigurePlayMessage(Task* unusedTask, s32 unusedMessageId, const AnimationPlayRequest* request, s32 unusedSecondArg);

static void _viewFigureStepAnim(Task* task);

/// Borrowed singleton work for this carrier's camera-parented scripted figure.
static ViewFigureWork* _gViewFigureWork;

/// Borrowed body task of this package's active scripted actor.
static Task* _gRoofGardenSwatMemberBodyTask;

/// The radio's task, attached to the body.
static Task* _gActorMsgHelperTask;

static TmdSource _gSwatMember2Body;
static TmdSource _gRoofGardenSwatMemberRadioModel;
static void      _roofGardenSwatMemberViewFigureTask(Task* task);
static void      _roofGardenSwatMemberAttachRadioTask(Task* task);

#include "assets/characters/swat_member_2/body.inc"

#include "assets/rooms/akropolis/roof_garden/swat_member_radio.inc"

#include "assets/rooms/akropolis/roof_garden/swat_member_clips.inc"

/// Animation-request and paired model-visibility messages for the view figure.
static TaskMessageEntry _gViewFigureMessages[3] = {
    { ACTOR_MESSAGE_PLAY_ANIMATION, _viewFigurePlayMessage },
    { ACTOR_MESSAGE_SET_MODEL_DRAW, actorMsgSetPairVisibility },
    { TASK_MESSAGE_TABLE_END, NULL },
};

TaskDesc gRoofGardenSwatMemberViewFigureTasks[VIEW_FIGURE_TASK_COUNT] = {
    { { { TASK_BODY_TMD, TASK_PRIORITY_CONTROL } }, _roofGardenSwatMemberViewFigureTask, { .model = &_gSwatMember2Body } },
    { { { TASK_BODY_TMD, TASK_PRIORITY_CONTROL } }, _roofGardenSwatMemberAttachRadioTask, { .model = &_gRoofGardenSwatMemberRadioModel } },
};

/// The SWAT member's radio calls by slot.
#include "assets/rooms/akropolis/roof_garden/swat_bank.inc"

static void _roofGardenSwatMemberUpdateViewFigure(Enemy* unusedEnemy, Task* task);

/// Selects the published view-figure body task for the shared fragments.
#define ACTOR_MESSAGE_BODY_TASK _gRoofGardenSwatMemberBodyTask

/// Selects this package's public TaskDesc array for the shared spawn state.
#define VIEW_FIGURE_TASK_DESCS gRoofGardenSwatMemberViewFigureTasks
#include "../../../../shared/actor/view_figure_spawn.inc.c"
#undef VIEW_FIGURE_TASK_DESCS

/// Emits an area sound once when a figure track reaches the selected cue record; every track shares one latch.
#define CUE_ANIMATION_SOUND(trackIndex, cueRecordIndex, soundScript)                                                                                             \
    {                                                                                                                                                            \
        if ((_gViewFigureWork->rig.slots[(trackIndex)].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK) == (cueRecordIndex)) {                   \
            if (_gViewFigureWork->st.cueRecord != (_gViewFigureWork->rig.slots[(trackIndex)].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK)) { \
                sndEvtRequestScriptStart((soundScript), 0, 0);                                                                                                   \
            }                                                                                                                                                    \
            _gViewFigureWork->st.cueRecord = _gViewFigureWork->rig.slots[(trackIndex)].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK;          \
        }                                                                                                                                                        \
    }

/// Advances the figure animation, emits record cues and refreshes root lighting.
static void _roofGardenSwatMemberUpdateViewFigure(Enemy* unusedEnemy, Task* task)
{
    enum {
        RIGHT_FOOT_TRACK = 16,
        LEFT_FOOT_TRACK  = 19,
        LIGHT_COUNT      = 3,
    };

    GfxCoord*  rootCoord;
    TmdObject* model;
    VECTOR     lightPosition;

    rootCoord = task->extra.tmd->coords;
    model     = task->extra.tmd;
    _viewFigureStepAnim(task);
    switch (_gViewFigureWork->st.animId) {
        case SWAT_ANIMATION_RADIO_CALL_4:
            CUE_ANIMATION_SOUND(LEFT_FOOT_TRACK, 0xC8, AKROPOLIS_ROOF_GARDEN_SOUND(AKROPOLIS_ROOF_GARDEN_SOUND_RADIO_CLICK));
            CUE_ANIMATION_SOUND(LEFT_FOOT_TRACK, 0xCA, AKROPOLIS_ROOF_GARDEN_SOUND(AKROPOLIS_ROOF_GARDEN_SOUND_SWAT_MEMBER_STEP_1));
            CUE_ANIMATION_SOUND(LEFT_FOOT_TRACK, 0xCD, AKROPOLIS_ROOF_GARDEN_SOUND(AKROPOLIS_ROOF_GARDEN_SOUND_SWAT_MEMBER_STEP_2));
            break;
        case SWAT_ANIMATION_RADIO_CALL_5:
            CUE_ANIMATION_SOUND(LEFT_FOOT_TRACK, 0x115, AKROPOLIS_ROOF_GARDEN_SOUND(AKROPOLIS_ROOF_GARDEN_SOUND_STEP_1));
            CUE_ANIMATION_SOUND(LEFT_FOOT_TRACK, 0x11F, AKROPOLIS_ROOF_GARDEN_SOUND(AKROPOLIS_ROOF_GARDEN_SOUND_STEP_1));
            CUE_ANIMATION_SOUND(RIGHT_FOOT_TRACK, 0xCE, AKROPOLIS_ROOF_GARDEN_SOUND(AKROPOLIS_ROOF_GARDEN_SOUND_STEP_2));
            CUE_ANIMATION_SOUND(RIGHT_FOOT_TRACK, 0xD8, AKROPOLIS_ROOF_GARDEN_SOUND(AKROPOLIS_ROOF_GARDEN_SOUND_STEP_2));
            break;
    }
    lightPosition.vx = rootCoord->workm.t[0];
    lightPosition.vy = rootCoord->workm.t[1];
    lightPosition.vz = rootCoord->workm.t[2];
    worldCoordSetModelLighting(model, &lightPosition, 0, LIGHT_COUNT);
}
#undef CUE_ANIMATION_SOUND

/// Dispatches the SWAT figure's spawn or per-frame update and publishes its work.
static void _roofGardenSwatMemberViewFigureTask(Task* task)
{
    EnemyTaskFunc stateHandlers[2] = {
        _viewFigureSpawnState,
        _roofGardenSwatMemberUpdateViewFigure,
    };

    _gViewFigureWork = task->work;
    stateHandlers[task->state](task->spawnArg2.pointer, task);
}

/// Attaches the radio to the right hand, 50 units along its -x.
static void _roofGardenSwatMemberAttachRadioTask(Task* task)
{
    enum {
        RIGHT_HAND_PART = 8,
        X_OFFSET        = -50
    };

    GfxCoord* bodyCoords;
    GfxCoord* radioRoot;

    bodyCoords              = _gRoofGardenSwatMemberBodyTask->extra.tmd->coords;
    radioRoot               = task->extra.tmd->coords;
    radioRoot->composeStamp = GRAPHICS_COORD_DIRTY;
    radioRoot->coord.t[0]   = X_OFFSET;
    radioRoot->parent       = &bodyCoords[RIGHT_HAND_PART];
}

#include "../../../../shared/actor/view_figure_exit.inc.c"

#include "../../../../shared/actor/view_figure.inc.c"

#include "../../../../shared/actor/actor_messages_pair_visibility.inc.c"
#undef ACTOR_MESSAGE_BODY_TASK
