#include "common.h"

#include "main/unknown_syms.h"
#include "main/text.h"
#include "main/title.h"
#include "main/ui.h"
#include "main/boot.h"
#include "main/fs.h"
#include "main/gameflow.h"
#include "main/mc.h"
#include "main/stage.h"
#include "main/task.h"
#include "main/devkit.h"
#include "gameplay/gameplay.h"
#include "gameplay/1A8.h"
#include "gameplay/D4.h"

static const char D_800138BC[];
static const char D_800138C8[];
static const char D_800138CC[];

static void Text_DrawGlyphImmediate(TextDrawReq* arg0, FontGlyph* arg1, s32 arg2);
static void Text_DrawGlyphOt(TextDrawReq* arg0, FontGlyph* arg1);
static void Text_DrawGlyphQueued(TextDrawReq* arg0, FontGlyph* arg1, s32 arg2);
static void Text_UiTaskCallback(Task* arg0);

static void Text_BootTask(Task* arg0);

static TaskDesc D_8005EDA0[] = {
    { 0x0, 0xC0, textNoopCallback },
    { 0x0, 0xC0, taskCountdownCallback },
    { 0x0, 0xC0, Title_Dispatch },
    { 0x0, 0xC0, GameFlow_StateByField34 },
    { 0x0, 0xC0, GameFlow_DispatchTable5 },
    { 0x0, 0xC0, Text_UiTaskCallback },
    { 0x0, 0xC0, Title_ExitTask },
    { 0x0, 0xC0, taskKill },
    { 0x0, 0x0, NULL },
    { 0x0, 0x18, GameFlow_DispatchTable },
    { 0x0, 0x10, Mc_DispatchStateTable },
    { 0x0, 0x10, Mc_DispatchStateTable26 },
    { 0x0, 0xC0, func_80036A1C },
    { 0x0, 0x10, Text_BootTask },
    { 0x2, 0x2F, func_800A8654 },
    { 0x0, 0x2F, Gp_ApplyViewTask },
    { 0x0, 0x40, func_800AD50C },
    { 0x0, 0x28, func_800AC0F0 },
    { 0x0, 0x10, Mc_DispatchStateTable },
    { 0x0, 0x10, Mc_DispatchStateTable26 },
    { 0x0, 0x1F, func_800AEE8C },
    { 0x0, 0xC0, taskKill },
    { 0x0, 0x30, Gp_ViewGateTask },
    { 0x0, 0x2F, Gp_AllocSprtListsTask },
    { 0x0, 0xC0, Stage_TaskExit },
    { 0x0, 0xC0, func_807011D8 },
    { 0x0, 0xE0, Gp_DrawDisp2dOt },
    { 0x0, 0xD0, func_800AD5B8 },
    { 0x0, 0x2F, Gp_LoadStateTask },
    { 0x0, 0x18, func_800A77B4 },
    { 0x0, 0xF8, Gp_LoadWaitDispatch },
    { 0x0, 0x10, Boot_LoadInitialFile },
    { 0x0, 0x10, Boot_LoadTask },
    { 0x0, 0x2F, Gp_FlashWhiteTask },
    { 0x0, 0xF8, NULL },
    { 0x0, 0xC0, func_80701400 },
    { 0x0, 0x2F, NULL },
    { 0x0, 0xF8, Gp_CommitSpawnLoc },
    { 0x0, 0xF8, Gp_SetupSprtDisplay },
};

TaskDesc* gTaskDescBanks[15] = {
    D_8005EDA0,
    D_800670D0,
    D_80067828,
    D_80062780,
    D_800676A8,
    D_800626AC,
    D_8010FC2C,
    D_800678F4,
    D_800626EC,
    D_80067734,
    (TaskDesc*)0x80114B34,
    D_80067828,
    D_80067828,
    D_80067828,
    D_80068B7C,
};

static u8 Font_Glyphs0[] = {
#include "assets/font_glyphs0.inc"
};
static u8 Font_Glyphs1[] = {
#include "assets/font_glyphs1.inc"
};
static u8 Font_Glyphs2[] = {
#include "assets/font_glyphs2.inc"
};

static UiObjectDesc D_800608F4[] = {
    { 2, 0xFF70, 0xFF98, 0x120, 0x90, 0x38, 0, 0, 0xC0, Ui_WaitCdThenOverlay, 0 },
};

/// Immediate-mode SPRT scratch used by Text_DrawGlyphImmediate.
static SPRT     D_80071710;
static DR_TPAGE D_80071728;

void textNoopCallback(Task* task)
{
}

/// Kerning between two adjacent glyphs: unless the previous glyph's trailing
/// byte (`prev`, its field_9) and the next glyph's field_8 sum to -1..1 as a
/// byte, the pen position `x` is pulled in by one pixel for font table 5 and
/// by two for the others. The string drawer applies the same rule.
#define TEXT_APPLY_KERNING(x, prev, glyph, req)         \
    do {                                                \
        if ((u8)((prev) + (glyph)->field_8 + 1) >= 3) { \
            if ((req)->glyphTable == 5) {               \
                (x) -= 1;                               \
            } else {                                    \
                (x) -= 2;                               \
            }                                           \
        }                                               \
    } while (0)

static s32 Text_MeasureGlyphWidth(TextDrawReq* req, u8* str, u8* table)
{
    s32        width;
    FontGlyph* glyph;
    u8         kern;
    s32        stop;
    u8         c;
    s32        idx;

    width = 0;
    glyph = (FontGlyph*)table;
    kern  = 0;
    for (c = *str; c != 0; c = *str) {
        if (c == '\n') {
            break;
        }
        stop = 0;
        if (c == '\\') {
            do {
                str++;
                switch (*str) {
                    case 'B':
                    case 'C':
                    case 'D':
                    case 'S':
                    case 'U':
                    case 'W':
                    case 'b':
                    case 'c':
                    case 'd':
                    case 's':
                    case 'u':
                    case 'w':
                        str += 2;
                        break;
                    case 'N':
                    case 'n':
                        stop = 1;
                        break;
                }
                if (*str == 0 || *str == '\n') {
                    stop = 1;
                }
            } while (*str == c);
        }
        if (stop) {
            break;
        }
        idx   = *str - ' ';
        glyph = &((FontGlyph*)table)[idx];
        TEXT_APPLY_KERNING(width, kern, glyph, req);
        str++;
        kern   = glyph->field_9;
        width += glyph->w + (s8)glyph->field_6 + (s8)glyph->off_x;
    }
    return width - (s8)glyph->field_6;
}

static void Text_DrawGlyphDualSprtA(TextDrawReq* arg0, FontGlyph* arg1, s32 arg2)
{
    SPRT* p;
    SPRT* p2;
    s32   temp;

    p                     = (SPRT*)gGpuPrimCursor;
    gGpuPrimCursor        = p + 1;
    PRIM_COLOR_WORD(p, 0) = arg2;
    setlen(p, 4);
    setcode(p, 0x66);

    p2             = (SPRT*)gGpuPrimCursor;
    gGpuPrimCursor = p2 + 1;
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

    addPrim(gGpuCurrentOt + arg0->otIndex + 1, p2);
    addPrim(gGpuCurrentOt + arg0->otIndex, p);
}

static void Text_DrawGlyphDualSprt(TextDrawReq* arg0, FontGlyph* arg1, s32 arg2)
{
    SPRT* p;
    SPRT* p2;
    s32   temp;

    p                     = (SPRT*)gGpuPrimCursor;
    gGpuPrimCursor        = p + 1;
    PRIM_COLOR_WORD(p, 0) = arg2;
    setlen(p, 4);
    setcode(p, 0x64);

    p2             = (SPRT*)gGpuPrimCursor;
    gGpuPrimCursor = p2 + 1;
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

    addPrim(gGpuCurrentOt + arg0->otIndex + 1, p2);
    addPrim(gGpuCurrentOt + arg0->otIndex, p);
}

static void Text_DrawGlyphDualSprtTpage(TextDrawReq* arg0, FontGlyph* arg1, s32 arg2)
{
    SPRT*     p;
    SPRT*     p2;
    DR_TPAGE* dr;
    s32       temp;

    p                     = (SPRT*)gGpuPrimCursor;
    gGpuPrimCursor        = p + 1;
    PRIM_COLOR_WORD(p, 0) = arg2;
    setlen(p, 4);
    setcode(p, 0x64);

    p2             = (SPRT*)gGpuPrimCursor;
    gGpuPrimCursor = p2 + 1;
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

    addPrim(gGpuCurrentOt + arg0->otIndex, p);
    dr             = gGpuPrimCursor;
    gGpuPrimCursor = dr + 1;
    setlen(dr, 1);
    dr->code[0] = 0xE100023F;
    addPrim(gGpuCurrentOt + arg0->otIndex, dr);

    addPrim(gGpuCurrentOt + arg0->otIndex, p2);
    dr             = gGpuPrimCursor;
    gGpuPrimCursor = dr + 1;
    setlen(dr, 1);
    dr->code[0] = 0xE100025F;
    addPrim(gGpuCurrentOt + arg0->otIndex, dr);
}

void func_8002E53C(TextDrawReq* arg0, u8* arg1)
{
    u8*        ptr;
    u8*        table;
    FontGlyph* glyph;
    void       (*draw)();
    s32        color;
    s32        prev9;
    s32        width;
    u8         c;
    s32        end_flag;
    s32        idx;
    s32        temp;
    DR_TPAGE*  dr;

    ptr         = arg1;
    prev9       = 0;
    color       = arg0->field_8;
    arg0->vBias = 0;
    switch (arg0->glyphTable) {
        case 0:
            table       = Font_Glyphs0;
            arg0->vBias = 0x26;
            break;
        case 5:
            table       = Font_Glyphs2;
            arg0->vBias = 0;
            break;
        default:
            table       = Font_Glyphs1;
            arg0->vBias = 0x80;
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
    switch (arg0->field_E) {
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
    while ((c = *ptr) != 0 && c != '\n' && c != '\r') {
        end_flag = 0;
        if (c == '\\') {
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
                                table       = Font_Glyphs2;
                                arg0->vBias = 0;
                                break;
                            case 'M':
                            case 'm':
                                table       = Font_Glyphs0;
                                arg0->vBias = 0x26;
                                break;
                            case 'L':
                            case 'l':
                                table       = Font_Glyphs1;
                                arg0->vBias = 0x80;
                                break;
                        }
                        ptr++;
                        break;
                    case 'W':
                    case 'w':
                        ptr++;
                        switch (*ptr) {
                            case '0':
                                arg0->field_E = 3;
                                draw          = Text_DrawGlyphDualSprtA;
                                break;
                            case '1':
                                arg0->field_E = 1;
                                draw          = Text_DrawGlyphDualSprt;
                                break;
                        }
                        ptr++;
                        break;
                    case 'U':
                    case 'u':
                        ptr++;
                        if ((u32)(*ptr - '0') < 10) {
                            arg0->y -= *ptr - '0';
                        }
                        ptr++;
                        break;
                    case 'D':
                    case 'd':
                        ptr++;
                        if ((u32)(*ptr - '0') < 10) {
                            arg0->y += *ptr - '0';
                        }
                        ptr++;
                        break;
                    case 'B':
                    case 'b':
                        ptr++;
                        if ((u32)(*ptr - '0') < 10) {
                            arg0->x = (*ptr - '0') << 3;
                        }
                        ptr++;
                        break;
                    case 'N':
                    case 'n':
                        end_flag = 1;
                        break;
                }
                if (*ptr == 0 || *ptr == '\n' || *ptr == '\r') {
                    end_flag = 1;
                }
            } while (*ptr == '\\');
        }
        if (end_flag != 0) {
            break;
        }
        if (*ptr < ' ') {
            ptr++;
            continue;
        }
        idx   = *ptr - ' ';
        glyph = &((FontGlyph*)table)[idx];
        temp  = prev9 + glyph->field_8 + 1;
        if ((u8)temp >= 3) {
            if (arg0->glyphTable == 5) {
                arg0->x -= 1;
            } else {
                arg0->x -= 2;
            }
        }
        prev9 = glyph->field_9;
        draw(arg0, glyph, color);
        ptr++;
        arg0->x += glyph->w + (s8)glyph->field_6 + (s8)glyph->off_x;
        arg0->y += (s8)glyph->field_7;
    }
    if (arg0->field_E == 1 || arg0->field_E == 3) {
        dr             = gGpuPrimCursor;
        gGpuPrimCursor = dr + 1;
        dr->code[0]    = 0xE100025F;
        setlen(dr, 1);
        addPrim(gGpuCurrentOt + arg0->otIndex + 1, dr);
    }
    if (arg0->field_E != 16) {
        if (arg0->field_E == 4) {
            dr             = gGpuPrimCursor;
            gGpuPrimCursor = dr + 1;
            dr->code[0]    = 0xE100025F;
            setlen(dr, 1);
            addPrim(gGpuCurrentOt + arg0->otIndex, dr);
        } else {
            dr             = gGpuPrimCursor;
            gGpuPrimCursor = dr + 1;
            dr->code[0]    = 0xE100023F;
            setlen(dr, 1);
            addPrim(gGpuCurrentOt + arg0->otIndex, dr);
        }
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

static u8* Text_ItoaHexSigned(u8* arg0, s32 arg1)
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
    return ret;
}

static u8* Text_ItoaHex(u8* arg0, u32 arg1)
{
    typedef struct {
        u8 data[2];
    } Bytes2;

    u8* dest;
    u32 place;
    u32 digit;

    place = 0x10000000;
    if (arg1 == 0) {
        *(Bytes2*)arg0 = *(Bytes2*)D_800138C8;
    } else {
        dest = arg0;
        if (arg1 < place) {
            do {
                place >>= 4;
            } while (arg1 < place);
        }
        if (place != 0) {
            do {
                *dest   = arg1 / place;
                digit   = *dest;
                arg1   -= digit * place;
                place >>= 4;
                if (digit >= 10) {
                    *dest = digit + 0x37;
                } else {
                    *dest = digit + 0x30;
                }
                dest++;
            } while (place != 0);
        }
        *dest = 0;
    }
    return arg0;
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

static void Text_DrawGlyphImmediate(TextDrawReq* arg0, FontGlyph* arg1, s32 arg2)
{
    SPRT* p;
    s32   temp;

    p = &D_80071710;
    setlen(p, 4);
    PRIM_COLOR_WORD(p, 0) = arg2;
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

static void Text_DrawGlyphQueued(TextDrawReq* arg0, FontGlyph* arg1, s32 arg2)
{
    SPRT* p;
    s32   temp;

    p              = (SPRT*)gGpuPrimCursor;
    gGpuPrimCursor = p + 1;
    setlen(p, 4);
    PRIM_COLOR_WORD(p, 0) = arg2;
    setcode(p, 0x64);
    p->x0   = arg0->x + (s8)arg1->off_x;
    p->y0   = (arg0->y - arg1->h) + (s8)arg1->off_y;
    p->u0   = arg1->u;
    p->v0   = arg1->v + arg0->vBias;
    p->w    = arg1->w + 1;
    temp    = arg1->h;
    p->clut = 0x7FFD;
    p->h    = temp + 1;
    addPrim(gGpuCurrentOt + arg0->otIndex, p);
}

static void Text_DrawGlyphOt(TextDrawReq* arg0, FontGlyph* arg1)
{
    SPRT* p;
    s32   temp;

    p              = (SPRT*)gGpuPrimCursor;
    gGpuPrimCursor = p + 1;
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
    addPrim(gGpuCurrentOt + arg0->otIndex, p);
}

static void Text_UiTaskCallback(Task* arg0)
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
        temp                = arg0->killCountdown - gDisplayState.frameTicks;
        arg0->killCountdown = temp;
        if (temp <= 0) {
            Task_Spawn(0, 2, 0xC, 0);
            Task_CallExit(arg0);
        }
    }
}

static void Text_BootTask(Task* arg0)
{
    Text_LoadClutImages();
    Display_SetMode(0x1010);
    Game_ClearSession();
    Task_SpawnFromTable(Title_TaskDescs, 0, 0, 0);
    taskKill(arg0);
}

/// Overflow and zero texts of the number formatters.
static const char D_800138BC[] = "99999999";
static const char D_800138C8[] = "0";
static const char D_800138CC[] = "999999999";
