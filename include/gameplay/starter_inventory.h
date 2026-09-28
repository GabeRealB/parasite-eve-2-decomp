#ifndef GAMEPLAY_STARTER_INVENTORY_H
#define GAMEPLAY_STARTER_INVENTORY_H

#include "main/mc_types.h"

/// Array of `McItemScan*` (`Gp_InitStarterInv` clears `[1]` and `[2]`).
extern McItemScan* Gp_ScanPtrs[];

void Gp_InitStarterInv(void);

#endif // GAMEPLAY_STARTER_INVENTORY_H
