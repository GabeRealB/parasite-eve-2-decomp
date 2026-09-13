#include "common.h"

#include "main/task.h"
#include "gameplay/3CD8.h"
#include "actors/actor_206100.h"

INCLUDE_ASM("actors/nonmatchings/actor_206100/actor_206100_2", func_actor_206100_8014F18C);

void func_actor_206100_8014F284(Task* task)
{
    Actor206100Work*       work;
    Actor206100AnimStride* stride;
    s32                    i;

    work   = (Actor206100Work*)task->idMap;
    i      = 1;
    stride = (Actor206100AnimStride*)work + 1;
    do {
        Gp_AnimResetSlot(&work->anim, i, work->field_510);
        i++;
        stride->field_1D = (u8)work->field_51A;
        stride++;
    } while (i < 0xF);
    work->field_50E = (u16)work->field_510;
}

INCLUDE_ASM("actors/nonmatchings/actor_206100/actor_206100_2", func_actor_206100_8014F2F0);

INCLUDE_ASM("actors/nonmatchings/actor_206100/actor_206100_2", func_actor_206100_8014F3C8);

INCLUDE_ASM("actors/nonmatchings/actor_206100/actor_206100_2", func_actor_206100_8014F428);
