#ifndef GAMEPLAY_STARTER_INVENTORY_H
#define GAMEPLAY_STARTER_INVENTORY_H

#include "main/mc_types.h"

/// Array of `InventoryItemRange*` (`Gp_InitStarterInv` clears `[1]` and `[2]`,
/// and uses `[3]` as the scan destination while it copies the current inventory out).
extern InventoryItemRange* Gp_ScanPtrs[];

void Gp_InitStarterInv(void);

#endif // GAMEPLAY_STARTER_INVENTORY_H
