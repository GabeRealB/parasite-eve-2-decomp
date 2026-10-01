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
#include "main/mem.h"
#include "main/scratch.h"
#include "main/task_types.h"
#include "main/tmd_types.h"
#include "../../shared/actor_messages.h"

/// The actor's work block, allocated by the setup state and parked in
/// `Task::work`. It holds the model's animation context and slot array and the
/// light and colour matrices the model is drawn under.
typedef struct Actor110700Work {
    ActorAnimRig19 rig;
    MATRIX         colorMtx;
    MATRIX         lightMtx;
    s32            animId; // animation the slots were last seeded with; 0 until message 0x7D3 arrives
} Actor110700Work;
STATIC_ASSERT_SIZEOF(Actor110700Work, 0x480);

/// The actor's message table: handlers for 0x7D3 (start an animation), 0x7D4
/// (place the actor) and 0x7D5 (visibility), then the terminator.
// Handler views preserve the signatures used by this TU. The dispatcher
// transports each argument in a word register.
typedef struct {
    s32 id;
    union {
        s32 (*call0)(Task*, s32, AnimationPlayRequest*);
        s32 (*call1)(Task*, s32, ActorTransform*);
        s32 (*call2)(Task*, s32, s32);
    } handler;
} Actor110700MsgEntry;
STATIC_ASSERT_SIZEOF(Actor110700MsgEntry, 8);

extern Actor110700MsgEntry D_actor_110700_8013BFA0[];

/// Animation source `func_800B3F84` seeds the work block's slots from.
extern u8 D_actor_110700_8013BFC0[];

static void func_actor_110700_80131E78(Enemy* enemy, Task* task);
static void func_actor_110700_80131F44(Enemy* enemy, Task* task);

extern TmdSource D_actor_110700_801377C8;
s32              func_actor_110700_8013201C(Task*, s32, AnimationPlayRequest*);
s32              func_actor_110700_801320D8(Task*, s32, s32);
void             func_actor_110700_80131E24(Task*);

TmdBone D_actor_110700_80132120[19] = {
#include "assets/no9_golem_akropolis_body_skeleton.inc"
};

u32 D_actor_110700_801323CC[19] = {
#include "assets/no9_golem_akropolis_body_partVerts.inc"
};

SVECTOR D_actor_110700_80132418[358] = {
#include "assets/no9_golem_akropolis_body_verts.inc"
};

SVECTOR D_actor_110700_80132F48[356] = {
#include "assets/no9_golem_akropolis_body_normals.inc"
};

u32 D_actor_110700_80133A68[3928] = {
#include "assets/no9_golem_akropolis_body_stream.inc"
};

TmdSource D_actor_110700_801377C8 = {
    0,
    21588,
    5944,
    19,
    D_actor_110700_801323CC,
    D_actor_110700_80132418,
    D_actor_110700_80132F48,
    D_actor_110700_80132120,
    D_actor_110700_80133A68,
};

AnimationPackedPose D_actor_110700_801377EC[6] = {
#include "assets/actor_110700_animation_05D64_bank1.inc"
};

AnimationPackedRotation D_actor_110700_80137834[80] = {
#include "assets/actor_110700_animation_05D64_bank4.inc"
};

AnimationRecord D_actor_110700_80137974[122] = {
#include "assets/actor_110700_animation_05D64_records.inc"
};

u16 D_actor_110700_80137B5C[20] = {
#include "assets/actor_110700_animation_05D64_indices.inc"
};

AnimationSet D_actor_110700_80137B84 = {
    D_actor_110700_80137974,
    D_actor_110700_80137B5C,
    { NULL, D_actor_110700_801377EC, NULL, NULL, D_actor_110700_80137834, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_110700_80137BAC[14] = {
#include "assets/actor_110700_animation_0667C_bank1.inc"
};

AnimationPackedRotation D_actor_110700_80137C54[238] = {
#include "assets/actor_110700_animation_0667C_bank4.inc"
};

AnimationRecord D_actor_110700_8013800C[282] = {
#include "assets/actor_110700_animation_0667C_records.inc"
};

u16 D_actor_110700_80138474[20] = {
#include "assets/actor_110700_animation_0667C_indices.inc"
};

AnimationSet D_actor_110700_8013849C = {
    D_actor_110700_8013800C,
    D_actor_110700_80138474,
    { NULL, D_actor_110700_80137BAC, NULL, NULL, D_actor_110700_80137C54, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_110700_801384C4[53] = {
#include "assets/actor_110700_animation_0820C_bank1.inc"
};

AnimationPackedRotation D_actor_110700_80138740[730] = {
#include "assets/actor_110700_animation_0820C_bank4.inc"
};

AnimationRecord D_actor_110700_801392A8[855] = {
#include "assets/actor_110700_animation_0820C_records.inc"
};

u16 D_actor_110700_8013A004[20] = {
#include "assets/actor_110700_animation_0820C_indices.inc"
};

AnimationSet D_actor_110700_8013A02C = {
    D_actor_110700_801392A8,
    D_actor_110700_8013A004,
    { NULL, D_actor_110700_801384C4, NULL, NULL, D_actor_110700_80138740, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_110700_8013A054[11] = {
#include "assets/actor_110700_animation_08714_bank1.inc"
};

AnimationPackedRotation D_actor_110700_8013A0D8[110] = {
#include "assets/actor_110700_animation_08714_bank4.inc"
};

AnimationRecord D_actor_110700_8013A290[159] = {
#include "assets/actor_110700_animation_08714_records.inc"
};

u16 D_actor_110700_8013A50C[20] = {
#include "assets/actor_110700_animation_08714_indices.inc"
};

AnimationSet D_actor_110700_8013A534 = {
    D_actor_110700_8013A290,
    D_actor_110700_8013A50C,
    { NULL, D_actor_110700_8013A054, NULL, NULL, D_actor_110700_8013A0D8, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_110700_8013A55C[51] = {
#include "assets/actor_110700_animation_0A14C_bank1.inc"
};

AnimationPackedRotation D_actor_110700_8013A7C0[692] = {
#include "assets/actor_110700_animation_0A14C_bank4.inc"
};

AnimationRecord D_actor_110700_8013B290[813] = {
#include "assets/actor_110700_animation_0A14C_records.inc"
};

u16 D_actor_110700_8013BF44[20] = {
#include "assets/actor_110700_animation_0A14C_indices.inc"
};

AnimationSet D_actor_110700_8013BF6C = {
    D_actor_110700_8013B290,
    D_actor_110700_8013BF44,
    { NULL, D_actor_110700_8013A55C, NULL, NULL, D_actor_110700_8013A7C0, NULL, NULL, NULL },
};

TaskDesc D_actor_110700_8013BF94 = { { { TASK_BODY_TMD, 96 } }, func_actor_110700_80131E24, { .model = &D_actor_110700_801377C8 } };

Actor110700MsgEntry D_actor_110700_8013BFA0[4] = {
    { 2003, { .call0 = func_actor_110700_8013201C } },
    { 2004, { .call1 = actorMsgPlaceRotMatrix } },
    { 2005, { .call2 = func_actor_110700_801320D8 } },
    { TASK_MESSAGE_TABLE_END, { .call0 = NULL } },
};

u8 D_actor_110700_8013BFC0[24] = {
    0,
    0,
    0,
    0,
    132,
    123,
    19,
    128,
    156,
    132,
    19,
    128,
    44,
    160,
    19,
    128,
    52,
    165,
    19,
    128,
    108,
    191,
    19,
    128,
};

/// The actor's task entry. Runs the handler for the task's current state,
/// passing the `Enemy` the task was spawned with: state 0 sets the actor up,
/// state 1 is its per-frame update. The two-entry handler table is built on
/// the stack on every call.
void func_actor_110700_80131E24(Task* task)
{
    void (*fns[2])(Enemy*, Task*) = {
        func_actor_110700_80131E78,
        func_actor_110700_80131F44,
    };

    fns[task->state](task->spawnArg2.pointer, task);
}

/// State 0: allocates the work block, points the model at the block's light
/// and colour matrices, shows it, seeds the animation slots, installs the
/// message table and moves to state 1. If the allocation fails the enemy is
/// destroyed instead.
static void func_actor_110700_80131E78(Enemy* enemy, Task* task)
{
    GfxCoord*        coord;
    TmdObject*       obj;
    Actor110700Work* work;

    obj   = task->extra.tmd;
    coord = obj->coords;
    work  = memCalloc(sizeof(Actor110700Work), false);
    if (work == NULL) {
        Gp_DestroyEnemy(enemy, task);
        return;
    }
    task->work    = work;
    obj->lightMtx = &work->lightMtx;
    obj->colorMtx = &work->colorMtx;
    obj->flags    = 0;
    func_800B3F84(&work->rig.anim, D_actor_110700_8013BFC0, obj, work->rig.poses, work->rig.slots);
    work->animId        = 0;
    task->msgTable      = D_actor_110700_8013BFA0;
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
    task->state         = 1;
}

/// State 1, run every frame: ticks animation slots 1..0x12 once an animation
/// has been started, then pushes the world translation of the model's second
/// coordinate on the scratch stack and hands it to `Gp_UpdateActorColor`.
static void func_actor_110700_80131F44(Enemy* enemy, Task* task)
{
    Actor110700Work* work;
    GfxCoord*        coord;
    VECTOR*          block;
    s32              i;

    work  = (Actor110700Work*)task->work;
    coord = &task->extra.tmd->coords[1];
    SCRATCH_STACK_RESERVE_BYTES(0x10);
    block = SCRATCH_STACK_CURSOR(VECTOR);
    if (work->animId != 0) {
        for (i = 1; i < 0x13; i++) {
            animationTickSlot(&work->rig.anim, i);
        }
    }
    block->vx = coord->workm.t[0];
    block->vy = coord->workm.t[1];
    block->vz = coord->workm.t[2];
    Gp_UpdateActorColor(enemy, block, 0, 0);
    SCRATCH_STACK_RELEASE_BYTES(0x10);
}

/// Message 0x7D3 handler: starts the animation the payload names, storing its
/// id in the work block and reseeding slots 1..0x12 with it.
s32 func_actor_110700_8013201C(Task* task, s32 msgId, AnimationPlayRequest* args)
{
    Actor110700Work* work;
    s32              i;

    work         = (Actor110700Work*)task->work;
    work->animId = args->animationId;
    i            = 1;
    do {
        animationResetSlot(&work->rig.anim, i, work->animId);
        i++;
    } while (i < 0x13);
    return 0;
}

#include "../../shared/actor_messages_place_rot_matrix.inc.c"

/// Message 0x7D5 handler: sets the model's visibility from `arg2`. Bit 0 clear
/// replaces the object's flags with 0x80 (hidden), bit 0 set clears them
/// (shown); bit 1 then ORs in 0x4.
s32 func_actor_110700_801320D8(Task* task, s32 msgId, s32 arg2)
{
    TmdObject* obj;

    obj = task->extra.tmd;
    if (!(arg2 & 1)) {
        obj->flags = TMD_OBJECT_SKIP_ACTIVE_DRAW;
    } else {
        obj->flags = 0;
    }
    if (arg2 & 2) {
        obj         = task->extra.tmd;
        obj->flags |= TMD_OBJECT_SKIP_AUTO_BUFFER;
    }
    return 0;
}
