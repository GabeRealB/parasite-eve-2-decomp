#include "common.h"

#include "main/gfx.h"
#include "main/session.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/tmd.h"

#include "gameplay/1BC.h"
#include "gameplay/3A34.h"
#include "gameplay/3CD8.h"
#include "gameplay/D4.h"
#include "gameplay/gameplay.h"

#include "actors/actor_341700.h"

/* Called with no argument: the caller's own `Task*` is already in
 * `$a0` at the `jal` and the callee reads it as its own `arg0`, so the target
 * has no register copy.  A real prototype would make GCC emit one, so it
 * stays unprototyped. */
s32 ActorsShared8016945c();

INCLUDE_ASM("actors/nonmatchings/actor_341700/actor_341700_11", func_actor_341700_80169E20);

INCLUDE_ASM("actors/nonmatchings/actor_341700/actor_341700_11", func_actor_341700_80169EE4);

INCLUDE_ASM("actors/nonmatchings/actor_341700/actor_341700_11", func_actor_341700_80169F38);

INCLUDE_ASM("actors/nonmatchings/actor_341700/actor_341700_11", func_actor_341700_80169FB0);

void func_actor_341700_8016A058(Task* arg0)
{
    Actor341700Work* work = (Actor341700Work*)arg0->idMap;

    work->field_426 = 4;
    work->field_41C = 0x10;
    work->field_418 = 0xF;
    work->field_414 = 1;
    work->field_422 = work->field_422 + 1;
}

INCLUDE_ASM("actors/nonmatchings/actor_341700/actor_341700_11", func_actor_341700_8016A08C);

void func_actor_341700_8016A0E0(Task* arg0)
{
    Actor341700Work* work = (Actor341700Work*)arg0->idMap;

    work->field_426 = 8;
    work->field_41C = 0x10;
    work->field_418 = 0xF;
    work->field_414 = 1;
    work->field_422 = work->field_422 + 1;
    Gp_ArmStateF0(1);
}

INCLUDE_ASM("actors/nonmatchings/actor_341700/actor_341700_11", func_actor_341700_8016A130);

void func_actor_341700_8016A1A8(Task* arg0)
{
    Actor341700Work* work;
    Actor341700Work* work2;

    work = (Actor341700Work*)arg0->idMap;
    if ((ActorsShared8016945c() << 0x10) == 0) {
        work2            = (Actor341700Work*)arg0->idMap;
        work2->field_426 = 8;
        work2->field_41C = 0x10;
        work2->field_418 = 0xF;
        work2->field_414 = 1;
        work->field_422  = work->field_422 + 1;
    }
}

INCLUDE_ASM("actors/nonmatchings/actor_341700/actor_341700_11", func_actor_341700_8016A21C);

INCLUDE_ASM("actors/nonmatchings/actor_341700/actor_341700_11", func_actor_341700_8016A2CC);

INCLUDE_ASM("actors/nonmatchings/actor_341700/actor_341700_11", func_actor_341700_8016A460);

void func_actor_341700_8016A568(Task* arg0)
{
    Actor341700Work* work2;
    Actor341700Work* work;
    GpEnemy*         enemy;
    TmdObject*       model;

    work            = (Actor341700Work*)arg0->idMap;
    enemy           = (GpEnemy*)arg0->spawnArg2;
    model           = (TmdObject*)arg0->extra;
    work->field_412 = 0;
    SndEvt_EnqueueType7(0x402C0002, 1);
    if ((Gp_StateF0.field_1F & 0xF) == (((GpEnemy*)arg0->spawnArg2)->field_8 >> 0xC)) {
        Gp_StateF0.field_1F = 0;
    }
    Gp_UnlinkNode(&enemy->node);
    enemy->field_54 = 0;
    work2           = (Actor341700Work*)arg0->idMap;
    Gp_UnlinkObj(&work2->obj_2AC);
    Gp_UnlinkObj(&work2->obj_2CC);
    Gp_UnlinkObj(&work2->obj_3AC);
    model->field_C  = model->field_C | 0x80;
    work->field_420 = work->field_420 + 1;
}

void func_actor_341700_8016A630(Task* arg0)
{
    Actor341700Work* work;
    TmdObject*       model;
    u16              ticks;

    work            = (Actor341700Work*)arg0->idMap;
    model           = (TmdObject*)arg0->extra;
    ticks           = work->field_412 + 1;
    work->field_412 = ticks;
    if ((s16)ticks == 3) {
        Tmd_FreeBuffers(model);
        model->field_C |= 4;
    }
    if ((s16)work->field_412 >= 0x24) {
        Gp_DestroyEnemy(arg0->spawnArg2, arg0);
    }
}

void func_actor_341700_8016A6C0(Task* arg0)
{
    Actor341700Work* work;
    GpEnemy*         enemy;

    enemy = (GpEnemy*)arg0->spawnArg2;
    work  = (Actor341700Work*)arg0->idMap;
    SndEvt_EnqueueType7(((enemy->field_8 >> 0xC) << 8) | 0x402C0002, 0xF);
    if ((Gp_StateF0.field_1F & 0xF) == (((GpEnemy*)arg0->spawnArg2)->field_8 >> 0xC)) {
        Gp_StateF0.field_1F = 0;
    }
    Gp_UnlinkNode(&enemy->node);
    work->field_420 = work->field_420 + 1;
}

INCLUDE_ASM("actors/nonmatchings/actor_341700/actor_341700_11", func_actor_341700_8016A758);

INCLUDE_ASM("actors/nonmatchings/actor_341700/actor_341700_11", func_actor_341700_8016A810);
