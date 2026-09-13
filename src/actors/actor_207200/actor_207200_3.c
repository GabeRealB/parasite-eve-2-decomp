#include "common.h"

#include "main/mem.h"
#include "main/task.h"
#include "main/tmd.h"

#include "gameplay/1BC.h"
#include "gameplay/3CD8.h"

#include "actors/actor_207200.h"
#include "actors/actors_shared_8013851c.h"

/// The enemy's three state handlers - spawn/setup, per-frame tick and
/// teardown - dispatched through by state.
extern GpEnemyTaskFuncTable3 D_actor_207200_80149E30;

INCLUDE_ASM("actors/nonmatchings/actor_207200/actor_207200_3", func_actor_207200_8014B278);

INCLUDE_ASM("actors/nonmatchings/actor_207200/actor_207200_3", func_actor_207200_8014B628);

INCLUDE_ASM("actors/nonmatchings/actor_207200/actor_207200_3", func_actor_207200_8014B87C);

INCLUDE_ASM("actors/nonmatchings/actor_207200/actor_207200_3", func_actor_207200_8014BEF4);

INCLUDE_ASM("actors/nonmatchings/actor_207200/actor_207200_3", func_actor_207200_8014C870);

INCLUDE_ASM("actors/nonmatchings/actor_207200/actor_207200_3", func_actor_207200_8014CA84);

INCLUDE_ASM("actors/nonmatchings/actor_207200/actor_207200_3", func_actor_207200_8014CE20);

/// Spawns the pair of effects that carry this actor's death animation, hands
/// the spawned task `D_actor_207200_801517F8` as its setup argument, arms the
/// two timers on the work area and unlinks its third display object.
void func_actor_207200_8014CFEC(Actor207200* arg0)
{
    GpEffArg*          effArg;
    struct _GpEffWork* effect;
    Actor207200Work*   work;
    Actor207200Ctx*    ctx;

    work = arg0->field_1C;
    ctx  = arg0->field_20;

    Gp_SpawnEff(0x6009C, (*(TmdObject**)&arg0->field_2C)->field_8, 0, NULL);
    func_800DA6E8(&ctx->node, ctx->field_40 - 1, 0);
    D_80062730 = (s32)&D_actor_207200_801517F8;
    effect     = Gp_SpawnEff(0x80005, (*(TmdObject**)&arg0->field_2C)->field_8 + 3, 0, NULL);
    if (effect != NULL) {
        ActorsShared8013851c(effect->field_0, (Task*)arg0);
    }
    effArg = &work->field_3F4;
    func_800FDB18(5, (*(TmdObject**)&arg0->field_2C)->field_8 + 3, &D_actor_207200_80153F18, effArg);
    func_800FDB18(5, (*(TmdObject**)&arg0->field_2C)->field_8 + 3, &D_actor_207200_80153F18, effArg);
    work->field_4A4            = 1;
    work->field_48C            = 5;
    work->field_4A6            = 1;
    ctx->field_54              = (s32)&work->field_214.field_20[0];
    ctx->field_40              = 1;
    work->field_2C4.obj.flags &= 0x7FFF;
    Gp_UnlinkObj(&work->field_2C4.obj);
    arg0->field_2A = 0x14;
}

INCLUDE_ASM("actors/nonmatchings/actor_207200/actor_207200_3", func_actor_207200_8014D128);

void func_actor_207200_8014D280(Task* arg0)
{
    GpEnemyTaskFuncTable3 sp;

    sp = D_actor_207200_80149E30;
    sp.funcs[arg0->state](arg0->spawnArg2, arg0);
}

INCLUDE_ASM("actors/nonmatchings/actor_207200/actor_207200_3", func_actor_207200_8014D2DC);

INCLUDE_ASM("actors/nonmatchings/actor_207200/actor_207200_3", func_actor_207200_8014D41C);

INCLUDE_ASM("actors/nonmatchings/actor_207200/actor_207200_3", func_actor_207200_8014D49C);

INCLUDE_ASM("actors/nonmatchings/actor_207200/actor_207200_3", func_actor_207200_8014D5C4);

INCLUDE_ASM("actors/nonmatchings/actor_207200/actor_207200_3", func_actor_207200_8014D65C);

/// Colours the actor from the *second* attach coordinate of its model: takes a
/// 0x10-byte `VECTOR` off `G_SCRATCH_HEAD`, fills it with that coordinate's
/// world position and hands it to `Gp_UpdateActorColor` with no blend
/// parameters. `arg0` is the colour target, passed straight through.
void func_actor_207200_8014D70C(void* arg0, Task* task)
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
