#include "common.h"

#include "actors/actor_310600.h"
#include "gameplay/1BC.h"
#include "main/mem.h"
#include "main/task.h"
#include "main/tmd.h"

/// The actor's three state handlers - spawn/setup, per-frame tick and
/// teardown - dispatched through by state.
extern TaskFuncTable3 D_actor_310600_80161E24;

void func_actor_310600_80161E64(Task* task)
{
    Actor310600Work* work;
    GpObj*           obj;

    work = Mem_Calloc(0x538, 0);
    if (work == NULL) {
        Gp_EnemyTaskExit(task);
        return;
    }
    task->idMap     = (TaskIdMap*)work;
    work->field_475 = -1;
    work->field_476 = -1;
    work->field_477 = -1;
    work->field_47C = 0;
    work->field_47E = 0;
    work->field_518 = 0;
    work->field_51C = 0;
    work->field_520 = 0;
    Task_SpawnFromTable(D_actor_310600_801796A4, 1, 8, (s32)task);
    func_actor_310600_80162A58(task);
    obj           = &work->obj;
    obj->field_8  = &((TmdObject*)task->extra)->field_8[1];
    obj->field_C  = &work->rec;
    obj->field_18 = 0x30000;
    obj->field_1C = 0x100;
    obj->field_10 = 0;
    obj->field_12 = 0;
    obj->field_14 = 0;
    obj->flags    = 1;
    Gp_LinkObj(2, obj);
    obj->flags |= 0x8000;
    Gp_InitRec18Table(obj->field_C, 1, 0);
    task->field_24 = D_actor_310600_801796BC;
    func_actor_310600_801625F0(task, 0x7D5, 0, 0);
    task->exitCallback = func_actor_310600_80162A24;
    task->state++;
}

INCLUDE_ASM("actors/nonmatchings/actor_310600/actor_310600", func_actor_310600_80161FA0);

INCLUDE_ASM("actors/nonmatchings/actor_310600/actor_310600", func_actor_310600_8016231C);

INCLUDE_ASM("actors/nonmatchings/actor_310600/actor_310600", func_actor_310600_8016246C);

INCLUDE_ASM("actors/nonmatchings/actor_310600/actor_310600", func_actor_310600_801625F0);

INCLUDE_RODATA("actors/nonmatchings/actor_310600/actor_310600", D_actor_310600_80161E20);

INCLUDE_RODATA("actors/nonmatchings/actor_310600/actor_310600", D_actor_310600_80161E24);

void func_actor_310600_8016274C(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_actor_310600_80161E24;
    sp.funcs[task->state](task);
}

INCLUDE_ASM("actors/nonmatchings/actor_310600/actor_310600", func_actor_310600_801627A4);

INCLUDE_RODATA("actors/nonmatchings/actor_310600/actor_310600", D_actor_310600_80161E3C);

INCLUDE_RODATA("actors/nonmatchings/actor_310600/actor_310600", D_actor_310600_80161E48);

INCLUDE_RODATA("actors/nonmatchings/actor_310600/actor_310600", D_actor_310600_80161E54);
