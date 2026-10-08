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

static s32 _viewFigurePlayMessage(Task* unusedTask, s32 unusedMessageId, const AnimationPlayRequest* request, s32 unusedSecondArg);

static void _viewFigureStepAnim(Task* task);

/// The block above, published by `func_actor_110800_801322A0` from the task's
/// `Task::work`.
extern ViewFigureWork* gViewFigureWork;

/// The actor's own task, stored by the step-0 handler. The message handlers
/// drive the step dispatcher and the model through it, and
/// `_actor110800AttachModelTask` parents a coordinate to one of its model's
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
static void      _actor110800AttachModelTask(Task* task);

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
    { ACTOR_MESSAGE_PLAY_ANIMATION, _viewFigurePlayMessage },
    { ACTOR_MESSAGE_SET_MODEL_DRAW, actorMsgSetPairVisibility },
    { TASK_MESSAGE_TABLE_END, NULL },
};

TaskDesc gViewFigureTasks[2] = {
    { { { TASK_BODY_TMD, 192 } }, func_actor_110800_801322A0, { .model = &_gActor110800SwatMember2Body } },
    { { { TASK_BODY_TMD, 192 } }, _actor110800AttachModelTask, { .model = &_gActor110800Model060A0 } },
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

static void _actor110800UpdateViewFigure(Enemy* unusedEnemy, Task* task);

#include "../../shared/view_figure_spawn.inc.c"

/// Emits a loaded area sound at one track record and updates the shared cue latch.
///
/// Requires the live singleton rig. Track/record/script arguments select one
/// of this carrier's established cues; held records compare against the single
/// latch shared by both tracks. Playback and sound resources must be loaded.
/// Standalone compound statements only; captures gViewFigureWork. trackIndex
/// is read repeatedly and must be side-effect-free; cueRecordIndex is evaluated
/// once and soundScript only on a new cue. No argument or payload is retained.
#define ACTOR_110800_CUE_ANIMATION_SOUND(trackIndex, cueRecordIndex, soundScript)                                                                              \
    {                                                                                                                                                          \
        if ((gViewFigureWork->rig.slots[(trackIndex)].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK) == (cueRecordIndex)) {                  \
            if (gViewFigureWork->st.cueRecord != (gViewFigureWork->rig.slots[(trackIndex)].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK)) { \
                sndEvtRequestScriptStart((soundScript), 0, 0);                                                                                                 \
            }                                                                                                                                                  \
            gViewFigureWork->st.cueRecord = gViewFigureWork->rig.slots[(trackIndex)].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK;          \
        }                                                                                                                                                      \
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
    switch (gViewFigureWork->st.animId) {
        case ACTOR_110800_CUED_CLIP_4:
            ACTOR_110800_CUE_ANIMATION_SOUND(19, 0xC8, SOUND_AREA(GAME_STAGE_ACROPOLIS, GAME_AREA_ACROPOLIS_ROOF_GARDEN, 0x11));
            ACTOR_110800_CUE_ANIMATION_SOUND(19, 0xCA, SOUND_AREA(GAME_STAGE_ACROPOLIS, GAME_AREA_ACROPOLIS_ROOF_GARDEN, 0x0D));
            ACTOR_110800_CUE_ANIMATION_SOUND(19, 0xCD, SOUND_AREA(GAME_STAGE_ACROPOLIS, GAME_AREA_ACROPOLIS_ROOF_GARDEN, 0x0E));
            break;
        case ACTOR_110800_CUED_CLIP_5:
            ACTOR_110800_CUE_ANIMATION_SOUND(19, 0x115, SOUND_AREA(GAME_STAGE_ACROPOLIS, GAME_AREA_ACROPOLIS_ROOF_GARDEN, 0x0F));
            ACTOR_110800_CUE_ANIMATION_SOUND(19, 0x11F, SOUND_AREA(GAME_STAGE_ACROPOLIS, GAME_AREA_ACROPOLIS_ROOF_GARDEN, 0x0F));
            ACTOR_110800_CUE_ANIMATION_SOUND(16, 0xCE, SOUND_AREA(GAME_STAGE_ACROPOLIS, GAME_AREA_ACROPOLIS_ROOF_GARDEN, 0x10));
            ACTOR_110800_CUE_ANIMATION_SOUND(16, 0xD8, SOUND_AREA(GAME_STAGE_ACROPOLIS, GAME_AREA_ACROPOLIS_ROOF_GARDEN, 0x10));
            break;
    }
    lightPosition.vx = rootCoord->workm.t[0];
    lightPosition.vy = rootCoord->workm.t[1];
    lightPosition.vz = rootCoord->workm.t[2];
    worldCoordSetModelLighting(model, &lightPosition, 0, ACTOR_110800_LIGHT_COUNT);
}
#undef ACTOR_110800_CUE_ANIMATION_SOUND

/// The actor's task entry: a two-state dispatcher whose handler table is built
/// on the stack. It publishes the task's work block in
/// `gViewFigureWork` before calling the handler, which is how the
/// overlay's other functions reach the block without the task.
void func_actor_110800_801322A0(Task* task)
{
    void (*fns[2])(Enemy*, Task*) = {
        _viewFigureSpawnState,
        _actor110800UpdateViewFigure,
    };

    gViewFigureWork = task->work;
    fns[task->state](task->spawnArg2.pointer, task);
}

/// Attaches the helper model's root to body part 8 with a -50-unit local x translation.
///
/// `task` must be the live slot-1 model task, and `gActorSelfTask` must own
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

    bodyCoords               = gActorSelfTask->extra.tmd->coords;
    helperRoot               = task->extra.tmd->coords;
    helperRoot->composeStamp = GRAPHICS_COORD_DIRTY;
    helperRoot->coord.t[0]   = ACTOR_110800_HELPER_X_OFFSET;
    helperRoot->parent       = &bodyCoords[ACTOR_110800_HELPER_PARENT_PART];
}

#include "../../shared/view_figure_exit.inc.c"

#include "../../shared/view_figure_step_anim.inc.c"

#include "../../shared/view_figure_tick_anim.inc.c"

#include "../../shared/view_figure_reset_anim.inc.c"

#include "../../shared/view_figure_reseed_anim.inc.c"

#include "../../shared/view_figure_play_message.inc.c"

#include "../../shared/actor_messages_pair_visibility.inc.c"
