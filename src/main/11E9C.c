#include "common.h"

#include "main/display.h"
#include "main/fs.h"
#include "main/mem.h"
#include "main/unknown_syms.h"

void func_80021D8C(u8* arg0);

INCLUDE_ASM("main/nonmatchings/11E9C", func_8002169C);

void func_80021808(void)
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

INCLUDE_ASM("main/nonmatchings/11E9C", func_8002191C);

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

/* Absolute copies of still-asm jump tables between matched jtbls. */
const s32 jtbl_80013188[48] = {
    0x80021DDC,
    0x80022028,
    0x80022028,
    0x80022028,
    0x80022028,
    0x80021E04,
    0x80022028,
    0x80022028,
    0x80022028,
    0x80022028,
    0x80022028,
    0x80022028,
    0x80022028,
    0x80022028,
    0x80022028,
    0x80021E4C,
    0x80022028,
    0x80022028,
    0x80022028,
    0x80021EB4,
    0x80022028,
    0x80022028,
    0x80022028,
    0x80022028,
    0x80022028,
    0x80022028,
    0x80022028,
    0x80022028,
    0x80022028,
    0x80022028,
    0x80021FC0,
    0x80022028,
    0x80022028,
    0x80022028,
    0x80022028,
    0x80021F1C,
    0x80022028,
    0x80022028,
    0x80022028,
    0x80022028,
    0x80021F78,
    0x80022028,
    0x80022028,
    0x80022028,
    0x80022028,
    0x80022028,
    0x80022028,
    0x00000000,
};

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

INCLUDE_ASM("main/nonmatchings/11E9C", func_80021D8C);

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

/* Alignment pad after the 5-entry func_8002207C jump table, then an
 * absolute copy of jtbl_80013260 for still-asm func_8002226C. */
static const s32 s_jtbl_pad_2207C = 0;
const s32        jtbl_80013260[5] = {
    0x800222C4,
    0x80022314,
    0x8002233C,
    0x80022444,
    0x80022490,
};

INCLUDE_ASM("main/nonmatchings/11E9C", func_8002226C);
