#include "common.h"

#include "main/display.h"
#include "main/fs.h"
#include "main/mc.h"
#include "main/mem.h"
#include "main/sound.h"
#include "main/text.h"
#include "main/wipsys.h"
#include "gameplay/D4.h"

/// A CLUT upload record: the rectangle a CLUT belongs at and the CLUT itself.
/// Each CLUT is followed by one, but nothing reads them.
typedef struct {
    s32     field_0;
    RECT    rect;
    u_long* clut;
    s32     field_10;
    s32     field_14[3];
} _TextClutRecord;

/// Fill palettes (64 entries) for Text_LoadClutImages → (256, 243).
static u_long D_80060910[] = {
#include "assets/text_clut0.inc"
};
/// Unreferenced.
static _TextClutRecord D_80060990 = { 0, { 0x100, 0xF3, 0x40, 1 }, D_80060910, 0xFF, { 0 } };

/// Outline palettes (48 entries) for Text_LoadClutImages → (0x3D0, 0x1FF).
static u_long D_800609B0[] = {
#include "assets/text_clut1.inc"
};
/// Unreferenced.
static _TextClutRecord D_80060A10 = { 0, { 0x100, 0xF0, 0x30, 1 }, D_800609B0, 0xFF, { 0 } };

GpFlagBank* Gp_FlagBanks[] = {
    NULL,
    (GpFlagBank*)D_800733F0,
    (GpFlagBank*)D_800734C8,
    (GpFlagBank*)D_80073628,
    (GpFlagBank*)D_80073670,
    (GpFlagBank*)D_80073838,
};

static s32 Text_ParseLine(u8** arg0, u8* arg1)
{
    s32 ret;
    u8* src;
    u8* p;
    u8  c;
    u8  next;

    ret = 0;
    do {
        src = *arg0;
        c   = *src;
        if (c == 0x5C) {
            *arg0 = src + 1;
            switch (src[1]) {
                case 'Z':
                case 'z':
                    *arg1++ = 0;
                    ret     = -1;
                    (*arg0)++;
                    break;
                case 'N':
                case 'n':
                    *arg1++ = 0;
                    ret     = 1;
                    *arg0  += ret;
                    break;
                case 0x5C:
                    *arg1 = **arg0;
                    arg1 += 1;
                    (*arg0)++;
                    break;
                default:
                    *arg1++ = 0x5C;
                    *arg1   = **arg0;
                    arg1   += 1;
                    (*arg0)++;
                    break;
            }
        } else if (c == 0) {
            *arg1++ = 0;
            ret     = -1;
            (*arg0)++;
        } else if (c == 0xA) {
            *arg1++ = 0;
            ret     = 1;
            *arg0  += ret;
        } else if (c == 0xD) {
            *arg1 = 0;
            p     = *arg0;
            *arg0 = p + 1;
            next  = p[1];
            arg1 += 1;
            if (next == 0xA) {
                arg1 += 1;
                *arg0 = p + 2;
            }
            ret = 1;
        } else if (((u8)(c + 0x7F) < 0x1FU) || ((u8)(c + 0x20) < 0x1DU)) {
            *arg1 = c;
            p     = *arg0;
            *arg0 = p + 1;
            arg1 += 1;
            *arg1 = p[1];
            arg1 += 1;
            (*arg0)++;
        } else {
            *arg1 = c;
            arg1 += 1;
            (*arg0)++;
        }
    } while (ret == 0);
    return ret;
}

/// One line of Text_DrawMultiLine or Text_DrawMultiLineScroll: relative to obj's origin, or at an absolute
/// position when obj is NULL; skipped when obj is in mode 5.
static inline void _textDrawLine(UiObject* obj, s32 x, s32 y, u8* text, s32 arg4, s32 arg5, s32 arg6)
{
    TextDrawReq req;
    TextDrawReq req2;
    s32         temp;

    if (obj != NULL) {
        if (obj->panel.field_8 != 5) {
            req.x          = obj->panel.field_20.u + x;
            req.y          = (obj->panel.field_22.u + y) - 3;
            temp           = obj->panel.field_14.s;
            req.field_8    = arg4;
            req.otIndex    = temp + 1;
            req.glyphTable = 4;
            req.centerMode = arg6;
            req.field_E    = arg5;
            func_8002E53C(&req, text);
        }
    } else {
        req2.x          = x;
        req2.y          = y;
        req2.otIndex    = 4;
        req2.field_8    = arg4;
        req2.glyphTable = 4;
        req2.centerMode = arg6;
        req2.field_E    = arg5;
        func_8002E53C(&req2, text);
    }
}

s32 Text_DrawMultiLine(UiObject* arg0, s32 arg1, s32 arg2, u8* arg3, s32 arg4, s32 arg5, s32 arg6)
{
    u8  buf[0x40];
    u8* cur;
    s32 x;
    s32 y;
    s32 ret;

    x   = arg1;
    y   = arg2;
    cur = arg3;
    do {
        ret = Text_ParseLine(&cur, buf);
        _textDrawLine(arg0, x, y, buf, arg4, arg5, arg6);
        x  = arg1;
        y += 0xF;
    } while (ret != -1);

    return 0;
}

s32 Text_MeasureWidth(u8* arg0)
{
    TextDrawReq sp10;

    sp10.glyphTable = 4;
    sp10.x          = 0;
    sp10.y          = 0;
    sp10.otIndex    = 0;
    sp10.field_8    = 0;
    sp10.centerMode = 2;
    sp10.field_E    = 0;
    Text_MeasureAndCenter(&sp10, arg0);
    return -sp10.x;
}

s32 Text_MeasureMultiLine(u8* arg0)
{
    u8           sp10[0x40];
    TextDrawReq  sp50;
    s32          maxWidth;
    s32          height;
    TextDrawReq* p;
    u8*          buf;
    u8*          cur;
    s32          ret;
    s32          tmp;
    s8           c;

    maxWidth = 0;
    height   = maxWidth;
    p        = &sp50;
    cur      = arg0;
    buf      = sp10;

    do {
        ret = Text_ParseLine(&cur, sp10);

        c             = 4;
        sp50.x        = 0;
        sp50.y        = 0;
        sp50.otIndex  = 0;
        sp50.field_8  = 0;
        tmp           = c;
        p->glyphTable = tmp;
        c             = 2;
        p->centerMode = c;
        sp50.field_E  = 0;
        Text_MeasureAndCenter(p, buf);

        if (maxWidth < -sp50.x) {
            do {
            } while (0);
            maxWidth = -sp50.x;
        }
        height += 0xF;
        cur     = buf;
    } while (ret != -1);

    return (height << 16) | maxWidth;
}

s32 Text_DrawPrompt(UiObject* arg0, s32 arg1, s32 arg2, u8* arg3, s32 arg4, s32 arg5, s32 arg6)
{
    TextDrawReq sp10;
    TextDrawReq sp20;
    s32         temp;

    if (arg0 != NULL) {
        if (arg0->panel.field_8 == 5) {
            return 0;
        }
    } else {
        sp20.x          = arg1;
        sp20.y          = arg2;
        sp20.otIndex    = 4;
        sp20.field_8    = arg4;
        sp20.glyphTable = 4;
        sp20.centerMode = arg6;
        sp20.field_E    = arg5;
        func_8002E53C(&sp20, arg3);
        return arg1;
    }
    sp10.x          = arg0->panel.field_20.u + arg1;
    sp10.y          = (arg0->panel.field_22.u + arg2) - 3;
    temp            = arg0->panel.field_14.s;
    sp10.field_8    = arg4;
    sp10.glyphTable = 4;
    sp10.centerMode = arg6;
    sp10.field_E    = arg5;
    sp10.otIndex    = temp + 1;
    func_8002E53C(&sp10, arg3);
    return sp10.x - (s16)arg0->panel.field_20.u;
}

static void Text_DrawPromptCompat(void* arg0, void* arg1, void* arg2, void* arg3, void* arg4, void* arg5, void* arg6)
{
    Text_DrawPrompt(arg0, arg1, arg2, arg3, arg4, arg5, arg6);
}

static s32 Text_DrawMultiLineScroll(UiObject* arg0, s32 arg1, s32 arg2, u8* arg3, s32 arg4, s32 arg5, s32 arg6,
                                    s32 arg7, s32 arg8)
{
    u8  buf[0x40];
    u8* cur;
    s32 x;
    s32 y;
    s32 ret;
    s32 result;

    x      = arg1;
    y      = arg2;
    result = 1;
    cur    = arg3;
    if ((arg8 & 0xF) != 0) {
        arg7 += 1;
        y    -= arg8 & 0xF;
    }
    arg8 >>= 4;
    if (arg8 != 0) {
        cur = Text_SkipLines(arg3, arg8);
    }
    do {
        ret = Text_ParseLine(&cur, buf);
        _textDrawLine(arg0, x, y, buf, arg4, arg5, arg6);
        arg7 -= 1;
        if (arg7 <= 0) {
            result = 0;
            break;
        }
        x  = arg1;
        y += 0xF;
    } while (ret != -1);

    return result;
}

void Text_LoadClutImages(void)
{
    RECT rect;

    rect.x = 0x100;
    rect.y = 0xF3;
    rect.w = 0x40;
    rect.h = 1;
    LoadImage(&rect, D_80060910);

    rect.x = 0x3D0;
    rect.y = 0x1FF;
    rect.w = 0x30;
    rect.h = 1;
    LoadImage(&rect, D_800609B0);
}
