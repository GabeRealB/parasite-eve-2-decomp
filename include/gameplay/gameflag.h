#ifndef GAMEPLAY_GAMEFLAG_H
#define GAMEPLAY_GAMEFLAG_H

#include "types.h"

#include "main/gameflag_types.h"

/// Optional flag updates use zero to mean that no flag is associated with the event.
enum { GAME_FLAG_OPTIONAL_NONE = 0 };

/// Nibble position of the whole-byte objective code used by map objective markers.
///
/// Packed-byte access reads or replaces positions 0xA2 and 0xA3 together.
enum { GAME_FLAG_CURRENT_OBJECTIVE = 0xA2 };

/// Writes an event's optional four-bit flag, skipping `GAME_FLAG_OPTIONAL_NONE`.
///
/// Nonzero `optionalFlagId` must be in 1..503 (`GAME_FLAG_NIBBLE_COUNT - 1`). Only the
/// low four bits of `value` are stored; the adjacent nibble is preserved.
/// Unlike `gameFlagSetNibble`, this interface cannot update flag zero.
/// Requires the gameplay overlay; no bounds check or checksum update occurs.
void gameFlagSetNibbleIfPresent(s32 optionalFlagId, s32 value);

/// Replaces both four-bit flags of one live packed game-flag byte.
///
/// `nibbleIndex` counts nibble positions, in 0..503 (`GAME_FLAG_NIBBLE_COUNT - 1`),
/// rather than bytes. Either index of an even/odd pair selects the same byte
/// through signed division by two. Only the low eight bits of `packedValue`
/// are stored. Requires the gameplay overlay; no bounds check, checksum or
/// backup update occurs. Positions sharing the play-time mark share its storage.
void gameFlagSetPackedByte(s32 nibbleIndex, s32 packedValue);

/// Reads both four-bit flags of one live packed game-flag byte as an integer 0..255.
///
/// `nibbleIndex` has the paired-position domain of `gameFlagSetPackedByte`.
/// The unsigned byte is zero-extended to s32. Requires the gameplay overlay;
/// no bounds check occurs and reading changes neither the bank nor its checksum.
s32 gameFlagGetPackedByte(s32 nibbleIndex);

#endif // GAMEPLAY_GAMEFLAG_H
