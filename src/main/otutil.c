#include "common.h"

#include "main/game.h"
#include "main/gamemain.h"
#include "main/unknown_syms.h"

#include <psyq/libetc.h>

s32 func_8003DFB0(s32 arg0, s32 arg1)
{
    DisplayState* temp;
    GameOt*       ot;
    u_long*       saved;
    u_long*       org;
    s32           size;
    s32           neg1;

    temp = &Display_State;
    if (temp->field_106 == 0) {
        temp->field_118 ^= 1;
    }
    if ((s8)temp->field_103 != 2) {
        temp->field_1f = (u8)temp->field_118;
    }
    ot = Gpu_OrderingTables;
    GsClearOt(0, 0, &ot[temp->field_118]);
    org        = ot[temp->field_118].org;
    size       = D_8007A0E4;
    *org       = C5F414_OTAG_END_PRIM;
    size      /= 2;
    saved      = D_800710A0;
    D_800710A0 = ot[temp->field_118].org;
    D_80071190 = (DR_TPAGE*)((s32)D_8007A0E0 + temp->field_118 * size);
    Task_ExecList(&D_8007A110);
    func_80014C2C();
    if (temp->field_106 == 0) {
        DrawSync(0);
    }
    if (((VSync(1) - arg1) & 0x7FFF) < D_8005EC6C) {
        EnterCriticalSection();
        temp->field_108   = 1;
        D_8005EC70        = temp->field_118;
        D_80070E38        = temp->field_103;
        *(u8*)&D_8006EC30 = temp->field_100;
        ExitCriticalSection();
        VSync(D_8005EC68);
        neg1 = -1;
        if (D_8005EC70 != neg1) {
            D_8005EC78 = 0;
            arg1       = VSync(1) & 0x7FFF;
            Display_FlipDraw(temp->field_118);
            D_8005EC70 = neg1;
        } else {
            D_8005EC78 = D_8005EC74;
            arg1       = -D_8005EC74;
        }
    } else {
        D_8005EC78        = 0;
        arg1              = VSync(1) & 0x7FFF;
        temp->field_108   = 1;
        D_8005EC70        = -2;
        D_80070E38        = temp->field_103;
        *(u8*)&D_8006EC30 = temp->field_100;
        Display_FlipDraw(temp->field_118);
        D_8005EC70 = -1;
    }
    D_800710A0 = saved;
    return arg1;
}

Task* func_8003E210(s32 arg0, s32 arg1, s32 arg2, s32 arg3)
{
    DisplayState* temp;
    GameOt*       ot;
    TaskNode*     saved;
    Task*         ret;

    temp = &Display_State;
    ret  = NULL;
    if (temp->field_1e == 0) {
        ot              = Gpu_OrderingTables;
        ot->length      = 6;
        ot->org         = D_8007A120;
        ot[1].length    = 6;
        ot[1].org       = D_8007A120 + 0x40;
        D_8007A0E0      = D_800740E0;
        D_8007A0E4      = 0x6000;
        temp->field_118 = temp->field_1f ^ 1;
        saved           = Task_GetActiveList();
        Task_InitList(&D_8007A110);
        ret = Task_Spawn(arg0, arg1, arg2, arg3);
        if (ret != NULL) {
            temp->field_10d = 0xFF;
            temp->field_1e  = 2;
            temp->field_103 = 0;
        }
        Task_SetActiveList(saved);
    }
    return ret;
}

Task* func_8003E324(TaskDesc* arg0, s32 arg1, s32 arg2, s32 arg3)
{
    DisplayState* temp;
    GameOt*       ot;
    TaskNode*     saved;
    Task*         ret;

    temp = &Display_State;
    ret  = NULL;
    if (temp->field_1e == 0) {
        ot              = Gpu_OrderingTables;
        ot->length      = 6;
        ot->org         = D_8007A120;
        ot[1].length    = 6;
        ot[1].org       = D_8007A120 + 0x40;
        D_8007A0E0      = D_800740E0;
        D_8007A0E4      = 0x6000;
        temp->field_118 = temp->field_1f ^ 1;
        saved           = Task_GetActiveList();
        Task_InitList(&D_8007A110);
        ret = Task_SpawnFromTable(arg0, arg1, arg2, arg3);
        if (ret != NULL) {
            temp->field_10d = 0xFF;
            temp->field_1e  = 2;
            temp->field_103 = 0;
        }
        Task_SetActiveList(saved);
    }
    return ret;
}

Task* func_8003E438(s32 arg0, s32 arg1, s32 arg2, s32 arg3)
{
    TaskNode* saved;
    Task*     ret;

    saved = Task_GetActiveList();
    Task_SetActiveList(&Task_DefaultList);
    ret = Task_Spawn(arg0, arg1, arg2, arg3);
    Task_SetActiveList(saved);
    return ret;
}

Task* func_8003E4BC(TaskDesc* arg0, s32 arg1, s32 arg2, s32 arg3)
{
    TaskNode* saved;
    Task*     ret;

    saved = Task_GetActiveList();
    Task_SetActiveList(&Task_DefaultList);
    ret = Task_SpawnFromTable(arg0, arg1, arg2, arg3);
    Task_SetActiveList(saved);
    return ret;
}

void func_8003E540(void)
{
    func_8003E814();
}

void func_8003E560(void)
{
    DisplayState* temp;
    u_long*       saved;
    s32           buf;
    u_long*       ot;

    temp            = &Display_State;
    saved           = D_800710A0;
    buf             = temp->field_114 ^ 1;
    temp->field_114 = buf;
    D_800710A0      = D5F414_OrderingTables + buf * C5F414_OTAG_ENTRIES;
    ClearOTagR(D_800710A0, C5F414_OTAG_ENTRIES);
    ot         = D_800710A0;
    *ot        = C5F414_OTAG_END_PRIM;
    D_800710A0 = ot + 0x20;
    func_800AC688();
    func_8009850C(&Gpu_OtBuffers[temp->field_114]);
    D_800710A0      = saved;
    temp->field_103 = 0;
}

void func_8003E610(void)
{
    DisplayState* temp;

    temp = &Display_State;
    if (temp->field_1d >= 0) {
        temp->field_1d |= 0x80;
        temp->field_11d = 1;
    } else {
        temp->field_11d++;
    }
}

void func_8003E64C(void)
{
    DisplayState* temp;
    u8            val;

    temp = &Display_State;
    if (temp->field_1d >= 0) {
        temp->field_11d = 0;
    } else {
        val             = temp->field_11d - 1;
        temp->field_11d = val;
        if (val == 0) {
            temp->field_1d &= 0x7F;
            temp->field_11d = 0;
        }
    }
}

s32 func_8003E698(void)
{
    s32 temp;

    temp = D_8007A118;
    if (temp == 2) {
        goto case2;
    }
    if (temp < 3) {
        goto default_case;
    }
    if (temp == 3) {
        goto case3;
    }
    goto default_case;
case2:
    return 4;
case3:
    return 6;
default_case:
    return Display_State.field_1d;
}

void Gpu_InitOtSmall(void)
{
    GameOt* ot;

    ot           = Gpu_OrderingTables;
    ot->length   = 6;
    ot->org      = D_8007A120;
    ot[1].length = 6;
    ot[1].org    = D_8007A120 + 0x40;
    D_8007A0E0   = D_800740E0;
    D_8007A0E4   = 0x6000;
}

s32 func_8003E72C(s32 arg0)
{
    if (arg0 >= 0x20) {
        if (arg0 < 0x80) {
            Display_State.field_10d = 0;
            if (arg0 != 0x43) {
                Display_InitModeObj(&D_8006268C, arg0, 0, 0);
            } else {
                Display_InitModeObj(&D_8006268C, 0x43, 0, 0);
            }
            Display_State.field_10d = arg0;
            if (Display_State.field_12c != 0) {
                Display_SetFadeMax(0xFF);
                Display_SetFadeRate(0, 0, 0x10, 1);
            } else if (arg0 != 0x42) {
                if (arg0 == 0x43) {
                    Display_SetFadeMax(0xFF);
                    Display_SetFadeRate(0, 0, 0x20, 1);
                } else {
                    Display_SetFadeMax(0x20);
                    Display_SetFadeRate(0, 0, 8, 1);
                }
            }
        }
        func_8003F690();
    }
    return 0;
}

void func_8003E814(void)
{
    GameSession* temp;

    temp = Game_Session;
    Mem_ConfigureAuxHeap(temp->field_7, temp->field_6);
    Display_State.field_1e  = 0;
    Display_State.field_10d = 0;
}

void func_8003E854(void)
{
    DisplayState* temp;
    u_long*       saved;
    s32           buf;

    temp            = &Display_State;
    saved           = D_800710A0;
    buf             = temp->field_114 ^ 1;
    temp->field_114 = buf;
    D_800710A0      = D5F414_OrderingTables + buf * C5F414_OTAG_ENTRIES;
    F179D4_ClearOTag(temp->field_114);
    D_800710A0 = D_800710A0 + 0x20;
    Task_ExecListFiltered(&Task_DefaultList, 0x62);
    func_80097AC0(&Gpu_OtBuffers[temp->field_114]);
    D_800710A0      = saved;
    temp->field_103 = 0;
}

void Gpu_InitOt(void)
{
    GameOt*       ot;
    DisplayState* temp;
    u_long*       org;

    ot           = Gpu_OrderingTables;
    ot->length   = 0xA;
    ot->org      = D5F414_OrderingTables;
    ot[1].length = 0xA;
    ot[1].org    = D5F414_OrderingTables + C5F414_OTAG_ENTRIES;
    temp         = &Display_State;
    GsClearOt(0, 0, &ot[temp->field_118]);
    org        = ot[temp->field_118].org;
    *org       = C5F414_OTAG_END_PRIM;
    D_800710A0 = org;
}

void Display_SetPrimBufLarge(void)
{
    D_8007A0E4 = 0x10000;
    D_8007A0E0 = (void*)D_80068F88;
}

void Display_SetPrimBufSmall(void)
{
    D_8007A0E0 = D_800740E0;
    D_8007A0E4 = 0x6000;
}
