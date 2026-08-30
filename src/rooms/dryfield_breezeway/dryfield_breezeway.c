#include "common.h"

#include "gameplay/268.h"
#include "gameplay/3A34.h"
#include "gameplay/3CD8.h"
#include "main/gameflag.h"
#include "main/task.h"

extern TaskDesc D_dryfield_breezeway_80181E10[];

INCLUDE_ASM("rooms/nonmatchings/dryfield_breezeway/dryfield_breezeway", func_dryfield_breezeway_8017D638);

INCLUDE_RODATA("rooms/nonmatchings/dryfield_breezeway/dryfield_breezeway", D_dryfield_breezeway_8017D5C0);

INCLUDE_ASM("rooms/nonmatchings/dryfield_breezeway/dryfield_breezeway", func_dryfield_breezeway_8017D79C);

INCLUDE_ASM("rooms/nonmatchings/dryfield_breezeway/dryfield_breezeway", func_dryfield_breezeway_8017D90C);

INCLUDE_ASM("rooms/nonmatchings/dryfield_breezeway/dryfield_breezeway", func_dryfield_breezeway_8017D940);

s32 func_dryfield_breezeway_8017DA48(Task* task, s32 msgId, s32 arg2, s32 arg3)
{
    switch (arg2) {
        case 1:
            if (Gp_StateF0.field_0 == 1) {
                Gp_RunCapCmd1(5);
            } else {
                if (GameFlag_GetNibble(0x56) != 4) {
                    if (Gp_HasCollectedBit(0x115) != 0) {
                        GameFlag_SetNibble(0x56, 3);
                    } else if (GameFlag_GetNibble(0xFE) != 0) {
                        if (Gp_HasCollectedBit(0x11B) == 0) {
                            if (GameFlag_GetNibble(0x56) != 6) {
                                GameFlag_SetNibble(0x56, 5);
                            }
                        } else {
                            GameFlag_SetNibble(0x56, 2);
                        }
                    }
                }
                Gp_MsgPlayerWeapon(0);
                Task_SpawnFromTable(D_dryfield_breezeway_80181E10, 1, arg2, 0);
            }
            break;
        case 3:
            if (GameFlag_GetNibble(0x56) >= 2) {
                if (Gp_StateF0.field_0 != 1) {
                    if (Gp_GetCurBit2Flag(6) == 1) {
                        Task_SpawnFromTable(D_dryfield_breezeway_80181E10, 0, 0, 0);
                        GameFlag_SetNibble(0xFE, 1);
                    }
                } else {
                    Gp_RunCapCmd1(5);
                }
            }
            break;
    }
    return 0;
}

INCLUDE_ASM("rooms/nonmatchings/dryfield_breezeway/dryfield_breezeway", func_dryfield_breezeway_8017DBA4);

INCLUDE_ASM("rooms/nonmatchings/dryfield_breezeway/dryfield_breezeway", func_dryfield_breezeway_8017DBD8);

INCLUDE_ASM("rooms/nonmatchings/dryfield_breezeway/dryfield_breezeway", func_dryfield_breezeway_8017DC3C);

INCLUDE_ASM("rooms/nonmatchings/dryfield_breezeway/dryfield_breezeway", func_dryfield_breezeway_8017DCE4);

INCLUDE_ASM("rooms/nonmatchings/dryfield_breezeway/dryfield_breezeway", func_dryfield_breezeway_8017DDB0);

void func_dryfield_breezeway_8017DE60(void)
{
}

INCLUDE_ASM("rooms/nonmatchings/dryfield_breezeway/dryfield_breezeway", func_dryfield_breezeway_8017DE68);

INCLUDE_ASM("rooms/nonmatchings/dryfield_breezeway/dryfield_breezeway", func_dryfield_breezeway_8017DEC0);

INCLUDE_ASM("rooms/nonmatchings/dryfield_breezeway/dryfield_breezeway", func_dryfield_breezeway_8017E010);

INCLUDE_ASM("rooms/nonmatchings/dryfield_breezeway/dryfield_breezeway", func_dryfield_breezeway_8017E114);

INCLUDE_ASM("rooms/nonmatchings/dryfield_breezeway/dryfield_breezeway", func_dryfield_breezeway_8017E2D4);

INCLUDE_ASM("rooms/nonmatchings/dryfield_breezeway/dryfield_breezeway", func_dryfield_breezeway_8017E350);

INCLUDE_ASM("rooms/nonmatchings/dryfield_breezeway/dryfield_breezeway", func_dryfield_breezeway_8017E370);

INCLUDE_ASM("rooms/nonmatchings/dryfield_breezeway/dryfield_breezeway", func_dryfield_breezeway_8017E390);

INCLUDE_ASM("rooms/nonmatchings/dryfield_breezeway/dryfield_breezeway", func_dryfield_breezeway_8017E464);

INCLUDE_ASM("rooms/nonmatchings/dryfield_breezeway/dryfield_breezeway", func_dryfield_breezeway_8017E65C);

INCLUDE_ASM("rooms/nonmatchings/dryfield_breezeway/dryfield_breezeway", func_dryfield_breezeway_8017E81C);

INCLUDE_ASM("rooms/nonmatchings/dryfield_breezeway/dryfield_breezeway", func_dryfield_breezeway_8017E948);

INCLUDE_ASM("rooms/nonmatchings/dryfield_breezeway/dryfield_breezeway", func_dryfield_breezeway_8017EB8C);

INCLUDE_ASM("rooms/nonmatchings/dryfield_breezeway/dryfield_breezeway", func_dryfield_breezeway_8017F1F4);

INCLUDE_RODATA("rooms/nonmatchings/dryfield_breezeway/dryfield_breezeway", D_dryfield_breezeway_8017D5E8);

INCLUDE_ASM("rooms/nonmatchings/dryfield_breezeway/dryfield_breezeway", func_dryfield_breezeway_8017F538);

INCLUDE_ASM("rooms/nonmatchings/dryfield_breezeway/dryfield_breezeway", func_dryfield_breezeway_8017F998);

INCLUDE_ASM("rooms/nonmatchings/dryfield_breezeway/dryfield_breezeway", func_dryfield_breezeway_8017FA80);

INCLUDE_ASM("rooms/nonmatchings/dryfield_breezeway/dryfield_breezeway", func_dryfield_breezeway_8017FAD0);

INCLUDE_ASM("rooms/nonmatchings/dryfield_breezeway/dryfield_breezeway", func_dryfield_breezeway_8017FB30);

INCLUDE_ASM("rooms/nonmatchings/dryfield_breezeway/dryfield_breezeway", func_dryfield_breezeway_8017FBC8);

INCLUDE_ASM("rooms/nonmatchings/dryfield_breezeway/dryfield_breezeway", func_dryfield_breezeway_8017FBEC);

INCLUDE_ASM("rooms/nonmatchings/dryfield_breezeway/dryfield_breezeway", func_dryfield_breezeway_8017FC38);

INCLUDE_ASM("rooms/nonmatchings/dryfield_breezeway/dryfield_breezeway", func_dryfield_breezeway_8017FCB4);

INCLUDE_ASM("rooms/nonmatchings/dryfield_breezeway/dryfield_breezeway", func_dryfield_breezeway_8017FD68);

INCLUDE_ASM("rooms/nonmatchings/dryfield_breezeway/dryfield_breezeway", func_dryfield_breezeway_8017FD9C);

INCLUDE_ASM("rooms/nonmatchings/dryfield_breezeway/dryfield_breezeway", func_dryfield_breezeway_8017FE08);

INCLUDE_ASM("rooms/nonmatchings/dryfield_breezeway/dryfield_breezeway", func_dryfield_breezeway_8017FE90);

INCLUDE_ASM("rooms/nonmatchings/dryfield_breezeway/dryfield_breezeway", func_dryfield_breezeway_8017FF1C);

INCLUDE_ASM("rooms/nonmatchings/dryfield_breezeway/dryfield_breezeway", func_dryfield_breezeway_8017FF7C);

INCLUDE_ASM("rooms/nonmatchings/dryfield_breezeway/dryfield_breezeway", func_dryfield_breezeway_8018034C);

INCLUDE_ASM("rooms/nonmatchings/dryfield_breezeway/dryfield_breezeway", func_dryfield_breezeway_80180858);

INCLUDE_ASM("rooms/nonmatchings/dryfield_breezeway/dryfield_breezeway", func_dryfield_breezeway_80181264);

INCLUDE_ASM("rooms/nonmatchings/dryfield_breezeway/dryfield_breezeway", func_dryfield_breezeway_80181938);
