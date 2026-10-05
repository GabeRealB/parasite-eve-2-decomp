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
#include "../../shared/actor_messages.h"
// Exported instance: a room spawns from this package's table by name.
#define gViewFigureTasks gActor110800ViewFigureTasks
#include "../../shared/view_figure.h"

/// The block above, published by `func_actor_110800_801322A0` from the task's
/// `Task::work`.
extern ViewFigureWork* gViewFigureWork;

/// The actor's own task, stored by the step-0 handler. The message handlers
/// drive the step dispatcher and the model through it, and
/// `func_actor_110800_801322FC` parents a coordinate to one of its model's
/// nodes.
extern Task* gActorSelfTask;

/// The helper task `taskSpawnFromTable` returns in the step-0 handler; the
/// visibility handler drives its model alongside the actor's, and the exit
/// callback kills it.
extern Task* gActorHelperTask;

/// Spawn descriptor table the step-0 handler spawns the helper task from.
extern TaskDesc gViewFigureTasks[];

/// Animation-set table bound to the work block's context by `animationInitContext`.
extern u8 gViewFigureAnimSets[];

/// Message table published as `Task::msgTable`: the 0x7D3 and 0x7D5 handlers
/// below and a terminator.
// Handler views preserve the signatures used by this TU. The dispatcher
// transports each argument in a word register.

extern TaskMessageEntry gViewFigureMessages[];

static TmdSource _gActor110800SwatMember2Body;
static TmdSource _gActor110800Model060A0;
void             func_actor_110800_801322A0(Task*);
void             func_actor_110800_801322FC(Task*);

static TmdBone _gActor110800SwatMember2BodySkeleton[20] = {
#include "assets/swat_member_2_body_skeleton.inc"
};

static u32 _gActor110800SwatMember2BodyPartVerts[20] = {
#include "assets/swat_member_2_body_partVerts.inc"
};

static SVECTOR _gActor110800SwatMember2BodyVerts[360] = {
#include "assets/swat_member_2_body_verts.inc"
};

static SVECTOR _gActor110800SwatMember2BodyNormals[358] = {
#include "assets/swat_member_2_body_normals.inc"
};

static u32 _gActor110800SwatMember2BodyStream[3973] = {
#include "assets/swat_member_2_body_stream.inc"
};

static TmdSource _gActor110800SwatMember2Body = {
    0,
    21176,
    6792,
    20,
    _gActor110800SwatMember2BodyPartVerts,
    _gActor110800SwatMember2BodyVerts,
    _gActor110800SwatMember2BodyNormals,
    _gActor110800SwatMember2BodySkeleton,
    _gActor110800SwatMember2BodyStream,
};

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

static AnimationPackedPose _gActor110800Animation066F0Bank1[10] = {
#include "assets/actor_110800_animation_066F0_bank1.inc"
};

static AnimationPackedRotation _gActor110800Animation066F0Bank4[108] = {
#include "assets/actor_110800_animation_066F0_bank4.inc"
};

static AnimationRecord _gActor110800Animation066F0Records[149] = {
#include "assets/actor_110800_animation_066F0_records.inc"
};

static u16 _gActor110800Animation066F0Indices[20] = {
#include "assets/actor_110800_animation_066F0_indices.inc"
};

static AnimationSet _gActor110800Animation066F0 = {
    _gActor110800Animation066F0Records,
    _gActor110800Animation066F0Indices,
    { NULL, _gActor110800Animation066F0Bank1, NULL, NULL, _gActor110800Animation066F0Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor110800Animation06A9CBank1[8] = {
#include "assets/actor_110800_animation_06A9C_bank1.inc"
};

static AnimationPackedRotation _gActor110800Animation06A9CBank4[76] = {
#include "assets/actor_110800_animation_06A9C_bank4.inc"
};

static AnimationRecord _gActor110800Animation06A9CRecords[115] = {
#include "assets/actor_110800_animation_06A9C_records.inc"
};

static u16 _gActor110800Animation06A9CIndices[20] = {
#include "assets/actor_110800_animation_06A9C_indices.inc"
};

static AnimationSet _gActor110800Animation06A9C = {
    _gActor110800Animation06A9CRecords,
    _gActor110800Animation06A9CIndices,
    { NULL, _gActor110800Animation06A9CBank1, NULL, NULL, _gActor110800Animation06A9CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor110800Animation07050Bank1[15] = {
#include "assets/actor_110800_animation_07050_bank1.inc"
};

static AnimationPackedRotation _gActor110800Animation07050Bank4[128] = {
#include "assets/actor_110800_animation_07050_bank4.inc"
};

static AnimationRecord _gActor110800Animation07050Records[172] = {
#include "assets/actor_110800_animation_07050_records.inc"
};

static u16 _gActor110800Animation07050Indices[20] = {
#include "assets/actor_110800_animation_07050_indices.inc"
};

static AnimationSet _gActor110800Animation07050 = {
    _gActor110800Animation07050Records,
    _gActor110800Animation07050Indices,
    { NULL, _gActor110800Animation07050Bank1, NULL, NULL, _gActor110800Animation07050Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor110800Animation0770CBank1[11] = {
#include "assets/actor_110800_animation_0770C_bank1.inc"
};

static AnimationPackedRotation _gActor110800Animation0770CBank4[170] = {
#include "assets/actor_110800_animation_0770C_bank4.inc"
};

static AnimationRecord _gActor110800Animation0770CRecords[208] = {
#include "assets/actor_110800_animation_0770C_records.inc"
};

static u16 _gActor110800Animation0770CIndices[20] = {
#include "assets/actor_110800_animation_0770C_indices.inc"
};

static AnimationSet _gActor110800Animation0770C = {
    _gActor110800Animation0770CRecords,
    _gActor110800Animation0770CIndices,
    { NULL, _gActor110800Animation0770CBank1, NULL, NULL, _gActor110800Animation0770CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor110800Animation0807CBank1[25] = {
#include "assets/actor_110800_animation_0807C_bank1.inc"
};

static AnimationPackedRotation _gActor110800Animation0807CBank4[215] = {
#include "assets/actor_110800_animation_0807C_bank4.inc"
};

static AnimationRecord _gActor110800Animation0807CRecords[294] = {
#include "assets/actor_110800_animation_0807C_records.inc"
};

static u16 _gActor110800Animation0807CIndices[20] = {
#include "assets/actor_110800_animation_0807C_indices.inc"
};

static AnimationSet _gActor110800Animation0807C = {
    _gActor110800Animation0807CRecords,
    _gActor110800Animation0807CIndices,
    { NULL, _gActor110800Animation0807CBank1, NULL, NULL, _gActor110800Animation0807CBank4, NULL, NULL, NULL },
};

TaskMessageEntry gViewFigureMessages[3] = {
    { ACTOR_MESSAGE_PLAY_ANIMATION, viewFigurePlayMessage },
    { ACTOR_MESSAGE_SET_MODEL_DRAW, actorMsgSetPairVisibility },
    { TASK_MESSAGE_TABLE_END, NULL },
};

TaskDesc gViewFigureTasks[2] = {
    { { { TASK_BODY_TMD, 192 } }, func_actor_110800_801322A0, { .model = &_gActor110800SwatMember2Body } },
    { { { TASK_BODY_TMD, 192 } }, func_actor_110800_801322FC, { .model = &_gActor110800Model060A0 } },
};

u8 gViewFigureAnimSets[28] = {
    0,
    0,
    0,
    0,
    16,
    133,
    19,
    128,
    188,
    136,
    19,
    128,
    112,
    142,
    19,
    128,
    44,
    149,
    19,
    128,
    156,
    158,
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

static void func_actor_110800_80131F9C(Enemy* enemy, Task* task);

#include "../../shared/view_figure_spawn.inc.c"

/// Step 1 of the `func_actor_110800_801322A0` dispatcher, the walk/run
/// footstep cue:
/// run the body the actor's step selects, cue the sound the running
/// animation's frame table asks for, then refresh the model root as step 0 did
/// by feeding its world translation to `func_800D7A9C` (the light solve)
/// against the model object itself.
///
/// The two animation ids this actor plays carry a frame table each: id 4
/// watches slot 19 alone, id 5 watches slot 19 and then slot 16. An entry
/// latches the frame it fired for in `st.cueRecord`, so a frame that is held
/// over several calls only cues once; the mask is the frame index of the slot's
/// halfword.
///
/// The body reaches the task through the second argument, so the incoming `$a1`
/// is copied into `$a0` (the first, unused, is the `Enemy*`), and the model
/// and its coordinate are read through that copy. `task->extra` is written
/// twice with the coordinate taken through the first read: that leaves cse's
/// load in a temporary and copies it into `obj`, which is the `move` between
/// the two loads the target has.
static void func_actor_110800_80131F9C(Enemy* enemy, Task* task)
{
    GfxCoord*  coord;
    TmdObject* obj;
    VECTOR     vec;

    coord = task->extra.tmd->coords;
    obj   = task->extra.tmd;
    viewFigureStepAnim(task);
    switch (gViewFigureWork->st.animId) {
        case 4:
            if ((gViewFigureWork->rig.slots[19].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK) == 0xC8) {
                if (gViewFigureWork->st.cueRecord != (gViewFigureWork->rig.slots[19].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK)) {
                    sndEvtRequestScriptStart(SOUND_AREA(GAME_STAGE_ACROPOLIS, GAME_AREA_ACROPOLIS_ROOF_GARDEN, 0x11), 0, 0);
                }
                gViewFigureWork->st.cueRecord = gViewFigureWork->rig.slots[19].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK;
            }
            if ((gViewFigureWork->rig.slots[19].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK) == 0xCA) {
                if (gViewFigureWork->st.cueRecord != (gViewFigureWork->rig.slots[19].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK)) {
                    sndEvtRequestScriptStart(SOUND_AREA(GAME_STAGE_ACROPOLIS, GAME_AREA_ACROPOLIS_ROOF_GARDEN, 0x0D), 0, 0);
                }
                gViewFigureWork->st.cueRecord = gViewFigureWork->rig.slots[19].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK;
            }
            if ((gViewFigureWork->rig.slots[19].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK) == 0xCD) {
                if (gViewFigureWork->st.cueRecord != (gViewFigureWork->rig.slots[19].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK)) {
                    sndEvtRequestScriptStart(SOUND_AREA(GAME_STAGE_ACROPOLIS, GAME_AREA_ACROPOLIS_ROOF_GARDEN, 0x0E), 0, 0);
                }
                gViewFigureWork->st.cueRecord = gViewFigureWork->rig.slots[19].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK;
            }
            break;
        case 5:
            if ((gViewFigureWork->rig.slots[19].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK) == 0x115) {
                if (gViewFigureWork->st.cueRecord != (gViewFigureWork->rig.slots[19].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK)) {
                    sndEvtRequestScriptStart(SOUND_AREA(GAME_STAGE_ACROPOLIS, GAME_AREA_ACROPOLIS_ROOF_GARDEN, 0x0F), 0, 0);
                }
                gViewFigureWork->st.cueRecord = gViewFigureWork->rig.slots[19].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK;
            }
            if ((gViewFigureWork->rig.slots[19].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK) == 0x11F) {
                if (gViewFigureWork->st.cueRecord != (gViewFigureWork->rig.slots[19].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK)) {
                    sndEvtRequestScriptStart(SOUND_AREA(GAME_STAGE_ACROPOLIS, GAME_AREA_ACROPOLIS_ROOF_GARDEN, 0x0F), 0, 0);
                }
                gViewFigureWork->st.cueRecord = gViewFigureWork->rig.slots[19].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK;
            }
            if ((gViewFigureWork->rig.slots[16].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK) == 0xCE) {
                if (gViewFigureWork->st.cueRecord != (gViewFigureWork->rig.slots[16].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK)) {
                    sndEvtRequestScriptStart(SOUND_AREA(GAME_STAGE_ACROPOLIS, GAME_AREA_ACROPOLIS_ROOF_GARDEN, 0x10), 0, 0);
                }
                gViewFigureWork->st.cueRecord = gViewFigureWork->rig.slots[16].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK;
            }
            if ((gViewFigureWork->rig.slots[16].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK) == 0xD8) {
                if (gViewFigureWork->st.cueRecord != (gViewFigureWork->rig.slots[16].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK)) {
                    sndEvtRequestScriptStart(SOUND_AREA(GAME_STAGE_ACROPOLIS, GAME_AREA_ACROPOLIS_ROOF_GARDEN, 0x10), 0, 0);
                }
                gViewFigureWork->st.cueRecord = gViewFigureWork->rig.slots[16].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK;
            }
            break;
    }
    vec.vx = coord->workm.t[0];
    vec.vy = coord->workm.t[1];
    vec.vz = coord->workm.t[2];
    func_800D7A9C(obj, &vec, 0, 3);
}

/// The actor's task entry: a two-state dispatcher whose handler table is built
/// on the stack. It publishes the task's work block in
/// `gViewFigureWork` before calling the handler, which is how the
/// overlay's other functions reach the block without the task.
void func_actor_110800_801322A0(Task* task)
{
    void (*fns[2])(Enemy*, Task*) = {
        viewFigureSpawnState,
        func_actor_110800_80131F9C,
    };

    gViewFigureWork = task->work;
    fns[task->state](task->spawnArg2.pointer, task);
}

/// Parents the given task's model root to node 8 of the actor's model, offset
/// -50 on x.
void func_actor_110800_801322FC(Task* arg0)
{
    GfxCoord* parent;
    GfxCoord* coord;

    parent              = gActorSelfTask->extra.tmd->coords;
    coord               = arg0->extra.tmd->coords;
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
    coord->coord.t[0]   = -50;
    coord->parent       = parent + 8;
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
