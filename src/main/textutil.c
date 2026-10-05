#include "main/text.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>

#include "types.h"

#include "main/gameflag.h"
#include "main/gameflag_types.h"
#include "text.h"
#include "main/ui_types.h"

#include "gameplay/collision.h"
#include "gameplay/gpu_image_upload.h"

/// Fill palettes (64 entries) for Text_LoadClutImages → (256, 243).
static u_long Text_FillClutPixels[];

/// Outline palettes (48 entries) for Text_LoadClutImages → (0x3D0, 0x1FF).
static u_long Text_OutlineClutPixels[];

/// How `_textParseLine` finished the line it copied.
enum {
    /// A line break was consumed. The cursor is at the next line, which may be empty.
    TEXT_LINE_BREAK = 1,
    /// NUL or a \\z command ended the text. No further line follows.
    TEXT_LINE_END = -1,
};

/// Placement defaults for large UI-text lines outside or inside a panel.
enum {
    TEXT_UI_LINE_ABSOLUTE_OT_INDEX     = 4,
    TEXT_UI_LINE_PANEL_BASELINE_OFFSET = 3,
};

static s32 _textParseLine(const u8** cursor, u8* line);

/// Initializes large UI metrics, packed RGB and byte-narrowed line selectors.
/// `request` is a TextDrawReq lvalue, evaluated repeatedly; all arguments must
/// be free of side effects. Coordinates and the OT index are left unchanged.
/// Expands to four statements and must be used within a braced statement block.
#define TEXT_SET_UI_LINE_STYLE(request, rgbValue, modeValue, alignmentValue) \
    (request).colorRgb   = (rgbValue);                                       \
    (request).glyphTable = TEXT_GLYPH_TABLE_LARGE;                           \
    (request).alignment  = (alignmentValue);                                 \
    (request).drawMode   = (modeValue)

static inline void _textDrawLine(const UiObject* object, s32 x, s32 y, const u8* text, u32 colorRgb, s32 drawMode, s32 alignment);

static void _textDrawUiLineVoid(const UiObject* object, s32 x, s32 y, const u8* text, u32 colorRgb, s32 drawMode, s32 alignment);

static s32 Text_DrawMultiLineScroll(UiObject* object, s32 arg1, s32 arg2, u8* arg3, s32 arg4, s32 arg5, s32 arg6,
                                    s32 arg7, s32 arg8);

/// Fill palettes (64 entries) for Text_LoadClutImages → (256, 243).
static u_long Text_FillClutPixels[] = {
#include "assets/text_clut0.inc"
};
/// Upload list for the fill palettes: one copy to (256, 243) and the end record.
///
/// The palette data carries the list form that gameplay's `gpuUploadImages`
/// walks, but the resident executable never reads it: `Text_LoadClutImages`
/// uploads the same words to the same rectangle itself.
static GpuImageUpload Text_FillClut[2] = {
    { GPU_IMAGE_UPLOAD_COPY, 0, { 0x100, 0xF3, 0x40, 1 }, Text_FillClutPixels },
    { GP_IMG_REC_END, 0, { 0, 0, 0, 0 }, NULL },
};

/// Outline palettes (48 entries) for Text_LoadClutImages → (0x3D0, 0x1FF).
static u_long Text_OutlineClutPixels[] = {
#include "assets/text_clut1.inc"
};
/// Upload list for the outline palettes: one copy to (256, 240) and the end record.
///
/// Never read, like `Text_FillClut`. Its destination is not where
/// `Text_LoadClutImages` uploads these words.
static GpuImageUpload Text_OutlineClut[2] = {
    { GPU_IMAGE_UPLOAD_COPY, 0, { 0x100, 0xF0, 0x30, 1 }, Text_OutlineClutPixels },
    { GP_IMG_REC_END, 0, { 0, 0, 0, 0 }, NULL },
};

GameFlagStageHeader* Gp_FlagBanks[] = {
    NULL,
    &GameFlag_AcropolisBanks[0].header,
    &GameFlag_DryfieldBanks[0].header,
    &GameFlag_DryfieldFullBanks[0].header,
    &GameFlag_ShelterBanks[0].header,
    &GameFlag_NeoArkBanks[0].header,
};

/// Copies one encoded UI-text line and advances the source cursor past it.
///
/// `cursor` addresses the caller's position in a readable encoded string and is
/// left on the first unconsumed byte. `line` receives a NUL-terminated copy and
/// is not retained. There is no capacity argument. Every caller supplies 64
/// bytes, so a line of 64 or more content bytes before its terminator overruns
/// that buffer.
///
/// Raw LF, CR, CR+LF, and a case-insensitive \\n command each store one NUL,
/// consume the break, and return `TEXT_LINE_BREAK`. For \\n and raw LF that
/// value is also the one-byte skip. NUL and a case-insensitive \\z command
/// store one NUL, consume the marker (both bytes of \\z), and return
/// `TEXT_LINE_END`. That negative result is not the skip distance: the NUL and
/// the \\z letter are each consumed by a separate one-byte advance. The cursor
/// is left just after the marker; callers stop, so bytes after \\z are not copied.
///
/// A doubled backslash stores one backslash and continues, so the next source
/// byte is read on a later iteration. Every other backslash command stores the
/// backslash and its letter; a later iteration copies any operand. Those bytes
/// stay encoded for drawing and measurement, which still read the line one byte
/// at a time.
///
/// A byte in 0x81..0x9F or 0xE0..0xFC, the Shift-JIS lead windows, is stored
/// with the immediately following byte. That byte is not tested as a terminator
/// or escape, need not be a valid trail byte, and must be readable. Every other
/// byte, including 0xA1..0xDF, is copied alone.
static s32 _textParseLine(const u8** cursor, u8* line)
{
    s32       lineEnd;
    const u8* source;
    const u8* atByte;
    u8        byte;
    u8        following;

    lineEnd = 0;
    do {
        source = *cursor;
        byte   = *source;
        if (byte == '\\') {
            *cursor = source + 1;
            switch (source[1]) {
                case 'Z':
                case 'z':
                    *line++ = '\0';
                    lineEnd = TEXT_LINE_END;
                    (*cursor)++;
                    break;
                case 'N':
                case 'n':
                    *line++  = '\0';
                    lineEnd  = TEXT_LINE_BREAK;
                    *cursor += lineEnd;
                    break;
                case '\\':
                    *line = **cursor;
                    line += 1;
                    (*cursor)++;
                    break;
                default:
                    *line++ = '\\';
                    *line   = **cursor;
                    line   += 1;
                    (*cursor)++;
                    break;
            }
        } else if (byte == '\0') {
            *line++ = '\0';
            lineEnd = TEXT_LINE_END;
            (*cursor)++;
        } else if (byte == '\n') {
            *line++  = '\0';
            lineEnd  = TEXT_LINE_BREAK;
            *cursor += lineEnd;
        } else if (byte == '\r') {
            // CR ends the line. A following LF is the same break.
            // The output pointer moves before that test; the extra step stores nothing.
            *line     = '\0';
            atByte    = *cursor;
            *cursor   = atByte + 1;
            following = atByte[1];
            line     += 1;
            if (following == '\n') {
                line   += 1;
                *cursor = atByte + 2;
            }
            lineEnd = TEXT_LINE_BREAK;
        } else if (((u8)(byte + 0x7F) < 0x1FU) || ((u8)(byte + 0x20) < 0x1DU)) {
            // Shift-JIS lead (0x81..0x9F or 0xE0..0xFC): copy the next byte untested.
            *line   = byte;
            atByte  = *cursor;
            *cursor = atByte + 1;
            line   += 1;
            *line   = atByte[1];
            line   += 1;
            (*cursor)++;
        } else {
            *line = byte;
            line += 1;
            (*cursor)++;
        }
    } while (lineEnd == 0);
    return lineEnd;
}

/// Draws one large UI-text line for the multiline drawers, discarding its final pen.
///
/// Borrows `object` and the read-only encoded bytes for this call. Placement,
/// hidden-panel suppression, RGB, byte-narrowed selectors and drawing resources
/// have the contract of `textDrawUiLine`. A NULL object selects absolute pixels.
static inline void _textDrawLine(const UiObject* object, s32 x, s32 y, const u8* text, u32 colorRgb, s32 drawMode, s32 alignment)
{
    TextDrawReq panelRequest;
    TextDrawReq absoluteRequest;
    s32         panelOtIndex;

    if (object != NULL) {
        if (object->panel.state != USER_INTERFACE_PANEL_HIDDEN) {
            panelRequest.x          = object->panel.contentOriginX.unsignedValue + x;
            panelRequest.y          = (object->panel.contentOriginY.unsignedValue + y) - TEXT_UI_LINE_PANEL_BASELINE_OFFSET;
            panelOtIndex            = object->panel.otIndex.signedValue;
            panelRequest.colorRgb   = colorRgb;
            panelRequest.otIndex    = panelOtIndex + 1;
            panelRequest.glyphTable = TEXT_GLYPH_TABLE_LARGE;
            panelRequest.alignment  = alignment;
            panelRequest.drawMode   = drawMode;
            textDrawString(&panelRequest, text);
        }
    } else {
        absoluteRequest.x       = x;
        absoluteRequest.y       = y;
        absoluteRequest.otIndex = TEXT_UI_LINE_ABSOLUTE_OT_INDEX;
        TEXT_SET_UI_LINE_STYLE(absoluteRequest, colorRgb, drawMode, alignment);
        textDrawString(&absoluteRequest, text);
    }
}

s32 Text_DrawMultiLine(UiObject* object, s32 arg1, s32 arg2, const u8* arg3, s32 arg4, s32 arg5, s32 arg6)
{
    u8        buf[0x40];
    const u8* cur;
    s32       x;
    s32       y;
    s32       ret;

    x   = arg1;
    y   = arg2;
    cur = arg3;
    do {
        ret = _textParseLine(&cur, buf);
        _textDrawLine(object, x, y, buf, arg4, arg5, arg6);
        x  = arg1;
        y += 0xF;
    } while (ret != TEXT_LINE_END);

    return 0;
}

s32 textMeasureLineWidth(const u8* text)
{
    TextDrawReq request;

    request.glyphTable = TEXT_GLYPH_TABLE_LARGE;
    request.x          = 0;
    request.y          = 0;
    request.otIndex    = 0;
    request.colorRgb   = 0;
    request.alignment  = TEXT_ALIGNMENT_RIGHT;
    request.drawMode   = TEXT_DRAW_FILL_ONLY;
    // Keep the alignment path's signed-halfword narrowing of the measured width.
    textAlignLine(&request, text);
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
    const u8*    cur;
    s32          ret;
    s32          tmp;
    s8           c;

    maxWidth   = 0;
    height     = maxWidth;
    requestPtr = &request;
    cur        = arg0;
    buf        = sp10;

    do {
        ret = _textParseLine(&cur, sp10);

        c                      = TEXT_GLYPH_TABLE_LARGE;
        request.x              = 0;
        request.y              = 0;
        request.otIndex        = 0;
        request.colorRgb       = 0;
        tmp                    = c;
        requestPtr->glyphTable = tmp;
        c                      = TEXT_ALIGNMENT_RIGHT;
        requestPtr->alignment  = c;
        request.drawMode       = TEXT_DRAW_FILL_ONLY;
        textAlignLine(requestPtr, buf);

        if (maxWidth < -request.x) {
            do {
            } while (0);
            maxWidth = -request.x;
        }
        height += 0xF;
        cur     = buf;
    } while (ret != TEXT_LINE_END);

    return (height << 16) | maxWidth;
}

s32 textDrawUiLine(const UiObject* object, s32 x, s32 y, const u8* text, u32 colorRgb, s32 drawMode, s32 alignment)
{
    TextDrawReq panelRequest;
    TextDrawReq absoluteRequest;
    s32         panelOtIndex;

    if (object != NULL) {
        if (object->panel.state == USER_INTERFACE_PANEL_HIDDEN) {
            return 0;
        }
    } else {
        absoluteRequest.x       = x;
        absoluteRequest.y       = y;
        absoluteRequest.otIndex = TEXT_UI_LINE_ABSOLUTE_OT_INDEX;
        TEXT_SET_UI_LINE_STYLE(absoluteRequest, colorRgb, drawMode, alignment);
        textDrawString(&absoluteRequest, text);
        return x;
    }
    // Translate panel content pixels to the glyph baseline and the panel's text OT entry.
    panelRequest.x = object->panel.contentOriginX.unsignedValue + x;
    panelRequest.y = (object->panel.contentOriginY.unsignedValue + y) - TEXT_UI_LINE_PANEL_BASELINE_OFFSET;
    panelOtIndex   = object->panel.otIndex.signedValue;
    TEXT_SET_UI_LINE_STYLE(panelRequest, colorRgb, drawMode, alignment);
    panelRequest.otIndex = panelOtIndex + 1;
    textDrawString(&panelRequest, text);
    return panelRequest.x - object->panel.contentOriginX.signedValue;
}

#undef TEXT_SET_UI_LINE_STYLE

/// Draws a large UI-text line through `textDrawUiLine` without returning its pen.
///
/// Arguments, narrowing, borrowed lifetimes and resource requirements are those
/// of `textDrawUiLine`.
static void _textDrawUiLineVoid(const UiObject* object, s32 x, s32 y, const u8* text, u32 colorRgb, s32 drawMode, s32 alignment)
{
    textDrawUiLine(object, x, y, text, colorRgb, drawMode, alignment);
}

static s32 Text_DrawMultiLineScroll(UiObject* object, s32 arg1, s32 arg2, u8* arg3, s32 arg4, s32 arg5, s32 arg6,
                                    s32 arg7, s32 arg8)
{
    u8        buf[0x40];
    const u8* cur;
    s32       x;
    s32       y;
    s32       ret;
    s32       result;

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
        cur = textSkipLines(arg3, arg8);
    }
    do {
        ret = _textParseLine(&cur, buf);
        _textDrawLine(object, x, y, buf, arg4, arg5, arg6);
        arg7 -= 1;
        if (arg7 <= 0) {
            result = 0;
            break;
        }
        x  = arg1;
        y += 0xF;
    } while (ret != TEXT_LINE_END);

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
