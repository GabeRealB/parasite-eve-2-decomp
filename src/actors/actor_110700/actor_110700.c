#include "common.h"

#include "actors/actor.h"
#include "actors/actors_shared_80132074.h"

#include "gameplay/collision.h"
#include "gameplay/display.h"
#include "gameplay/message.h"
#include "gameplay/scene_runtime.h"
#include "gameplay/world_coords.h"

#include "gameplay/enemy.h"
#include "main/mem.h"
#include "main/scratch.h"
#include "main/session_types.h"
#include "main/task_types.h"

#include "gameplay/animation.h"

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
        s32 (*call0)(Task *, s32, GpAnimArg *);
        s32 (*call1)(Task *, s32, GpXformArg *);
        s32 (*call2)(Task *, s32, s32);
    } handler;
} Actor110700MsgEntry;
STATIC_ASSERT_SIZEOF(Actor110700MsgEntry, 8);

extern Actor110700MsgEntry D_actor_110700_8013BFA0[];

/// Animation source `func_800B3F84` seeds the work block's slots from.
extern u8 D_actor_110700_8013BFC0[];

static void func_actor_110700_80131E78(GpEnemy* enemy, Task* task);
static void func_actor_110700_80131F44(GpEnemy* enemy, Task* task);

extern TmdSource D_actor_110700_801377C8;
s32 func_actor_110700_8013201C(Task *, s32, GpAnimArg *);
s32 func_actor_110700_80132074(Task *, s32, GpXformArg *);
s32 func_actor_110700_801320D8(Task *, s32, s32);
void func_actor_110700_80131E24(Task *);

TmdBone D_actor_110700_80132120[19] = {
#include "assets/actor_110700_model_059A8_skeleton.inc"
};

u32 D_actor_110700_801323CC[19] = {
#include "assets/actor_110700_model_059A8_partVerts.inc"
};

SVECTOR D_actor_110700_80132418[358] = {
#include "assets/actor_110700_model_059A8_verts.inc"
};

SVECTOR D_actor_110700_80132F48[356] = {
#include "assets/actor_110700_model_059A8_normals.inc"
};

u32 D_actor_110700_80133A68[3928] = {
#include "assets/actor_110700_model_059A8_stream.inc"
};

TmdSource D_actor_110700_801377C8 = {
    0, 21588, 5944, 19,
    D_actor_110700_801323CC, D_actor_110700_80132418, D_actor_110700_80132F48, D_actor_110700_80132120, D_actor_110700_80133A68,
};

// The player indexes this pose bank in words, then reads a full pose.
typedef union {
    GpPackedPose poses[6];
    GpPackedSvec words[18];
} Actor110700PoseBank59CC;

Actor110700PoseBank59CC D_actor_110700_801377EC = { .poses = {
#include "assets/actor_110700_animation_05D64_bank1.inc"
} };

GpPackedSvec D_actor_110700_80137834[80] = {
#include "assets/actor_110700_animation_05D64_bank4.inc"
};

GpAnimRec D_actor_110700_80137974[122] = {
#include "assets/actor_110700_animation_05D64_records.inc"
};

u16 D_actor_110700_80137B5C[20] = {
#include "assets/actor_110700_animation_05D64_indices.inc"
};

GpAnimSet D_actor_110700_80137B84 = {
    D_actor_110700_80137974, D_actor_110700_80137B5C,
    { NULL, D_actor_110700_801377EC.words, NULL, NULL, D_actor_110700_80137834, NULL, NULL, NULL },
};

// The player indexes this pose bank in words, then reads a full pose.
typedef union {
    GpPackedPose poses[14];
    GpPackedSvec words[42];
} Actor110700PoseBank5D8C;

Actor110700PoseBank5D8C D_actor_110700_80137BAC = { .poses = {
#include "assets/actor_110700_animation_0667C_bank1.inc"
} };

GpPackedSvec D_actor_110700_80137C54[238] = {
#include "assets/actor_110700_animation_0667C_bank4.inc"
};

GpAnimRec D_actor_110700_8013800C[282] = {
#include "assets/actor_110700_animation_0667C_records.inc"
};

u16 D_actor_110700_80138474[20] = {
#include "assets/actor_110700_animation_0667C_indices.inc"
};

GpAnimSet D_actor_110700_8013849C = {
    D_actor_110700_8013800C, D_actor_110700_80138474,
    { NULL, D_actor_110700_80137BAC.words, NULL, NULL, D_actor_110700_80137C54, NULL, NULL, NULL },
};

// The player indexes this pose bank in words, then reads a full pose.
typedef union {
    GpPackedPose poses[53];
    GpPackedSvec words[159];
} Actor110700PoseBank66A4;

Actor110700PoseBank66A4 D_actor_110700_801384C4 = { .poses = {
#include "assets/actor_110700_animation_0820C_bank1.inc"
} };

GpPackedSvec D_actor_110700_80138740[730] = {
#include "assets/actor_110700_animation_0820C_bank4.inc"
};

GpAnimRec D_actor_110700_801392A8[855] = {
#include "assets/actor_110700_animation_0820C_records.inc"
};

u16 D_actor_110700_8013A004[20] = {
#include "assets/actor_110700_animation_0820C_indices.inc"
};

GpAnimSet D_actor_110700_8013A02C = {
    D_actor_110700_801392A8, D_actor_110700_8013A004,
    { NULL, D_actor_110700_801384C4.words, NULL, NULL, D_actor_110700_80138740, NULL, NULL, NULL },
};

// The player indexes this pose bank in words, then reads a full pose.
typedef union {
    GpPackedPose poses[11];
    GpPackedSvec words[33];
} Actor110700PoseBank8234;

Actor110700PoseBank8234 D_actor_110700_8013A054 = { .poses = {
#include "assets/actor_110700_animation_08714_bank1.inc"
} };

GpPackedSvec D_actor_110700_8013A0D8[110] = {
#include "assets/actor_110700_animation_08714_bank4.inc"
};

GpAnimRec D_actor_110700_8013A290[159] = {
#include "assets/actor_110700_animation_08714_records.inc"
};

u16 D_actor_110700_8013A50C[20] = {
#include "assets/actor_110700_animation_08714_indices.inc"
};

GpAnimSet D_actor_110700_8013A534 = {
    D_actor_110700_8013A290, D_actor_110700_8013A50C,
    { NULL, D_actor_110700_8013A054.words, NULL, NULL, D_actor_110700_8013A0D8, NULL, NULL, NULL },
};

// The player indexes this pose bank in words, then reads a full pose.
typedef union {
    GpPackedPose poses[51];
    GpPackedSvec words[153];
} Actor110700PoseBank873C;

Actor110700PoseBank873C D_actor_110700_8013A55C = { .poses = {
#include "assets/actor_110700_animation_0A14C_bank1.inc"
} };

GpPackedSvec D_actor_110700_8013A7C0[692] = {
#include "assets/actor_110700_animation_0A14C_bank4.inc"
};

GpAnimRec D_actor_110700_8013B290[813] = {
#include "assets/actor_110700_animation_0A14C_records.inc"
};

u16 D_actor_110700_8013BF44[20] = {
#include "assets/actor_110700_animation_0A14C_indices.inc"
};

GpAnimSet D_actor_110700_8013BF6C = {
    D_actor_110700_8013B290, D_actor_110700_8013BF44,
    { NULL, D_actor_110700_8013A55C.words, NULL, NULL, D_actor_110700_8013A7C0, NULL, NULL, NULL },
};

TaskDesc D_actor_110700_8013BF94 = { 1, 96, func_actor_110700_80131E24, { .model = &D_actor_110700_801377C8 } };

Actor110700MsgEntry D_actor_110700_8013BFA0[4] = {
    { 2003, { .call0 = func_actor_110700_8013201C } },
    { 2004, { .call1 = func_actor_110700_80132074 } },
    { 2005, { .call2 = func_actor_110700_801320D8 } },
    { 0x7FFFFFFF, { .call0 = NULL } },
};

u8 D_actor_110700_8013BFC0[24] = {
    0, 0, 0, 0, 132, 123, 19, 128, 156, 132, 19, 128, 44, 160, 19, 128,
    52, 165, 19, 128, 108, 191, 19, 128,
};

/// The actor's task entry. Runs the handler for the task's current state,
/// passing the `GpEnemy` the task was spawned with: state 0 sets the actor up,
/// state 1 is its per-frame update. The two-entry handler table is built on
/// the stack on every call.
void func_actor_110700_80131E24(Task* task)
{
    void (*fns[2])(GpEnemy*, Task*) = {
        func_actor_110700_80131E78,
        func_actor_110700_80131F44,
    };

    fns[task->state](task->spawnArg2.pointer, task);
}

/// State 0: allocates the work block, points the model at the block's light
/// and colour matrices, shows it, seeds the animation slots, installs the
/// message table and moves to state 1. If the allocation fails the enemy is
/// destroyed instead.
static void func_actor_110700_80131E78(GpEnemy* enemy, Task* task)
{
    GpCoord*         coord;
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
    work->animId   = 0;
    task->msgTable = D_actor_110700_8013BFA0;
    coord->flg     = 0;
    task->state    = 1;
}

/// State 1, run every frame: ticks animation slots 1..0x12 once an animation
/// has been started, then pushes the world translation of the model's second
/// coordinate on the scratch stack and hands it to `Gp_UpdateActorColor`.
static void func_actor_110700_80131F44(GpEnemy* enemy, Task* task)
{
    Actor110700Work* work;
    GpCoord*         coord;
    VECTOR*          block;
    s32              i;

    work  = (Actor110700Work*)task->work;
    coord = &task->extra.tmd->coords[1];
    SCRATCH_PUSH_BYTES(0x10);
    block = (VECTOR*)SCRATCH_HEAD(void);
    if (work->animId != 0) {
        for (i = 1; i < 0x13; i++) {
            Gp_AnimTickIndex(&work->rig.anim, i);
        }
    }
    block->vx = coord->workm.t[0];
    block->vy = coord->workm.t[1];
    block->vz = coord->workm.t[2];
    Gp_UpdateActorColor(enemy, block, 0, 0);
    SCRATCH_POP_BYTES(0x10);
}

/// Message 0x7D3 handler: starts the animation the payload names, storing its
/// id in the work block and reseeding slots 1..0x12 with it.
s32 func_actor_110700_8013201C(Task* task, s32 msgId, GpAnimArg* args)
{
    Actor110700Work* work;
    s32              i;

    work         = (Actor110700Work*)task->work;
    work->animId = args->field_4;
    i            = 1;
    do {
        Gp_AnimResetSlot(&work->rig.anim, i, work->animId);
        i++;
    } while (i < 0x13);
    return 0;
}

/// Message 0x7D4 handler: places the actor. Builds the root coordinate's
/// rotation from the message's Euler angles, writes its translation, and
/// clears `flg` so the world matrix is recomputed from them.
s32 func_actor_110700_80132074(Task* task, s32 msgId, GpXformArg* args)
{
    TmdObject* ext   = task->extra.tmd;
    GpCoord*   coord = ext->coords;

    RotMatrix(&args->rot, &coord->coord);
    coord->coord.t[0] = args->pos.vx;
    coord->coord.t[1] = args->pos.vy;
    coord->coord.t[2] = args->pos.vz;
    coord->flg        = 0;
    return 0;
}

/// Message 0x7D5 handler: sets the model's visibility from `arg2`. Bit 0 clear
/// replaces the object's flags with 0x80 (hidden), bit 0 set clears them
/// (shown); bit 1 then ORs in 0x4.
s32 func_actor_110700_801320D8(Task* task, s32 msgId, s32 arg2)
{
    TmdObject* obj;

    obj = task->extra.tmd;
    if (!(arg2 & 1)) {
        obj->flags = 0x80;
    } else {
        obj->flags = 0;
    }
    if (arg2 & 2) {
        obj         = task->extra.tmd;
        obj->flags |= 4;
    }
    return 0;
}
