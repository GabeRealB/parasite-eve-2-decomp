#include "common.h"
#include "main/task.h"
#include "main/mem.h"
#include "main/tmd.h"
#include "gameplay/D4.h"
#include "gameplay/3CD8.h"
#include "gameplay/3FB8.h"
#include "gameplay/gameplay.h"
#include "actors/actor.h"

/// 4-byte record in the table at `D_actor_342400_8016C010`, indexed (1..16)
/// by `gGameSession->enemyCullZone`. `func_actor_342400_801626CC` compares
/// an enemy's x against `limit` when `axis` is 0 and its z otherwise.
typedef struct Actor342400Limit {
    /* 0x0 */ s16 axis;
    /* 0x2 */ s16 limit;
} Actor342400Limit;
STATIC_ASSERT_SIZEOF(Actor342400Limit, 0x4);

extern TaskDesc             D_801575F0;                // absolute, spawned by func_actor_342400_80162DA0
extern u8                   D_actor_342400_8016BF48[]; // stored into `Task::msgTable` by func_actor_342400_801628F0
extern OverlayEncounterSlot D_actor_342400_8016BF58[];
extern TaskDesc             D_actor_342400_8016BFE0;
extern Actor342400Limit     D_actor_342400_8016C010[];
extern s16                  D_actor_342400_8016C054[][4]; // spawn variant per player-position band, 4 random picks
extern TaskDesc             D_actor_342400_80173A54;
extern u16                  D_actor_342400_80173AAC;      // spawn counter, `<< 12` into `GpEnemy::placeKey`

s16  func_actor_342400_801624A4(void);
s16  func_actor_342400_801626CC(s16 arg0, s16 arg1, s16 arg2);
void func_actor_342400_801628F0(Task* arg0);
void func_actor_342400_8016299C(Task* arg0);
void func_actor_342400_80162A34(Task* arg0);
void func_actor_342400_80162AB0(Task* arg0);
void func_actor_342400_80162B60(Task* arg0);
void func_actor_342400_80162C10(Task* arg0);
void func_actor_342400_80162CA8(Task* arg0);
void func_actor_342400_80162CBC(Task* arg0);
void func_actor_342400_80162DA0(Task* arg0);
void func_actor_342400_80162E6C(Task* arg0);
void func_actor_342400_80162F08(Task* arg0);
void func_actor_342400_80162F1C(Task* arg0);
void func_actor_342400_80162FFC(Task* arg0);
void func_actor_342400_80163010(Task* arg0);
void func_actor_342400_801630A4(Task* arg0);
void func_actor_342400_80163178(Task* arg0);
void func_actor_342400_80163200(s16 arg0, s16 arg1, s16 arg2);
void func_actor_342400_801632D4(Task* arg0);

void func_actor_342400_80162084(Task* arg0)
{
    OverlayEncounterPairWork* work;
    GpEnemy*                  enemy;
    Task*                     task;
    TmdObject*                obj;

    work = memCalloc(0xC, 0);
    if (work == NULL) {
        goto kill;
    }
    arg0->work   = (TaskIdMap*)work;
    work->enemy0 = Gp_SpawnEnemyFromTable(&D_80151E60, 1, 1, 0);
    work->enemy1 = Gp_SpawnEnemyFromTable(&D_80151E60, 1, 1, 0);
    if (work->enemy0 == NULL && work->enemy1 == NULL) {
    kill:
        taskKill(arg0);
        return;
    }
    if (work->enemy0 != NULL) {
        enemy           = work->enemy0;
        enemy->placeKey = D_actor_342400_80173AAC << 12;
        D_actor_342400_80173AAC++;
        task       = enemy->task;
        obj        = task->extra;
        obj->tpage = 3;
        obj->clut  = 5;
        enemy->hp  = 1;
    }
    if (work->enemy1 != NULL) {
        enemy           = work->enemy1;
        enemy->placeKey = D_actor_342400_80173AAC << 12;
        D_actor_342400_80173AAC++;
        task       = enemy->task;
        obj        = task->extra;
        obj->tpage = 3;
        obj->clut  = 5;
        enemy->hp  = 1;
    }
    D_actor_342400_8016BF58[(s16)(arg0->spawnArg1 >> 16)].status = 1;
    arg0->state++;
}

void func_actor_342400_801621D8(Task* arg0)
{
    OverlayEncounterPairWork* work = (OverlayEncounterPairWork*)arg0->work;
    GpEnemy*                  enemy;
    Task*                     task;
    GsCOORDINATE2*            coord;
    GpCmdArg                  msg;

    if (work->enemy0 != NULL) {
        enemy = work->enemy0;
        task  = enemy->task;
        coord = ((TmdObject*)task->extra)->coords;
        if (enemy->hp <= 0) {
            work->enemy0 = NULL;
        } else if (func_actor_342400_801626CC(gGameSession->enemyCullZone, coord->coord.t[0], coord->coord.t[2])) {
            msg.from.loc.stage = 0;
            msg.from.loc.area  = 0;
            msg.command        = 5;
            Gp_DispatchMsg(task, 0x7DB, (s32)&msg, 0);
            work->enemy0 = NULL;
        }
    } else {
        work->goneMask |= 1;
    }
    if (work->enemy1 != NULL) {
        enemy = work->enemy1;
        task  = enemy->task;
        coord = ((TmdObject*)task->extra)->coords;
        if (enemy->hp <= 0) {
            work->enemy1 = NULL;
        } else if (func_actor_342400_801626CC(gGameSession->enemyCullZone, coord->coord.t[0], coord->coord.t[2])) {
            msg.from.loc.stage = 0;
            msg.from.loc.area  = 0;
            msg.command        = 5;
            Gp_DispatchMsg(task, 0x7DB, (s32)&msg, 0);
            work->enemy1 = NULL;
        }
    } else {
        work->goneMask |= 2;
    }
}

void func_actor_342400_80162324(Task* arg0)
{
    OverlayEncounterCtrlWork* work = (OverlayEncounterCtrlWork*)arg0->work;
    s16                       count;
    s16                       i;
    s16                       idx;
    s16                       type;
    s16                       arg;

    count = 0;
    for (i = 0; i < 17; i++) {
        if (D_actor_342400_8016BF58[i].status == 1) {
            count++;
        }
    }
    if (count < 3) {
        idx = work->nextSlot;
        if (idx < 17 && gGameSession->sceneClock >= 0x3D) {
            type = D_actor_342400_8016BF58[idx].kind;
            arg  = D_actor_342400_8016BF58[idx].command;
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
            work->nextSlot++;
        }
    }
}

s16 func_actor_342400_801624A4(void)
{
    GsCOORDINATE2* coord = ((TmdObject*)(*Gp_ActorSlots)->extra)->coords;
    s16            x     = coord->coord.t[0];
    s16            z     = coord->coord.t[2];

    if (x <= 5000) {
        return D_actor_342400_8016C054[0][(Gp_LcgState = Gp_LcgState * 5 + 0x71357911) >> 16 & 3];
    }
    if (x <= 8000) {
        return D_actor_342400_8016C054[1][(Gp_LcgState = Gp_LcgState * 5 + 0x71357911) >> 16 & 3];
    }
    if (x <= 11000) {
        return D_actor_342400_8016C054[2][(Gp_LcgState = Gp_LcgState * 5 + 0x71357911) >> 16 & 3];
    }
    if (z >= -5500) {
        return D_actor_342400_8016C054[3][(Gp_LcgState = Gp_LcgState * 5 + 0x71357911) >> 16 & 3];
    }
    if (z >= -8500) {
        return D_actor_342400_8016C054[4][(Gp_LcgState = Gp_LcgState * 5 + 0x71357911) >> 16 & 3];
    }
    if (z >= -11500) {
        return D_actor_342400_8016C054[5][(Gp_LcgState = Gp_LcgState * 5 + 0x71357911) >> 16 & 3];
    }
    if (z >= -24500) {
        return D_actor_342400_8016C054[5][(Gp_LcgState = Gp_LcgState * 5 + 0x71357911) >> 16 & 3];
    }
    return D_actor_342400_8016C054[5][(Gp_LcgState = Gp_LcgState * 5 + 0x71357911) >> 16 & 3];
}

void func_actor_342400_801626AC(Task* arg0, s32 arg1, GpCmdArg* arg2)
{
    OverlayEncounterCtrlWork* work = (OverlayEncounterCtrlWork*)arg0->work;
    u16                       id   = arg2->command;

    if (id == 4) {
        work->stop = id;
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

/// The controller task's four state handlers, dispatched by
/// `func_actor_342400_80162748` on `Task::state`.
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
    if (Gp_StateF0.field_4 == 0) {
        sp.funcs[arg0->state](arg0);
    }
}

/// The first spawner task's four state handlers, dispatched by
/// `func_actor_342400_801627C0`.
const TaskFuncTable4 D_actor_342400_80161E34 = { {
    func_actor_342400_80162B60,
    func_actor_342400_80162C10,
    func_actor_342400_80162CA8,
    func_actor_342400_80162CBC,
} };

/// Runs the first spawner task's handler for its `Task::state`.
void func_actor_342400_801627C0(Task* arg0)
{
    TaskFuncTable4 sp;

    sp = D_actor_342400_80161E34;
    sp.funcs[arg0->state](arg0);
}

/// The second spawner task's four state handlers, dispatched by
/// `func_actor_342400_80162824`.
const TaskFuncTable4 D_actor_342400_80161E44 = { {
    func_actor_342400_80162DA0,
    func_actor_342400_80162E6C,
    func_actor_342400_80162F08,
    func_actor_342400_80162F1C,
} };

/// Runs the second spawner task's handler for its `Task::state`.
void func_actor_342400_80162824(Task* arg0)
{
    TaskFuncTable4 sp;

    sp = D_actor_342400_80161E44;
    sp.funcs[arg0->state](arg0);
}

/// The five state handlers `func_actor_342400_80162888` dispatches through by
/// `Task::state`.
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
    OverlayEncounterCtrlWork* work;
    s32                       i;

    if ((u8)gGameSession->spawnPhase[1] == 2 || (u8)gGameSession->spawnPhase[0] == 0 ||
        (work = memCalloc(6, 0)) == NULL) {
        taskKill(arg0);
        return;
    }
    for (i = 16; i >= 0; i--) {
        D_actor_342400_8016BF58[i].status = 0;
    }
    D_actor_342400_80173AAC = 0;
    arg0->work              = (TaskIdMap*)work;
    arg0->msgTable          = D_actor_342400_8016BF48;
    arg0->state++;
}

void func_actor_342400_8016299C(Task* arg0)
{
    s32                       i;
    OverlayEncounterCtrlWork* work = (OverlayEncounterCtrlWork*)arg0->work;
    OverlayEncounterSlot*     slot;

    for (i = 0; i < 3; i++) {
        slot = &D_actor_342400_8016BF58[work->nextSlot];
        func_actor_342400_80163200(work->nextSlot, slot->kind, slot->command);
        work->nextSlot++;
    }
    arg0->state++;
}

void func_actor_342400_80162A34(Task* arg0)
{
    OverlayEncounterCtrlWork* work = (OverlayEncounterCtrlWork*)arg0->work;

    if (++work->frames == 15) {
        ((void (*)(s32))Gp_IncStateF0Ref)(0);
        gGameSession->spawnPhase[1] = 1;
        Gp_ArmStateF0(1);
        arg0->state++;
    }
}

void func_actor_342400_80162AB0(Task* arg0)
{
    OverlayEncounterCtrlWork* work = (OverlayEncounterCtrlWork*)arg0->work;
    s16                       count;
    s32                       i;

    count = 0;
    if (work->stop != 4) {
        func_actor_342400_80162324(arg0);
        for (i = 0; i < 17; i++) {
            if (D_actor_342400_8016BF58[i].status == 2) {
                count++;
            }
        }
        if (count == 17) {
            ((void (*)(Task*, s32))Gp_ReleaseStateF0Clear)(arg0, 0);
            gGameSession->spawnPhase[1] = 2;
            taskKill(arg0);
        }
    }
}

void func_actor_342400_80162B60(Task* arg0)
{
    OverlayEncounterSingleWork* work;
    GpEnemy*                    enemy;

    work = memCalloc(8, 0);
    if (work != NULL) {
        arg0->work = (TaskIdMap*)work;
        enemy      = Gp_SpawnEnemyFromTable(&D_actor_342400_80173A54, 1, 0, 0);
        if (enemy != NULL) {
            D_actor_342400_8016BF58[(s16)(arg0->spawnArg1 >> 16)].status = 1;
            work->enemy                                                  = enemy;
            enemy->placeKey                                              = D_actor_342400_80173AAC << 12;
            D_actor_342400_80173AAC++;
            arg0->state++;
            return;
        }
    }
    taskKill(arg0);
}

void func_actor_342400_80162C10(Task* arg0)
{
    OverlayEncounterSingleWork* work = (OverlayEncounterSingleWork*)arg0->work;
    GpEnemy*                    enemy;
    Task*                       task;
    TmdObject*                  obj;
    GpCmdArg                    msg;

    enemy = work->enemy;
    task  = enemy->task;
    if (++work->frames > 60) {
        obj                = task->extra;
        obj->clut          = 2;
        obj->tpage         = 0;
        enemy->workType    = 0x900;
        msg.from.loc.stage = 0;
        msg.from.loc.area  = 0x2C;
        msg.command        = arg0->spawnArg1;
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
    OverlayEncounterSingleWork* work = (OverlayEncounterSingleWork*)arg0->work;
    GpEnemy*                    enemy;
    Task*                       task;
    GsCOORDINATE2*              coord;
    GpCmdArg                    msg;

    enemy = work->enemy;
    task  = enemy->task;
    coord = ((TmdObject*)task->extra)->coords;
    if (enemy->hp <= 0) {
        D_actor_342400_8016BF58[(s16)(arg0->spawnArg1 >> 16)].status = 2;
        taskKill(arg0);
        return;
    }
    if (func_actor_342400_801626CC(gGameSession->enemyCullZone, coord->coord.t[0], coord->coord.t[2])) {
        msg.from.loc.stage = 0;
        msg.from.loc.area  = 0x2C;
        msg.command        = 5;
        Gp_DispatchMsg(task, 0x7DB, (s32)&msg, 0);
        D_actor_342400_8016BF58[(s16)(arg0->spawnArg1 >> 16)].status = 2;
        taskKill(arg0);
    }
}

void func_actor_342400_80162DA0(Task* arg0)
{
    OverlayEncounterSingleWork* work;
    GpEnemy*                    enemy;
    TmdObject*                  obj;

    work = memCalloc(8, 0);
    if (work != NULL) {
        arg0->work = (TaskIdMap*)work;
        enemy      = Gp_SpawnEnemyFromTable(&D_801575F0, 2, 0, 0);
        if (enemy != NULL) {
            D_actor_342400_8016BF58[(s16)(arg0->spawnArg1 >> 16)].status = 1;
            work->enemy                                                  = enemy;
            enemy->placeKey                                              = D_actor_342400_80173AAC << 12;
            D_actor_342400_80173AAC++;
            obj        = enemy->task->extra;
            obj->tpage = 2;
            obj->clut  = 4;
            arg0->state++;
            return;
        }
    }
    taskKill(arg0);
}

void func_actor_342400_80162E6C(Task* arg0)
{
    OverlayEncounterSingleWork* work = (OverlayEncounterSingleWork*)arg0->work;
    GpEnemy*                    enemy;
    Task*                       task;
    TmdObject*                  obj;
    GpCmdArg                    msg;

    enemy = work->enemy;
    task  = enemy->task;
    if (++work->frames > 60) {
        obj                = task->extra;
        obj->tpage         = 2;
        obj->clut          = 4;
        enemy->workType    = 0x900;
        msg.from.loc.stage = 0;
        msg.from.loc.area  = 0x2A;
        msg.command        = arg0->spawnArg1;
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
    OverlayEncounterSingleWork* work = (OverlayEncounterSingleWork*)arg0->work;
    GpEnemy*                    enemy;
    Task*                       task;
    GsCOORDINATE2*              coord;
    GpCmdArg                    msg;

    enemy = work->enemy;
    task  = enemy->task;
    coord = ((TmdObject*)task->extra)->coords;
    if (enemy->hp <= 0) {
        D_actor_342400_8016BF58[(s16)(arg0->spawnArg1 >> 16)].status = 2;
        taskKill(arg0);
        return;
    }
    if (func_actor_342400_801626CC(gGameSession->enemyCullZone, coord->coord.t[0], coord->coord.t[2])) {
        msg.from.loc.stage = 0;
        msg.from.loc.area  = 0;
        msg.command        = 5;
        Gp_DispatchMsg(task, 0x7DB, (s32)&msg, 0);
        D_actor_342400_8016BF58[(s16)(arg0->spawnArg1 >> 16)].status = 2;
        taskKill(arg0);
    }
}

void func_actor_342400_80162FFC(Task* arg0)
{
    arg0->state = arg0->state + 1;
}

void func_actor_342400_80163010(Task* arg0)
{
    OverlayEncounterPairWork* work = (OverlayEncounterPairWork*)arg0->work;
    GpEnemy*                  enemy;
    Task*                     task;
    TmdObject*                obj;
    GpCmdArg                  msg;

    enemy = work->enemy0;
    if (enemy != NULL) {
        task               = enemy->task;
        obj                = task->extra;
        obj->tpage         = 3;
        obj->clut          = 5;
        enemy->workType    = 0x900;
        msg.from.loc.stage = 0;
        msg.from.loc.area  = 0x2E;
        msg.command        = arg0->spawnArg1;
        Gp_DispatchMsg(task, 0x7DB, (s32)&msg, 0);
    }
    work->frames = 0;
    arg0->state++;
}

void func_actor_342400_801630A4(Task* arg0)
{
    OverlayEncounterPairWork* work = (OverlayEncounterPairWork*)arg0->work;
    GpEnemy*                  enemy;
    Task*                     task;
    TmdObject*                obj;
    GpCmdArg                  msg;

    enemy = work->enemy1;
    func_actor_342400_801632D4(arg0);
    if (work->enemy1 != NULL) {
        if (++work->frames <= 0x3C) {
            return;
        }
        task               = work->enemy1->task;
        obj                = task->extra;
        obj->tpage         = 3;
        obj->clut          = 5;
        enemy->workType    = 0x900;
        msg.from.loc.stage = 0;
        msg.from.loc.area  = 0x2E;
        msg.command        = arg0->spawnArg1;
        Gp_DispatchMsg(task, 0x7DB, (s32)&msg, 0);
    }
    work->frames = 0;
    arg0->state++;
}

void func_actor_342400_80163178(Task* arg0)
{
    OverlayEncounterPairWork* work = (OverlayEncounterPairWork*)arg0->work;

    func_actor_342400_801621D8(arg0);
    if (work->goneMask == 3) {
        D_actor_342400_8016BF58[(s16)(arg0->spawnArg1 >> 16)].status = 2;
        taskKill(arg0);
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
    OverlayEncounterPairWork* work = (OverlayEncounterPairWork*)arg0->work;

    if (work->enemy0 != NULL) {
        if (work->enemy0->hp <= 0) {
            work->enemy0 = NULL;
        }
    } else {
        work->goneMask |= 1;
    }
    if (work->enemy1 != NULL) {
        if (work->enemy1->hp <= 0) {
            work->enemy1 = NULL;
        }
    } else {
        work->goneMask |= 2;
    }
}
