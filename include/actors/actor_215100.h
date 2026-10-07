#ifndef INCLUDE_ACTORS_ACTOR_215100_H
#define INCLUDE_ACTORS_ACTOR_215100_H

#include "types.h"

/// Selects a keyed caption record and caches its screen-pixel text layout.
///
/// commandIndex is zero-based and must be below the loaded `CapCommandTable`'s
/// count and in 0..32767. A null entry hides the caption and returns 1, retaining
/// the other selection state. Otherwise key must be in 0..255 and match a
/// nonterminal record in slots 1..32767; a missing key reaches the terminal and
/// its invalid text pointer is still measured. The command's key-selection mode
/// is ignored. Returns 0 after selection; this neither draws nor starts a task.
///
/// bottomBaselineY is the last text baseline in screen pixels, stored as a
/// signed halfword before layout measurement. Selection resets the caret's
/// thirty-draw delay. Keep this actor overlay and the relocated CAP file and
/// glyph cells loaded through caption use. Text must end within 32768 u16 words
/// and every nonnegative code's low-ten-bit glyph index must exist in the table.
s32 actor215100CapCaptionSelectRecord(s16 commandIndex, s16 key, s32 bottomBaselineY);

/// Starts Pierce's next shooting-gallery conversation from persistent talk progress.
///
/// Requires this actor overlay and the gallery's conversation resources to be
/// loaded, with event playback available. Progress 0 starts the first scene
/// and becomes 1 before playback; progress 1 starts the second and becomes 2
/// after starting playback. Progress 2 repeats the final scene. Other values
/// do nothing. The event script hides the HUD and restores it on completion.
void actor215100StartPierceConversation(void);

/// Queues the actor's selected caption, background, title and continuation caret.
///
/// Does nothing without a selected sequence, at its terminal record, or while
/// resident CAP playback is busy. The selected slot must be in 1..32767 within
/// a live relocated sequence. Keep this actor overlay and its caption file,
/// glyph metrics, textures and palettes loaded; selection must already have
/// established the block metrics. Text must end within 32768 u16 words, use
/// valid glyph indices and icon selectors 0..3, and provide a line break for
/// the caret position. OT entries 2 and 3 and enough primitive storage must
/// remain writable and live through GPU completion.
///
/// Draws all text without advancing the sequence. Title 0 means none; 1..255
/// selects glyph 0..254. The title-bank flag is passed through but ignored by
/// this drawer. Instant-text records suppress the caret; other drawing calls
/// step its initial thirty-call delay, then emit and pulse it.
void actor215100CapCaptionDrawCurrent(void);

#endif // INCLUDE_ACTORS_ACTOR_215100_H
