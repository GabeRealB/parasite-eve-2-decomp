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
#define ACTOR_MESSAGE_BODY_TASK _gActor110300BodyTask
#include "../../shared/actor_messages.h"
#include "../../shared/view_figure.h"

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

static TmdSource _gActor110300SwatMember2Body;
static TmdSource _gActor110300Model05E6C;
static void      _actor110300ViewFigureTask(Task* task);
static void      _actor110300AttachModelTask(Task* task);

static TmdBone _gActor110300SwatMember2BodySkeleton[20] = {
#include "assets/swat_member_2_body_skeleton.inc"
};

static u32 _gActor110300SwatMember2BodyPartVerts[20] = {
#include "assets/swat_member_2_body_partVerts.inc"
};

static SVECTOR _gActor110300SwatMember2BodyVerts[360] = {
#include "assets/swat_member_2_body_verts.inc"
};

static SVECTOR _gActor110300SwatMember2BodyNormals[358] = {
#include "assets/swat_member_2_body_normals.inc"
};

static u32 _gActor110300SwatMember2BodyStream[3973] = {
#include "assets/swat_member_2_body_stream.inc"
};

static TmdSource _gActor110300SwatMember2Body = {
    0,
    21176,
    6792,
    20,
    _gActor110300SwatMember2BodyPartVerts,
    _gActor110300SwatMember2BodyVerts,
    _gActor110300SwatMember2BodyNormals,
    _gActor110300SwatMember2BodySkeleton,
    _gActor110300SwatMember2BodyStream,
};

static TmdBone _gActor110300Model05E6CSkeleton[1] = {
#include "assets/actor_110300_model_05E6C_skeleton.inc"
};

static u32 _gActor110300Model05E6CPartVerts[1] = {
#include "assets/actor_110300_model_05E6C_partVerts.inc"
};

static SVECTOR _gActor110300Model05E6CVerts[22] = {
#include "assets/actor_110300_model_05E6C_verts.inc"
};

static SVECTOR _gActor110300Model05E6CNormals[20] = {
#include "assets/actor_110300_model_05E6C_normals.inc"
};

static u32 _gActor110300Model05E6CStream[155] = {
#include "assets/actor_110300_model_05E6C_stream.inc"
};

static TmdSource _gActor110300Model05E6C = {
    0,
    1056,
    0,
    1,
    _gActor110300Model05E6CPartVerts,
    _gActor110300Model05E6CVerts,
    _gActor110300Model05E6CNormals,
    _gActor110300Model05E6CSkeleton,
    _gActor110300Model05E6CStream,
};

static AnimationPackedPose _gActor110300Animation0639CBank1[5] = {
#include "assets/actor_110300_animation_0639C_bank1.inc"
};

static AnimationPackedRotation _gActor110300Animation0639CBank4[37] = {
#include "assets/actor_110300_animation_0639C_bank4.inc"
};

static AnimationRecord _gActor110300Animation0639CRecords[106] = {
#include "assets/actor_110300_animation_0639C_records.inc"
};

static u16 _gActor110300Animation0639CIndices[20] = {
#include "assets/actor_110300_animation_0639C_indices.inc"
};

static AnimationSet _gActor110300Animation0639C = {
    _gActor110300Animation0639CRecords,
    _gActor110300Animation0639CIndices,
    { NULL, _gActor110300Animation0639CBank1, NULL, NULL, _gActor110300Animation0639CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor110300Animation078F8Bank1[22] = {
#include "assets/actor_110300_animation_078F8_bank1.inc"
};

static AnimationPackedRotation _gActor110300Animation078F8Bank4[406] = {
#include "assets/actor_110300_animation_078F8_bank4.inc"
};

static AnimationRecord _gActor110300Animation078F8Records[875] = {
#include "assets/actor_110300_animation_078F8_records.inc"
};

static u16 _gActor110300Animation078F8Indices[20] = {
#include "assets/actor_110300_animation_078F8_indices.inc"
};

static AnimationSet _gActor110300Animation078F8 = {
    _gActor110300Animation078F8Records,
    _gActor110300Animation078F8Indices,
    { NULL, _gActor110300Animation078F8Bank1, NULL, NULL, _gActor110300Animation078F8Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor110300Animation07D58Bank1[7] = {
#include "assets/actor_110300_animation_07D58_bank1.inc"
};

static AnimationPackedRotation _gActor110300Animation07D58Bank4[96] = {
#include "assets/actor_110300_animation_07D58_bank4.inc"
};

static AnimationRecord _gActor110300Animation07D58Records[143] = {
#include "assets/actor_110300_animation_07D58_records.inc"
};

static u16 _gActor110300Animation07D58Indices[20] = {
#include "assets/actor_110300_animation_07D58_indices.inc"
};

static AnimationSet _gActor110300Animation07D58 = {
    _gActor110300Animation07D58Records,
    _gActor110300Animation07D58Indices,
    { NULL, _gActor110300Animation07D58Bank1, NULL, NULL, _gActor110300Animation07D58Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor110300Animation07FE4Bank1[4] = {
#include "assets/actor_110300_animation_07FE4_bank1.inc"
};

static AnimationPackedRotation _gActor110300Animation07FE4Bank4[39] = {
#include "assets/actor_110300_animation_07FE4_bank4.inc"
};

static AnimationRecord _gActor110300Animation07FE4Records[92] = {
#include "assets/actor_110300_animation_07FE4_records.inc"
};

static u16 _gActor110300Animation07FE4Indices[20] = {
#include "assets/actor_110300_animation_07FE4_indices.inc"
};

static AnimationSet _gActor110300Animation07FE4 = {
    _gActor110300Animation07FE4Records,
    _gActor110300Animation07FE4Indices,
    { NULL, _gActor110300Animation07FE4Bank1, NULL, NULL, _gActor110300Animation07FE4Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor110300Animation0820CBank1[3] = {
#include "assets/actor_110300_animation_0820C_bank1.inc"
};

static AnimationPackedRotation _gActor110300Animation0820CBank4[36] = {
#include "assets/actor_110300_animation_0820C_bank4.inc"
};

static AnimationRecord _gActor110300Animation0820CRecords[73] = {
#include "assets/actor_110300_animation_0820C_records.inc"
};

static u16 _gActor110300Animation0820CIndices[20] = {
#include "assets/actor_110300_animation_0820C_indices.inc"
};

static AnimationSet _gActor110300Animation0820C = {
    _gActor110300Animation0820CRecords,
    _gActor110300Animation0820CIndices,
    { NULL, _gActor110300Animation0820CBank1, NULL, NULL, _gActor110300Animation0820CBank4, NULL, NULL, NULL },
};

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
    { { { TASK_BODY_TMD, VIEW_FIGURE_TASK_PRIORITY } }, _actor110300ViewFigureTask, { .model = &_gActor110300SwatMember2Body } },
    { { { TASK_BODY_TMD, VIEW_FIGURE_TASK_PRIORITY } }, _actor110300AttachModelTask, { .model = &_gActor110300Model05E6C } },
};

/// Body animation-set bank for this carrier's camera-parented scripted figure.
///
/// Seven native pointers retain NULL slots 0 and 6; loaded clips 1..5 support
/// body slots 1..19. The context borrows this bank and its animation streams for
/// the body task lifetime. Message requests must select a loaded clip even though
/// the handler's signed upper-bound check also admits zero and negative IDs.
static AnimationSet* _gViewFigureAnimSets[7] = {
    NULL,
    &_gActor110300Animation0639C,
    &_gActor110300Animation078F8,
    &_gActor110300Animation07D58,
    &_gActor110300Animation07FE4,
    &_gActor110300Animation0820C,
    NULL,
};

/// Selects this package's public TaskDesc array for the shared spawn state.
///
/// Requires a live two-entry array with the body at VIEW_FIGURE_TASK_BODY and
/// its attached model at VIEW_FIGURE_TASK_HELPER. Only this inclusion uses it.
#define VIEW_FIGURE_TASK_DESCS gActor110300ViewFigureTasks
#include "../../shared/view_figure_spawn.inc.c"
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

#include "../../shared/view_figure_exit.inc.c"

#include "../../shared/view_figure_step_anim.inc.c"

#include "../../shared/view_figure_tick_anim.inc.c"

#include "../../shared/view_figure_reset_anim.inc.c"

#include "../../shared/view_figure_reseed_anim.inc.c"

#include "../../shared/view_figure_play_message.inc.c"

#include "../../shared/actor_messages_pair_visibility.inc.c"
