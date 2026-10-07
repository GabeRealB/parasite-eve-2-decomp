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
#include "main/task_types.h"
#include "main/tmd.h"
#include "main/tmd_types.h"
#include "../../shared/model_placement.h"
#include "../../shared/actor_messages.h"

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
    s8             animId;            // Clip the slots were last seeded with, within `bank` (`ACTOR_MODEL_STATE_NONE` before the first request)
    s8             bank;              // Index, in the package's animation bank table, of the bank the rig is bound to (`ACTOR_MODEL_STATE_NONE` before the first request)
    s8             freeCountdown;     // Ticks left before the body model's buffers are freed, which the tick finding 0 does (-1 no free pending)
    s32            field_478;         // Cleared by the spawn state and never read; role unproven
    MATRIX         light;             // Light-direction matrix lent to the model object
    MATRIX         color;             // Light-colour matrix lent to the model object
    Task*          heldModelTasks[2]; // Tasks drawing the two models hung on body part 8, a hand; an actor command shows or hides each. `NULL` where the spawn failed
} _Actor213000EricBaldwinWork;
STATIC_ASSERT_SIZEOF(_Actor213000EricBaldwinWork, 0x4C4);

/// The actor's spawn table: entry 0 is the actor itself, entries 1 to 4 the
/// children its spawn handler creates.
extern TaskDesc D_actor_213000_80157DE0[];

/// The actor's message table: `(message id, handler)` pairs for 0x7D3 / 0x7D4 /
/// 0x7D5 / 0x7DB, ended by `TASK_MESSAGE_TABLE_END`. The spawn handler parks its address in
/// `Task::msgTable`.
// Handler views preserve the signatures used by this TU. The dispatcher
// transports each argument in a word register.

extern TaskMessageEntry D_actor_213000_80157E1C[];

/// Animation bank table the 0x7D3 handler indexes with the preset's `field_0`.
extern AnimationSet*  D_actor_213000_80157DB0[11];
extern AnimationSet** D_actor_213000_80157DDC[1];

static void _modelPlacementAttachPartTask(Task* childTask);
static void _modelPlacementMirrorParentDrawFlags(Task* childTask);
static void _modelPlacementMirrorParentDrawFlagsTask(Task* childTask);
static void _actor213000HeldModelIdle(Task* unusedTask);
static void _actor213000AttachThreeRootModel(Task* childTask);
static void _actor213000UpdateBody(Task* actorTask);
static void _actor213000InitBodyLighting(Task* actorTask);

static TmdSource _gActor213000EricBaldwinBody;
static TmdSource _gActor213000Model06A10;
static TmdSource _gActor213000EricBaldwinHandRight;
static TmdSource _gActor213000Model072AC;
static TmdSource _gActor213000Prop;
static s32       _actor213000PlayAnimation(Task* actorTask, s32 messageId, const AnimationPlayRequest* request, s32 unusedSecondArg);
static s32       _actor213000SetModelDraw(Task* actorTask, s32 messageId, s32 mode, s32 unusedSecondArg);
static s32       _actor213000ApplyHeldModelCommand(Task* actorTask, s32 messageId, const ActorCommand* request, s32 unusedSecondArg);
static void      _actor213000HeldModelTask(Task* heldModelTask);
static void      _actor213000RightHandTask(Task* handTask);
static void      _actor213000ThreeRootModelTask(Task* childTask);
static void      _actor213000BodyTask(Task* actorTask);

/// Body part whose world translation supplies the lighting sample.
enum { ACTOR_213000_LIGHTING_PART = 1 };

static TmdBone _gActor213000EricBaldwinBodySkeleton[20] = {
#include "assets/eric_baldwin_body_skeleton.inc"
};

static u32 _gActor213000EricBaldwinBodyPartVerts[20] = {
#include "assets/eric_baldwin_body_partVerts.inc"
};

static SVECTOR _gActor213000EricBaldwinBodyVerts[338] = {
#include "assets/eric_baldwin_body_verts.inc"
};

static SVECTOR _gActor213000EricBaldwinBodyNormals[413] = {
#include "assets/eric_baldwin_body_normals.inc"
};

static u32 _gActor213000EricBaldwinBodyStream[4155] = {
#include "assets/eric_baldwin_body_stream.inc"
};

static TmdSource _gActor213000EricBaldwinBody = {
    0,
    20860,
    8300,
    20,
    _gActor213000EricBaldwinBodyPartVerts,
    _gActor213000EricBaldwinBodyVerts,
    _gActor213000EricBaldwinBodyNormals,
    _gActor213000EricBaldwinBodySkeleton,
    _gActor213000EricBaldwinBodyStream,
};

static TmdBone _gActor213000Model06A10Skeleton[3] = {
#include "assets/actor_213000_model_06A10_skeleton.inc"
};

static u32 _gActor213000Model06A10PartVerts[3] = {
#include "assets/actor_213000_model_06A10_partVerts.inc"
};

static SVECTOR _gActor213000Model06A10Verts[28] = {
#include "assets/actor_213000_model_06A10_verts.inc"
};

static SVECTOR _gActor213000Model06A10Normals[28] = {
#include "assets/actor_213000_model_06A10_normals.inc"
};

static u32 _gActor213000Model06A10Stream[246] = {
#include "assets/actor_213000_model_06A10_stream.inc"
};

static TmdSource _gActor213000Model06A10 = {
    0,
    1312,
    312,
    3,
    _gActor213000Model06A10PartVerts,
    _gActor213000Model06A10Verts,
    _gActor213000Model06A10Normals,
    _gActor213000Model06A10Skeleton,
    _gActor213000Model06A10Stream,
};

static TmdBone _gActor213000EricBaldwinHandRightSkeleton[1] = {
#include "assets/eric_baldwin_hand_right_skeleton.inc"
};

static u32 _gActor213000EricBaldwinHandRightPartVerts[1] = {
#include "assets/eric_baldwin_hand_right_partVerts.inc"
};

static SVECTOR _gActor213000EricBaldwinHandRightVerts[19] = {
#include "assets/eric_baldwin_hand_right_verts.inc"
};

static SVECTOR _gActor213000EricBaldwinHandRightNormals[19] = {
#include "assets/eric_baldwin_hand_right_normals.inc"
};

static u32 _gActor213000EricBaldwinHandRightStream[130] = {
#include "assets/eric_baldwin_hand_right_stream.inc"
};

static TmdSource _gActor213000EricBaldwinHandRight = {
    0,
    876,
    0,
    1,
    _gActor213000EricBaldwinHandRightPartVerts,
    _gActor213000EricBaldwinHandRightVerts,
    _gActor213000EricBaldwinHandRightNormals,
    _gActor213000EricBaldwinHandRightSkeleton,
    _gActor213000EricBaldwinHandRightStream,
};

static TmdBone _gActor213000Model072ACSkeleton[1] = {
#include "assets/actor_213000_model_072AC_skeleton.inc"
};

static u32 _gActor213000Model072ACPartVerts[1] = {
#include "assets/actor_213000_model_072AC_partVerts.inc"
};

static SVECTOR _gActor213000Model072ACVerts[14] = {
#include "assets/actor_213000_model_072AC_verts.inc"
};

static SVECTOR _gActor213000Model072ACNormals[17] = {
#include "assets/actor_213000_model_072AC_normals.inc"
};

static u32 _gActor213000Model072ACStream[98] = {
#include "assets/actor_213000_model_072AC_stream.inc"
};

static TmdSource _gActor213000Model072AC = {
    0,
    652,
    0,
    1,
    _gActor213000Model072ACPartVerts,
    _gActor213000Model072ACVerts,
    _gActor213000Model072ACNormals,
    _gActor213000Model072ACSkeleton,
    _gActor213000Model072ACStream,
};

static TmdBone _gActor213000PropSkeleton[1] = {
#include "assets/actor_213000_prop_skeleton.inc"
};

static u32 _gActor213000PropPartVerts[1] = {
#include "assets/actor_213000_prop_partVerts.inc"
};

static SVECTOR _gActor213000PropVerts[14] = {
#include "assets/actor_213000_prop_verts.inc"
};

static u32 _gActor213000PropStream[79] = {
#include "assets/actor_213000_prop_stream.inc"
};

static TmdSource _gActor213000Prop = {
    0,
    528,
    0,
    1,
    _gActor213000PropPartVerts,
    _gActor213000PropVerts,
    &_gActor213000PropVerts[14],
    _gActor213000PropSkeleton,
    _gActor213000PropStream,
};

static AnimationPackedPose _gActor213000Animation08080Bank1[11] = {
#include "assets/actor_213000_animation_08080_bank1.inc"
};

static AnimationPackedRotation _gActor213000Animation08080Bank4[277] = {
#include "assets/actor_213000_animation_08080_bank4.inc"
};

static AnimationRecord _gActor213000Animation08080Records[332] = {
#include "assets/actor_213000_animation_08080_records.inc"
};

static u16 _gActor213000Animation08080Indices[20] = {
#include "assets/actor_213000_animation_08080_indices.inc"
};

static AnimationSet _gActor213000Animation08080 = {
    _gActor213000Animation08080Records,
    _gActor213000Animation08080Indices,
    { NULL, _gActor213000Animation08080Bank1, NULL, NULL, _gActor213000Animation08080Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor213000Animation0894CBank1[2] = {
#include "assets/actor_213000_animation_0894C_bank1.inc"
};

static AnimationPackedRotation _gActor213000Animation0894CBank4[242] = {
#include "assets/actor_213000_animation_0894C_bank4.inc"
};

static AnimationRecord _gActor213000Animation0894CRecords[295] = {
#include "assets/actor_213000_animation_0894C_records.inc"
};

static u16 _gActor213000Animation0894CIndices[20] = {
#include "assets/actor_213000_animation_0894C_indices.inc"
};

static AnimationSet _gActor213000Animation0894C = {
    _gActor213000Animation0894CRecords,
    _gActor213000Animation0894CIndices,
    { NULL, _gActor213000Animation0894CBank1, NULL, NULL, _gActor213000Animation0894CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor213000Animation09DDCBank1[2] = {
#include "assets/actor_213000_animation_09DDC_bank1.inc"
};

static AnimationPackedRotation _gActor213000Animation09DDCBank4[570] = {
#include "assets/actor_213000_animation_09DDC_bank4.inc"
};

static AnimationRecord _gActor213000Animation09DDCRecords[720] = {
#include "assets/actor_213000_animation_09DDC_records.inc"
};

static u16 _gActor213000Animation09DDCIndices[20] = {
#include "assets/actor_213000_animation_09DDC_indices.inc"
};

static AnimationSet _gActor213000Animation09DDC = {
    _gActor213000Animation09DDCRecords,
    _gActor213000Animation09DDCIndices,
    { NULL, _gActor213000Animation09DDCBank1, NULL, NULL, _gActor213000Animation09DDCBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor213000Animation0B0DCBank1[2] = {
#include "assets/actor_213000_animation_0B0DC_bank1.inc"
};

static AnimationPackedRotation _gActor213000Animation0B0DCBank4[555] = {
#include "assets/actor_213000_animation_0B0DC_bank4.inc"
};

static AnimationRecord _gActor213000Animation0B0DCRecords[635] = {
#include "assets/actor_213000_animation_0B0DC_records.inc"
};

static u16 _gActor213000Animation0B0DCIndices[20] = {
#include "assets/actor_213000_animation_0B0DC_indices.inc"
};

static AnimationSet _gActor213000Animation0B0DC = {
    _gActor213000Animation0B0DCRecords,
    _gActor213000Animation0B0DCIndices,
    { NULL, _gActor213000Animation0B0DCBank1, NULL, NULL, _gActor213000Animation0B0DCBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor213000Animation0BA6CBank1[2] = {
#include "assets/actor_213000_animation_0BA6C_bank1.inc"
};

static AnimationPackedRotation _gActor213000Animation0BA6CBank4[255] = {
#include "assets/actor_213000_animation_0BA6C_bank4.inc"
};

static AnimationRecord _gActor213000Animation0BA6CRecords[331] = {
#include "assets/actor_213000_animation_0BA6C_records.inc"
};

static u16 _gActor213000Animation0BA6CIndices[20] = {
#include "assets/actor_213000_animation_0BA6C_indices.inc"
};

static AnimationSet _gActor213000Animation0BA6C = {
    _gActor213000Animation0BA6CRecords,
    _gActor213000Animation0BA6CIndices,
    { NULL, _gActor213000Animation0BA6CBank1, NULL, NULL, _gActor213000Animation0BA6CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor213000Animation0C7C8Bank1[2] = {
#include "assets/actor_213000_animation_0C7C8_bank1.inc"
};

static AnimationPackedRotation _gActor213000Animation0C7C8Bank4[383] = {
#include "assets/actor_213000_animation_0C7C8_bank4.inc"
};

static AnimationRecord _gActor213000Animation0C7C8Records[446] = {
#include "assets/actor_213000_animation_0C7C8_records.inc"
};

static u16 _gActor213000Animation0C7C8Indices[20] = {
#include "assets/actor_213000_animation_0C7C8_indices.inc"
};

static AnimationSet _gActor213000Animation0C7C8 = {
    _gActor213000Animation0C7C8Records,
    _gActor213000Animation0C7C8Indices,
    { NULL, _gActor213000Animation0C7C8Bank1, NULL, NULL, _gActor213000Animation0C7C8Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor213000Animation0CBE8Bank1[3] = {
#include "assets/actor_213000_animation_0CBE8_bank1.inc"
};

static AnimationPackedRotation _gActor213000Animation0CBE8Bank4[96] = {
#include "assets/actor_213000_animation_0CBE8_bank4.inc"
};

static AnimationRecord _gActor213000Animation0CBE8Records[139] = {
#include "assets/actor_213000_animation_0CBE8_records.inc"
};

static u16 _gActor213000Animation0CBE8Indices[20] = {
#include "assets/actor_213000_animation_0CBE8_indices.inc"
};

static AnimationSet _gActor213000Animation0CBE8 = {
    _gActor213000Animation0CBE8Records,
    _gActor213000Animation0CBE8Indices,
    { NULL, _gActor213000Animation0CBE8Bank1, NULL, NULL, _gActor213000Animation0CBE8Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor213000Animation0D3A4Bank1[3] = {
#include "assets/actor_213000_animation_0D3A4_bank1.inc"
};

static AnimationPackedRotation _gActor213000Animation0D3A4Bank4[213] = {
#include "assets/actor_213000_animation_0D3A4_bank4.inc"
};

static AnimationRecord _gActor213000Animation0D3A4Records[253] = {
#include "assets/actor_213000_animation_0D3A4_records.inc"
};

static u16 _gActor213000Animation0D3A4Indices[20] = {
#include "assets/actor_213000_animation_0D3A4_indices.inc"
};

static AnimationSet _gActor213000Animation0D3A4 = {
    _gActor213000Animation0D3A4Records,
    _gActor213000Animation0D3A4Indices,
    { NULL, _gActor213000Animation0D3A4Bank1, NULL, NULL, _gActor213000Animation0D3A4Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor213000Animation0D724Bank1[2] = {
#include "assets/actor_213000_animation_0D724_bank1.inc"
};

static AnimationPackedRotation _gActor213000Animation0D724Bank4[79] = {
#include "assets/actor_213000_animation_0D724_bank4.inc"
};

static AnimationRecord _gActor213000Animation0D724Records[119] = {
#include "assets/actor_213000_animation_0D724_records.inc"
};

static u16 _gActor213000Animation0D724Indices[20] = {
#include "assets/actor_213000_animation_0D724_indices.inc"
};

static AnimationSet _gActor213000Animation0D724 = {
    _gActor213000Animation0D724Records,
    _gActor213000Animation0D724Indices,
    { NULL, _gActor213000Animation0D724Bank1, NULL, NULL, _gActor213000Animation0D724Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor213000Animation0DF68Bank1[2] = {
#include "assets/actor_213000_animation_0DF68_bank1.inc"
};

static AnimationPackedRotation _gActor213000Animation0DF68Bank4[222] = {
#include "assets/actor_213000_animation_0DF68_bank4.inc"
};

static AnimationRecord _gActor213000Animation0DF68Records[281] = {
#include "assets/actor_213000_animation_0DF68_records.inc"
};

static u16 _gActor213000Animation0DF68Indices[20] = {
#include "assets/actor_213000_animation_0DF68_indices.inc"
};

static AnimationSet _gActor213000Animation0DF68 = {
    _gActor213000Animation0DF68Records,
    _gActor213000Animation0DF68Indices,
    { NULL, _gActor213000Animation0DF68Bank1, NULL, NULL, _gActor213000Animation0DF68Bank4, NULL, NULL, NULL },
};

AnimationSet* D_actor_213000_80157DB0[11] = {
    NULL,
    &_gActor213000Animation08080,
    &_gActor213000Animation0894C,
    &_gActor213000Animation09DDC,
    &_gActor213000Animation0B0DC,
    &_gActor213000Animation0BA6C,
    &_gActor213000Animation0C7C8,
    &_gActor213000Animation0CBE8,
    &_gActor213000Animation0D3A4,
    &_gActor213000Animation0D724,
    &_gActor213000Animation0DF68,
};

AnimationSet** D_actor_213000_80157DDC[1] = {
    D_actor_213000_80157DB0,
};

TaskDesc D_actor_213000_80157DE0[5] = {
    { { { TASK_BODY_TMD, 192 } }, _actor213000BodyTask, { .model = &_gActor213000EricBaldwinBody } },
    { { { TASK_BODY_TMD, 192 } }, _actor213000HeldModelTask, { .model = &_gActor213000Model072AC } },
    { { { TASK_BODY_TMD, 192 } }, _actor213000HeldModelTask, { .model = &_gActor213000Prop } },
    { { { TASK_BODY_TMD, 192 } }, _actor213000ThreeRootModelTask, { .model = &_gActor213000Model06A10 } },
    { { { TASK_BODY_TMD, 192 } }, _actor213000RightHandTask, { .model = &_gActor213000EricBaldwinHandRight } },
};

TaskMessageEntry D_actor_213000_80157E1C[5] = {
    { ACTOR_MESSAGE_PLAY_ANIMATION, _actor213000PlayAnimation },
    { ACTOR_MESSAGE_PLACE, actorMsgPlaceEuler },
    { ACTOR_MESSAGE_SET_MODEL_DRAW, _actor213000SetModelDraw },
    { ACTOR_COMMAND_MESSAGE_APPLY, _actor213000ApplyHeldModelCommand },
    { TASK_MESSAGE_TABLE_END, NULL },
}; /// Spawn handler: allocates the work block, seeds its animation bytes and

static void func_actor_213000_80149E54(Task* task);

/// countdown, hides the model, then spawns the four children of the spawn
/// table -- entries 1 and 2 attached to part 8 and kept in `heldModelTasks`,
/// entry 3 attached to part 9 and entry 4 to part 12. Each of the
/// last two has its model's `tpage` / `clut` loaded from the `AreaPlacement` of
/// the current area selected by the model id the parent's `spawnArg2` carries
/// at `Enemy::placeKey >> ENEMY_PLACE_INDEX_SHIFT`, and has its texture stream processed twice
/// when it has a buffer. It then publishes the work block's matrices on the
/// model, installs the message table and `enemyTaskExit` as the exit
/// callback, and advances to the tick. A failed allocation exits the task
/// instead.
static void func_actor_213000_80149E54(Task* task)
{
    _Actor213000EricBaldwinWork* work;
    TmdObject*                   obj;
    GameLocationKey              key;
    Task*                        spawned1;
    Task*                        spawned2;

    obj  = task->extra.tmd;
    work = memCalloc(sizeof(_Actor213000EricBaldwinWork), 0);
    if (work == NULL) {
        enemyTaskExit(task);
        return;
    }
    task->work              = work;
    work->animId            = ACTOR_MODEL_STATE_NONE;
    work->bank              = ACTOR_MODEL_STATE_NONE;
    work->field_478         = 0;
    work->freeCountdown     = -1;
    obj->flags             |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
    work->heldModelTasks[0] = taskSpawnFromTable(D_actor_213000_80157DE0, 1, 8, task);
    work->heldModelTasks[1] = taskSpawnFromTable(D_actor_213000_80157DE0, 2, 8, task);
    spawned1                = taskSpawnFromTable(D_actor_213000_80157DE0, 3, 9, task);
    spawned2                = taskSpawnFromTable(D_actor_213000_80157DE0, 4, 0xC, task);
    if (spawned1 != NULL) {
        TmdObject*       model;
        AreaVariant*     layout;
        AreaPlacement*   place;
        GameLocationKey* sessionKey;
        s32              idx;

        idx        = ((Enemy*)task->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT;
        model      = spawned1->extra.tmd;
        sessionKey = &gGameSession->location.loc;
        key.stage  = sessionKey->stage;
        key.area   = sessionKey->area;
        key.room   = sessionKey->room;
        key.view   = sessionKey->view;
        areaSyncLocationVariant(&key);
        layout                   = areaGetVariant(&key);
        place                    = gpAreaPlaceAt(layout->placements, idx);
        model->texturePageOffset = place->texturePageOffset;
        model->clutRowOffset     = place->clutRowOffset;
        if (model->buffer != NULL) {
            tmdBuildBufferHalf(model);
            tmdBuildBufferHalf(model);
        }
    }
    if (spawned2 != NULL) {
        TmdObject*       model;
        AreaVariant*     layout;
        AreaPlacement*   place;
        GameLocationKey* sessionKey;
        s32              idx;

        model      = spawned2->extra.tmd;
        idx        = ((Enemy*)task->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT;
        sessionKey = &gGameSession->location.loc;
        key.stage  = sessionKey->stage;
        key.area   = sessionKey->area;
        key.room   = sessionKey->room;
        key.view   = sessionKey->view;
        areaSyncLocationVariant(&key);
        layout                   = areaGetVariant(&key);
        place                    = gpAreaPlaceAt(layout->placements, idx);
        model->texturePageOffset = place->texturePageOffset;
        model->clutRowOffset     = place->clutRowOffset;
        if (model->buffer != NULL) {
            tmdBuildBufferHalf(model);
            tmdBuildBufferHalf(model);
        }
    }
    _actor213000InitBodyLighting(task);
    task->msgTable     = D_actor_213000_80157E1C;
    task->exitCallback = enemyTaskExit;
    task->state++;
}

/// State table of the children spawned from table entries 1 and 2: attach to
/// the parent, idle, kill.
static const TaskFuncTable3 D_actor_213000_80149E24 = {
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

    states = D_actor_213000_80149E24;
    states.funcs[heldModelTask->state](heldModelTask);
}

#include "../../shared/model_placement_attach_part.inc.c"

/// Keeps a held model attached without changing its pose, flags or task state.
///
/// The task argument is unused; rendering follows the borrowed parent coordinate.
static void _actor213000HeldModelIdle(Task* unusedTask)
{
}

/// State table of the child spawned from table entry 4: setup, tick, kill.
static const TaskFuncTable3 D_actor_213000_80149E30 = {
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

    states = D_actor_213000_80149E30;
    states.funcs[handTask->state](handTask);
}

#include "../../shared/model_placement_attach.inc.c"

#include "../../shared/model_placement_mirror_parent.inc.c"

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
#include "../../shared/model_placement_mirror_parent.inc.c"

/// State table of the child spawned from table entry 3: setup, tick, kill.
static const TaskFuncTable3 D_actor_213000_80149E3C = {
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

    states = D_actor_213000_80149E3C;
    states.funcs[childTask->state](childTask);
}

/// The actor's three states: spawn, per-frame tick and teardown.
static const TaskFuncTable3 D_actor_213000_80149E48 = {
    {
        func_actor_213000_80149E54,
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

    states = D_actor_213000_80149E48;
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
        work->animId = ACTOR_MODEL_STATE_NONE;
        animationInitContext(&work->rig.anim, D_actor_213000_80157DDC[work->bank], model, work->rig.poses,
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

#include "../../shared/actor_messages_place_euler.inc.c"

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
