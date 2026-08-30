#include "common.h"

#include "gameplay/3CD8.h"
#include "gameplay/D4.h"
#include "main/gameflag.h"
#include "main/task.h"

extern s32 D_dryfield_night_garage_80182DE0;
extern s32 D_dryfield_night_garage_80182DE4;

s32 func_800D4D2C(s32 arg0);
s32 func_dryfield_night_garage_80180A64(s32 arg0);

INCLUDE_ASM("rooms/nonmatchings/dryfield_night_garage/dryfield_night_garage", func_dryfield_night_garage_8017DDC4);

INCLUDE_ASM("rooms/nonmatchings/dryfield_night_garage/dryfield_night_garage", func_dryfield_night_garage_8017E250);

INCLUDE_ASM("rooms/nonmatchings/dryfield_night_garage/dryfield_night_garage", func_dryfield_night_garage_8017E39C);

INCLUDE_RODATA("rooms/nonmatchings/dryfield_night_garage/dryfield_night_garage", D_dryfield_night_garage_8017D5C0);

INCLUDE_ASM("rooms/nonmatchings/dryfield_night_garage/dryfield_night_garage", func_dryfield_night_garage_8017E768);

INCLUDE_ASM("rooms/nonmatchings/dryfield_night_garage/dryfield_night_garage", func_dryfield_night_garage_8017E9B8);

INCLUDE_RODATA("rooms/nonmatchings/dryfield_night_garage/dryfield_night_garage", D_dryfield_night_garage_8017D6D8);

INCLUDE_ASM("rooms/nonmatchings/dryfield_night_garage/dryfield_night_garage", func_dryfield_night_garage_8017EBD8);

INCLUDE_ASM("rooms/nonmatchings/dryfield_night_garage/dryfield_night_garage", func_dryfield_night_garage_8017ED80);

INCLUDE_ASM("rooms/nonmatchings/dryfield_night_garage/dryfield_night_garage", func_dryfield_night_garage_8017EF64);

INCLUDE_ASM("rooms/nonmatchings/dryfield_night_garage/dryfield_night_garage", func_dryfield_night_garage_8017F178);

INCLUDE_ASM("rooms/nonmatchings/dryfield_night_garage/dryfield_night_garage", func_dryfield_night_garage_8017F2F8);

INCLUDE_ASM("rooms/nonmatchings/dryfield_night_garage/dryfield_night_garage", func_dryfield_night_garage_8017F5C0);

INCLUDE_ASM("rooms/nonmatchings/dryfield_night_garage/dryfield_night_garage", func_dryfield_night_garage_8017F794);

INCLUDE_ASM("rooms/nonmatchings/dryfield_night_garage/dryfield_night_garage", func_dryfield_night_garage_8017FC14);

INCLUDE_ASM("rooms/nonmatchings/dryfield_night_garage/dryfield_night_garage", func_dryfield_night_garage_8017FCD0);

INCLUDE_ASM("rooms/nonmatchings/dryfield_night_garage/dryfield_night_garage", func_dryfield_night_garage_8017FDF8);

INCLUDE_ASM("rooms/nonmatchings/dryfield_night_garage/dryfield_night_garage", func_dryfield_night_garage_8017FF2C);

INCLUDE_ASM("rooms/nonmatchings/dryfield_night_garage/dryfield_night_garage", func_dryfield_night_garage_801800C8);

INCLUDE_ASM("rooms/nonmatchings/dryfield_night_garage/dryfield_night_garage", func_dryfield_night_garage_80180300);

INCLUDE_ASM("rooms/nonmatchings/dryfield_night_garage/dryfield_night_garage", func_dryfield_night_garage_80180358);

INCLUDE_ASM("rooms/nonmatchings/dryfield_night_garage/dryfield_night_garage", func_dryfield_night_garage_80180360);

INCLUDE_ASM("rooms/nonmatchings/dryfield_night_garage/dryfield_night_garage", func_dryfield_night_garage_801803A4);

INCLUDE_ASM("rooms/nonmatchings/dryfield_night_garage/dryfield_night_garage", func_dryfield_night_garage_801803AC);

INCLUDE_ASM("rooms/nonmatchings/dryfield_night_garage/dryfield_night_garage", func_dryfield_night_garage_801803BC);

INCLUDE_ASM("rooms/nonmatchings/dryfield_night_garage/dryfield_night_garage", func_dryfield_night_garage_80180414);

INCLUDE_ASM("rooms/nonmatchings/dryfield_night_garage/dryfield_night_garage", func_dryfield_night_garage_80180604);

void func_dryfield_night_garage_801807E4(Task* arg0)
{
    s32 temp_v1;

    temp_v1 = arg0->state;
    switch (temp_v1) {
        case 0:
            Gp_StartCapSlot((s16)arg0->spawnArg1, 0, 0);
            Gp_DispatchMsg(
                (Task*)func_dryfield_night_garage_80180A64(0), 0x7DB, (s32)&D_dryfield_night_garage_80182DE0, 0);
            goto block_12;
        case 1:
            if (Gp_CapBusy() == 0) {
                func_800D4D2C(0x20);
                goto block_12;
            }
            return;
        case 2:
            Gp_StartCapSlot((s16)arg0->spawnArg1, 0, (s16)(GameFlag_GetNibble(0x107) + 1));
            if (GameFlag_GetNibble(0x107) == 0) {
                GameFlag_SetNibble(0x107, 1);
            }
        block_12:
            arg0->state = arg0->state + 1;
            return;
        case 3:
            if (Gp_CapBusy() != 0) {
                break;
            }
            Gp_MsgPlayerWeapon(1);
            Gp_DispatchMsg(
                (Task*)func_dryfield_night_garage_80180A64(0), 0x7DB, (s32)&D_dryfield_night_garage_80182DE4, 0);
        default:
            Task_Kill(arg0);
            break;
    }
}

INCLUDE_ASM("rooms/nonmatchings/dryfield_night_garage/dryfield_night_garage", func_dryfield_night_garage_80180924);

INCLUDE_ASM("rooms/nonmatchings/dryfield_night_garage/dryfield_night_garage", func_dryfield_night_garage_80180944);

INCLUDE_ASM("rooms/nonmatchings/dryfield_night_garage/dryfield_night_garage", func_dryfield_night_garage_80180964);

INCLUDE_ASM("rooms/nonmatchings/dryfield_night_garage/dryfield_night_garage", func_dryfield_night_garage_80180984);

INCLUDE_ASM("rooms/nonmatchings/dryfield_night_garage/dryfield_night_garage", func_dryfield_night_garage_801809A4);

INCLUDE_ASM("rooms/nonmatchings/dryfield_night_garage/dryfield_night_garage", func_dryfield_night_garage_80180A64);

INCLUDE_ASM("rooms/nonmatchings/dryfield_night_garage/dryfield_night_garage", func_dryfield_night_garage_80180AB0);

INCLUDE_ASM("rooms/nonmatchings/dryfield_night_garage/dryfield_night_garage", func_dryfield_night_garage_80180B20);

INCLUDE_ASM("rooms/nonmatchings/dryfield_night_garage/dryfield_night_garage", func_dryfield_night_garage_80180CEC);

INCLUDE_ASM("rooms/nonmatchings/dryfield_night_garage/dryfield_night_garage", func_dryfield_night_garage_80180D4C);

INCLUDE_ASM("rooms/nonmatchings/dryfield_night_garage/dryfield_night_garage", func_dryfield_night_garage_80180D9C);

INCLUDE_ASM("rooms/nonmatchings/dryfield_night_garage/dryfield_night_garage", func_dryfield_night_garage_80181518);

INCLUDE_RODATA("rooms/nonmatchings/dryfield_night_garage/dryfield_night_garage", func_dryfield_night_garage_8017D754);
