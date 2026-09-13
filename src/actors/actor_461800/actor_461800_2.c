#include "common.h"

#include "actors/actor_461800.h"

INCLUDE_ASM("actors/nonmatchings/actor_461800/actor_461800_2", func_actor_461800_80132AD8);

INCLUDE_ASM("actors/nonmatchings/actor_461800/actor_461800_2", func_actor_461800_80132B74);

/// Ticks animation slots 1..0x13 of the actor's animation context.
void func_actor_461800_80132C28(void)
{
    s32 i;

    i = 1;
    do {
        Gp_AnimTickIndex(&D_actor_461800_80143894->anim, i);
        i++;
    } while (i < 0x14);
}

INCLUDE_ASM("actors/nonmatchings/actor_461800/actor_461800_2", func_actor_461800_80132C74);

INCLUDE_ASM("actors/nonmatchings/actor_461800/actor_461800_2", func_actor_461800_80132D04);

INCLUDE_ASM("actors/nonmatchings/actor_461800/actor_461800_2", func_actor_461800_80132D84);

INCLUDE_ASM("actors/nonmatchings/actor_461800/actor_461800_2", func_actor_461800_80132E14);

INCLUDE_ASM("actors/nonmatchings/actor_461800/actor_461800_2", func_actor_461800_80132EA4);

s32 func_actor_461800_80132F20(Task* arg0, s32 arg1, Actor461800Msg* arg2, s32 arg3)
{
    if (arg2->field_2 == 0) {
        D_actor_461800_80143894->field_4EC = 0x14;
    }
    return 0;
}

INCLUDE_ASM("actors/nonmatchings/actor_461800/actor_461800_2", func_actor_461800_80132F44);

INCLUDE_ASM("actors/nonmatchings/actor_461800/actor_461800_2", func_actor_461800_8013307C);

INCLUDE_ASM("actors/nonmatchings/actor_461800/actor_461800_2", func_actor_461800_801331E4);

INCLUDE_ASM("actors/nonmatchings/actor_461800/actor_461800_2", func_actor_461800_80133554);

INCLUDE_ASM("actors/nonmatchings/actor_461800/actor_461800_2", func_actor_461800_801335B0);
