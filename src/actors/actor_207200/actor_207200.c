#include "common.h"

#include "main/mem.h"
#include "main/task.h"
#include "main/tmd.h"

#include "gameplay/1BC.h"

#include "actors/actors_shared_80135d50.h"

/// The enemy's three state handlers - spawn/setup, per-frame tick and
/// teardown - dispatched through by state.
extern GpEnemyTaskFuncTable3 D_actor_207200_80149E24;

extern u8 D_801153F4;

INCLUDE_ASM("actors/nonmatchings/actor_207200/actor_207200", func_actor_207200_80149E84);

INCLUDE_ASM("actors/nonmatchings/actor_207200/actor_207200", func_actor_207200_8014A1C4);

INCLUDE_ASM("actors/nonmatchings/actor_207200/actor_207200", func_actor_207200_8014A588);

INCLUDE_ASM("actors/nonmatchings/actor_207200/actor_207200", func_actor_207200_8014AA74);

INCLUDE_RODATA("actors/nonmatchings/actor_207200/actor_207200", D_actor_207200_80149E20);

INCLUDE_RODATA("actors/nonmatchings/actor_207200/actor_207200", D_actor_207200_80149E24);

void func_actor_207200_8014AC9C(Task* arg0)
{
    GpEnemyTaskFuncTable3 sp;

    sp = D_actor_207200_80149E24;
    sp.funcs[arg0->state](arg0->spawnArg2, arg0);
}

void func_actor_207200_8014A588(Task* arg0);
void func_actor_207200_8014AE08(Task* arg0);
void func_actor_207200_8014AE70(Task* arg0);
void func_actor_207200_8014AF2C(Task* arg0);
void func_actor_207200_8014AFDC(void* arg0, Task* arg1);
void Gp_UpdateCoord(GsCOORDINATE2* arg0);

/// Global mode byte in the main executable shared by the enemy actors: 1 runs
/// only the tail below, 2 puts the model in its hidden pose, 0 clears the
/// node flag before falling into the update, and any other value updates
/// directly.
///
/// The update raises the root coordinate's Y translation by 0x80 and ticks the
/// five model helpers, clears the display flags of the first two parts and
/// recomputes the second part's world matrix; the tail then colours the actor
/// from that part.
void func_actor_207200_8014ACF8(GpEnemy* arg0, Task* arg1)
{
    s32 state;
    s32 one;

    state = D_801153F4;
    one   = 1;
    if (state == one) {
        goto case1;
    }
    if (state >= 2) {
        goto ge2;
    }
    if (state == 0) {
        goto case0;
    }
    goto default_body;
ge2:
    if (state == 2) {
        goto case2;
    }
    goto default_body;
case0:
    arg0->node.field_4 = 0;
    goto default_body;
case2:
    ((TmdObject*)arg1->extra)->field_C = 0x80;
    arg0->node.field_4                 = one;
    return;
default_body:
    ((TmdObject*)arg1->extra)->field_8[0].coord.t[1] += 0x80;
    func_actor_207200_8014AE70(arg1);
    ActorsShared80135d50(arg1);
    func_actor_207200_8014AE08(arg1);
    func_actor_207200_8014A588(arg1);
    func_actor_207200_8014AF2C(arg1);
    ((TmdObject*)arg1->extra)->field_8[0].flg = 0;
    ((TmdObject*)arg1->extra)->field_8[1].flg = 0;
    Gp_UpdateCoord(&((TmdObject*)arg1->extra)->field_8[1]);
case1:
    func_actor_207200_8014AFDC(arg0, arg1);
}

INCLUDE_ASM("actors/nonmatchings/actor_207200/actor_207200", func_actor_207200_8014AE08);

INCLUDE_ASM("actors/nonmatchings/actor_207200/actor_207200", func_actor_207200_8014AE70);

INCLUDE_ASM("actors/nonmatchings/actor_207200/actor_207200", func_actor_207200_8014AF2C);

/// Colours the actor from the *second* attach coordinate of its model: takes a
/// 0x10-byte `VECTOR` off `G_SCRATCH_HEAD`, fills it with that coordinate's
/// world position and hands it to `Gp_UpdateActorColor` with no blend
/// parameters. `arg0` is the colour target, passed straight through.
void func_actor_207200_8014AFDC(void* arg0, Task* task)
{
    GsCOORDINATE2* coord;
    void**         scratch;
    u8*            head;
    VECTOR*        block;

    coord     = &((TmdObject*)task->extra)->field_8[1];
    scratch   = (void**)G_SCRATCH_HEAD;
    head      = *scratch;
    block     = (VECTOR*)(head - 0x10);
    block->vx = coord->workm.t[0];
    block->vy = coord->workm.t[1];
    block->vz = coord->workm.t[2];
    *scratch  = block;
    Gp_UpdateActorColor(arg0, block, 0, 0);
    *scratch = (u8*)*scratch + 0x10;
}

INCLUDE_RODATA("actors/nonmatchings/actor_207200/actor_207200", D_actor_207200_80149E30);
