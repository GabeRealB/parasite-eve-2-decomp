#include "common.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/tmd.h"
#include "gameplay/1BC.h"
#include "gameplay/3A34.h"
#include "actors/actor_342400.h"
#include "actors/actors_shared_8016945c.h"
#include "actors/actors_shared_8016b370.h"

void func_actor_342400_8016B33C(Task* arg0);
void func_actor_342400_801694A8(Task* arg0, s32 arg1);

INCLUDE_ASM("actors/nonmatchings/actor_342400/actor_342400_19", func_actor_342400_8016A950);

void func_actor_342400_8016A9AC(Task* arg0)
{
    Actor342400Work* work;

    work            = (Actor342400Work*)arg0->idMap;
    arg0->state     = 5;
    work->field_420 = 0;
    work->field_422 = 0;
}

INCLUDE_ASM("actors/nonmatchings/actor_342400/actor_342400_19", func_actor_342400_8016A9C4);

void func_actor_342400_8016AA08(Task* arg0)
{
    Actor342400Work* work;
    Actor342400Work* work2;
    TmdObject*       model;
    GpEnemy*         enemy;

    model = (TmdObject*)arg0->extra;
    enemy = (GpEnemy*)arg0->spawnArg2;
    Tmd_FreeBuffers(model);
    model->field_C |= 4;
    func_actor_342400_801637DC(arg0);
    Gp_ReleaseStateF0Add((GpObj20E*)arg0, 0);
    enemy->field_54 = 0;
    work            = (Actor342400Work*)arg0->idMap;
    Gp_UnlinkObj(&work->obj_2AC);
    Gp_UnlinkObj(&work->obj_2CC);
    Gp_UnlinkObj(&work->obj_3AC);
    work2            = (Actor342400Work*)arg0->idMap;
    arg0->state      = 5;
    work2->field_420 = 0;
    work2->field_422 = 0;
}

void func_actor_342400_8016AA9C(Task* arg0)
{
    Actor342400Work* work;

    work            = (Actor342400Work*)arg0->idMap;
    work->field_412 = 0;
    work->field_420 = work->field_420 + 1;
}

INCLUDE_ASM("actors/nonmatchings/actor_342400/actor_342400_19", func_actor_342400_8016AAB8);

void func_actor_342400_8016AB6C(Task* arg0)
{
    Actor342400Work* work;
    Actor342400Work* work2;
    s32              soundId;
    s32              pan;

    work            = (Actor342400Work*)arg0->idMap;
    work->field_44F = D_actor_342400_80173A84[work->field_418 - 1];
    if (work->field_44F == 1) {
        work2            = (Actor342400Work*)arg0->idMap;
        work2->field_426 = 8;
        work2->field_41C = 0x10;
        work2->field_418 = 0xB;
        work2->field_414 = 1;
        SndEvt_EnqueueType7(0x402C0002, 1);
    } else {
        work2            = (Actor342400Work*)arg0->idMap;
        work2->field_426 = 8;
        work2->field_41C = 0x10;
        work2->field_418 = 0x11;
        work2->field_414 = 1;
    }
    soundId = ((((GpEnemy*)arg0->spawnArg2)->field_8 >> 0xC) << 8) | 0x402C0003;
    pan     = (s8)Gp_GetObjPan((GpObj38*)((TmdObject*)arg0->extra)->field_8);
    SndEvt_EnqueueType6(soundId, pan, (s8)Gp_GetObjDepth((GpObj38*)((TmdObject*)arg0->extra)->field_8));
    work->field_422++;
}

void func_actor_342400_8016AC80(Task* arg0)
{
    Actor342400Work* work;
    Actor342400Work* work2;
    s32              soundId;
    s32              pan;

    work            = (Actor342400Work*)arg0->idMap;
    work->field_44F = D_actor_342400_80173A84[work->field_418 - 1];
    if (work->field_44F == 1) {
        work2            = (Actor342400Work*)arg0->idMap;
        work2->field_426 = 2;
        work2->field_41C = 0x10;
        work2->field_418 = 0xC;
        work2->field_414 = 1;
        SndEvt_EnqueueType7(0x402C0002, 1);
    } else {
        work2            = (Actor342400Work*)arg0->idMap;
        work2->field_426 = 8;
        work2->field_41C = 0x10;
        work2->field_418 = 0x11;
        work2->field_414 = 1;
    }
    soundId = ((((GpEnemy*)arg0->spawnArg2)->field_8 >> 0xC) << 8) | 0x402C0003;
    pan     = (s8)Gp_GetObjPan((GpObj38*)((TmdObject*)arg0->extra)->field_8);
    SndEvt_EnqueueType6(soundId, pan, (s8)Gp_GetObjDepth((GpObj38*)((TmdObject*)arg0->extra)->field_8));
    work->field_422++;
}

void func_actor_342400_8016AD94(Task* arg0)
{
    Actor342400Work* work;
    s32              cond;

    work = (Actor342400Work*)arg0->idMap;
    if ((work->flags_EC.half & 1) || (work->flags_EC.word & 0x102)) {
        cond = 1;
    } else {
        cond = 0;
    }
    if (cond) {
        if (work->field_44F == 1) {
            work            = (Actor342400Work*)arg0->idMap;
            work->field_420 = 3;
            work->field_422 = 0;
        } else {
            func_actor_342400_801694A8(arg0, 1);
            work            = (Actor342400Work*)arg0->idMap;
            work->field_420 = 5;
            work->field_422 = 0;
        }
    }
}

extern TaskFuncTable3 D_actor_342400_80161FB4;

void func_actor_342400_8016AE24(Task* arg0)
{
    Actor342400Work* work;
    TaskFuncTable3   sp;

    work = (Actor342400Work*)arg0->idMap;
    sp   = D_actor_342400_80161FB4;
    if ((ActorsShared8016945c(arg0) << 0x10) == 0) {
        sp.funcs[(s16)work->field_422](arg0);
    }
}

extern TaskFuncTable3 D_actor_342400_80161FC0;

void func_actor_342400_8016AEAC(Task* arg0)
{
    Actor342400Work* work;
    TaskFuncTable3   sp;

    work = (Actor342400Work*)arg0->idMap;
    sp   = D_actor_342400_80161FC0;
    if ((ActorsShared8016945c(arg0) << 0x10) == 0) {
        sp.funcs[(s16)work->field_422](arg0);
    }
}

void func_actor_342400_8016AF34(Task* arg0)
{
    Actor342400Work* work                = (Actor342400Work*)arg0->idMap;
    void             (*states[2])(Task*) = {
        func_actor_342400_8016B33C,
        ActorsShared8016b370,
    };

    if (ActorsShared8016945c(arg0) == 0) {
        states[(s16)work->field_422](arg0);
    }
}

extern TaskFuncTable3 D_actor_342400_80161FCC;

void func_actor_342400_8016AFA8(Task* arg0)
{
    Actor342400Work* work;
    TaskFuncTable3   sp;

    work = (Actor342400Work*)arg0->idMap;
    sp   = D_actor_342400_80161FCC;
    if ((ActorsShared8016945c(arg0) << 0x10) != 0) {
        work->field_438 = 0;
        return;
    }
    sp.funcs[(s16)work->field_422](arg0);
}

INCLUDE_ASM("actors/nonmatchings/actor_342400/actor_342400_19", func_actor_342400_8016B038);
