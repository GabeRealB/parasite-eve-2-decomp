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

/// One line of Text_DrawMultiLine: relative to obj's origin, or at an absolute
/// position when obj is NULL; skipped when obj is in mode 5.
static inline void _textDrawLine(UiObject* obj, s32 x, s32 y, u8* text, s32 arg4, s32 arg5, s32 arg6)
{
    TextDrawReq req;
    TextDrawReq req2;
    s32         temp;

    if (obj != NULL) {
        if (obj->mode != 5) {
            req.x          = obj->baseX + x;
            req.y          = (obj->baseY + y) - 3;
            temp           = obj->drawOrder;
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
        if (arg0->mode == 5) {
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
    sp10.x          = arg0->baseX + arg1;
    sp10.y          = (arg0->baseY + arg2) - 3;
    temp            = arg0->drawOrder;
    sp10.field_8    = arg4;
    sp10.glyphTable = 4;
    sp10.centerMode = arg6;
    sp10.field_E    = arg5;
    sp10.otIndex    = temp + 1;
    func_8002E53C(&sp10, arg3);
    return sp10.x - (s16)arg0->baseX;
}

static void Text_DrawPromptCompat(void* arg0, void* arg1, void* arg2, void* arg3, void* arg4, void* arg5, void* arg6)
{
    Text_DrawPrompt(arg0, arg1, arg2, arg3, arg4, arg5, arg6);
}

static s32 Text_DrawMultiLineScroll(UiObject* arg0, s32 arg1, s32 arg2, u8* arg3, s32 arg4, s32 arg5, s32 arg6,
                                    s32 arg7, s32 arg8)
{
    u8                 sp10[0x40];
    TextDrawReq        sp50[2];
    u8*                cur;
    s32                temp;
    s32                four;
    register UiObject* obj asm("s4");
    s32                x;
    s32                y;
    s32                result;
    s32                rem;
    TextDrawReq*       p;
    u8*                buf;
    s32                ret;
    u8*                a0tmp;
    s32                a1tmp;

    x      = arg1;
    y      = arg2;
    result = 1;
    rem    = arg7;
    a1tmp  = arg8;
    a0tmp  = arg3;
    cur    = a0tmp;

    if ((a1tmp & 0xF) != 0) {
        rem += result;
        y   -= a1tmp & 0xF;
    }
    a1tmp >>= 4;
    if (a1tmp != 0) {
        cur = Text_SkipLines(a0tmp, a1tmp);
    }
    obj  = arg0;
    p    = sp50;
    buf  = sp10;
    four = 4;

    do {
        ret = Text_ParseLine(&cur, sp10);
        if (obj != NULL) {
            if (obj->mode != 5) {
                sp50[0].x          = obj->baseX + x;
                sp50[0].y          = (obj->baseY + y) - 3;
                temp               = obj->drawOrder;
                sp50[0].field_8    = arg4;
                sp50[0].otIndex    = temp + 1;
                p->glyphTable      = 4;
                sp50[0].centerMode = (u8)arg6;
                sp50[0].field_E    = (u8)arg5;
                func_8002E53C(p, buf);
            }
        } else {
            sp50[1].x          = x;
            sp50[1].y          = y;
            p[1].otIndex       = four;
            sp50[1].field_8    = arg4;
            p[1].glyphTable    = four;
            sp50[1].centerMode = (u8)arg6;
            sp50[1].field_E    = (u8)arg5;
            func_8002E53C(&sp50[1], buf);
        }
        rem -= 1;
        if (rem <= 0) {
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
