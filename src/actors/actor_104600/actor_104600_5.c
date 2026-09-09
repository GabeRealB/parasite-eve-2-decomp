#include "common.h"

#include "main/mem.h"
#include "main/task.h"
#include "main/tmd.h"

#include "gameplay/1BC.h"
#include "gameplay/3A34.h"

#include "actors/actor_104600.h"

/// The enemy's three state handlers - spawn/setup, per-frame tick and
/// teardown - dispatched through by state.
extern GpEnemyTaskFuncTable3 D_actor_104600_80131E5C;

INCLUDE_ASM("actors/nonmatchings/actor_104600/actor_104600_5", func_actor_104600_80134B88);

INCLUDE_ASM("actors/nonmatchings/actor_104600/actor_104600_5", func_actor_104600_80134EC8);

INCLUDE_ASM("actors/nonmatchings/actor_104600/actor_104600_5", func_actor_104600_8013528C);

INCLUDE_ASM("actors/nonmatchings/actor_104600/actor_104600_5", func_actor_104600_80135778);

void func_actor_104600_801359A0(Task* arg0)
{
    GpEnemyTaskFuncTable3 sp;

    sp = D_actor_104600_80131E5C;
    sp.funcs[arg0->state](arg0->spawnArg2, arg0);
}

INCLUDE_ASM("actors/nonmatchings/actor_104600/actor_104600_5", func_actor_104600_801359FC);

INCLUDE_ASM("actors/nonmatchings/actor_104600/actor_104600_5", func_actor_104600_80135B0C);

/// Per-frame tick of the actor's four-way state machine. State 0 hands the
/// frame to `func_actor_104600_80134EC8` and state 2 idles. State 3 clears the
/// two flags at 0x292 / 0x2A6 and counts frames: every fourth one it arms
/// `field_28E`, clears `field_290` and restarts the count. Whatever the count,
/// state 3 drops back to state 0 once `Gp_TickObjFlag2` fires on the spawn
/// block.
void func_actor_104600_80135B74(Task* task)
{
    Actor104600Work* work;

    work = (Actor104600Work*)task->idMap;
    switch (work->field_286) {
        case 0:
            func_actor_104600_80134EC8(task);
            break;
        case 2:
            break;
        case 3:
            work->field_292 = 0;
            work->field_2A6 = 0;
            work->field_28A = work->field_28A + 1;
            if (work->field_28A >= 4) {
                work->field_28E = 1;
                work->field_290 = 0;
                work->field_28A = 0;
            }
            if (Gp_TickObjFlag2((GpObj5D*)task->spawnArg2) != 0) {
                work->field_286 = 0;
            }
            break;
    }
}

INCLUDE_ASM("actors/nonmatchings/actor_104600/actor_104600_5", func_actor_104600_80135C30);

/// Colours the actor from the *second* attach coordinate of its model: takes a
/// 0x10-byte `VECTOR` off `G_SCRATCH_HEAD`, fills it with that coordinate's
/// world position and hands it to `Gp_UpdateActorColor` with no blend
/// parameters. `arg0` is the colour target, passed straight through.
void func_actor_104600_80135CE0(void* arg0, Task* task)
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
