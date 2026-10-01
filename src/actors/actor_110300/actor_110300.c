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
extern Actor110300Work* gViewFigureWork;

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

/// Animation source `func_800B3F84` seeds the work block's slots from.
extern u8 gViewFigureAnimSets[];

/// Message table published as `Task::msgTable`: the 0x7D3 and 0x7D5 handlers
/// and a terminator.
// Handler views preserve the signatures used by this TU. The dispatcher
// transports each argument in a word register.
typedef struct {
    s32 id;
    union {
        s32 (*call0)(Task*, s32, AnimationPlayRequest*);
        s32 (*call1)(Task*, s32, s32);
    } handler;
} Actor110300MsgEntry;
STATIC_ASSERT_SIZEOF(Actor110300MsgEntry, 8);

extern Actor110300MsgEntry gViewFigureMessages[];

static void func_actor_110300_80132020(Enemy* enemy, Task* task);

extern TmdSource D_actor_110300_80137AF0;
extern TmdSource D_actor_110300_80137EF8;
void             func_actor_110300_80131F9C(Task*);
void             func_actor_110300_80131FF8(Task*);

TmdBone D_actor_110300_8013234C[20] = {
#include "assets/actor_110300_model_05CD0_skeleton.inc"
};

u32 D_actor_110300_8013261C[20] = {
#include "assets/actor_110300_model_05CD0_partVerts.inc"
};

SVECTOR D_actor_110300_8013266C[360] = {
#include "assets/actor_110300_model_05CD0_verts.inc"
};

SVECTOR D_actor_110300_801331AC[358] = {
#include "assets/actor_110300_model_05CD0_normals.inc"
};

u32 D_actor_110300_80133CDC[3973] = {
#include "assets/actor_110300_model_05CD0_stream.inc"
};

TmdSource D_actor_110300_80137AF0 = {
    0,
    21176,
    6792,
    20,
    D_actor_110300_8013261C,
    D_actor_110300_8013266C,
    D_actor_110300_801331AC,
    D_actor_110300_8013234C,
    D_actor_110300_80133CDC,
};

TmdBone D_actor_110300_80137B14[1] = {
#include "assets/actor_110300_model_060D8_skeleton.inc"
};

u32 D_actor_110300_80137B38[1] = {
#include "assets/actor_110300_model_060D8_partVerts.inc"
};

SVECTOR D_actor_110300_80137B3C[22] = {
#include "assets/actor_110300_model_060D8_verts.inc"
};

SVECTOR D_actor_110300_80137BEC[20] = {
#include "assets/actor_110300_model_060D8_normals.inc"
};

u32 D_actor_110300_80137C8C[155] = {
#include "assets/actor_110300_model_060D8_stream.inc"
};

TmdSource D_actor_110300_80137EF8 = {
    0,
    1056,
    0,
    1,
    D_actor_110300_80137B38,
    D_actor_110300_80137B3C,
    D_actor_110300_80137BEC,
    D_actor_110300_80137B14,
    D_actor_110300_80137C8C,
};

AnimationPackedPose D_actor_110300_80137F1C[5] = {
#include "assets/actor_110300_animation_0639C_bank1.inc"
};

AnimationPackedRotation D_actor_110300_80137F58[37] = {
#include "assets/actor_110300_animation_0639C_bank4.inc"
};

AnimationRecord D_actor_110300_80137FEC[106] = {
#include "assets/actor_110300_animation_0639C_records.inc"
};

u16 D_actor_110300_80138194[20] = {
#include "assets/actor_110300_animation_0639C_indices.inc"
};

AnimationSet D_actor_110300_801381BC = {
    D_actor_110300_80137FEC,
    D_actor_110300_80138194,
    { NULL, D_actor_110300_80137F1C, NULL, NULL, D_actor_110300_80137F58, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_110300_801381E4[22] = {
#include "assets/actor_110300_animation_078F8_bank1.inc"
};

AnimationPackedRotation D_actor_110300_801382EC[406] = {
#include "assets/actor_110300_animation_078F8_bank4.inc"
};

AnimationRecord D_actor_110300_80138944[875] = {
#include "assets/actor_110300_animation_078F8_records.inc"
};

u16 D_actor_110300_801396F0[20] = {
#include "assets/actor_110300_animation_078F8_indices.inc"
};

AnimationSet D_actor_110300_80139718 = {
    D_actor_110300_80138944,
    D_actor_110300_801396F0,
    { NULL, D_actor_110300_801381E4, NULL, NULL, D_actor_110300_801382EC, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_110300_80139740[7] = {
#include "assets/actor_110300_animation_07D58_bank1.inc"
};

AnimationPackedRotation D_actor_110300_80139794[96] = {
#include "assets/actor_110300_animation_07D58_bank4.inc"
};

AnimationRecord D_actor_110300_80139914[143] = {
#include "assets/actor_110300_animation_07D58_records.inc"
};

u16 D_actor_110300_80139B50[20] = {
#include "assets/actor_110300_animation_07D58_indices.inc"
};

AnimationSet D_actor_110300_80139B78 = {
    D_actor_110300_80139914,
    D_actor_110300_80139B50,
    { NULL, D_actor_110300_80139740, NULL, NULL, D_actor_110300_80139794, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_110300_80139BA0[4] = {
#include "assets/actor_110300_animation_07FE4_bank1.inc"
};

AnimationPackedRotation D_actor_110300_80139BD0[39] = {
#include "assets/actor_110300_animation_07FE4_bank4.inc"
};

AnimationRecord D_actor_110300_80139C6C[92] = {
#include "assets/actor_110300_animation_07FE4_records.inc"
};

u16 D_actor_110300_80139DDC[20] = {
#include "assets/actor_110300_animation_07FE4_indices.inc"
};

AnimationSet D_actor_110300_80139E04 = {
    D_actor_110300_80139C6C,
    D_actor_110300_80139DDC,
    { NULL, D_actor_110300_80139BA0, NULL, NULL, D_actor_110300_80139BD0, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_110300_80139E2C[3] = {
#include "assets/actor_110300_animation_0820C_bank1.inc"
};

AnimationPackedRotation D_actor_110300_80139E50[36] = {
#include "assets/actor_110300_animation_0820C_bank4.inc"
};

AnimationRecord D_actor_110300_80139EE0[73] = {
#include "assets/actor_110300_animation_0820C_records.inc"
};

u16 D_actor_110300_8013A004[20] = {
#include "assets/actor_110300_animation_0820C_indices.inc"
};

AnimationSet D_actor_110300_8013A02C = {
    D_actor_110300_80139EE0,
    D_actor_110300_8013A004,
    { NULL, D_actor_110300_80139E2C, NULL, NULL, D_actor_110300_80139E50, NULL, NULL, NULL },
};

Actor110300MsgEntry gViewFigureMessages[3] = {
    { 2003, { .call0 = viewFigurePlayMessage } },
    { 2005, { .call1 = actorMsgSetPairVisibility } },
    { TASK_MESSAGE_TABLE_END, { .call0 = NULL } },
};

TaskDesc gViewFigureTasks[2] = {
    { { { TASK_BODY_TMD, 192 } }, func_actor_110300_80131F9C, { .model = &D_actor_110300_80137AF0 } },
    { { { TASK_BODY_TMD, 192 } }, func_actor_110300_80131FF8, { .model = &D_actor_110300_80137EF8 } },
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

Actor110300Work* gViewFigureWork;

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
    Gp_DestroyEnemy(arg0->spawnArg2.pointer, arg0);
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
