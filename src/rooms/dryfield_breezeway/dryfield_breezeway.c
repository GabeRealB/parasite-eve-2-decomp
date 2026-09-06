#include "common.h"

#include "gameplay/3CD8.h"
#include "main/gameflag.h"
#include "main/task.h"
#include "gameplay/268.h"
#include "gameplay/3A34.h"

extern TaskDesc D_dryfield_breezeway_80181E10[];

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

void func_dryfield_breezeway_8017DCE4(Task* task)
{
    switch (task->state) {
        case 0:
            Gp_RunCapCmd(task->spawnArg1, 0);
            task->state++;
            break;
        case 1:
            if (Gp_CapBusy() == 0) {
                if (GameFlag_GetNibble(0x56) != 4) {
                    if (Gp_GetCapEventKey() == 0xB) {
                        GameFlag_SetNibble(0x56, 2);
                    }
                    if (GameFlag_GetNibble(0x56) == 5) {
                        GameFlag_SetNibble(0x56, 6);
                    }
                }
                Gp_MsgPlayerWeapon(1);
                Task_Kill(task);
            }
            break;
    }
}

INCLUDE_ASM("rooms/nonmatchings/dryfield_breezeway/dryfield_breezeway", func_dryfield_breezeway_8017DDB0);

void func_dryfield_breezeway_8017DE60(void)
{
}

INCLUDE_RODATA("rooms/nonmatchings/dryfield_breezeway/dryfield_breezeway", RoomsShared8017d878Table);

INCLUDE_RODATA("rooms/nonmatchings/dryfield_breezeway/dryfield_breezeway", RoomsShared8017fc38Table);
