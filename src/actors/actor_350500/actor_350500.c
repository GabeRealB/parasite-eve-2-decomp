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
/// `reverseWalkSpawn`; terminator id `TASK_MESSAGE_TABLE_END`.
// Handler views preserve the signatures used by this TU. The dispatcher
// transports each argument in a word register.

extern TaskMessageEntry gReverseWalkMessages[];

/// Spawn, tick and exit handlers, dispatched by `func_actor_350500_80162360`.
static const TaskFuncTable3 D_actor_350500_80161E24 = { {
    reverseWalkSpawn,
    reverseWalkUpdate,
    reverseWalkExit,
} };

/// The walk steps, indexed by `ReverseWalkWork::walk.motionStep`: turn to face
/// `target`, start moving, approach until arrival, then turn to the placement
/// yaw.
static const TaskFuncTable4 D_actor_350500_80161E30 = { {
    reverseWalkFaceTarget,
    reverseWalkBeginMove,
    actorMotionArrive19,
    reverseWalkTurnToYaw,
} };

/// Local-space offset the start-moving step rotates: straight ahead along
/// the part's own +Z.
static const VECTOR _gReverseWalkForward = { 0, 0, 0x200000, 0 };

static TmdSource _gActor350500EveBreaMaskedBody;
s32              func_actor_350500_80162ABC(Task* task, s32 msgId, ActorCommand* msg, s32 arg3);
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
    { ACTOR_MESSAGE_SET_MODEL_DRAW, reverseWalkVisibilityMsg },
    { ACTOR_MESSAGE_WALK_TO, reverseWalkStartMsg },
    { ACTOR_COMMAND_MESSAGE_APPLY, func_actor_350500_80162ABC },
    { TASK_MESSAGE_TABLE_END, NULL },
}; /// Per-frame tick: runs the idle or the walk handler `walk.motion` selects,
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

/// Exit callback `reverseWalkSpawn` installs; tears the task down.
void reverseWalkExit(Task* arg0)
{
    enemyTaskExit(arg0);
}

/// Republishes the work block's two matrices onto `TmdObject::lightMtx` /
/// `colorMtx`, so the actor draws with its own lighting.
void reverseWalkBindLighting(Task* arg0)
{
    TmdObject*       ext;
    ReverseWalkWork* work;

    ext           = arg0->extra.tmd;
    work          = arg0->work;
    ext->lightMtx = &work->model.light;
    ext->colorMtx = &work->model.color;
}

/// Idle tick handler, selected while `walk.motion` is clear.
void reverseWalkIdle(Task* arg0)
{
}

/// Walk tick handler: runs the step of `D_actor_350500_80161E30` that
/// `walk.motionStep` selects.
void reverseWalkRunStep(Task* arg0)
{
    TaskFuncTable4   handlers;
    ReverseWalkWork* work;

    work     = arg0->work;
    handlers = D_actor_350500_80161E30;
    handlers.funcs[work->walk.motionStep](arg0);
}

#include "../../shared/reversing_walker_face.inc.c"

#include "../../shared/reversing_walker_move.inc.c"

#include "../../shared/reversing_walker_turn.inc.c"

#include "../../shared/actor_motion_play19.inc.c"

#include "../../shared/actor_messages_place_euler.inc.c"

#include "../../shared/reversing_walker_visibility.inc.c"

/// `taskMessageDispatch` handler: latches the walk direction the message's
/// command selects into `walksForward` -- 1 clears it, so the walker backs
/// toward its targets, 2 sets it, anything else leaves it. Always returns 0.
s32 func_actor_350500_80162ABC(Task* task, s32 arg1, ActorCommand* msg, s32 arg3)
{
    ReverseWalkWork* work;

    work = task->work;
    switch (msg->command) {
        case 1:
            work->walksForward = 0;
            break;
        case 2:
            work->walksForward = 1;
            break;
    }
    return 0;
}
