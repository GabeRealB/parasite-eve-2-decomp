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

/// Byte encodings of the font's three pair-kerning classes.
enum {
    /// Glyph-edge class that preserves normal spacing with all three classes.
    ///
    /// Also represents the missing previous glyph when starting a string.
    FONT_KERNING_CLASS_NEUTRAL  = 0,
    FONT_KERNING_CLASS_POSITIVE = 1,
    FONT_KERNING_CLASS_NEGATIVE = 0xFF,
};

/// Page-local texture V offsets, encoded modulo 256 in the request's signed byte.
enum {
    /// Medium-face texture V translation in page-local texels.
    ///
    /// Adds 38 to `_FontGlyph::v` to address the medium glyphs in the font
    /// texture page. Stored unchanged in `TextDrawReq::vBias`; the sprite's
    /// unsigned V byte stores the sum modulo 256.
    /// Initial `TEXT_GLYPH_TABLE_MEDIUM` selection and an inline \sM command
    /// (either letter's case) set this bias. The inline command also selects
    /// medium metrics without changing `glyphTable`, so measurement and pair
    /// tightening still follow the request's initial selector.
    TEXT_GLYPH_V_BIAS_MEDIUM = 38,
    /// Small-face texture V translation in page-local texels.
    ///
    /// Zero leaves `_FontGlyph::v` unchanged in the sprite's unsigned V byte.
    /// Drawing stores it in `TextDrawReq::vBias` for initial
    /// `TEXT_GLYPH_TABLE_SMALL` selection or an inline \sS command (either
    /// letter's case). The command selects small metrics without changing
    /// `glyphTable`, so alignment measurement and pair tightening retain the
    /// initial selector. Glyph bytes must be in 0x20..0x7A while this face is
    /// active; each sprite stores the glyph V plus this bias modulo 256.
    TEXT_GLYPH_V_BIAS_SMALL = 0,
    /// Large-face texture V translation in page-local texels.
    ///
    /// Stored in `TextDrawReq::vBias` as -128 (byte 0x80). Adding it to a
    /// glyph's V and storing the result in the sprite's unsigned byte is
    /// equivalent to adding 128 modulo 256. Initial drawing selects it for
    /// every glyph-table selector except medium and small; an inline \sL
    /// command (either letter's case) also selects it without changing
    /// `glyphTable`, so measurement and pair tightening keep that selector.
    TEXT_GLYPH_V_BIAS_LARGE = -128,
};

/// Record count of the medium UI face.
///
/// One record per character byte from ' ' through 0xFF, indexed as
/// `byte - ' '`, so every such byte is in range. The large face covers the
/// same bytes; the small face stops at 0x7A and does not use this count.
/// 224 records occupy 0xA80 bytes.
enum { FONT_GLYPH_MEDIUM_COUNT = 0x100 - ' ' };

/// Record count of the large UI face.
///
/// One record per character byte from ' ' through 0xFF, indexed as
/// `byte - ' '`, so every such byte is in range. The medium face covers the
/// same bytes under `FONT_GLYPH_MEDIUM_COUNT`. The small face stops at 0x7A
/// and does not use this count.
/// 224 records occupy 0xA80 bytes.
enum { FONT_GLYPH_LARGE_COUNT = 0x100 - ' ' };

/// Number of glyph-metric records in the small UI font.
///
/// Character bytes ' ' through 'z' (0x20..0x7A) map to indices 0..90 by
/// subtracting ' '. All records, including space, are counted; the 91 records
/// occupy 0x444 bytes. Drawing and measurement rely on glyph bytes falling
/// within this range.
enum { FONT_GLYPH_SMALL_COUNT = 'z' - ' ' + 1 };

/// Texture bounds and pen metrics for one encoded UI-font character.
///
/// Tables are indexed by the character byte minus ' ': `_gFontGlyphsMedium` and
/// `_gFontGlyphsLarge` cover 0x20..0xFF, and `_gFontGlyphsSmall` covers 0x20..0x7A.
/// The texture origin is relative to a 4bpp page; the selected font supplies
/// an additional V bias. Adjacent right/left kerning classes combine modulo
/// 256, allowing pair tightening when equal and non-neutral.
typedef struct {
    u8 u;                 // Texture origin X, in page-local texels.
    u8 v;                 // Texture origin Y before the font's V bias, in texels.
    u8 widthMinusOne;     // Sprite width minus one, in texels.
    u8 heightMinusOne;    // Sprite height minus one, in texels.
    s8 xOffset;           // Sprite origin offset from the pen X, in pixels.
    s8 yOffset;           // Last sprite row's offset from the pen baseline, in pixels.
    s8 advanceExtraX;     // Extra pixels in pen advance after xOffset + widthMinusOne; omitted for the final measured glyph.
    s8 advanceY;          // Pen baseline advance after this glyph, in pixels.
    u8 leftKerningClass;  // Left-side class (0 neutral, 1 positive, 255 negative).
    u8 rightKerningClass; // Right-side class with the same byte encodings.
    u8 field_A[2];        // Unread bytes; purpose unproven.
} _FontGlyph;
STATIC_ASSERT_SIZEOF(_FontGlyph, 0xC);
STATIC_ASSERT(FONT_GLYPH_MEDIUM_COUNT * sizeof(_FontGlyph) == 0xA80, fontGlyphsMediumBytes);
STATIC_ASSERT(FONT_GLYPH_LARGE_COUNT * sizeof(_FontGlyph) == 0xA80, fontGlyphsLargeBytes);
STATIC_ASSERT(FONT_GLYPH_SMALL_COUNT * sizeof(_FontGlyph) == 0x444, fontGlyphsSmallBytes);

/// Immediate-mode SPRT scratch used by Text_DrawGlyphImmediate.
static SPRT D_80071710;

static DR_TPAGE D_80071728;

static TaskDesc D_8005EDA0[];

static _FontGlyph _gFontGlyphsMedium[FONT_GLYPH_MEDIUM_COUNT];

static _FontGlyph _gFontGlyphsLarge[FONT_GLYPH_LARGE_COUNT];

static _FontGlyph _gFontGlyphsSmall[FONT_GLYPH_SMALL_COUNT];

static UiObjectDesc Ui_OverlayLoadingDesc[];

/// Overflow and zero texts of the number formatters.
static const char Text_MaxEightDigits[];

static const char Text_ZeroDigit[];

static const char Text_MaxNineDigits[];

void func_807011D8(Task* arg0);

void func_80701400(Task* arg0);

static s32 _textMeasureLineWidth(const TextDrawReq* request, const u8* text, const _FontGlyph* glyphTable);

static void Text_DrawGlyphDualSprtA(TextDrawReq* request, const _FontGlyph* glyph, s32 arg2);

static void Text_DrawGlyphDualSprt(TextDrawReq* request, const _FontGlyph* glyph, s32 arg2);

static void Text_DrawGlyphDualSprtTpage(TextDrawReq* request, const _FontGlyph* glyph, s32 arg2);

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

static void Text_DrawGlyphImmediate(TextDrawReq* request, const _FontGlyph* glyph, s32 arg2);

static void Text_DrawGlyphQueued(TextDrawReq* request, const _FontGlyph* glyph, s32 arg2);

static void Text_DrawGlyphOt(TextDrawReq* request, const _FontGlyph* glyph, s32 unusedColor);

static void Text_UiTaskCallback(Task* task);

static void Text_BootTask(Task* task);

static const char Text_MaxEightDigits[];
static const char Text_ZeroDigit[];
static const char Text_MaxNineDigits[];

static TaskDesc D_8005EDA0[] = {
    { { { TASK_BODY_NONE, 0xC0 } }, taskNoopCallback },
    { { { TASK_BODY_NONE, 0xC0 } }, taskCountdownCallback },
    { { { TASK_BODY_NONE, 0xC0 } }, Title_Dispatch },
    { { { TASK_BODY_NONE, 0xC0 } }, GameFlow_StateByField34 },
    { { { TASK_BODY_NONE, 0xC0 } }, GameFlow_DispatchTable5 },
    { { { TASK_BODY_NONE, 0xC0 } }, Text_UiTaskCallback },
    { { { TASK_BODY_NONE, 0xC0 } }, Title_ExitTask },
    { { { TASK_BODY_NONE, 0xC0 } }, taskKill },
    { { { TASK_BODY_NONE, 0x0 } }, NULL },
    { { { TASK_BODY_NONE, 0x18 } }, GameFlow_DispatchTable },
    { { { TASK_BODY_NONE, 0x10 } }, Mc_DispatchStateTable },
    { { { TASK_BODY_NONE, 0x10 } }, Mc_DispatchStateTable26 },
    { { { TASK_BODY_NONE, 0xC0 } }, McMenu_NoOpTask },
    { { { TASK_BODY_NONE, 0x10 } }, Text_BootTask },
    { { { TASK_BODY_COORD, 0x2F } }, func_800A8654 },
    { { { TASK_BODY_NONE, 0x2F } }, Gp_ApplyViewTask },
    { { { TASK_BODY_NONE, 0x40 } }, func_800AD50C },
    { { { TASK_BODY_NONE, 0x28 } }, func_800AC0F0 },
    { { { TASK_BODY_NONE, 0x10 } }, Mc_DispatchStateTable },
    { { { TASK_BODY_NONE, 0x10 } }, Mc_DispatchStateTable26 },
    { { { TASK_BODY_NONE, 0x1F } }, func_800AEE8C },
    { { { TASK_BODY_NONE, 0xC0 } }, taskKill },
    { { { TASK_BODY_NONE, 0x30 } }, Gp_ViewGateTask },
    { { { TASK_BODY_NONE, 0x2F } }, Gp_AllocSprtListsTask },
    { { { TASK_BODY_NONE, 0xC0 } }, Stage_TaskExit },
    { { { TASK_BODY_NONE, 0xC0 } }, func_807011D8 },
    { { { TASK_BODY_NONE, 0xE0 } }, Gp_DrawDisp2dOt },
    { { { TASK_BODY_NONE, 0xD0 } }, func_800AD5B8 },
    { { { TASK_BODY_NONE, 0x2F } }, Gp_LoadStateTask },
    { { { TASK_BODY_NONE, 0x18 } }, func_800A77B4 },
    { { { TASK_BODY_NONE, 0xF8 } }, Gp_LoadWaitDispatch },
    { { { TASK_BODY_NONE, 0x10 } }, Boot_LoadInitialFile },
    { { { TASK_BODY_NONE, 0x10 } }, Boot_LoadTask },
    { { { TASK_BODY_NONE, 0x2F } }, Gp_FlashWhiteTask },
    { { { TASK_BODY_NONE, 0xF8 } }, NULL },
    { { { TASK_BODY_NONE, 0xC0 } }, func_80701400 },
    { { { TASK_BODY_NONE, 0x2F } }, NULL },
    { { { TASK_BODY_NONE, 0xF8 } }, Gp_CommitSpawnLoc },
    { { { TASK_BODY_NONE, 0xF8 } }, Gp_SetupSprtDisplay },
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

/// Medium UI-font glyph metrics, one record per character byte from ' ' through 0xFF.
///
/// `Text_DrawString` and `Text_MeasureAndCenter` select this face when
/// `glyphTable` is `TEXT_GLYPH_TABLE_MEDIUM`. Drawing also selects it for an
/// `\sM` command, in either letter's case, and adds `TEXT_GLYPH_V_BIAS_MEDIUM`
/// to each record's texture V. The initializer is the embedded `font_glyphs0`
/// catalogue blob. Bytes below space are not records in this face.
static _FontGlyph _gFontGlyphsMedium[FONT_GLYPH_MEDIUM_COUNT] = {
#include "assets/font_glyphs0.inc"
};

/// Large UI-font glyph metrics, one record per character byte from ' ' through 0xFF.
///
/// `Text_DrawString` and `Text_MeasureAndCenter` select this face when
/// `glyphTable` is neither `TEXT_GLYPH_TABLE_MEDIUM` nor
/// `TEXT_GLYPH_TABLE_SMALL`. Named callers use `TEXT_GLYPH_TABLE_LARGE` and
/// `TEXT_GLYPH_TABLE_LARGE_ALTERNATE`; any other selector takes this face too.
/// Drawing also selects it for an `\sL` command, in either letter's case, and
/// adds `TEXT_GLYPH_V_BIAS_LARGE` to each record's texture V. The initializer
/// is the embedded `font_glyphs1` catalogue blob. Bytes below space are not
/// records in this face.
static _FontGlyph _gFontGlyphsLarge[FONT_GLYPH_LARGE_COUNT] = {
#include "assets/font_glyphs1.inc"
};

/// Small UI-font glyph metrics, one record per character byte from ' ' through 0x7A.
///
/// `Text_DrawString` and `Text_MeasureAndCenter` select this face when
/// `glyphTable` is `TEXT_GLYPH_TABLE_SMALL`. Drawing also selects it for an
/// `\sS` command, in either letter's case, and adds `TEXT_GLYPH_V_BIAS_SMALL`
/// to each record's texture V. The initializer is the embedded `font_glyphs2`
/// catalogue blob. Bytes outside 0x20..0x7A are not records in this face.
static _FontGlyph _gFontGlyphsSmall[FONT_GLYPH_SMALL_COUNT] = {
#include "assets/font_glyphs2.inc"
};

static UiObjectDesc Ui_OverlayLoadingDesc[] = {
    { 2, 0xFF70, 0xFF98, 0x120, 0x90, 0x38, 0, 0, 0xC0, Ui_WaitCdThenOverlay, 0 },
};

void taskNoopCallback(Task* unusedTask)
{
}

/// Tightens the horizontal pixel lvalue `x` by the pair-kerning gap before `glyph`.
///
/// `previousRightClass` is the previous glyph's right-edge class, one of
/// `FONT_KERNING_CLASS_*`, or neutral when no glyph precedes this one.
/// `glyph` points at the next `_FontGlyph`; only `leftKerningClass` is read.
/// `request` points at the `TextDrawReq`; only `glyphTable` is read. Each
/// argument is evaluated once. `x` must be a modifiable lvalue in
/// draw-environment pixels. The replacement is a statement and captures nothing.
///
/// The class bytes are 0, 1 and 255. The gap tightens when the low 8 bits of
/// `previousRightClass + leftKerningClass + 1` are at least 3, which for these
/// bytes is exactly an equal non-neutral pair (1 with 1, or 255 with 255).
/// Read as signed codes +1 and -1, those pairs sum to +2 and -2; every other
/// pair sums to -1, 0 or 1 and keeps its spacing. `TEXT_GLYPH_TABLE_SMALL`
/// pulls `x` in by one pixel, and every other `glyphTable` value pulls it in
/// by two. The count follows `glyphTable`, not the metrics table that
/// produced `glyph`.
#define TEXT_APPLY_KERNING(x, previousRightClass, glyph, request)              \
    do {                                                                       \
        if ((u8)((previousRightClass) + (glyph)->leftKerningClass + 1) >= 3) { \
            if ((request)->glyphTable == TEXT_GLYPH_TABLE_SMALL) {             \
                (x) -= 1;                                                      \
            } else {                                                           \
                (x) -= 2;                                                      \
            }                                                                  \
        }                                                                      \
    } while (0)

/// Measures one encoded UI-text line for horizontal alignment, in pixels.
///
/// Borrows `request`, `text` and `glyphTable` for this call without modifying them.
/// Only `request->glyphTable` is read, selecting one-pixel pair tightening for the
/// small face and two pixels otherwise. The supplied metrics stay fixed even
/// across inline font commands; color, style and position commands are skipped.
/// NUL, LF and a case-insensitive \n command end the measurement; CR does not.
/// The result is the final glyph's rightmost-pixel X relative to the starting pen,
/// including pair kerning and omitting its extra pen advance. With no measured
/// glyph it is minus `glyphTable[0].advanceExtraX` (-4 for all resident faces).
///
/// `text` must be readable through the line terminator. Each \B, \C, \D, \S,
/// \U or \W command (either case) needs a readable operand byte followed by
/// another readable byte. Every byte that reaches glyph indexing must be in
/// 0x20..0xFF for medium/large metrics or 0x20..0x7A for small metrics; there is
/// no range check. An unrecognized escape drops its backslash and measures the
/// following byte; consecutive backslashes continue the command scan.
static s32 _textMeasureLineWidth(const TextDrawReq* request, const u8* text, const _FontGlyph* glyphTable)
{
    /// Byte count after the backslash in a UI-text command with one operand.
    ///
    /// B/C/D/S/U/W (either case) each encode a command letter and one operand.
    /// B sets X, D/U move Y, and C/S/W select color, font and draw mode.
    /// Measurement skips both bytes regardless of the operand's value. The
    /// cursor starts at the command letter and reads the byte after this payload;
    /// that byte must be readable even when the skipped operand is NUL.
    enum { TEXT_LINE_COMMAND_PAYLOAD_BYTES = 2 };

    /// Skips adjacent backslash commands during UI-text line measurement.
    ///
    /// `cursor` must be a simple const u8* lvalue initially pointing at '\\';
    /// `escapeByte` is the already-read u8 prefix byte and must remain '\\'
    /// throughout the expansion. `endLine` must be a distinct simple flag lvalue,
    /// initially false; it is set to true on N/n, NUL or LF and never cleared.
    /// CR does not end the scan. Arguments can be evaluated repeatedly and must
    /// have no side effects beyond cursor/flag updates. The statement captures
    /// the enclosing function's `TEXT_LINE_COMMAND_PAYLOAD_BYTES` constant.
    ///
    /// B/C/D/S/U/W (either case) skip the command letter and one operand without
    /// applying their effects or inspecting the operand. Each skipped payload
    /// and the byte after it must be readable, even if the operand is NUL.
    /// The cursor remains at N/n or a terminator on exit, otherwise at the next
    /// byte for glyph indexing; unknown commands discard only the backslash. '\\'
    /// continues the scan, including consecutive backslashes. No bounds check
    /// or text-buffer write is performed.
#define TEXT_SKIP_LINE_WIDTH_COMMANDS(cursor, escapeByte, endLine) \
    do {                                                           \
        do {                                                       \
            (cursor)++;                                            \
            switch (*(cursor)) {                                   \
                case 'B':                                          \
                case 'C':                                          \
                case 'D':                                          \
                case 'S':                                          \
                case 'U':                                          \
                case 'W':                                          \
                case 'b':                                          \
                case 'c':                                          \
                case 'd':                                          \
                case 's':                                          \
                case 'u':                                          \
                case 'w':                                          \
                    (cursor) += TEXT_LINE_COMMAND_PAYLOAD_BYTES;   \
                    break;                                         \
                case 'N':                                          \
                case 'n':                                          \
                    (endLine) = true;                              \
                    break;                                         \
            }                                                      \
            if (*(cursor) == '\0' || *(cursor) == '\n') {          \
                (endLine) = true;                                  \
            }                                                      \
        } while (*(cursor) == (escapeByte));                       \
    } while (0)

    s32               width;
    const _FontGlyph* glyph;
    u8                previousRightKerningClass;
    s32               endLine;
    u8                leadingByte;
    s32               glyphIndex;

    width                     = 0;
    glyph                     = glyphTable;
    previousRightKerningClass = FONT_KERNING_CLASS_NEUTRAL;
    for (leadingByte = *text; leadingByte != '\0'; leadingByte = *text) {
        if (leadingByte == '\n') {
            break;
        }
        // Consume adjacent commands without applying their drawing side effects.
        endLine = false;
        if (leadingByte == '\\') {
            TEXT_SKIP_LINE_WIDTH_COMMANDS(text, leadingByte, endLine);
#undef TEXT_SKIP_LINE_WIDTH_COMMANDS
        }
        if (endLine) {
            break;
        }
        glyphIndex = *text - ' ';
        glyph      = &glyphTable[glyphIndex];
        TEXT_APPLY_KERNING(width, previousRightKerningClass, glyph, request);
        text++;
        previousRightKerningClass = glyph->rightKerningClass;
        width                    += glyph->widthMinusOne + glyph->advanceExtraX + glyph->xOffset;
    }
    // Alignment omits trailing spacing, retaining the space record for empty lines.
    return width - glyph->advanceExtraX;
}

static void Text_DrawGlyphDualSprtA(TextDrawReq* request, const _FontGlyph* glyph, s32 arg2)
{
    SPRT* p;
    SPRT* p2;
    s32   temp;

    p                              = gGpuPrimCursor;
    gGpuPrimCursor                 = p + 1;
    GPU_PRIMITIVE_COLOR_WORD(p, 0) = arg2;
    setlen(p, 4);
    setcode(p, 0x66);

    p2             = gGpuPrimCursor;
    gGpuPrimCursor = p2 + 1;
    setlen(p2, 4);
    setcode(p2, 0x67);

    p2->x0 = p->x0 = request->x + glyph->xOffset;
    p2->y0 = p->y0 = (request->y - glyph->heightMinusOne) + glyph->yOffset;
    p2->u0 = p->u0 = glyph->u;
    p2->v0 = p->v0 = glyph->v + request->vBias;
    p2->w = p->w = glyph->widthMinusOne + 1;
    temp         = glyph->heightMinusOne;
    p2->h = p->h = temp + 1;
    p2->clut     = 0x7FFE;
    p->clut      = 0x7FFD;

    addPrim(gGpuCurrentOt + request->otIndex + 1, p2);
    addPrim(gGpuCurrentOt + request->otIndex, p);
}

static void Text_DrawGlyphDualSprt(TextDrawReq* request, const _FontGlyph* glyph, s32 arg2)
{
    SPRT* p;
    SPRT* p2;
    s32   temp;

    p                              = gGpuPrimCursor;
    gGpuPrimCursor                 = p + 1;
    GPU_PRIMITIVE_COLOR_WORD(p, 0) = arg2;
    setlen(p, 4);
    setcode(p, 0x64);

    p2             = gGpuPrimCursor;
    gGpuPrimCursor = p2 + 1;
    setlen(p2, 4);
    setcode(p2, 0x67);

    p2->x0 = p->x0 = request->x + glyph->xOffset;
    p2->y0 = p->y0 = (request->y - glyph->heightMinusOne) + glyph->yOffset;
    p2->u0 = p->u0 = glyph->u;
    p2->v0 = p->v0 = glyph->v + request->vBias;
    p2->w = p->w = glyph->widthMinusOne + 1;
    temp         = glyph->heightMinusOne;
    p2->h = p->h = temp + 1;
    p2->clut     = 0x7FFF;
    p->clut      = 0x7FFD;

    addPrim(gGpuCurrentOt + request->otIndex + 1, p2);
    addPrim(gGpuCurrentOt + request->otIndex, p);
}

static void Text_DrawGlyphDualSprtTpage(TextDrawReq* request, const _FontGlyph* glyph, s32 arg2)
{
    SPRT*     p;
    SPRT*     p2;
    DR_TPAGE* dr;
    s32       temp;

    p                              = gGpuPrimCursor;
    gGpuPrimCursor                 = p + 1;
    GPU_PRIMITIVE_COLOR_WORD(p, 0) = arg2;
    setlen(p, 4);
    setcode(p, 0x64);

    p2             = gGpuPrimCursor;
    gGpuPrimCursor = p2 + 1;
    setlen(p2, 4);
    setcode(p2, 0x67);

    p2->x0 = p->x0 = request->x + glyph->xOffset;
    p2->y0 = p->y0 = (request->y - glyph->heightMinusOne) + glyph->yOffset;
    p2->u0 = p->u0 = glyph->u;
    p2->v0 = p->v0 = glyph->v + request->vBias;
    p2->w = p->w = glyph->widthMinusOne + 1;
    temp         = glyph->heightMinusOne;
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
    u8*               ptr;
    const _FontGlyph* table;
    const _FontGlyph* glyph;
    void              (*draw)(TextDrawReq*, const _FontGlyph*, s32);
    s32               color;
    s32               previousRightKerningClass;
    s32               width;
    u8                c;
    s32               end_flag;
    s32               idx;
    s32               temp;
    DR_TPAGE*         dr;

    ptr                       = text;
    previousRightKerningClass = FONT_KERNING_CLASS_NEUTRAL;
    color                     = request->colorRgb;
    request->vBias            = 0;
    switch (request->glyphTable) {
        case TEXT_GLYPH_TABLE_MEDIUM:
            table          = _gFontGlyphsMedium;
            request->vBias = TEXT_GLYPH_V_BIAS_MEDIUM;
            break;
        case TEXT_GLYPH_TABLE_SMALL:
            table          = _gFontGlyphsSmall;
            request->vBias = TEXT_GLYPH_V_BIAS_SMALL;
            break;
        default:
            table          = _gFontGlyphsLarge;
            request->vBias = TEXT_GLYPH_V_BIAS_LARGE;
            break;
    }
    switch (request->alignment) {
        case TEXT_ALIGNMENT_CENTER:
            width       = _textMeasureLineWidth(request, text, table);
            request->x -= width >> 1;
            break;
        case TEXT_ALIGNMENT_RIGHT:
            width       = _textMeasureLineWidth(request, text, table);
            request->x -= width;
            break;
    }
    switch (request->drawMode) {
        case TEXT_DRAW_OUTLINED:
            draw = Text_DrawGlyphDualSprt;
            break;
        case TEXT_DRAW_OUTLINED_SINGLE_ENTRY:
            draw = Text_DrawGlyphDualSprtTpage;
            break;
        case TEXT_DRAW_TRANSLUCENT_OUTLINED:
            draw = Text_DrawGlyphDualSprtA;
            break;
        case TEXT_DRAW_OUTLINE_ONLY:
            draw = Text_DrawGlyphOt;
            break;
        case TEXT_DRAW_IMMEDIATE:
            dr = &D_80071728;
            setlen(dr, 1);
            dr->code[0] = 0xE100023F;
            DrawPrim(dr);
            draw = Text_DrawGlyphImmediate;
            break;
        case TEXT_DRAW_FILL_ONLY:
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
                                table          = _gFontGlyphsSmall;
                                request->vBias = TEXT_GLYPH_V_BIAS_SMALL;
                                break;
                            case 'M':
                            case 'm':
                                table          = _gFontGlyphsMedium;
                                request->vBias = TEXT_GLYPH_V_BIAS_MEDIUM;
                                break;
                            case 'L':
                            case 'l':
                                table          = _gFontGlyphsLarge;
                                request->vBias = TEXT_GLYPH_V_BIAS_LARGE;
                                break;
                        }
                        ptr++;
                        break;
                    case 'W':
                    case 'w':
                        ptr++;
                        switch (*ptr) {
                            case '0':
                                request->drawMode = TEXT_DRAW_TRANSLUCENT_OUTLINED;
                                draw              = Text_DrawGlyphDualSprtA;
                                break;
                            case '1':
                                request->drawMode = TEXT_DRAW_OUTLINED;
                                draw              = Text_DrawGlyphDualSprt;
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
        glyph = &table[idx];
        temp  = previousRightKerningClass + glyph->leftKerningClass + 1;
        if ((u8)temp >= 3) {
            if (request->glyphTable == TEXT_GLYPH_TABLE_SMALL) {
                request->x -= 1;
            } else {
                request->x -= 2;
            }
        }
        previousRightKerningClass = glyph->rightKerningClass;
        draw(request, glyph, color);
        ptr++;
        request->x += glyph->widthMinusOne + glyph->advanceExtraX + glyph->xOffset;
        request->y += glyph->advanceY;
    }
    if (request->drawMode == TEXT_DRAW_OUTLINED || request->drawMode == TEXT_DRAW_TRANSLUCENT_OUTLINED) {
        dr             = gGpuPrimCursor;
        gGpuPrimCursor = dr + 1;
        dr->code[0]    = 0xE100025F;
        setlen(dr, 1);
        addPrim(gGpuCurrentOt + request->otIndex + 1, dr);
    }
    if (request->drawMode != TEXT_DRAW_IMMEDIATE) {
        if (request->drawMode == TEXT_DRAW_OUTLINE_ONLY) {
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
    const _FontGlyph* table;
    s32               width;

    switch (request->glyphTable) {
        case TEXT_GLYPH_TABLE_MEDIUM:
            table = _gFontGlyphsMedium;
            break;
        case TEXT_GLYPH_TABLE_SMALL:
            table = _gFontGlyphsSmall;
            break;
        default:
            table = _gFontGlyphsLarge;
            break;
    }

    switch (request->alignment) {
        case TEXT_ALIGNMENT_CENTER:
            width       = _textMeasureLineWidth(request, arg1, table);
            request->x -= width >> 1;
            break;
        case TEXT_ALIGNMENT_RIGHT:
            width       = _textMeasureLineWidth(request, arg1, table);
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

static void Text_DrawGlyphImmediate(TextDrawReq* request, const _FontGlyph* glyph, s32 arg2)
{
    SPRT* p;
    s32   temp;

    p = &D_80071710;
    setlen(p, 4);
    GPU_PRIMITIVE_COLOR_WORD(p, 0) = arg2;
    setcode(p, 0x64);
    p->x0   = request->x + glyph->xOffset;
    p->y0   = (request->y - glyph->heightMinusOne) + glyph->yOffset;
    p->u0   = glyph->u;
    p->v0   = glyph->v + request->vBias;
    p->w    = glyph->widthMinusOne + 1;
    temp    = glyph->heightMinusOne;
    p->clut = 0x7FFD;
    p->h    = temp + 1;
    DrawPrim(p);
}

static void Text_DrawGlyphQueued(TextDrawReq* request, const _FontGlyph* glyph, s32 arg2)
{
    SPRT* p;
    s32   temp;

    p              = gGpuPrimCursor;
    gGpuPrimCursor = p + 1;
    setlen(p, 4);
    GPU_PRIMITIVE_COLOR_WORD(p, 0) = arg2;
    setcode(p, 0x64);
    p->x0   = request->x + glyph->xOffset;
    p->y0   = (request->y - glyph->heightMinusOne) + glyph->yOffset;
    p->u0   = glyph->u;
    p->v0   = glyph->v + request->vBias;
    p->w    = glyph->widthMinusOne + 1;
    temp    = glyph->heightMinusOne;
    p->clut = 0x7FFD;
    p->h    = temp + 1;
    addPrim(gGpuCurrentOt + request->otIndex, p);
}

static void Text_DrawGlyphOt(TextDrawReq* request, const _FontGlyph* glyph, s32 unusedColor)
{
    SPRT* p;
    s32   temp;

    p              = gGpuPrimCursor;
    gGpuPrimCursor = p + 1;
    setlen(p, 4);
    setcode(p, 0x67);
    p->x0   = request->x + glyph->xOffset;
    p->y0   = (request->y - glyph->heightMinusOne) + glyph->yOffset;
    p->u0   = glyph->u;
    p->v0   = glyph->v + request->vBias;
    p->w    = glyph->widthMinusOne + 1;
    temp    = glyph->heightMinusOne;
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
        if (obj->result == USER_INTERFACE_RESULT_CANCEL || obj->result == USER_INTERFACE_RESULT_CONFIRM) {
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
