#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/abs.h>

#include "common.h"

#include "actors/actor.h"

#include "gameplay/actor_render.h"
#include "gameplay/animation.h"
#include "gameplay/effect_tasks.h"
#include "gameplay/enemy.h"
#include "gameplay/message.h"
#include "gameplay/room_effects.h"
#include "gameplay/scene.h"
#include "gameplay/scene_combat.h"
#include "gameplay/scene_runtime.h"
#include "gameplay/world_coords.h"

#include "main/coord.h"
#include "main/gfx.h"
#include "main/gfx_types.h"
#include "main/mem.h"
#include "main/task_types.h"
#include "main/tmd.h"
#include "main/tmd_types.h"
#include "../../shared/actor_motion.h"
#include "../../shared/actor_messages.h"
#include "../../shared/reversing_walker.h"

/// Animation bank table the preset's bank index selects from.
extern AnimationSet*  D_actor_350500_80168E8C[5];
extern AnimationSet** gActorMotionAnimBanks19[1];

/// `taskMessageDispatch` handler table installed at `Task::msgTable` by
/// `_reverseWalkSpawn`; terminator id `TASK_MESSAGE_TABLE_END`.
// Handler views preserve the signatures used by this TU. The dispatcher
// transports each argument in a word register.

extern TaskMessageEntry gReverseWalkMessages[];

/// Spawn, tick and exit handlers, dispatched by `func_actor_350500_80162360`.
static const TaskFuncTable3 D_actor_350500_80161E24 = { {
    _reverseWalkSpawn,
    _reverseWalkUpdate,
    _reverseWalkExit,
} };

/// The walk steps, indexed by `ReverseWalkWork::walk.motionStep`: turn to face
/// `target`, start moving, approach until arrival, then turn to the placement
/// yaw.
static const TaskFuncTable4 D_actor_350500_80161E30 = { {
    _reverseWalkOrientForWalk,
    _reverseWalkBeginMove,
    _actorMotionArrive19,
    _reverseWalkTurnToYaw,
} };

/// Local-space offset the start-moving step rotates: straight ahead along
/// the part's own +Z.
static const VECTOR _gReverseWalkForward = { 0, 0, 0x200000, 0 };

static TmdSource _gActor350500EveBreaMaskedBody;
static s32       _actor350500ReverseWalkCommandMsg(Task* task, s32 messageId, const ActorCommand* command, s32 unusedArg);
void             func_actor_350500_80162360(Task*);

static TmdBone _gActor350500EveBreaMaskedBodySkeleton[19] = {
#include "assets/eve_brea_masked_body_skeleton.inc"
};

static u32 _gActor350500EveBreaMaskedBodyPartVerts[19] = {
#include "assets/eve_brea_masked_body_partVerts.inc"
};

static SVECTOR _gActor350500EveBreaMaskedBodyVerts[312] = {
#include "assets/eve_brea_masked_body_verts.inc"
};

static SVECTOR _gActor350500EveBreaMaskedBodyNormals[338] = {
#include "assets/eve_brea_masked_body_normals.inc"
};

static u32 _gActor350500EveBreaMaskedBodyStream[3463] = {
#include "assets/eve_brea_masked_body_stream.inc"
};

static TmdSource _gActor350500EveBreaMaskedBody = {
    0,
    18392,
    6232,
    19,
    _gActor350500EveBreaMaskedBodyPartVerts,
    _gActor350500EveBreaMaskedBodyVerts,
    _gActor350500EveBreaMaskedBodyNormals,
    _gActor350500EveBreaMaskedBodySkeleton,
    _gActor350500EveBreaMaskedBodyStream,
};

static AnimationPackedPose _gActor350500Animation05C4CBank1[2] = {
#include "assets/actor_350500_animation_05C4C_bank1.inc"
};

static AnimationPackedRotation _gActor350500Animation05C4CBank4[23] = {
#include "assets/actor_350500_animation_05C4C_bank4.inc"
};

static AnimationRecord _gActor350500Animation05C4CRecords[84] = {
#include "assets/actor_350500_animation_05C4C_records.inc"
};

static u16 _gActor350500Animation05C4CIndices[20] = {
#include "assets/actor_350500_animation_05C4C_indices.inc"
};

static AnimationSet _gActor350500Animation05C4C = {
    _gActor350500Animation05C4CRecords,
    _gActor350500Animation05C4CIndices,
    { NULL, _gActor350500Animation05C4CBank1, NULL, NULL, _gActor350500Animation05C4CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor350500Animation06830Bank1[22] = {
#include "assets/actor_350500_animation_06830_bank1.inc"
};

static AnimationPackedRotation _gActor350500Animation06830Bank4[298] = {
#include "assets/actor_350500_animation_06830_bank4.inc"
};

static AnimationRecord _gActor350500Animation06830Records[377] = {
#include "assets/actor_350500_animation_06830_records.inc"
};

static u16 _gActor350500Animation06830Indices[20] = {
#include "assets/actor_350500_animation_06830_indices.inc"
};

static AnimationSet _gActor350500Animation06830 = {
    _gActor350500Animation06830Records,
    _gActor350500Animation06830Indices,
    { NULL, _gActor350500Animation06830Bank1, NULL, NULL, _gActor350500Animation06830Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor350500Animation06C38Bank1[5] = {
#include "assets/actor_350500_animation_06C38_bank1.inc"
};

static AnimationPackedRotation _gActor350500Animation06C38Bank4[76] = {
#include "assets/actor_350500_animation_06C38_bank4.inc"
};

static AnimationRecord _gActor350500Animation06C38Records[147] = {
#include "assets/actor_350500_animation_06C38_records.inc"
};

static u16 _gActor350500Animation06C38Indices[20] = {
#include "assets/actor_350500_animation_06C38_indices.inc"
};

static AnimationSet _gActor350500Animation06C38 = {
    _gActor350500Animation06C38Records,
    _gActor350500Animation06C38Indices,
    { NULL, _gActor350500Animation06C38Bank1, NULL, NULL, _gActor350500Animation06C38Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor350500Animation07044Bank1[5] = {
#include "assets/actor_350500_animation_07044_bank1.inc"
};

static AnimationPackedRotation _gActor350500Animation07044Bank4[89] = {
#include "assets/actor_350500_animation_07044_bank4.inc"
};

static AnimationRecord _gActor350500Animation07044Records[135] = {
#include "assets/actor_350500_animation_07044_records.inc"
};

static u16 _gActor350500Animation07044Indices[20] = {
#include "assets/actor_350500_animation_07044_indices.inc"
};

static AnimationSet _gActor350500Animation07044 = {
    _gActor350500Animation07044Records,
    _gActor350500Animation07044Indices,
    { NULL, _gActor350500Animation07044Bank1, NULL, NULL, _gActor350500Animation07044Bank4, NULL, NULL, NULL },
};

AnimationSet* D_actor_350500_80168E8C[5] = {
    NULL,
    &_gActor350500Animation05C4C,
    &_gActor350500Animation06830,
    &_gActor350500Animation06C38,
    &_gActor350500Animation07044,
};

AnimationSet** gActorMotionAnimBanks19[1] = {
    D_actor_350500_80168E8C,
};

TaskDesc D_actor_350500_80168EA4 = { { { (TASK_BODY_TMD | TASK_DESC_SKIP_AUTO_MODEL_BUFFER), 192 } }, func_actor_350500_80162360, { .model = &_gActor350500EveBreaMaskedBody } };

TaskMessageEntry gReverseWalkMessages[6] = {
    { ACTOR_MESSAGE_PLAY_ANIMATION, _actorMotionPlayAnim19 },
    { ACTOR_MESSAGE_PLACE, actorMsgPlaceEuler },
    { ACTOR_MESSAGE_SET_MODEL_DRAW, _reverseWalkSetDrawModeMsg },
    { ACTOR_MESSAGE_WALK_TO, _reverseWalkStartWalkMsg },
    { ACTOR_COMMAND_MESSAGE_APPLY, _actor350500ReverseWalkCommandMsg },
    { TASK_MESSAGE_TABLE_END, NULL },
};
#include "../../shared/reversing_walker_update.inc.c"

#include "../../shared/actor_motion_arrive19.inc.c"

#include "../../shared/reversing_walker_start.inc.c"

/// Per-frame dispatcher: runs the spawn, tick or exit state from
/// `D_actor_350500_80161E24`, skipping the frame while the global freeze
/// byte is set.
void func_actor_350500_80162360(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_actor_350500_80161E24;
    if (gSceneCombatState.actorControl == SCENE_COMBAT_ACTORS_RUNNING) {
        sp.funcs[task->state](task);
    }
}

#include "../../shared/reversing_walker_spawn.inc.c"

/// Releases the walker's enemy record and begins task and model teardown.
///
/// Requires a live task with its owned primary-heap `Enemy` in
/// `spawnArg2.pointer`. Target tracking and actor locks are released before
/// children, work and model storage. Do not access the task or enemy afterwards.
static void _reverseWalkExit(Task* task)
{
    enemyTaskExit(task);
}

/// Lends the walker's lighting matrices to its model for lighting and drawing.
///
/// Requires a live TMD task with allocated `ReverseWalkWork`. The model borrows
/// writable matrices in that work; keep it live until model use ends.
/// This binds storage without calculating or initializing either matrix.
static void _reverseWalkBindLighting(Task* task)
{
    TmdObject*       model;
    ReverseWalkWork* work;

    model           = task->extra.tmd;
    work            = task->work;
    model->lightMtx = &work->model.light;
    model->colorMtx = &work->model.color;
}

/// Leaves walk state unchanged while no scripted walk is in progress.
///
/// The frame update still integrates velocity, ticks animation and draws.
/// The task parameter is unused but retains the `TaskFunc` callback signature.
static void _reverseWalkIdle(Task* task)
{
}

/// Runs the current phase of a scripted forward or backward walk.
///
/// Requires initialized `ReverseWalkWork` and `walk.motionStep` in 0..3:
/// face the target, begin movement, approach until arrival, then turn to the
/// destination yaw. Runs one phase per tick without checking the index.
static void _reverseWalkRunStep(Task* task)
{
    ReverseWalkWork*     work     = task->work;
    const TaskFuncTable4 handlers = D_actor_350500_80161E30;

    handlers.funcs[work->walk.motionStep](task);
}

#include "../../shared/reversing_walker_face.inc.c"

#include "../../shared/reversing_walker_move.inc.c"

#include "../../shared/reversing_walker_turn.inc.c"

#include "../../shared/actor_motion_play19.inc.c"

#include "../../shared/actor_messages_place_euler.inc.c"

#include "../../shared/reversing_walker_visibility.inc.c"

/// Selects whether subsequent walk setup approaches targets forward or backward.
///
/// Requires initialized `ReverseWalkWork` and a borrowed, readable command.
/// Command 1 selects backing, 2 selects forward walking; other commands leave
/// the direction unchanged. Ignores the context, message ID and second payload.
/// Retains no command pointer and always returns 0. Existing velocity is intact.
static s32 _actor350500ReverseWalkCommandMsg(Task* task, s32 messageId, const ActorCommand* command, s32 unusedArg)
{
    enum {
        ACTOR_350500_REVERSE_WALK_COMMAND_BACKWARD = 1,
        ACTOR_350500_REVERSE_WALK_COMMAND_FORWARD  = 2,
    };
    ReverseWalkWork* work;

    work = task->work;
    switch (command->command) {
        case ACTOR_350500_REVERSE_WALK_COMMAND_BACKWARD:
            work->walksForward = 0;
            break;
        case ACTOR_350500_REVERSE_WALK_COMMAND_FORWARD:
            work->walksForward = 1;
            break;
    }
    return 0;
}
