#ifndef GAMEPLAY_PRIVATE_GAMEFLAG_H
#define GAMEPLAY_PRIVATE_GAMEFLAG_H

/// Clears every nibble in the live game-flag payload for a new session.
///
/// Resets positions 0..`GAME_FLAG_NIBBLE_COUNT`-1, including the play-time mark
/// that shares these bytes. The checksum header, backup and stage banks are
/// unchanged; the save system maintains their checksums and comparison copies.
void gameFlagClearLiveNibbles(void);

#endif // GAMEPLAY_PRIVATE_GAMEFLAG_H
