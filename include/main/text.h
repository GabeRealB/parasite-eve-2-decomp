#ifndef MAIN_TEXT_H
#define MAIN_TEXT_H

#include "common.h"

#include "main/ui_types.h"

/// Glyph-table selectors; every selector except medium and small uses large metrics.
enum {
    /// Selects the medium UI face for initial drawing and line measurement.
    ///
    /// Stored as 0 in `TextDrawReq::glyphTable`. Glyph bytes 0x20..0xFF index
    /// the metrics by subtracting space; drawing adds 38 texels to texture V
    /// modulo 256.
    /// Eligible kerning pairs tighten by two pixels. Inline font commands
    /// change the drawing face and V bias without changing this selector, so
    /// pair tightening remains two pixels and measurement keeps medium metrics.
    TEXT_GLYPH_TABLE_MEDIUM          = 0,
    TEXT_GLYPH_TABLE_LARGE_ALTERNATE = 2,
    /// Selects the large UI face for initial drawing and line measurement.
    ///
    /// Stored as 4 in `TextDrawReq::glyphTable`. Glyph bytes 0x20..0xFF index
    /// the metrics by subtracting space; drawing initializes `vBias` to -128,
    /// adding 128 texels to texture V modulo 256. Eligible kerning pairs
    /// tighten by two pixels.
    /// Inline font commands change the drawing face and V bias without changing
    /// this selector, so alignment measurement keeps large metrics and pair
    /// tightening remains two pixels. `TEXT_GLYPH_TABLE_LARGE_ALTERNATE` has
    /// the same drawing and measurement behavior.
    TEXT_GLYPH_TABLE_LARGE = 4,
    /// Small UI face, covering character bytes 0x20..0x7A.
    ///
    /// Drawing stores V bias 0. Pair kerning tightens by one pixel while the
    /// request's `glyphTable` remains this selector. An inline face command
    /// changes the live metrics and V bias without storing a new selector, so
    /// kerning still follows the request.
    TEXT_GLYPH_TABLE_SMALL = 5,
};

/// Placement of the measured line relative to its initial X coordinate.
enum {
    /// Starts an encoded UI-text line at the request's current pen X.
    ///
    /// Selector 0, stored in the signed byte `TextDrawReq::alignment`.
    /// Alignment skips width measurement and leaves X unchanged, including
    /// for empty lines. Drawing still applies glyph offsets, pair kerning and
    /// inline position commands, advancing the mutable X/Y pen while retaining
    /// this selector. Reinitialize placement before drawing an independent line.
    TEXT_ALIGNMENT_LEFT = 0,
    /// Centers a measured UI-text line on the request's initial X coordinate.
    ///
    /// Stored in `TextDrawReq::alignment`. `textDrawString` and
    /// `textAlignLine` subtract `width >> 1` pixels from X, so odd
    /// nonnegative widths use their rounded-down half. Measurement uses the
    /// initial glyph table and kerning even when inline commands change the
    /// drawing metrics or position. If no glyph is measured, width is -4 and
    /// X moves two pixels right. The selector stays set; restore the X anchor
    /// before applying alignment again.
    TEXT_ALIGNMENT_CENTER = 1,
    /// Anchors the measured UI-text line's right edge at its initial X coordinate.
    ///
    /// Stored in `TextDrawReq::alignment`. Alignment subtracts the full measured
    /// width in pixels from X, retaining the selector. Measurement uses the
    /// initial glyph table and pair kerning, omitting the final glyph's extra
    /// pen advance; inline font and position commands do not change that width.
    /// With no measured glyph, width is -4 and X moves four pixels right.
    /// The result is stored in signed 16-bit X; restore the anchor before another
    /// alignment call. `textMeasureLineWidth` uses X=0 and returns the negated X.
    TEXT_ALIGNMENT_RIGHT = 2,
};

/// Glyph drawing paths selected by `TextDrawReq::drawMode`.
enum {
    /// Queues opaque, color-modulated glyph fills without an outline.
    ///
    /// Selector 0 in `TextDrawReq::drawMode`. Fill RGB starts at `colorRgb`
    /// and follows inline color commands. While this mode remains selected,
    /// only the signed `otIndex` entry in `gGpuCurrentOt` is needed. Each glyph
    /// reserves one sprite (20 bytes); a line ending in this mode reserves one
    /// texture-page packet (8 bytes), even when no glyph was drawn. Packets are
    /// prepended to the entry, so the texture-page setup executes before the fills.
    /// The entry and primitive storage must be writable, with sufficient capacity;
    /// font textures and the fill palette must already be loaded.
    /// Inline `\w0` or `\w1` (either letter case) switches to translucent
    /// outlined or outlined text and stores that mode in the request;
    /// subsequent drawing also needs `otIndex + 1` and space for both passes.
    TEXT_DRAW_FILL_ONLY = 0,
    /// Draws opaque, color-modulated glyphs with a subtractive outline.
    ///
    /// Selector 1 in `TextDrawReq::drawMode`. The fill uses `colorRgb` at
    /// `otIndex`; the unmodulated outline uses `otIndex + 1`. Both indices count
    /// entries in `gGpuCurrentOt` and must be writable. Each glyph reserves two
    /// sprites in the active primitive buffer. A line ending in this mode also
    /// reserves one texture-page packet in each entry. Font textures and text
    /// palettes must already be loaded.
    /// Inline `\w1` or `\W1` also selects this path and stores the selector in
    /// the request; inline `\w0` or `\W0` switches to translucent outlined text.
    TEXT_DRAW_OUTLINED = 1,
    /// Draws opaque glyph fills and subtractive outlines in one ordering-table entry.
    ///
    /// Selector 2 in `TextDrawReq::drawMode`. Both passes use the signed
    /// `otIndex` entry in `gGpuCurrentOt`; no following entry is needed while
    /// this mode remains selected. Each glyph's unmodulated outline executes
    /// before its color-modulated fill, with a texture-page packet before each
    /// pass. Fill RGB starts at `colorRgb` and follows inline color commands.
    /// Each glyph reserves two sprites and two texture-page packets (56 bytes)
    /// in the active primitive buffer. A line ending in this mode reserves one
    /// additional texture-page packet (8 bytes), even when no glyph was drawn.
    /// The entry and packet storage must be writable, with sufficient capacity;
    /// font textures and text palettes must already be loaded.
    /// Inline `\w0` or `\w1` (either letter case) changes the request to
    /// translucent outlined or outlined text, which also needs `otIndex + 1`.
    TEXT_DRAW_OUTLINED_SINGLE_ENTRY = 2,
    /// Draws additively blended, color-modulated glyphs with a subtractive outline.
    ///
    /// Selector 3 in `TextDrawReq::drawMode`. The fill uses `colorRgb` and
    /// inline color commands at the signed `otIndex` entry in `gGpuCurrentOt`;
    /// the outline uses the alternate outline palette without RGB modulation
    /// at `otIndex + 1`. Both indices count entries and must be writable.
    /// Each glyph reserves two sprites (40 bytes) in the active primitive
    /// buffer. A line ending in this mode also reserves one texture-page
    /// packet in each entry (16 bytes total), even when no glyph was drawn.
    /// Font textures and text palettes must already be loaded, and packet
    /// storage must have sufficient capacity. Packets are borrowed by the GPU;
    /// keep them intact until the ordering table has been consumed.
    /// Inline `\w0` or `\W0` selects this mode and stores it in the request;
    /// inline `\w1` or `\W1` switches to `TEXT_DRAW_OUTLINED`, retaining
    /// the two-entry and two-sprite requirements.
    TEXT_DRAW_TRANSLUCENT_OUTLINED = 3,
    /// Queues the subtractive outline pass without a glyph fill.
    ///
    /// Selector 4 in `TextDrawReq::drawMode`. Uses the outline palette without
    /// RGB modulation, so `colorRgb` and inline color commands do not tint it.
    /// While this mode remains selected, only the signed `otIndex` entry in
    /// `gGpuCurrentOt` is needed. Each glyph reserves one sprite (20 bytes);
    /// a line ending in this mode reserves one texture-page packet (8 bytes),
    /// even when no glyph was drawn. The entry and primitive storage must be
    /// writable, with sufficient capacity; font textures and text palettes
    /// must already be loaded.
    /// To pair it with a separate fill in the same OT entry, queue the fill
    /// first and restore the placement before queuing this pass: packets are
    /// prepended, so the outline executes before the fill.
    /// Inline `\w0` or `\w1` (either letter case) switches to translucent
    /// outlined or outlined text and stores that mode in the request;
    /// subsequent drawing also needs `otIndex + 1` and space for both passes.
    TEXT_DRAW_OUTLINE_ONLY = 4,
};

/// Mutable placement and style for one encoded UI-text line.
///
/// `textAlignLine` adjusts X for alignment; `textDrawString` also
/// advances the X/Y pen and lets inline commands change drawMode and vBias.
/// Reinitialize placement before drawing an independent line. Drawing initializes
/// vBias, so callers need not set it. The request is borrowed only during a call.
/// Medium/large tables cover character bytes 0x20..0xFF; small covers 0x20..0x7A.
/// OT indices count entries relative to `gGpuCurrentOt`, including its reserved
/// negative entries; outlined modes 1 and 3 require the following entry as well.
typedef struct {
    s16 x;          // Pen X in draw-environment pixels; initially the alignment anchor.
    s16 y;          // Pen baseline Y in draw-environment pixels.
    s32 otIndex;    // Signed OT entry index; unused by immediate drawing.
    u32 colorRgb;   // Initial modulation RGB in bits 0..23 (R low byte); command byte ignored.
    s8  glyphTable; // Initial metrics (0 medium, 5 small, otherwise large; callers also use 2 and 4).
    s8  alignment;  // Horizontal placement (0 left, 1 center, 2 right; other values leave X unchanged).
    s8  drawMode;   // Path (0 fill only, 1 outlined, 2 outlined single entry, 3 translucent outlined, 4 outline only, 16 immediate).
    s8  vBias;      // Texture V offset in texels, added modulo 256 (38 medium, 0 small, -128 large).
} TextDrawReq;
STATIC_ASSERT_SIZEOF(TextDrawReq, 0x10);

/// Texture cell for a caption glyph, title label or inline icon.
///
/// Renderers supply the texture page and palette, and add any texture-origin
/// offsets before storing GPU coordinates modulo 256. The stream sprite path
/// subtracts one from both extents; CAP quads use them as corner deltas.
/// Glyph indices and table lengths belong to the containing text format.
/// CAP tables borrow the relocated file's storage for drawing and measurement.
typedef struct {
    u8 u;      // Texture U in texels, before renderer-supplied offsets.
    u8 v;      // Texture V in texels, before renderer-supplied offsets.
    u8 width;  // Cell width in texels; pen advance depends on the text format.
    u8 height; // Cell height in texels; zero suppresses stream sprites.
} TextGlyphCell;
STATIC_ASSERT_SIZEOF(TextGlyphCell, 0x4);

/// Caption font cells shared with room title sequences.
extern TextGlyphCell Caption_Glyphs[];

/// Glyph-script encoding for `TextStream::chars`.
enum {
    /// Low 7 bits of a glyph byte: the cell index in `glyphs`.
    TEXT_STREAM_GLYPH_INDEX_MASK = 0x7F,
    /// Line break. The pen returns to its origin X and advances by the line height.
    TEXT_STREAM_LINE_BREAK = 0xFE,
    /// End of the glyph script.
    TEXT_STREAM_END = 0xFF,
};

/// Placement, font and timing for one progressively revealed caption.
///
/// `charDelay` is the number of frames between glyphs. A negative value reveals
/// the whole script on the first step. `delayReload` is the hold, in frames,
/// after the last glyph, and the countdown used for that immediate reveal.
/// `cursor` counts script bytes already revealed, including line breaks.
///
/// The caller keeps the record, the script and the glyph cells for the whole
/// reveal. Only `cursor` is written while a caption is revealing. Bit 7 of a
/// glyph byte marks a glyph one drawer can omit when its caller asks to filter;
/// that drawer indexes cells with the low 7 bits. `boxWidth` and `boxHeight`
/// are the backing plate in pixels. A drawer may leave the plate unpainted.
typedef struct {
    s16            x;           // Pen origin X in draw-environment pixels.
    s16            y;           // Pen origin Y in draw-environment pixels.
    s16            tpageX;      // Font texture X in VRAM. The low 6 bits are the sprite U origin.
    s16            tpageY;      // Font texture Y in VRAM. The low 8 bits are the sprite V origin.
    s16            clutX;       // Palette X in VRAM.
    s16            clutY;       // Palette Y in VRAM.
    s16            charDelay;   // Frames between glyphs. Negative reveals the whole script at once.
    s16            cursor;      // Script bytes already revealed, including line breaks.
    u8*            chars;       // Glyph-index script, ended by `TEXT_STREAM_END`.
    TextGlyphCell* glyphs;      // Cell table indexed by a glyph byte's low 7 bits.
    s16            lineHeight;  // Pixels added to the pen Y at a line break.
    s16            delayReload; // Frames to hold the caption after the last glyph.
    s16            boxWidth;    // Backing plate width in pixels.
    s16            boxHeight;   // Backing plate height in pixels.
} TextStream;
STATIC_ASSERT_SIZEOF(TextStream, 0x20);

/// Screen rectangle, texture origin and colour for one `TILE` or `SPRT`.
///
/// The caption drawers fill one on the stack and hand it to their primitive
/// emitters, which write the rectangle, colour and blend mode into the next
/// primitive; only the `SPRT` emitter reads `u`/`v`, and takes their low byte.
/// The caller builds the texture origin as a halfword (glyph offset plus the
/// font page's origin) and keeps the record only for the call.
typedef struct {
    s16 x; // Left edge in draw-environment pixels.
    s16 y; // Top edge in draw-environment pixels.
    s16 u; // Texture U; the primitive takes the low byte.
    s16 v; // Texture V; the primitive takes the low byte.
    s16 w; // Width in pixels; the primitive is written `w - 1`.
    s16 h; // Height in pixels; the primitive is written `h - 1`.
    u8  r; // Colour, or texture modulation when `semiTrans` is set.
    u8  g;
    u8  b;
    u8  pad_F;
    s16 semiTrans; // 0 opaque with the texture drawn unmodulated; nonzero semi-transparent and colour-modulated.
    s16 unused_12; // Captions store `ONE` here; no emitter reads it, and its role is unproven.
} PrimDrawParams;
STATIC_ASSERT_SIZEOF(PrimDrawParams, 0x14);

/// Applies horizontal alignment to one encoded UI-text line's X anchor.
///
/// Borrows `request` and `text` for this call, retaining neither. Only `x`,
/// `glyphTable` and `alignment` need initialization. X is in draw-environment
/// pixels: center subtracts the measured width shifted right by one, right
/// subtracts the full width, and other selectors skip measurement and leave X
/// unchanged. The result narrows to signed 16-bit X; the selector stays set.
/// Restore the anchor before another alignment call or `textDrawString`, which
/// applies alignment again. No other request field or text byte is changed.
///
/// Width is the offset to the final glyph's rightmost pixel, including pair
/// kerning and excluding its extra pen advance. Metrics and kerning use the
/// initial `glyphTable`; inline font, position, color and style commands have
/// no effect on alignment. With no measured glyph, width is -4, moving X two
/// pixels right for center alignment or four for right alignment.
///
/// NUL, LF and a case-insensitive \\n command end measurement; CR does not.
/// For center/right alignment, `text` must be readable through that terminator.
/// Each \\B, \\C, \\D, \\S, \\U or \\W command (either case) needs a readable
/// operand and a readable byte after it, even when the operand is NUL.
/// Unrecognized escapes discard the backslash; adjacent backslashes continue
/// scanning commands. Every byte reaching glyph indexing must be 0x20..0x7A
/// for the small face or 0x20..0xFF otherwise; there is no range check.
void textAlignLine(TextDrawReq* request, const u8* text);

/// Returns the borrowed suffix after up to `lineCount` encoded line breaks.
///
/// Counts LF and N/n immediately preceded by a backslash; CR is not a break.
/// Stops at NUL and returns its address when fewer breaks remain. A nonpositive
/// count returns `text` without reading it. The source is never modified.
/// The source must be readable through NUL or the requested break. If its first
/// byte is N/n, the byte before `text` must also be readable: this scanner tests
/// the preceding byte without tracking whether it has advanced yet.
const u8* textSkipLines(const u8* text, s32 lineCount);

s32 Text_DrawMultiLine(UiObject* object, s32 arg1, s32 arg2, const u8* arg3, s32 arg4, s32 arg5, s32 arg6);

/// Measures one encoded UI-text line with the large face, in pixels.
///
/// Borrows read-only unsigned bytes under `textAlignLine`'s readability and
/// glyph-index contract. NUL, LF and a case-insensitive \\n command end the
/// measurement; CR does not. Inline styling and position commands are skipped.
/// Includes pair kerning and excludes the final glyph's extra pen advance;
/// returns -4 when no glyph is measured. The measured width is negated into a
/// signed 16-bit X, then negated as s32 for the return, retaining that narrowing.
s32 textMeasureLineWidth(const u8* text);

/// Draws one large encoded UI-text line relative to a panel or at absolute pixels.
///
/// With an object, X/Y are content pixels: the panel origin is added and three
/// pixels are subtracted from Y for the glyph baseline. A hidden panel skips drawing
/// and returns zero. Otherwise returns the final signed-16-bit pen X minus the
/// panel's signed origin X, suitable for continuing text on the same line.
/// A NULL object uses X/Y directly in draw-environment pixels and returns the
/// original s32 X argument, irrespective of alignment or glyph advance.
/// Both paths narrow pen coordinates to signed 16 bits. Panel-origin additions
/// use the unsigned halfword views; the final origin subtraction uses signed X.
///
/// `colorRgb` packs R/G/B into bits 0..23 (R low byte); the high byte is ignored.
/// `drawMode` and `alignment` narrow to signed bytes selecting `TEXT_DRAW_*`
/// and `TEXT_ALIGNMENT_*`. Initial metrics are large; inline commands can
/// change drawing style under `textDrawString`'s stream contract. Text and the
/// object are borrowed only for the call, and neither is modified or retained.
/// Fonts, palettes, OT entries and primitive capacity must satisfy that drawer's
/// requirements: the starting OT entry is panel base + 1, or 4 without an object.
s32 textDrawUiLine(const UiObject* object, s32 x, s32 y, const u8* text, u32 colorRgb, s32 drawMode, s32 alignment);

/// EXE palettes over the title font clut dests. Text_FillClutPixels (64) → (256, 243);
/// Text_OutlineClutPixels (48) → (0x3D0, 0x1FF) = clut 0x7FFD/E/F. TIM pe2clut_0 row 0 is
/// empty; UI text uses 0x7FFD (indices 0–10 skip). Called from Title_InitTask.
void Text_LoadClutImages(void);

/// Draws one encoded UI-text line and advances the request's pixel pen.
///
/// Borrows both arguments for the call. Reads unsigned text bytes without
/// modifying them; updates X/Y, initializes vBias and lets inline commands
/// change vBias and drawMode. Color commands affect only this call's live RGB.
/// Reinitialize the placement before drawing an independent line.
/// Alignment and pair-tightening scale use the initial glyphTable even after
/// a font command changes the live metrics. Center/right alignment has the
/// measurement contract of `textAlignLine`, including width -4 for no glyphs.
///
/// Drawing ends at NUL, LF, CR or a case-insensitive \\n command. Other bytes
/// below space are skipped. Commands (either letter case) are \\C plus a color
/// letter (W/Y/O/G/H/C/R), \\S plus a face (S/M/L), \\W plus mode 0/1,
/// \\U/\\D plus a decimal digit moving Y up/down, and \\B plus a digit setting
/// X to eight times that digit. Each operand and the byte after it must be
/// readable, even if the operand is NUL. Unknown operands leave the setting;
/// unknown commands discard only the backslash. Adjacent backslashes continue
/// the command scan. Glyph bytes must be 0x20..0x7A while small metrics are
/// active, or 0x20..0xFF otherwise; there is no upper-bound check.
///
/// Font textures and palettes must be resident. Queued modes require writable
/// OT entries and sufficient word-aligned primitive storage as described by
/// `TEXT_DRAW_*`; retain packets until GPU completion. Immediate drawing uses
/// reusable private packets and the active draw environment. Inline mode
/// commands can switch it to queued drawing during the call.
void textDrawString(TextDrawReq* request, const u8* text);

/// Writes a signed decimal string, saturating its magnitude at 99,999,999.
///
/// Negative values receive one minus sign; zero is "0". `value` must be
/// -2,147,483,647..2,147,483,647 because the negative path negates it as s32.
/// `buffer` needs up to ten writable bytes including NUL. Returns `buffer`;
/// the caller owns it, no pointer is retained and no capacity is checked.
u8* textItoaSigned(u8* buffer, s32 value);

/// Prepends a sign byte to a saturated signed decimal string.
///
/// Nonnegative values produce "+0", "+1", etc. Negative values keep the
/// decimal formatter's minus as well as the prefix: -1 produces "--1".
/// Magnitudes above 99,999,999 saturate. `value` has `textItoaSigned`'s range;
/// `buffer` needs up to eleven writable bytes including NUL. Returns `buffer`
/// without retaining it; the caller owns it and no capacity is checked.
u8* textItoaSignPrefixed(u8* buffer, s32 value);

u8* Text_ItoaUnsigned(u8* arg0, u32 arg1);

/// Writes a fixed-width, zero-padded unsigned decimal string and returns `buffer`.
///
/// `digitCount` must be 1..9 and `buffer` must provide digitCount + 1 writable
/// bytes, including NUL. Values above 10^digitCount - 1 become all nines.
/// A signed negative argument converts to u32 and therefore saturates too.
/// The caller owns the buffer; no pointer is retained and no capacity is checked.
u8* textItoaPadded(u8* buffer, u32 value, s32 digitCount);

/// Appends a NUL-terminated byte string and returns the new terminator's address.
///
/// `dest` must already be terminated and have space for both strings and NUL.
/// `src` is read-only and must be terminated; the source and destination regions
/// must not overlap. The returned pointer is borrowed from `dest`, including
/// when `src` is empty. No capacity check or allocation is performed.
u8* textAppendString(u8* dest, const u8* src);

u8* Text_FormatTime(u8* arg0, u16 time);

#endif // MAIN_TEXT_H
