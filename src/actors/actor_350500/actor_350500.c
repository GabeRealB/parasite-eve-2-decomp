#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/abs.h>

#include "common.h"

#include "actors/actor.h"

#include "gameplay/actor_render.h"
#include "gameplay/animation.h"
#include "gameplay/effect_tasks.h"
#include "gameplay/message.h"
#include "gameplay/room_effects.h"
#include "gameplay/scene.h"
#include "gameplay/scene_runtime.h"
#include "gameplay/world_coords.h"
#include "gameplay/world_state.h"
#include "gameplay/world_targets.h"

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

/// Optional start animation the placement handler takes: the preset's
/// `field_4` and the `model.nextAnimId` byte. Absent, the defaults are anim 3 (or 2
/// once `field_4C4` is set) and 1.
typedef GpSpawnAnimArg Actor350500SpawnAnim;

/// Animation bank table the preset's bank index selects from.
extern AnimationSet*  D_actor_350500_80168E8C[5];
extern AnimationSet** gActorMotionAnimBanks19[1];

/// `Gp_DispatchMsg` handler table installed at `Task::msgTable` by
/// `reverseWalkSpawn`; terminator id 0x7FFFFFFF.
// Handler views preserve the signatures used by this TU. The dispatcher
// transports each argument in a word register.
typedef struct {
    s32 id;
    union {
        s32 (*call0)(Task*, s32, AnimationPlayRequest*, s32);
        s32 (*call1)(Task*, s32, ActorCommand* request);
        s32 (*call2)(Task*, s32, ActorTransform*);
        s32 (*call3)(Task*, s32, ActorTransform*, Actor350500SpawnAnim*);
        s32 (*call4)(Task*, s32, s32);
    } handler;
} Actor350500MsgEntry;
STATIC_ASSERT_SIZEOF(Actor350500MsgEntry, 8);

extern Actor350500MsgEntry gReverseWalkMessages[];

/// Spawn, tick and exit handlers, dispatched by `func_actor_350500_80162360`.
static const TaskFuncTable3 D_actor_350500_80161E24 = { {
    reverseWalkSpawn,
    reverseWalkUpdate,
    reverseWalkExit,
} };

/// The walk steps, indexed by `Actor350500Work::walk.motionStep`: turn to face
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

extern TmdSource D_actor_350500_8016785C;
s32              func_actor_350500_80162ABC(Task*, s32, ActorCommand* msg);
void             func_actor_350500_80162360(Task*);

TmdBone D_actor_350500_80162AF8[19] = {
#include "assets/actor_350500_model_05A3C_skeleton.inc"
};

u32 D_actor_350500_80162DA4[19] = {
#include "assets/actor_350500_model_05A3C_partVerts.inc"
};

SVECTOR D_actor_350500_80162DF0[312] = {
#include "assets/actor_350500_model_05A3C_verts.inc"
};

SVECTOR D_actor_350500_801637B0[338] = {
#include "assets/actor_350500_model_05A3C_normals.inc"
};

u32 D_actor_350500_80164240[3463] = {
#include "assets/actor_350500_model_05A3C_stream.inc"
};

TmdSource D_actor_350500_8016785C = {
    0,
    18392,
    6232,
    19,
    D_actor_350500_80162DA4,
    D_actor_350500_80162DF0,
    D_actor_350500_801637B0,
    D_actor_350500_80162AF8,
    D_actor_350500_80164240,
};

AnimationPackedPose D_actor_350500_80167880[2] = {
#include "assets/actor_350500_animation_05C4C_bank1.inc"
};

AnimationPackedRotation D_actor_350500_80167898[23] = {
#include "assets/actor_350500_animation_05C4C_bank4.inc"
};

AnimationRecord D_actor_350500_801678F4[84] = {
#include "assets/actor_350500_animation_05C4C_records.inc"
};

u16 D_actor_350500_80167A44[20] = {
#include "assets/actor_350500_animation_05C4C_indices.inc"
};

AnimationSet D_actor_350500_80167A6C = {
    D_actor_350500_801678F4,
    D_actor_350500_80167A44,
    { NULL, D_actor_350500_80167880, NULL, NULL, D_actor_350500_80167898, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_350500_80167A94[22] = {
#include "assets/actor_350500_animation_06830_bank1.inc"
};

AnimationPackedRotation D_actor_350500_80167B9C[298] = {
#include "assets/actor_350500_animation_06830_bank4.inc"
};

AnimationRecord D_actor_350500_80168044[377] = {
#include "assets/actor_350500_animation_06830_records.inc"
};

u16 D_actor_350500_80168628[20] = {
#include "assets/actor_350500_animation_06830_indices.inc"
};

AnimationSet D_actor_350500_80168650 = {
    D_actor_350500_80168044,
    D_actor_350500_80168628,
    { NULL, D_actor_350500_80167A94, NULL, NULL, D_actor_350500_80167B9C, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_350500_80168678[5] = {
#include "assets/actor_350500_animation_06C38_bank1.inc"
};

AnimationPackedRotation D_actor_350500_801686B4[76] = {
#include "assets/actor_350500_animation_06C38_bank4.inc"
};

AnimationRecord D_actor_350500_801687E4[147] = {
#include "assets/actor_350500_animation_06C38_records.inc"
};

u16 D_actor_350500_80168A30[20] = {
#include "assets/actor_350500_animation_06C38_indices.inc"
};

AnimationSet D_actor_350500_80168A58 = {
    D_actor_350500_801687E4,
    D_actor_350500_80168A30,
    { NULL, D_actor_350500_80168678, NULL, NULL, D_actor_350500_801686B4, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_350500_80168A80[5] = {
#include "assets/actor_350500_animation_07044_bank1.inc"
};

AnimationPackedRotation D_actor_350500_80168ABC[89] = {
#include "assets/actor_350500_animation_07044_bank4.inc"
};

AnimationRecord D_actor_350500_80168C20[135] = {
#include "assets/actor_350500_animation_07044_records.inc"
};

u16 D_actor_350500_80168E3C[20] = {
#include "assets/actor_350500_animation_07044_indices.inc"
};

AnimationSet D_actor_350500_80168E64 = {
    D_actor_350500_80168C20,
    D_actor_350500_80168E3C,
    { NULL, D_actor_350500_80168A80, NULL, NULL, D_actor_350500_80168ABC, NULL, NULL, NULL },
};

AnimationSet* D_actor_350500_80168E8C[5] = {
    NULL,
    &D_actor_350500_80167A6C,
    &D_actor_350500_80168650,
    &D_actor_350500_80168A58,
    &D_actor_350500_80168E64,
};

AnimationSet** gActorMotionAnimBanks19[1] = {
    D_actor_350500_80168E8C,
};

TaskDesc D_actor_350500_80168EA4 = { { { (TASK_BODY_TMD | TASK_DESC_SKIP_MODEL_BUFFER), 192 } }, func_actor_350500_80162360, { .model = &D_actor_350500_8016785C } };

Actor350500MsgEntry gReverseWalkMessages[6] = {
    { 2003, { .call0 = actorMotionPlayAnim19 } },
    { 2004, { .call2 = actorMsgPlaceEuler } },
    { 2005, { .call4 = reverseWalkVisibilityMsg } },
    { 2013, { .call3 = reverseWalkStartMsg } },
    { ACTOR_COMMAND_MESSAGE_APPLY, { .call1 = func_actor_350500_80162ABC } },
    { 0x7FFFFFFF, { .call0 = NULL } },
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
    if (Gp_StateF0.actorControl == SCENE_COMBAT_ACTORS_RUNNING) {
        sp.funcs[task->state](task);
    }
}

#include "../../shared/reversing_walker_spawn.inc.c"

/// Exit callback `reverseWalkSpawn` installs; tears the task down.
void reverseWalkExit(Task* arg0)
{
    Gp_EnemyTaskExit(arg0);
}

/// Republishes the work block's two matrices onto `TmdObject::lightMtx` /
/// `colorMtx`, so the actor draws with its own lighting.
void reverseWalkBindLighting(Task* arg0)
{
    TmdObject*       ext;
    Actor350500Work* work;

    ext           = arg0->extra.tmd;
    work          = (Actor350500Work*)arg0->work;
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
    TaskFuncTable4   sp;
    Actor350500Work* work;

    work = (Actor350500Work*)arg0->work;
    sp   = D_actor_350500_80161E30;
    sp.funcs[(s16)work->walk.motionStep](arg0);
}

#include "../../shared/reversing_walker_face.inc.c"

#include "../../shared/reversing_walker_move.inc.c"

#include "../../shared/reversing_walker_turn.inc.c"

#include "../../shared/actor_motion_play19.inc.c"

#include "../../shared/actor_messages_place_euler.inc.c"

#include "../../shared/reversing_walker_visibility.inc.c"

/// `Gp_DispatchMsg` handler: latches the variant the message's halfword at
/// 0x2 selects into `field_4C4` -- 1 clears it, 2 sets it, anything else
/// leaves it. Always returns 0.
s32 func_actor_350500_80162ABC(Task* task, s32 arg1, ActorCommand* msg)
{
    Actor350500Work* work;

    work = (Actor350500Work*)task->work;
    switch (msg->command) {
        case 1:
            work->field_4C4 = 0;
            break;
        case 2:
            work->field_4C4 = 1;
            break;
    }
    return 0;
}
