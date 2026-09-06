#include "common.h"

#include "actors/actor_303600.h"
#include "main/task.h"

extern Task* D_actor_303600_8016E4C0;
INCLUDE_RODATA("actors/nonmatchings/actor_303600/actor_303600", D_actor_303600_80161E20);

INCLUDE_ASM("actors/nonmatchings/actor_303600/actor_303600", func_actor_303600_80161F40);

INCLUDE_ASM("actors/nonmatchings/actor_303600/actor_303600", func_actor_303600_8016216C);

INCLUDE_ASM("actors/nonmatchings/actor_303600/actor_303600", func_actor_303600_801622E8);

INCLUDE_ASM("actors/nonmatchings/actor_303600/actor_303600", func_actor_303600_801623CC);

INCLUDE_ASM("actors/nonmatchings/actor_303600/actor_303600", func_actor_303600_801624B0);

INCLUDE_ASM("actors/nonmatchings/actor_303600/actor_303600", func_actor_303600_8016253C);

void func_actor_303600_80162600(s16 arg0)
{
    Actor303600Work* work = (Actor303600Work*)D_actor_303600_8016E4C0->idMap;

    work->command = arg0;
    work->field_6 = 0;
}

INCLUDE_ASM("actors/nonmatchings/actor_303600/actor_303600", func_actor_303600_80162620);
