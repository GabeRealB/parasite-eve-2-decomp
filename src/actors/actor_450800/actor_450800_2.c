#include "common.h"

#include "actors/actor_450800.h"

/// `func_800B4114` is deliberately declared locally with a signed `arg2`; see
/// the note in `include/gameplay/1BC.h`.
void func_800B4114(GpAnimCtx* arg0, s32 arg1, s32 arg2, s32 arg3, s32 arg4);

void func_actor_450800_80132AE0(Task* task)
{
    Actor450800Work* work;
    s32              i;

    work = (Actor450800Work*)task->idMap;
    i    = 1;
    do {
        func_800B4114(&work->anim, i, work->field_4B8, 0, work->field_4FC);
        i++;
    } while (i < 0x14);
    work->field_4B6 = work->field_4B8;
}

INCLUDE_ASM("actors/nonmatchings/actor_450800/actor_450800_2", func_actor_450800_80132B44);

INCLUDE_ASM("actors/nonmatchings/actor_450800/actor_450800_2", func_actor_450800_80132BB0);
