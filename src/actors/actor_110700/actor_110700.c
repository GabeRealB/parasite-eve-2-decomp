#include "common.h"

#include "actors/actor.h"
#include "actors/actors_shared_80132074.h"
#include "gameplay/1BC.h"
#include "gameplay/3A34.h"
#include "gameplay/D4.h"
#include "main/mem.h"
#include "main/task.h"
#include "main/tmd.h"

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
extern GpMsgEntry D_actor_110700_8013BFA0[];

/// Animation source `func_800B3F84` seeds the work block's slots from.
extern u8 D_actor_110700_8013BFC0[];

void func_actor_110700_80131E78(GpEnemy* enemy, Task* task);
void func_actor_110700_80131F44(GpEnemy* enemy, Task* task);

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

    fns[task->state](task->spawnArg2, task);
}

/// State 0: allocates the work block, points the model at the block's light
/// and colour matrices, shows it, seeds the animation slots, installs the
/// message table and moves to state 1. If the allocation fails the enemy is
/// destroyed instead.
void func_actor_110700_80131E78(GpEnemy* enemy, Task* task)
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
void func_actor_110700_80131F44(GpEnemy* enemy, Task* task)
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
