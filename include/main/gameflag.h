#ifndef MAIN_GAMEFLAG_H
#define MAIN_GAMEFLAG_H

#include "types.h"

#include "main/gameflag_ids.h"
#include "main/gameflag_types.h"

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

/// Set one of the 504 game flags; index is 0..503 and value is 0..15.
void GameFlag_SetNibble(s32 index, s32 value);

/// Read a four-bit game flag, indexed 0..503.
s32 GameFlag_GetNibble(s32 index);

#endif // MAIN_GAMEFLAG_H
