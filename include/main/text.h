#ifndef TEXT_H
#define TEXT_H

#include "common.h"

#include "main/task.h"
#include "main/ui.h"

// Types — font / text draw

/// Text-measure / draw-request block passed to Text_MeasureAndCenter / func_8002E53C.
typedef struct _TextDrawReq {
    /* 0x00 */ s16 x;
    /* 0x02 */ s16 y;
    /* 0x04 */ s32 otIndex;
    /* 0x08 */ s32 field_8;
    /* 0x0C */ s8  glyphTable;
    /* 0x0D */ s8  centerMode;
    /* 0x0E */ s8  field_E;
    /* 0x0F */ u8  vBias;
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
/// w/h are inclusive and decremented when written to the primitive.
typedef struct _PrimDrawParams {
    /* 0x00 */ s16 x;
    /* 0x02 */ s16 y;
    /* 0x04 */ u8  u;
    /* 0x05 */ u8  pad_5;
    /* 0x06 */ u8  v;
    /* 0x07 */ u8  pad_7;
    /* 0x08 */ s16 w;
    /* 0x0A */ s16 h;
    /* 0x0C */ u8  r;
    /* 0x0D */ u8  g;
    /* 0x0E */ u8  b;
    /* 0x0F */ u8  pad_F;
    /* 0x10 */ s16 shadeMode;
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
/// tpageX low 6 bits are the SPRT u base. chars: 0xFE newline, 0xFF end.
typedef struct _TextStream {
    /* 0x00 */ s16        x;
    /* 0x02 */ s16        y;
    /* 0x04 */ s16        tpageX;
    /* 0x06 */ s16        tpageY;
    /* 0x08 */ s16        clutX;
    /* 0x0A */ s16        clutY;
    /* 0x0C */ s16        charDelay;
    /* 0x0E */ s16        cursor;
    /* 0x10 */ u8*        chars;
    /* 0x14 */ GlyphUvwh* glyphs;
    /* 0x18 */ s16        lineHeight;
    /* 0x1A */ s16        delayReload;
    /* 0x1C */ s16        field_1C;
    /* 0x1E */ s16        field_1E;
} TextStream;
STATIC_ASSERT_SIZEOF(TextStream, 0x20);

// --- APIs (from unknown_syms) ---
s32  TextStream_Draw(TextStream* arg0, u8* arg1, s16* arg2, s32 arg3);
s32  Text_MeasureGlyphWidth(TextDrawReq* arg0, u8* arg1, u8* arg2);
void Text_DrawGlyphDualSprtA(TextDrawReq* arg0, FontGlyph* arg1, s32 arg2);
void Text_DrawGlyphDualSprt(TextDrawReq* arg0, FontGlyph* arg1, s32 arg2);
void Text_DrawGlyphDualSprtTpage(TextDrawReq* arg0, FontGlyph* arg1, s32 arg2);
void Text_MeasureAndCenter(TextDrawReq* arg0, u8* arg1);
u8*  Text_SkipLines(u8* arg0, s32 arg1);
void Text_DrawGlyphImmediate(TextDrawReq* arg0, FontGlyph* arg1, s32 arg2);
void Text_DrawGlyphQueued(TextDrawReq* arg0, FontGlyph* arg1, s32 arg2);
void Text_DrawGlyphOt(TextDrawReq* arg0, FontGlyph* arg1);
void Text_BootTask(Task* arg0);
s32  Text_ParseLine(u8** arg0, u8* arg1);
s32  Text_DrawMultiLine(UiObject* arg0, s32 arg1, s32 arg2, u8* arg3, s32 arg4, s32 arg5, s32 arg6);
s32  Text_MeasureWidth(u8* arg0);
s32  Text_MeasureMultiLine(u8* arg0);
s32  Text_DrawPrompt(UiObject* arg0, s32 arg1, s32 arg2, u8* arg3, s32 arg4, s32 arg5, s32 arg6);
s32  Text_DrawMultiLineScroll(UiObject* arg0, s32 arg1, s32 arg2, u8* arg3, s32 arg4, s32 arg5, s32 arg6, s32 arg7, s32 arg8);
void Text_LoadClutImages(void);
void Prim_DrawLoadingSprt(void);
void func_8002DEC4(void);
void func_8002E53C(TextDrawReq* arg0, u8* arg1);

// Glyph tables (selected by TextDrawReq.glyphTable); FontGlyph (0xC each).
extern u8 Font_Glyphs0[];
extern u8 Font_Glyphs1[];
extern u8 Font_Glyphs2[];
/// Image data uploaded to VRAM by Text_LoadClutImages.
extern u_long D_80060910[];
extern u_long D_800609B0[];
/// Immediate-mode SPRT scratch used by Text_DrawGlyphImmediate.
extern SPRT D_80071710;

#endif // TEXT_H
