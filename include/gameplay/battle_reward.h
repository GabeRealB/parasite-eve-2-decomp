#ifndef GAMEPLAY_BATTLE_REWARD_H
#define GAMEPLAY_BATTLE_REWARD_H

#include <psyq/sys/types.h>

#include "common.h"

/// Fixed values of an `InventoryBattleReward` list.
enum {
    INVENTORY_BATTLE_REWARD_LIST_END   = -1, // `areaLayoutKey` of the row closing a list
    INVENTORY_BATTLE_REWARD_BONUS_SLOT = 3   // `items` slot granted only with a Medicine Wheel attached
};

/// Items awarded for winning the battle of one area layout.
///
/// Each map package holds two lists for its stage, one for normal, replay
/// and Scavenger runs and one for Bounty and Nightmare runs. A won battle
/// searches the run's list for the live stage, area and placement variant and
/// grants the first matching row; a location with no row awards nothing.
/// A list is closed by a row keyed `INVENTORY_BATTLE_REWARD_LIST_END`, whose
/// items are not read.
///
/// Every nonzero slot is granted independently, as one pack of that item. A
/// slot is withheld when the player already owns the item and it is one that
/// cannot be held twice (armor, weapons and the parts that upgrade a weapon).
/// The last slot is the bonus item, announced separately, and is also withheld
/// unless a Medicine Wheel is attached.
typedef struct {
    s32 areaLayoutKey; // `GAME_LOCATION_KEY(stage, area, variant, 0)`, or `INVENTORY_BATTLE_REWARD_LIST_END`
    u16 items[4];      // Item ids (0 none); slot `INVENTORY_BATTLE_REWARD_BONUS_SLOT` is the bonus item
} InventoryBattleReward;
STATIC_ASSERT_SIZEOF(InventoryBattleReward, 0xC);

#endif // GAMEPLAY_BATTLE_REWARD_H
