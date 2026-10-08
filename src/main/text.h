#ifndef MAIN_PRIVATE_TEXT_H
#define MAIN_PRIVATE_TEXT_H

#include "types.h"

#include "main/text.h"

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

/// Steps a caption reveal and queues its visible glyph prefix for this draw frame.
///
/// Phase 0 resets the cursor and arms the frame countdown; phase 1 redraws and
/// advances it, and phases >=2 report completion. Returns 0 while progressing,
/// -1 when advancing onto the script terminator, and 1 in a completed phase.
/// A negative charDelay initially reveals all content bytes. Countdown expiry
/// means decrementing below zero; the signed-halfword cursor/countdown narrowing
/// is retained. The caller keeps the record, script, phase and countdown alive
/// between calls. The script must end before cursor overflow; glyphs must cover
/// all low-seven-bit indices. Phase 1 also reads the byte before the cursor,
/// including chars[-1] when cursor is zero, and one byte beyond the terminator
/// when advancing past it. Both addresses must be readable; the resident
/// scripts' surrounding storage contract remains unproven.
/// Nonzero skipMarkedGlyphs omits high-bit glyphs but retains their pen advance;
/// current resident callers pass zero. Font textures, CLUT, OT entry 4 and room
/// for the emitted sprites and texture-page command must be ready for drawing.
s32 textDrawStream(TextStream* stream, u8* phase, s16* framesLeft, s32 skipMarkedGlyphs);

/// Returns packed pixel dimensions used to size a UI panel from encoded text.
///
/// Width occupies bits 0..15 and height bits 16..31. Each parse adds 15 pixels
/// of height. Width is the largest nonnegative result from large-face, right
/// alignment, retaining its signed-16-bit X narrowing; empty text gives width 0.
/// No GPU resources are needed. Text is borrowed, read-only and not retained.
///
/// Only the first source line is copied. After a break, the copy is reparsed
/// in place rather than advancing through later source lines. An unbroken
/// line therefore has height 15; an ordinary first break gives height 30 and
/// the first line's width. A doubled backslash followed by N/n at the start
/// of a broken first line exposes a break in the copy: three passes give
/// height 45 and width 0. Z/z instead ends on the second pass (height 30).
/// These retained dimensions need not cover the lines drawn by `textDrawUiLines`.
///
/// The copied line must fit in 64 bytes (63 content bytes plus NUL), with no
/// capacity check. Source markers, doubled backslashes and Shift-JIS lead-byte
/// readability follow `textDrawUiLines`; copied commands and glyphs must obey
/// `textAlignLine`'s operand-readability and glyph-index requirements on each pass.
u32 textMeasureUiTextSize(const u8* text);

#endif // MAIN_PRIVATE_TEXT_H
