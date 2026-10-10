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

/// Resident live and backup state for Akropolis Tower and the MIST areas.
///
/// Indexed by `GAME_FLAG_STAGE_BANK_LIVE` / `GAME_FLAG_STAGE_BANK_BACKUP`.
/// Each 0x6C-byte copy holds visited bits, object states and 18 area slots.
/// The MIST shooting gallery instead uses `gGameFlagDryfieldBanks` slot 29.
extern GameFlagAcropolisBank gGameFlagAcropolisBanks[GAME_FLAG_STAGE_BANK_COUNT];

/// Resident live and backup Dryfield state, including shared day/night area slots.
///
/// Indexed by `GAME_FLAG_STAGE_BANK_LIVE` / `GAME_FLAG_STAGE_BANK_BACKUP`.
/// Each 0xB0-byte copy holds the daytime visited bits, object states used by
/// both times of day, and 36 area slots, including the MIST shooting gallery.
/// Nighttime visited bits are stored separately in `gGameFlagDryfieldNightBanks`.
extern GameFlagDryfieldBank gGameFlagDryfieldBanks[GAME_FLAG_STAGE_BANK_COUNT];

/// Resident live and backup visited-area state for Dryfield by night.
///
/// Indexed by GAME_FLAG_STAGE_BANK_LIVE / GAME_FLAG_STAGE_BANK_BACKUP. Each
/// 0x24-byte copy contains a checksum header and retained trailing bytes. Night
/// placement records and object states instead use gGameFlagDryfieldBanks.
/// Initialization clears the live bank and fills the backup with 0xFF; card
/// loading restores both copies, while attract-demo restore replaces only live.
extern GameFlagDryfieldNightBank gGameFlagDryfieldNightBanks[GAME_FLAG_STAGE_BANK_COUNT];

/// Resident live and backup state for the mine and Shelter B1 through B4.
///
/// Indexed by `GAME_FLAG_STAGE_BANK_LIVE` / `GAME_FLAG_STAGE_BANK_BACKUP`.
/// Each 0xE4-byte copy holds visited bits, object states and 48 area slots.
/// Shelter area 49 shares the mine refuge's slot 5. Shelter 1F and B6 use
/// `gGameFlagNeoArkBanks` instead.
extern GameFlagMineShelterBank gGameFlagMineShelterBanks[GAME_FLAG_STAGE_BANK_COUNT];

/// Resident live and backup state for Shelter 1F and B6 together with Neo Ark.
///
/// Indexed by `GAME_FLAG_STAGE_BANK_LIVE` / `GAME_FLAG_STAGE_BANK_BACKUP`.
/// Each 0xA4-byte copy holds visited bits, object states and 32 area slots.
/// The B6 nursery and growth room share slot 21.
extern GameFlagNeoArkBank gGameFlagNeoArkBanks[GAME_FLAG_STAGE_BANK_COUNT];

/// Resident directory of live visited-area headers, indexed by stage ID.
///
/// Slots 1..5 borrow the live Acropolis, Dryfield, Dryfield-night, mine/Shelter
/// and Shelter/Neo Ark headers; slot 0 is NULL. Map and area-visit code updates
/// or reads the two visited-area words through these stable pointers. Night
/// visits have their own header even though day and night share area records.
extern GameFlagStageHeader* gGameFlagStageHeaders[6];

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
