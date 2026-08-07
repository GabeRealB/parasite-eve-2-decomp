#ifndef TEXT_H
#define TEXT_H

#include "common.h"

// Types — font / text draw

/// Text-measure / draw-request block passed to Text_MeasureAndCenter / func_8002E53C.
/// field_0/field_2 = x/y (or accumulate measured width); field_4 = OT priority;
/// field_C selects Font_Glyphs0/1/2; field_D = center mode (1=half, 2=full);
/// field_F = v bias added when drawing sprites.
typedef struct _TextDrawReq {
    /* 0x00 */ s16 field_0; // x
    /* 0x02 */ s16 field_2; // y
    /* 0x04 */ s32 field_4; // otIndex
    /* 0x08 */ s32 field_8;
    /* 0x0C */ s8  field_C; // glyphTable
    /* 0x0D */ s8  field_D; // centerMode
    /* 0x0E */ s8  field_E;
    /* 0x0F */ u8  field_F; // vBias
} TextDrawReq;
STATIC_ASSERT_SIZEOF(TextDrawReq, 0x10);
/// Per-glyph metrics in the font tables (Font_Glyphs0 / Font_Glyphs1 / Font_Glyphs2).
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
/// Draw params for Prim_DrawSprt (SPRT) / Prim_DrawTile (TILE).
/// field_0/2 = x/y; field_4/6 = u/v (SPRT); field_8/A = w/h (inclusive, decremented
/// when written); field_C/D/E = RGB; field_10 = 0 shade-tex / nonzero semi-trans.
typedef struct _PrimDrawParams {
    /* 0x00 */ s16 field_0;  // x
    /* 0x02 */ s16 field_2;  // y
    /* 0x04 */ u8  field_4;  // u
    /* 0x05 */ u8  pad_5;
    /* 0x06 */ u8  field_6;  // v
    /* 0x07 */ u8  pad_7;
    /* 0x08 */ s16 field_8;  // w
    /* 0x0A */ s16 field_A;  // h
    /* 0x0C */ u8  field_C;  // r
    /* 0x0D */ u8  field_D;  // g
    /* 0x0E */ u8  field_E;  // b
    /* 0x0F */ u8  pad_F;
    /* 0x10 */ s16 field_10; // shadeMode
} PrimDrawParams;
STATIC_ASSERT_SIZEOF(PrimDrawParams, 0x12);
/// 4-byte glyph UVWH entry used by TextStream_Draw (tables like D_800627E0).
/// Distinct from FontGlyph (0xC full font metrics).
typedef struct _GlyphUvwh {
    /* 0x0 */ u8 u;
    /* 0x1 */ u8 v;
    /* 0x2 */ u8 w;
    /* 0x3 */ u8 h;
} GlyphUvwh;
STATIC_ASSERT_SIZEOF(GlyphUvwh, 0x4);
/// Text stream / font draw object (e.g. D_800630B0).
/// field_0/2 = x/y; field_4/6 = tpage xy (u base = field_4 & 0x3F);
/// field_8/A = clut xy; field_C = per-char delay; field_E = stream cursor;
/// field_10 = char stream (0xFE newline, 0xFF end); field_14 = GlyphUvwh table;
/// field_18 = line height; field_1A = delay reload on line advance.
typedef struct _TextStream {
    /* 0x00 */ s16        field_0;  // x
    /* 0x02 */ s16        field_2;  // y
    /* 0x04 */ s16        field_4;  // tpageX
    /* 0x06 */ s16        field_6;  // tpageY
    /* 0x08 */ s16        field_8;  // clutX
    /* 0x0A */ s16        field_A;  // clutY
    /* 0x0C */ s16        field_C;  // charDelay
    /* 0x0E */ s16        field_E;  // cursor
    /* 0x10 */ u8*        field_10; // chars
    /* 0x14 */ GlyphUvwh* field_14; // glyphs
    /* 0x18 */ s16        field_18; // lineHeight
    /* 0x1A */ s16        field_1A; // delayReload
    /* 0x1C */ s16        field_1C;
    /* 0x1E */ s16        field_1E;
} TextStream;
STATIC_ASSERT_SIZEOF(TextStream, 0x20);

#endif // TEXT_H
