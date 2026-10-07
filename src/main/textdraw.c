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
#include "main/task.h"
#include "task.h"
#include "main/task_types.h"
#include "text.h"
#include "main/ui.h"
#include "main/ui_types.h"

#include "gameplay/area_transitions.h"
#include "gameplay/companion_load.h"
#include "gameplay/display.h"
#include "gameplay/effect_tasks.h"
#include "gameplay/hud_sprites.h"
#include "gameplay/loading.h"
#include "gameplay/model_objects.h"
#include "gameplay/room_effects.h"
#include "gameplay/view.h"

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

/// Immediate-mode SPRT scratch used by _textDrawGlyphImmediate.
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

static inline u8* _textItoaUnsigned(u8* buffer, u32 value);

static inline u8* _textItoaSigned(u8* buffer, s32 value);

static inline u8* _textItoaPadded(u8* buffer, u32 value, s32 digitCount);

static u8* _textItoaHexSigned(u8* buffer, s32 value);

static u8* _textItoaHex(u8* buffer, u32 value);

static void _textDrawGlyphImmediate(TextDrawReq* request, const _FontGlyph* glyph, s32 colorRgb);

static void _textDrawGlyphFill(TextDrawReq* request, const _FontGlyph* glyph, s32 colorRgb);

static void _textDrawGlyphOutline(TextDrawReq* request, const _FontGlyph* glyph, s32 unusedColor);

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
    { { { TASK_BODY_NONE, 0xC0 } }, titleExitTask },
    { { { TASK_BODY_NONE, 0xC0 } }, taskKill },
    { { { TASK_BODY_NONE, 0x0 } }, NULL },
    { { { TASK_BODY_NONE, 0x18 } }, GameFlow_DispatchTable },
    { { { TASK_BODY_NONE, 0x10 } }, mcSaveDialogTask },
    { { { TASK_BODY_NONE, 0x10 } }, mcLoadDialogTask },
    { { { TASK_BODY_NONE, 0xC0 } }, taskNoopBank0Slot12 },
    { { { TASK_BODY_NONE, 0x10 } }, Text_BootTask },
    { { { TASK_BODY_COORD, 0x2F } }, viewApplyCoordTask },
    { { { TASK_BODY_NONE, 0x2F } }, viewApplyCameraTask },
    { { { TASK_BODY_NONE, 0x40 } }, func_800AD50C },
    { { { TASK_BODY_NONE, 0x28 } }, func_800AC0F0 },
    { { { TASK_BODY_NONE, 0x10 } }, mcSaveDialogTask },
    { { { TASK_BODY_NONE, 0x10 } }, mcLoadDialogTask },
    { { { TASK_BODY_NONE, 0x1F } }, func_800AEE8C },
    { { { TASK_BODY_NONE, 0xC0 } }, taskKill },
    { { { TASK_BODY_NONE, 0x30 } }, Gp_ViewGateTask },
    { { { TASK_BODY_NONE, 0x2F } }, spriteAllocateViewCachedPacketsTask },
    { { { TASK_BODY_NONE, 0xC0 } }, taskExitCallback },
    { { { TASK_BODY_NONE, 0xC0 } }, func_807011D8 },
    { { { TASK_BODY_NONE, 0xE0 } }, modelObjectDrawTemporaryListsTask },
    { { { TASK_BODY_NONE, 0xD0 } }, spriteViewTask },
    { { { TASK_BODY_NONE, 0x2F } }, Gp_LoadStateTask },
    { { { TASK_BODY_NONE, 0x18 } }, func_800A77B4 },
    { { { TASK_BODY_NONE, 0xF8 } }, Gp_LoadWaitDispatch },
    { { { TASK_BODY_NONE, 0x10 } }, Boot_LoadInitialFile },
    { { { TASK_BODY_NONE, 0x10 } }, Boot_LoadTask },
    { { { TASK_BODY_NONE, 0x2F } }, fadeResumeSessionTask },
    { { { TASK_BODY_NONE, 0xF8 } }, NULL },
    { { { TASK_BODY_NONE, 0xC0 } }, func_80701400 },
    { { { TASK_BODY_NONE, 0x2F } }, NULL },
    { { { TASK_BODY_NONE, 0xF8 } }, viewCommitIndexTask },
    { { { TASK_BODY_NONE, 0xF8 } }, loadingRestoreViewGraphicsTask },
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
/// `textDrawString` and `textAlignLine` select this face when
/// `glyphTable` is `TEXT_GLYPH_TABLE_MEDIUM`. Drawing also selects it for an
/// `\sM` command, in either letter's case, and adds `TEXT_GLYPH_V_BIAS_MEDIUM`
/// to each record's texture V. The initializer is the embedded `font_glyphs0`
/// catalogue blob. Bytes below space are not records in this face.
static _FontGlyph _gFontGlyphsMedium[FONT_GLYPH_MEDIUM_COUNT] = {
#include "assets/font_glyphs0.inc"
};

/// Large UI-font glyph metrics, one record per character byte from ' ' through 0xFF.
///
/// `textDrawString` and `textAlignLine` select this face when
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
/// `textDrawString` and `textAlignLine` select this face when
/// `glyphTable` is `TEXT_GLYPH_TABLE_SMALL`. Drawing also selects it for an
/// `\sS` command, in either letter's case, and adds `TEXT_GLYPH_V_BIAS_SMALL`
/// to each record's texture V. The initializer is the embedded `font_glyphs2`
/// catalogue blob. Bytes outside 0x20..0x7A are not records in this face.
static _FontGlyph _gFontGlyphsSmall[FONT_GLYPH_SMALL_COUNT] = {
#include "assets/font_glyphs2.inc"
};

static UiObjectDesc Ui_OverlayLoadingDesc[] = {
    { USER_INTERFACE_PANEL_TITLE_STYLE, { -144, -104, 0x120, 0x90 }, 0x38, 0, TASK_BODY_NONE, 0xC0, Ui_WaitCdThenOverlay, 0 },
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

/// Sets matching screen and texture rectangles for a UI glyph's fill and outline.
///
/// Pen coordinates and glyph offsets are in draw-environment pixels. X starts
/// at the pen plus `glyph->xOffset`; Y places the last row at the pen baseline
/// plus `glyph->yOffset`, before narrowing X/Y to signed 16-bit sprite fields.
/// U/V are page-local texels; adding signed `request->vBias` to V wraps modulo
/// 256. Byte-sized minus-one dimensions decode to 1..256 pixels and texels.
///
/// `fill` and `outline` are distinct writable `SPRT` packets owned by the caller.
/// Updates only their position, UV and dimensions. Packet allocation,
/// header/color/CLUT setup and queueing belong to the caller. Borrows `request`
/// and `glyph` without modifying or retaining them or advancing the pen.
static inline void _textSetOutlinedGlyphRectangle(SPRT* fill, SPRT* outline,
                                                  const TextDrawReq* request, const _FontGlyph* glyph)
{
    outline->x0 = fill->x0 = request->x + glyph->xOffset;
    outline->y0 = fill->y0 = (request->y - glyph->heightMinusOne) + glyph->yOffset;
    outline->u0 = fill->u0 = glyph->u;
    outline->v0 = fill->v0 = glyph->v + request->vBias;
    outline->w = fill->w = glyph->widthMinusOne + 1;
    outline->h = fill->h = glyph->heightMinusOne + 1;
}

/// Queues an additive glyph fill and subtractive outline in adjacent OT entries.
///
/// Borrows `request` and `glyph` without modifying or retaining them or advancing
/// the pen. Placement and glyph offsets are draw-environment pixels; U/V and
/// dimensions are texels. Screen X/Y narrow to signed 16-bit fields, V wraps
/// modulo 256 after the signed bias, and minus-one dimensions decode to 1..256.
/// `colorRgb` supplies modulation RGB in bits 0..23 (red low); its high byte is
/// replaced by the sprite command. The raw outline ignores RGB.
/// Reserves two `SPRT` packets (40 bytes) at the word-aligned `gGpuPrimCursor`;
/// the arena and signed OT entries `otIndex` and `otIndex + 1` must be writable
/// and in bounds. Font textures and palettes must already be loaded. The caller
/// must prepend font-page commands selecting additive blending for the fill
/// entry and subtractive blending for the earlier-executing outline entry.
/// Keep the packets intact until GPU drawing completes.
static void _textDrawGlyphTranslucentOutlined(TextDrawReq* request, const _FontGlyph* glyph, s32 colorRgb)
{
    /// GPU CLUT selectors for the palettes in the final VRAM row.
    enum {
        /// GPU CLUT selector for the color-modulated fill of translucent outlined UI text.
        ///
        /// Encodes VRAM word X=976, row Y=511 as 0x7FFD for `SPRT::clut`.
        /// The first 16-color palette uploaded there by `Text_LoadClutImages`
        /// must be resident: indices 0..10 are transparent, and 11..15 are
        /// increasing gray with the semi-transparency bit set. The fill sprite
        /// modulates those colors by RGB and enables blending; its ordering-table
        /// entry must select additive texture-page blending before the fill draws.
        TEXT_TRANSLUCENT_GLYPH_FILL_CLUT = getClut(0x3D0, 0x1FF),
        /// GPU CLUT selector for the subtractive outline of translucent UI text.
        ///
        /// Encodes VRAM word X=992, row Y=511 as 0x7FFE for `SPRT::clut`.
        /// The middle 16-color palette uploaded by `Text_LoadClutImages` must be
        /// resident: indices 0..10 are transparent, and 11..15 contain RGB5
        /// grays 2, 4, 6, 16 and 31 with the semi-transparency bit set.
        /// The raw-texture sprite ignores RGB modulation and requires subtractive
        /// page blending to darken the background before the additive fill draws.
        TEXT_TRANSLUCENT_GLYPH_OUTLINE_CLUT = getClut(0x3E0, 0x1FF),
    };

    SPRT* fill;
    SPRT* outline;

    fill                              = gGpuPrimCursor;
    gGpuPrimCursor                    = fill + 1;
    GPU_PRIMITIVE_COLOR_WORD(fill, 0) = colorRgb;
    setSprt(fill);
    setSemiTrans(fill, true);

    outline        = gGpuPrimCursor;
    gGpuPrimCursor = outline + 1;
    setSprt(outline);
    setSemiTrans(outline, true);
    setShadeTex(outline, true);

    // Both passes share geometry; the alternate outline palette supplies its color.
    _textSetOutlinedGlyphRectangle(fill, outline, request, glyph);
    outline->clut = TEXT_TRANSLUCENT_GLYPH_OUTLINE_CLUT;
    fill->clut    = TEXT_TRANSLUCENT_GLYPH_FILL_CLUT;

    // Descending OT traversal draws the subtractive outline before the additive fill.
    addPrim(gGpuCurrentOt + request->otIndex + 1, outline);
    addPrim(gGpuCurrentOt + request->otIndex, fill);
}

/// Queues an opaque glyph fill and subtractive outline in adjacent OT entries.
///
/// Borrows `request` and `glyph` without modifying or retaining them. Pen and
/// glyph offsets are in draw-environment pixels; texture U/V and dimensions are
/// in texels. V wraps modulo 256 after adding `request->vBias`; screen X/Y are
/// stored in signed 16-bit sprite fields. `colorRgb` supplies modulation RGB in
/// bits 0..23 (red in the low byte); the command byte is replaced.
/// Reserves two `SPRT` packets (40 bytes) from the word-aligned `gGpuPrimCursor`.
/// Both signed tag indices `request->otIndex` and `request->otIndex + 1` must
/// lie within the active `gGpuCurrentOt`, and the arena must have room for both
/// packets. Font textures and palettes must already be loaded. The caller must
/// prepend font-page commands to both entries, selecting subtractive blending
/// for the outline entry so it executes before the fill. Packets remain live
/// until GPU drawing completes; this callback does not advance the pen.
static void _textDrawGlyphOutlined(TextDrawReq* request, const _FontGlyph* glyph, s32 colorRgb)
{
    /// Palettes in the final VRAM row: glyph fill and opaque-text outline.
    enum {
        /// GPU CLUT selector for the color-modulated fill of an outlined UI glyph.
        ///
        /// Encodes VRAM word X=976, row Y=511 as 0x7FFD for `SPRT::clut`.
        /// `Text_LoadClutImages` uploads the 16-color palette there: texel indices
        /// 0..10 are transparent and 11..15 are progressively brighter gray.
        /// The fill sprite disables semi-transparency and modulates these colors by RGB.
        TEXT_OUTLINED_GLYPH_FILL_CLUT = getClut(0x3D0, 0x1FF),
        /// GPU CLUT selector for the subtractive outline of an opaque UI glyph.
        ///
        /// Encodes VRAM word X=1008, row Y=511 as 0x7FFF for `SPRT::clut`.
        /// The final 16-color palette uploaded by `Text_LoadClutImages` must be
        /// resident: indices 0..5 are transparent, 6..9 are increasing gray,
        /// and 10..15 are white. All nonzero colors enable semi-transparency.
        /// The raw-texture sprite ignores RGB; with subtractive page blending,
        /// these colors darken the background before the opaque fill is drawn.
        TEXT_OUTLINED_GLYPH_OUTLINE_CLUT = getClut(0x3F0, 0x1FF),
    };

    SPRT* fill;
    SPRT* outline;

    fill                              = gGpuPrimCursor;
    gGpuPrimCursor                    = fill + 1;
    GPU_PRIMITIVE_COLOR_WORD(fill, 0) = colorRgb;
    setSprt(fill);

    outline        = gGpuPrimCursor;
    gGpuPrimCursor = outline + 1;
    setSprt(outline);
    setSemiTrans(outline, true);
    setShadeTex(outline, true);

    // Both passes sample the same rectangle; the outline's raw texture ignores RGB.
    _textSetOutlinedGlyphRectangle(fill, outline, request, glyph);
    outline->clut = TEXT_OUTLINED_GLYPH_OUTLINE_CLUT;
    fill->clut    = TEXT_OUTLINED_GLYPH_FILL_CLUT;

    // Descending OT traversal draws the outline entry before the opaque fill.
    addPrim(gGpuCurrentOt + request->otIndex + 1, outline);
    addPrim(gGpuCurrentOt + request->otIndex, fill);
}

/// Queues an opaque glyph fill and subtractive outline in one OT entry.
///
/// Borrows `request` and `glyph` without modifying or retaining them or advancing
/// the pen. Pen coordinates and glyph offsets are draw-environment pixels;
/// U/V and dimensions are texels. Screen X/Y narrow to signed 16-bit fields,
/// V wraps modulo 256 after the signed bias, and minus-one dimensions decode
/// to 1..256. `colorRgb` supplies modulation RGB in bits 0..23 (red low); its
/// high byte is replaced by the sprite command. The raw outline ignores RGB.
///
/// Reserves two `SPRT` and two `DR_TPAGE` packets (56 bytes) from the word-aligned
/// `gGpuPrimCursor`. The arena and signed `request->otIndex` entry in
/// `gGpuCurrentOt` must be writable and in bounds; no adjacent entry is used.
/// Font textures and palettes must already be loaded. Each pass sets the font
/// page before drawing, so the caller need not supply per-pass page commands.
/// Keep the packets intact until GPU drawing completes.
static void _textDrawGlyphOutlinedSingleEntry(TextDrawReq* request, const _FontGlyph* glyph, s32 colorRgb)
{
    /// Palette selectors and draw-mode commands for the two glyph passes.
    enum {
        /// GPU texture-depth selector for 4-bit indexed font texels.
        ///
        /// `getTPage` encodes value 0 in texture-page bits 7..8. Fill and outline
        /// sample the same font page, using separate 16-color CLUTs to interpret
        /// texel indices 0..15.
        TEXT_SINGLE_ENTRY_GLYPH_TEXTURE_DEPTH_4BIT = 0,
        /// GPU CLUT selector for the opaque, color-modulated fill in one OT entry.
        ///
        /// Encodes VRAM word X=976, row Y=511 as 0x7FFD for `SPRT::clut`.
        /// The first 16-color palette uploaded by `Text_LoadClutImages` must be
        /// resident: indices 0..10 are transparent; 11..15 have RGB5 gray levels
        /// 7, 13, 19, 25 and 31. These colors set the semi-transparency bit, but
        /// the fill sprite disables blending and modulates them by RGB.
        TEXT_SINGLE_ENTRY_GLYPH_FILL_CLUT = getClut(0x3D0, 0x1FF),
        /// GPU CLUT selector for the subtractive glyph outline in one OT entry.
        ///
        /// Encodes VRAM word X=1008, row Y=511 as 0x7FFF for `SPRT::clut`.
        /// The final 16-color palette uploaded by `Text_LoadClutImages` must be
        /// resident: indices 0..5 are transparent, 6..9 have RGB5 gray levels
        /// 1, 3, 6 and 9, and 10..15 are white. Every nonzero color sets the
        /// semi-transparency bit. The outline sprite uses raw texture colors,
        /// ignoring RGB modulation; its subtractive page command must execute
        /// first so this coverage darkens the background before the opaque fill.
        TEXT_SINGLE_ENTRY_GLYPH_OUTLINE_CLUT = getClut(0x3F0, 0x1FF),
        /// Complete GPU draw-mode word for the opaque fill in one OT entry.
        ///
        /// Encodes 0xE100023F: the 4bpp font page at VRAM word X=960, Y=256,
        /// additive blending, dithering on and drawing into the display area off.
        /// Store in `DR_TPAGE::code[0]` with a one-word payload, executing after
        /// the subtractive outline and before the fill. The fill sprite disables
        /// semitransparency, so it draws opaquely despite the additive selection.
        /// This draw mode remains active until replaced; the font page and the
        /// `TEXT_SINGLE_ENTRY_GLYPH_FILL_CLUT` palette must already be resident.
        TEXT_SINGLE_ENTRY_GLYPH_FILL_PAGE_COMMAND =
            _get_mode(false, true, getTPage(TEXT_SINGLE_ENTRY_GLYPH_TEXTURE_DEPTH_4BIT, GPU_BLEND_ADD, 0x3C0, 0x100)),
        /// Complete GPU draw-mode word for the subtractive outline in one OT entry.
        ///
        /// Encodes 0xE100025F: the 4bpp font page at VRAM word X=960, Y=256,
        /// subtractive blending, dithering on and drawing into the display area off.
        /// Store in `DR_TPAGE::code[0]` with a one-word payload before the outline.
        /// The raw, semitransparent outline sprite ignores RGB modulation and uses
        /// `TEXT_SINGLE_ENTRY_GLYPH_OUTLINE_CLUT`; its nonzero colors enable blending
        /// to darken the background. The font texture and palette must be resident.
        /// In the shared OT entry, the outline precedes the opaque fill, whose
        /// `TEXT_SINGLE_ENTRY_GLYPH_FILL_PAGE_COMMAND` replaces this draw mode.
        TEXT_SINGLE_ENTRY_GLYPH_OUTLINE_PAGE_COMMAND =
            _get_mode(false, true, getTPage(TEXT_SINGLE_ENTRY_GLYPH_TEXTURE_DEPTH_4BIT, GPU_BLEND_SUBTRACT, 0x3C0, 0x100)),
    };

    SPRT*     fill;
    SPRT*     outline;
    DR_TPAGE* page;

    fill                              = gGpuPrimCursor;
    gGpuPrimCursor                    = fill + 1;
    GPU_PRIMITIVE_COLOR_WORD(fill, 0) = colorRgb;
    setSprt(fill);

    outline        = gGpuPrimCursor;
    gGpuPrimCursor = outline + 1;
    setSprt(outline);
    setSemiTrans(outline, true);
    setShadeTex(outline, true);

    // Both passes share geometry; the raw outline uses its own palette.
    _textSetOutlinedGlyphRectangle(fill, outline, request, glyph);
    outline->clut = TEXT_SINGLE_ENTRY_GLYPH_OUTLINE_CLUT;
    fill->clut    = TEXT_SINGLE_ENTRY_GLYPH_FILL_CLUT;

    // Prepend in reverse execution order: outline page, outline, fill page, fill.
    addPrim(gGpuCurrentOt + request->otIndex, fill);
    page           = gGpuPrimCursor;
    gGpuPrimCursor = page + 1;
    setlen(page, ARRAY_SIZE(page->code));
    page->code[0] = TEXT_SINGLE_ENTRY_GLYPH_FILL_PAGE_COMMAND;
    addPrim(gGpuCurrentOt + request->otIndex, page);

    addPrim(gGpuCurrentOt + request->otIndex, outline);
    page           = gGpuPrimCursor;
    gGpuPrimCursor = page + 1;
    setlen(page, ARRAY_SIZE(page->code));
    page->code[0] = TEXT_SINGLE_ENTRY_GLYPH_OUTLINE_PAGE_COMMAND;
    addPrim(gGpuCurrentOt + request->otIndex, page);
}

/// Tightens the drawing pen's horizontal gap before a kerned glyph pair.
///
/// `previousRightClass` is the preceding glyph's unsigned right-edge class
/// (0 neutral, 1 positive, 255 negative), or neutral before the first glyph.
/// Only `glyph->leftKerningClass` is read; equal non-neutral classes tighten
/// the gap. Classes are added after integer promotion and tested modulo 256.
/// `request->x` decreases by one draw-environment pixel for
/// `TEXT_GLYPH_TABLE_SMALL` and two otherwise, narrowing back to signed 16 bits.
/// This scale follows the initial selector even after inline face changes.
/// Borrows both objects for the call, changing only X and retaining neither;
/// the caller supplies the next previous class and advances the pen separately.
static inline void _textApplyDrawKerning(TextDrawReq* request, u8 previousRightClass, const _FontGlyph* glyph)
{
    // Bias maps every non-tightening pair into 0..2 in the low byte.
    if ((u8)(previousRightClass + glyph->leftKerningClass + 1) >= 3) {
        if (request->glyphTable == TEXT_GLYPH_TABLE_SMALL) {
            request->x -= 1;
        } else {
            request->x -= 2;
        }
    }
}

void textDrawString(TextDrawReq* request, const u8* text)
{
    enum {
        TEXT_LINE_COLOR_WHITE          = 0x606060,
        TEXT_LINE_COLOR_YELLOW         = 0x037A78,
        TEXT_LINE_COLOR_ORANGE         = 0x0D287F,
        TEXT_LINE_COLOR_GREEN          = 0x01741F,
        TEXT_LINE_COLOR_H              = 0x38443C,
        TEXT_LINE_COLOR_CYAN           = 0x808008,
        TEXT_LINE_COLOR_RED            = 0x001666,
        TEXT_LINE_FONT_PAGE_COMMAND    = _get_mode(false, true, getTPage(0, GPU_BLEND_ADD, 0x3C0, 0x100)),
        TEXT_LINE_OUTLINE_PAGE_COMMAND = _get_mode(false, true, getTPage(0, GPU_BLEND_SUBTRACT, 0x3C0, 0x100)),
    };

    const u8*         cursor;
    const _FontGlyph* table;
    const _FontGlyph* glyph;
    void              (*drawGlyph)(TextDrawReq* request, const _FontGlyph* glyph, s32 colorRgb);
    s32               colorRgb;
    s32               previousRightKerningClass;
    s32               width;
    u8                byte;
    s32               endLine;
    s32               glyphIndex;
    DR_TPAGE*         texturePage;

    cursor                    = text;
    previousRightKerningClass = FONT_KERNING_CLASS_NEUTRAL;
    colorRgb                  = request->colorRgb;
    request->vBias            = TEXT_GLYPH_V_BIAS_SMALL;
    // Alignment keeps the initial metrics; inline face changes affect drawing only.
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
            drawGlyph = _textDrawGlyphOutlined;
            break;
        case TEXT_DRAW_OUTLINED_SINGLE_ENTRY:
            drawGlyph = _textDrawGlyphOutlinedSingleEntry;
            break;
        case TEXT_DRAW_TRANSLUCENT_OUTLINED:
            drawGlyph = _textDrawGlyphTranslucentOutlined;
            break;
        case TEXT_DRAW_OUTLINE_ONLY:
            drawGlyph = _textDrawGlyphOutline;
            break;
        case TEXT_DRAW_IMMEDIATE:
            texturePage = &D_80071728;
            setlen(texturePage, ARRAY_SIZE(texturePage->code));
            texturePage->code[0] = TEXT_LINE_FONT_PAGE_COMMAND;
            DrawPrim(texturePage);
            drawGlyph = _textDrawGlyphImmediate;
            break;
        case TEXT_DRAW_FILL_ONLY:
        default:
            drawGlyph = _textDrawGlyphFill;
            break;
    }
    // Commands update live styling; pair tightening keeps the initial selector.
    while ((byte = *cursor) != 0 && byte != '\n' && byte != '\r') {
        endLine = 0;
        if (byte == '\\') {
            do {
                cursor++;
                switch (*cursor) {
                    case 'C':
                    case 'c':
                        cursor++;
                        switch (*cursor) {
                            case 'W':
                            case 'w':
                                colorRgb = TEXT_LINE_COLOR_WHITE;
                                break;
                            case 'Y':
                            case 'y':
                                colorRgb = TEXT_LINE_COLOR_YELLOW;
                                break;
                            case 'O':
                            case 'o':
                                colorRgb = TEXT_LINE_COLOR_ORANGE;
                                break;
                            case 'G':
                            case 'g':
                                colorRgb = TEXT_LINE_COLOR_GREEN;
                                break;
                            case 'H':
                            case 'h':
                                colorRgb = TEXT_LINE_COLOR_H;
                                break;
                            case 'C':
                            case 'c':
                                colorRgb = TEXT_LINE_COLOR_CYAN;
                                break;
                            case 'R':
                            case 'r':
                                colorRgb = TEXT_LINE_COLOR_RED;
                                break;
                        }
                        cursor++;
                        break;
                    case 'S':
                    case 's':
                        cursor++;
                        switch (*cursor) {
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
                        cursor++;
                        break;
                    case 'W':
                    case 'w':
                        cursor++;
                        switch (*cursor) {
                            case '0':
                                request->drawMode = TEXT_DRAW_TRANSLUCENT_OUTLINED;
                                drawGlyph         = _textDrawGlyphTranslucentOutlined;
                                break;
                            case '1':
                                request->drawMode = TEXT_DRAW_OUTLINED;
                                drawGlyph         = _textDrawGlyphOutlined;
                                break;
                        }
                        cursor++;
                        break;
                    case 'U':
                    case 'u':
                        cursor++;
                        if ((u32)(*cursor - '0') < 10) {
                            request->y -= *cursor - '0';
                        }
                        cursor++;
                        break;
                    case 'D':
                    case 'd':
                        cursor++;
                        if ((u32)(*cursor - '0') < 10) {
                            request->y += *cursor - '0';
                        }
                        cursor++;
                        break;
                    case 'B':
                    case 'b':
                        cursor++;
                        if ((u32)(*cursor - '0') < 10) {
                            request->x = (*cursor - '0') << 3;
                        }
                        cursor++;
                        break;
                    case 'N':
                    case 'n':
                        endLine = 1;
                        break;
                }
                if (*cursor == 0 || *cursor == '\n' || *cursor == '\r') {
                    endLine = 1;
                }
            } while (*cursor == '\\');
        }
        if (endLine != 0) {
            break;
        }
        if (*cursor < ' ') {
            cursor++;
            continue;
        }
        glyphIndex = *cursor - ' ';
        glyph      = &table[glyphIndex];
        _textApplyDrawKerning(request, previousRightKerningClass, glyph);
        previousRightKerningClass = glyph->rightKerningClass;
        drawGlyph(request, glyph, colorRgb);
        cursor++;
        request->x += glyph->widthMinusOne + glyph->advanceExtraX + glyph->xOffset;
        request->y += glyph->advanceY;
    }
    // Prepend page commands after the sprites so each OT entry executes them first.
    if (request->drawMode == TEXT_DRAW_OUTLINED || request->drawMode == TEXT_DRAW_TRANSLUCENT_OUTLINED) {
        texturePage          = gGpuPrimCursor;
        gGpuPrimCursor       = texturePage + 1;
        texturePage->code[0] = TEXT_LINE_OUTLINE_PAGE_COMMAND;
        setlen(texturePage, ARRAY_SIZE(texturePage->code));
        addPrim(gGpuCurrentOt + request->otIndex + 1, texturePage);
    }
    if (request->drawMode != TEXT_DRAW_IMMEDIATE) {
        if (request->drawMode == TEXT_DRAW_OUTLINE_ONLY) {
            texturePage          = gGpuPrimCursor;
            gGpuPrimCursor       = texturePage + 1;
            texturePage->code[0] = TEXT_LINE_OUTLINE_PAGE_COMMAND;
            setlen(texturePage, ARRAY_SIZE(texturePage->code));
            addPrim(gGpuCurrentOt + request->otIndex, texturePage);
        } else {
            texturePage          = gGpuPrimCursor;
            gGpuPrimCursor       = texturePage + 1;
            texturePage->code[0] = TEXT_LINE_FONT_PAGE_COMMAND;
            setlen(texturePage, ARRAY_SIZE(texturePage->code));
            addPrim(gGpuCurrentOt + request->otIndex, texturePage);
        }
    }
}

/// Writes the saturated unsigned decimal bytes shared by the public formatters.
///
/// Zero is "0"; values at or above 999,999,999 become nine nines. Every u32
/// input is accepted. `buffer` supplies up to ten writable bytes including
/// NUL and is returned without being retained. No capacity check is performed.
static inline u8* _textItoaUnsigned(u8* buffer, u32 value)
{
    enum { TEXT_UNSIGNED_DECIMAL_MAX = 999999999 };

    u8* destination;
    u32 decimalPlace;

    /// Writes one unsigned decimal digit, advancing its place and remainder.
    ///
    /// Arguments must be distinct simple lvalues with no side effects: they
    /// are evaluated repeatedly. The nonzero place must give a quotient 0..9.
    /// The byte store narrows the quotient before unsigned remainder math.
#define TEXT_STEP_UNSIGNED_DECIMAL_DIGIT(destination, decimalPlace, value) \
    do {                                                                   \
        *(destination)      = (value) / (decimalPlace);                    \
        (value)            -= *(destination) * (decimalPlace);             \
        (decimalPlace)     /= 10;                                          \
        *((destination)++) += '0';                                         \
    } while (0)

    decimalPlace = 100000000;
    if (value >= TEXT_UNSIGNED_DECIMAL_MAX) {
        __builtin_memcpy(buffer, Text_MaxNineDigits, sizeof("999999999"));
    } else if (value == 0) {
        __builtin_memcpy(buffer, Text_ZeroDigit, sizeof("0"));
    } else {
        // Skip leading zero places, then consume the remaining decimal digits.
        destination = buffer;
        while (value < decimalPlace) {
            decimalPlace /= 10;
        }
        while (decimalPlace != 0) {
            TEXT_STEP_UNSIGNED_DECIMAL_DIGIT(destination, decimalPlace, value);
        }
        *destination = '\0';
    }
#undef TEXT_STEP_UNSIGNED_DECIMAL_DIGIT
    return buffer;
}

/// Writes the saturated signed decimal bytes shared by the public formatters.
///
/// Magnitudes above 99,999,999 become eight nines; negatives receive '-'.
/// `value` must exclude the s32 minimum because it is negated before recursion.
/// `buffer` supplies up to ten writable bytes including NUL and is returned
/// without being retained. No allocation or capacity check is performed.
static inline u8* _textItoaSigned(u8* buffer, s32 value)
{
    enum { TEXT_SIGNED_DECIMAL_MAX = 99999999 };

    u8* destination;
    s32 decimalPlace;
    s32 belowLeadingPlace;

    /// Writes one decimal digit, advances its place and consumes its value.
    ///
    /// Arguments must be distinct simple lvalues, with no side effects: they
    /// are evaluated repeatedly. The nonzero place must give a quotient 0..9.
#define TEXT_STEP_SIGNED_DECIMAL_DIGIT(destination, decimalPlace, value) \
    do {                                                                 \
        s32 quotient;                                                    \
        s32 digitValue;                                                  \
        s32 consumedValue;                                               \
        quotient        = (value) / (decimalPlace);                      \
        *(destination)  = quotient;                                      \
        digitValue      = *(destination) & 0xFF;                         \
        consumedValue   = digitValue * (decimalPlace);                   \
        (decimalPlace) /= 10;                                            \
        *(destination)  = digitValue + '0';                              \
        (destination)++;                                                 \
        (value) -= consumedValue;                                        \
    } while (0)

    decimalPlace = 10000000;
    if (value < 0) {
        *buffer = '-';
        textItoaSigned(buffer + 1, -value);
        return buffer;
    }
    if (value > TEXT_SIGNED_DECIMAL_MAX) {
        __builtin_memcpy(buffer, Text_MaxEightDigits, sizeof("99999999"));
        return buffer;
    }
    belowLeadingPlace = value < decimalPlace;
    if (value == 0) {
        __builtin_memcpy(buffer, Text_ZeroDigit, sizeof("0"));
        return buffer;
    }
    // Skip leading zero places, then consume the remaining decimal digits.
    destination = buffer;
    if (belowLeadingPlace) {
        do {
            decimalPlace /= 10;
        } while (value < decimalPlace);
    }
    if (decimalPlace > 0) {
        do {
            TEXT_STEP_SIGNED_DECIMAL_DIGIT(destination, decimalPlace, value);
        } while (decimalPlace > 0);
    }
#undef TEXT_STEP_SIGNED_DECIMAL_DIGIT
    *destination = '\0';
    return buffer;
}

/// Writes a saturated unsigned decimal value in exactly `digitCount` digits.
///
/// `digitCount` must be 1..9 so the decimal limit fits u32. `buffer` supplies
/// digitCount + 1 writable bytes; leading zeros and NUL are included. Returns
/// the original buffer without retaining it. There is no capacity check.
static inline u8* _textItoaPadded(u8* buffer, u32 value, s32 digitCount)
{
    u8* destination;
    u32 decimalPlace;
    s32 remainingPlaces;
    u32 maximumValue;

    decimalPlace    = 1;
    remainingPlaces = digitCount - 1;
    while (remainingPlaces > 0) {
        decimalPlace *= 10;
        remainingPlaces--;
    }
    maximumValue = decimalPlace * 10 - 1;
    if (maximumValue < value) {
        value = maximumValue;
    }
    destination = buffer;
    while (value < decimalPlace) {
        decimalPlace  /= 10;
        *destination++ = '0';
    }
    while (decimalPlace != 0) {
        *destination    = value / decimalPlace;
        value          -= *destination * decimalPlace;
        decimalPlace   /= 10;
        *destination++ += '0';
    }
    *destination = 0;
    return buffer;
}

u8* textFormatPlayTime(u8* buffer, u16 totalMinutes)
{
    enum { TEXT_PLAY_TIME_MAX_MINUTES = 59999 };

    u8* start;
    s32 hours;
    s32 minutes;

    start = buffer;
    if (totalMinutes > TEXT_PLAY_TIME_MAX_MINUTES) {
        totalMinutes = TEXT_PLAY_TIME_MAX_MINUTES;
    }
    hours   = totalMinutes / 60;
    minutes = totalMinutes % 60;
    _textItoaUnsigned(start, hours);
    // Replace the hours terminator with the separator and two minute digits.
    if (hours >= 100) {
        buffer += 3;
    } else if (hours >= 10) {
        buffer += 2;
    } else {
        buffer += 1;
    }
    *buffer++ = ':';
    _textItoaPadded(buffer, minutes, 2);
    return start;
}

void textAlignLine(TextDrawReq* request, const u8* text)
{
    const _FontGlyph* glyphTable;
    s32               width;

    // Alignment keeps the initial face even when the line contains font commands.
    switch (request->glyphTable) {
        case TEXT_GLYPH_TABLE_MEDIUM:
            glyphTable = _gFontGlyphsMedium;
            break;
        case TEXT_GLYPH_TABLE_SMALL:
            glyphTable = _gFontGlyphsSmall;
            break;
        default:
            glyphTable = _gFontGlyphsLarge;
            break;
    }

    switch (request->alignment) {
        case TEXT_ALIGNMENT_CENTER:
            width       = _textMeasureLineWidth(request, text, glyphTable);
            request->x -= width >> 1;
            break;
        case TEXT_ALIGNMENT_RIGHT:
            width       = _textMeasureLineWidth(request, text, glyphTable);
            request->x -= width;
            break;
    }
}

u8* textItoaSignPrefixed(u8* buffer, s32 value)
{
    *buffer = value >= 0 ? '+' : '-';
    // Pass the signed value through, retaining its own minus after the prefix.
    _textItoaSigned(buffer + 1, value);
    return buffer;
}

u8* textItoaSigned(u8* buffer, s32 value)
{
    return _textItoaSigned(buffer, value);
}

u8* textItoaUnsigned(u8* buffer, u32 value)
{
    return _textItoaUnsigned(buffer, value);
}

/// Writes signed-magnitude uppercase hexadecimal without a radix prefix.
///
/// Zero is "0" and negatives receive '-'. `value` must exclude the s32
/// minimum because recursion negates it as s32. `buffer` supplies up to ten
/// writable bytes including NUL and is returned without being retained.
/// The caller owns the buffer; no allocation or capacity check is performed.
static u8* _textItoaHexSigned(u8* buffer, s32 value)
{
    u8* destination;
    s32 hexPlace;
    s32 quotient;
    s32 digitValue;
    s32 consumedValue;
    s32 belowLeadingPlace;

    hexPlace = 0x10000000;
    if (value < 0) {
        *buffer = '-';
        _textItoaHexSigned(buffer + 1, -value);
        return buffer;
    }
    belowLeadingPlace = value < hexPlace;
    if (value == 0) {
        __builtin_memcpy(buffer, Text_ZeroDigit, sizeof("0"));
        return buffer;
    }
    // Skip leading zero nibbles, then consume the magnitude in base sixteen.
    destination = buffer;
    if (belowLeadingPlace) {
        do {
            hexPlace >>= 4;
        } while (value < hexPlace);
    }
    if (hexPlace > 0) {
        do {
            quotient      = value / hexPlace;
            *destination  = quotient;
            digitValue    = *destination & 0xFF;
            consumedValue = digitValue * hexPlace;
            hexPlace    >>= 4;
            value        -= consumedValue;
            if (digitValue >= 10U) {
                *destination = digitValue + ('A' - 10);
            } else {
                *destination = digitValue + '0';
            }
            destination++;
        } while (hexPlace > 0);
    }
    *destination = '\0';
    return buffer;
}

/// Writes unsigned uppercase hexadecimal without leading zeros or a radix prefix.
///
/// Accepts every u32 value, including zero as "0". `buffer` supplies up to
/// nine writable bytes including NUL and is returned without being retained.
/// The caller owns the buffer; no allocation or capacity check is performed.
static u8* _textItoaHex(u8* buffer, u32 value)
{
    u8* destination;
    u32 hexPlace;
    u32 digitValue;

    hexPlace = 0x10000000;
    if (value == 0) {
        __builtin_memcpy(buffer, Text_ZeroDigit, sizeof("0"));
    } else {
        // Skip leading zero nibbles, then consume all remaining nibbles.
        destination = buffer;
        if (value < hexPlace) {
            do {
                hexPlace >>= 4;
            } while (value < hexPlace);
        }
        if (hexPlace != 0) {
            do {
                *destination = value / hexPlace;
                digitValue   = *destination;
                value       -= digitValue * hexPlace;
                hexPlace   >>= 4;
                if (digitValue >= 10) {
                    *destination = digitValue + ('A' - 10);
                } else {
                    *destination = digitValue + '0';
                }
                destination++;
            } while (hexPlace != 0);
        }
        *destination = '\0';
    }
    return buffer;
}

u8* textItoaPadded(u8* buffer, u32 value, s32 digitCount)
{
    return _textItoaPadded(buffer, value, digitCount);
}

const u8* textSkipLines(const u8* text, s32 lineCount)
{
    u8 byte;

    while (lineCount > 0) {
        byte = *text;
        if (byte == '\0') {
            break;
        }
        if (byte == '\n') {
            lineCount -= 1;
        } else if (byte == 'N' || byte == 'n') {
            if (text[-1] == '\\') {
                lineCount -= 1;
            }
        }
        text += 1;
    }
    return text;
}

u8* textAppendString(u8* dest, const u8* src)
{
    u8 byte;

    if (*dest != 0) {
        while (*++dest != 0) {
        }
    }

    byte = *src;
    if (byte != 0) {
        do {
            src++;
            *dest = byte;
            byte  = *src;
            dest++;
        } while (byte != 0);
    }

    *dest = 0;
    return dest;
}

/// Initializes an opaque, RGB-modulated UI-glyph fill for immediate submission.
///
/// `fill` supplies one writable, word-aligned `SPRT`, separate from `request`
/// and `glyph`. Sets its four-word GPU payload length, RGB, command, rectangle
/// and fill CLUT, preserving the tag's 24-bit DMA address. `colorRgb` supplies
/// modulation RGB in bits 0..23 (red low); the command replaces its high byte.
/// Borrows all objects for the call, leaving request and glyph read-only and
/// retaining none. Packet storage and submission belong to the caller.
///
/// Pen coordinates and glyph offsets are draw-environment pixels. X/Y narrow
/// to signed 16-bit sprite fields; the last row is at pen Y plus glyph Y offset.
/// U/V are page-local texels, with signed V bias wrapping modulo 256.
/// Minus-one dimensions decode to 1..256 pixels/texels. Drawing requires the
/// 4bpp font page to be selected and the font texture and fill palette resident.
static inline void _textInitImmediateGlyphSprite(SPRT* fill, const TextDrawReq* request,
                                                 const _FontGlyph* glyph, u32 colorRgb)
{
    enum {
        /// First 16-color font palette at VRAM word X=976, Y=511 (selector 0x7FFD).
        TEXT_IMMEDIATE_GLYPH_FILL_CLUT = getClut(976, 511),
        /// Variable-size textured sprite with RGB modulation and blending disabled.
        TEXT_IMMEDIATE_GLYPH_SPRITE_COMMAND = 0x64,
    };

    s32 heightMinusOne;

    setlen(fill, (sizeof(*fill) - sizeof(fill->tag)) / sizeof(u32));
    // The packed RGB store includes the command byte, so set the command last.
    GPU_PRIMITIVE_COLOR_WORD(fill, 0) = colorRgb;
    setcode(fill, TEXT_IMMEDIATE_GLYPH_SPRITE_COMMAND);
    fill->x0       = request->x + glyph->xOffset;
    fill->y0       = (request->y - glyph->heightMinusOne) + glyph->yOffset;
    fill->u0       = glyph->u;
    fill->v0       = glyph->v + request->vBias;
    fill->w        = glyph->widthMinusOne + 1;
    heightMinusOne = glyph->heightMinusOne;
    fill->clut     = TEXT_IMMEDIATE_GLYPH_FILL_CLUT;
    fill->h        = heightMinusOne + 1;
}

/// Submits one opaque, RGB-modulated UI-glyph fill in the active draw environment.
///
/// Borrows `request` and `glyph` without modifying or retaining them or moving
/// the pen. The matching drawer signature supplies RGB in bits 0..23 (red low);
/// the sprite command replaces its high byte. Coordinates are draw-environment
/// pixels, U/V and dimensions are texels; X/Y narrow to s16 and V wraps to u8.
/// Requires the 4bpp font page and fill palette to be resident and selected.
/// Reuses the private sprite for each synchronous `DrawPrim` submission, so
/// no primitive-arena space or OT entry is consumed. Calls must not overlap.
static void _textDrawGlyphImmediate(TextDrawReq* request, const _FontGlyph* glyph, s32 colorRgb)
{
    SPRT* fill;

    fill = &D_80071710;
    _textInitImmediateGlyphSprite(fill, request, glyph, colorRgb);
    DrawPrim(fill);
}

/// Initializes an opaque, RGB-modulated UI-glyph fill packet at the text pen.
///
/// `fill` provides one writable, word-aligned `SPRT`, separate from `request`
/// and `glyph`. Sets the four-word payload length, color, command, rectangle
/// and fill CLUT while preserving the tag's DMA address. Borrows all objects
/// for this call; the request and metrics stay read-only and the pen stays put.
/// Allocation, DMA linking and submission belong to the caller.
///
/// Pen coordinates and glyph offsets are draw-environment pixels. X/Y narrow
/// to signed 16-bit fields; the last row lies at pen Y plus the glyph Y offset.
/// U/V are page-local texels, with signed V bias wrapping modulo 256, and
/// byte-sized minus-one dimensions decode to 1..256 pixels/texels. `colorRgb`
/// supplies RGB in bits 0..23 (red low); the command replaces its high byte.
/// Drawing requires the font textures and fill palette to be resident and a
/// 4bpp font page to be selected before the packet executes.
static inline void _textInitGlyphFillSprite(SPRT* fill, const TextDrawReq* request,
                                            const _FontGlyph* glyph, u32 colorRgb)
{
    enum {
        /// GPU CLUT selector for the opaque, color-modulated fill of queued UI text.
        ///
        /// Encodes VRAM word X=976, row Y=511 as 0x7FFD in `SPRT::clut`.
        /// Selects the first 16 entries of the 48-color row uploaded by
        /// `Text_LoadClutImages`; these must remain resident while drawing.
        /// 4bpp texel indices 0..10 are transparent; 11..15 are RGB5 grays
        /// 7, 13, 19, 25 and 31, with the semi-transparency bit set.
        /// The fill's sprite command modulates them by RGB and disables blending;
        /// this selector encodes only the palette address.
        TEXT_QUEUED_GLYPH_FILL_CLUT = getClut(976, 511),
    };

    s32 heightMinusOne;

    // The packed RGB write includes the command byte, so set the command last.
    GPU_PRIMITIVE_COLOR_WORD(fill, 0) = colorRgb;
    setSprt(fill);
    fill->x0       = request->x + glyph->xOffset;
    fill->y0       = (request->y - glyph->heightMinusOne) + glyph->yOffset;
    fill->u0       = glyph->u;
    fill->v0       = glyph->v + request->vBias;
    fill->w        = glyph->widthMinusOne + 1;
    heightMinusOne = glyph->heightMinusOne;
    fill->clut     = TEXT_QUEUED_GLYPH_FILL_CLUT;
    fill->h        = heightMinusOne + 1;
}

/// Queues one opaque, color-modulated UI-glyph fill without an outline.
///
/// Borrows `request` and `glyph` without modifying or retaining them or advancing
/// the pen. Pen coordinates and glyph offsets are draw-environment pixels; the
/// last row lies at pen Y plus the glyph's Y offset. U/V are page-local texels,
/// with signed V bias wrapping modulo 256; dimensions decode to 1..256.
/// `colorRgb` is the live modulation RGB, including inline color changes,
/// in bits 0..23 (red low); its high byte is replaced by the sprite command.
///
/// Reserves one `SPRT` (20 bytes) at the word-aligned `gGpuPrimCursor`. The arena
/// and signed `request->otIndex` entry in `gGpuCurrentOt` must be writable and
/// in bounds. Font textures and the fill palette must already be resident.
/// The caller must prepend a 4bpp font-page command before this entry executes;
/// keep the packet intact until GPU drawing completes.
static void _textDrawGlyphFill(TextDrawReq* request, const _FontGlyph* glyph, s32 colorRgb)
{
    SPRT* fill;

    fill           = gGpuPrimCursor;
    gGpuPrimCursor = fill + 1;
    _textInitGlyphFillSprite(fill, request, glyph, colorRgb);
    addPrim(gGpuCurrentOt + request->otIndex, fill);
}

/// Initializes a raw, semitransparent UI-glyph outline at the current text pen.
///
/// `outline` is one writable, word-aligned `SPRT`, separate from `request` and
/// `glyph`. Sets its four-word GPU payload length, command, rectangle and CLUT;
/// RGB and the tag's DMA address remain untouched. The caller owns the packet's
/// allocation and linking. All three objects are borrowed only for this call;
/// the request and glyph are read-only, and the pen is not advanced.
///
/// Pen coordinates and glyph offsets are draw-environment pixels. Screen X/Y
/// narrow to signed 16-bit fields; the last row is at pen Y plus the Y offset.
/// U/V are page-local texels, with V wrapping modulo 256 after the signed bias.
/// Byte-sized minus-one dimensions decode to 1..256 pixels and texels.
/// Drawing requires the font texture and outline palette to be resident, with
/// the 4bpp font page selecting `GPU_BLEND_SUBTRACT` before the sprite executes.
static inline void _textInitOutlineGlyphSprite(SPRT* outline, const TextDrawReq* request, const _FontGlyph* glyph)
{
    /// Palette used by the raw outline-only glyph pass.
    enum {
        /// 4bpp outline CLUT at VRAM word X=1008, Y=511.
        ///
        /// `getClut` packs X / 16 and Y into selector 0x7FFF. This is the final
        /// 16-color block uploaded by `Text_LoadClutImages`: indices 0..5 are
        /// transparent, 6..9 have RGB5 gray levels 1, 3, 6 and 9, and 10..15
        /// are white. Nonzero colors enable blending; the raw sprite subtracts
        /// their coverage from the background when the font page selects
        /// `GPU_BLEND_SUBTRACT`. The selector itself carries no blend mode.
        TEXT_OUTLINE_ONLY_GLYPH_CLUT = getClut(0x3F0, 0x1FF),
    };

    setSprt(outline);
    setSemiTrans(outline, true);
    setShadeTex(outline, true);
    outline->x0   = request->x + glyph->xOffset;
    outline->y0   = (request->y - glyph->heightMinusOne) + glyph->yOffset;
    outline->u0   = glyph->u;
    outline->v0   = glyph->v + request->vBias;
    outline->w    = glyph->widthMinusOne + 1;
    outline->h    = glyph->heightMinusOne + 1;
    outline->clut = TEXT_OUTLINE_ONLY_GLYPH_CLUT;
}

/// Queues one subtractive glyph outline without a fill or RGB modulation.
///
/// Borrows `request` and `glyph` without modifying or retaining them or advancing
/// the pen. `unusedColor` preserves the shared glyph-drawer callback signature;
/// the raw-texture command ignores RGB, including the packet's untouched colors.
/// Pen coordinates and offsets are draw-environment pixels. X/Y narrow to signed
/// 16-bit fields; the last row lies at pen Y plus the glyph's Y offset. U/V are
/// page-local texels, V wraps modulo 256 after adding the signed bias, and
/// minus-one dimensions decode to 1..256 pixels and texels.
///
/// Reserves one `SPRT` (20 bytes) at the word-aligned `gGpuPrimCursor`. The arena
/// and signed `request->otIndex` entry in `gGpuCurrentOt` must be writable and in
/// bounds. Font textures and the final palette from `Text_LoadClutImages` must
/// already be resident. The caller must prepend a 4bpp font-page command with
/// subtractive blending to this entry; packets remain live until GPU completion.
static void _textDrawGlyphOutline(TextDrawReq* request, const _FontGlyph* glyph, s32 unusedColor)
{
    SPRT* outline;

    outline        = gGpuPrimCursor;
    gGpuPrimCursor = outline + 1;
    _textInitOutlineGlyphSprite(outline, request, glyph);
    addPrim(gGpuCurrentOt + request->otIndex, outline);
}

static void Text_UiTaskCallback(Task* task)
{
    UiObject* obj;
    s16       temp;

    if (task->state == 0) {
        Wip_UiHolder = NULL;
        obj          = uiSpawnObject(Ui_OverlayLoadingDesc, 1, 1, 2, 0);
        if (obj != NULL) {
            task->spawnArg2.pointer = obj;
            task->state             = task->state + 1;
        }
    } else if (task->state == 1) {
        obj = task->spawnArg2.pointer;
        if (obj->result == USER_INTERFACE_RESULT_CANCEL || obj->result == USER_INTERFACE_RESULT_CONFIRM) {
            task->killCountdown = 0xA;
            task->state         = task->state + 1;
            uiStartTreeClosing(obj, obj->owner);
        }
    } else {
        temp                = task->killCountdown - gDisplayState.frameTicks;
        task->killCountdown = temp;
        if (temp <= 0) {
            taskSpawn(0, 2, 0xC, 0);
            taskCallExit(task);
        }
    }
}

static void Text_BootTask(Task* task)
{
    Text_LoadClutImages();
    displayConfigureFramebuffers(DISPLAY_SETUP_DEFAULT);
    Game_ClearSession();
    taskSpawnFromTable(Title_TaskDescs, 0, 0, 0);
    taskKill(task);
}

/// Overflow and zero texts of the number formatters.
static const char Text_MaxEightDigits[] = "99999999";
static const char Text_ZeroDigit[]      = "0";
static const char Text_MaxNineDigits[]  = "999999999";
