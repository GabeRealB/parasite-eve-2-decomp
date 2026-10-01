#ifndef MAIN_PRIVATE_TEXT_H
#define MAIN_PRIVATE_TEXT_H

#include "types.h"

#include "main/text.h"
#include "text_types.h"

/// Draws opaque, color-modulated glyphs directly in the active draw environment.
///
/// Stored as 16 in `TextDrawReq::drawMode`. Sets the font texture page and
/// submits each fill sprite through `DrawPrim`, reusing private packets.
/// The draw environment, font textures and fill palette must already be ready.
/// While this mode remains active, `otIndex` is ignored and drawing consumes
/// no ordering-table entries or primitive-arena space.
/// Inline `\w0` or `\w1` (either letter case) stores a queued outlined mode
/// in the request; subsequent glyphs and end-of-line texture-page packets then
/// require primitive space and writable OT entries at `otIndex` and `otIndex + 1`.
enum { TEXT_DRAW_IMMEDIATE = 16 };

s32 TextStream_Draw(TextStream* stream, u8* arg1, s16* arg2, s32 arg3);

s32 Text_MeasureMultiLine(u8* arg0);

#endif // MAIN_PRIVATE_TEXT_H
