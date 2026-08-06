#include "common.h"

#include <psyq/libmcrd.h>

#include "main/unknown_syms.h"

void func_8002DEC4(void)
{
}

s32 func_8002DECC(GStruct38* arg0, u8* arg1, u8* arg2)
{
    register GStruct38* ctx asm("t5");
    register s32        width asm("t0");
    GStruct68*          glyph;
    s32                 c;
    s32                 prev9;
    s32                 nl;
    s32                 end_flag;
    s32                 ch;
    s32                 bs;
    s32                 idx;

    ctx   = arg0;
    width = 0;
    glyph = (GStruct68*)arg2;
    asm("" : "+r"(ctx), "+r"(width), "+r"(glyph));
    c = *arg1;
    if (c == 0) {
        goto end;
    }
    prev9 = width;
    nl    = 0xA;

    do {
        if ((c & 0xFF) == nl) {
            goto end;
        }
        end_flag = 0;
        if ((c & 0xFF) == 0x5C) {
            bs    = 0x5C;
            arg1 += 1;
            do {
                ch = *arg1;
                if ((u32)(ch - 0x42) < 0x36U) {
                    switch (ch) {
                        case 0x42:
                        case 0x43:
                        case 0x44:
                        case 0x53:
                        case 0x55:
                        case 0x57:
                        case 0x62:
                        case 0x63:
                        case 0x64:
                        case 0x73:
                        case 0x75:
                        case 0x77:
                            arg1 += 2;
                            break;
                        case 0x4E:
                        case 0x6E:
                            end_flag = 1;
                        default:
                            break;
                    }
                }
                ch = *arg1;
                if (ch != 0 && ch != nl) {
                    goto check_bs;
                }
                end_flag = 1;
                asm("" ::: "memory");
                ch = *arg1;
            check_bs:
                arg1 += 1;
                if (ch != bs) {
                    arg1 -= 1;
                    break;
                }
            } while (1);
        }
        if (end_flag != 0) {
            goto end;
        }
        idx   = *arg1;
        idx  -= 0x20;
        glyph = (GStruct68*)(arg2 + idx * 0xC);
        ch    = prev9;
        ch   += glyph->field_8;
        ch   += 1;
        if ((u8)ch >= 3U) {
            if (ctx->field_C == 5) {
                width -= 1;
            } else {
                width -= 2;
            }
        }
        arg1  += 1;
        prev9  = glyph->field_9;
        c      = *arg1;
        width += glyph->w + (s8)glyph->field_6 + (s8)glyph->off_x;
    } while (c != 0);

end:
    return width - (s8)glyph->field_6;
}

INCLUDE_ASM("main/nonmatchings/1E6C4", func_8002E010);

void func_8002E188(GStruct38* arg0, GStruct68* arg1, s32 arg2)
{
    SPRT* p;
    SPRT* p2;
    s32   temp;

    p             = (SPRT*)D_80071190;
    D_80071190    = (DR_TPAGE*)(p + 1);
    *(s32*)&p->r0 = arg2;
    setlen(p, 4);
    setcode(p, 0x64);

    p2         = (SPRT*)D_80071190;
    D_80071190 = (DR_TPAGE*)(p2 + 1);
    setlen(p2, 4);
    setcode(p2, 0x67);

    p2->x0 = p->x0 = arg0->field_0 + (s8)arg1->off_x;
    p2->y0 = p->y0 = (arg0->field_2 - arg1->h) + (s8)arg1->off_y;
    p2->u0 = p->u0 = arg1->u;
    p2->v0 = p->v0 = arg1->v + arg0->field_F;
    p2->w = p->w = arg1->w + 1;
    temp         = arg1->h;
    p2->h = p->h = temp + 1;
    p2->clut     = 0x7FFF;
    p->clut      = 0x7FFD;

    addPrim(D_800710A0 + arg0->field_4 + 1, p2);
    addPrim(D_800710A0 + arg0->field_4, p);
}

INCLUDE_ASM("main/nonmatchings/1E6C4", func_8002E300);

INCLUDE_ASM("main/nonmatchings/1E6C4", func_8002E53C);

INCLUDE_ASM("main/nonmatchings/1E6C4", func_8002EB94);

void func_8002EDFC(GStruct38* arg0, u8* arg1)
{
    u8* table;
    s32 width;

    switch (arg0->field_C) {
        case 0:
            table = D_8005EFB0;
            break;
        case 5:
            table = D_800604B0;
            break;
        default:
            table = D_8005FA30;
            break;
    }

    switch (arg0->field_D) {
        case 1:
            width          = func_8002DECC(arg0, arg1, table);
            arg0->field_0 -= width >> 1;
            break;
        case 2:
            width          = func_8002DECC(arg0, arg1, table);
            arg0->field_0 -= width;
            break;
    }
}

u8* func_8002F020(u8* arg0, s32 arg1);

u8* func_8002EEA0(u8* arg0, s32 arg1)
{
    typedef struct {
        u8 data[9];
    } Bytes9;
    typedef struct {
        u8 data[2];
    } Bytes2;

    u8* dest;
    u8* start;
    s32 place;
    s32 digit;
    s32 temp;
    s32 cmp;
    s32 sign;

    asm("" ::: "memory");
    sign = 0x2D;
    if (arg1 >= 0) {
        sign = 0x2B;
    }
    *arg0 = sign;

    place = 0x989680;
    start = arg0 + 1;
    if (arg1 < 0) {
        arg0[1] = 0x2D;
        func_8002F020(arg0 + 2, -arg1);
        return arg0;
    }
    if (arg1 > 0x5F5E0FF) {
        *(Bytes9*)(arg0 + 1) = *(Bytes9*)D_800138BC;
        return arg0;
    }
    cmp = arg1 < place;
    if (arg1 == 0) {
        *(Bytes2*)(arg0 + 1) = *(Bytes2*)D_800138C8;
        return arg0;
    }
    dest = start;
    if (cmp) {
        do {
            place /= 10;
        } while (arg1 < place);
    }
    if (place > 0) {
        do {
            digit  = arg1 / place;
            *dest  = digit;
            temp   = *dest & 0xFF;
            digit  = temp * place;
            place /= 10;
            *dest  = temp + 0x30;
            dest++;
            arg1 -= digit;
        } while (place > 0);
    }
    *dest = 0;
    return arg0;
}

u8* func_8002F020(u8* arg0, s32 arg1)
{
    typedef struct {
        u8 data[9];
    } Bytes9;
    typedef struct {
        u8 data[2];
    } Bytes2;

    u8* dest;
    s32 place;
    s32 digit;
    s32 temp;
    s32 cmp;

    place = 0x989680;
    if (arg1 < 0) {
        *arg0 = 0x2D;
        func_8002F020(arg0 + 1, -arg1);
        return arg0;
    }
    if (arg1 > 0x5F5E0FF) {
        *(Bytes9*)arg0 = *(Bytes9*)D_800138BC;
        return arg0;
    }
    cmp = arg1 < place;
    if (arg1 == 0) {
        *(Bytes2*)arg0 = *(Bytes2*)D_800138C8;
        return arg0;
    }
    dest = arg0;
    if (cmp) {
        do {
            place /= 10;
        } while (arg1 < place);
    }
    if (place > 0) {
        do {
            digit  = arg1 / place;
            *dest  = digit;
            temp   = *dest & 0xFF;
            digit  = temp * place;
            place /= 10;
            *dest  = temp + 0x30;
            dest++;
            arg1 -= digit;
        } while (place > 0);
    }
    *dest = 0;
    return arg0;
}

u8* func_8002F18C(u8* arg0, u32 arg1)
{
    typedef struct {
        u8 data[10];
    } Bytes10;
    typedef struct {
        u8 data[2];
    } Bytes2;

    register u8* dest asm("a3");
    register u32 place asm("a2");
    register u32 digit asm("v1");
    u32          temp;
    u8*          ret;
    u32          cmp;

    place = 0x5F5E100;
    if (arg1 > 0x3B9AC9FEU) {
        *(Bytes10*)arg0 = *(Bytes10*)D_800138CC;
        return arg0;
    }
    cmp = arg1 < place;
    if (arg1 == 0) {
        *(Bytes2*)arg0 = *(Bytes2*)D_800138C8;
        return arg0;
    }
    dest = arg0;
    if (cmp) {
        do {
            place /= 10;
        } while (arg1 < place);
    }
    if (place != 0) {
        do {
            digit  = arg1 / place;
            *dest  = digit;
            temp   = *dest & 0xFF;
            digit  = temp * place;
            place /= 10;
            *dest  = temp + 0x30;
            dest++;
            arg1 -= digit;
        } while (place != 0);
    }
    *dest = 0;
    ret   = arg0;
    asm("" : "+r"(ret));
    return ret;
}

u8* func_8002F2A4(u8* arg0, s32 arg1)
{
    register u8* dest asm("a2");
    register s32 place asm("a0");
    s32          digit;
    s32          temp;
    s32          prod;
    u8*          ret;

    place = 0x10000000;
    if (arg1 < 0) {
        *arg0 = 0x2D;
        func_8002F2A4(arg0 + 1, -arg1);
        return arg0;
    }
    if (arg1 == 0) {
        register s8* src asm("t2");
        register s32 c0 asm("a3");
        register s32 c1 asm("t0");
        register s32 hi asm("v0");
        do {
            asm volatile("lui %1, %%hi(D_800138C8)\n\t"
                         "addiu %0, %1, %%lo(D_800138C8)"
                         : "=r"(src), "=r"(hi));
            c0      = src[0];
            c1      = src[1];
            arg0[0] = c0;
            arg0[1] = c1;
        } while (0);
        return arg0;
    }
    dest = arg0;
    if (arg1 < place) {
        do {
            place >>= 4;
        } while (arg1 < place);
    }
    if (place > 0) {
        do {
            digit = arg1 / place;
            *dest = digit;
            asm("");
            temp    = digit & 0xFF;
            prod    = temp * place;
            place >>= 4;
            arg1   -= prod;
            if (temp >= 10U) {
                *dest = temp + 0x37;
            } else {
                *dest = temp + 0x30;
            }
            dest++;
        } while (place > 0);
    }
    *dest = 0;
    ret   = arg0;
    asm("" : "+r"(ret));
    return ret;
}

u8* func_8002F3A0(u8* arg0, u32 arg1)
{
    register u8* dest asm("a3");
    register u32 place asm("a2");
    u32          digit;
    u32          temp;
    s32          prod;
    u8*          ret;

    place = 0x10000000;
    if (arg1 == 0) {
        register s8* src asm("t3");
        register s32 c0 asm("t0");
        register s32 c1 asm("t1");
        register s32 hi asm("v0");
        do {
            asm volatile("lui %1, %%hi(D_800138C8)\n\t"
                         "addiu %0, %1, %%lo(D_800138C8)"
                         : "=r"(src), "=r"(hi));
            c0      = src[0];
            c1      = src[1];
            arg0[0] = c0;
            arg0[1] = c1;
        } while (0);
        return arg0;
    }
    dest = arg0;
    if (arg1 < place) {
        do {
            place >>= 4;
        } while (arg1 < place);
    }
    if (place != 0) {
        do {
            digit = arg1 / place;
            *dest = digit;
            asm("");
            temp    = digit & 0xFF;
            prod    = temp * place;
            place >>= 4;
            arg1   -= prod;
            if (temp >= 10U) {
                *dest = temp + 0x37;
            } else {
                *dest = temp + 0x30;
            }
            dest++;
        } while (place != 0);
    }
    *dest = 0;
    ret   = arg0;
    asm("" : "+r"(ret));
    return ret;
}

INCLUDE_ASM("main/nonmatchings/1E6C4", func_8002F44C);

u8* func_8002F528(u8* arg0, s32 arg1)
{
    u8 temp;

    if (arg1 > 0) {
        s32 c_nl = 0xA;
        s32 c_N  = 0x4E;
        s32 c_n  = 0x6E;
        s32 c_bs = 0x5C;
    loop:
        temp = *arg0;
        if (temp == 0) {
            goto end;
        }
        if (temp == c_nl) {
            arg1 -= 1;
        } else if (temp == c_N || temp == c_n) {
            if (arg0[-1] == c_bs) {
                arg1 -= 1;
            }
        }
        arg0 += 1;
        if (arg1 > 0) {
            goto loop;
        }
    }
end:
    return arg0;
}

u8* func_8002F588(u8* dest, u8* src)
{
    u8 c;

    if (*dest != 0) {
        while (*++dest != 0) {
        }
    }

    c = *src;
    if (c != 0) {
        do {
            src++;
            *dest = c;
            c     = *src;
            dest++;
        } while (c != 0);
    }

    *dest = 0;
    return dest;
}

void func_8002F5E4(GStruct38* arg0, GStruct68* arg1, s32 arg2)
{
    SPRT* p;
    s32   temp;

    p = &D_80071710;
    setlen(p, 4);
    *(s32*)&p->r0 = arg2;
    setcode(p, 0x64);
    p->x0   = arg0->field_0 + (s8)arg1->off_x;
    p->y0   = (arg0->field_2 - arg1->h) + (s8)arg1->off_y;
    p->u0   = arg1->u;
    p->v0   = arg1->v + arg0->field_F;
    p->w    = arg1->w + 1;
    temp    = arg1->h;
    p->clut = 0x7FFD;
    p->h    = temp + 1;
    DrawPrim(p);
}

void func_8002F69C(GStruct38* arg0, GStruct68* arg1, s32 arg2)
{
    SPRT* p;
    s32   temp;

    p          = (SPRT*)D_80071190;
    D_80071190 = (DR_TPAGE*)(p + 1);
    setlen(p, 4);
    *(s32*)&p->r0 = arg2;
    setcode(p, 0x64);
    p->x0   = arg0->field_0 + (s8)arg1->off_x;
    p->y0   = (arg0->field_2 - arg1->h) + (s8)arg1->off_y;
    p->u0   = arg1->u;
    p->v0   = arg1->v + arg0->field_F;
    p->w    = arg1->w + 1;
    temp    = arg1->h;
    p->clut = 0x7FFD;
    p->h    = temp + 1;
    addPrim(D_800710A0 + arg0->field_4, p);
}

void func_8002F798(GStruct38* arg0, GStruct68* arg1)
{
    SPRT* p;
    s32   temp;

    p          = (SPRT*)D_80071190;
    D_80071190 = (DR_TPAGE*)(p + 1);
    setlen(p, 4);
    setcode(p, 0x67);
    p->x0   = arg0->field_0 + (s8)arg1->off_x;
    p->y0   = (arg0->field_2 - arg1->h) + (s8)arg1->off_y;
    p->u0   = arg1->u;
    p->v0   = arg1->v + arg0->field_F;
    p->w    = arg1->w + 1;
    temp    = arg1->h;
    p->clut = 0x7FFF;
    p->h    = temp + 1;
    addPrim(D_800710A0 + arg0->field_4, p);
}

void func_8002F890(Task* arg0)
{
    UiObject* obj;
    s16       temp;

    if (arg0->field_30 == 0) {
        D_80067694 = NULL;
        obj        = func_800486F0(D_800608F4, 1, 1, 2, 0);
        if (obj != NULL) {
            arg0->field_20 = obj;
            arg0->field_30 = arg0->field_30 + 1;
        }
    } else if (arg0->field_30 == 1) {
        obj = arg0->field_20;
        if (obj->field_2E == -1 || obj->field_2E == 6) {
            arg0->field_2a = 0xA;
            arg0->field_30 = arg0->field_30 + 1;
            func_80048838(obj, obj->field_28);
        }
    } else {
        temp           = arg0->field_2a - Display_State.field_10a;
        arg0->field_2a = temp;
        if (temp <= 0) {
            Task_Spawn(0, 2, 0xC, 0);
            Task_CallExit(arg0);
        }
    }
}

void func_8002F98C(Task* arg0)
{
    func_80030074();
    func_8003DB48(0x1010);
    func_8002BB9C();
    Task_SpawnFromTable((TaskDesc*)&D_80094C8C, 0, 0, 0);
    Task_Kill(arg0);
}
