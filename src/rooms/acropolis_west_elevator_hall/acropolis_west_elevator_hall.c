#include "common.h"

#include "gameplay/1A8.h"
#include "main/gameflag.h"
#include "main/task.h"

extern s8 D_8007272D;

extern Task* D_acropolis_west_elevator_hall_80186AE4[];

INCLUDE_ASM("rooms/nonmatchings/acropolis_west_elevator_hall/acropolis_west_elevator_hall", func_acropolis_west_elevator_hall_8017D5FC);

INCLUDE_ASM("rooms/nonmatchings/acropolis_west_elevator_hall/acropolis_west_elevator_hall", func_acropolis_west_elevator_hall_8017D7B0);

INCLUDE_RODATA("rooms/nonmatchings/acropolis_west_elevator_hall/acropolis_west_elevator_hall", D_acropolis_west_elevator_hall_8017D5C0);

INCLUDE_ASM("rooms/nonmatchings/acropolis_west_elevator_hall/acropolis_west_elevator_hall", func_acropolis_west_elevator_hall_8017F134);

INCLUDE_ASM("rooms/nonmatchings/acropolis_west_elevator_hall/acropolis_west_elevator_hall", func_acropolis_west_elevator_hall_8017F304);

INCLUDE_ASM("rooms/nonmatchings/acropolis_west_elevator_hall/acropolis_west_elevator_hall", func_acropolis_west_elevator_hall_8017F354);

INCLUDE_RODATA("rooms/nonmatchings/acropolis_west_elevator_hall/acropolis_west_elevator_hall", D_acropolis_west_elevator_hall_8017D5D4);

INCLUDE_ASM("rooms/nonmatchings/acropolis_west_elevator_hall/acropolis_west_elevator_hall", func_acropolis_west_elevator_hall_8017F418);

s32 func_acropolis_west_elevator_hall_8017F470(void)
{
    D_acropolis_west_elevator_hall_80186AE4[0]->spawnArg1 = 1;
    D_acropolis_west_elevator_hall_80186AE4[1]->spawnArg1 = 1;
    return 0;
}

s32 func_acropolis_west_elevator_hall_8017F498(void)
{
    D_acropolis_west_elevator_hall_80186AE4[0]->spawnArg1 = -1;
    D_acropolis_west_elevator_hall_80186AE4[1]->spawnArg1 = -1;
    return 0;
}

s32 func_acropolis_west_elevator_hall_8017F4C0(Task* task, s32 msgId, GpSaveLoc* src, GpSaveLoc* dst)
{
    *dst = *src;
    if (*(u16*)src == 1 && GameFlag_GetNibble(0x21) == 0 && src->field_5 == 0) {
        GameFlag_SetNibble(0x21, 1);
        D_8007272D   = 1;
        dst->field_2 = 7;
    }
    return 1;
}

INCLUDE_RODATA("rooms/nonmatchings/acropolis_west_elevator_hall/acropolis_west_elevator_hall", D_acropolis_west_elevator_hall_8017D5EC);

INCLUDE_RODATA("rooms/nonmatchings/acropolis_west_elevator_hall/acropolis_west_elevator_hall", D_acropolis_west_elevator_hall_8017D5F4);
