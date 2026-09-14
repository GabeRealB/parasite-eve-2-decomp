#include "common.h"

#include "actors/actor_317000.h"

#include "main/task.h"
#include "main/tmd.h"

void func_actor_317000_80162744(Task* arg0)
{
    TmdObject*       ext;
    Actor317000Work* work;

    ext           = arg0->extra;
    work          = (Actor317000Work*)arg0->idMap;
    ext->field_1C = &work->light;
    ext->field_20 = &work->color;
}

void func_actor_317000_80162760(void)
{
}

INCLUDE_ASM("actors/nonmatchings/actor_317000/actor_317000_3", func_actor_317000_80162768);

INCLUDE_ASM("actors/nonmatchings/actor_317000/actor_317000_3", func_actor_317000_801627D0);

/// State handler at index 1 of `D_actor_317000_80161E30`, reached by the
/// `field_4C2` advance this body ends with: rotates the constant local-space
/// offset `D_actor_317000_80161E40` through the root part's matrix into
/// `work->step`, then raises the flag at 0x4C4 and moves the dispatcher on.
/// The same body as `func_actor_335800_80163CA0` / `ActorsShared80132920`,
/// minus their per-axis stop threshold.
void func_actor_317000_801628D8(Task* task)
{
    Actor317000Work* work;
    GsCOORDINATE2*   coord;
    VECTOR           vec;

    coord = ((TmdObject*)task->extra)->field_8;
    work  = (Actor317000Work*)task->idMap;

    vec = D_actor_317000_80161E40;
    ApplyMatrixLV(&coord->coord, &vec, (VECTOR*)&work->step);
    work->field_4C4 = 1;
    work->field_4C2++;
}

INCLUDE_ASM("actors/nonmatchings/actor_317000/actor_317000_3", func_actor_317000_80162950);

INCLUDE_ASM("actors/nonmatchings/actor_317000/actor_317000_3", func_actor_317000_80162A10);

INCLUDE_RODATA("actors/nonmatchings/actor_317000/actor_317000_3", D_actor_317000_80161E50);

INCLUDE_RODATA("actors/nonmatchings/actor_317000/actor_317000_3", D_actor_317000_80161E60);

INCLUDE_RODATA("actors/nonmatchings/actor_317000/actor_317000_3", D_actor_317000_80161E64);
