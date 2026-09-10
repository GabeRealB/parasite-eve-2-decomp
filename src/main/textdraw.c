#include "common.h"

#include "main/unknown_syms.h"
#include "main/text.h"
#include "main/title.h"
#include "main/ui.h"

void func_8002DEC4(void)
{
}

s32 Text_MeasureGlyphWidth(TextDrawReq* arg0, u8* arg1, u8* arg2)
{
    register TextDrawReq* ctx asm("t5");
    register s32          width asm("t0");
    FontGlyph*            glyph;
    s32                   c;
    s32                   prev9;
    s32                   nl;
    s32                   end_flag;
    s32                   ch;
    s32                   bs;
    s32                   idx;

    ctx   = arg0;
    width = 0;
    glyph = (FontGlyph*)arg2;
    SOFT_TOUCH_REG3(ctx, width, glyph);
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
                SOFT_COMPILER_BARRIER();
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
        glyph = (FontGlyph*)(arg2 + idx * 0xC);
        ch    = prev9;
        ch   += glyph->field_8;
        ch   += 1;
        if ((u8)ch >= 3U) {
            if (ctx->glyphTable == 5) {
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

void Text_DrawGlyphDualSprtA(TextDrawReq* arg0, FontGlyph* arg1, s32 arg2)
{
    SPRT* p;
    SPRT* p2;
    s32   temp;

    p              = (SPRT*)Gpu_PrimCursor;
    Gpu_PrimCursor = (DR_TPAGE*)(p + 1);
    *(s32*)&p->r0  = arg2;
    setlen(p, 4);
    setcode(p, 0x66);

    p2             = (SPRT*)Gpu_PrimCursor;
    Gpu_PrimCursor = (DR_TPAGE*)(p2 + 1);
    setlen(p2, 4);
    setcode(p2, 0x67);

    p2->x0 = p->x0 = arg0->x + (s8)arg1->off_x;
    p2->y0 = p->y0 = (arg0->y - arg1->h) + (s8)arg1->off_y;
    p2->u0 = p->u0 = arg1->u;
    p2->v0 = p->v0 = arg1->v + arg0->vBias;
    p2->w = p->w = arg1->w + 1;
    temp         = arg1->h;
    p2->h = p->h = temp + 1;
    p2->clut     = 0x7FFE;
    p->clut      = 0x7FFD;

    addPrim(Gpu_CurrentOt + arg0->otIndex + 1, p2);
    addPrim(Gpu_CurrentOt + arg0->otIndex, p);
}

void Text_DrawGlyphDualSprt(TextDrawReq* arg0, FontGlyph* arg1, s32 arg2)
{
    SPRT* p;
    SPRT* p2;
    s32   temp;

    p              = (SPRT*)Gpu_PrimCursor;
    Gpu_PrimCursor = (DR_TPAGE*)(p + 1);
    *(s32*)&p->r0  = arg2;
    setlen(p, 4);
    setcode(p, 0x64);

    p2             = (SPRT*)Gpu_PrimCursor;
    Gpu_PrimCursor = (DR_TPAGE*)(p2 + 1);
    setlen(p2, 4);
    setcode(p2, 0x67);

    p2->x0 = p->x0 = arg0->x + (s8)arg1->off_x;
    p2->y0 = p->y0 = (arg0->y - arg1->h) + (s8)arg1->off_y;
    p2->u0 = p->u0 = arg1->u;
    p2->v0 = p->v0 = arg1->v + arg0->vBias;
    p2->w = p->w = arg1->w + 1;
    temp         = arg1->h;
    p2->h = p->h = temp + 1;
    p2->clut     = 0x7FFF;
    p->clut      = 0x7FFD;

    addPrim(Gpu_CurrentOt + arg0->otIndex + 1, p2);
    addPrim(Gpu_CurrentOt + arg0->otIndex, p);
}

void Text_DrawGlyphDualSprtTpage(TextDrawReq* arg0, FontGlyph* arg1, s32 arg2)
{
    SPRT*     p;
    SPRT*     p2;
    DR_TPAGE* dr;
    s32       temp;

    p              = (SPRT*)Gpu_PrimCursor;
    Gpu_PrimCursor = (DR_TPAGE*)(p + 1);
    *(s32*)&p->r0  = arg2;
    setlen(p, 4);
    setcode(p, 0x64);

    p2             = (SPRT*)Gpu_PrimCursor;
    Gpu_PrimCursor = (DR_TPAGE*)(p2 + 1);
    setlen(p2, 4);
    setcode(p2, 0x67);

    p2->x0 = p->x0 = arg0->x + (s8)arg1->off_x;
    p2->y0 = p->y0 = (arg0->y - arg1->h) + (s8)arg1->off_y;
    p2->u0 = p->u0 = arg1->u;
    p2->v0 = p->v0 = arg1->v + arg0->vBias;
    p2->w = p->w = arg1->w + 1;
    temp         = arg1->h;
    p2->h = p->h = temp + 1;
    p2->clut     = 0x7FFF;
    p->clut      = 0x7FFD;

    addPrim(Gpu_CurrentOt + arg0->otIndex, p);
    dr             = Gpu_PrimCursor;
    Gpu_PrimCursor = dr + 1;
    setlen(dr, 1);
    dr->code[0] = 0xE100023F;
    addPrim(Gpu_CurrentOt + arg0->otIndex, dr);

    addPrim(Gpu_CurrentOt + arg0->otIndex, p2);
    dr             = Gpu_PrimCursor;
    Gpu_PrimCursor = dr + 1;
    setlen(dr, 1);
    dr->code[0] = 0xE100025F;
    addPrim(Gpu_CurrentOt + arg0->otIndex, dr);
}

void func_8002E53C(TextDrawReq* arg0, u8* arg1)
{
    TextDrawReq* ctx;
    u8*          ptr;
    u8*          table;
    FontGlyph*   glyph;
    void         (*draw)();
    s32          color;
    s32          prev9;
    s32          width;
    s32          ch;
    u8           c;
    s32          end_flag;
    s32          idx;
    s32          temp;
    DR_TPAGE*    dr;
    u32          tpage;

    ctx   = arg0;
    ptr   = arg1;
    prev9 = 0;
    SOFT_TOUCH_REG3(ctx, ptr, prev9);
    color      = ctx->field_8;
    ctx->vBias = 0;
    switch (ctx->glyphTable) {
        case 0:
            table      = Font_Glyphs0;
            ctx->vBias = 0x26;
            break;
        case 5:
            table      = Font_Glyphs2;
            ctx->vBias = 0;
            break;
        default:
            table             = Font_Glyphs1;
            *(s8*)&ctx->vBias = -128;
            break;
    }
    switch (ctx->centerMode) {
        case 1:
            width   = Text_MeasureGlyphWidth(ctx, arg1, table);
            ctx->x -= width >> 1;
            break;
        case 2:
            width   = Text_MeasureGlyphWidth(ctx, arg1, table);
            ctx->x -= width;
            break;
    }
    switch ((s32)ctx->field_E) {
        case 1:
            draw = Text_DrawGlyphDualSprt;
            break;
        case 2:
            draw = Text_DrawGlyphDualSprtTpage;
            break;
        case 3:
            draw = Text_DrawGlyphDualSprtA;
            break;
        case 4:
            draw = Text_DrawGlyphOt;
            break;
        case 16:
            dr = &D_80071728;
            setlen(dr, 1);
            dr->code[0] = 0xE100023F;
            DrawPrim(dr);
            draw = Text_DrawGlyphImmediate;
            break;
        case 0:
        default:
            draw = Text_DrawGlyphQueued;
            break;
    }
    while ((c = *ptr) != 0 && c != 10 && c != 13) {
        end_flag = 0;
        if (c == 0x5C) {
            do {
                ptr++;
                switch (*ptr) {
                    case 'C':
                    case 'c':
                        ptr++;
                        switch (*ptr) {
                            case 'W':
                            case 'w':
                                color = 0x606060;
                                break;
                            case 'Y':
                            case 'y':
                                color = 0x037A78;
                                break;
                            case 'O':
                            case 'o':
                                color = 0x0D287F;
                                break;
                            case 'G':
                            case 'g':
                                color = 0x01741F;
                                break;
                            case 'H':
                            case 'h':
                                color = 0x38443C;
                                break;
                            case 'C':
                            case 'c':
                                color = 0x808008;
                                break;
                            case 'R':
                            case 'r':
                                color = 0x001666;
                                break;
                        }
                        ptr++;
                        break;
                    case 'S':
                    case 's':
                        ptr++;
                        switch (*ptr) {
                            case 'S':
                            case 's':
                                table      = Font_Glyphs2;
                                ctx->vBias = 0;
                                break;
                            case 'M':
                            case 'm':
                                table      = Font_Glyphs0;
                                ctx->vBias = 0x26;
                                break;
                            case 'L':
                            case 'l':
                                table             = Font_Glyphs1;
                                *(s8*)&ctx->vBias = -128;
                                break;
                        }
                        ptr++;
                        break;
                    case 'W':
                    case 'w':
                        ptr++;
                        switch (*ptr) {
                            case '0':
                                ctx->field_E = 3;
                                draw         = Text_DrawGlyphDualSprtA;
                                break;
                            case '1':
                                ctx->field_E = 1;
                                draw         = Text_DrawGlyphDualSprt;
                                break;
                        }
                        ptr++;
                        break;
                    case 'U':
                    case 'u':
                        ptr++;
                        if ((u32)(*ptr - '0') < 10)
                            ctx->y -= *ptr - '0';
                        ptr++;
                        break;
                    case 'D':
                    case 'd':
                        ptr++;
                        if ((u32)(*ptr - '0') < 10)
                            ctx->y += *ptr - '0';
                        ptr++;
                        break;
                    case 'B':
                    case 'b':
                        ptr++;
                        if ((u32)(*ptr - '0') < 10) {
                            SOFT_COMPILER_BARRIER();
                            ctx->x = (*ptr - '0') * 8;
                        }
                        ptr++;
                        break;
                    case 'N':
                    case 'n':
                        end_flag = 1;
                        break;
                }
                ch = *ptr;
                if (ch != 0 && ch != 10 && ch != 13)
                    goto check_bs;
                end_flag = 1;
                SOFT_COMPILER_BARRIER();
                ch = *ptr;
            check_bs:
                if (ch != 0x5C)
                    break;
            } while (1);
        }
        if (end_flag != 0)
            break;
        if (*ptr < 0x20) {
            ptr++;
            continue;
        }
        idx   = *ptr - 0x20;
        glyph = (FontGlyph*)(table + idx * 12);
        temp  = prev9;
        temp += glyph->field_8;
        temp += 1;
        if ((u8)temp >= 3U) {
            if (ctx->glyphTable == 5)
                ctx->x -= 1;
            else
                ctx->x -= 2;
        }
        prev9 = glyph->field_9;
        draw(ctx, glyph, color);
        ptr++;
        ctx->x += glyph->w + (s8)glyph->field_6 + (s8)glyph->off_x;
        ctx->y += (s8)glyph->field_7;
    }
    if (ctx->field_E == 1 || ctx->field_E == 3) {
        dr             = Gpu_PrimCursor;
        Gpu_PrimCursor = dr + 1;
        dr->code[0]    = 0xE100025F;
        setlen(dr, 1);
        addPrim(Gpu_CurrentOt + ctx->otIndex + 1, dr);
    }
    if (ctx->field_E != 16) {
        tpage = 0xE1000000;
        if (ctx->field_E == 4) {
            SOFT_TOUCH_REG(tpage);
            tpage |= 0x25F;
        } else {
            SOFT_TOUCH_REG(tpage);
            tpage |= 0x23F;
        }
        dr             = Gpu_PrimCursor;
        Gpu_PrimCursor = dr + 1;
        dr->code[0]    = tpage;
        setlen(dr, 1);
        addPrim(Gpu_CurrentOt + ctx->otIndex, dr);
    }
}

u8* Text_FormatTime(u8* arg0, s32 arg1)
{
    typedef struct {
        u8 data[10];
    } Bytes10;
    typedef struct {
        u8 data[2];
    } Bytes2;

    u8* dest;
    u8* ret;
    u8* ptr;
    u32 place;
    s32 minutes;
    u32 mins_work;
    u32 quot;
    u32 digit;
    u32 temp;
    s32 i;
    s32 tmp;
    s32 lt10;

    ptr = arg0;
    SOFT_TOUCH_REG(ptr);
    if ((u32)(arg1 & 0xFFFF) > 0xEA5FU) {
        arg1 = 0xEA5F;
        ret  = ptr;
    } else {
        ret = ptr;
    }
    arg1 = arg1 & 0xFFFF;

    place     = 0x5F5E100;
    quot      = (u32)arg1 / 60;
    minutes   = quot & 0xFFFF;
    mins_work = minutes;
    arg1      = arg1 - quot * 60;
    arg1      = arg1 & 0xFFFF;

    if ((u32)minutes > 0x3B9AC9FEU) {
        *(Bytes10*)ret = *(Bytes10*)D_800138CC;
    } else if (minutes == 0) {
        *(Bytes2*)ret = *(Bytes2*)D_800138C8;
    } else {
        dest = ret;
        if ((u32)minutes < place) {
            do {
                place /= 10;
            } while (mins_work < place);
        }
        if (place != 0) {
            do {
                digit  = mins_work / place;
                *dest  = digit;
                temp   = *dest & 0xFF;
                digit  = temp * place;
                place /= 10;
                *dest  = temp + 0x30;
                dest++;
                mins_work -= digit;
            } while (place != 0);
        }
        *dest = 0;
    }

    if (minutes < 0x64) {
        goto check_lt10;
    }
    ptr += 3;
    goto after_off;
check_lt10:
    lt10 = minutes < 0xA;
    if (lt10 != 0) {
        goto plus1;
    }
    ptr += 2;
    goto after_off;
plus1:
    ptr += 1;
after_off:
    *ptr = 0x3A;
    ptr += 1;

    place = 1;
    i     = place;
    do {
        tmp   = place * 5;
        place = tmp * 2;
    } while (--i > 0);
    i = ((tmp * 8) + place) * 2 - 1;
    if ((u32)i < (u32)arg1) {
        arg1 = i;
    }
    dest = ptr;
    if ((u32)arg1 < place) {
        {
            u32 mag;
            mag = 0xCCCCCCCD;
            i   = 0x30;
            TOUCH_REG2(mag, i);
            do {
                asm volatile(
                    "multu %0, %2\n\t"
                    "sb %3, 0(%1)\n\t"
                    "mfhi $12\n\t"
                    "srl %0, $12, 3"
                    : "+r"(place)
                    : "r"(dest), "r"(mag), "r"(i));
                dest += 1;
            } while ((u32)arg1 < place);
        }
    }
    if (place != 0) {
        do {
            digit  = (u32)arg1 / place;
            *dest  = digit;
            temp   = *dest & 0xFF;
            digit  = temp * place;
            place /= 10;
            *dest  = temp + 0x30;
            dest  += 1;
            arg1  -= digit;
        } while (place != 0);
    }
    *dest = 0;
    return ret;
}

void Text_MeasureAndCenter(TextDrawReq* arg0, u8* arg1)
{
    u8* table;
    s32 width;

    switch (arg0->glyphTable) {
        case 0:
            table = Font_Glyphs0;
            break;
        case 5:
            table = Font_Glyphs2;
            break;
        default:
            table = Font_Glyphs1;
            break;
    }

    switch (arg0->centerMode) {
        case 1:
            width    = Text_MeasureGlyphWidth(arg0, arg1, table);
            arg0->x -= width >> 1;
            break;
        case 2:
            width    = Text_MeasureGlyphWidth(arg0, arg1, table);
            arg0->x -= width;
            break;
    }
}

u8* Text_ItoaSignedPlus(u8* arg0, s32 arg1)
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

    SOFT_COMPILER_BARRIER();
    sign = 0x2D;
    if (arg1 >= 0) {
        sign = 0x2B;
    }
    *arg0 = sign;

    place = 0x989680;
    start = arg0 + 1;
    if (arg1 < 0) {
        arg0[1] = 0x2D;
        Text_ItoaSigned(arg0 + 2, -arg1);
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

u8* Text_ItoaSigned(u8* arg0, s32 arg1)
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
        Text_ItoaSigned(arg0 + 1, -arg1);
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

u8* Text_ItoaUnsigned(u8* arg0, u32 arg1)
{
    typedef struct {
        u8 data[10];
    } Bytes10;
    typedef struct {
        u8 data[2];
    } Bytes2;

    u8*          dest;
    u32          place;
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
    SOFT_TOUCH_REG(ret);
    return ret;
}

u8* Text_ItoaHexSigned(u8* arg0, s32 arg1)
{
    typedef struct {
        u8 data[2];
    } Bytes2;

    u8*          dest;
    register s32 place asm("a0");
    s32          digit;
    s32          temp;
    s32          prod;
    u8*          ret;
    s32          cmp;

    place = 0x10000000;
    if (arg1 < 0) {
        *arg0 = 0x2D;
        Text_ItoaHexSigned(arg0 + 1, -arg1);
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
            place >>= 4;
        } while (arg1 < place);
    }
    if (place > 0) {
        do {
            digit = arg1 / place;
            *dest = digit;
            SOFT_BARRIER();
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
    SOFT_TOUCH_REG(ret);
    return ret;
}

u8* Text_ItoaHex(u8* arg0, u32 arg1)
{
    typedef struct {
        u8 data[2];
    } Bytes2;

    u8*          dest;
    register u32 place asm("a2");
    u32          digit;
    u32          temp;
    s32          prod;
    u8*          ret;

    place = 0x10000000;
    if (arg1 == 0) {
        *(Bytes2*)arg0 = *(Bytes2*)D_800138C8;
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
            SOFT_BARRIER();
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
    SOFT_TOUCH_REG(ret);
    return ret;
}

u8* func_8002F44C(u8* arg0, s32 arg1, s32 arg2)
{
    u8* p;
    u32 place;
    s32 val;
    s32 count;
    u32 limit;
    u32 raw;
    u32 digit;
    s32 product;

    val   = arg1;
    count = arg2 - 1;
    place = 1;
    if (count > 0) {
        do {
            place *= 10;
            count--;
        } while (count > 0);
    }
    limit = place * 10 - 1;
    if (limit < (u32)val) {
        val = limit;
    }
    p = arg0;
    if ((u32)val < place) {
        do {
            place /= 10;
            *p     = 0x30;
            p++;
        } while ((u32)val < place);
    }
    if (place != 0) {
        do {
            raw     = (u32)val / place;
            *p      = raw;
            digit   = *p;
            product = digit * place;
            place  /= 10;
            *p      = digit + 0x30;
            p++;
            val -= product;
        } while (place != 0);
    }
    *p = 0;
    return arg0;
}

u8* Text_SkipLines(u8* arg0, s32 arg1)
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

u8* Text_Strcat(u8* dest, u8* src)
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

void Text_DrawGlyphImmediate(TextDrawReq* arg0, FontGlyph* arg1, s32 arg2)
{
    SPRT* p;
    s32   temp;

    p = &D_80071710;
    setlen(p, 4);
    *(s32*)&p->r0 = arg2;
    setcode(p, 0x64);
    p->x0   = arg0->x + (s8)arg1->off_x;
    p->y0   = (arg0->y - arg1->h) + (s8)arg1->off_y;
    p->u0   = arg1->u;
    p->v0   = arg1->v + arg0->vBias;
    p->w    = arg1->w + 1;
    temp    = arg1->h;
    p->clut = 0x7FFD;
    p->h    = temp + 1;
    DrawPrim(p);
}

void Text_DrawGlyphQueued(TextDrawReq* arg0, FontGlyph* arg1, s32 arg2)
{
    SPRT* p;
    s32   temp;

    p              = (SPRT*)Gpu_PrimCursor;
    Gpu_PrimCursor = (DR_TPAGE*)(p + 1);
    setlen(p, 4);
    *(s32*)&p->r0 = arg2;
    setcode(p, 0x64);
    p->x0   = arg0->x + (s8)arg1->off_x;
    p->y0   = (arg0->y - arg1->h) + (s8)arg1->off_y;
    p->u0   = arg1->u;
    p->v0   = arg1->v + arg0->vBias;
    p->w    = arg1->w + 1;
    temp    = arg1->h;
    p->clut = 0x7FFD;
    p->h    = temp + 1;
    addPrim(Gpu_CurrentOt + arg0->otIndex, p);
}

void Text_DrawGlyphOt(TextDrawReq* arg0, FontGlyph* arg1)
{
    SPRT* p;
    s32   temp;

    p              = (SPRT*)Gpu_PrimCursor;
    Gpu_PrimCursor = (DR_TPAGE*)(p + 1);
    setlen(p, 4);
    setcode(p, 0x67);
    p->x0   = arg0->x + (s8)arg1->off_x;
    p->y0   = (arg0->y - arg1->h) + (s8)arg1->off_y;
    p->u0   = arg1->u;
    p->v0   = arg1->v + arg0->vBias;
    p->w    = arg1->w + 1;
    temp    = arg1->h;
    p->clut = 0x7FFF;
    p->h    = temp + 1;
    addPrim(Gpu_CurrentOt + arg0->otIndex, p);
}

void Text_UiTaskCallback(Task* arg0)
{
    UiObject* obj;
    s16       temp;

    if (arg0->state == 0) {
        Wip_UiHolder = NULL;
        obj          = Ui_SpawnFromDesc(D_800608F4, 1, 1, 2, 0);
        if (obj != NULL) {
            arg0->spawnArg2 = obj;
            arg0->state     = arg0->state + 1;
        }
    } else if (arg0->state == 1) {
        obj = arg0->spawnArg2;
        if (obj->field_2E == -1 || obj->field_2E == 6) {
            arg0->killCountdown = 0xA;
            arg0->state         = arg0->state + 1;
            Ui_TeardownTree(obj, obj->owner);
        }
    } else {
        temp                = arg0->killCountdown - Display_State.field_10a;
        arg0->killCountdown = temp;
        if (temp <= 0) {
            Task_Spawn(0, 2, 0xC, 0);
            Task_CallExit(arg0);
        }
    }
}

void Text_BootTask(Task* arg0)
{
    Text_LoadClutImages();
    Display_SetMode(0x1010);
    Game_ClearSession();
    Task_SpawnFromTable(Title_TaskDescs, 0, 0, 0);
    Task_Kill(arg0);
}
