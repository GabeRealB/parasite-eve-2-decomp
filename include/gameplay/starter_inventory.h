#ifndef GAMEPLAY_STARTER_INVENTORY_H
#define GAMEPLAY_STARTER_INVENTORY_H

#include "main/mc_types.h"

/// Array of `InventoryItemRange*` (`inventoryInitializeStarterLoadout` clears `[1]` and `[2]`,
/// and uses `[3]` as the scan destination while it copies the current inventory out).
extern InventoryItemRange* Gp_ScanPtrs[];

/// Rebuilds the player's starter field loadout while preserving selected carried extras.
///
/// Transfers carried rows to container range 3 without first clearing that range.
/// MP5A5 variants become Ringer's Solution, Grenade Pistol becomes Protein Capsule,
/// and Tactical Vest becomes a Belt Pouch. The standard training gear is omitted.
/// Clears carried rows and container ranges 1 and 2, resets weapon loads/supplies,
/// gives Assault Suit, GPS, Recovery2, M93R with 100 9mm rounds, and Tonfa Baton,
/// then attaches the GPS, recovery item and M93R and restores HP/MP to their maxima.
/// Resets collection flags while retaining Armory Cardkey and Mendel Journal.
/// Requires initialized live save/player state and writable, disjoint inventory
/// ranges with enough free rows for transfers and the three attached-item grants.
/// Returned item rows are dereferenced without an allocation-failure check.
void inventoryInitializeStarterLoadout(void);

#endif // GAMEPLAY_STARTER_INVENTORY_H
