#include "main/text.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>

#include "common.h"

#include "boot.h"
#include "main/display.h"
#include "main/display_types.h"
#include "fs.h"
#include "gameflow.h"
#include "mc.h"
#include "session.h"
#include "stage.h"
#include "main/task.h"
#include "task.h"
#include "main/task_types.h"
#include "text.h"
#include "main/ui.h"
#include "main/ui_types.h"

#include "gameplay/area_transitions.h"
#include "gameplay/companion_load.h"
#include "gameplay/effect_tasks.h"
#include "gameplay/hud_sprites.h"
#include "gameplay/loading.h"
#include "gameplay/model_objects.h"
#include "gameplay/room_effects.h"

#include "title/title.h"

/// Per-glyph metrics in the font tables (Font_Glyphs0 / Font_Glyphs1 / Font_Glyphs2).
/// u/v/w/h are texels in the 4bpp page at (960, 256). SPRT w/h are w+1 / h+1.
/// off_x / off_y are stored as bytes but used as signed offsets when drawing.
typedef struct _FontGlyph {
    /* 0x0 */ u8 u;
    /* 0x1 */ u8 v;
    /* 0x2 */ u8 w;
    /* 0x3 */ u8 h;
    /* 0x4 */ u8 off_x;
    /* 0x5 */ u8 off_y;
    /* 0x6 */ u8 field_6;
    /* 0x7 */ u8 field_7;
    /* 0x8 */ u8 field_8;
    /* 0x9 */ u8 field_9;
    /* 0xA */ u8 pad_A[2];
} FontGlyph;
STATIC_ASSERT_SIZEOF(FontGlyph, 0xC);

/// Immediate-mode SPRT scratch used by Text_DrawGlyphImmediate.
static SPRT D_80071710;

static DR_TPAGE D_80071728;

static TaskDesc D_8005EDA0[];

static u8 Font_Glyphs0[];

static u8 Font_Glyphs1[];

static u8 Font_Glyphs2[];

static UiObjectDesc Ui_OverlayLoadingDesc[];

/// Overflow and zero texts of the number formatters.
static const char Text_MaxEightDigits[];

static const char Text_ZeroDigit[];

static const char Text_MaxNineDigits[];

void func_807011D8(Task* arg0);

void func_80701400(Task* arg0);

static s32 Text_MeasureGlyphWidth(TextDrawReq* req, u8* str, u8* table);

static void Text_DrawGlyphDualSprtA(TextDrawReq* request, FontGlyph* glyph, s32 arg2);

static void Text_DrawGlyphDualSprt(TextDrawReq* request, FontGlyph* glyph, s32 arg2);

static void Text_DrawGlyphDualSprtTpage(TextDrawReq* request, FontGlyph* glyph, s32 arg2);

/// Writes `value` in decimal to `arg0` and terminates it; values past nine
/// digits are written as all nines.
static inline u8* _textItoaUnsigned(u8* arg0, u32 value);

/// Writes `arg1` in decimal to `arg0`, with a leading '-' when negative, and
/// terminates it; values past nine digits are written as all nines.
static inline u8* _textItoaSigned(u8* arg0, s32 arg1);

/// Writes `value` in decimal to `arg0` as exactly `width` digits, padded with
/// leading zeros and clamped to the largest value that fits, and terminates it.
static inline u8* _textItoaPadded(u8* arg0, u32 value, s32 width);

static u8* Text_ItoaHexSigned(u8* arg0, s32 arg1);

static u8* Text_ItoaHex(u8* arg0, u32 arg1);

static void Text_DrawGlyphImmediate(TextDrawReq* request, FontGlyph* glyph, s32 arg2);

static void Text_DrawGlyphQueued(TextDrawReq* request, FontGlyph* glyph, s32 arg2);

static void Text_DrawGlyphOt(TextDrawReq* request, FontGlyph* glyph, s32 unusedColor);

static void Text_UiTaskCallback(Task* task);

static void Text_BootTask(Task* task);

static const char Text_MaxEightDigits[];
static const char Text_ZeroDigit[];
static const char Text_MaxNineDigits[];

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
    { 0x0, 0xC0, McMenu_NoOpTask },
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
    D_80114B34,
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

static UiObjectDesc Ui_OverlayLoadingDesc[] = {
    { 2, 0xFF70, 0xFF98, 0x120, 0x90, 0x38, 0, 0, 0xC0, Ui_WaitCdThenOverlay, 0 },
};

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

static void Text_DrawGlyphDualSprtA(TextDrawReq* request, FontGlyph* glyph, s32 arg2)
{
    SPRT* p;
    SPRT* p2;
    s32   temp;

    p                     = gGpuPrimCursor;
    gGpuPrimCursor        = p + 1;
    PRIM_COLOR_WORD(p, 0) = arg2;
    setlen(p, 4);
    setcode(p, 0x66);

    p2             = gGpuPrimCursor;
    gGpuPrimCursor = p2 + 1;
    setlen(p2, 4);
    setcode(p2, 0x67);

    p2->x0 = p->x0 = request->x + (s8)glyph->off_x;
    p2->y0 = p->y0 = (request->y - glyph->h) + (s8)glyph->off_y;
    p2->u0 = p->u0 = glyph->u;
    p2->v0 = p->v0 = glyph->v + request->vBias;
    p2->w = p->w = glyph->w + 1;
    temp         = glyph->h;
    p2->h = p->h = temp + 1;
    p2->clut     = 0x7FFE;
    p->clut      = 0x7FFD;

    addPrim(gGpuCurrentOt + request->otIndex + 1, p2);
    addPrim(gGpuCurrentOt + request->otIndex, p);
}

static void Text_DrawGlyphDualSprt(TextDrawReq* request, FontGlyph* glyph, s32 arg2)
{
    SPRT* p;
    SPRT* p2;
    s32   temp;

    p                     = gGpuPrimCursor;
    gGpuPrimCursor        = p + 1;
    PRIM_COLOR_WORD(p, 0) = arg2;
    setlen(p, 4);
    setcode(p, 0x64);

    p2             = gGpuPrimCursor;
    gGpuPrimCursor = p2 + 1;
    setlen(p2, 4);
    setcode(p2, 0x67);

    p2->x0 = p->x0 = request->x + (s8)glyph->off_x;
    p2->y0 = p->y0 = (request->y - glyph->h) + (s8)glyph->off_y;
    p2->u0 = p->u0 = glyph->u;
    p2->v0 = p->v0 = glyph->v + request->vBias;
    p2->w = p->w = glyph->w + 1;
    temp         = glyph->h;
    p2->h = p->h = temp + 1;
    p2->clut     = 0x7FFF;
    p->clut      = 0x7FFD;

    addPrim(gGpuCurrentOt + request->otIndex + 1, p2);
    addPrim(gGpuCurrentOt + request->otIndex, p);
}

static void Text_DrawGlyphDualSprtTpage(TextDrawReq* request, FontGlyph* glyph, s32 arg2)
{
    SPRT*     p;
    SPRT*     p2;
    DR_TPAGE* dr;
    s32       temp;

    p                     = gGpuPrimCursor;
    gGpuPrimCursor        = p + 1;
    PRIM_COLOR_WORD(p, 0) = arg2;
    setlen(p, 4);
    setcode(p, 0x64);

    p2             = gGpuPrimCursor;
    gGpuPrimCursor = p2 + 1;
    setlen(p2, 4);
    setcode(p2, 0x67);

    p2->x0 = p->x0 = request->x + (s8)glyph->off_x;
    p2->y0 = p->y0 = (request->y - glyph->h) + (s8)glyph->off_y;
    p2->u0 = p->u0 = glyph->u;
    p2->v0 = p->v0 = glyph->v + request->vBias;
    p2->w = p->w = glyph->w + 1;
    temp         = glyph->h;
    p2->h = p->h = temp + 1;
    p2->clut     = 0x7FFF;
    p->clut      = 0x7FFD;

    addPrim(gGpuCurrentOt + request->otIndex, p);
    dr             = gGpuPrimCursor;
    gGpuPrimCursor = dr + 1;
    setlen(dr, 1);
    dr->code[0] = 0xE100023F;
    addPrim(gGpuCurrentOt + request->otIndex, dr);

    addPrim(gGpuCurrentOt + request->otIndex, p2);
    dr             = gGpuPrimCursor;
    gGpuPrimCursor = dr + 1;
    setlen(dr, 1);
    dr->code[0] = 0xE100025F;
    addPrim(gGpuCurrentOt + request->otIndex, dr);
}

void Text_DrawString(TextDrawReq* request, u8* text)
{
    u8*        ptr;
    u8*        table;
    FontGlyph* glyph;
    void       (*draw)(TextDrawReq*, FontGlyph*, s32);
    s32        color;
    s32        prev9;
    s32        width;
    u8         c;
    s32        end_flag;
    s32        idx;
    s32        temp;
    DR_TPAGE*  dr;

    ptr            = text;
    prev9          = 0;
    color          = request->field_8;
    request->vBias = 0;
    switch (request->glyphTable) {
        case 0:
            table          = Font_Glyphs0;
            request->vBias = 0x26;
            break;
        case 5:
            table          = Font_Glyphs2;
            request->vBias = 0;
            break;
        default:
            table          = Font_Glyphs1;
            request->vBias = 0x80;
            break;
    }
    switch (request->centerMode) {
        case 1:
            width       = Text_MeasureGlyphWidth(request, text, table);
            request->x -= width >> 1;
            break;
        case 2:
            width       = Text_MeasureGlyphWidth(request, text, table);
            request->x -= width;
            break;
    }
    switch (request->field_E) {
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
                                table          = Font_Glyphs2;
                                request->vBias = 0;
                                break;
                            case 'M':
                            case 'm':
                                table          = Font_Glyphs0;
                                request->vBias = 0x26;
                                break;
                            case 'L':
                            case 'l':
                                table          = Font_Glyphs1;
                                request->vBias = 0x80;
                                break;
                        }
                        ptr++;
                        break;
                    case 'W':
                    case 'w':
                        ptr++;
                        switch (*ptr) {
                            case '0':
                                request->field_E = 3;
                                draw             = Text_DrawGlyphDualSprtA;
                                break;
                            case '1':
                                request->field_E = 1;
                                draw             = Text_DrawGlyphDualSprt;
                                break;
                        }
                        ptr++;
                        break;
                    case 'U':
                    case 'u':
                        ptr++;
                        if ((u32)(*ptr - '0') < 10) {
                            request->y -= *ptr - '0';
                        }
                        ptr++;
                        break;
                    case 'D':
                    case 'd':
                        ptr++;
                        if ((u32)(*ptr - '0') < 10) {
                            request->y += *ptr - '0';
                        }
                        ptr++;
                        break;
                    case 'B':
                    case 'b':
                        ptr++;
                        if ((u32)(*ptr - '0') < 10) {
                            request->x = (*ptr - '0') << 3;
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
            if (request->glyphTable == 5) {
                request->x -= 1;
            } else {
                request->x -= 2;
            }
        }
        prev9 = glyph->field_9;
        draw(request, glyph, color);
        ptr++;
        request->x += glyph->w + (s8)glyph->field_6 + (s8)glyph->off_x;
        request->y += (s8)glyph->field_7;
    }
    if (request->field_E == 1 || request->field_E == 3) {
        dr             = gGpuPrimCursor;
        gGpuPrimCursor = dr + 1;
        dr->code[0]    = 0xE100025F;
        setlen(dr, 1);
        addPrim(gGpuCurrentOt + request->otIndex + 1, dr);
    }
    if (request->field_E != 16) {
        if (request->field_E == 4) {
            dr             = gGpuPrimCursor;
            gGpuPrimCursor = dr + 1;
            dr->code[0]    = 0xE100025F;
            setlen(dr, 1);
            addPrim(gGpuCurrentOt + request->otIndex, dr);
        } else {
            dr             = gGpuPrimCursor;
            gGpuPrimCursor = dr + 1;
            dr->code[0]    = 0xE100023F;
            setlen(dr, 1);
            addPrim(gGpuCurrentOt + request->otIndex, dr);
        }
    }
}

/// Writes `value` in decimal to `arg0` and terminates it; values past nine
/// digits are written as all nines.
static inline u8* _textItoaUnsigned(u8* arg0, u32 value)
{
    typedef struct {
        u8 data[10];
    } Bytes10;
    typedef struct {
        u8 data[2];
    } Bytes2;

    u8* dest;
    u32 place;

    place = 0x5F5E100;
    if (value > 0x3B9AC9FEU) {
        *(Bytes10*)arg0 = *(Bytes10*)Text_MaxNineDigits;
    } else if (value == 0) {
        *(Bytes2*)arg0 = *(Bytes2*)Text_ZeroDigit;
    } else {
        dest = arg0;
        while (value < place) {
            place /= 10;
        }
        while (place != 0) {
            *dest    = value / place;
            value   -= *dest * place;
            place   /= 10;
            *dest++ += '0';
        }
        *dest = 0;
    }
    return arg0;
}

/// Writes `arg1` in decimal to `arg0`, with a leading '-' when negative, and
/// terminates it; values past nine digits are written as all nines.
static inline u8* _textItoaSigned(u8* arg0, s32 arg1)
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
        *(Bytes9*)arg0 = *(Bytes9*)Text_MaxEightDigits;
        return arg0;
    }
    cmp = arg1 < place;
    if (arg1 == 0) {
        *(Bytes2*)arg0 = *(Bytes2*)Text_ZeroDigit;
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

/// Writes `value` in decimal to `arg0` as exactly `width` digits, padded with
/// leading zeros and clamped to the largest value that fits, and terminates it.
static inline u8* _textItoaPadded(u8* arg0, u32 value, s32 width)
{
    u8* p;
    u32 place;
    s32 count;
    u32 limit;

    place = 1;
    count = width - 1;
    while (count > 0) {
        place *= 10;
        count--;
    }
    limit = place * 10 - 1;
    if (limit < value) {
        value = limit;
    }
    p = arg0;
    while (value < place) {
        place /= 10;
        *p++   = '0';
    }
    while (place != 0) {
        *p     = value / place;
        value -= *p * place;
        place /= 10;
        *p++  += '0';
    }
    *p = 0;
    return arg0;
}

/// Writes a play time given in minutes as `H:MM` to `arg0`, capping it at
/// 999:59, and returns `arg0`.
u8* Text_FormatTime(u8* arg0, u16 time)
{
    u8* ret;
    s32 hours;
    s32 minutes;

    ret = arg0;
    if (time > 59999) {
        time = 59999;
    }
    hours   = time / 60;
    minutes = time % 60;
    _textItoaUnsigned(ret, hours);
    if (hours >= 100) {
        arg0 += 3;
    } else if (hours >= 10) {
        arg0 += 2;
    } else {
        arg0 += 1;
    }
    *arg0++ = ':';
    _textItoaPadded(arg0, minutes, 2);
    return ret;
}

void Text_MeasureAndCenter(TextDrawReq* request, u8* arg1)
{
    u8* table;
    s32 width;

    switch (request->glyphTable) {
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

    switch (request->centerMode) {
        case 1:
            width       = Text_MeasureGlyphWidth(request, arg1, table);
            request->x -= width >> 1;
            break;
        case 2:
            width       = Text_MeasureGlyphWidth(request, arg1, table);
            request->x -= width;
            break;
    }
}

u8* Text_ItoaSignedPlus(u8* arg0, s32 arg1)
{
    *arg0 = arg1 >= 0 ? '+' : '-';
    _textItoaSigned(arg0 + 1, arg1);
    return arg0;
}

u8* Text_ItoaSigned(u8* arg0, s32 arg1)
{
    return _textItoaSigned(arg0, arg1);
}

u8* Text_ItoaUnsigned(u8* arg0, u32 arg1)
{
    return _textItoaUnsigned(arg0, arg1);
}

static u8* Text_ItoaHexSigned(u8* arg0, s32 arg1)
{
    typedef struct {
        u8 data[2];
    } Bytes2;

    u8* dest;
    s32 place;
    s32 digit;
    s32 temp;
    s32 cmp;

    place = 0x10000000;
    if (arg1 < 0) {
        *arg0 = 0x2D;
        Text_ItoaHexSigned(arg0 + 1, -arg1);
        return arg0;
    }
    cmp = arg1 < place;
    if (arg1 == 0) {
        *(Bytes2*)arg0 = *(Bytes2*)Text_ZeroDigit;
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
            digit   = arg1 / place;
            *dest   = digit;
            temp    = *dest & 0xFF;
            digit   = temp * place;
            place >>= 4;
            arg1   -= digit;
            if (temp >= 10U) {
                *dest = temp + 0x37;
            } else {
                *dest = temp + 0x30;
            }
            dest++;
        } while (place > 0);
    }
    *dest = 0;
    return arg0;
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
        *(Bytes2*)arg0 = *(Bytes2*)Text_ZeroDigit;
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

u8* Text_ItoaPadded(u8* buffer, s32 value, s32 width)
{
    return _textItoaPadded(buffer, value, width);
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

static void Text_DrawGlyphImmediate(TextDrawReq* request, FontGlyph* glyph, s32 arg2)
{
    SPRT* p;
    s32   temp;

    p = &D_80071710;
    setlen(p, 4);
    PRIM_COLOR_WORD(p, 0) = arg2;
    setcode(p, 0x64);
    p->x0   = request->x + (s8)glyph->off_x;
    p->y0   = (request->y - glyph->h) + (s8)glyph->off_y;
    p->u0   = glyph->u;
    p->v0   = glyph->v + request->vBias;
    p->w    = glyph->w + 1;
    temp    = glyph->h;
    p->clut = 0x7FFD;
    p->h    = temp + 1;
    DrawPrim(p);
}

static void Text_DrawGlyphQueued(TextDrawReq* request, FontGlyph* glyph, s32 arg2)
{
    SPRT* p;
    s32   temp;

    p              = gGpuPrimCursor;
    gGpuPrimCursor = p + 1;
    setlen(p, 4);
    PRIM_COLOR_WORD(p, 0) = arg2;
    setcode(p, 0x64);
    p->x0   = request->x + (s8)glyph->off_x;
    p->y0   = (request->y - glyph->h) + (s8)glyph->off_y;
    p->u0   = glyph->u;
    p->v0   = glyph->v + request->vBias;
    p->w    = glyph->w + 1;
    temp    = glyph->h;
    p->clut = 0x7FFD;
    p->h    = temp + 1;
    addPrim(gGpuCurrentOt + request->otIndex, p);
}

static void Text_DrawGlyphOt(TextDrawReq* request, FontGlyph* glyph, s32 unusedColor)
{
    SPRT* p;
    s32   temp;

    p              = gGpuPrimCursor;
    gGpuPrimCursor = p + 1;
    setlen(p, 4);
    setcode(p, 0x67);
    p->x0   = request->x + (s8)glyph->off_x;
    p->y0   = (request->y - glyph->h) + (s8)glyph->off_y;
    p->u0   = glyph->u;
    p->v0   = glyph->v + request->vBias;
    p->w    = glyph->w + 1;
    temp    = glyph->h;
    p->clut = 0x7FFF;
    p->h    = temp + 1;
    addPrim(gGpuCurrentOt + request->otIndex, p);
}

static void Text_UiTaskCallback(Task* task)
{
    UiObject* obj;
    s16       temp;

    if (task->state == 0) {
        Wip_UiHolder = NULL;
        obj          = Ui_SpawnFromDesc(Ui_OverlayLoadingDesc, 1, 1, 2, 0);
        if (obj != NULL) {
            task->spawnArg2.pointer = obj;
            task->state             = task->state + 1;
        }
    } else if (task->state == 1) {
        obj = task->spawnArg2.pointer;
        if (obj->field_2E == -1 || obj->field_2E == 6) {
            task->killCountdown = 0xA;
            task->state         = task->state + 1;
            Ui_TeardownTree(obj, obj->owner);
        }
    } else {
        temp                = task->killCountdown - gDisplayState.frameTicks;
        task->killCountdown = temp;
        if (temp <= 0) {
            Task_Spawn(0, 2, 0xC, 0);
            Task_CallExit(task);
        }
    }
}

static void Text_BootTask(Task* task)
{
    Text_LoadClutImages();
    Display_SetMode(DISPLAY_SETUP_DEFAULT);
    Game_ClearSession();
    Task_SpawnFromTable(Title_TaskDescs, 0, 0, 0);
    taskKill(task);
}

/// Overflow and zero texts of the number formatters.
static const char Text_MaxEightDigits[] = "99999999";
static const char Text_ZeroDigit[]      = "0";
static const char Text_MaxNineDigits[]  = "999999999";
