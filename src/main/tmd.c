#include "common.h"

#include "main/game.h"
#include "main/mem.h"
#include "main/unknown_syms.h"
#include "psyq/libpress.h"
#include "psyq/inline_c.h"

#define gte_rtir_real() __asm__ volatile("nop; nop; .word 0x4A49E012")

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

    if ((s8)Display_State.field_103 != 2) {
        temp = (s8)p->field_18;
        if (temp != 0) {
            product = temp * Display_State.field_10a;
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

        tile       = (TILE*)D_80071190;
        D_80071190 = (DR_TPAGE*)(tile + 1);
        yoff       = Display_State.field_109;
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

        dr         = (DR_TPAGE*)D_80071190;
        D_80071190 = dr + 1;
        if (!(Stage_Ctx->field_19 & 1)) {
            setlen(dr, 1);
            dr->code[0] = 0xE1000240;
        } else {
            setlen(dr, 1);
            dr->code[0] = 0xE1000220;
        }

        ot = (u_long*)((otIdx << 2) + (s32)D_800710A0);
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
    Stage_Ctx->field_24     = Display_State.field_118;
    Display_State.field_122 = 1;
    Gfx_LoadImageSlot(Game_Session->field_7, Game_Session->field_6, Display_State.field_118);
    Display_State.field_103 = 2;
    Stage_Ctx->field_28     = Stage_Ctx->field_28 + 1;
    goto end;
case1:
    if (CdCmd_IsIdle() & 0xFFFF) {
        CdCmd_Enqueue(0x21, Stage_Ctx->field_2C, Stage_Ctx->field_34);
        Stage_Ctx->field_28 = Stage_Ctx->field_28 + 1;
    }
    goto end;
case2:
    if ((CdCmd_IsIdle() & 0xFFFF) && (Display_State.field_118 != Stage_Ctx->field_24)) {
        Gfx_StoreImageSlot(Game_Session->field_7, Game_Session->field_6, Display_State.field_118, 0x10000);
        Mem_InitAux();
        rect.x = 0;
        rect.w = 0x140;
        rect.h = 0xF0;
        rect.y = (Display_State.field_118 ^ 1) * 0x110;
        ClearImage(&rect, 0, 0, 0);
        rect.x = 0;
        rect.w = 0x140;
        rect.h = 0xF0;
        rect.y = Display_State.field_118 * 0x110;
        ClearImage(&rect, 0, 0, 0);
        DrawSync(0);
        Stage_Ctx->field_12 = 1;
        Stage_Ctx->field_28 = Stage_Ctx->field_28 + 1;
    }
    goto end;
case3:
    Display_State.field_103 = 1;
    Display_State.field_100 = 2;
    Stage_Ctx->field_28     = Stage_Ctx->field_28 + 1;
default_case:
    SetDispMask(1);
    Stage_Ctx->field_1c = Stage_Ctx->field_1c & 0xF7FFFFFF;
end:
    return 1;
}

Task* Display_SpawnFromMode(void)
{
    Task*             ret;
    u32               mode;
    Task*             slot;
    GameActor*        obj;
    s32*              ptr;
    GameSessionFrom4* ed;
    s32               flag;

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
        slot                = Game_GetPtrSlot(3);
        obj                 = (GameActor*)slot->field_1C;
        flag                = obj->field_984 & 1;
        ptr                 = ((GameActorExt*)slot->field_2c)->field_8;
        if (flag) {
            func_801011D0(ptr, obj->field_90, 6, &obj->field_930);
        }
        func_800E1A6C(&obj->field_17C);
        *ptr = 0;
    block_case13:
        Stage_Ctx->field_15 = 1;
        if (Stage_Ctx->field_C == 3) {
            Display_State.field_103 = 2;
            Display_State.field_100 = 0;
        } else {
            Display_State.field_103 = 0;
            Display_State.field_100 = 1;
        }
    } else {
        goto block_end;
    }
    goto block_end;

block_default:
    ed = (GameSessionFrom4*)&Game_Session->field_4;
    Gpu_ResetGraphAndOt();
    Gfx_StoreImageSlot(ed->field_3, ed->field_2, Display_State.field_1f, 0x10000);
    if (Stage_Ctx->field_C == 0x100) {
        Display_InvertFramebufferGray();
    }
    Mem_InitAux();
    Display_State.field_103 = 1;
    Display_State.field_100 = 3;
    slot                    = Game_GetPtrSlot(3);
    obj                     = (GameActor*)slot->field_1C;
    flag                    = obj->field_984 & 1;
    ptr                     = ((GameActorExt*)slot->field_2c)->field_8;
    if (flag) {
        func_801011D0(ptr, obj->field_90, 6, &obj->field_930);
    }
    func_800E1A6C(&obj->field_17C);
    *ptr = 0;

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
                Stage_Ctx->field_24     = Display_State.field_118;
                Game_Session->field_4   = Stage_Ctx->field_20;
                Stage_Ctx->field_C      = 0;
                Display_State.field_103 = 2;
                Mem_ConfigureAuxHeap(Game_Session->field_7, Game_Session->field_6);
                if (!(Stage_Ctx->field_1c & 0x10000000)) {
                    ((Task*)Game_GetPtrSlot(1))->field_34 = (u8)Game_Session->field_4;
                    ResetGraph(1);
                    F179D4_ClearOTag(0);
                    F179D4_ClearOTag(1);
                    Mem_InitAux();
                    D_8007216C = Game_Session->field_4;
                    Pad_SetCooldown(0);
                    func_800A8DC0(2);
                    Game_Session->field_4D = 0;
                    Task_Spawn(0, 0x1E, 2, 0);
                } else {
                    Tmd_AllocMissingBuffers();
                    Game_Session->field_4D = 1;
                }
                Stage_Ctx->field_28 = Stage_Ctx->field_28 + 1;
                break;
            case 1:
                ed   = Game_Session;
                flag = ed->field_4D;
                if (flag == 1) {
                    disp = Display_State.field_118;
                    g    = Stage_Ctx;
                    if (disp == g->field_24) {
                        f11          = g->field_11;
                        ed->field_4D = 0;
                        if (f11 == 0) {
                            arg0->field_2a      = flag;
                            Stage_Ctx->field_28 = Stage_Ctx->field_28 + 2;
                        } else {
                            Stage_Ctx->field_28 = Stage_Ctx->field_28 + 1;
                        }
                    }
                    CdCmd_ActivatePhase2();
                }
                break;
            case 2:
                Display_State.field_114 = Display_State.field_118;
                Display_FlipOtAndDispatch(0);
                Stage_Ctx->field_19     = Stage_Ctx->field_19 | 0x80;
                Display_State.field_103 = Display_State.field_103 | 0x10;
                arg0->field_2a          = 3;
                Stage_Ctx->field_28     = Stage_Ctx->field_28 + 1;
                break;
            case 3:
                Display_State.field_103 = 2;
                arg0->field_2a          = arg0->field_2a - 1;
                if (arg0->field_2a == 0) {
                    Gpu_ResetGraphAndOt();
                    Gfx_StoreImageSlot(Game_Session->field_7, Game_Session->field_6,
                                       Display_State.field_118, 0x10000);
                    Mem_InitAux();
                    Stage_Ctx->field_12 = 0;
                    if ((s32)Stage_Ctx->field_1c < 0) {
                        Pad_ClearCooldown(0);
                        arg0->field_30 = arg0->field_30 + 1;
                        Display_TaskLoadStep(arg0);
                        return;
                    }
                    Stage_Ctx->field_28 = Stage_Ctx->field_28 + 1;
                }
                break;
            case 4:
                Display_State.field_103 = 1;
                Display_State.field_100 = 3;
                Stage_Ctx->field_28     = Stage_Ctx->field_28 + 1;
                break;
            case 5:
                Pad_ClearCooldown(0);
                Stage_Ctx->field_1c = Stage_Ctx->field_1c & 0xBFFFFFFF;
                break;
        }
    } else if (flags & 0x08000000) {
        Display_TransitionLoad(arg0);
    } else if ((s32)flags < 0) {
        arg0->field_30 = arg0->field_30 + 1;
        Display_TaskLoadStep(arg0);
    } else if (flags & 0x20000000) {
        Gfx_StoreImageSlot(Game_Session->field_7, Game_Session->field_6, Display_State.field_118,
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
    u_long*       ot;
    u32           mode;

    temp            = &Display_State;
    saved           = D_800710A0;
    buf             = temp->field_114 ^ 1;
    temp->field_114 = buf;
    D_800710A0      = D5F414_OrderingTables + buf * C5F414_OTAG_ENTRIES;
    ClearOTagR(D_800710A0, C5F414_OTAG_ENTRIES);
    ot              = D_800710A0;
    *ot             = C5F414_OTAG_END_PRIM;
    D_800710A0      = ot + 0x20;
    temp->field_103 = 0;
    temp->field_1f  = *(u8*)&temp->field_118;
    mode            = Stage_Ctx->field_11;
    switch (mode) {
        case 3:
        case 0x20:
            Task_ExecDefaultList(&Task_DefaultList);
            break;
        case 2:
            func_800AC688();
            func_8009850C(&Gpu_OtBuffers[temp->field_114]);
            break;
        case 1:
            Task_ExecListFiltered(&Task_DefaultList, 0x62);
            func_800AC688();
            func_80097AC0(&Gpu_OtBuffers[temp->field_114]);
            break;
    }
    D_800710A0 = saved;
}

void Display_InvertFramebufferGray(void)
{
    s32           i;
    u32           maskR;
    u32           maskG;
    u32           maskB;
    u32           maskAll;
    register u32* p0 asm("a3");
    u32*          p1;
    register u32  a0 asm("a0");
    register u32  a2 asm("a2");
    register u32  a1 asm("a1");
    register u32  v0 asm("v0");
    register u32  v1 asm("v1");

    v0      = 0xFFFDA800;
    i       = 0;
    maskR   = 0x001F001F;
    maskG   = 0x03E003E0;
    maskB   = 0x1F001F00;
    maskAll = 0x1F1F1F1F;
    v1      = D_80068F88;
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

void func_8003F690(void)
{
    DisplayState* temp;

    Gpu_InitOtSmall();
    temp            = &Display_State;
    temp->field_1e  = 1;
    temp->field_103 = 2;
    temp->field_118 = temp->field_114 ^ 1;
    Task_InitList(&D_8007A110);
    Task_SpawnFromTable(&D_8006269C, 0, 0, 0);
}

s32 func_8003F6F8(void)
{
    Stage_Ctx->field_1c |= 0x80000000;
    return 0;
}

s32 func_8003F71C(s32 arg0, s32 arg1)
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
    return (u8)Game_Session->field_4;
}

s32 Display_BeginMode7(s32 arg0)
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
        ret                  = (u8)Game_Session->field_4;
        Stage_Ctx->field_1c |= 0x80000000;
    }
    return ret;
}

s32 Display_SetFlag20000000(void)
{
    Stage_Ctx->field_1c |= 0x20000000;
    return 0;
}

s32 Display_SetFadeRate(s32 arg0, s32 arg1, s32 arg2, s32 arg3)
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

s32 func_8003F900(void)
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

s32 Display_HasTransitionFlags(void)
{
    return (Stage_Ctx->field_1c & 0x48000000) != 0;
}

void func_8003F964(void)
{
    if (Stage_Ctx->field_13 == 0) {
        Gpu_InitOt();
        Stage_Ctx->field_13 = 1;
    }
}

void Display_InitPrimBufOnce(void)
{
    if (Stage_Ctx->field_14 == 0) {
        Display_SetPrimBufLarge();
        Stage_Ctx->field_14 = 1;
    }
}

void func_8003F9F4(void)
{
    if (Stage_Ctx->field_14 == 1) {
        Display_SetPrimBufSmall();
        Stage_Ctx->field_14 = 0;
    }
}

void Display_SetFadeMax(u8 arg0)
{
    Stage_Ctx->field_1a = arg0;
}

void Display_SetDrawMode(s32 arg0)
{
    switch (arg0) {
        case 0:
            Display_State.field_103 = 1;
            Display_State.field_100 = 0;
            Display_SetAutoClear(0, 0, 0);
            return;
        case 1:
            Display_State.field_103 = (u8)arg0;
            Display_State.field_100 = 3;
            Display_SetAutoClear(-1, 0, 0);
            return;
        case 2:
            Display_State.field_103 = 1;
            Display_State.field_100 = 2;
            Display_SetAutoClear(-1, 0, 0);
            return;
        case 3:
            Display_State.field_103 = 2;
            return;
    }
}

s32 Display_BeginTransition(void)
{
    StageCtx* temp;
    u32       flags;
    s32       val;

    temp  = Stage_Ctx;
    flags = temp->field_1c;
    if (!(flags & 0x40000000)) {
        temp->field_1c = flags | 0x50000000;
        val            = (u8)Game_Session->field_4;
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
    u8*       ptr;
    u32       i;

    if (Display_State.field_10d != 0) {
        return 0;
    }

    ptr = (u8*)Stage_Ctx;
    for (i = 0; i < 0x38U; i++) {
        *ptr++ = 0;
    }

    temp          = Stage_Ctx;
    temp->field_0 = arg0;
    temp->field_4 = arg1;
    temp->field_8 = arg2;
    temp->field_C = arg3;
    if (arg3 == 0) {
        if ((*(u32*)&Game_Session->field_4 & 0xFFFF0000) == 0x1050000) {
            temp->field_C = 1;
        }
    }
    Stage_Ctx->field_1a     = 0xFF;
    Display_State.field_10d = 0x81;
    return 0;
}

u8 Display_GetModeByte12(void)
{
    return Stage_Ctx->field_12;
}

void func_8003FC30(u8 arg0)
{
    StageCtx* temp;

    temp = Stage_Ctx;
    if (temp->field_15 == 1) {
        temp->field_11 = arg0;
        Display_FlipOtAndDispatch(0);
    }
}

void func_8003FC6C(void)
{
    Stage_Ctx->field_17 = 0;
    Stage_Ctx->field_1a = 0xFF;
}

void func_8003FC8C(Task* arg0)
{
    Pad_SetCooldown(0);
    if (CdCmd_ActivatePhase2() != 0) {
        arg0->field_30 += 1;
    } else {
        Pad_States[0].field_A = 1;
        Display_SpawnFromMode();
        arg0->field_30 += 2;
    }
}

void func_8003FCF8(Task* arg0)
{
    Pad_SetCooldown(0);
    if (func_8001D82C() != 0) {
        Pad_States[0].field_A = 1;
        Display_SpawnFromMode();
        arg0->field_30 += 1;
    }
}

void Display_TaskLoadStep(Task* arg0)
{
    u32 temp_v1;

    Display_State.field_103 = 2;
    temp_v1                 = Stage_Ctx->field_C;
    if (temp_v1 < 5U) {
        if (temp_v1 < 3U) {
            if (temp_v1 != 1) {
                goto block_3;
            }
        }
    } else {
    block_3:
        Mem_ConfigureAuxHeap(Game_Session->field_7, Game_Session->field_6);
        Tmd_AllocMissingBuffers();
        func_800ACAA8();
    }
    CdCmd_EnqueueLoadFile(0, 0, 4);
    arg0->field_30 = (s32)(arg0->field_30 + 1);
    func_8003FE00(arg0);
}

void func_8003FE00(Task* arg0)
{
    if (func_8001D82C() != 0) {
        arg0->field_30 += 1;
    }
}

void func_8003FE40(Task* arg0)
{
    if (CdCmd_EnqueueFollowUp() != 0) {
        Display_State.field_1e  = 0;
        Display_State.field_10d = 0;
        Display_State.field_100 = 1;
        Display_SetAutoClear(-1, 0, 0);
        Task_CallExit(arg0);
    }
}

void Display_DispatchTaskTable(Task* arg0)
{
    TaskFuncTable6 sp;

    sp = D_80013E98;
    sp.funcs[arg0->field_30](arg0);
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
    D_8007A368 = &p->field_58[i];
    type       = D_8007A368->field_34;
    switch (type) {
        case 0:
            base = p->field_184;
            goto store_base;
        case 1:
            D_8007A364 = (u8*)D_8005C36C;
            if (p->field_190->field_1A == 1) {
                D_8007A364 = (u8*)D_8005C36C + 0x11000;
            }
            if (p->field_190->field_3 == 2) {
                D_8007A364 = D_8007A364 + p->field_190->field_1E;
            }
            Game_Session->field_7C = 0;
            break;
        case 2:
            D_8007A364 = (u8*)D_8005C370;
            if (p->field_190->field_1A == 2) {
                D_8007A364 = (u8*)D_8005C370 + 0x11000;
            }
            if (p->field_190->field_3 == 3) {
                D_8007A364 = D_8007A364 + p->field_190->field_1E;
            }
            Game_Session->field_7E = 0;
            break;
        case 3:
            D_8007A364 = (u8*)D_8005C374;
            if (p->field_190->field_1A == 3) {
                D_8007A364 = (u8*)D_8005C374 + 0x11000;
            }
            if (p->field_190->field_3 == 4) {
                D_8007A364 = D_8007A364 + p->field_190->field_1E;
            }
            Game_Session->field_80 = 0;
            break;
        case 4:
            base = p->field_198;
        store_base:
            D_8007A364 = base;
            break;
    }
    offset       = D_8007A368->field_0;
    D_8007A35C   = 0;
    p->field_200 = 1;
    p->field_1FE = 0;
    p->field_202 = 0;
    D_8007A360   = D_8007A364 + offset;
}

INCLUDE_ASM("main/nonmatchings/tmd", func_8004017C);

void Mdec_StripCallback(void);

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
            DecDCTvlc2((u_long*)D_8007A360, (u_long*)GActiveAuxHeap,
                       (u_short*)((u8*)D4CB64_ImgBuffers + 0x8800));
            D_8007A35E = 1;
            DecDCToutCallback(Mdec_StripCallback);
            DecDCTin((u_long*)GActiveAuxHeap, p->field_22A);
            p->field_22A = 0;
            DecDCTout((u_long*)D4CB64_ImgBuffers, 0x780);
            p->field_202 += 1;
            /* fallthrough */
        case 1:
            i = 0;
            if (p->field_1EC == 0) {
                rect.w = 0x10;
                rect.h = 0xF0;
                rect.y = (Display_State.field_1f ^ 1) * 0x110;
                do {
                    temp   = i & 0xFFFF;
                    rect.x = temp * 0x10;
                    LoadImage(&rect, (u_long*)((u8*)D4CB64_ImgBuffers + (temp * 0x1E00)));
                    i++;
                } while ((u32)(i & 0xFFFF) < 0x14U);
                rect.w = 0x140;
                rect.x = 0;
                rect.h = 0xF0;
                d      = &Display_State;
                rect.y = (d->field_1f ^ 1) * 0x110;
                StoreImage(&rect, (u_long*)D4CB64_ImgBuffers);
                if (p->field_23E != 0) {
                    rect.x = 0;
                    rect.w = 0x1E0;
                    rect.h = 0xF0;
                    rect.y = d->field_1f * 0x110;
                    MoveImage(&rect, 0, (d->field_1f ^ 1) * 0x110);
                    p->field_23E = 0;
                } else {
                    ClearImage(&rect, 0, 0, 0);
                }
                p->field_21C = 0;
                q            = &CdCmd_Queue;
                if ((s8)Display_State.field_122 == 0) {
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
            DecDCTvlcBuild((u16*)((u8*)D4CB64_ImgBuffers + 0x8800));
            p->field_234 = 0;
        }
        if ((p->field_200 != 0) && (p->field_234 == 0)) {
            Mdec_DecodeToVram();
        }
    } else if (p->field_200 != 0) {
        func_8004017C();
    }
}

void func_800408C0(void* arg0)
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
        DecDCTout(D4CB64_ImgBuffers->buffers[D_8007A35C], 0x780);
    }
}

void func_800409B0(Task* arg0)
{
    Task_CallExit(arg0);
}

extern u8 D_800105AC;
extern u8 D_800105F4;
extern u8 D_80010718;
extern u8 D_80010A90;
extern u8 D_80010BF4;
extern u8 D_80010E1C;
extern u8 D_80010EF4;
extern u8 D_80010F08;
extern u8 D_8001108C;
extern u8 D_800110A0;
extern u8 D_800112DC;
extern u8 D_800113B4;
extern u8 D_800113E8;
extern u8 D_800114FC;
extern u8 D_80011530;
extern u8 D_80011678;
extern u8 D_800117BC;
extern u8 D_80011994;
extern u8 D_800119A8;
extern u8 D_80011AF4;
extern u8 D_80011B08;
extern u8 D_8001237C;
extern u8 D_80012520;
extern u8 D_80099994;
extern u8 D_80099B94;
extern u8 D_80099D40;
extern u8 D_80099FF4;
extern u8 D_8009A348;
extern u8 D_8009A57C;
extern u8 D_8009AF90;
extern u8 D_8009B2F4;
extern u8 D_8009B500;
extern u8 D_8009BD00;
extern u8 D_8009C024;
extern u8 D_8009C414;
extern u8 D_8009CED0;
extern u8 D_8009D0DC;
extern u8 D_8009D388;
extern u8 D_8009D518;
extern u8 D_8009D718;
extern u8 D_8009D900;
extern u8 D_8009DB00;
extern u8 D_8009DCB8;
extern u8 D_8009DE48;
extern u8 D_8009E048;
extern u8 D_8009E274;
extern u8 D_8009E4A0;
extern u8 D_8009E770;
extern u8 D_8009EAA4;
extern u8 D_8009EB84;
extern u8 D_8009EC1C;
extern u8 D_8009ECC0;
extern u8 D_8009ED28;
extern u8 D_8009ED90;
extern u8 D_8009EE28;
extern u8 D_8009EECC;
extern u8 D_8009EF64;
extern u8 D_8009EFFC;
extern u8 D_8009F0A0;
extern u8 D_8009F144;
extern u8 D_8009F1DC;
extern u8 D_8009F280;
extern u8 D_8009F360;
extern u8 D_8009F3F8;
extern u8 D_8009F49C;
extern u8 D_8009F504;
extern u8 D_8009F56C;
extern u8 D_8009F670;
extern u8 D_8009F708;
extern u8 D_8009F824;
extern u8 D_8009F8C8;
extern u8 D_8009F970;
extern u8 D_8009FA24;
extern u8 D_8009FB28;
extern u8 D_8009FC44;
extern u8 D_8009FC90;
extern u8 D_80136224;
extern u8 D_80136500;
extern u8 D_8013685C;
extern u8 D_80136C00;
extern u8 D_8013700C;
extern u8 D_80137300;
extern u8 D_801375F8;
extern u8 D_801379B4;
extern u8 D_80138004;
extern u8 D_801386EC;

/// 0x88-byte scratch workspace allocated from G_SCRATCH_HEAD for Tmd_ProcessStream.
typedef struct {
    /* 0x00 */ u8*        field_0;
    /* 0x04 */ u8*        field_4;
    /* 0x08 */ s32        field_8;
    /* 0x0C */ s32        field_C;
    /* 0x10 */ byte       pad_10[0x8];
    /* 0x18 */ s32        field_18;
    /* 0x1C */ s32        field_1C;
    /* 0x20 */ u32        field_20;
    /* 0x24 */ byte       pad_24[0x4C];
    /* 0x70 */ s16        field_70;
    /* 0x72 */ s16        field_72;
    /* 0x74 */ byte       pad_74[0xC];
    /* 0x80 */ TmdObject* field_80;
    /* 0x84 */ byte       pad_84[0x4];
} ScratchModelBlock;

/// 0x98-byte scratch workspace for Tmd_SetupDraw (draw path).
typedef struct {
    /* 0x00 */ u8*        field_0;
    /* 0x04 */ u8*        field_4;
    /* 0x08 */ s32        field_8;
    /* 0x0C */ s32        field_C;
    /* 0x10 */ void*      field_10;
    /* 0x14 */ u_long*    field_14;
    /* 0x18 */ byte       pad_18[0x38];
    /* 0x50 */ MATRIX     mat;
    /* 0x70 */ byte       pad_70[0x10];
    /* 0x80 */ TmdObject* field_80;
    /* 0x84 */ s32        field_84;
} ScratchDrawBlock; /* 0x98 */

typedef u32* (*ModelStreamHandler)(ScratchModelBlock* ws, s32 arg1, u32* stream);

extern MATRIX D_80070F34;
extern void   func_80010848(ScratchDrawBlock* ws, u32 flags, void* stream, TmdObject* node);

void Tmd_InitSourceStream(TmdSource* arg0)
{
    u32*         stream;
    u32          id;
    u32          dims;
    void*        handler;
    s32          flag;
    register u32 tmp asm("v0");

    stream = arg0->field_20;
    if (arg0->field_0 == 0) {
        tmp  = *(u32*)&Game_Session->field_4;
        tmp  = (tmp & 0xFFFF0000) ^ 0x02100000;
        flag = tmp < 1;
        goto read_id;

        for (;;) {
            switch (id) {
                case 0x20:
                case 0x22:
                    handler = &D_80010A90;
                    break;
                case 0x60:
                case 0x62:
                    handler = &D_80010BF4;
                    break;
                case 0xC0:
                    handler = &D_80010E1C;
                    break;
                case 0xC4:
                    handler = &D_8009EAA4;
                    break;
                case 5:
                    handler = &D_80099B94;
                    break;
                case 0x45:
                    handler = &D_80099994;
                    break;
                case 0x21:
                case 0x121:
                    handler = &D_800105F4;
                    break;
                case 0x61:
                case 0x161:
                    handler = &D_80010718;
                    break;
                case 0x38:
                    handler = &D_80010F08;
                    break;
                case 0x8038:
                    handler = &D_80136224;
                    break;
                case 0x10038:
                    handler = &D_8013700C;
                    break;
                case 0x20038:
                    handler = &D_801379B4;
                    break;
                case 0x3A:
                    handler = &D_80010EF4;
                    break;
                case 0x1003A:
                    handler = &D_80137300;
                    break;
                case 0x78:
                    handler = &D_800110A0;
                    break;
                case 0x8078:
                    handler = &D_8013685C;
                    break;
                case 0x10078:
                    handler = &D_801375F8;
                    break;
                case 0x20078:
                    handler = &D_80138004;
                    break;
                case 0x7A:
                    handler = &D_8001108C;
                    break;
                case 0xC8:
                    handler = &D_800112DC;
                    break;
                case 0x40C8:
                    handler = &D_8009AF90;
                    if (flag != 0) {
                        handler = &D_8009B2F4;
                    }
                    break;
                case 0x200C8:
                    handler = &D_801386EC;
                    break;
                case 0x31:
                case 0x39:
                case 0x131:
                    handler = &D_800113E8;
                    break;
                case 0x8039:
                    handler = &D_80136500;
                    break;
                case 0x3B:
                    handler = &D_800113B4;
                    break;
                case 0x71:
                case 0x79:
                case 0x171:
                    handler = &D_80011530;
                    break;
                case 0x8079:
                    handler = &D_80136C00;
                    break;
                case 0x7B:
                    handler = &D_800114FC;
                    break;
                case 0x4039:
                    handler = &D_80099D40;
                    if (flag != 0) {
                        handler = &D_8009A348;
                    }
                    break;
                case 0x4079:
                    handler = &D_80099FF4;
                    if (flag != 0) {
                        handler = &D_8009A57C;
                    }
                    break;
                case 0:
                    handler = &D_80011678;
                    break;
                case 0x40:
                    handler = &D_800117BC;
                    break;
                case 0x18:
                    handler = &D_800119A8;
                    break;
                case 0x1A:
                    handler = &D_80011994;
                    break;
                case 0x58:
                    handler = &D_80011AF4;
                    break;
                case 0x5A:
                    handler = &D_80011B08;
                    break;
                case 0x4078:
                    handler = &D_8009C414;
                    if (flag != 0) {
                        handler = &D_8009C024;
                    }
                    break;
                case 0x4038:
                    handler = &D_8009B500;
                    if (flag != 0) {
                        handler = &D_8009BD00;
                    }
                    break;
                case 0x120:
                    handler = &D_8009E048;
                    break;
                case 0x122:
                    handler = &D_8009E274;
                    break;
                case 0x160:
                    handler = &D_8009E4A0;
                    break;
                case 0x162:
                    handler = &D_8009E770;
                    break;
                case 0x1C:
                    handler = &D_8009D388;
                    break;
                case 0x1E:
                    handler = &D_8009DCB8;
                    break;
                case 0x5C:
                    handler = &D_8009D518;
                    break;
                case 0x5E:
                    handler = &D_8009DE48;
                    break;
                case 0x30:
                    handler = &D_8009CED0;
                    break;
                case 0x130:
                    handler = &D_8001237C;
                    break;
                case 0x70:
                    handler = &D_8009D0DC;
                    break;
                case 0x170:
                    handler = &D_80012520;
                    break;
                case 0x156:
                    handler = &D_8009D718;
                    break;
                case 4:
                    handler = &D_8009DB00;
                    break;
                case 0x44:
                    handler = &D_8009D900;
                    break;
                default:
                    handler = &D_800105AC;
                    break;
            }

            stream++;
            *stream = (u32)handler;
            stream++;
            dims = *stream;
            stream++;
            id      = dims & 0xFFFF;
            stream += (dims >> 16) * id;
            id      = *stream;

            while (1) {
                if (id != -2U) {
                    break;
                }
                stream++;
            read_id:
                id = *stream;
                if (id == -1U) {
                    goto done;
                }
            }
        }
    done:
        arg0->field_0 = 1;
    }
}

void Tmd_ProcessStream(TmdObject* arg0)
{
    ScratchModelBlock* ws;
    TmdSource*         src;
    u32*               stream;
    u32                id;
    void*              handler;
    s32                flag;
    void*              buf;
    u32                hi;

    flag = 0;
    {
        register void**             scratch asm("a0");
        register ScratchModelBlock* head asm("v1");
        void*                       tmp;

        scratch  = (void**)G_SCRATCH_HEAD;
        src      = arg0->field_10;
        tmp      = *scratch;
        stream   = src->field_20;
        hi       = *(u32*)&Game_Session->field_4;
        head     = (ScratchModelBlock*)((u8*)tmp - 0x88);
        hi      &= 0xFFFF0000;
        *scratch = head;
        if ((hi == 0x020F0000) || (hi == 0x02100000)) {
            flag = 1;
        }
        ws = head;
    }

    ws->field_80 = arg0;
    buf          = arg0->field_18;
    ws->field_0  = buf;
    if (arg0->field_14 != 0) {
        ws->field_0 = (u8*)buf + arg0->field_16;
    }
    ws->field_4     = ws->field_0;
    ws->field_0     = (u8*)ws->field_0 + src->field_8;
    arg0->field_14 ^= 1;
    ws->field_8     = arg0->field_10->field_14;
    ws->field_C     = arg0->field_10->field_18;
    ws->field_70    = (s8)arg0->field_24;
    ws->field_72    = (s8)arg0->field_25 << 6;
    goto read_id;

    for (;;) {
        switch (id) {
            case 0x4038:
                handler = &D_8009F670;
                if (flag != 0) {
                    handler = &D_8009F56C;
                }
                break;
            case 0x38:
            case 0x3A:
            case 0x8038:
            case 0x10038:
            case 0x1003A:
            case 0x20038:
                handler = &D_8009ED90;
                break;
            case 0x4078:
                handler = &D_8009F824;
                if (flag != 0) {
                    handler = &D_8009F708;
                }
                break;
            case 0x78:
            case 0x7A:
            case 0x8078:
            case 0x10078:
            case 0x20078:
                handler = &D_8009EE28;
                break;
            case 0x31:
            case 0x39:
            case 0x3B:
            case 0x131:
            case 0x8039:
                handler = &D_8009EB84;
                break;
            case 0x71:
            case 0x79:
            case 0x7B:
            case 0x171:
            case 0x8079:
                handler = &D_8009EC1C;
                break;
            case 0x4039:
                handler = &D_8009F8C8;
                if (flag != 0) {
                    handler = &D_8009FA24;
                }
                break;
            case 0x4079:
                handler = &D_8009F970;
                if (flag != 0) {
                    handler = &D_8009FB28;
                }
                break;
            case 0x18:
            case 0x1A:
                handler = &D_8009F144;
                break;
            case 0x58:
            case 0x5A:
                handler = &D_8009F1DC;
                break;
            case 0x1C:
            case 0x1E:
                handler = &D_8009F360;
                break;
            case 0x5C:
            case 0x5E:
                handler = &D_8009F3F8;
                break;
            case 0x30:
                handler = &D_8009EECC;
                break;
            case 0x130:
                handler = &D_8009EF64;
                break;
            case 0x70:
                handler = &D_8009EFFC;
                break;
            case 0x170:
                handler = &D_8009F0A0;
                break;
            case 0x156:
                handler = &D_8009F280;
                break;
            case 4:
                handler = &D_8009F504;
                break;
            case 0x44:
                handler = &D_8009F49C;
                break;
            case 5:
                handler = &D_8009ED28;
                break;
            case 0x45:
                handler = &D_8009ECC0;
                break;
            case 0x40:
            case 0x60:
            case 0x160:
            case 0x4040:
            case 0x4060:
            case 0x4160:
                handler = &D_8009FC44;
                break;
            default:
                handler = &D_800105AC;
                break;
            case 0:
            case 0x20:
            case 0x120:
            case 0x4000:
            case 0x4020:
            case 0x4120:
                handler = &D_8009FC90;
                break;
        }

        ws->field_20 = *stream;
        stream      += 2;
        ws->field_18 = ((u16*)stream)[0];
        ws->field_1C = ((u16*)stream)[1];
        stream       = (u32*)((u8*)stream + 4);
        stream       = ((ModelStreamHandler)handler)(ws, 0, stream);
        id           = *stream;

        while (1) {
            if (id != -2U) {
                break;
            }
            stream++;
        read_id:
            id = *stream;
            if (id == -1U) {
                goto done;
            }
        }
    }
done:
    *(void**)G_SCRATCH_HEAD = (u8*)*(void**)G_SCRATCH_HEAD + 0x88;
}

INCLUDE_ASM("main/nonmatchings/tmd", Tmd_Create);

void Tmd_SetupDraw(TmdObject* arg0)
{
    u8                         buf[0x1000];
    void**                     scratch;
    register void*             tmp asm("t1");
    register ScratchDrawBlock* ws asm("t0");
    register void*             stream asm("a2");
    register MATRIX*           colorMtx asm("t2");
    register short             t4 asm("t4");
    register short             t5 asm("t5");
    register short             t6 asm("t6");
    register u32               flags asm("a1");
    void*                      bufptr;
    s32                        disp;

    scratch = (void**)G_SCRATCH_HEAD;
    {
        register u32        ds_hi asm("v1");
        register TmdSource* p asm("v0");
        register s32        d asm("v0");

        __asm__("lui %0, %%hi(Display_State)" : "=r"(ds_hi));
        p      = arg0->field_10;
        tmp    = *scratch;
        stream = p->field_20;
        __asm__ volatile("lbu %0, %%lo(Display_State+0x128)(%1)" : "=r"(d) : "r"(ds_hi));
        disp         = d;
        ws           = (ScratchDrawBlock*)((u8*)tmp - 0x98);
        ws->field_80 = arg0;
        ws->field_84 = disp;
    }
    bufptr      = arg0->field_18;
    ws->field_0 = bufptr;
    *scratch    = ws;
    if (arg0->field_14 != 0) {
        ws->field_0 = (u8*)bufptr + arg0->field_16;
    }
    ws->field_4     = ws->field_0;
    ws->field_0     = (u8*)ws->field_0 + arg0->field_10->field_8;
    arg0->field_14 ^= 1;
    ws->field_8     = arg0->field_10->field_14;
    {
        register u_long*    ot asm("a0");
        register TmdSource* p asm("v1");
        register s32        e asm("v0");
        register void*      b asm("v1");
        s32                 field18;

        __asm__ volatile("" ::: "memory");
        ot           = D_800710A0;
        p            = arg0->field_10;
        field18      = p->field_18;
        ws->field_14 = ot;
        ws->field_C  = field18;
        e            = arg0->field_E;
        b            = buf;
        ws->field_10 = b;
        ws->field_14 = ot + e;
    }

    colorMtx = (MATRIX*)arg0->field_20;
    gte_SetColorMatrix(colorMtx);
    {
        register MATRIX* m asm("v0");
        register s32     r asm("t3");
        register s32     g asm("t7");
        register s32     b asm("t2");
        m = colorMtx;
        r = m->t[0];
        g = m->t[1];
        b = m->t[2];
        gte_ldbkdir(r, g, b);
    }

    {
        register MATRIX* m asm("v0");
        register MATRIX* src asm("t3");
        register MATRIX* light asm("t7");

        m = (MATRIX*)((u8*)tmp - 0x48);
        __asm__ volatile("" : "+r"(m));
        src = &D_80070F34;
        __asm__ volatile("" : "+r"(src));
        flags = arg0->field_C;
        __asm__ volatile("" : "+r"(flags));

        t4         = src->m[0][0];
        t5         = src->m[1][0];
        t6         = src->m[2][0];
        m->m[0][0] = t4;
        m->m[0][1] = t5;
        m->m[0][2] = t6;

        t4         = src->m[0][1];
        t5         = src->m[1][1];
        t6         = src->m[2][1];
        m->m[1][0] = t4;
        m->m[1][1] = t5;
        m->m[1][2] = t6;

        t4         = src->m[0][2];
        t5         = src->m[1][2];
        t6         = src->m[2][2];
        m->m[2][0] = t4;
        m->m[2][1] = t5;
        m->m[2][2] = t6;

        light = (MATRIX*)arg0->field_1C;
        gte_SetRotMatrix(light);

        gte_ldclmv(m);
        gte_rtir_real();
        gte_stclmv(m);

        m = (MATRIX*)((u8*)tmp - 0x46);
        gte_ldclmv(m);
        gte_rtir_real();
        gte_stclmv(m);

        m = (MATRIX*)((u8*)tmp - 0x44);
        gte_ldclmv(m);
        gte_rtir_real();
        gte_stclmv(m);
    }

    func_80010848(ws, flags, stream, arg0);

    *scratch = (u8*)*scratch + 0x98;
}

void Tmd_FreeBuffers(TmdObject* arg0)
{
    if (arg0->field_18 != NULL) {
        Mem_Free2(arg0->field_18, 1);
        arg0->field_18 = NULL;
    }
}

s32 Tmd_AllocBuffers(TmdObject* arg0)
{
    s32   result;
    void* mem;

    result = 0;
    if (arg0->field_18 == NULL) {
        mem            = Mem_Calloc(arg0->field_10->field_4 * 2, 1);
        arg0->field_18 = mem;
        if (mem != NULL) {
            arg0->field_14 = 0;
            Tmd_ProcessStream(arg0);
            Tmd_ProcessStream(arg0);
            result = 1;
        }
    }
    return result;
}

s32 Tmd_SumBufferBytes(void)
{
    TmdObject* node;
    s32        result;

    result = 0;
    node   = Tmd_List.next;
    while (node != NULL) {
        if (node->field_18 != NULL) {
            result += node->field_10->field_4 * 2;
        }
        node = node->next;
    }
    return result;
}

void Tmd_RewriteOpcodes(TmdSource* arg0)
{
    u32* stream;
    u32  id;
    u32  dims;
    u32  lo;
    u32  stop;

    stream = arg0->field_20;
    if (*stream != -1U) {
        stop = -2;
        do {
            if (*stream != stop) {
                do {
                    id = *stream;
                    if (id == 0x3B) {
                        goto case_advance;
                    }
                    if (id < 0x3CU) {
                        if (id == 0x38) {
                            goto case_38;
                        }
                        goto default_advance;
                    }
                    if (id == 0x79) {
                        goto case_advance;
                    }
                    if (id >= 0x7AU) {
                        goto case_advance;
                    }
                    if (id == 0x78) {
                        goto case_78;
                    }
                    goto default_advance;

                case_38:
                    *stream = 0x4038;
                    goto case_advance;
                case_78:
                    *stream = 0x4078;
                case_advance:
                    stream += 2;
                    goto after;
                default_advance:
                    stream += 2;
                after:
                    dims = *stream;
                    lo   = dims & 0xFFFF;
                    stream++;
                    stream += (dims >> 16) * lo;
                } while (*stream != -2U);
            }
            stream++;
        } while (*stream != -1U);
    }
}

void Tmd_FlagAllNodes(TmdObject* arg0)
{
    TmdObject* node;

    node = Tmd_List.next;
    while (node != NULL) {
        node->field_C |= 0x80;
        node           = node->next;
    }
    arg0->field_30++;
}

void Tmd_FreeNodeBuffers(TmdObject* arg0)
{
    TmdObject* node;

    node = Tmd_List.next;
    while (node != NULL) {
        if (node->field_18 != NULL) {
            Mem_Free2(node->field_18, 1);
            node->field_18 = NULL;
        }
        node = node->next;
    }
    arg0->field_30++;
}

void Tmd_DispatchTask(Task* arg0)
{
    TaskFuncTable3 sp;

    sp = D_80013EDC;
    sp.funcs[arg0->field_30](arg0);
}

void Gpu_ResetGraphAndOt(void)
{
    TmdObject* node;

    node = Tmd_List.next;
    ResetGraph(1);
    F179D4_ClearOTag(0);
    F179D4_ClearOTag(1);
    while (node != NULL) {
        if (node->field_18 != NULL) {
            node->field_18 = NULL;
        }
        node = node->next;
    }
}

void Tmd_AllocMissingBuffers(void)
{
    TmdObject* node;
    void*      mem;

    node = Tmd_List.next;
    Mem_InitAux();
    CdCmd_SetupMdecBuffers();
    while (node != NULL) {
        if (node->field_18 == NULL) {
            if (!(node->field_C & 4)) {
                mem = Mem_Calloc(node->field_10->field_4 * 2, 1);
                if (mem != NULL) {
                    node->field_18 = mem;
                    node->field_14 = 0;
                    Tmd_ProcessStream(node);
                    Tmd_ProcessStream(node);
                }
            }
        }
        node = node->next;
    }
}

void Tmd_AllocNodeBuffers(Task* arg0)
{
    TmdObject* node;
    void*      mem;

    node = Tmd_List.next;
    while (node != NULL) {
        if (node->field_18 == NULL) {
            mem = Mem_Calloc(node->field_10->field_4 * 2, 1);
            if (mem != NULL) {
                node->field_18 = mem;
                node->field_14 = 0;
                node->field_C &= ~0x80;
                Tmd_ProcessStream(node);
                Tmd_ProcessStream(node);
            }
        }
        node = node->next;
    }
    Task_Kill(arg0);
}

void Tmd_DrawFlaggedNodes(TmdObject* node)
{
    while (node != NULL) {
        if (node->field_C & 8) {
            if (node->field_18 != NULL) {
                Tmd_SetupDraw(node);
            }
        }
        node = node->next;
    }
}

void func_80042058(TmdObject* node)
{
    while (node != NULL) {
        if (!(node->field_C & 0x80)) {
            if (node->field_18 != NULL) {
                Tmd_SetupDraw(node);
            }
        }
        node = node->next;
    }
}
