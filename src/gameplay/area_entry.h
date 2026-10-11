#ifndef GAMEPLAY_PRIVATE_AREA_ENTRY_H
#define GAMEPLAY_PRIVATE_AREA_ENTRY_H

#include "types.h"

#include "main/mc_types.h"
#include "main/task_types.h"

extern u8 Gp_StrItemObtained[];

extern u8 Gp_StrBonusItem[];

extern s32 Gp_ItemGrantCooldown;

/// Five-row staging range of items won in battle, exposed as container selector 0.
///
/// Borrows `gInventoryBattleRewardRows` at rows 0..4. A victorious battle clears
/// and fills those rows before the transfer screen offers them to the player.
extern InventoryItemRange gInventoryBattleRewardRange;

extern TaskDesc D_8010CAB0;

/// Battle-result payload modes used by `gSceneBattleResultTaskDesc`.
enum {
    SCENE_BATTLE_RESULT_WON    = 0,
    SCENE_BATTLE_RESULT_ESCAPE = 1
};

/// Stage-mode task descriptor for battle results and the subsequent reward transfer.
///
/// spawnArg1 is WON (0) or ESCAPE (1). Victory borrows a live HudState through
/// spawnArg2 outside the shooting gallery; escape needs no HUD. The task replaces
/// spawnArg2 with its result panel, restores area music and releases its stage
/// primitive reservation after all result and reward panels close.
extern TaskDesc gSceneBattleResultTaskDesc;

#endif // GAMEPLAY_PRIVATE_AREA_ENTRY_H
