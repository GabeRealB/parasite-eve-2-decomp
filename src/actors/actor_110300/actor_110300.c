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
#include "../../shared/actor_messages.h"
#include "../../shared/view_figure.h"

/// The block above, published by `func_actor_110300_80131F9C` from the task's
/// `Task::work`.
extern ViewFigureWork* gViewFigureWork;

/// The actor's own task, stored by the step-0 handler. The message handlers
/// drive the animation step driver and the model through it, and the helper
/// task's entry `func_actor_110300_80131FF8` parents its coordinate to one of
/// its model's nodes.
extern Task* gActorSelfTask;

/// The helper task `Task_SpawnFromTable` returns in the step-0 handler; the
/// visibility handler drives its model alongside the actor's, and the exit
/// callback kills it.
extern Task* gActorHelperTask;

/// Spawn descriptor table the step-0 handler spawns the helper task from:
/// index 0 is the actor's own dispatcher, index 1 the helper.
extern TaskDesc gViewFigureTasks[];

/// Animation-set table bound to the work block's context by `animationInitContext`.
extern u8 gViewFigureAnimSets[];

/// Message table published as `Task::msgTable`: the 0x7D3 and 0x7D5 handlers
/// and a terminator.
// Handler views preserve the signatures used by this TU. The dispatcher
// transports each argument in a word register.

extern TaskMessageEntry gViewFigureMessages[];

static void func_actor_110300_80132020(Enemy* enemy, Task* task);

static TmdSource _gActor110300SwatMember2Body;
static TmdSource _gActor110300Model05E6C;
void             func_actor_110300_80131F9C(Task*);
void             func_actor_110300_80131FF8(Task*);

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

TaskMessageEntry gViewFigureMessages[3] = {
    { ACTOR_MESSAGE_PLAY_ANIMATION, viewFigurePlayMessage },
    { ACTOR_MESSAGE_SET_MODEL_DRAW, actorMsgSetPairVisibility },
    { TASK_MESSAGE_TABLE_END, NULL },
};

TaskDesc gViewFigureTasks[2] = {
    { { { TASK_BODY_TMD, 192 } }, func_actor_110300_80131F9C, { .model = &_gActor110300SwatMember2Body } },
    { { { TASK_BODY_TMD, 192 } }, func_actor_110300_80131FF8, { .model = &_gActor110300Model05E6C } },
};

u8 gViewFigureAnimSets[28] = {
    0,
    0,
    0,
    0,
    188,
    129,
    19,
    128,
    24,
    151,
    19,
    128,
    120,
    155,
    19,
    128,
    4,
    158,
    19,
    128,
    44,
    160,
    19,
    128,
    0,
    0,
    0,
    0,
};

ViewFigureWork* gViewFigureWork;

Task* gActorSelfTask;

Task* gActorHelperTask;

#include "../../shared/view_figure_spawn.inc.c"

/// The actor's task entry: a two-state dispatcher whose handler table is built
/// on the stack. It publishes the task's work block in
/// `gViewFigureWork` before calling the handler, which is how the
/// overlay's other functions reach the block without the task.
void func_actor_110300_80131F9C(Task* task)
{
    void (*fns[2])(Enemy*, Task*) = {
        viewFigureSpawnState,
        func_actor_110300_80132020,
    };

    gViewFigureWork = task->work;
    fns[task->state](task->spawnArg2.pointer, task);
}

/// Entry of the helper task: parents the given task's model root to node 8 of
/// the actor's model.
void func_actor_110300_80131FF8(Task* arg0)
{
    GfxCoord* parent;
    GfxCoord* coord;

    parent              = gActorSelfTask->extra.tmd->coords;
    coord               = arg0->extra.tmd->coords;
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
    coord->parent       = parent + 8;
}

/// Step 1 of the `func_actor_110300_80131F9C` dispatcher: run the body the
/// actor's step selects, then refresh the model root as step 0 did by feeding
/// its world translation to `func_800D7A9C` (the light solve) against the
/// model object itself.
///
/// The body reaches the task through the second argument, so the incoming `$a1`
/// is copied into `$a0` (the first, unused, is the `Enemy*`): that copy is
/// what the first call's argument, and the `Task::extra` load feeding it, are
/// both read off.
static void func_actor_110300_80132020(Enemy* enemy, Task* task)
{
    TmdObject* obj;
    GfxCoord*  coord;
    VECTOR     vec;

    obj   = task->extra.tmd;
    coord = obj->coords;
    viewFigureStepAnim(task);
    vec.vx = coord->workm.t[0];
    vec.vy = coord->workm.t[1];
    vec.vz = coord->workm.t[2];
    func_800D7A9C(obj, &vec, 0, 3);
}

/// Exit callback the step-0 handler installs: kills the helper task, then
/// destroys the actor.
void viewFigureExit(Task* arg0)
{
    taskKill(gActorHelperTask);
    enemyDestroy(arg0->spawnArg2.pointer, arg0);
}

#include "../../shared/view_figure_step_anim.inc.c"

/// Ticks animation slots 1..0x13 of the work block's animation context.
void viewFigureTickAnim(void)
{
    s32 i;

    i = 1;
    do {
        animationTickSlot(&gViewFigureWork->rig.anim, i);
        i++;
    } while (i < 0x14);
}

#include "../../shared/view_figure_reset_anim.inc.c"

#include "../../shared/view_figure_reseed_anim.inc.c"

#include "../../shared/view_figure_play_message.inc.c"

#include "../../shared/actor_messages_pair_visibility.inc.c"
