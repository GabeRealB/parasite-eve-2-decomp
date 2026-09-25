#include "common.h"
#include "main/mem.h"

#include "main/unknown_syms.h"
#include "main/fs.h"
#include "main/gfx.h"
#include "main/pad.h"
#include "main/stage.h"
#include "main/stream.h"
#include "main/tmd.h"

#include "psyq/libpress.h"

void Display_StepFadeOverlay(void)
{
    StageCtx* p;
    s32       temp;
    s32       product;
    s32       otIdx;
    s32       yoff;
    u8        max;
    u8        val;
    TILE*     tile;
    DR_TPAGE* dr;
    u_long*   ot;

    p = Stage_Ctx;
    if ((s8)p->field_19 & 0x80) {
        p->field_19 = p->field_19 & 0x7F;
        return;
    }

    if (gDisplayState.at100.flags.flipMode != 2) {
        temp = (s8)p->field_18;
        if (temp != 0) {
            product = temp * gDisplayState.frameTicks;
            temp    = p->field_17;
            temp    = temp + product;
            if (temp <= 0) {
                p->field_17         = 0;
                Stage_Ctx->field_18 = 0;
            } else {
                max = p->field_1a;
                if (temp >= (s32)max) {
                    p->field_17         = max;
                    Stage_Ctx->field_18 = 0;
                } else {
                    p->field_17 = (u8)temp;
                }
            }
        }
    }

    if (Stage_Ctx->field_17 != 0) {
        otIdx = 0;
        if (Stage_Ctx->field_19 & 2) {
            otIdx = 0x3FF;
            if (Stage_Ctx->field_13 == 0) {
                otIdx = 0x3F;
            }
        }

        tile           = (TILE*)gGpuPrimCursor;
        gGpuPrimCursor = tile + 1;
        yoff           = gDisplayState.vramYOffset;
        setlen(tile, 3);
        setcode(tile, 0x62);
        tile->x0 = -0xA0;
        tile->y0 = -0x78 - yoff;
        tile->w  = 0x140;
        tile->h  = 0xF0;
        val      = Stage_Ctx->field_17;
        tile->b0 = val;
        tile->g0 = val;
        tile->r0 = val;

        dr             = (DR_TPAGE*)gGpuPrimCursor;
        gGpuPrimCursor = dr + 1;
        if (!(Stage_Ctx->field_19 & 1)) {
            setlen(dr, 1);
            dr->code[0] = 0xE1000240;
        } else {
            setlen(dr, 1);
            dr->code[0] = 0xE1000220;
        }

        ot = (u_long*)((otIdx << 2) + (s32)gGpuCurrentOt);
        addPrim(ot, tile);
        addPrim(ot, dr);
    }
}

s32 Display_TransitionLoad(Task* arg0)
{
    RECT rect;
    s32  temp_v1;

    temp_v1 = Stage_Ctx->field_28;
    if (temp_v1 == 1) {
        goto case1;
    }
    if (temp_v1 == 0) {
        goto case0;
    }
    if (temp_v1 == 2) {
        goto case2;
    }
    if (temp_v1 == 3) {
        goto case3;
    }
    goto default_case;

case0:
    SetDispMask(0);
    Stage_Ctx->field_24        = gDisplayState.frameBuffer;
    gDisplayState.keepGraphics = 1;
    Gfx_LoadImageSlot(gGameSession->at4.loc.stage, gGameSession->at4.loc.area, gDisplayState.frameBuffer);
    gDisplayState.at100.flags.flipMode = 2;
    Stage_Ctx->field_28                = Stage_Ctx->field_28 + 1;
    goto end;
case1:
    if (CdCmd_IsIdle() & 0xFFFF) {
        CdCmd_Enqueue(0x21, Stage_Ctx->field_2C, Stage_Ctx->field_34);
        Stage_Ctx->field_28 = Stage_Ctx->field_28 + 1;
    }
    goto end;
case2:
    if ((CdCmd_IsIdle() & 0xFFFF) && (gDisplayState.frameBuffer != Stage_Ctx->field_24)) {
        Gfx_StoreImageSlot(gGameSession->at4.loc.stage, gGameSession->at4.loc.area, gDisplayState.frameBuffer, 0x10000);
        Mem_InitAux();
        rect.x = 0;
        rect.w = 0x140;
        rect.h = 0xF0;
        rect.y = (gDisplayState.frameBuffer ^ 1) * 0x110;
        ClearImage(&rect, 0, 0, 0);
        rect.x = 0;
        rect.w = 0x140;
        rect.h = 0xF0;
        rect.y = gDisplayState.frameBuffer * 0x110;
        ClearImage(&rect, 0, 0, 0);
        DrawSync(0);
        Stage_Ctx->field_12 = 1;
        Stage_Ctx->field_28 = Stage_Ctx->field_28 + 1;
    }
    goto end;
case3:
    gDisplayState.at100.flags.flipMode    = 1;
    gDisplayState.at100.flags.imageSource = 2;
    Stage_Ctx->field_28                   = Stage_Ctx->field_28 + 1;
default_case:
    SetDispMask(1);
    Stage_Ctx->field_1c = Stage_Ctx->field_1c & 0xF7FFFFFF;
end:
    return 1;
}

Task* Display_SpawnFromMode(void)
{
    Task*      ret;
    u32        mode;
    Task*      slot;
    GameActor* obj;
    GpCoord*   ptr;
    GpAreaKey* ed;
    s32        flag;

    ret = Task_SpawnFromTable(Stage_Ctx->field_0, 0, Stage_Ctx->field_4, Stage_Ctx->field_8);
    if (ret != NULL) {
        mode = Stage_Ctx->field_C;
        if (mode == 4) {
            goto block_case4;
        }
        if (mode >= 5U) {
            goto block_default;
        }
        if (mode == 1) {
            goto block_case13;
        }
        if (mode == 3) {
            goto block_case13;
        }
        goto block_default;

    block_case4:
        Stage_Ctx->field_11 = 2;
        slot                = gameGetPtrSlot(3);
        obj                 = (GameActor*)slot->work;
        flag                = obj->field_984 & 1;
        ptr                 = slot->extra.tmd->coords;
        if (flag) {
            func_801011D0(ptr, obj->field_90, 6, &obj->field_930);
        }
        Gp_ClearRec18Occupied(&obj->field_17C);
        ptr->flg = 0;
    block_case13:
        Stage_Ctx->field_15 = 1;
        if (Stage_Ctx->field_C == 3) {
            gDisplayState.at100.flags.flipMode    = 2;
            gDisplayState.at100.flags.imageSource = 0;
        } else {
            gDisplayState.at100.flags.flipMode    = 0;
            gDisplayState.at100.flags.imageSource = 1;
        }
    } else {
        goto block_end;
    }
    goto block_end;

block_default:
    ed = &gGameSession->at4.loc;
    Gpu_ResetGraphAndOt();
    Gfx_StoreImageSlot(ed->stage, ed->area, gDisplayState.drawBuffer, 0x10000);
    if (Stage_Ctx->field_C == 0x100) {
        Display_InvertFramebufferGray();
    }
    Mem_InitAux();
    gDisplayState.at100.flags.flipMode    = 1;
    gDisplayState.at100.flags.imageSource = 3;
    slot                                  = gameGetPtrSlot(3);
    obj                                   = (GameActor*)slot->work;
    flag                                  = obj->field_984 & 1;
    ptr                                   = slot->extra.tmd->coords;
    if (flag) {
        func_801011D0(ptr, obj->field_90, 6, &obj->field_930);
    }
    Gp_ClearRec18Occupied(&obj->field_17C);
    ptr->flg = 0;

block_end:
    return ret;
}

void Display_TaskLoadStep(Task* arg0);

void Display_TransitionTask(Task* arg0)
{
    u32          flags;
    s32          state;
    GameSession* ed;
    StageCtx*    g;
    s32          flag;
    s32          f11;
    s32          disp;

    flags = Stage_Ctx->field_1c;
    if (flags & 0x40000000) {
        Pad_SetCooldown(0);
        Stage_Ctx->field_15 = 0;
        state               = Stage_Ctx->field_28;
        switch (state) {
            case 0:
                Stage_Ctx->field_24                = gDisplayState.frameBuffer;
                gGameSession->at4.loc.view         = Stage_Ctx->field_20;
                Stage_Ctx->field_C                 = 0;
                gDisplayState.at100.flags.flipMode = 2;
                Mem_ConfigureAuxHeap(gGameSession->at4.loc.stage, gGameSession->at4.loc.area);
                if (!(Stage_Ctx->field_1c & 0x10000000)) {
                    (gameGetPtrSlot(1))->spawnArg1 = (u8)gGameSession->at4.loc.view;
                    ResetGraph(1);
                    Gpu_ClearOTag(0);
                    Gpu_ClearOTag(1);
                    Mem_InitAux();
                    Mc_SaveData.at4.loc.view = gGameSession->at4.loc.view;
                    Pad_SetCooldown(0);
                    Gp_SpawnCurView(2);
                    gGameSession->viewReady = 0;
                    Task_Spawn(0, 0x1E, 2, 0);
                } else {
                    Tmd_AllocMissingBuffers();
                    gGameSession->viewReady = 1;
                }
                Stage_Ctx->field_28 = Stage_Ctx->field_28 + 1;
                break;
            case 1:
                ed   = gGameSession;
                flag = ed->viewReady;
                if (flag == 1) {
                    disp = gDisplayState.frameBuffer;
                    g    = Stage_Ctx;
                    if (disp == g->field_24) {
                        f11           = g->field_11;
                        ed->viewReady = 0;
                        if (f11 == 0) {
                            arg0->killCountdown = flag;
                            Stage_Ctx->field_28 = Stage_Ctx->field_28 + 2;
                        } else {
                            Stage_Ctx->field_28 = Stage_Ctx->field_28 + 1;
                        }
                    }
                    CdCmd_ActivatePhase2();
                }
                break;
            case 2:
                gDisplayState.otBuffer = gDisplayState.frameBuffer;
                Display_FlipOtAndDispatch(0);
                Stage_Ctx->field_19                = Stage_Ctx->field_19 | 0x80;
                gDisplayState.at100.flags.flipMode = gDisplayState.at100.flags.flipMode | 0x10;
                arg0->killCountdown                = 3;
                Stage_Ctx->field_28                = Stage_Ctx->field_28 + 1;
                break;
            case 3:
                gDisplayState.at100.flags.flipMode = 2;
                arg0->killCountdown                = arg0->killCountdown - 1;
                if (arg0->killCountdown == 0) {
                    Gpu_ResetGraphAndOt();
                    Gfx_StoreImageSlot(gGameSession->at4.loc.stage, gGameSession->at4.loc.area,
                                       gDisplayState.frameBuffer, 0x10000);
                    Mem_InitAux();
                    Stage_Ctx->field_12 = 0;
                    if ((s32)Stage_Ctx->field_1c < 0) {
                        Pad_ClearCooldown(0);
                        arg0->state = arg0->state + 1;
                        Display_TaskLoadStep(arg0);
                        return;
                    }
                    Stage_Ctx->field_28 = Stage_Ctx->field_28 + 1;
                }
                break;
            case 4:
                gDisplayState.at100.flags.flipMode    = 1;
                gDisplayState.at100.flags.imageSource = 3;
                Stage_Ctx->field_28                   = Stage_Ctx->field_28 + 1;
                break;
            case 5:
                Pad_ClearCooldown(0);
                Stage_Ctx->field_1c = Stage_Ctx->field_1c & 0xBFFFFFFF;
                break;
        }
    } else if (flags & 0x08000000) {
        Display_TransitionLoad(arg0);
    } else if ((s32)flags < 0) {
        arg0->state = arg0->state + 1;
        Display_TaskLoadStep(arg0);
    } else if (flags & 0x20000000) {
        Gfx_StoreImageSlot(gGameSession->at4.loc.stage, gGameSession->at4.loc.area, gDisplayState.frameBuffer,
                           0x10000);
        Stage_Ctx->field_1c = Stage_Ctx->field_1c & 0xDFFFFFFF;
    }

    if (Stage_Ctx->field_C == 4) {
        Display_FlipOtAndDispatch(0);
    }
}

void Display_FlipOtAndDispatch(s32 arg0)
{
    DisplayState* temp;
    u_long*       saved;
    s32           buf;
    u32           mode;

    temp           = &gDisplayState;
    saved          = gGpuCurrentOt;
    buf            = temp->otBuffer ^ 1;
    temp->otBuffer = buf;
    gpuBeginOt(buf);
    temp->at100.flags.flipMode = 0;
    temp->drawBuffer           = (u8)temp->frameBuffer;
    mode                       = Stage_Ctx->field_11;
    switch (mode) {
        case 3:
        case 0x20:
            Task_ExecDefaultList(&gTaskDefaultList);
            break;
        case 2:
            Gp_LinkViewSprts();
            Gp_DrawActorTmdActive(&Gpu_OtBuffers[temp->otBuffer]);
            break;
        case 1:
            Task_ExecListFiltered(&gTaskDefaultList, 0x62);
            Gp_LinkViewSprts();
            Gp_DrawActorTmdFlagged(&Gpu_OtBuffers[temp->otBuffer]);
            break;
    }
    gGpuCurrentOt = saved;
}

void Display_InvertFramebufferGray(void)
{
    s32          i;
    u32          maskR;
    u32          maskG;
    u32          maskB;
    u32          maskAll;
    u32*         p0;
    u32*         p1;
    u32          a0;
    u32          a2;
    u32          a1;
    register u32 v0 asm("v0");
    register u32 v1 asm("v1");

    v0      = 0xFFFDA800;
    i       = 0;
    maskR   = 0x001F001F;
    maskG   = 0x03E003E0;
    maskB   = 0x1F001F00;
    maskAll = 0x1F1F1F1F;
    v1      = Gpu_PrimHeapBase;
    p0      = (u32*)(v1 + v0);
    p1      = p0 + 1;

    do {
        i += 1;
        a0 = *p1;
        a2 = *p0;

        v1 = (a0 & maskR) << 8;
        v0 = a2 & maskR;
        v1 = v1 | v0;
        v0 = v1 << 1;
        a1 = v0 + v1;

        v1 = (a0 & maskG) << 3;
        a2 = a2 >> 5;
        v0 = a2 & maskR;
        v1 = v1 | v0;
        v0 = v1 << 2;
        a1 = a1 + v0;

        a0 = a0 >> 2;
        v1 = a0 & maskB;
        a2 = a2 >> 5;
        v0 = a2 & maskR;
        v1 = v1 | v0;
        a1 = a1 + v1;

        v0 = a1 >> 3;
        a1 = v0 & maskAll;
        a1 = maskAll - a1;

        a2 = a1 & maskR;
        v0 = a2 << 10;
        v1 = a2 << 5;
        v0 = v0 | v1;
        a2 = a2 | v0;

        a0 = a1 & maskB;
        a0 = a0 >> 8;
        v0 = a0 << 10;
        v1 = a0 << 5;
        v0 = v0 | v1;
        a0 = a0 | v0;

        *p0 = a2;
        *p1 = a0;
        p1 += 2;
        p0 += 2;
    } while (i < 0x4B00);
}

void Stage_InitOtAndSpawn(void)
{
    DisplayState* temp;

    Gpu_InitOtSmall();
    temp                       = &gDisplayState;
    temp->displayOwner         = 1;
    temp->at100.flags.flipMode = 2;
    temp->frameBuffer          = temp->otBuffer ^ 1;
    Task_InitList(&gTaskDisplayList);
    Task_SpawnFromTable(&D_8006269C, 0, 0, 0);
}

s32 Stage_SetEndingFlag(void)
{
    Stage_Ctx->field_1c |= 0x80000000;
    return 0;
}

s32 Stage_BeginTransition(s32 arg0, s32 arg1)
{
    StageCtx* temp;
    s32       mask;

    mask = 0x40000000;
    if (!(Stage_Ctx->field_1c & mask)) {
        Pad_SetCooldown(0);
        temp            = Stage_Ctx;
        temp->field_20  = arg0;
        temp->field_24  = 0;
        temp->field_28  = 0;
        temp->field_11  = arg1;
        temp->field_1c |= mask;
    }
    return (u8)gGameSession->at4.loc.view;
}

s32 Stage_BeginTransitionKind7(s32 arg0)
{
    StageCtx* temp;
    s32       mask;
    s32       ret;

    mask = 0x40000000;
    ret  = -1;
    if (!(Stage_Ctx->field_1c & mask)) {
        Pad_SetCooldown(0);
        temp                 = Stage_Ctx;
        temp->field_20       = arg0;
        temp->field_24       = 0;
        temp->field_28       = 0;
        temp->field_11       = 7;
        temp->field_1c      |= mask;
        ret                  = (u8)gGameSession->at4.loc.view;
        Stage_Ctx->field_1c |= 0x80000000;
    }
    return ret;
}

s32 Stage_RequestImageCapture(void)
{
    Stage_Ctx->field_1c |= 0x20000000;
    return 0;
}

s32 Stage_SetFadeRate(s32 arg0, s32 arg1, s32 arg2, s32 arg3)
{
    if (arg2 == 0) {
        Stage_Ctx->field_18 = 0x20;
    } else {
        Stage_Ctx->field_18 = arg2;
    }
    if (arg0 != 0) {
        Stage_Ctx->field_18 = -Stage_Ctx->field_18;
    }
    Stage_Ctx->field_19 = 0;
    if (arg1 != 0) {
        Stage_Ctx->field_19 |= 1;
    }
    if (arg3 != 0) {
        Stage_Ctx->field_19 |= 2;
    }
    return 0;
}

s32 Stage_GetFadeStatus(void)
{
    StageCtx* temp;
    u8        temp_a0;

    temp    = Stage_Ctx;
    temp_a0 = temp->field_17;
    if (temp_a0 == 0) {
        return 0;
    }
    if (temp_a0 >= temp->field_1a) {
        return 1;
    }
    return -1;
}

s32 Stage_HasTransitionFlags(void)
{
    return (Stage_Ctx->field_1c & 0x48000000) != 0;
}

void Stage_InitOtOnce(void)
{
    if (Stage_Ctx->field_13 == 0) {
        Gpu_InitOt();
        Stage_Ctx->field_13 = 1;
    }
}

void Stage_InitPrimBufOnce(void)
{
    if (Stage_Ctx->field_14 == 0) {
        Display_SetPrimBufLarge();
        Stage_Ctx->field_14 = 1;
    }
}

void Stage_ReleasePrimBuf(void)
{
    if (Stage_Ctx->field_14 == 1) {
        Display_SetPrimBufSmall();
        Stage_Ctx->field_14 = 0;
    }
}

void Stage_SetFadeMax(u8 arg0)
{
    Stage_Ctx->field_1a = arg0;
}

void Display_SetDrawMode(s32 arg0)
{
    switch (arg0) {
        case 0:
            gDisplayState.at100.flags.flipMode    = 1;
            gDisplayState.at100.flags.imageSource = 0;
            Display_SetAutoClear(0, 0, 0);
            return;
        case 1:
            gDisplayState.at100.flags.flipMode    = (u8)arg0;
            gDisplayState.at100.flags.imageSource = 3;
            Display_SetAutoClear(-1, 0, 0);
            return;
        case 2:
            gDisplayState.at100.flags.flipMode    = 1;
            gDisplayState.at100.flags.imageSource = 2;
            Display_SetAutoClear(-1, 0, 0);
            return;
        case 3:
            gDisplayState.at100.flags.flipMode = 2;
            return;
    }
}

s32 Stage_BeginTransitionKind3(void)
{
    StageCtx* temp;
    u32       flags;
    s32       val;

    temp  = Stage_Ctx;
    flags = temp->field_1c;
    if (!(flags & 0x40000000)) {
        temp->field_1c = flags | 0x50000000;
        val            = (u8)gGameSession->at4.loc.view;
        temp->field_24 = 0;
        temp->field_28 = 0;
        temp->field_11 = 3;
        temp->field_20 = val;
    }
    return 0;
}

s32 Display_InitModeObj(TaskDesc* arg0, s32 arg1, s32 arg2, s32 arg3)
{
    StageCtx* temp;

    if (gDisplayState.pendingMode != 0) {
        return 0;
    }

    MEM_CLEAR(Stage_Ctx, sizeof(StageCtx));

    temp          = Stage_Ctx;
    temp->field_0 = arg0;
    temp->field_4 = arg1;
    temp->field_8 = arg2;
    temp->field_C = arg3;
    if (arg3 == 0) {
        if ((GP_LOC_WORD(gGameSession->at4.loc) & GP_LOC_STAGE_AREA) == GP_LOC_KEY(1, 5, 0, 0)) {
            temp->field_C = 1;
        }
    }
    Stage_Ctx->field_1a       = 0xFF;
    gDisplayState.pendingMode = 0x81;
    return 0;
}

u8 Stage_GetModeByte12(void)
{
    return Stage_Ctx->field_12;
}

void Stage_SetModeAndFlip(u8 arg0)
{
    StageCtx* temp;

    temp = Stage_Ctx;
    if (temp->field_15 == 1) {
        temp->field_11 = arg0;
        Display_FlipOtAndDispatch(0);
    }
}

void Stage_ResetFade(void)
{
    Stage_Ctx->field_17 = 0;
    Stage_Ctx->field_1a = 0xFF;
}

void Stage_WaitCdActivate(Task* arg0)
{
    Pad_SetCooldown(0);
    if (CdCmd_ActivatePhase2() != 0) {
        arg0->state += 1;
    } else {
        Pad_States[0].cooldown = 1;
        Display_SpawnFromMode();
        arg0->state += 2;
    }
}

void Stage_WaitCdAndSpawn(Task* arg0)
{
    Pad_SetCooldown(0);
    if (func_8001D82C() != 0) {
        Pad_States[0].cooldown = 1;
        Display_SpawnFromMode();
        arg0->state += 1;
    }
}

void Display_TaskLoadStep(Task* arg0)
{
    u32 temp_v1;

    gDisplayState.at100.flags.flipMode = 2;
    temp_v1                            = Stage_Ctx->field_C;
    if (temp_v1 < 5U) {
        if (temp_v1 < 3U) {
            if (temp_v1 != 1) {
                goto block_3;
            }
        }
    } else {
    block_3:
        Mem_ConfigureAuxHeap(gGameSession->at4.loc.stage, gGameSession->at4.loc.area);
        Tmd_AllocMissingBuffers();
        Gp_AllocSprtLists();
    }
    CdCmd_EnqueueLoadFile(0, 0, 4);
    arg0->state = (s32)(arg0->state + 1);
    Stage_WaitCdEntry(arg0);
}

void Stage_WaitCdEntry(Task* arg0)
{
    if (func_8001D82C() != 0) {
        arg0->state += 1;
    }
}

void Stage_FinishCdFollowUp(Task* arg0)
{
    if (CdCmd_EnqueueFollowUp() != 0) {
        gDisplayState.displayOwner            = 0;
        gDisplayState.pendingMode             = 0;
        gDisplayState.at100.flags.imageSource = 1;
        Display_SetAutoClear(-1, 0, 0);
        Task_CallExit(arg0);
    }
}

void Display_DispatchTaskTable(Task* arg0)
{
    TaskFuncTable6 sp;

    sp = Display_TaskStates;
    sp.funcs[arg0->state](arg0);
    Display_StepFadeOverlay();
}

void Mdec_ResolveStreamBuffer(u8* arg0)
{
    u16         i;
    u16         found;
    s16         type;
    s16         neg;
    s32         key;
    s32         offset;
    CdCmdQueue* p;
    void*       base;

    p     = &CdCmd_Queue;
    i     = 0;
    found = 0;
    key   = *arg0;
loop:
    if (key == p->field_58[i].field_32) {
        goto matched;
    }
    i++;
    if (i < 5) {
        goto loop;
    }
done:
    if ((found & 0xFFFF) != 0) {
        if (p->field_218 == 0) {
            goto success;
        }
    }
    p->field_200 = 1;
    p->field_1FE = 0;
    neg          = -1;
    p->field_202 = neg;
    return;

matched:
    found = 1;
    goto done;

success:
    Stage_CdEntry = &p->field_58[i];
    type          = Stage_CdEntry->field_34;
    switch (type) {
        case 0:
            base = p->field_184;
            goto store_base;
        case 1:
            Mdec_DecodeBase = (u8*)D_8005C36C;
            if (p->field_190->field_1A == 1) {
                Mdec_DecodeBase = (u8*)D_8005C36C + 0x11000;
            }
            if (p->field_190->field_3 == 2) {
                Mdec_DecodeBase = Mdec_DecodeBase + p->field_190->field_1E;
            }
            gGameSession->field_7C = 0;
            break;
        case 2:
            Mdec_DecodeBase = (u8*)D_8005C370;
            if (p->field_190->field_1A == 2) {
                Mdec_DecodeBase = (u8*)D_8005C370 + 0x11000;
            }
            if (p->field_190->field_3 == 3) {
                Mdec_DecodeBase = Mdec_DecodeBase + p->field_190->field_1E;
            }
            gGameSession->field_7E = 0;
            break;
        case 3:
            Mdec_DecodeBase = (u8*)D_8005C374;
            if (p->field_190->field_1A == 3) {
                Mdec_DecodeBase = (u8*)D_8005C374 + 0x11000;
            }
            if (p->field_190->field_3 == 4) {
                Mdec_DecodeBase = Mdec_DecodeBase + p->field_190->field_1E;
            }
            gGameSession->field_80 = 0;
            break;
        case 4:
            base = p->field_198;
        store_base:
            Mdec_DecodeBase = base;
            break;
    }
    offset       = Stage_CdEntry->field_0;
    D_8007A35C   = 0;
    p->field_200 = 1;
    p->field_1FE = 0;
    p->field_202 = 0;
    D_8007A360   = Mdec_DecodeBase + offset;
}

void Mdec_StripCallback(void);

static __inline__ void mdecFinishDecode(void)
{
    CdCmdQueue* q = &CdCmd_Queue;

    if (gDisplayState.keepGraphics == 0) {
        Tmd_AllocMissingBuffers();
    }
    q->field_1FE = 0xFF;
    q->field_200 = 0;
    D_8007A35C   = 0;
    q->field_202 = 0;
    q->field_246 = 0;
}

void Mdec_ProcessDecode(void)
{
    CdCmdQueue* p;
    u16         i;
    s32         r;

    p = &CdCmd_Queue;
    switch ((s16)p->field_202) {
        case -1:
            Mdec_ResolveStreamBuffer(&gGameSession->at4.loc.view);
            if ((u32)++D_8007A358 >= 0x5B) {
                D_8007A358 = 0;
                Gpu_ResetGraphAndOt();
                if (Stage_CdEntry->field_34 == 0) {
                    p->field_188 = (s32)p->field_194;
                }
                mdecFinishDecode();
            }
            break;
        case 0:
            Gpu_ResetGraphAndOt();
            p->field_1EC = 1;
            if (p->field_238 == 1) {
                DecDCTvlcBuild((u16*)((u8*)Fs_ImgBuffers + 0x8800));
                p->field_234 = 0;
                p->field_18C = (u16*)((u8*)Fs_ImgBuffers + 0x8800);
            }
            DecDCTReset(0);
            DecDCTvlcSize2(0);
            DecDCTvlc2((u_long*)D_8007A360, (u_long*)gMemActiveAuxHeap,
                       (u_short*)p->field_18C);
            D_8007A35E = 1;
            DecDCToutCallback(Mdec_StripCallback);
            DecDCTin((u_long*)gMemActiveAuxHeap, p->field_22A);
            p->field_22A = 0;
            DecDCTout((u_long*)Fs_ImgBuffers, 0x780);
            D_8007A358 = 0;
            p->field_202++;
            /* fallthrough */
        case 1:
            if (p->field_1EC == 0) {
                for (i = 0; i < 3; i++) {
                    if (Stage_CdEntry->field_4[i] != 0) {
                        if (Stage_CdEntry->field_24[i] != 0) {
                            Fs_ChunkMode    = 2;
                            D5B498_8006C233 = -8;
                        }
                        Fs_CopyWorkEntries((FsWorkEntry*)(Mdec_DecodeBase + Stage_CdEntry->field_4[i]));
                        while (Fs_LoadImageStrip(1) != 1) {
                            r = Fs_LoadImageStrip(1);
                            if (r == 1) {
                                break;
                            }
                            if (r == 0x7F) {
                                Fs_CopyWorkEntries((FsWorkEntry*)(Mdec_DecodeBase + Stage_CdEntry->field_4[i]));
                            }
                        }
                        Fs_ChunkMode    = 0;
                        D5B498_8006C233 = 0;
                    }
                }
                for (i = 0; i < 3; i++) {
                    if (Stage_CdEntry->field_10[i] != 0) {
                        if (Stage_CdEntry->field_2A[i] != 0) {
                            Fs_ChunkMode    = 2;
                            D5B498_8006C234 = -3;
                        }
                        while (Fs_LoadImageChunk((FsImageChunk*)(Mdec_DecodeBase + Stage_CdEntry->field_10[i]), 1)) {
                        }
                        Fs_ChunkMode    = 0;
                        D5B498_8006C234 = 0;
                    }
                }
                p->field_21C = 1;
                if (Stage_CdEntry->field_34 == 0) {
                    p->field_188 = (s32)p->field_194;
                }
                if (p->field_190->field_3 != 0) {
                    Mem_CopyUnaligned(&Mdec_DecodeBase[Stage_CdEntry->field_1C], p->field_1A4,
                                      Stage_CdEntry->field_38);
                }
                mdecFinishDecode();
            } else if ((u32)++D_8007A358 >= 0x5B) {
                D_8007A358 = 0;
                Gpu_ResetGraphAndOt();
                if (Stage_CdEntry->field_34 == 0) {
                    p->field_188 = (s32)p->field_194;
                }
                mdecFinishDecode();
            }
            break;
    }
}

void Mdec_DecodeToVram(void)
{
    RECT          rect;
    s32           i;
    s32           temp;
    CdCmdQueue*   p;
    CdCmdQueue*   q;
    DisplayState* d;

    p = &CdCmd_Queue;
    switch ((s16)p->field_202) {
        case 0:
            Gpu_ResetGraphAndOt();
            p->field_1EC = 1;
            DecDCTReset(0);
            DecDCTvlcSize2(0);
            DecDCTvlc2((u_long*)D_8007A360, (u_long*)gMemActiveAuxHeap,
                       (u_short*)((u8*)Fs_ImgBuffers + 0x8800));
            D_8007A35E = 1;
            DecDCToutCallback(Mdec_StripCallback);
            DecDCTin((u_long*)gMemActiveAuxHeap, p->field_22A);
            p->field_22A = 0;
            DecDCTout((u_long*)Fs_ImgBuffers, 0x780);
            p->field_202 += 1;
            /* fallthrough */
        case 1:
            i = 0;
            if (p->field_1EC == 0) {
                rect.w = 0x10;
                rect.h = 0xF0;
                rect.y = (gDisplayState.drawBuffer ^ 1) * 0x110;
                do {
                    temp   = i & 0xFFFF;
                    rect.x = temp * 0x10;
                    LoadImage(&rect, (u_long*)Fs_ImgBuffers->buffers[temp]);
                    i++;
                } while ((u32)(i & 0xFFFF) < 0x14U);
                rect.w = 0x140;
                rect.x = 0;
                rect.h = 0xF0;
                d      = &gDisplayState;
                rect.y = (d->drawBuffer ^ 1) * 0x110;
                StoreImage(&rect, (u_long*)Fs_ImgBuffers);
                if (p->field_23E != 0) {
                    rect.x = 0;
                    rect.w = 0x1E0;
                    rect.h = 0xF0;
                    rect.y = d->drawBuffer * 0x110;
                    MoveImage(&rect, 0, (d->drawBuffer ^ 1) * 0x110);
                    p->field_23E = 0;
                } else {
                    ClearImage(&rect, 0, 0, 0);
                }
                p->field_21C = 0;
                q            = &CdCmd_Queue;
                if (gDisplayState.keepGraphics == 0) {
                    Tmd_AllocMissingBuffers();
                }
                q->field_1FE = 0xFF;
                q->field_200 = 0;
                D_8007A35C   = 0;
                q->field_202 = 0;
                q->field_246 = 0;
            }
            return;
    }
}

void CdCmd_StepVlcRebuild(void)
{
    CdCmdQueue* p;

    p = &CdCmd_Queue;
    if (p->field_214 == 0) {
        if (p->field_234 != 0) {
            DecDCTvlcBuild((u16*)((u8*)Fs_ImgBuffers + 0x8800));
            p->field_234 = 0;
        }
        if ((p->field_200 != 0) && (p->field_234 == 0)) {
            Mdec_DecodeToVram();
        }
    } else if (p->field_200 != 0) {
        Mdec_ProcessDecode();
    }
}

void Mdec_BeginDecode(void* arg0)
{
    CdCmdQueue* p;

    D_8007A35C   = 0;
    p            = &CdCmd_Queue;
    p->field_200 = 1;
    p->field_1FE = 0;
    p->field_202 = 0;
    D_8007A360   = arg0;
    D_8007A358   = 0;
}

void CdCmd_RequestVlcRebuild(void)
{
    CdCmd_Queue.field_234 = 1;
}

void Mdec_StripCallback(void)
{
    s32         temp;
    CdCmdQueue* p;

    temp = 0x140 / (D_8007A35E * 16);
    p    = &CdCmd_Queue;
    if (D_8007A35C == temp - 1) {
        p->field_1EC = 0;
        DecDCToutCallback(0);
    } else {
        D_8007A35C = D_8007A35C + 1;
        DecDCTout(Fs_ImgBuffers->buffers[D_8007A35C], 0x780);
    }
}

void Stage_TaskExit(Task* arg0)
{
    Task_CallExit(arg0);
}
