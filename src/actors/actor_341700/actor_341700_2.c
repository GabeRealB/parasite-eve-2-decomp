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

/* Both are called with no argument: the caller's own `Task*` is already in
 * `$a0` at the `jal` and the callee reads it as its own `arg0`, so the target
 * has no register copy.  A real prototype would make GCC emit one, so these
 * stay unprototyped. */
s32 func_actor_341700_80168178();
s32 func_actor_341700_80168468();

void func_actor_341700_80168370(Task* arg0, s16 arg1, SVECTOR3* arg2)
{
    MATRIX         local;
    MATRIX         world;
    GsCOORDINATE2* coord;
    GsCOORDINATE2* coords;

    coords = (GsCOORDINATE2*)((TmdObject*)arg0->extra)->field_8;
    coord  = &coords[arg1];
    Gp_UpdateCoord(coord);
    Gp_WorldToLocal(&Gfx_ViewWorldMtx, &coords->workm, &local);
    Gp_WorldToLocal(&Gfx_ViewWorldMtx, &coord->workm, &world);
    coords->coord.t[0] = arg2->vx - (world.t[0] - local.t[0]);
    coords->coord.t[1] = arg2->vy - (world.t[1] - local.t[1]);
    coords->coord.t[2] = arg2->vz - (world.t[2] - local.t[2]);
    coord->flg         = 0;
}

s32 func_actor_341700_80168444(Task* arg0, s16 arg1)
{
    return (s32)((((Actor341700Work*)arg0->idMap)->field_41C * arg1) << 0xC) >> 0x10;
}

INCLUDE_ASM("actors/nonmatchings/actor_341700/actor_341700_2", func_actor_341700_80168468);

INCLUDE_ASM("actors/nonmatchings/actor_341700/actor_341700_2", func_actor_341700_801684A8);

INCLUDE_ASM("actors/nonmatchings/actor_341700/actor_341700_2", func_actor_341700_8016852C);

INCLUDE_ASM("actors/nonmatchings/actor_341700/actor_341700_2", func_actor_341700_8016859C);

INCLUDE_ASM("actors/nonmatchings/actor_341700/actor_341700_2", func_actor_341700_801685F0);

void func_actor_341700_80168684(Task* arg0)
{
    Actor341700Work* work = (Actor341700Work*)arg0->idMap;

    work->field_420 = 5;
    work->field_422 = 0;
}

void func_actor_341700_80168698(Task* arg0)
{
    Actor341700Work* work = (Actor341700Work*)arg0->idMap;

    work->field_420 = 5;
    work->field_422 = 0;
}

void func_actor_341700_801686AC(Task* arg0)
{
    Actor341700Work* work = (Actor341700Work*)arg0->idMap;

    work->field_420 = 5;
    work->field_422 = 0;
}

INCLUDE_ASM("actors/nonmatchings/actor_341700/actor_341700_2", func_actor_341700_801686C0);

INCLUDE_ASM("actors/nonmatchings/actor_341700/actor_341700_2", func_actor_341700_80168748);

INCLUDE_ASM("actors/nonmatchings/actor_341700/actor_341700_2", func_actor_341700_801687B4);

INCLUDE_ASM("actors/nonmatchings/actor_341700/actor_341700_2", func_actor_341700_80168820);

INCLUDE_ASM("actors/nonmatchings/actor_341700/actor_341700_2", func_actor_341700_80168874);

INCLUDE_ASM("actors/nonmatchings/actor_341700/actor_341700_2", func_actor_341700_801688C8);

INCLUDE_ASM("actors/nonmatchings/actor_341700/actor_341700_2", func_actor_341700_8016891C);

INCLUDE_ASM("actors/nonmatchings/actor_341700/actor_341700_2", func_actor_341700_801689A0);

void func_actor_341700_80168A14(Task* arg0)
{
    Actor341700Work* work = (Actor341700Work*)arg0->idMap;

    work->field_426 = 8;
    work->field_41C = 0x10;
    work->field_418 = 0xC;
    work->field_414 = 1;
    work->field_422 = work->field_422 + 1;
}

void func_actor_341700_80168A48(Task* arg0)
{
    Actor341700Work* work;
    Actor341700Work* work2;

    if ((func_actor_341700_80168468() << 0x10) != 0) {
        work            = (Actor341700Work*)arg0->idMap;
        work->field_426 = 4;
        work->field_41C = 0x10;
        work->field_418 = 0xB;
        work->field_414 = 1;
    }
    if (Gp_TickObjFlag2((GpObj5D*)arg0->spawnArg2) != 0) {
        work2            = (Actor341700Work*)arg0->idMap;
        work2->field_420 = 3;
        work2->field_422 = 0;
    }
}

INCLUDE_ASM("actors/nonmatchings/actor_341700/actor_341700_2", func_actor_341700_80168AC0);

void func_actor_341700_80168B40(Task* arg0)
{
    Actor341700Work* work;
    Actor341700Work* slow;
    Actor341700Work* fast;

    work = (Actor341700Work*)arg0->idMap;
    if ((func_actor_341700_80168468() << 0x10) != 0) {
        if (work->field_44F == 1) {
            fast            = (Actor341700Work*)arg0->idMap;
            fast->field_426 = 0x32;
            fast->field_41C = 0x10;
            fast->field_418 = 7;
            fast->field_414 = 1;
        } else {
            slow            = (Actor341700Work*)arg0->idMap;
            slow->field_426 = 0x1E;
            slow->field_41C = 0x10;
            slow->field_418 = 1;
            slow->field_414 = 1;
        }
        work->field_422 = work->field_422 + 1;
    }
}

INCLUDE_ASM("actors/nonmatchings/actor_341700/actor_341700_2", func_actor_341700_80168BE0);

INCLUDE_ASM("actors/nonmatchings/actor_341700/actor_341700_2", func_actor_341700_80168C4C);

INCLUDE_ASM("actors/nonmatchings/actor_341700/actor_341700_2", func_actor_341700_80168D3C);

INCLUDE_ASM("actors/nonmatchings/actor_341700/actor_341700_2", func_actor_341700_80168DA0);

INCLUDE_ASM("actors/nonmatchings/actor_341700/actor_341700_2", func_actor_341700_80168EA0);

void func_actor_341700_80168F5C(Task* arg0)
{
    u16              ticks;
    Actor341700Work* work;

    work            = (Actor341700Work*)arg0->idMap;
    ticks           = work->field_412;
    work->field_412 = ticks + 1;
    if ((s16)ticks >= 0x51) {
        work->field_422 = work->field_422 + 1;
    }
}

void func_actor_341700_80168F9C(Task* arg0)
{
    Actor341700Work* work;
    Actor341700Work* work2;

    work = (Actor341700Work*)arg0->idMap;
    if ((func_actor_341700_80168468() << 0x10) != 0) {
        func_actor_341700_801681C4(arg0, 0);
        work2            = (Actor341700Work*)arg0->idMap;
        work2->field_426 = 8;
        work2->field_41C = 0x10;
        work2->field_418 = 0xF;
        work2->field_414 = 1;
        work->field_422  = work->field_422 + 1;
    }
}

void func_actor_341700_80169018(Task* arg0)
{
    Actor341700Work* work;
    Actor341700Work* work2;

    work = (Actor341700Work*)arg0->idMap;
    if ((func_actor_341700_80168468() << 0x10) != 0) {
        work->field_438  = 1;
        work->field_412  = 0;
        work2            = (Actor341700Work*)arg0->idMap;
        work2->field_426 = 4;
        work2->field_41C = 0x10;
        work2->field_418 = 4;
        work2->field_414 = 1;
        work->field_422  = work->field_422 + 1;
    }
}

INCLUDE_ASM("actors/nonmatchings/actor_341700/actor_341700_2", func_actor_341700_8016908C);

INCLUDE_ASM("actors/nonmatchings/actor_341700/actor_341700_2", func_actor_341700_801691B0);

void func_actor_341700_80169218(Task* arg0)
{
    Actor341700Work* work;
    Actor341700Work* work2;

    work             = (Actor341700Work*)arg0->idMap;
    work->field_432  = 1;
    work2            = (Actor341700Work*)arg0->idMap;
    work2->field_41C = 0x10;
    work2->field_418 = 7;
    work2->field_414 = 2;
    work->field_422  = work->field_422 + 1;
}

INCLUDE_ASM("actors/nonmatchings/actor_341700/actor_341700_2", func_actor_341700_80169254);

void func_actor_341700_80169380(Task* arg0)
{
    GpEnemy*         enemy;
    Actor341700Work* work;
    TmdObject*       model;
    Actor341700Work* work2;

    enemy = (GpEnemy*)arg0->spawnArg2;
    model = (TmdObject*)arg0->extra;
    work  = (Actor341700Work*)arg0->idMap;
    SndEvt_EnqueueType7(((enemy->field_8 >> 0xC) << 8) | 0x402C0002, 0xF);
    func_actor_341700_801681C4(arg0, 0);
    Gp_UnlinkNode(&enemy->node);
    if (work->field_448 == 4) {
        work->field_412  = 0;
        model->field_C   = model->field_C | 0x80;
        work2            = (Actor341700Work*)arg0->idMap;
        work2->field_420 = 7;
        work2->field_422 = 0;
        return;
    }
    work->field_420 = work->field_420 + 1;
}

INCLUDE_ASM("actors/nonmatchings/actor_341700/actor_341700_2", func_actor_341700_80169440);

INCLUDE_ASM("actors/nonmatchings/actor_341700/actor_341700_2", func_actor_341700_80169520);

INCLUDE_ASM("actors/nonmatchings/actor_341700/actor_341700_2", func_actor_341700_801695A0);

void func_actor_341700_8016966C(Task* arg0)
{
    u16              ticks;
    Actor341700Work* work;
    TmdObject*       model;

    work            = (Actor341700Work*)arg0->idMap;
    model           = (TmdObject*)arg0->extra;
    ticks           = work->field_412 + 1;
    work->field_412 = ticks;
    if ((s16)ticks >= 0x18) {
        model->field_C  = model->field_C | 2;
        work->field_412 = 0U;
        work->field_451 = 1;
        work->field_420 = work->field_420 + 1;
    }
}

void func_actor_341700_801696C8(Task* arg0)
{
    Actor341700Work* work;

    work            = (Actor341700Work*)arg0->idMap;
    arg0->state     = 5;
    work->field_420 = 0;
    work->field_422 = 0;
}

void func_actor_341700_801696E0(Task* arg0)
{
    u16              ticks;
    Actor341700Work* work;

    work            = (Actor341700Work*)arg0->idMap;
    ticks           = work->field_412 + 1;
    work->field_412 = ticks;
    if ((s16)ticks >= 2) {
        work->field_420 = work->field_420 + 1;
    }
}

INCLUDE_ASM("actors/nonmatchings/actor_341700/actor_341700_2", func_actor_341700_80169724);

void func_actor_341700_801697B8(Task* arg0)
{
    Actor341700Work* work;

    work            = (Actor341700Work*)arg0->idMap;
    work->field_412 = 0;
    work->field_420 = work->field_420 + 1;
}

void func_actor_341700_801697D4(Task* arg0)
{
    Actor341700Work* work;
    u16              ticks;

    work            = (Actor341700Work*)arg0->idMap;
    ticks           = work->field_412 + 1;
    work->field_412 = ticks;
    if ((s16)ticks >= 0x24) {
        if ((Game_Session->field_7 == 4) && ((u32)(Game_Session->field_6 - 0x27) < 2U) && (Game_Session->field_9 == 1)) {
            Gp_DispatchMsg((Task*)Gp_LookupSlot4(0), 0x13F4, 1, 0);
        }
        Gp_DestroyEnemy(arg0->spawnArg2, arg0);
    }
}

INCLUDE_ASM("actors/nonmatchings/actor_341700/actor_341700_2", func_actor_341700_80169888);

INCLUDE_ASM("actors/nonmatchings/actor_341700/actor_341700_2", func_actor_341700_8016999C);

INCLUDE_ASM("actors/nonmatchings/actor_341700/actor_341700_2", func_actor_341700_80169AB0);

INCLUDE_ASM("actors/nonmatchings/actor_341700/actor_341700_2", func_actor_341700_80169B40);

INCLUDE_ASM("actors/nonmatchings/actor_341700/actor_341700_2", func_actor_341700_80169BC8);

INCLUDE_ASM("actors/nonmatchings/actor_341700/actor_341700_2", func_actor_341700_80169C50);

INCLUDE_ASM("actors/nonmatchings/actor_341700/actor_341700_2", func_actor_341700_80169CC4);

INCLUDE_ASM("actors/nonmatchings/actor_341700/actor_341700_2", func_actor_341700_80169D54);

INCLUDE_ASM("actors/nonmatchings/actor_341700/actor_341700_2", func_actor_341700_80169DBC);

INCLUDE_ASM("actors/nonmatchings/actor_341700/actor_341700_2", func_actor_341700_80169E20);

INCLUDE_ASM("actors/nonmatchings/actor_341700/actor_341700_2", func_actor_341700_80169EE4);

INCLUDE_ASM("actors/nonmatchings/actor_341700/actor_341700_2", func_actor_341700_80169F38);

INCLUDE_ASM("actors/nonmatchings/actor_341700/actor_341700_2", func_actor_341700_80169FB0);

void func_actor_341700_8016A058(Task* arg0)
{
    Actor341700Work* work = (Actor341700Work*)arg0->idMap;

    work->field_426 = 4;
    work->field_41C = 0x10;
    work->field_418 = 0xF;
    work->field_414 = 1;
    work->field_422 = work->field_422 + 1;
}

INCLUDE_ASM("actors/nonmatchings/actor_341700/actor_341700_2", func_actor_341700_8016A08C);

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

INCLUDE_ASM("actors/nonmatchings/actor_341700/actor_341700_2", func_actor_341700_8016A130);

void func_actor_341700_8016A1A8(Task* arg0)
{
    Actor341700Work* work;
    Actor341700Work* work2;

    work = (Actor341700Work*)arg0->idMap;
    if ((func_actor_341700_80168178() << 0x10) == 0) {
        work2            = (Actor341700Work*)arg0->idMap;
        work2->field_426 = 8;
        work2->field_41C = 0x10;
        work2->field_418 = 0xF;
        work2->field_414 = 1;
        work->field_422  = work->field_422 + 1;
    }
}

INCLUDE_ASM("actors/nonmatchings/actor_341700/actor_341700_2", func_actor_341700_8016A21C);

INCLUDE_ASM("actors/nonmatchings/actor_341700/actor_341700_2", func_actor_341700_8016A2CC);

INCLUDE_ASM("actors/nonmatchings/actor_341700/actor_341700_2", func_actor_341700_8016A460);

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

INCLUDE_ASM("actors/nonmatchings/actor_341700/actor_341700_2", func_actor_341700_8016A758);

INCLUDE_ASM("actors/nonmatchings/actor_341700/actor_341700_2", func_actor_341700_8016A810);
