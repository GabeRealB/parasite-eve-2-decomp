#ifndef MAIN_TEXT_H
#define MAIN_TEXT_H

#include "common.h"

#include "main/ui_types.h"

/// Text-measure / draw-request block passed to Text_MeasureAndCenter / Text_DrawString.
/// glyphTable: 0 → Font_Glyphs0, 5 → Font_Glyphs2, else Font_Glyphs1.
/// Text_DrawString writes vBias (0x26 / 0 / 0x80) from that selector; SPRT v is
/// glyph.v + vBias. Ui_DrawTextUnderline uses glyphTable 5.
typedef struct _TextDrawReq {
    /* 0x00 */ s16 x;
    /* 0x02 */ s16 y;
    /* 0x04 */ s32 otIndex;
    /* 0x08 */ s32 field_8;
    /* 0x0C */ s8  glyphTable;
    /* 0x0D */ s8  centerMode;
    /* 0x0E */ s8  field_E;
    /* 0x0F */ s8  vBias;
} TextDrawReq;
STATIC_ASSERT_SIZEOF(TextDrawReq, 0x10);

/// 4-byte glyph UVWH entry used by TextStream_Draw (tables like D_800627E0).
/// Distinct from FontGlyph (0xC full font metrics).
typedef struct _GlyphUvwh {
    /* 0x0 */ u8 u;
    /* 0x1 */ u8 v;
    /* 0x2 */ u8 w;
    /* 0x3 */ u8 h;
} GlyphUvwh;
STATIC_ASSERT_SIZEOF(GlyphUvwh, 0x4);

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
