#include "main/text.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>

#include "types.h"

#include "main/gameflag.h"
#include "main/gameflag_types.h"
#include "text.h"
#include "main/ui_types.h"

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
static u_long Text_FillClutPixels[];

/// Unreferenced.
static _TextClutRecord Text_FillClut;

/// Outline palettes (48 entries) for Text_LoadClutImages → (0x3D0, 0x1FF).
static u_long Text_OutlineClutPixels[];

/// Unreferenced.
static _TextClutRecord Text_OutlineClut;

static s32 Text_ParseLine(u8** arg0, u8* arg1);

/// One line of Text_DrawMultiLine or Text_DrawMultiLineScroll: relative to obj's origin, or at an absolute
/// position when obj is NULL; skipped when obj is in mode 5.
static inline void _textDrawLine(UiObject* obj, s32 x, s32 y, u8* text, s32 arg4, s32 arg5, s32 arg6);

static void Text_DrawPromptCompat(UiObject* object, s32 arg1, s32 arg2, u8* arg3, s32 arg4, s32 arg5, s32 arg6);

static s32 Text_DrawMultiLineScroll(UiObject* object, s32 arg1, s32 arg2, u8* arg3, s32 arg4, s32 arg5, s32 arg6,
                                    s32 arg7, s32 arg8);

/// Fill palettes (64 entries) for Text_LoadClutImages → (256, 243).
static u_long Text_FillClutPixels[] = {
#include "assets/text_clut0.inc"
};
/// Unreferenced.
static _TextClutRecord Text_FillClut = { 0, { 0x100, 0xF3, 0x40, 1 }, Text_FillClutPixels, 0xFF, { 0 } };

/// Outline palettes (48 entries) for Text_LoadClutImages → (0x3D0, 0x1FF).
static u_long Text_OutlineClutPixels[] = {
#include "assets/text_clut1.inc"
};
/// Unreferenced.
static _TextClutRecord Text_OutlineClut = { 0, { 0x100, 0xF0, 0x30, 1 }, Text_OutlineClutPixels, 0xFF, { 0 } };

GpFlagBank* Gp_FlagBanks[] = {
    NULL,
    &GameFlag_AcropolisBanks[0].header,
    &GameFlag_DryfieldBanks[0].header,
    &GameFlag_DryfieldFullBanks[0].header,
    &GameFlag_ShelterBanks[0].header,
    &GameFlag_NeoArkBanks[0].header,
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
        if (obj->panel.state != USER_INTERFACE_PANEL_HIDDEN) {
            req.x          = obj->panel.contentOriginX.unsignedValue + x;
            req.y          = (obj->panel.contentOriginY.unsignedValue + y) - 3;
            temp           = obj->panel.otIndex.signedValue;
            req.colorRgb   = arg4;
            req.otIndex    = temp + 1;
            req.glyphTable = TEXT_GLYPH_TABLE_LARGE;
            req.alignment  = arg6;
            req.drawMode   = arg5;
            Text_DrawString(&req, text);
        }
    } else {
        req2.x          = x;
        req2.y          = y;
        req2.otIndex    = 4;
        req2.colorRgb   = arg4;
        req2.glyphTable = TEXT_GLYPH_TABLE_LARGE;
        req2.alignment  = arg6;
        req2.drawMode   = arg5;
        Text_DrawString(&req2, text);
    }
}

s32 Text_DrawMultiLine(UiObject* object, s32 arg1, s32 arg2, u8* arg3, s32 arg4, s32 arg5, s32 arg6)
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
        _textDrawLine(object, x, y, buf, arg4, arg5, arg6);
        x  = arg1;
        y += 0xF;
    } while (ret != -1);

    return 0;
}

s32 Text_MeasureWidth(u8* arg0)
{
    TextDrawReq request;

    request.glyphTable = TEXT_GLYPH_TABLE_LARGE;
    request.x          = 0;
    request.y          = 0;
    request.otIndex    = 0;
    request.colorRgb   = 0;
    request.alignment  = TEXT_ALIGNMENT_RIGHT;
    request.drawMode   = TEXT_DRAW_QUEUED;
    Text_MeasureAndCenter(&request, arg0);
    return -request.x;
}

s32 Text_MeasureMultiLine(u8* arg0)
{
    u8           sp10[0x40];
    TextDrawReq  request;
    s32          maxWidth;
    s32          height;
    TextDrawReq* requestPtr;
    u8*          buf;
    u8*          cur;
    s32          ret;
    s32          tmp;
    s8           c;

    maxWidth   = 0;
    height     = maxWidth;
    requestPtr = &request;
    cur        = arg0;
    buf        = sp10;

    do {
        ret = Text_ParseLine(&cur, sp10);

        c                      = TEXT_GLYPH_TABLE_LARGE;
        request.x              = 0;
        request.y              = 0;
        request.otIndex        = 0;
        request.colorRgb       = 0;
        tmp                    = c;
        requestPtr->glyphTable = tmp;
        c                      = TEXT_ALIGNMENT_RIGHT;
        requestPtr->alignment  = c;
        request.drawMode       = TEXT_DRAW_QUEUED;
        Text_MeasureAndCenter(requestPtr, buf);

        if (maxWidth < -request.x) {
            do {
            } while (0);
            maxWidth = -request.x;
        }
        height += 0xF;
        cur     = buf;
    } while (ret != -1);

    return (height << 16) | maxWidth;
}

s32 Text_DrawPrompt(UiObject* object, s32 arg1, s32 arg2, u8* arg3, s32 arg4, s32 arg5, s32 arg6)
{
    TextDrawReq panelRequest;
    TextDrawReq absoluteRequest;
    s32         temp;

    if (object != NULL) {
        if (object->panel.state == USER_INTERFACE_PANEL_HIDDEN) {
            return 0;
        }
    } else {
        absoluteRequest.x          = arg1;
        absoluteRequest.y          = arg2;
        absoluteRequest.otIndex    = 4;
        absoluteRequest.colorRgb   = arg4;
        absoluteRequest.glyphTable = TEXT_GLYPH_TABLE_LARGE;
        absoluteRequest.alignment  = arg6;
        absoluteRequest.drawMode   = arg5;
        Text_DrawString(&absoluteRequest, arg3);
        return arg1;
    }
    panelRequest.x          = object->panel.contentOriginX.unsignedValue + arg1;
    panelRequest.y          = (object->panel.contentOriginY.unsignedValue + arg2) - 3;
    temp                    = object->panel.otIndex.signedValue;
    panelRequest.colorRgb   = arg4;
    panelRequest.glyphTable = TEXT_GLYPH_TABLE_LARGE;
    panelRequest.alignment  = arg6;
    panelRequest.drawMode   = arg5;
    panelRequest.otIndex    = temp + 1;
    Text_DrawString(&panelRequest, arg3);
    return panelRequest.x - object->panel.contentOriginX.signedValue;
}

static void Text_DrawPromptCompat(UiObject* object, s32 arg1, s32 arg2, u8* arg3, s32 arg4, s32 arg5, s32 arg6)
{
    Text_DrawPrompt(object, arg1, arg2, arg3, arg4, arg5, arg6);
}

static s32 Text_DrawMultiLineScroll(UiObject* object, s32 arg1, s32 arg2, u8* arg3, s32 arg4, s32 arg5, s32 arg6,
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
        _textDrawLine(object, x, y, buf, arg4, arg5, arg6);
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
    LoadImage(&rect, Text_FillClutPixels);

    rect.x = 0x3D0;
    rect.y = 0x1FF;
    rect.w = 0x30;
    rect.h = 1;
    LoadImage(&rect, Text_OutlineClutPixels);
}
