#include "common.h"

#include "gameplay/1A8.h"
#include "gameplay/3CD8.h"

#include "main/gameflag.h"
#include "main/task.h"
extern s32 D_neo_ark_pyramid_801818A4;

extern TaskDesc D_neo_ark_pyramid_8017FC0C;

void func_neo_ark_pyramid_8017DAC0(s32 arg0);

s32 func_neo_ark_pyramid_8017DA3C(void)
{
    return 0;
}

s32 func_neo_ark_pyramid_8017DA44(Task* task, s32 msgId, GpMsg13EF* arg2)
{
    if (arg2->field_2 == 1) {
        func_neo_ark_pyramid_8017DAC0(0);
        if (GameFlag_GetNibble(0xEC) == 4) {
            Gp_SpawnIfCapIdle(3, 1);
        } else {
            Gp_MsgPlayerWeapon(0);
            Gp_MsgPlayer3F3(0);
            Task_SpawnFromTable(&D_neo_ark_pyramid_8017FC0C, 0, 0, 0);
        }
    }
    return 0;
}

void func_neo_ark_pyramid_8017DAC0(s32 arg0)
{
    D_neo_ark_pyramid_801818A4 = (((GameFlag_GetNibble(0xEC) - 4) << 0xC) / 12) + arg0;
}

INCLUDE_ASM("rooms/nonmatchings/neo_ark_pyramid/neo_ark_pyramid_2", func_neo_ark_pyramid_8017DB18);

INCLUDE_ASM("rooms/nonmatchings/neo_ark_pyramid/neo_ark_pyramid_2", func_neo_ark_pyramid_8017DB5C);

INCLUDE_ASM("rooms/nonmatchings/neo_ark_pyramid/neo_ark_pyramid_2", func_neo_ark_pyramid_8017DB98);

INCLUDE_ASM("rooms/nonmatchings/neo_ark_pyramid/neo_ark_pyramid_2", func_neo_ark_pyramid_8017DBF0);

INCLUDE_ASM("rooms/nonmatchings/neo_ark_pyramid/neo_ark_pyramid_2", func_neo_ark_pyramid_8017DC50);
