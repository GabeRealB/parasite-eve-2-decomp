#include "common.h"

#include "gameplay/268.h"
#include "gameplay/3CD8.h"
#include "gameplay/D4.h"
#include "main/gameflag.h"
#include "main/session.h"
#include "main/task.h"

extern s32 D_actor_146300_80137AAC;
extern s32 D_actor_146300_80137B10;
extern s32 D_actor_146300_80137B38;
extern s32 D_actor_146300_80137B60;
extern s32 D_actor_146300_80137C10;
extern s32 D_actor_146300_801386C0;
extern s32 D_actor_146300_80138810;
extern s32 D_actor_146300_801388D0;
extern s32 D_actor_146300_80138A38;
extern s32 D_actor_146300_80138AC8;
extern s32 D_actor_146300_80142824;

void func_actor_146300_80131ECC(Task* task)
{
    switch (task->state) {
        case 0:
            Gp_AgeFlag119();
            switch (GameFlag_GetNibble(0x7B)) {
                case 2:
                    if (Gp_HasCollectedBit(0x119) == 0) {
                        Gp_RunCapCmd1(0x12);
                        task->state++;
                    } else {
                        Gp_ClearCollectedBit(0x119);
                        D_actor_146300_80142824 = 0x13;
                        GameFlag_SetNibble(0x7B, 3);
                        task->state = 0xA;
                    }
                    break;
                case 3:
                    if (Gp_HasCollectedBit(0x119) == 0) {
                        if (Gp_GetCurBit2Flag(0x1F) == 1) {
                            Gp_DispatchMsg(Game_GetPtrSlot(6), 0xFA4, 0, 0);
                            D_actor_146300_80142824 = 0x13;
                            task->state             = 0x14;
                        } else {
                            Gp_RunCapCmd1(0x12);
                            task->state++;
                        }
                    } else {
                        Gp_ClearCollectedBit(0x119);
                        D_actor_146300_80142824 = 0x14;
                        GameFlag_SetNibble(0x7B, 4);
                        task->state = 0xA;
                    }
                    break;
                case 4:
                    if (Gp_HasCollectedBit(0x119) == 0) {
                        if (Gp_GetCurBit2Flag(0x20) == 1) {
                            Gp_DispatchMsg(Game_GetPtrSlot(6), 0xFA4, 0, 0);
                            D_actor_146300_80142824 = 0x14;
                            task->state             = 0x14;
                        } else {
                            Gp_RunCapCmd1(0x12);
                            task->state++;
                        }
                    } else {
                        Gp_ClearCollectedBit(0x119);
                        GameFlag_SetNibble(0x7B, 5);
                        task->state = 0x1E;
                    }
                    break;
                case 5:
                    if (Gp_GetCurBit2Flag(0x21) == 1) {
                        Gp_DispatchMsg(Game_GetPtrSlot(6), 0xFA4, 0, 0);
                        task->state = 0x28;
                    } else {
                        func_800E8614((s32)&D_actor_146300_80138AC8, 0);
                        Task_Kill(task);
                    }
                    break;
                default:
                    task->state++;
                    break;
            }
            break;
        case 1:
            Gp_MsgPlayerWeapon(1);
            Task_Kill(task);
            break;
        case 10:
            Gp_StartCapSlot((s16)D_actor_146300_80142824, 0, 0);
            func_800E8614((s32)&D_actor_146300_801386C0, 1);
            task->state++;
            break;
        case 11:
            if (Game_Session->field_1 == 0) {
                task->state = 0x14;
            }
            break;
        case 20:
            Gp_StartCapSlot((s16)D_actor_146300_80142824, 0, 1);
            func_800E8614((s32)&D_actor_146300_80138810, 1);
            task->state++;
            break;
        case 30:
            func_800E8614((s32)&D_actor_146300_801388D0, 1);
            task->state++;
            break;
        case 31:
            if (Game_Session->field_1 == 0) {
                task->state = 0x28;
            }
            break;
        case 40:
            Gp_StartCapSlot(0x15, 0, 1);
            func_800E8614((s32)&D_actor_146300_80138A38, 1);
            task->state++;
            break;
        case 21:
        case 41:
            if (Game_Session->field_1 == 0) {
                task->state = 1;
            }
            break;
    }
}

void func_actor_146300_8013224C(void)
{
    switch (GameFlag_GetNibble(0x7B)) {
        case 2:
            Gp_DispatchMsg((Task*)Gp_LookupSlot4(0), 0x7D3, (s32)&D_actor_146300_80137B38, 0);
            break;
        case 3:
            if (Gp_HasCollectedBit(0x119) == 0) {
                if (Gp_GetCurBit2Flag(0x1F) == 1) {
                    Gp_DispatchMsg((Task*)Gp_LookupSlot4(0), 0x7D3, (s32)&D_actor_146300_80137AAC, 0);
                } else {
                    Gp_DispatchMsg((Task*)Gp_LookupSlot4(0), 0x7D3, (s32)&D_actor_146300_80137B38, 0);
                }
            } else {
                Gp_DispatchMsg((Task*)Gp_LookupSlot4(0), 0x7D3, (s32)&D_actor_146300_80137B38, 0);
            }
            break;
        case 4:
            if (Gp_HasCollectedBit(0x119) == 0) {
                if (Gp_GetCurBit2Flag(0x20) == 1) {
                    Gp_DispatchMsg((Task*)Gp_LookupSlot4(0), 0x7D3, (s32)&D_actor_146300_80137AAC, 0);
                } else {
                    Gp_DispatchMsg((Task*)Gp_LookupSlot4(0), 0x7D3, (s32)&D_actor_146300_80137B38, 0);
                }
                break;
            }
            /* fallthrough */
        case 5:
            Gp_DispatchMsg((Task*)Gp_LookupSlot4(0), 0x7D4, (s32)&D_actor_146300_80137C10, 0);
            Gp_DispatchMsg((Task*)Gp_LookupSlot4(0), 0x7D3, (s32)&D_actor_146300_80137B60, 0);
            break;
    }
}

INCLUDE_ASM("actors/nonmatchings/actor_146300/actor_146300", func_actor_146300_801323E0);

void func_actor_146300_80132418(s32 arg0)
{
    switch (arg0) {
        case 0:
            if (Gp_GetCapEventKey() == 1) {
                Gp_DispatchMsg((Task*)Gp_LookupSlot4(0), 0x7D3, (s32)&D_actor_146300_80137B10, 0);
            }
            break;
        case 1:
            if (Gp_GetCapEventKey() == 2) {
                Gp_DispatchMsg((Task*)Gp_LookupSlot4(0), 0x7D3, (s32)&D_actor_146300_80137AAC, 0);
            }
            break;
    }
}

INCLUDE_ASM("actors/nonmatchings/actor_146300/actor_146300", func_actor_146300_801324AC);

INCLUDE_ASM("actors/nonmatchings/actor_146300/actor_146300", func_actor_146300_801326CC);

INCLUDE_ASM("actors/nonmatchings/actor_146300/actor_146300", func_actor_146300_80132728);
