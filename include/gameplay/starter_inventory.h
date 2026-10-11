#ifndef GAMEPLAY_STARTER_INVENTORY_H
#define GAMEPLAY_STARTER_INVENTORY_H

#include "main/mc_types.h"

/// Number of container selectors accepted by the inventory-transfer screen.
enum { INVENTORY_CONTAINER_COUNT = 12 };

/// Container ranges selected by the low byte of a bank-7 placed-item kind.
///
/// Selector 0 is the temporary battle-reward range; 1..11 address saved
/// containers. Each borrowed descriptor supplies its backing table and row
/// extent. Gameplay owns the descriptors for its loaded lifetime; transfers,
/// new-game setup and room prize grants change their rows, not the pointer order.
/// The selector must be in 0..11; lookup performs no bounds check.
extern InventoryItemRange* gInventoryContainerRanges[INVENTORY_CONTAINER_COUNT];

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
