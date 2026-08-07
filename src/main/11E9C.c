#include "common.h"

#include "main/display.h"
#include "main/fs.h"
#include "main/mem.h"
#include "main/unknown_syms.h"

s32 Fade_StepIn(s32 arg0)
{
    TILE*        p;
    DR_TPAGE*    dr;
    u8           color;
    RECT         rect;
    s16          cur;
    u16          next;
    register s32 ret asm("v0");
    register s32 w asm("t7");

    w          = 0x140;
    color      = *(volatile u8*)&D_8006ACB4;
    p          = (TILE*)D_80070EE0;
    D_80070EE0 = (u8*)(p + 1);
    setlen(p, 3);
    setcode(p, 0x62);
    p->r0 = color;
    p->g0 = color;
    p->b0 = color;
    p->x0 = -0xA0;
    p->y0 = -0x78;
    p->w  = w;
    p->h  = 0xF0;
    addPrim(D_800710A0 - 0x10, p);

    dr         = (DR_TPAGE*)D_80070EE0;
    D_80070EE0 = (u8*)(dr + 1);
    setDrawTPage(dr, 0, 1, 0x40);
    addPrim(D_800710A0 - 0x10, dr);

    cur  = *(s16*)&D_8006ACB4;
    next = D_8006ACB4;
    if (cur < 0x101) {
        D_8006ACB4 = next + arg0;
        ret        = 0;
    } else {
        rect.y = 0;
        rect.x = 0;
        rect.w = w;
        rect.h = 0xF0;
        ClearImage(&rect, 0, 0, 0);
        rect.y = 0x110;
        ClearImage(&rect, 0, 0, 0);
        Display_State.field_100 = 0;
        ret                     = 1;
    }
    return ret;
}

void Fade_StartWhite(void)
{
    TILE*     p;
    DR_TPAGE* dr;
    u8        color;

    func_8003DB48(0xD010);
    SetDispMask(1);

    D_8006ACB4 = 0xFF;
    color      = *(volatile u8*)&D_8006ACB4;
    p          = (TILE*)D_80070EE0;
    D_80070EE0 = (u8*)(p + 1);
    setlen(p, 3);
    setcode(p, 0x62);
    p->r0 = color;
    p->g0 = color;
    p->b0 = color;
    p->x0 = -0xA0;
    p->y0 = -0x78;
    p->w  = 0x140;
    p->h  = 0xF0;
    addPrim(D_800710A0 - 0x10, p);

    dr         = (DR_TPAGE*)D_80070EE0;
    D_80070EE0 = (u8*)(dr + 1);
    setDrawTPage(dr, 0, 1, 0x40);
    addPrim(D_800710A0 - 0x10, dr);
}

s32 Fade_StepOut(s32 arg0)
{
    TILE*     p;
    DR_TPAGE* dr;
    u8        color;
    s16       val;

    Display_State.field_100 = 1;
    color                   = *(volatile u8*)&D_8006ACB4;
    p                       = (TILE*)D_80070EE0;
    D_80070EE0              = (u8*)(p + 1);
    setlen(p, 3);
    setcode(p, 0x62);
    p->r0 = color;
    p->g0 = color;
    p->b0 = color;
    p->x0 = -0xA0;
    p->y0 = -0x78;
    p->w  = 0x140;
    p->h  = 0xF0;
    addPrim(D_800710A0 - 0x10, p);

    dr         = (DR_TPAGE*)D_80070EE0;
    D_80070EE0 = (u8*)(dr + 1);
    setDrawTPage(dr, 0, 1, 0x40);
    addPrim(D_800710A0 - 0x10, dr);

    val        = D_8006ACB4 - arg0;
    D_8006ACB4 = val;
    return val < 0;
}

void func_80021A20(u8* arg0)
{
    switch (D5B498_8006ACB8.field_2) {
        case 1:
            D5B498_8006ACB0 = D_80062DB0;
            *arg0           = 5;
            break;
        case 5:
            D5B498_8006ACB0 = D_800629B0;
            D5B498_8006ACAC = D_800629D0;
            *arg0           = 3;
            break;
        case 2:
        case 3:
        case 4:
        case 6:
        case 7:
        case 8:
        case 9:
        case 10:
        case 11:
        case 12:
        case 13:
        case 14:
        case 15:
        case 16:
        case 17:
        case 18:
        default:
            D5B498_8006ACB0 = D_80062DB0;
            *arg0           = 6;
            break;
        case 19:
            if (func_8004ACAC(0x7A) == 0) {
                D5B498_8006ACB0 = D_80062D08;
                *arg0           = 7;
            } else {
                D5B498_8006ACB0 = D_80062E04;
                *arg0           = 8;
            }
            break;
        case 20:
            D5B498_8006ACB0 = D_80062934;
            D5B498_8006ACAC = D_80062954;
            *arg0           = 3;
            break;
    }
}

void func_80021B28(u8* arg0)
{
    switch (D5B498_8006ACB8.field_2) {
        case 1:
            if (D5B498_8006ACC0 == 0) {
                D5B498_8006ACB0 = D_80062E50;
                *arg0           = 0xA;
            } else {
                D5B498_8006ACB0 = D_80062A24;
                D5B498_8006ACAC = D_80062A44;
                *arg0           = 3;
            }
            break;
        case 2:
            D5B498_8006ACB0 = D_80062E50;
            *arg0           = 0xA;
            break;
        case 27:
            D5B498_8006ACB0 = D_80062E50;
            *arg0           = 0xB;
            break;
        case 30:
            D5B498_8006ACB0 = D_80062E50;
            *arg0           = 4;
            break;
        case 3:
        case 4:
        case 5:
        case 6:
        case 7:
        case 8:
        case 9:
        case 10:
        case 11:
        case 12:
        case 13:
        case 14:
        case 15:
        case 16:
        case 17:
        case 18:
        case 19:
        case 20:
        case 21:
        case 22:
        case 23:
        case 24:
        case 25:
        case 26:
        case 28:
        case 29:
        case 31:
        case 32:
        case 33:
        case 34:
        case 35:
        case 36:
        case 37:
        case 38:
        default:
            D5B498_8006ACB0 = D_80062E50;
            *arg0           = 0x27;
            break;
    }
}

void func_80021C0C(u8* arg0)
{
    s32 temp_v1;
    s32 field2;
    s32 val;

    temp_v1 = func_8004ACAC(0x7A);
    field2  = D5B498_8006ACB8.field_2;

    if (field2 == 0x1B) {
        goto case_1B;
    }
    if (field2 < 0x1C) {
        if (field2 != 0x11) {
            goto case_default;
        }
    } else {
        goto case_default;
    }

    val = temp_v1 & 0xFFFF;
    if (val == 4) {
        goto case_11_4;
    }
    if (val < 5) {
        goto case_11_def;
    }
    if (val == 5) {
        goto case_11_5;
    }
case_11_def:
    D5B498_8006ACB0 = D_80062E50;
    *arg0           = 0xC;
    return;
case_11_4:
    D5B498_8006ACB0 = D_80062F80;
    *arg0           = 0x13;
    return;
case_11_5:
    D5B498_8006ACB0 = D_80062F80;
    *arg0           = 0x1A;
    return;

case_1B:
    val = temp_v1 & 0xFFFF;
    if (val == 4) {
        goto case_1B_4;
    }
    if (val < 5) {
        goto case_1B_def;
    }
    if (val == 5) {
        goto case_1B_5;
    }
case_1B_def:
    D5B498_8006ACB0 = D_80062E50;
    *arg0           = 0xD;
    return;
case_1B_4:
    D5B498_8006ACB0 = D_80062F80;
    *arg0           = 0x14;
    return;
case_1B_5:
    D5B498_8006ACB0 = D_80062F80;
    *arg0           = field2;
    return;

case_default:
    val = temp_v1 & 0xFFFF;
    if (val == 4) {
        goto case_def_4;
    }
    if (val < 5) {
        goto case_def_def;
    }
    if (val == 5) {
        goto case_def_5;
    }
case_def_def:
    D5B498_8006ACB0 = D_80062E50;
    *arg0           = 0xE;
    return;
case_def_4:
    D5B498_8006ACB0 = D_80062F80;
    *arg0           = 0xF;
    return;
case_def_5:
    D5B498_8006ACB0 = D_80062F80;
    *arg0           = 0x20;
}

void func_80021D8C(u8* arg0)
{
    s32 temp_v1;
    s32 val;

    temp_v1 = func_8004ACAC(0x7A);
    switch (D5B498_8006ACB8.field_2) {
        case 1:
            D5B498_8006ACB0 = D_80062AB4;
            D5B498_8006ACAC = D_80062AD4;
            *arg0           = 3;
            break;
        case 6:
            val = temp_v1 & 0xFFFF;
            if (val == 4) {
                goto case_6_4;
            }
            if (val == 5) {
                goto case_6_5;
            }
        case_6_4:
            D5B498_8006ACB0 = D_80062EA0;
            *arg0           = 0x10;
            return;
        case_6_5:
            D5B498_8006ACB0 = D_80062EA0;
            *arg0           = 0x17;
            return;
        case 16:
            val = temp_v1 & 0xFFFF;
            if (val == 5) {
                goto case_16_5;
            }
            if (val < 6) {
                goto case_16_def;
            }
            if (val == 6) {
                goto case_16_6;
            }
        case_16_def:
            D5B498_8006ACB0 = D_80062EEC;
            *arg0           = 0x11;
            return;
        case_16_5:
            D5B498_8006ACB0 = D_80062EEC;
            *arg0           = 0x18;
            return;
        case_16_6:
            D5B498_8006ACB0 = D_80062EEC;
            *arg0           = 0x24;
            return;
        case 20:
            val = temp_v1 & 0xFFFF;
            if (val == 5) {
                goto case_20_5;
            }
            if (val < 6) {
                goto case_20_def;
            }
            if (val == 6) {
                goto case_20_6;
            }
        case_20_def:
            D5B498_8006ACB0 = D_80062EEC;
            *arg0           = 0x28;
            return;
        case_20_5:
            D5B498_8006ACB0 = D_80062EEC;
            *arg0           = 0x1F;
            return;
        case_20_6:
            D5B498_8006ACB0 = D_80062EEC;
            *arg0           = 0x26;
            return;
        case 36:
            if (D5B498_8006ACC0 == 0) {
                D5B498_8006ACB0 = D_80062C20;
                D5B498_8006ACAC = D_80062C40;
                *arg0           = 3;
            } else {
                D5B498_8006ACB0 = D_80062C94;
                D5B498_8006ACAC = D_80062CB4;
                *arg0           = 3;
            }
            break;
        case 41:
            val = temp_v1 & 0xFFFF;
            if (val == 4) {
                goto case_41_4;
            }
            if (val == 5) {
                goto case_41_5;
            }
        case_41_4:
            D5B498_8006ACB0 = D_80062F34;
            *arg0           = 0x12;
            return;
        case_41_5:
            D5B498_8006ACB0 = D_80062F34;
            *arg0           = 0x19;
            return;
        case 31:
            val = temp_v1 & 0xFFFF;
            if (val == 5) {
                goto case_31_5;
            }
            if (val < 6) {
                goto case_31_def;
            }
            if (val == 6) {
                goto case_31_6;
            }
        case_31_def:
            D5B498_8006ACB0 = D_80062FD0;
            *arg0           = 0x15;
            return;
        case_31_5:
            D5B498_8006ACB0 = D_80062FD0;
            *arg0           = 0x1C;
            return;
        case_31_6:
            D5B498_8006ACB0 = D_80062FD0;
            *arg0           = 0x25;
            return;
        case 2:
        case 3:
        case 4:
        case 5:
        case 7:
        case 8:
        case 9:
        case 10:
        case 11:
        case 12:
        case 13:
        case 14:
        case 15:
        case 17:
        case 18:
        case 19:
        case 21:
        case 22:
        case 23:
        case 24:
        case 25:
        case 26:
        case 27:
        case 28:
        case 29:
        case 30:
        case 32:
        case 33:
        case 34:
        case 35:
        case 37:
        case 38:
        case 39:
        case 40:
        case 42:
        case 43:
        case 44:
        case 45:
        case 46:
        case 47:
        default:
            val = temp_v1 & 0xFFFF;
            if (val == 4) {
                goto case_def_4;
            }
            if (val == 5) {
                goto case_def_5;
            }
        case_def_4:
            D5B498_8006ACB0 = D_80063018;
            *arg0           = 0x16;
            return;
        case_def_5:
            D5B498_8006ACB0 = D_80063018;
            *arg0           = 0x1D;
            return;
    }
}

/* Pad after 47-entry jtbl for func_80021D8C (original had trailing .word 0). */
static const s32 s_jtbl_pad_21D8C = 0;

void func_8002207C(void)
{
    u8    sp10[8];
    u8    sp18[8];
    RECT  rect;
    RECT* r;
    s32   field2;

    Mem_Set(D4CB64_ImgBuffers, 0, 0x25800);
    rect.y = 0;
    rect.x = 0;
    r      = &rect;
    r->w   = 0x140;
    r->h   = 0xF0;
    ClearImage(r, 0, 0, 0);
    r->y = 0x110;
    ClearImage(r, 0, 0, 0);
    Display_State.field_100 = 0;
    SetDispMask(0);
    sp10[3]         = 0;
    sp10[2]         = 0;
    D5B498_8006ACAC = NULL;
    switch (D5B498_8006ACB8.field_3) {
        case 1:
            func_80021A20(sp10);
            break;
        case 2:
            func_80021B28(sp10);
            break;
        case 3:
            func_80021C0C(sp10);
            break;
        case 4:
            func_80021D8C(sp10);
            break;
        case 5:
        default:
            func_8004ACAC(0x7A);
            field2 = D5B498_8006ACB8.field_2;
            if (field2 == 0x16) {
                goto case_16;
            }
            if (field2 < 0x17) {
                goto case_default;
            }
            if (field2 == 0x1A) {
                goto case_1a;
            }
        case_default:
            if (D5B498_8006ACC0 == 0) {
                D5B498_8006ACB0 = D_800630B0;
                sp10[0]         = 0x23;
            } else {
                D5B498_8006ACB0 = D_80062BA4;
                D5B498_8006ACAC = D_80062BC4;
                sp10[0]         = 3;
            }
            break;
        case_16:
            D5B498_8006ACB0 = D_80063068;
            sp10[0]         = 0x1E;
            break;
        case_1a:
            D5B498_8006ACB0 = D_80062B30;
            D5B498_8006ACAC = D_80062B50;
            sp10[0]         = 3;
            break;
    }
    sp18[0]         = 0;
    sp18[1]         = 0;
    sp18[2]         = 0;
    sp18[3]         = 0;
    D5B498_8006AC98 = CdCmd_Enqueue(0x21, sp10, sp18);
}

/* Alignment pad after the 5-entry func_8002207C jump table. */
static const s32 s_jtbl_pad_2207C = 0;

void func_8002226C(void* arg0, void* arg1)
{
    void* secondary;
    s32   ret;
    s32   temp;

    arg1      = &CdCmd_Queue;
    secondary = NULL;
    switch (*(s16*)&D5B498_8006AC9C) {
        case 0:
            D_8006ACA0 = 0;
            D_8006AC9F = 0;
            D_8006AC9E = 0;
            D_8006ACA4 = 0;
            D_8006ACA2 = 0;
            D_8006ACA8 = 0;
            D_8006ACA6 = 0;
            Fade_StartWhite();
            D5B498_8006AC9C++;
            /* fallthrough */
        case 1:
            if ((Fade_StepOut(0x10) & 0xFFFF) != 0) {
                D5B498_8006AC9C++;
            }
            break;
        case 2:
            if (D_8006ACA6 < 0) {
                D_8006ACA2 = 1;
            }
            temp = TextStream_Draw(arg0, &D_8006AC9E, &D_8006ACA2, 0);
            if (D_8006ACA6 >= 0) {
                D_8006ACA6 = temp;
            }
            if (D_8006ACA0 >= 0x3D) {
                if (D_8006ACA8 < 0) {
                    D_8006ACA4 = 1;
                }
                if (secondary != NULL) {
                    ret = TextStream_Draw(secondary, &D_8006AC9F, &D_8006ACA4, 0);
                } else {
                    D_8006ACA8 = -1;
                }
                if (D_8006ACA8 >= 0) {
                    D_8006ACA8 = ret;
                } else {
                    goto check_done;
                }
            } else {
                D_8006ACA0++;
            }
            if (D_8006ACA8 >= 0) {
                break;
            }
        check_done:
            if (D_8006ACA6 >= 0) {
                break;
            }
            D_8006ACA0 = 0;
            D5B498_8006AC9C++;
            break;
        case 3:
            if (D_8006ACA0 >= 0x3C) {
                if (((CdCmdQueue*)arg1)->field_22E != 0) {
                    goto draw;
                }
                D_8006ACB4 = 0;
                D5B498_8006AC9C++;
            } else {
                D_8006ACA0++;
            }
            goto draw;
        case 4:
            if ((Fade_StepIn(0x10) & 0xFFFF) != 0) {
                D5B498_8006AC9A       = 0;
                CdCmd_Queue.field_224 = 0;
            }
        draw:
            D_8006ACA4 = 1;
            D_8006ACA2 = 1;
            TextStream_Draw(arg0, &D_8006AC9E, &D_8006ACA2, 0);
            if (secondary != NULL) {
                TextStream_Draw(secondary, &D_8006AC9F, &D_8006ACA4, 0);
            }
            break;
    }
}
