#include "common.h"

#include "actors/actor_141000.h"

#include "main/task.h"
#include "main/tmd.h"

void func_actor_141000_80133A68(Task* arg0);
void func_actor_141000_80133B28(Task* arg0);
void func_actor_141000_80133490(Task* arg0);
void func_actor_141000_80133BD8(Task* arg0);

/// The four main-body handlers, dispatched by state in
/// `func_actor_141000_80133A00`. It opens this unit's .rodata, so a C
/// definition lands at the address the split gave it.
const TaskFuncTable4 D_actor_141000_80131E58 = { {
    func_actor_141000_80133A68,
    func_actor_141000_80133B28,
    func_actor_141000_80133490,
    func_actor_141000_80133BD8,
} };

void func_actor_141000_801339DC(Task* arg0)
{
    TmdObject*       ext;
    Actor141000Work* work;

    ext           = arg0->extra;
    work          = (Actor141000Work*)arg0->idMap;
    ext->field_1C = &work->light;
    ext->field_20 = &work->color;
}

void func_actor_141000_801339F8(void)
{
}

/// Dispatches the actor's four main-body handlers by state.
void func_actor_141000_80133A00(Task* arg0)
{
    TaskFuncTable4   sp;
    Actor141000Work* work;

    work = (Actor141000Work*)arg0->idMap;
    sp   = D_actor_141000_80131E58;
    sp.funcs[work->field_4C2](arg0);
}

INCLUDE_ASM("actors/nonmatchings/actor_141000/actor_141000_3", func_actor_141000_80133A68);

INCLUDE_ASM("actors/nonmatchings/actor_141000/actor_141000_3", func_actor_141000_80133B28);

INCLUDE_ASM("actors/nonmatchings/actor_141000/actor_141000_3", func_actor_141000_80133BD8);

INCLUDE_ASM("actors/nonmatchings/actor_141000/actor_141000_3", func_actor_141000_80133CD8);
