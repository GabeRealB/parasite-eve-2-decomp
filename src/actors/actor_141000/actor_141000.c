#include "common.h"

#include "actors/actor_141000.h"
#include "actors/actors_shared_80162850.h"

#include "main/mem.h"
#include "main/task.h"
#include "main/tmd.h"

/// The actor's three state handlers - spawn/setup, per-frame tick and
/// teardown - dispatched through by state.
extern TaskFuncTable3 D_actor_141000_80131E30;

/// The descriptor table the controller spawns from: index 1 is the task this
/// state spawns and index 2 the model actor `func_actor_141000_80132EF4`
/// spawns later, every eighth frame.
extern TaskDesc D_actor_141000_801348D8[];

INCLUDE_ASM("actors/nonmatchings/actor_141000/actor_141000", func_actor_141000_80131E94);

INCLUDE_ASM("actors/nonmatchings/actor_141000/actor_141000", func_actor_141000_801323F0);

INCLUDE_RODATA("actors/nonmatchings/actor_141000/actor_141000", D_actor_141000_80131E20);

INCLUDE_RODATA("actors/nonmatchings/actor_141000/actor_141000", D_actor_141000_80131E24);

INCLUDE_RODATA("actors/nonmatchings/actor_141000/actor_141000", D_actor_141000_80131E30);

void func_actor_141000_80132C24(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_actor_141000_80131E30;
    sp.funcs[task->state](task);
}

/// Spawn state of the overlay's controller task: takes the display object's
/// root coordinate, allocates the work block the later states read through
/// `Task::idMap` and arms it at step 0xFFF, un-parks the model (`field_C` bit
/// 0x80 is the flag that keeps a `TmdObject` out of the coordinate update),
/// republishes that coordinate onto the two scale helpers, spawns the attach
/// task from `D_actor_141000_801348D8` and hands the controller the shared kill
/// callback before advancing to the per-frame state. A failed allocation kills
/// the task instead of leaving a half-built controller behind.
void func_actor_141000_80132C7C(Task* task)
{
    Actor141000CtrlWork* work;
    TmdObject*           obj;
    GsCOORDINATE2*       coord;

    obj   = task->extra;
    coord = obj->field_8;
    work  = Mem_Calloc(0x10, 0);
    if (work == NULL) {
        Task_Kill(task);
        return;
    }
    task->idMap   = (TaskIdMap*)work;
    work->field_0 = 0xFFF;
    obj->field_C &= 0xFF7F;
    func_actor_141000_80132FD0(coord, 0);
    func_actor_141000_8013308C(coord, 0);
    Task_SpawnFromTable(&D_actor_141000_801348D8, 1, 0, (s32)task);
    task->exitCallback = ActorsShared80162850;
    task->state       += 1;
}

INCLUDE_ASM("actors/nonmatchings/actor_141000/actor_141000", func_actor_141000_80132D3C);
