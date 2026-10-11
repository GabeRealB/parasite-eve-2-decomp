#ifndef INCLUDE_ROOMS_SHOP_TIER_H
#define INCLUDE_ROOMS_SHOP_TIER_H

#include "common.h"

/// Number of replay-unlock rows in the shop tier ladder.
///
/// Row indices 0..12 are also bit positions in `McSaveState::shopTiers`.
/// Bounds both the replay-award ladder and each room shop's copy; every row
/// contributes three item IDs, independently of how many stock rows are offered.
#define SHOP_TIER_COUNT 13

/// Unlock mask with every shop-tier row enabled in `McSaveState::shopTiers`.
///
/// Sets bits 0..`SHOP_TIER_COUNT` - 1 (0x1FFF), with bit n selecting ladder row n.
/// Replay selection tests exact equality with this mask to report no new tier;
/// stored unlock masks must have no bits outside it. Demo shop sessions write
/// it directly to the live save.
#define SHOP_TIER_ALL_MASK ((1 << SHOP_TIER_COUNT) - 1)

/// One row of the shop's tier ladder: three items the replay bonus adds to the
/// shop's stock.
///
/// The ladder is a table of `SHOP_TIER_COUNT` rows, and bit `n` of
/// `McSaveState::shopTiers` says row `n` is unlocked. The replay bonus unlocks
/// one row each time it runs and presents that row's three items; the shop, in
/// game mode 0, adds the items of every unlocked row to the stock it sells.
/// The replay-bonus package and each room carrying the shop store their own
/// copy of the table, with the same thirteen rows.
///
/// The replay bonus starts its pick at the first row whose `expCeiling` is not
/// below the player's EXP total, which is the EXP in hand plus the price of
/// every Parasite Energy level bought. It then moves up by the game mode,
/// stopping at the last row, and on past rows already unlocked. The shop
/// reads only `items`.
typedef struct {
    u32 expCeiling; // Highest EXP total that starts the pick at this row; 0x7FFFFFFF in the last row
    s16 items[3];   // Item ids the row adds to the shop's stock
} ShopTier;
STATIC_ASSERT_SIZEOF(ShopTier, 0xC);

#endif // INCLUDE_ROOMS_SHOP_TIER_H
