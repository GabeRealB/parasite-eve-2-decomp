#include "actors/actor_202900.h"

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

/// Work block of the overlay's actor, allocated zeroed by its setup handler
/// and reached through `D_actor_202900_80156E54`, which the actor's update
/// refreshes from the task every frame: the light and colour matrices the
/// model is drawn under, its nineteen-part rig and its animation state, in
/// which `st.field_8` latches the second slot's record once it reaches 0x15,
/// so the overlay reacts to that record once.
typedef struct Actor202900Work {
    MATRIX          light;
    MATRIX          color;
    ActorAnimRig19  rig;
    ActorEnemyState st;
    byte            pad_4B4[0xB0];
} Actor202900Work;
STATIC_ASSERT_SIZEOF(Actor202900Work, 0x564);

// Message-table callbacks use the argument views required by this TU.
typedef struct {
    s32 id;
    union {
        s32 (*call0)(Task*, s32, AnimationPlayRequest*);
        s32 (*call1)(Task*, s32, s32);
    } handler;
} Actor202900MessageEntry;
STATIC_ASSERT_SIZEOF(Actor202900MessageEntry, 8);

extern Actor202900MessageEntry D_actor_202900_80156E0C[3];
extern TaskDesc                D_actor_202900_80156E24[];
extern u8                      D_actor_202900_80156E3C[];

/// The actor's work block, published so the overlay's functions can reach it
/// without the task in hand.
extern Actor202900Work* D_actor_202900_80156E54;

/// The actor's task, published by the setup handler so the overlay's other
/// functions can reach the actor's model without the task in hand.
extern Task* D_actor_202900_80156E58;

/// The second task the setup handler starts. Its model is textured from the
/// area record the actor was placed from, shown and hidden together with the
/// actor's, and the task is killed when the actor's exit callback runs.
extern Task* D_actor_202900_80156E5C;

static void func_actor_202900_8014A0B4(GpEnemy* enemy, Task* task);
static void func_actor_202900_8014A158(Task* arg0);
static void func_actor_202900_8014A194(Task* arg0);
static void func_actor_202900_8014A208(void);
static void func_actor_202900_8014A260(void);
static void func_actor_202900_8014A304(void);
static s32  func_actor_202900_8014A394(void);

extern TmdSource D_actor_202900_801559A8;
extern TmdSource D_actor_202900_80155B48;
void             func_actor_202900_8014A02C(Task*);
void             func_actor_202900_8014A088(Task*);

s32 func_actor_202900_8014A3E0(Task*, s32, AnimationPlayRequest*);
s32 func_actor_202900_8014A440(Task*, s32, s32);

AnimationPackedPose D_actor_202900_8014A4AC[158] = {
#include "assets/actor_202900_animation_06098_bank1.inc"
};

AnimationPackedRotation D_actor_202900_8014AC14[2161] = {
#include "assets/actor_202900_animation_06098_bank4.inc"
};

AnimationRecord D_actor_202900_8014CDD8[3118] = {
#include "assets/actor_202900_animation_06098_records.inc"
};

u16 D_actor_202900_8014FE90[20] = {
#include "assets/actor_202900_animation_06098_indices.inc"
};

AnimationSet D_actor_202900_8014FEB8 = {
    D_actor_202900_8014CDD8,
    D_actor_202900_8014FE90,
    { NULL, D_actor_202900_8014A4AC, NULL, NULL, D_actor_202900_8014AC14, NULL, NULL, NULL },
};

TmdBone D_actor_202900_8014FEE0[19] = {
#include "assets/actor_202900_model_0BB88_skeleton.inc"
};

u32 D_actor_202900_8015018C[19] = {
#include "assets/actor_202900_model_0BB88_partVerts.inc"
};

SVECTOR D_actor_202900_801501D8[377] = {
#include "assets/actor_202900_model_0BB88_verts.inc"
};

SVECTOR D_actor_202900_80150DA0[406] = {
#include "assets/actor_202900_model_0BB88_normals.inc"
};

u32 D_actor_202900_80151A50[4054] = {
#include "assets/actor_202900_model_0BB88_stream.inc"
};

TmdSource D_actor_202900_801559A8 = {
    0,
    22188,
    6568,
    19,
    D_actor_202900_8015018C,
    D_actor_202900_801501D8,
    D_actor_202900_80150DA0,
    D_actor_202900_8014FEE0,
    D_actor_202900_80151A50,
};

TmdBone D_actor_202900_801559CC[1] = {
#include "assets/actor_202900_model_0BD28_skeleton.inc"
};

u32 D_actor_202900_801559F0[1] = {
#include "assets/actor_202900_model_0BD28_partVerts.inc"
};

SVECTOR D_actor_202900_801559F4[6] = {
#include "assets/actor_202900_model_0BD28_verts.inc"
};

SVECTOR D_actor_202900_80155A24[8] = {
#include "assets/actor_202900_model_0BD28_normals.inc"
};

u32 D_actor_202900_80155A64[57] = {
#include "assets/actor_202900_model_0BD28_stream.inc"
};

TmdSource D_actor_202900_80155B48 = {
    0,
    320,
    0,
    1,
    D_actor_202900_801559F0,
    D_actor_202900_801559F4,
    D_actor_202900_80155A24,
    D_actor_202900_801559CC,
    D_actor_202900_80155A64,
};

AnimationPackedPose D_actor_202900_80155B6C[25] = {
#include "assets/actor_202900_animation_0CDB4_bank1.inc"
};

AnimationPackedRotation D_actor_202900_80155C98[427] = {
#include "assets/actor_202900_animation_0CDB4_bank4.inc"
};

AnimationRecord D_actor_202900_80156344[538] = {
#include "assets/actor_202900_animation_0CDB4_records.inc"
};

u16 D_actor_202900_80156BAC[20] = {
#include "assets/actor_202900_animation_0CDB4_indices.inc"
};

AnimationSet D_actor_202900_80156BD4 = {
    D_actor_202900_80156344,
    D_actor_202900_80156BAC,
    { NULL, D_actor_202900_80155B6C, NULL, NULL, D_actor_202900_80155C98, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_202900_80156BFC[2] = {
#include "assets/actor_202900_animation_0CFC4_bank1.inc"
};

AnimationPackedRotation D_actor_202900_80156C14[33] = {
#include "assets/actor_202900_animation_0CFC4_bank4.inc"
};

AnimationRecord D_actor_202900_80156C98[73] = {
#include "assets/actor_202900_animation_0CFC4_records.inc"
};

u16 D_actor_202900_80156DBC[20] = {
#include "assets/actor_202900_animation_0CFC4_indices.inc"
};

AnimationSet D_actor_202900_80156DE4 = {
    D_actor_202900_80156C98,
    D_actor_202900_80156DBC,
    { NULL, D_actor_202900_80156BFC, NULL, NULL, D_actor_202900_80156C14, NULL, NULL, NULL },
};

Actor202900MessageEntry D_actor_202900_80156E0C[3] = {
    { 2003, { .call0 = func_actor_202900_8014A3E0 } },
    { 2005, { .call1 = func_actor_202900_8014A440 } },
    { 0x7FFFFFFF, { .call0 = NULL } },
};

TaskDesc D_actor_202900_80156E24[2] = {
    { TASK_BODY_TMD, 192, func_actor_202900_8014A02C, { .model = &D_actor_202900_801559A8 } },
    { TASK_BODY_TMD, 192, func_actor_202900_8014A088, { .model = &D_actor_202900_80155B48 } },
};

u8 D_actor_202900_80156E3C[24] = {
    0,
    0,
    0,
    0,
    212,
    107,
    21,
    128,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    228,
    109,
    21,
    128,
    0,
    0,
    0,
    0,
};

Actor202900Work* D_actor_202900_80156E54 = NULL;

Task* D_actor_202900_80156E58;

Task* D_actor_202900_80156E5C;

static void func_actor_202900_80149E24(GpEnemy* enemy, Task* task);

/// Setup handler, state 0 of the actor's update: allocates and publishes the
/// work block, starts the second task and textures its model from the area
/// record the actor was placed from, then seeds the animation context and runs
/// the first step body.
static void func_actor_202900_80149E24(GpEnemy* enemy, Task* task)
{
    VECTOR     vec;
    GfxCoord*  coord;
    TmdObject* obj;

    obj        = task->extra.tmd;
    coord      = obj->coords;
    task->work = (D_actor_202900_80156E54 = memCalloc(0x564, false));
    if (D_actor_202900_80156E54 == NULL) {
        Gp_DestroyEnemy(enemy, task);
        return;
    }
    task->exitCallback               = func_actor_202900_8014A158;
    coord->parent                    = &gGfxViewCoord;
    enemy->field_4                   = &coord->coord;
    enemy->field_48                  = 0;
    enemy->node.state.parts.targeted = 0;
    enemy->node.state.parts.flags    = WORLD_TARGET_NOT_LOCKABLE;
    obj->otOffset                    = 1;
    obj->flags                       = 0;
    D_actor_202900_80156E58          = task;
    D_actor_202900_80156E5C          = Task_SpawnFromTable(D_actor_202900_80156E24, 1, 0, 0);
    actorTintTask(D_actor_202900_80156E5C, enemy);
    obj->lightMtx = &D_actor_202900_80156E54->light;
    obj->colorMtx = &D_actor_202900_80156E54->color;
    vec.vx        = coord->workm.t[0];
    vec.vy        = coord->workm.t[1] - 0x320;
    vec.vz        = coord->workm.t[2];
    func_800D7A9C(obj, &vec, 0, 3);
    Gp_AnimInitCtx(&D_actor_202900_80156E54->rig.anim, D_actor_202900_80156E3C, obj,
                   D_actor_202900_80156E54->rig.poses);
    D_actor_202900_80156E54->st.animId  = 4;
    D_actor_202900_80156E54->st.state   = 2;
    D_actor_202900_80156E54->st.field_8 = 0;
    task->msgTable                      = D_actor_202900_80156E0C;
    func_actor_202900_8014A194(task);
    task->state++;
}

/// Update of the actor's task: publishes the task's work block in
/// `D_actor_202900_80156E54`, so the overlay's other functions can reach it
/// without the task in hand, then dispatches on the task's state to the setup
/// handler `func_actor_202900_80149E24` (state 0) or the per-frame handler
/// `func_actor_202900_8014A0B4` (state 1), passing the task's enemy record
/// along with the task.
void func_actor_202900_8014A02C(Task* task)
{
    void (*fns[2])(GpEnemy*, Task*) = {
        func_actor_202900_80149E24,
        func_actor_202900_8014A0B4,
    };

    D_actor_202900_80156E54 = (Actor202900Work*)task->work;
    fns[task->state](task->spawnArg2.pointer, task);
}

/// Update of the second task: parents its model to the fifth coordinate of the
/// actor's model, marks the coordinate for recomputation and shows the model.
void func_actor_202900_8014A088(Task* arg0)
{
    GfxCoord*  parent;
    GfxCoord*  coord;
    TmdObject* extra;

    extra               = arg0->extra.tmd;
    parent              = D_actor_202900_80156E58->extra.tmd->coords;
    coord               = extra->coords;
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
    extra->flags        = 0;
    coord->parent       = parent + 4;
}

/// Per-frame handler, state 1 of the actor's update: passes the model and a
/// point 0x320 above its origin to `func_800D7A9C`, runs the step dispatcher,
/// and while animation 1 plays enqueues a sound event each time the second
/// animation slot reaches frame 0x15.
static void func_actor_202900_8014A0B4(GpEnemy* enemy, Task* task)
{
    TmdObject* obj;
    GfxCoord*  coord;
    VECTOR     pos;

    obj    = task->extra.tmd;
    coord  = obj->coords;
    pos.vx = coord->workm.t[0];
    pos.vy = coord->workm.t[1] - 0x320;
    pos.vz = coord->workm.t[2];
    func_800D7A9C(obj, &pos, 0, 3);
    func_actor_202900_8014A194(task);
    if ((s16)D_actor_202900_80156E54->st.animId == 1 && (func_actor_202900_8014A394() & 0xFF)) {
        SndEvt_EnqueueType6(0x5104000D, 0, 0);
    }
}

/// Exit callback: kills the second task and destroys the enemy.
static void func_actor_202900_8014A158(Task* arg0)
{
    taskKill(D_actor_202900_80156E5C);
    Gp_DestroyEnemy(arg0->spawnArg2.pointer, arg0);
}

/// Runs the body the actor's step selects and then leaves it in step 3, the
/// running state. Steps 1 and 2 each return through their own copy of the
/// advance; the two are identical, so jump.c cross-jumps them and only the
/// second survives. `arg0` is handed the actor but the body ignores it: it
/// reaches the work block through the global, like the overlay's other
/// functions.
static void func_actor_202900_8014A194(Task* arg0)
{
    if (D_actor_202900_80156E54->st.state == 1) {
        func_actor_202900_8014A304();
        D_actor_202900_80156E54->st.state = 3;
        return;
    }
    if (D_actor_202900_80156E54->st.state == 2) {
        func_actor_202900_8014A260();
        D_actor_202900_80156E54->st.state = 3;
        return;
    }
    if (D_actor_202900_80156E54->st.state == 3) {
        func_actor_202900_8014A208();
    }
}

/// Ticks animation slots 1..0x12 of the actor's animation context.
static void func_actor_202900_8014A208(void)
{
    s32 i;

    i = 1;
    do {
        Gp_AnimTickSlot(&D_actor_202900_80156E54->rig.anim, &D_actor_202900_80156E54->rig.slots[i]);
        i++;
    } while (i < 0x13);
}

/// Reseeds animation slots 1..0x12 from `animId`, forcing each slot's set
/// index to 1 first, and latches that id into `st.appliedAnimId` as the one now
/// playing.
///
/// The third argument is the loop counter itself, and the two scaled induction variables are the
/// compiler's own, not a pair of source level pointers.
static void func_actor_202900_8014A260(void)
{
    s32 i;

    i = 1;
    do {
        D_actor_202900_80156E54->rig.slots[i].rate = 1;
        Gp_AnimInitSlot(&D_actor_202900_80156E54->rig.anim, &D_actor_202900_80156E54->rig.slots[i], i,
                        (s16)D_actor_202900_80156E54->st.animId);
        i++;
    } while (i < 0x13);
    D_actor_202900_80156E54->st.appliedAnimId = D_actor_202900_80156E54->st.animId;
}

/// Reseeds animation slots 1..0x12 from `animId` and latches that id into
/// `st.appliedAnimId` as the one now playing.
///
/// The third argument is the loop counter itself. Giving the call its own
/// counter copy (as m2c does) makes the preheader's `a2` initialisation a
/// separate pseudo, and the scheduler then orders the prologue saves around it
/// instead of leaving each `sw` paired with the load that overwrites it.
static void func_actor_202900_8014A304(void)
{
    s32 i;

    i = 1;
    do {
        func_800B3AA4(&D_actor_202900_80156E54->rig.anim, &D_actor_202900_80156E54->rig.slots[i], i,
                      (s16)D_actor_202900_80156E54->st.animId, 0, 8);
        i++;
    } while (i < 0x13);
    D_actor_202900_80156E54->st.appliedAnimId = D_actor_202900_80156E54->st.animId;
}

/// Watches the second animation slot for the frame the overlay reacts to:
/// while it holds 0x15, records it in `st.field_8` and reports whether that is
/// a change.
///
/// The mask is written at each use rather than hoisted into a `u16` local.
/// Hoisting makes the local a copy of the masked word, and combine then folds
/// the compare's zero-extension into a `move`; masking where the value is read
/// keeps the `andi $a1,$a0,0xffff`.
static s32 func_actor_202900_8014A394(void)
{
    u16 frame;

    frame = D_actor_202900_80156E54->rig.slots[1].currentPose.indices.recordIndex;
    if ((frame & ANIMATION_POSE_CUE_INDEX_MASK) == 0x15) {
        if (D_actor_202900_80156E54->st.field_8 != (frame & ANIMATION_POSE_CUE_INDEX_MASK)) {
            D_actor_202900_80156E54->st.field_8 = frame & ANIMATION_POSE_CUE_INDEX_MASK;
            return 1;
        }
        D_actor_202900_80156E54->st.field_8 = frame & ANIMATION_POSE_CUE_INDEX_MASK;
    }
    return 0;
}

/// Message 0x7D3 handler, animation start: seeds the work block's `animId` with the requested
/// one, rejecting anything from 5 up, and leaves the actor in step 2 with
/// `st.field_6` cleared before running the step dispatcher.
///
/// The actor is read into a local between the first two stores on purpose: that
/// is where the original evaluates it, and it is what puts the global's
/// `lui`/`lw` ahead of the `li 2` and leaves the `st.field_6` clear for the
/// call's delay slot.
s32 func_actor_202900_8014A3E0(Task* task, s32 arg1, AnimationPlayRequest* args)
{
    Task* actor;

    if (args->animationId < 5) {
        D_actor_202900_80156E54->st.animId  = args->animationId;
        actor                               = D_actor_202900_80156E58;
        D_actor_202900_80156E54->st.state   = 2;
        D_actor_202900_80156E54->st.field_6 = 0;
        func_actor_202900_8014A194(actor);
        return 0;
    }
    return -1;
}

/// Message 0x7D5 handler: applies the draw-state flags to the actor's model and
/// to the second task's model alike. Bit 0 set shows both, clear hides them
/// (flag 0x80); bit 1 also sets flag 0x4 on both.
///
/// The handler is declared with the message arguments it does not read so the
/// flags arrive in `$a2` as they do for every other handler: with a single
/// parameter the compiler copies the incoming `$a0` into the pseudo global
/// allocation gave `$a2`, one instruction the target does not have.
s32 func_actor_202900_8014A440(Task* task, s32 arg1, s32 flags)
{
    TmdObject* actorModel;
    TmdObject* taskModel;

    actorModel = D_actor_202900_80156E58->extra.tmd;
    taskModel  = D_actor_202900_80156E5C->extra.tmd;
    if (flags & 1) {
        actorModel->flags = 0;
        taskModel->flags  = 0;
    } else {
        actorModel->flags = TMD_OBJECT_SKIP_ACTIVE_DRAW;
        taskModel->flags  = TMD_OBJECT_SKIP_ACTIVE_DRAW;
    }
    if (flags & 2) {
        actorModel->flags |= TMD_OBJECT_SKIP_AUTO_BUFFER;
        taskModel->flags  |= TMD_OBJECT_SKIP_AUTO_BUFFER;
    }
    return 0;
}
