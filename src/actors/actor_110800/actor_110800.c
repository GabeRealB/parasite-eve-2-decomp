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

/// The block above, published by `func_actor_110800_801322A0` from the task's
/// `Task::work`.
extern Actor110300Work* D_actor_110800_80139F10;

/// The actor's own task, stored by the step-0 handler. The message handlers
/// drive the step dispatcher and the model through it, and
/// `func_actor_110800_801322FC` parents a coordinate to one of its model's
/// nodes.
extern Task* gActorSelfTask;

/// The helper task `Task_SpawnFromTable` returns in the step-0 handler; the
/// visibility handler drives its model alongside the actor's, and the exit
/// callback kills it.
extern Task* gActorHelperTask;

/// Spawn descriptor table the step-0 handler spawns the helper task from.
extern TaskDesc D_actor_110800_80139EDC[];

/// Animation source `func_800B3F84` seeds the work block's slots from.
extern u8 D_actor_110800_80139EF4[];

/// Message table published as `Task::msgTable`: the 0x7D3 and 0x7D5 handlers
/// below and a terminator.
// Handler views preserve the signatures used by this TU. The dispatcher
// transports each argument in a word register.
typedef struct {
    s32 id;
    union {
        s32 (*call0)(Task*, s32, AnimationPlayRequest*);
        s32 (*call1)(Task*, s32, s32);
    } handler;
} Actor110800MsgEntry;
STATIC_ASSERT_SIZEOF(Actor110800MsgEntry, 8);

extern Actor110800MsgEntry D_actor_110800_80139EC4[];

static void func_actor_110800_8013232C(Task* task);
static void func_actor_110800_80132368(Task* task);
static void func_actor_110800_801323DC(void);
static void func_actor_110800_80132424(void);
static void func_actor_110800_801324AC(void);

extern TmdSource D_actor_110800_80137D94;
extern TmdSource D_actor_110800_80138048;
void             func_actor_110800_801322A0(Task*);
void             func_actor_110800_801322FC(Task*);

s32 func_actor_110800_80132524(Task*, s32, AnimationPlayRequest*);

TmdBone D_actor_110800_801325F0[20] = {
#include "assets/actor_110800_model_05F74_skeleton.inc"
};

u32 D_actor_110800_801328C0[20] = {
#include "assets/actor_110800_model_05F74_partVerts.inc"
};

SVECTOR D_actor_110800_80132910[360] = {
#include "assets/actor_110800_model_05F74_verts.inc"
};

SVECTOR D_actor_110800_80133450[358] = {
#include "assets/actor_110800_model_05F74_normals.inc"
};

u32 D_actor_110800_80133F80[3973] = {
#include "assets/actor_110800_model_05F74_stream.inc"
};

TmdSource D_actor_110800_80137D94 = {
    0,
    21176,
    6792,
    20,
    D_actor_110800_801328C0,
    D_actor_110800_80132910,
    D_actor_110800_80133450,
    D_actor_110800_801325F0,
    D_actor_110800_80133F80,
};

TmdBone D_actor_110800_80137DB8[1] = {
#include "assets/actor_110800_model_06228_skeleton.inc"
};

u32 D_actor_110800_80137DDC[1] = {
#include "assets/actor_110800_model_06228_partVerts.inc"
};

SVECTOR D_actor_110800_80137DE0[14] = {
#include "assets/actor_110800_model_06228_verts.inc"
};

SVECTOR D_actor_110800_80137E50[14] = {
#include "assets/actor_110800_model_06228_normals.inc"
};

u32 D_actor_110800_80137EC0[98] = {
#include "assets/actor_110800_model_06228_stream.inc"
};

TmdSource D_actor_110800_80138048 = {
    0,
    652,
    0,
    1,
    D_actor_110800_80137DDC,
    D_actor_110800_80137DE0,
    D_actor_110800_80137E50,
    D_actor_110800_80137DB8,
    D_actor_110800_80137EC0,
};

AnimationPackedPose D_actor_110800_8013806C[10] = {
#include "assets/actor_110800_animation_066F0_bank1.inc"
};

AnimationPackedRotation D_actor_110800_801380E4[108] = {
#include "assets/actor_110800_animation_066F0_bank4.inc"
};

AnimationRecord D_actor_110800_80138294[149] = {
#include "assets/actor_110800_animation_066F0_records.inc"
};

u16 D_actor_110800_801384E8[20] = {
#include "assets/actor_110800_animation_066F0_indices.inc"
};

AnimationSet D_actor_110800_80138510 = {
    D_actor_110800_80138294,
    D_actor_110800_801384E8,
    { NULL, D_actor_110800_8013806C, NULL, NULL, D_actor_110800_801380E4, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_110800_80138538[8] = {
#include "assets/actor_110800_animation_06A9C_bank1.inc"
};

AnimationPackedRotation D_actor_110800_80138598[76] = {
#include "assets/actor_110800_animation_06A9C_bank4.inc"
};

AnimationRecord D_actor_110800_801386C8[115] = {
#include "assets/actor_110800_animation_06A9C_records.inc"
};

u16 D_actor_110800_80138894[20] = {
#include "assets/actor_110800_animation_06A9C_indices.inc"
};

AnimationSet D_actor_110800_801388BC = {
    D_actor_110800_801386C8,
    D_actor_110800_80138894,
    { NULL, D_actor_110800_80138538, NULL, NULL, D_actor_110800_80138598, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_110800_801388E4[15] = {
#include "assets/actor_110800_animation_07050_bank1.inc"
};

AnimationPackedRotation D_actor_110800_80138998[128] = {
#include "assets/actor_110800_animation_07050_bank4.inc"
};

AnimationRecord D_actor_110800_80138B98[172] = {
#include "assets/actor_110800_animation_07050_records.inc"
};

u16 D_actor_110800_80138E48[20] = {
#include "assets/actor_110800_animation_07050_indices.inc"
};

AnimationSet D_actor_110800_80138E70 = {
    D_actor_110800_80138B98,
    D_actor_110800_80138E48,
    { NULL, D_actor_110800_801388E4, NULL, NULL, D_actor_110800_80138998, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_110800_80138E98[11] = {
#include "assets/actor_110800_animation_0770C_bank1.inc"
};

AnimationPackedRotation D_actor_110800_80138F1C[170] = {
#include "assets/actor_110800_animation_0770C_bank4.inc"
};

AnimationRecord D_actor_110800_801391C4[208] = {
#include "assets/actor_110800_animation_0770C_records.inc"
};

u16 D_actor_110800_80139504[20] = {
#include "assets/actor_110800_animation_0770C_indices.inc"
};

AnimationSet D_actor_110800_8013952C = {
    D_actor_110800_801391C4,
    D_actor_110800_80139504,
    { NULL, D_actor_110800_80138E98, NULL, NULL, D_actor_110800_80138F1C, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_110800_80139554[25] = {
#include "assets/actor_110800_animation_0807C_bank1.inc"
};

AnimationPackedRotation D_actor_110800_80139680[215] = {
#include "assets/actor_110800_animation_0807C_bank4.inc"
};

AnimationRecord D_actor_110800_801399DC[294] = {
#include "assets/actor_110800_animation_0807C_records.inc"
};

u16 D_actor_110800_80139E74[20] = {
#include "assets/actor_110800_animation_0807C_indices.inc"
};

AnimationSet D_actor_110800_80139E9C = {
    D_actor_110800_801399DC,
    D_actor_110800_80139E74,
    { NULL, D_actor_110800_80139554, NULL, NULL, D_actor_110800_80139680, NULL, NULL, NULL },
};

Actor110800MsgEntry D_actor_110800_80139EC4[3] = {
    { 2003, { .call0 = func_actor_110800_80132524 } },
    { 2005, { .call1 = actorMsgSetPairVisibility } },
    { 0x7FFFFFFF, { .call0 = NULL } },
};

TaskDesc D_actor_110800_80139EDC[2] = {
    { TASK_BODY_TMD, 192, func_actor_110800_801322A0, { .model = &D_actor_110800_80137D94 } },
    { TASK_BODY_TMD, 192, func_actor_110800_801322FC, { .model = &D_actor_110800_80138048 } },
};

u8 D_actor_110800_80139EF4[28] = {
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

Actor110300Work* D_actor_110800_80139F10;

Task* gActorSelfTask;

Task* gActorHelperTask;

static void func_actor_110800_80131E24(Enemy* enemy, Task* task);
static void func_actor_110800_80131F9C(Enemy* enemy, Task* task);

/// Step 0 of the `func_actor_110800_801322A0` dispatcher: allocate the work
/// block, publish it, and hand the model's animation context its slot array.
///
/// Every access to the block goes through `D_actor_110800_80139F10` rather
/// than the `memCalloc` result, which is why the pointer is reloaded at each
/// use instead of staying in a callee-saved register. The task's message table
/// becomes the one holding the animation-start and visibility handlers.
static void func_actor_110800_80131E24(Enemy* enemy, Task* task)
{
    VECTOR     vec;
    void*      work;
    TmdObject* obj;
    GfxCoord*  coord;

    obj                     = task->extra.tmd;
    coord                   = obj->coords;
    work                    = memCalloc(sizeof(Actor110300Work), 0);
    D_actor_110800_80139F10 = work;
    task->work              = work;
    if (work == NULL) {
        Gp_DestroyEnemy(enemy, task);
        return;
    }
    task->exitCallback               = func_actor_110800_8013232C;
    coord->parent                    = &gGfxViewCoord;
    enemy->field_4                   = &coord->coord;
    enemy->node.state.parts.flags    = WORLD_TARGET_NOT_LOCKABLE;
    enemy->field_48                  = 0;
    enemy->node.state.parts.targeted = 0;
    obj->otOffset                    = 0;
    coord->composeStamp              = GRAPHICS_COORD_DIRTY;
    gActorSelfTask                   = task;
    gActorHelperTask                 = Task_SpawnFromTable(D_actor_110800_80139EDC, 1, 0, 0);
    func_800B3F84(&D_actor_110800_80139F10->rig.anim, D_actor_110800_80139EF4, obj,
                  D_actor_110800_80139F10->rig.poses, D_actor_110800_80139F10->rig.slots);
    D_actor_110800_80139F10->st.animId = 1;
    D_actor_110800_80139F10->st.state  = 2;
    func_actor_110800_80132368(task);
    vec.vx = coord->workm.t[0];
    vec.vy = coord->workm.t[1];
    vec.vz = coord->workm.t[2];
    func_800D7A9C(obj, &vec, 0, 3);
    D_actor_110800_80139F10->st.field_6++;
    task->msgTable = D_actor_110800_80139EC4;
    task->state++;
}

/// Step 1 of the `func_actor_110800_801322A0` dispatcher, the walk/run
/// footstep cue:
/// run the body the actor's step selects, cue the sound the running
/// animation's frame table asks for, then refresh the model root as step 0 did
/// by feeding its world translation to `func_800D7A9C` (the light solve)
/// against the model object itself.
///
/// The two animation ids this actor plays carry a frame table each: id 4
/// watches slot 19 alone, id 5 watches slot 19 and then slot 16. An entry
/// latches the frame it fired for in `st.field_8`, so a frame that is held over
/// several calls only cues once; the mask is the frame index of the slot's
/// halfword.
///
/// The body reaches the task through the second argument, so the incoming `$a1`
/// is copied into `$a0` (the first, unused, is the `Enemy*`), and the model
/// and its coordinate are read through that copy. `task->extra` is written
/// twice with the coordinate taken through the first read: that leaves cse's
/// load in a temporary and copies it into `obj`, which is the `move` between
/// the two loads the target has.
///
/// The switch reads `animId` signed. The field is unsigned, so the cast is
/// load-bearing: without it the halfword load is `lhu` where the target has
/// `lh`.
static void func_actor_110800_80131F9C(Enemy* enemy, Task* task)
{
    GfxCoord*  coord;
    TmdObject* obj;
    VECTOR     vec;

    coord = task->extra.tmd->coords;
    obj   = task->extra.tmd;
    func_actor_110800_80132368(task);
    switch ((s16)D_actor_110800_80139F10->st.animId) {
        case 4:
            if ((D_actor_110800_80139F10->rig.slots[19].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK) == 0xC8) {
                if (D_actor_110800_80139F10->st.field_8 != (D_actor_110800_80139F10->rig.slots[19].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK)) {
                    SndEvt_EnqueueType6(0x510D0011, 0, 0);
                }
                D_actor_110800_80139F10->st.field_8 = D_actor_110800_80139F10->rig.slots[19].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK;
            }
            if ((D_actor_110800_80139F10->rig.slots[19].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK) == 0xCA) {
                if (D_actor_110800_80139F10->st.field_8 != (D_actor_110800_80139F10->rig.slots[19].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK)) {
                    SndEvt_EnqueueType6(0x510D000D, 0, 0);
                }
                D_actor_110800_80139F10->st.field_8 = D_actor_110800_80139F10->rig.slots[19].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK;
            }
            if ((D_actor_110800_80139F10->rig.slots[19].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK) == 0xCD) {
                if (D_actor_110800_80139F10->st.field_8 != (D_actor_110800_80139F10->rig.slots[19].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK)) {
                    SndEvt_EnqueueType6(0x510D000E, 0, 0);
                }
                D_actor_110800_80139F10->st.field_8 = D_actor_110800_80139F10->rig.slots[19].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK;
            }
            break;
        case 5:
            if ((D_actor_110800_80139F10->rig.slots[19].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK) == 0x115) {
                if (D_actor_110800_80139F10->st.field_8 != (D_actor_110800_80139F10->rig.slots[19].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK)) {
                    SndEvt_EnqueueType6(0x510D000F, 0, 0);
                }
                D_actor_110800_80139F10->st.field_8 = D_actor_110800_80139F10->rig.slots[19].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK;
            }
            if ((D_actor_110800_80139F10->rig.slots[19].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK) == 0x11F) {
                if (D_actor_110800_80139F10->st.field_8 != (D_actor_110800_80139F10->rig.slots[19].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK)) {
                    SndEvt_EnqueueType6(0x510D000F, 0, 0);
                }
                D_actor_110800_80139F10->st.field_8 = D_actor_110800_80139F10->rig.slots[19].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK;
            }
            if ((D_actor_110800_80139F10->rig.slots[16].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK) == 0xCE) {
                if (D_actor_110800_80139F10->st.field_8 != (D_actor_110800_80139F10->rig.slots[16].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK)) {
                    SndEvt_EnqueueType6(0x510D0010, 0, 0);
                }
                D_actor_110800_80139F10->st.field_8 = D_actor_110800_80139F10->rig.slots[16].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK;
            }
            if ((D_actor_110800_80139F10->rig.slots[16].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK) == 0xD8) {
                if (D_actor_110800_80139F10->st.field_8 != (D_actor_110800_80139F10->rig.slots[16].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK)) {
                    SndEvt_EnqueueType6(0x510D0010, 0, 0);
                }
                D_actor_110800_80139F10->st.field_8 = D_actor_110800_80139F10->rig.slots[16].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK;
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
/// `D_actor_110800_80139F10` before calling the handler, which is how the
/// overlay's other functions reach the block without the task.
void func_actor_110800_801322A0(Task* task)
{
    void (*fns[2])(Enemy*, Task*) = {
        func_actor_110800_80131E24,
        func_actor_110800_80131F9C,
    };

    D_actor_110800_80139F10 = task->work;
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
static void func_actor_110800_8013232C(Task* arg0)
{
    taskKill(gActorHelperTask);
    Gp_DestroyEnemy(arg0->spawnArg2.pointer, arg0);
}

/// Advances the animation per the work block's `st.state`: step 1 reseeds the
/// slots through `func_800B4114`, step 2 resets them outright, and either moves
/// on to step 3, which ticks them. The argument is never read.
static void func_actor_110800_80132368(Task* task)
{
    if (D_actor_110800_80139F10->st.state == 1) {
        func_actor_110800_801324AC();
        D_actor_110800_80139F10->st.state = 3;
        return;
    }
    if (D_actor_110800_80139F10->st.state == 2) {
        func_actor_110800_80132424();
        D_actor_110800_80139F10->st.state = 3;
        return;
    }
    if (D_actor_110800_80139F10->st.state == 3) {
        func_actor_110800_801323DC();
    }
}

/// Ticks animation slots 1..0x13 of the work block's animation context.
static void func_actor_110800_801323DC(void)
{
    s32 i;

    i = 1;
    do {
        Gp_AnimTickIndex(&D_actor_110800_80139F10->rig.anim, i);
        i++;
    } while (i < 0x14);
}

/// Sets the rate of animation slots 1..0x13 to 1 and resets each of them to
/// the current animation id, then records that id as the one now playing.
static void func_actor_110800_80132424(void)
{
    s32 i;

    i = 1;
    do {
        D_actor_110800_80139F10->rig.slots[i].rate = 1;
        Gp_AnimResetSlot(&D_actor_110800_80139F10->rig.anim, i, (s16)D_actor_110800_80139F10->st.animId);
        i++;
    } while (i < 0x14);
    D_actor_110800_80139F10->st.appliedAnimId = D_actor_110800_80139F10->st.animId;
}

/// Reseeds animation slots 1..0x13 of the work block's animation context from
/// the current animation id through `func_800B4114` (arguments 0 and 8), then
/// records that id as the one now playing.
static void func_actor_110800_801324AC(void)
{
    s32 i;

    i = 1;
    do {
        func_800B4114(&D_actor_110800_80139F10->rig.anim, i, (s16)D_actor_110800_80139F10->st.animId, 0, 8);
        i++;
    } while (i < 0x14);
    D_actor_110800_80139F10->st.appliedAnimId = D_actor_110800_80139F10->st.animId;
}

/// Starts the actor's scripted animation selected by the request.
///
/// Rejects ids 6 and above before changing playback state.
s32 func_actor_110800_80132524(Task* task, s32 arg1, AnimationPlayRequest* args)
{
    Task* actor;

    if (args->animationId < 6) {
        D_actor_110800_80139F10->st.animId  = args->animationId;
        actor                               = gActorSelfTask;
        D_actor_110800_80139F10->st.state   = 2;
        D_actor_110800_80139F10->st.field_6 = 0;
        func_actor_110800_80132368(actor);
        return 0;
    }
    return -1;
}

#include "../../shared/actor_messages_pair_visibility.inc.c"
