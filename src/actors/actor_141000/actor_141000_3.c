#include "common.h"

#include "actors/actor_141000.h"
#include "actors/actors_shared_80133a68.h"

#include "main/task.h"
#include "main/tmd.h"

void func_actor_141000_80133B28(Task* arg0);
void func_actor_141000_80133490(Task* arg0);
void func_actor_141000_80133BD8(Task* arg0);

/// The four main-body handlers, dispatched by state in
/// `func_actor_141000_80133A00`. It opens this unit's .rodata, so a C
/// definition lands at the address the split gave it.
const TaskFuncTable4 D_actor_141000_80131E58 = { {
    ActorsShared80133a68,
    func_actor_141000_80133B28,
    func_actor_141000_80133490,
    func_actor_141000_80133BD8,
} };

/// The scratch vector `func_actor_141000_80133B28` seeds from; its rodata run
/// follows this unit's handler table, so it is included here rather than with
/// the function, which the shared cut moved on to `actor_141000_4`.
INCLUDE_RODATA("actors/nonmatchings/actor_141000/actor_141000_3", D_actor_141000_80131E68);

void func_actor_141000_801339DC(Task* arg0)
{
    TmdObject*       ext;
    Actor141000Work* work;

    ext           = arg0->extra;
    work          = (Actor141000Work*)arg0->work;
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

    work = (Actor141000Work*)arg0->work;
    sp   = D_actor_141000_80131E58;
    sp.funcs[(s16)work->field_4C2](arg0);
}
