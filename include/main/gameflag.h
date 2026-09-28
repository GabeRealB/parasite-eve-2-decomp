#ifndef MAIN_GAMEFLAG_H
#define MAIN_GAMEFLAG_H

#include "types.h"

#include "main/gameflag_types.h"

extern GameFlagNibbleBank GameFlag_NibbleBanks[2];

/// Stage banks contain the live state followed by its memory-card backup.
extern GameFlagAcropolisBank    GameFlag_AcropolisBanks[2];
extern GameFlagDryfieldBank     GameFlag_DryfieldBanks[2];
extern GameFlagDryfieldFullBank GameFlag_DryfieldFullBanks[2];
extern GameFlagShelterBank      GameFlag_ShelterBanks[2];
extern GameFlagNeoArkBank       GameFlag_NeoArkBanks[2];

/// Live stage headers, indexed by GpAreaKey.stage (1..5; slot 0 is NULL).
extern GpFlagBank* Gp_FlagBanks[6];

/// Set one of the 504 game flags; index is 0..503 and value is 0..15.
void GameFlag_SetNibble(s32 index, s32 value);

/// Read a four-bit game flag, indexed 0..503.
s32 GameFlag_GetNibble(s32 index);

#endif // MAIN_GAMEFLAG_H
