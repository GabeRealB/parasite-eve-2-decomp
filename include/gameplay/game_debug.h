#ifndef GAMEPLAY_GAME_DEBUG_H
#define GAMEPLAY_GAME_DEBUG_H

#include "types.h"

/// Applies the diagnostic input override to one active-high controller sample.
///
/// The resident pad update calls this while gameplay is loaded and omits mode
/// zero (live input). Mode 1 invokes the development hook; every other value
/// consumes the initialized replay cursor. `buttons` borrows one writable
/// halfword and is not retained.
///
/// Replay records are native u16 button/duration pairs after the 0xD4C-byte
/// save prefix. The loaded buffer (or fixed development buffer) must remain
/// readable for 0x18000 bytes. The initialized cursor is four-byte aligned
/// relative to its base and advances by four bytes; its signed byte-offset
/// limit is 0x17FDF. Each accepted cursor can read its pair and the next
/// button halfword. No lower-bound or record validation is performed.
///
/// Duration counts calls, wraps at 16 bits, and reloads only when the record's
/// buttons differ from the cached word. Halted playback publishes the cache
/// without stepping. Otherwise live Start is preserved and requests intro
/// cancellation. A 0xFFFF next-button terminator clears that request and
/// restores live input; exceeding the cursor limit only restores live input.
void gameDebugApplyInputOverride(s32 overrideMode, u16* buttons);

#endif // GAMEPLAY_GAME_DEBUG_H
