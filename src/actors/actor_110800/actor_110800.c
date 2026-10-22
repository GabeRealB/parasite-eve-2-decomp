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
#include "main/sound.h"
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
static Task* _gActor110800BodyTask;

/// Borrowed helper-model task paired with this carrier's published body task.
///
/// Successful body setup publishes the handle for paired visibility and model
/// parenting. Teardown kills the helper before releasing the body; neither
/// published handle is cleared. Consumers require the current pair to be live.
static Task* _gActorMsgHelperTask;

static TmdSource _gSwatMember2Body;
static TmdSource _gActor110800Model060A0;
static void      _actor110800ViewFigureTask(Task* task);
static void      _actor110800AttachModelTask(Task* task);

#include "assets/characters/swat_member_2/body.inc"

static TmdBone _gActor110800Model060A0Skeleton[1] = {
#include "assets/actor_110800_model_060A0_skeleton.inc"
};

static u32 _gActor110800Model060A0PartVerts[1] = {
#include "assets/actor_110800_model_060A0_partVerts.inc"
};

static SVECTOR _gActor110800Model060A0Verts[14] = {
#include "assets/actor_110800_model_060A0_verts.inc"
};

static SVECTOR _gActor110800Model060A0Normals[14] = {
#include "assets/actor_110800_model_060A0_normals.inc"
};

static u32 _gActor110800Model060A0Stream[98] = {
#include "assets/actor_110800_model_060A0_stream.inc"
};

static TmdSource _gActor110800Model060A0 = {
    0,
    652,
    0,
    1,
    _gActor110800Model060A0PartVerts,
    _gActor110800Model060A0Verts,
    _gActor110800Model060A0Normals,
    _gActor110800Model060A0Skeleton,
    _gActor110800Model060A0Stream,
};

#include "assets/rooms/akropolis/roof_garden/swat_member_clips.inc"

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

TaskDesc gActor110800ViewFigureTasks[VIEW_FIGURE_TASK_COUNT] = {
    { { { TASK_BODY_TMD, VIEW_FIGURE_TASK_PRIORITY } }, _actor110800ViewFigureTask, { .model = &_gSwatMember2Body } },
    { { { TASK_BODY_TMD, VIEW_FIGURE_TASK_PRIORITY } }, _actor110800AttachModelTask, { .model = &_gActor110800Model060A0 } },
};

/// Body animation-set bank for this carrier's camera-parented scripted figure.
///
/// Seven native pointers retain NULL slots 0 and 6; loaded clips 1..5 support
/// body slots 1..19. The context borrows this bank and its animation streams for
/// the body task lifetime. Message requests must select a loaded clip even though
/// the handler's signed upper-bound check also admits zero and negative IDs.
#include "assets/rooms/akropolis/roof_garden/swat_bank.inc"

static void _actor110800UpdateViewFigure(Enemy* unusedEnemy, Task* task);

/// Selects the published view-figure body task for the shared fragments.
///
/// Expands to a writable `Task*` lvalue with no evaluation side effects. Spawn
/// publishes the body after work allocation succeeds; animation messages read
/// it and pair visibility dereferences its TMD model. Keep bound through those
/// fragment includes. Pair visibility also requires the matching live helper
/// in `_gActorMsgHelperTask`. The pointer borrows the current body; a later spawn
/// replaces it and teardown leaves it uncleared, so no read may follow teardown.
#define ACTOR_MESSAGE_BODY_TASK _gActor110800BodyTask

/// Selects this package's public TaskDesc array for the shared spawn state.
///
/// Requires a live two-entry array with the body at VIEW_FIGURE_TASK_BODY and
/// its attached model at VIEW_FIGURE_TASK_HELPER. Only this inclusion uses it.
#define VIEW_FIGURE_TASK_DESCS gActor110800ViewFigureTasks
#include "../../shared/actor/view_figure_spawn.inc.c"
#undef VIEW_FIGURE_TASK_DESCS

/// Emits an area sound once when a figure track reaches the selected cue record.
///
/// Captures the live _gViewFigureWork singleton and its shared cueRecord latch.
/// trackIndex must select a live slot in 0..19 and be side-effect-free: it occurs
/// three times. cueRecordIndex is evaluated once and matches the low ten
/// record-index bits. soundScript is evaluated only for a matching record that
/// differs from the latch; loaded sound resources are required. Matching records
/// update the latch even when already held. Other records leave it unchanged.
/// Both tracks share one latch. Invoke within a braced statement body.
#define ACTOR_110800_CUE_ANIMATION_SOUND(trackIndex, cueRecordIndex, soundScript)                                                                                \
    {                                                                                                                                                            \
        if ((_gViewFigureWork->rig.slots[(trackIndex)].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK) == (cueRecordIndex)) {                   \
            if (_gViewFigureWork->st.cueRecord != (_gViewFigureWork->rig.slots[(trackIndex)].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK)) { \
                sndEvtRequestScriptStart((soundScript), 0, 0);                                                                                                   \
            }                                                                                                                                                    \
            _gViewFigureWork->st.cueRecord = _gViewFigureWork->rig.slots[(trackIndex)].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK;          \
        }                                                                                                                                                        \
    }

/// Advances the figure animation, emits record cues and refreshes root lighting.
///
/// Requires the initialized twenty-slot singleton rig and live model. Clips 4 and
/// 5 select cue records on tracks 19 and 16. Each matching cue emits only when
/// its record differs from the single shared latch, then overwrites that latch.
/// Other clips leave the latch unchanged.
/// Lighting uses cached view-frame XYZ without composition. unusedEnemy is ignored.
static void _actor110800UpdateViewFigure(Enemy* unusedEnemy, Task* task)
{
    enum {
        ACTOR_110800_CUED_CLIP_4 = 4,
        ACTOR_110800_CUED_CLIP_5 = 5,
        ACTOR_110800_LIGHT_COUNT = 3,
    };

    GfxCoord*  rootCoord;
    TmdObject* model;
    VECTOR     lightPosition;

    rootCoord = task->extra.tmd->coords;
    model     = task->extra.tmd;
    _viewFigureStepAnim(task);
    switch (_gViewFigureWork->st.animId) {
        case ACTOR_110800_CUED_CLIP_4:
            ACTOR_110800_CUE_ANIMATION_SOUND(19, 0xC8, SOUND_AREA(GAME_STAGE_AKROPOLIS, GAME_AREA_AKROPOLIS_ROOF_GARDEN, 0x11));
            ACTOR_110800_CUE_ANIMATION_SOUND(19, 0xCA, SOUND_AREA(GAME_STAGE_AKROPOLIS, GAME_AREA_AKROPOLIS_ROOF_GARDEN, 0x0D));
            ACTOR_110800_CUE_ANIMATION_SOUND(19, 0xCD, SOUND_AREA(GAME_STAGE_AKROPOLIS, GAME_AREA_AKROPOLIS_ROOF_GARDEN, 0x0E));
            break;
        case ACTOR_110800_CUED_CLIP_5:
            ACTOR_110800_CUE_ANIMATION_SOUND(19, 0x115, SOUND_AREA(GAME_STAGE_AKROPOLIS, GAME_AREA_AKROPOLIS_ROOF_GARDEN, 0x0F));
            ACTOR_110800_CUE_ANIMATION_SOUND(19, 0x11F, SOUND_AREA(GAME_STAGE_AKROPOLIS, GAME_AREA_AKROPOLIS_ROOF_GARDEN, 0x0F));
            ACTOR_110800_CUE_ANIMATION_SOUND(16, 0xCE, SOUND_AREA(GAME_STAGE_AKROPOLIS, GAME_AREA_AKROPOLIS_ROOF_GARDEN, 0x10));
            ACTOR_110800_CUE_ANIMATION_SOUND(16, 0xD8, SOUND_AREA(GAME_STAGE_AKROPOLIS, GAME_AREA_AKROPOLIS_ROOF_GARDEN, 0x10));
            break;
    }
    lightPosition.vx = rootCoord->workm.t[0];
    lightPosition.vy = rootCoord->workm.t[1];
    lightPosition.vz = rootCoord->workm.t[2];
    worldCoordSetModelLighting(model, &lightPosition, 0, ACTOR_110800_LIGHT_COUNT);
}
#undef ACTOR_110800_CUE_ANIMATION_SOUND

/// Dispatches the SWAT figure's spawn or per-frame update and publishes its work.
///
/// Task state must be 0 (spawn) or 1 (update), with a live TMD body and enemy
/// spawn argument. Publishes the current task work before dispatch; the spawn
/// handler replaces it after allocation. The published block is borrowed until
/// actor teardown. The package's body descriptor is this entry's only consumer.
static void _actor110800ViewFigureTask(Task* task)
{
    EnemyTaskFunc stateHandlers[2] = {
        _viewFigureSpawnState,
        _actor110800UpdateViewFigure,
    };

    _gViewFigureWork = task->work;
    stateHandlers[task->state](task->spawnArg2.pointer, task);
}

/// Attaches the helper model's root to body part 8 with a -50-unit local x translation.
///
/// `task` must be the live slot-1 model task, and `_gActor110800BodyTask` must own
/// the live twenty-part body model. Each call replaces the root's local x
/// translation in parent-coordinate units, preserving its rotation and y/z
/// translation, and invalidates its composed transform. The parent coordinate
/// is borrowed until helper teardown, which precedes body teardown.
static void _actor110800AttachModelTask(Task* task)
{
    enum {
        ACTOR_110800_HELPER_PARENT_PART = 8,
        ACTOR_110800_HELPER_X_OFFSET    = -50
    };

    GfxCoord* bodyCoords;
    GfxCoord* helperRoot;

    bodyCoords               = _gActor110800BodyTask->extra.tmd->coords;
    helperRoot               = task->extra.tmd->coords;
    helperRoot->composeStamp = GRAPHICS_COORD_DIRTY;
    helperRoot->coord.t[0]   = ACTOR_110800_HELPER_X_OFFSET;
    helperRoot->parent       = &bodyCoords[ACTOR_110800_HELPER_PARENT_PART];
}

#include "../../shared/actor/view_figure_exit.inc.c"

#include "../../shared/actor/view_figure.inc.c"

#include "../../shared/actor/actor_messages_pair_visibility.inc.c"
#undef ACTOR_MESSAGE_BODY_TASK
