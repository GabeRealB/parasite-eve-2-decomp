#ifndef MAIN_GAMEFLAG_H
#define MAIN_GAMEFLAG_H

#include "types.h"

#include "main/gameflag_ids.h"
#include "main/gameflag_types.h"

/// Values of `GAME_FLAG_WATER_TOWER_MECHANISM_STATE`, shared by both water rooms.
enum {
    GAME_FLAG_WATER_TOWER_MECHANISM_INITIAL        = 0,
    GAME_FLAG_WATER_TOWER_MECHANISM_TOWER_RESTORED = 1,
    GAME_FLAG_WATER_TOWER_MECHANISM_TOWER_OPERATED = 2,
    GAME_FLAG_WATER_TOWER_MECHANISM_TANK_OPERATED  = 3,
};

/// Live packed game flags followed by their memory-card comparison copy.
///
/// The two contiguous 256-byte banks remain resident across overlay changes.
/// Gameplay reads and writes `GAME_FLAG_NIBBLE_BANK_LIVE`, including its shared
/// play-time mark.
/// Initialization clears that bank and fills `GAME_FLAG_NIBBLE_BANK_BACKUP`
/// with 0xFF. The memory-card system copies the live bank to the backup when
/// saving and restores both complete banks when loading; demo restore replaces
/// only the live bank. Each bank's checksum covers its entire payload.
extern GameFlagNibbleBank gGameFlagNibbleBanks[GAME_FLAG_NIBBLE_BANK_COUNT];

/// Stage banks contain the live state followed by its memory-card backup.
extern GameFlagAcropolisBank     GameFlag_AcropolisBanks[2];
extern GameFlagDryfieldBank      GameFlag_DryfieldBanks[2];
extern GameFlagDryfieldNightBank GameFlag_DryfieldFullBanks[2];
extern GameFlagMineShelterBank   GameFlag_ShelterBanks[2];
extern GameFlagNeoArkBank        GameFlag_NeoArkBanks[2];

/// Live stage headers, indexed by GameLocationKey.stage (1..5; slot 0 is NULL).
extern GameFlagStageHeader* Gp_FlagBanks[6];

/// Writes a four-bit game-flag value in the current session's save bank.
///
/// `flagId` counts nibble positions and must be in 0..GAME_FLAG_NIBBLE_COUNT-1;
/// zero is a valid flag ID, and no bounds check is performed. Only the low
/// four bits of `value` are stored, preserving the neighboring flag: even IDs
/// select the high nibble and odd IDs the low nibble. This writes the payload;
/// the save system maintains its checksum and backup copy. Positions sharing
/// the play-time mark access the same bytes described by `GameFlagNibbleBank`.
void gameFlagSetNibble(s32 flagId, s32 value);

/// Reads the current session's four-bit game-flag value as an integer 0..15.
///
/// `flagId` counts nibble positions and must be in 0..GAME_FLAG_NIBBLE_COUNT-1;
/// zero is a valid flag ID, and no bounds check is performed. Even IDs select
/// the high nibble and odd IDs the low nibble of the live payload byte.
/// Positions sharing the play-time mark access the same bytes described by
/// `GameFlagNibbleBank`.
s32 gameFlagGetNibble(s32 flagId);

#endif // MAIN_GAMEFLAG_H
