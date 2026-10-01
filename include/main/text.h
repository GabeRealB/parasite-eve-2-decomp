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
    TEXT_GLYPH_TABLE_LARGE           = 4,
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
    TEXT_ALIGNMENT_LEFT = 0,
    /// Centers a measured UI-text line on the request's initial X coordinate.
    ///
    /// Stored in `TextDrawReq::alignment`. `Text_DrawString` and
    /// `Text_MeasureAndCenter` subtract `width >> 1` pixels from X, so odd
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
    /// alignment call. `Text_MeasureWidth` uses X=0 and returns the negated X.
    TEXT_ALIGNMENT_RIGHT = 2,
};

/// Glyph drawing paths selected by `TextDrawReq::drawMode`.
enum {
    TEXT_DRAW_QUEUED = 0, // Opaque fill in the selected OT entry.
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
    TEXT_DRAW_TRANSLUCENT_OUTLINED  = 3, // Translucent fill, alternate outline palette in the next OT entry.
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
/// `Text_MeasureAndCenter` adjusts X for alignment; `Text_DrawString` also
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
    s8  drawMode;   // Path (0 queued, 1 outlined, 2 outlined single entry, 3 translucent outlined, 4 outline only, 16 immediate).
    s8  vBias;      // Texture V offset in texels, added modulo 256 (38 medium, 0 small, -128 large).
} TextDrawReq;
STATIC_ASSERT_SIZEOF(TextDrawReq, 0x10);

/// 4-byte glyph UVWH entry used by TextStream_Draw (tables like D_800627E0).
/// Distinct from _FontGlyph (0xC full font metrics).
typedef struct _GlyphUvwh {
    /* 0x0 */ u8 u;
    /* 0x1 */ u8 v;
    /* 0x2 */ u8 w;
    /* 0x3 */ u8 h;
} GlyphUvwh;
STATIC_ASSERT_SIZEOF(GlyphUvwh, 0x4);

/// Caption font cells shared with room title sequences.
extern GlyphUvwh Caption_Glyphs[];

void Text_MeasureAndCenter(TextDrawReq* request, u8* arg1);

u8* Text_SkipLines(u8* arg0, s32 arg1);

s32 Text_DrawMultiLine(UiObject* object, s32 arg1, s32 arg2, u8* arg3, s32 arg4, s32 arg5, s32 arg6);

s32 Text_MeasureWidth(u8* arg0);

s32 Text_DrawPrompt(UiObject* object, s32 arg1, s32 arg2, u8* arg3, s32 arg4, s32 arg5, s32 arg6);

/// EXE palettes over the title font clut dests. Text_FillClutPixels (64) → (256, 243);
/// Text_OutlineClutPixels (48) → (0x3D0, 0x1FF) = clut 0x7FFD/E/F. TIM pe2clut_0 row 0 is
/// empty; UI text uses 0x7FFD (indices 0–10 skip). Called from Title_InitTask.
void Text_LoadClutImages(void);

/// Draws encoded text using the request's glyph table, placement and style.
void Text_DrawString(TextDrawReq* request, u8* text);

u8* Text_ItoaSigned(u8* arg0, s32 arg1);

u8* Text_ItoaSignedPlus(u8* arg0, s32 arg1);

u8* Text_ItoaUnsigned(u8* arg0, u32 arg1);

/// Formats a decimal integer with the requested minimum width.
u8* Text_ItoaPadded(u8* buffer, s32 value, s32 width);

u8* Text_Strcat(u8* dest, u8* src);

u8* Text_FormatTime(u8* arg0, u16 time);

#endif // MAIN_TEXT_H
