#include "common.h"

#include "main/task.h"
#include "main/tmd.h"

INCLUDE_ASM("actors/nonmatchings/actor_560800/actor_560800_4", func_actor_560800_80136AA8);

INCLUDE_ASM("actors/nonmatchings/actor_560800/actor_560800_4", func_actor_560800_801376E0);

INCLUDE_ASM("actors/nonmatchings/actor_560800/actor_560800_4", func_actor_560800_80137820);

INCLUDE_ASM("actors/nonmatchings/actor_560800/actor_560800_4", func_actor_560800_80137BEC);

INCLUDE_ASM("actors/nonmatchings/actor_560800/actor_560800_4", func_actor_560800_80137F58);

INCLUDE_ASM("actors/nonmatchings/actor_560800/actor_560800_4", func_actor_560800_801384EC);

INCLUDE_ASM("actors/nonmatchings/actor_560800/actor_560800_4", func_actor_560800_801386D4);

INCLUDE_ASM("actors/nonmatchings/actor_560800/actor_560800_4", func_actor_560800_80138A4C);

INCLUDE_ASM("actors/nonmatchings/actor_560800/actor_560800_4", func_actor_560800_80138BCC);

INCLUDE_ASM("actors/nonmatchings/actor_560800/actor_560800_4", func_actor_560800_80138D04);

INCLUDE_ASM("actors/nonmatchings/actor_560800/actor_560800_4", func_actor_560800_80138FC8);

INCLUDE_ASM("actors/nonmatchings/actor_560800/actor_560800_4", func_actor_560800_80139360);

void func_actor_560800_801393EC(Task* task, s32 arg1, s32 arg2)
{
    TmdObject* extra;

    extra = (TmdObject*)task->extra;
    switch (arg2) {
        case 0:
            break;
        case 1:
            extra->field_C = extra->field_C & 0xFF7B;
            return;
        case 2:
            extra->field_C = extra->field_C | 0x84;
            return;
    }
}

INCLUDE_ASM("actors/nonmatchings/actor_560800/actor_560800_4", func_actor_560800_80139440);
