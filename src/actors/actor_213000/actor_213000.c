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

static void func_actor_213000_8014A158(Task* task);
static void func_actor_213000_8014A5D0(Task* task);
static void func_actor_213000_8014A6AC(Task* task);

static TmdSource _gActor213000EricBaldwinBody;
static TmdSource _gActor213000Model06A10;
static TmdSource _gActor213000EricBaldwinHandRight;
static TmdSource _gActor213000Model072AC;
static TmdSource _gActor213000Prop;
s32              func_actor_213000_8014A70C(Task*, s32, AnimationPlayRequest*, s32);
s32              func_actor_213000_8014A8A4(Task*, s32, s32, s32);
s32              func_actor_213000_8014A980(Task* task, s32 msgId, ActorCommand* msg, s32 arg3);
void             func_actor_213000_8014A084(Task*);
void             func_actor_213000_8014A160(Task*);
void             func_actor_213000_8014A520(Task*);
void             func_actor_213000_8014A578(Task*);

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
    { { { TASK_BODY_TMD, 192 } }, func_actor_213000_8014A578, { .model = &_gActor213000EricBaldwinBody } },
    { { { TASK_BODY_TMD, 192 } }, func_actor_213000_8014A084, { .model = &_gActor213000Model072AC } },
    { { { TASK_BODY_TMD, 192 } }, func_actor_213000_8014A084, { .model = &_gActor213000Prop } },
    { { { TASK_BODY_TMD, 192 } }, func_actor_213000_8014A520, { .model = &_gActor213000Model06A10 } },
    { { { TASK_BODY_TMD, 192 } }, func_actor_213000_8014A160, { .model = &_gActor213000EricBaldwinHandRight } },
};

TaskMessageEntry D_actor_213000_80157E1C[5] = {
    { ACTOR_MESSAGE_PLAY_ANIMATION, func_actor_213000_8014A70C },
    { ACTOR_MESSAGE_PLACE, actorMsgPlaceEuler },
    { ACTOR_MESSAGE_SET_MODEL_DRAW, func_actor_213000_8014A8A4 },
    { ACTOR_COMMAND_MESSAGE_APPLY, func_actor_213000_8014A980 },
    { TASK_MESSAGE_TABLE_END, NULL },
}; /// Spawn handler: allocates the work block, seeds its animation bytes and

static void func_actor_213000_80149E54(Task* task);
static void func_actor_213000_8014A35C(Task* task);
static void func_actor_213000_8014A488(Task* task);

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
        layout                   = Gp_GetNestedAreaRec(&key);
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
        layout                   = Gp_GetNestedAreaRec(&key);
        place                    = gpAreaPlaceAt(layout->placements, idx);
        model->texturePageOffset = place->texturePageOffset;
        model->clutRowOffset     = place->clutRowOffset;
        if (model->buffer != NULL) {
            tmdBuildBufferHalf(model);
            tmdBuildBufferHalf(model);
        }
    }
    func_actor_213000_8014A6AC(task);
    task->msgTable     = D_actor_213000_80157E1C;
    task->exitCallback = enemyTaskExit;
    task->state++;
}

/// State table of the children spawned from table entries 1 and 2: attach to
/// the parent, idle, kill.
static const TaskFuncTable3 D_actor_213000_80149E24 = {
    {
        modelPlacementAttachPart,
        func_actor_213000_8014A158,
        taskKill,
    },
};

/// Body of the children spawned from table entries 1 and 2: dispatches on
/// the state through `D_actor_213000_80149E24`.
void func_actor_213000_8014A084(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_actor_213000_80149E24;
    sp.funcs[task->state](task);
}

#include "../../shared/model_placement_attach_part.inc.c"

/// The idle state of the children spawned from table entries 1 and 2: does
/// nothing.
static void func_actor_213000_8014A158(Task* task)
{
}

/// State table of the child spawned from table entry 4: setup, tick, kill.
static const TaskFuncTable3 D_actor_213000_80149E30 = {
    {
        modelPlacementAttachChild,
        modelPlacementMirrorParent,
        taskKill,
    },
};

/// Body of the child spawned from table entry 4: dispatches on the state
/// through `D_actor_213000_80149E30`.
void func_actor_213000_8014A160(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_actor_213000_80149E30;
    sp.funcs[task->state](task);
}

#include "../../shared/model_placement_attach.inc.c"

#include "../../shared/model_placement_mirror_parent.inc.c"

/// Setup state of the child spawned from table entry 3: hangs each of the
/// child's three root coordinates off the parent's part nine slots above it
/// (parts 9 to 11) with a zero local transform, shares the parent's light and
/// colour matrices, mirrors the parent's model bits 0x80 (hidden) and 0x4 as
/// the tick state does, draws the model at order-table offset -4, reparents
/// the task under the parent and steps to the tick state.
static void func_actor_213000_8014A35C(Task* task)
{
    Task*      parent;
    TmdObject* obj;
    TmdObject* parentObj;
    GfxCoord*  coords;
    GfxCoord*  root;
    s32        i;
    u16        flags;

    parent    = task->spawnArg2.pointer;
    obj       = task->extra.tmd;
    parentObj = parent->extra.tmd;
    for (i = 0; i < 3; i++) {
        coords             = &(parent->extra.tmd->coords)[i + 9];
        root               = &(task->extra.tmd->coords)[i];
        root->parent       = coords;
        root->coord.t[0]   = 0;
        root->coord.t[1]   = 0;
        root->coord.t[2]   = 0;
        root->param.rot.vx = 0;
        root->param.rot.vy = 0;
        root->param.rot.vz = 0;
        root->composeStamp = GRAPHICS_COORD_DIRTY;
    }
    obj->lightMtx = parentObj->lightMtx;
    obj->colorMtx = parentObj->colorMtx;
    flags         = obj->flags | TMD_OBJECT_SKIP_ACTIVE_DRAW;
    obj->flags    = flags;
    if (!(parentObj->flags & TMD_OBJECT_SKIP_ACTIVE_DRAW)) {
        obj->flags = flags & (u16)~TMD_OBJECT_SKIP_ACTIVE_DRAW;
    }
    if (!(parentObj->flags & TMD_OBJECT_SKIP_AUTO_BUFFER)) {
        obj->flags &= (u16)~TMD_OBJECT_SKIP_AUTO_BUFFER;
        tmdAllocPrimitiveBuffer(obj);
    } else {
        obj->flags |= TMD_OBJECT_SKIP_AUTO_BUFFER;
    }
    obj->otOffset = -4;
    taskReparent(parent, task);
    task->state += 1;
}

/// A second copy, under this file's own name.
#define modelPlacementMirrorParent func_actor_213000_8014A488
#include "../../shared/model_placement_mirror_parent.inc.c"
#undef modelPlacementMirrorParent

/// State table of the child spawned from table entry 3: setup, tick, kill.
static const TaskFuncTable3 D_actor_213000_80149E3C = {
    {
        func_actor_213000_8014A35C,
        func_actor_213000_8014A488,
        taskKill,
    },
};

/// Body of the child spawned from table entry 3: dispatches on the state
/// through `D_actor_213000_80149E3C`.
void func_actor_213000_8014A520(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_actor_213000_80149E3C;
    sp.funcs[task->state](task);
}

/// The actor's three states: spawn, per-frame tick and teardown.
static const TaskFuncTable3 D_actor_213000_80149E48 = {
    {
        func_actor_213000_80149E54,
        func_actor_213000_8014A5D0,
        enemyTaskExit,
    },
};

/// Body of the actor's task (spawn table entry 0): dispatches on the state
/// through `D_actor_213000_80149E48`.
void func_actor_213000_8014A578(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_actor_213000_80149E48;
    sp.funcs[task->state](task);
}

/// Per-frame tick: ticks the work block's animation slots once a preset has
/// started them, and once the view is ready rebuilds model part 1's world
/// matrix and hands its translation to `func_800D7A9C`. `freeCountdown` then
/// frees the model's buffers as it reaches zero.
static void func_actor_213000_8014A5D0(Task* task)
{
    _Actor213000EricBaldwinWork* work;
    TmdObject*                   extra;
    GfxCoord*                    coords;
    s32                          i;

    extra  = task->extra.tmd;
    work   = task->work;
    coords = &extra->coords[1];
    if (work->ticking != 0) {
        for (i = 1; i < ARRAY_SIZE(work->rig.slots); i++) {
            animationTickSlot(&work->rig.anim, i);
        }
    }
    if (gGameSession->viewReady != 0) {
        coords->composeStamp = GRAPHICS_COORD_DIRTY;
        actorRenderComposeCoord(coords);
        func_800D7A9C(extra, (VECTOR*)coords->workm.t, 0, 3);
    }
    if (work->freeCountdown >= 0) {
        if (work->freeCountdown == 0) {
            tmdFreePrimitiveBuffer(extra);
        }
        work->freeCountdown--;
    }
}

/// Points the model's light and colour matrices at the work block's own pair,
/// then rebuilds model part 1's world matrix and hands its translation to
/// `func_800D7A9C`.
static void func_actor_213000_8014A6AC(Task* task)
{
    _Actor213000EricBaldwinWork* work;
    GfxCoord*                    coords;
    TmdObject*                   extra;

    work                   = task->work;
    extra                  = task->extra.tmd;
    coords                 = extra->coords;
    extra->lightMtx        = &work->light;
    extra->colorMtx        = &work->color;
    coords[1].composeStamp = GRAPHICS_COORD_DIRTY;
    actorRenderComposeCoord(&coords[1]);
    func_800D7A9C(extra, (VECTOR*)coords[1].workm.t, 0, 3);
}

/// Applies the requested animation bank and clip to this actor's rig.
///
/// A changed bank installs its set table. The requested clip is applied to the slots.
/// Requested blending uses 6 frames; otherwise the slots reset.
s32 func_actor_213000_8014A70C(Task* task, s32 arg1, AnimationPlayRequest* msg, s32 arg3)
{
    _Actor213000EricBaldwinWork* work;
    TmdObject*                   ext;
    s32                          i;

    work = task->work;
    ext  = task->extra.tmd;
    if (msg->source.index != work->bank) {
        work->bank   = msg->source.index;
        work->animId = ACTOR_MODEL_STATE_NONE;
        animationInitContext(&work->rig.anim, D_actor_213000_80157DDC[work->bank], ext, work->rig.poses,
                             work->rig.slots);
    }
    work->animId = msg->animationId;
    if (msg->blend != ANIMATION_BLEND_RESET) {
        for (i = 1; i < ARRAY_SIZE(work->rig.slots); i++) {
            animationSeekSlotWithBlend(&work->rig.anim, i, work->animId, 0, 6);
        }
    } else {
        for (i = 1; i < ARRAY_SIZE(work->rig.slots); i++) {
            animationResetSlot(&work->rig.anim, i, work->animId);
        }
    }
    for (i = 1; i < ARRAY_SIZE(work->rig.slots); i++) {
        animationTickSlot(&work->rig.anim, i);
    }
    work->ticking = 1;
    return 0;
}

#include "../../shared/actor_messages_place_euler.inc.c"

/// Message-0x7D5 display handler, switching on the message's mode word. Mode
/// 0 hides the model and clears `TMD_OBJECT_SKIP_AUTO_BUFFER`; 1 shows it, reallocates its buffers
/// through `tmdAllocPrimitiveBuffer` and clears `TMD_OBJECT_SKIP_AUTO_BUFFER`; 2 hides it, sets
/// `TMD_OBJECT_SKIP_AUTO_BUFFER` and starts
/// `freeCountdown` at 2, after which the tick frees the buffers; 3
/// shows it and sets `TMD_OBJECT_SKIP_AUTO_BUFFER`. The handled modes return 0; any other mode changes
/// nothing and returns 1.
/// The handler reads `work` before the switch even though mode 2 is its only
/// use, so retail's `lw $v1,0x1C($a0)` sits in the entry block.
s32 func_actor_213000_8014A8A4(Task* task, s32 arg1, s32 mode, s32 arg3)
{
    TmdObject*                   obj;
    _Actor213000EricBaldwinWork* work;
    s32                          ret;

    obj  = task->extra.tmd;
    work = task->work;
    ret  = 0;

    switch (mode) {
        case 0:
            obj->flags |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
            obj->flags &= ~TMD_OBJECT_SKIP_AUTO_BUFFER;
            break;
        case 1:
            obj->flags &= ~TMD_OBJECT_SKIP_ACTIVE_DRAW;
            tmdAllocPrimitiveBuffer(obj);
            obj->flags &= ~TMD_OBJECT_SKIP_AUTO_BUFFER;
            break;
        case 2:
            obj->flags         |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
            work->freeCountdown = 2;
            obj->flags         |= TMD_OBJECT_SKIP_AUTO_BUFFER;
            break;
        case 3:
            obj->flags &= ~TMD_OBJECT_SKIP_ACTIVE_DRAW;
            obj->flags |= TMD_OBJECT_SKIP_AUTO_BUFFER;
            break;
        default:
            ret = 1;
            break;
    }
    return ret;
}

/// Message-0x7DB handler: shows or hides the models of the two children the
/// work block keeps in `heldModelTasks`. Mode 0 shows the first
/// (clears bit 0x80 of its `TmdObject::flags`) and 1 hides it; 2 and 3 show
/// and hide the second. A missing child or an unknown mode touches nothing.
/// Every path returns 0.
/// Both `|= 0x80` arms are written out in the source; the post-reload `jump2`
/// cross-jump folds mode 1's copy into mode 3's, which is why retail's mode-1
/// arm is only the `lw` plus a jump while modes 0 and 2 each keep their own
/// `& 0xFF7F` copy. Which tails jump2 merges is decided by which jumps share a
/// target label, not by how alike the bodies are.
s32 func_actor_213000_8014A980(Task* task, s32 arg1, ActorCommand* msg, s32 arg3)
{
    _Actor213000EricBaldwinWork* work;
    Task*                        child;
    u16                          mode;

    mode = msg->command;
    work = task->work;

    switch (mode) {
        case 0:
            child = work->heldModelTasks[0];
            if (child != NULL) {
                child->extra.tmd->flags &= (u16)~TMD_OBJECT_SKIP_ACTIVE_DRAW;
            }
            break;
        case 1:
            child = work->heldModelTasks[0];
            if (child != NULL) {
                child->extra.tmd->flags |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
            }
            break;
        case 2:
            child = work->heldModelTasks[1];
            if (child != NULL) {
                child->extra.tmd->flags &= (u16)~TMD_OBJECT_SKIP_ACTIVE_DRAW;
            }
            break;
        case 3:
            child = work->heldModelTasks[1];
            if (child != NULL) {
                child->extra.tmd->flags |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
            }
            break;
    }
    return 0;
}
