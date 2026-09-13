#include "common.h"

#include "actors/actor_310100.h"
#include "main/display.h"
#include "main/gfx.h"
#include "main/task.h"
#include "main/tmd.h"

extern TaskDesc D_actor_310100_801798E4;

INCLUDE_ASM("actors/nonmatchings/actor_310100/actor_310100", func_actor_310100_80161E24);

INCLUDE_ASM("actors/nonmatchings/actor_310100/actor_310100", func_actor_310100_80161F80);

INCLUDE_ASM("actors/nonmatchings/actor_310100/actor_310100", func_actor_310100_801620FC);

INCLUDE_ASM("actors/nonmatchings/actor_310100/actor_310100", func_actor_310100_80162284);

INCLUDE_ASM("actors/nonmatchings/actor_310100/actor_310100", func_actor_310100_80162414);

INCLUDE_ASM("actors/nonmatchings/actor_310100/actor_310100", func_actor_310100_801625E4);

INCLUDE_ASM("actors/nonmatchings/actor_310100/actor_310100", func_actor_310100_801627BC);

INCLUDE_ASM("actors/nonmatchings/actor_310100/actor_310100", func_actor_310100_801629FC);

void func_actor_310100_80162C64(Task* task, s32 msgId, s32 arg2, Actor310100Placement* placement)
{
    Actor310100Work* work;

    work = (Actor310100Work*)task->idMap;
    if (work->field_4E4 != NULL) {
        Task_Kill(work->field_4E4);
    }
    work->field_506 = placement->pos.vy;
    Display_SpawnWithOt(&D_actor_310100_801798E4, 0, arg2, (s32)task);
}

INCLUDE_ASM("actors/nonmatchings/actor_310100/actor_310100", func_actor_310100_80162CDC);

INCLUDE_ASM("actors/nonmatchings/actor_310100/actor_310100", func_actor_310100_80162D50);

/// Message 0x7D4 handler: drops the payload's translation into the display
/// task's root coordinate frame, yaws that frame to the payload's `rot.vy` and
/// marks it dirty.
void func_actor_310100_80162EC8(Task* task, s32 msgId, Actor310100Placement* placement)
{
    Actor310100Work* work;
    GsCOORDINATE2*   coord;

    work              = (Actor310100Work*)task->idMap;
    coord             = ((TmdObject*)work->field_4E4->extra)->field_8;
    coord->coord.t[0] = placement->pos.vx;
    coord->coord.t[1] = placement->pos.vy;
    coord->coord.t[2] = placement->pos.vz;
    Gfx_RotMatrixY(&coord->coord, placement->rot.vy, 0);
    coord->flg = 0;
}

/// Teardown handler: kills the display task hanging off the work block and
/// parks this task in state 3.
void func_actor_310100_80162F34(Task* task)
{
    Actor310100Work* work;

    work = (Actor310100Work*)task->idMap;
    if (work->field_4E4 != NULL) {
        Task_Kill(work->field_4E4);
        work->field_4E4 = NULL;
    }
    task->state = 3;
}

INCLUDE_ASM("actors/nonmatchings/actor_310100/actor_310100", func_actor_310100_80162F88);

INCLUDE_ASM("actors/nonmatchings/actor_310100/actor_310100", func_actor_310100_8016309C);

INCLUDE_ASM("actors/nonmatchings/actor_310100/actor_310100", func_actor_310100_801631B0);

INCLUDE_ASM("actors/nonmatchings/actor_310100/actor_310100", func_actor_310100_801632B0);

INCLUDE_RODATA("actors/nonmatchings/actor_310100/actor_310100", D_actor_310100_80161E20);
