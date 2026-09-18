#include "common.h"

#include <psyq/libetc.h>

#include "main/unknown_syms.h"
#include "main/boot.h"
#include "main/gamemain.h"
#include "main/stage.h"

s32 Display_FrameFlipDraw(s32 arg0, s32 arg1, s32 arg2)
{
    DisplayState* temp;
    GsOT*         ot;
    u_long*       saved;
    u_long*       org;
    s32           size;
    s32           neg1;

    temp = &gDisplayState;
    if (temp->mdecActive == 0) {
        temp->frameBuffer ^= 1;
    }
    if ((s8)temp->at100.flags.flipMode != 2) {
        temp->drawBuffer = (u8)temp->frameBuffer;
    }
    ot = Gpu_OrderingTables;
    GsClearOt(0, 0, &ot[temp->frameBuffer]);
    org            = ot[temp->frameBuffer].org;
    size           = D_8007A0E4;
    *org           = GPU_OT_END_PRIM;
    size          /= 2;
    saved          = Gpu_CurrentOt;
    Gpu_CurrentOt  = ot[temp->frameBuffer].org;
    Gpu_PrimCursor = (DR_TPAGE*)((s32)Gpu_PrimBufBase + temp->frameBuffer * size);
    Task_ExecList(&D_8007A110);
    Boot_DispatchCdCmd();
    if (temp->mdecActive == 0) {
        DrawSync(0);
    }
    if (((VSync(1) - arg1) & 0x7FFF) < D_8005EC6C) {
        EnterCriticalSection();
        temp->vsyncFlag     = 1;
        Display_PendingFlip = temp->frameBuffer;
        D_80070E38          = temp->at100.flags.flipMode;
        *(u8*)&D_8006EC30   = temp->at100.flags.imageSource;
        ExitCriticalSection();
        VSync(D_8005EC68);
        neg1 = -1;
        if (Display_PendingFlip != neg1) {
            D_8005EC78 = 0;
            arg1       = VSync(1) & 0x7FFF;
            Display_FlipDraw(temp->frameBuffer);
            Display_PendingFlip = neg1;
        } else {
            D_8005EC78 = D_8005EC74;
            arg1       = -D_8005EC74;
        }
    } else {
        D_8005EC78          = 0;
        arg1                = VSync(1) & 0x7FFF;
        temp->vsyncFlag     = 1;
        Display_PendingFlip = -2;
        D_80070E38          = temp->at100.flags.flipMode;
        *(u8*)&D_8006EC30   = temp->at100.flags.imageSource;
        Display_FlipDraw(temp->frameBuffer);
        Display_PendingFlip = -1;
    }
    Gpu_CurrentOt = saved;
    return arg1;
}

Task* Display_SpawnWithOtSmall(s32 arg0, s32 arg1, s32 arg2, s32 arg3)
{
    DisplayState* temp;
    GsOT*         ot;
    TaskNode*     saved;
    Task*         ret;

    temp = &gDisplayState;
    ret  = NULL;
    if (temp->displayOwner == 0) {
        ot                = Gpu_OrderingTables;
        ot->length        = 6;
        ot->org           = D_8007A120;
        ot[1].length      = 6;
        ot[1].org         = D_8007A120 + 0x40;
        Gpu_PrimBufBase   = Gpu_PrimBufStatic;
        D_8007A0E4        = 0x6000;
        temp->frameBuffer = temp->drawBuffer ^ 1;
        saved             = Task_GetActiveList();
        Task_InitList(&D_8007A110);
        ret = Task_Spawn(arg0, arg1, arg2, arg3);
        if (ret != NULL) {
            temp->pendingMode          = 0xFF;
            temp->displayOwner         = 2;
            temp->at100.flags.flipMode = 0;
        }
        Task_SetActiveList(saved);
    }
    return ret;
}

Task* Display_SpawnWithOt(TaskDesc* arg0, s32 arg1, s32 arg2, s32 arg3)
{
    DisplayState* temp;
    GsOT*         ot;
    TaskNode*     saved;
    Task*         ret;

    temp = &gDisplayState;
    ret  = NULL;
    if (temp->displayOwner == 0) {
        ot                = Gpu_OrderingTables;
        ot->length        = 6;
        ot->org           = D_8007A120;
        ot[1].length      = 6;
        ot[1].org         = D_8007A120 + 0x40;
        Gpu_PrimBufBase   = Gpu_PrimBufStatic;
        D_8007A0E4        = 0x6000;
        temp->frameBuffer = temp->drawBuffer ^ 1;
        saved             = Task_GetActiveList();
        Task_InitList(&D_8007A110);
        ret = Task_SpawnFromTable(arg0, arg1, arg2, arg3);
        if (ret != NULL) {
            temp->pendingMode          = 0xFF;
            temp->displayOwner         = 2;
            temp->at100.flags.flipMode = 0;
        }
        Task_SetActiveList(saved);
    }
    return ret;
}

Task* Task_SpawnOnDefaultListA(s32 arg0, s32 arg1, s32 arg2, s32 arg3)
{
    TaskNode* saved;
    Task*     ret;

    saved = Task_GetActiveList();
    Task_SetActiveList(&gTaskDefaultList);
    ret = Task_Spawn(arg0, arg1, arg2, arg3);
    Task_SetActiveList(saved);
    return ret;
}

Task* Task_SpawnOnDefaultList(TaskDesc* arg0, s32 arg1, s32 arg2, s32 arg3)
{
    TaskNode* saved;
    Task*     ret;

    saved = Task_GetActiveList();
    Task_SetActiveList(&gTaskDefaultList);
    ret = Task_SpawnFromTable(arg0, arg1, arg2, arg3);
    Task_SetActiveList(saved);
    return ret;
}

void Display_ResetHeapWrapper(void)
{
    Display_ResetHeapFromSession();
}

void Display_FlipOt(void)
{
    DisplayState* temp;
    u_long*       saved;
    s32           buf;
    u_long*       ot;

    temp           = &gDisplayState;
    saved          = Gpu_CurrentOt;
    buf            = temp->otBuffer ^ 1;
    temp->otBuffer = buf;
    Gpu_CurrentOt  = Gpu_OtTags + buf * GPU_OT_ENTRIES;
    ClearOTagR(Gpu_CurrentOt, GPU_OT_ENTRIES);
    ot            = Gpu_CurrentOt;
    *ot           = GPU_OT_END_PRIM;
    Gpu_CurrentOt = ot + 0x20;
    Gp_LinkViewSprts();
    Gp_DrawActorTmdActive(&Gpu_OtBuffers[temp->otBuffer]);
    Gpu_CurrentOt              = saved;
    temp->at100.flags.flipMode = 0;
}

void Display_AcquireRef(void)
{
    DisplayState* temp;

    temp = &gDisplayState;
    if (temp->holdState >= 0) {
        temp->holdState |= 0x80;
        temp->holdCount  = 1;
    } else {
        temp->holdCount++;
    }
}

void Display_ReleaseRef(void)
{
    DisplayState* temp;
    u8            val;

    temp = &gDisplayState;
    if (temp->holdState >= 0) {
        temp->holdCount = 0;
    } else {
        val             = temp->holdCount - 1;
        temp->holdCount = val;
        if (val == 0) {
            temp->holdState &= 0x7F;
            temp->holdCount  = 0;
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
    return gDisplayState.holdState;
}

void Gpu_InitOtSmall(void)
{
    GsOT* ot;

    ot              = Gpu_OrderingTables;
    ot->length      = 6;
    ot->org         = D_8007A120;
    ot[1].length    = 6;
    ot[1].org       = D_8007A120 + 0x40;
    Gpu_PrimBufBase = Gpu_PrimBufStatic;
    D_8007A0E4      = 0x6000;
}

s32 Display_DispatchModeId(s32 arg0)
{
    if (arg0 >= 0x20) {
        if (arg0 < 0x80) {
            gDisplayState.pendingMode = 0;
            if (arg0 != 0x43) {
                Display_InitModeObj(&D_8006268C, arg0, 0, 0);
            } else {
                Display_InitModeObj(&D_8006268C, 0x43, 0, 0);
            }
            gDisplayState.pendingMode = arg0;
            if (gDisplayState.demoScene != 0) {
                Stage_SetFadeMax(0xFF);
                Stage_SetFadeRate(0, 0, 0x10, 1);
            } else if (arg0 != 0x42) {
                if (arg0 == 0x43) {
                    Stage_SetFadeMax(0xFF);
                    Stage_SetFadeRate(0, 0, 0x20, 1);
                } else {
                    Stage_SetFadeMax(0x20);
                    Stage_SetFadeRate(0, 0, 8, 1);
                }
            }
        }
        Stage_InitOtAndSpawn();
    }
    return 0;
}

void Display_ResetHeapFromSession(void)
{
    GameSession* temp;

    temp = gGameSession;
    Mem_ConfigureAuxHeap(temp->at4.loc.stage, temp->at4.loc.area);
    gDisplayState.displayOwner = 0;
    gDisplayState.pendingMode  = 0;
}

void Display_FlipOtAlt(void)
{
    DisplayState* temp;
    u_long*       saved;
    s32           buf;

    temp           = &gDisplayState;
    saved          = Gpu_CurrentOt;
    buf            = temp->otBuffer ^ 1;
    temp->otBuffer = buf;
    Gpu_CurrentOt  = Gpu_OtTags + buf * GPU_OT_ENTRIES;
    Gpu_ClearOTag(temp->otBuffer);
    Gpu_CurrentOt = Gpu_CurrentOt + 0x20;
    Task_ExecListFiltered(&gTaskDefaultList, 0x62);
    Gp_DrawActorTmdFlagged(&Gpu_OtBuffers[temp->otBuffer]);
    Gpu_CurrentOt              = saved;
    temp->at100.flags.flipMode = 0;
}

void Gpu_InitOt(void)
{
    GsOT*         ot;
    DisplayState* temp;
    u_long*       org;

    ot           = Gpu_OrderingTables;
    ot->length   = 0xA;
    ot->org      = Gpu_OtTags;
    ot[1].length = 0xA;
    ot[1].org    = Gpu_OtTags + GPU_OT_ENTRIES;
    temp         = &gDisplayState;
    GsClearOt(0, 0, &ot[temp->frameBuffer]);
    org           = ot[temp->frameBuffer].org;
    *org          = GPU_OT_END_PRIM;
    Gpu_CurrentOt = org;
}

void Display_SetPrimBufLarge(void)
{
    D_8007A0E4      = 0x10000;
    Gpu_PrimBufBase = (void*)Gpu_PrimHeapBase;
}

void Display_SetPrimBufSmall(void)
{
    Gpu_PrimBufBase = Gpu_PrimBufStatic;
    D_8007A0E4      = 0x6000;
}
