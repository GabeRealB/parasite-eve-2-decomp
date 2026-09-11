#include "common.h"
#include "main/task.h"
#include "main/mem.h"
#include "main/tmd.h"
#include "gameplay/3CD8.h"
#include "gameplay/3FB8.h"
#include "actors/actor_342400.h"

void func_actor_342400_80162084(Task* arg0)
{
    Actor342400ChildWork* work;
    GpEnemy*              enemy;
    Task*                 task;
    TmdObject*            obj;

    work = Mem_Calloc(0xC, 0);
    if (work == NULL) {
        goto kill;
    }
    arg0->idMap  = (TaskIdMap*)work;
    work->enemy0 = Gp_SpawnEnemyFromTable(&D_80151E60, 1, 1, 0);
    work->enemy1 = Gp_SpawnEnemyFromTable(&D_80151E60, 1, 1, 0);
    if (work->enemy0 == NULL && work->enemy1 == NULL) {
    kill:
        Task_Kill(arg0);
        return;
    }
    if (work->enemy0 != NULL) {
        enemy          = work->enemy0;
        enemy->field_8 = D_actor_342400_80173AAC << 12;
        D_actor_342400_80173AAC++;
        task            = enemy->task;
        obj             = task->extra;
        obj->field_24   = 3;
        obj->field_25   = 5;
        enemy->field_40 = 1;
    }
    if (work->enemy1 != NULL) {
        enemy          = work->enemy1;
        enemy->field_8 = D_actor_342400_80173AAC << 12;
        D_actor_342400_80173AAC++;
        task            = enemy->task;
        obj             = task->extra;
        obj->field_24   = 3;
        obj->field_25   = 5;
        enemy->field_40 = 1;
    }
    D_actor_342400_8016BF58[(s16)(arg0->spawnArg1 >> 16)].field_6 = 1;
    arg0->state++;
}

void func_actor_342400_801621D8(Task* arg0)
{
    Actor342400ChildWork* work = (Actor342400ChildWork*)arg0->idMap;
    GpEnemy*              enemy;
    Task*                 task;
    GsCOORDINATE2*        coord;
    Actor342400Msg7DB     msg;

    if (work->enemy0 != NULL) {
        enemy = work->enemy0;
        task  = enemy->task;
        coord = ((TmdObject*)task->extra)->field_8;
        if (enemy->field_40 <= 0) {
            work->enemy0 = NULL;
        } else if (func_actor_342400_801626CC(Game_Session->field_136, coord->coord.t[0], coord->coord.t[2])) {
            msg.field_0 = 0;
            msg.field_1 = 0;
            msg.field_2 = 5;
            Gp_DispatchMsg(task, 0x7DB, (s32)&msg, 0);
            work->enemy0 = NULL;
        }
    } else {
        work->field_A |= 1;
    }
    if (work->enemy1 != NULL) {
        enemy = work->enemy1;
        task  = enemy->task;
        coord = ((TmdObject*)task->extra)->field_8;
        if (enemy->field_40 <= 0) {
            work->enemy1 = NULL;
        } else if (func_actor_342400_801626CC(Game_Session->field_136, coord->coord.t[0], coord->coord.t[2])) {
            msg.field_0 = 0;
            msg.field_1 = 0;
            msg.field_2 = 5;
            Gp_DispatchMsg(task, 0x7DB, (s32)&msg, 0);
            work->enemy1 = NULL;
        }
    } else {
        work->field_A |= 2;
    }
}

void func_actor_342400_80162324(Task* arg0)
{
    Actor342400CtrlWork* work = (Actor342400CtrlWork*)arg0->idMap;
    s16                  count;
    s16                  i;
    s16                  idx;
    s16                  type;
    s16                  arg;

    count = 0;
    for (i = 0; i < 17; i++) {
        if (D_actor_342400_8016BF58[i].field_6 == 1) {
            count++;
        }
    }
    if (count < 3) {
        idx = work->field_2;
        if (idx < 17 && Game_Session->field_120 >= 0x3D) {
            type = D_actor_342400_8016BF58[idx].field_0;
            arg  = D_actor_342400_8016BF58[idx].field_2;
            switch (type) {
                case 0:
                    Task_SpawnFromTable(&D_actor_342400_8016BFE0, 1, (idx << 16) + arg + (func_actor_342400_801624A4() << 16 >> 8), 0);
                    break;
                case 1:
                    Task_SpawnFromTable(&D_actor_342400_8016BFE0, 2, (idx << 16) + arg + (func_actor_342400_801624A4() << 16 >> 8), 0);
                    break;
                case 2:
                    Task_SpawnFromTable(&D_actor_342400_8016BFE0, 3, (idx << 16) + arg + (func_actor_342400_801624A4() << 16 >> 8), 0);
                    break;
            }
            work->field_2++;
        }
    }
}

INCLUDE_ASM("actors/nonmatchings/actor_342400/actor_342400", func_actor_342400_801624A4);

void func_actor_342400_801626AC(Task* arg0, s32 arg1, Actor342400Msg* arg2)
{
    Actor342400CtrlWork* work = (Actor342400CtrlWork*)arg0->idMap;
    u16                  id   = arg2->field_2;

    if (id == 4) {
        work->field_4 = id;
    }
}

s16 func_actor_342400_801626CC(s16 arg0, s16 arg1, s16 arg2)
{
    if (arg0 == 0 || arg0 > 0x10) {
        return 0;
    }
    if (D_actor_342400_8016C010[arg0].axis == 0) {
        if (arg1 < D_actor_342400_8016C010[arg0].limit) {
            return 1;
        }
    } else {
        if (arg2 >= D_actor_342400_8016C010[arg0].limit) {
            return 1;
        }
    }
    return 0;
}

INCLUDE_RODATA("actors/nonmatchings/actor_342400/actor_342400", D_actor_342400_80161E20);

/// The controller task's four state handlers, dispatched by
/// `func_actor_342400_80162748`. Like `D_actor_342400_80161E54` below, splat
/// migrates it into the function's own `.s`, so it is defined here.
const TaskFuncTable4 D_actor_342400_80161E24 = { {
    func_actor_342400_801628F0,
    func_actor_342400_8016299C,
    func_actor_342400_80162A34,
    func_actor_342400_80162AB0,
} };

void func_actor_342400_80162748(Task* arg0)
{
    TaskFuncTable4 sp;

    sp = D_actor_342400_80161E24;
    if (D_801153F4 == 0) {
        sp.funcs[arg0->state](arg0);
    }
}

INCLUDE_ASM("actors/nonmatchings/actor_342400/actor_342400", func_actor_342400_801627C0);

INCLUDE_ASM("actors/nonmatchings/actor_342400/actor_342400", func_actor_342400_80162824);

/// The five state handlers `func_actor_342400_80162888` dispatches through by
/// `Task::state`. splat migrates this table into that function's own `.s`, so
/// there is no standalone rodata file to `INCLUDE_RODATA`; defining it here
/// emits it where the function sits, between the 0x80161E44 rodata of the
/// `INCLUDE_ASM` above and the `D_actor_342400_80161E68` include below.
const TaskFuncTable5 D_actor_342400_80161E54 = { {
    func_actor_342400_80162084,
    func_actor_342400_80162FFC,
    func_actor_342400_80163010,
    func_actor_342400_801630A4,
    func_actor_342400_80163178,
} };

void func_actor_342400_80162888(Task* arg0)
{
    TaskFuncTable5 sp;

    sp = D_actor_342400_80161E54;
    sp.funcs[arg0->state](arg0);
}

void func_actor_342400_801628F0(Task* arg0)
{
    Actor342400CtrlWork* work;
    s32                  i;

    if ((u8)Game_Session->unknown_130[1] == 2 || (u8)Game_Session->unknown_130[0] == 0 ||
        (work = Mem_Calloc(6, 0)) == NULL) {
        Task_Kill(arg0);
        return;
    }
    for (i = 16; i >= 0; i--) {
        D_actor_342400_8016BF58[i].field_6 = 0;
    }
    D_actor_342400_80173AAC = 0;
    arg0->idMap             = (TaskIdMap*)work;
    arg0->field_24          = D_actor_342400_8016BF48;
    arg0->state++;
}

void func_actor_342400_8016299C(Task* arg0)
{
    s32                  i;
    Actor342400CtrlWork* work = (Actor342400CtrlWork*)arg0->idMap;
    Actor342400Slot*     slot;

    for (i = 0; i < 3; i++) {
        slot = &D_actor_342400_8016BF58[work->field_2];
        func_actor_342400_80163200(work->field_2, slot->field_0, slot->field_2);
        work->field_2++;
    }
    arg0->state++;
}

void func_actor_342400_80162A34(Task* arg0)
{
    Actor342400CtrlWork* work = (Actor342400CtrlWork*)arg0->idMap;

    if (++work->field_0 == 15) {
        ((void (*)(s32))Gp_IncStateF0Ref)(0);
        Game_Session->unknown_130[1] = 1;
        Gp_ArmStateF0(1);
        arg0->state++;
    }
}

void func_actor_342400_80162AB0(Task* arg0)
{
    Actor342400CtrlWork* work = (Actor342400CtrlWork*)arg0->idMap;
    s16                  count;
    s32                  i;

    count = 0;
    if (work->field_4 != 4) {
        func_actor_342400_80162324(arg0);
        for (i = 0; i < 17; i++) {
            if (D_actor_342400_8016BF58[i].field_6 == 2) {
                count++;
            }
        }
        if (count == 17) {
            ((void (*)(Task*, s32))Gp_ReleaseStateF0Clear)(arg0, 0);
            Game_Session->unknown_130[1] = 2;
            Task_Kill(arg0);
        }
    }
}

void func_actor_342400_80162B60(Task* arg0)
{
    Actor342400SpawnWork* work;
    GpEnemy*              enemy;

    work = Mem_Calloc(8, 0);
    if (work != NULL) {
        arg0->idMap = (TaskIdMap*)work;
        enemy       = Gp_SpawnEnemyFromTable(&D_actor_342400_80173A54, 1, 0, 0);
        if (enemy != NULL) {
            D_actor_342400_8016BF58[(s16)(arg0->spawnArg1 >> 16)].field_6 = 1;
            work->enemy                                                   = enemy;
            enemy->field_8                                                = D_actor_342400_80173AAC << 12;
            D_actor_342400_80173AAC++;
            arg0->state++;
            return;
        }
    }
    Task_Kill(arg0);
}

void func_actor_342400_80162C10(Task* arg0)
{
    Actor342400SpawnWork* work = (Actor342400SpawnWork*)arg0->idMap;
    GpEnemy*              enemy;
    Task*                 task;
    TmdObject*            obj;
    Actor342400Msg7DB     msg;

    enemy = work->enemy;
    task  = enemy->task;
    if (++work->field_4 > 60) {
        obj            = task->extra;
        obj->field_25  = 2;
        obj->field_24  = 0;
        enemy->field_A = 0x900;
        msg.field_0    = 0;
        msg.field_1    = 0x2C;
        msg.field_2    = arg0->spawnArg1;
        Gp_DispatchMsg(task, 0x7DB, (s32)&msg, 0);
        arg0->state++;
    }
}

void func_actor_342400_80162CA8(Task* arg0)
{
    arg0->state = arg0->state + 1;
}

void func_actor_342400_80162CBC(Task* arg0)
{
    Actor342400SpawnWork* work = (Actor342400SpawnWork*)arg0->idMap;
    GpEnemy*              enemy;
    Task*                 task;
    GsCOORDINATE2*        coord;
    Actor342400Msg7DB     msg;

    enemy = work->enemy;
    task  = enemy->task;
    coord = ((TmdObject*)task->extra)->field_8;
    if (enemy->field_40 <= 0) {
        D_actor_342400_8016BF58[(s16)(arg0->spawnArg1 >> 16)].field_6 = 2;
        Task_Kill(arg0);
        return;
    }
    if (func_actor_342400_801626CC(Game_Session->field_136, coord->coord.t[0], coord->coord.t[2])) {
        msg.field_0 = 0;
        msg.field_1 = 0x2C;
        msg.field_2 = 5;
        Gp_DispatchMsg(task, 0x7DB, (s32)&msg, 0);
        D_actor_342400_8016BF58[(s16)(arg0->spawnArg1 >> 16)].field_6 = 2;
        Task_Kill(arg0);
    }
}

void func_actor_342400_80162DA0(Task* arg0)
{
    Actor342400SpawnWork* work;
    GpEnemy*              enemy;
    TmdObject*            obj;

    work = Mem_Calloc(8, 0);
    if (work != NULL) {
        arg0->idMap = (TaskIdMap*)work;
        enemy       = Gp_SpawnEnemyFromTable(&D_801575F0, 2, 0, 0);
        if (enemy != NULL) {
            D_actor_342400_8016BF58[(s16)(arg0->spawnArg1 >> 16)].field_6 = 1;
            work->enemy                                                   = enemy;
            enemy->field_8                                                = D_actor_342400_80173AAC << 12;
            D_actor_342400_80173AAC++;
            obj           = enemy->task->extra;
            obj->field_24 = 2;
            obj->field_25 = 4;
            arg0->state++;
            return;
        }
    }
    Task_Kill(arg0);
}

void func_actor_342400_80162E6C(Task* arg0)
{
    Actor342400SpawnWork* work = (Actor342400SpawnWork*)arg0->idMap;
    GpEnemy*              enemy;
    Task*                 task;
    TmdObject*            obj;
    Actor342400Msg7DB     msg;

    enemy = work->enemy;
    task  = enemy->task;
    if (++work->field_4 > 60) {
        obj            = task->extra;
        obj->field_24  = 2;
        obj->field_25  = 4;
        enemy->field_A = 0x900;
        msg.field_0    = 0;
        msg.field_1    = 0x2A;
        msg.field_2    = arg0->spawnArg1;
        Gp_DispatchMsg(task, 0x7DB, (s32)&msg, 0);
        arg0->state++;
    }
}

void func_actor_342400_80162F08(Task* arg0)
{
    arg0->state = arg0->state + 1;
}

void func_actor_342400_80162F1C(Task* arg0)
{
    Actor342400SpawnWork* work = (Actor342400SpawnWork*)arg0->idMap;
    GpEnemy*              enemy;
    Task*                 task;
    GsCOORDINATE2*        coord;
    Actor342400Msg7DB     msg;

    enemy = work->enemy;
    task  = enemy->task;
    coord = ((TmdObject*)task->extra)->field_8;
    if (enemy->field_40 <= 0) {
        D_actor_342400_8016BF58[(s16)(arg0->spawnArg1 >> 16)].field_6 = 2;
        Task_Kill(arg0);
        return;
    }
    if (func_actor_342400_801626CC(Game_Session->field_136, coord->coord.t[0], coord->coord.t[2])) {
        msg.field_0 = 0;
        msg.field_1 = 0;
        msg.field_2 = 5;
        Gp_DispatchMsg(task, 0x7DB, (s32)&msg, 0);
        D_actor_342400_8016BF58[(s16)(arg0->spawnArg1 >> 16)].field_6 = 2;
        Task_Kill(arg0);
    }
}

void func_actor_342400_80162FFC(Task* arg0)
{
    arg0->state = arg0->state + 1;
}

void func_actor_342400_80163010(Task* arg0)
{
    Actor342400ChildWork* work = (Actor342400ChildWork*)arg0->idMap;
    GpEnemy*              enemy;
    Task*                 task;
    TmdObject*            obj;
    Actor342400Msg7DB     msg;

    enemy = work->enemy0;
    if (enemy != NULL) {
        task           = enemy->task;
        obj            = task->extra;
        obj->field_24  = 3;
        obj->field_25  = 5;
        enemy->field_A = 0x900;
        msg.field_0    = 0;
        msg.field_1    = 0x2E;
        msg.field_2    = arg0->spawnArg1;
        Gp_DispatchMsg(task, 0x7DB, (s32)&msg, 0);
    }
    work->field_8 = 0;
    arg0->state++;
}

void func_actor_342400_801630A4(Task* arg0)
{
    Actor342400ChildWork* work = (Actor342400ChildWork*)arg0->idMap;
    GpEnemy*              enemy;
    Task*                 task;
    TmdObject*            obj;
    Actor342400Msg7DB     msg;

    enemy = work->enemy1;
    func_actor_342400_801632D4(arg0);
    if (work->enemy1 != NULL) {
        if (++work->field_8 <= 0x3C) {
            return;
        }
        task           = work->enemy1->task;
        obj            = task->extra;
        obj->field_24  = 3;
        obj->field_25  = 5;
        enemy->field_A = 0x900;
        msg.field_0    = 0;
        msg.field_1    = 0x2E;
        msg.field_2    = arg0->spawnArg1;
        Gp_DispatchMsg(task, 0x7DB, (s32)&msg, 0);
    }
    work->field_8 = 0;
    arg0->state++;
}

void func_actor_342400_80163178(Task* arg0)
{
    Actor342400ChildWork* work = (Actor342400ChildWork*)arg0->idMap;

    func_actor_342400_801621D8(arg0);
    if (work->field_A == 3) {
        D_actor_342400_8016BF58[(s16)(arg0->spawnArg1 >> 16)].field_6 = 2;
        Task_Kill(arg0);
    }
}

extern s8 D_80187329;

void func_actor_342400_801631DC(s16 arg0)
{
    if (arg0 == 0) {
        D_80187329 = 2;
        return;
    }
    D_80187329 = 0x11;
}

void func_actor_342400_80163200(s16 arg0, s16 arg1, s16 arg2)
{
    switch (arg1) {
        case 0:
            Task_SpawnFromTable(&D_actor_342400_8016BFE0, 1, (arg0 << 16) + arg2 + (func_actor_342400_801624A4() << 8), 0);
            break;
        case 1:
            Task_SpawnFromTable(&D_actor_342400_8016BFE0, 2, (arg0 << 16) + arg2 + (func_actor_342400_801624A4() << 8), 0);
            break;
        case 2:
            Task_SpawnFromTable(&D_actor_342400_8016BFE0, 3, (arg0 << 16) + arg2 + (func_actor_342400_801624A4() << 8), 0);
            break;
    }
}

void func_actor_342400_801632D4(Task* arg0)
{
    Actor342400ChildWork* work = (Actor342400ChildWork*)arg0->idMap;

    if (work->enemy0 != NULL) {
        if (work->enemy0->field_40 <= 0) {
            work->enemy0 = NULL;
        }
    } else {
        work->field_A |= 1;
    }
    if (work->enemy1 != NULL) {
        if (work->enemy1->field_40 <= 0) {
            work->enemy1 = NULL;
        }
    } else {
        work->field_A |= 2;
    }
}

INCLUDE_RODATA("actors/nonmatchings/actor_342400/actor_342400", D_actor_342400_80161E68);

INCLUDE_RODATA("actors/nonmatchings/actor_342400/actor_342400", D_actor_342400_80161E80);

INCLUDE_RODATA("actors/nonmatchings/actor_342400/actor_342400", D_actor_342400_80161EA8);

INCLUDE_RODATA("actors/nonmatchings/actor_342400/actor_342400", D_actor_342400_80161ED4);

INCLUDE_RODATA("actors/nonmatchings/actor_342400/actor_342400", D_actor_342400_80161EE0);

INCLUDE_RODATA("actors/nonmatchings/actor_342400/actor_342400", D_actor_342400_80161EEC);

INCLUDE_RODATA("actors/nonmatchings/actor_342400/actor_342400", D_actor_342400_80161F00);

INCLUDE_RODATA("actors/nonmatchings/actor_342400/actor_342400", D_actor_342400_80161F14);

INCLUDE_RODATA("actors/nonmatchings/actor_342400/actor_342400", jtbl_actor_342400_80161F28);

INCLUDE_RODATA("actors/nonmatchings/actor_342400/actor_342400", D_actor_342400_80161F50);
