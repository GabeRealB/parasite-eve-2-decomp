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

/// The block above, published by `func_actor_110300_80131F9C` from the task's
/// `Task::work`.
extern Actor110300Work* D_actor_110300_8013A0A0;

/// The actor's own task, stored by the step-0 handler. The message handlers
/// drive the animation step driver and the model through it, and the helper
/// task's entry `func_actor_110300_80131FF8` parents its coordinate to one of
/// its model's nodes.
extern Task* D_actor_110300_8013A0A4;

/// The helper task `Task_SpawnFromTable` returns in the step-0 handler; the
/// visibility handler drives its model alongside the actor's, and the exit
/// callback kills it.
extern Task* D_actor_110300_8013A0A8;

/// Spawn descriptor table the step-0 handler spawns the helper task from:
/// index 0 is the actor's own dispatcher, index 1 the helper.
extern TaskDesc D_actor_110300_8013A06C[];

/// Animation source `func_800B3F84` seeds the work block's slots from.
extern u8 D_actor_110300_8013A084[];

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

extern Actor110300MsgEntry D_actor_110300_8013A054[];

static void func_actor_110300_80132020(GpEnemy* enemy, Task* task);
static void func_actor_110300_80132088(Task* task);
static void func_actor_110300_801320C4(Task* arg0);
static void func_actor_110300_80132138(void);
static void func_actor_110300_80132180(void);
static void func_actor_110300_80132208(void);

extern TmdSource D_actor_110300_80137AF0;
extern TmdSource D_actor_110300_80137EF8;
void             func_actor_110300_80131F9C(Task*);
void             func_actor_110300_80131FF8(Task*);

s32 func_actor_110300_80132280(Task*, s32, AnimationPlayRequest*);
s32 func_actor_110300_801322E0(Task*, s32, s32);

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

Actor110300MsgEntry D_actor_110300_8013A054[3] = {
    { 2003, { .call0 = func_actor_110300_80132280 } },
    { 2005, { .call1 = func_actor_110300_801322E0 } },
    { 0x7FFFFFFF, { .call0 = NULL } },
};

TaskDesc D_actor_110300_8013A06C[2] = {
    { TASK_BODY_TMD, 192, func_actor_110300_80131F9C, { .model = &D_actor_110300_80137AF0 } },
    { TASK_BODY_TMD, 192, func_actor_110300_80131FF8, { .model = &D_actor_110300_80137EF8 } },
};

u8 D_actor_110300_8013A084[28] = {
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

Actor110300Work* D_actor_110300_8013A0A0;

Task* D_actor_110300_8013A0A4;

Task* D_actor_110300_8013A0A8;

static void func_actor_110300_80131E24(GpEnemy* enemy, Task* task);

/// Step 0 of the `func_actor_110300_80131F9C` dispatcher: allocate the work
/// block, publish it, and hand the model's animation context its slot array.
///
/// Every access to the block goes through `D_actor_110300_8013A0A0` rather
/// than the `memCalloc` result, which is why the pointer is reloaded at each
/// use instead of staying in a callee-saved register. The task's message table
/// becomes the one holding the animation-start and visibility handlers.
static void func_actor_110300_80131E24(GpEnemy* enemy, Task* task)
{
    VECTOR     vec;
    void*      work;
    TmdObject* obj;
    GfxCoord*  coord;

    obj                     = task->extra.tmd;
    coord                   = obj->coords;
    work                    = memCalloc(sizeof(Actor110300Work), 0);
    D_actor_110300_8013A0A0 = work;
    task->work              = work;
    if (work == NULL) {
        Gp_DestroyEnemy(enemy, task);
        return;
    }
    task->exitCallback           = func_actor_110300_80132088;
    coord->parent                = &gGfxViewCoord;
    enemy->field_4               = &coord->coord;
    enemy->node.state.b.flags    = 1;
    enemy->field_48              = 0;
    enemy->node.state.b.targeted = 0;
    obj->otOffset                = 0;
    coord->composeStamp          = GRAPHICS_COORD_DIRTY;
    D_actor_110300_8013A0A4      = task;
    D_actor_110300_8013A0A8      = Task_SpawnFromTable(D_actor_110300_8013A06C, 1, 0, 0);
    func_800B3F84(&D_actor_110300_8013A0A0->rig.anim, D_actor_110300_8013A084, obj,
                  D_actor_110300_8013A0A0->rig.poses, D_actor_110300_8013A0A0->rig.slots);
    D_actor_110300_8013A0A0->st.animId = 1;
    D_actor_110300_8013A0A0->st.state  = 2;
    func_actor_110300_801320C4(task);
    vec.vx = coord->workm.t[0];
    vec.vy = coord->workm.t[1];
    vec.vz = coord->workm.t[2];
    func_800D7A9C(obj, &vec, 0, 3);
    D_actor_110300_8013A0A0->st.field_6++;
    task->msgTable = D_actor_110300_8013A054;
    task->state++;
}

/// The actor's task entry: a two-state dispatcher whose handler table is built
/// on the stack. It publishes the task's work block in
/// `D_actor_110300_8013A0A0` before calling the handler, which is how the
/// overlay's other functions reach the block without the task.
void func_actor_110300_80131F9C(Task* task)
{
    void (*fns[2])(GpEnemy*, Task*) = {
        func_actor_110300_80131E24,
        func_actor_110300_80132020,
    };

    D_actor_110300_8013A0A0 = task->work;
    fns[task->state](task->spawnArg2.pointer, task);
}

/// Entry of the helper task: parents the given task's model root to node 8 of
/// the actor's model.
void func_actor_110300_80131FF8(Task* arg0)
{
    GfxCoord* parent;
    GfxCoord* coord;

    parent              = D_actor_110300_8013A0A4->extra.tmd->coords;
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
/// is copied into `$a0` (the first, unused, is the `GpEnemy*`): that copy is
/// what the first call's argument, and the `Task::extra` load feeding it, are
/// both read off.
static void func_actor_110300_80132020(GpEnemy* enemy, Task* task)
{
    TmdObject* obj;
    GfxCoord*  coord;
    VECTOR     vec;

    obj   = task->extra.tmd;
    coord = obj->coords;
    func_actor_110300_801320C4(task);
    vec.vx = coord->workm.t[0];
    vec.vy = coord->workm.t[1];
    vec.vz = coord->workm.t[2];
    func_800D7A9C(obj, &vec, 0, 3);
}

/// Exit callback the step-0 handler installs: kills the helper task, then
/// destroys the actor.
static void func_actor_110300_80132088(Task* arg0)
{
    taskKill(D_actor_110300_8013A0A8);
    Gp_DestroyEnemy(arg0->spawnArg2.pointer, arg0);
}

/// Advances the animation per the work block's `st.state`: step 1 reseeds the
/// slots through `func_800B4114`, step 2 resets them outright, and either moves
/// on to step 3, which ticks them. The argument is never read.
static void func_actor_110300_801320C4(Task* arg0)
{
    if (D_actor_110300_8013A0A0->st.state == 1) {
        func_actor_110300_80132208();
        D_actor_110300_8013A0A0->st.state = 3;
        return;
    }
    if (D_actor_110300_8013A0A0->st.state == 2) {
        func_actor_110300_80132180();
        D_actor_110300_8013A0A0->st.state = 3;
        return;
    }
    if (D_actor_110300_8013A0A0->st.state == 3) {
        func_actor_110300_80132138();
    }
}

/// Ticks animation slots 1..0x13 of the work block's animation context.
static void func_actor_110300_80132138(void)
{
    s32 i;

    i = 1;
    do {
        Gp_AnimTickIndex(&D_actor_110300_8013A0A0->rig.anim, i);
        i++;
    } while (i < 0x14);
}

/// Sets the rate of animation slots 1..0x13 to 1 and resets each of them to
/// the current animation id, then records that id as the one now playing.
static void func_actor_110300_80132180(void)
{
    s32 i;

    i = 1;
    do {
        D_actor_110300_8013A0A0->rig.slots[i].rate = 1;
        Gp_AnimResetSlot(&D_actor_110300_8013A0A0->rig.anim, i, (s16)D_actor_110300_8013A0A0->st.animId);
        i++;
    } while (i < 0x14);
    D_actor_110300_8013A0A0->st.appliedAnimId = D_actor_110300_8013A0A0->st.animId;
}

/// Reseeds animation slots 1..0x13 of the work block's animation context from
/// the current animation id through `func_800B4114` (arguments 0 and 8), then
/// records that id as the one now playing.
static void func_actor_110300_80132208(void)
{
    s32 i;

    i = 1;
    do {
        func_800B4114(&D_actor_110300_8013A0A0->rig.anim, i, (s16)D_actor_110300_8013A0A0->st.animId, 0, 8);
        i++;
    } while (i < 0x14);
    D_actor_110300_8013A0A0->st.appliedAnimId = D_actor_110300_8013A0A0->st.animId;
}

/// Starts the actor's scripted animation selected by the request.
///
/// Rejects ids 6 and above before changing playback state.
s32 func_actor_110300_80132280(Task* task, s32 arg1, AnimationPlayRequest* args)
{
    Task* actor;

    if (args->animationId < 6) {
        D_actor_110300_8013A0A0->st.animId  = args->animationId;
        actor                               = D_actor_110300_8013A0A4;
        D_actor_110300_8013A0A0->st.state   = 2;
        D_actor_110300_8013A0A0->st.field_6 = 0;
        func_actor_110300_801320C4(actor);
        return 0;
    }
    return -1;
}

/// Message 0x7D5 handler: shows or hides the actor's model and the helper
/// task's together. Bit 0 of `flags` clears both models' `TmdObject::flags`
/// (shown); without it both get 0x80 (hidden). Bit 1 additionally ORs in 0x4
/// on both.
s32 func_actor_110300_801322E0(Task* task, s32 arg1, s32 flags)
{
    TmdObject* self;
    TmdObject* other;

    self  = D_actor_110300_8013A0A4->extra.tmd;
    other = D_actor_110300_8013A0A8->extra.tmd;

    if (flags & 1) {
        self->flags  = 0;
        other->flags = 0;
    } else {
        self->flags  = TMD_OBJECT_SKIP_ACTIVE_DRAW;
        other->flags = TMD_OBJECT_SKIP_ACTIVE_DRAW;
    }

    if (flags & 2) {
        self->flags  |= TMD_OBJECT_SKIP_AUTO_BUFFER;
        other->flags |= TMD_OBJECT_SKIP_AUTO_BUFFER;
    }
    return 0;
}
