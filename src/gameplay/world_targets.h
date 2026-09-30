#ifndef GAMEPLAY_PRIVATE_WORLD_TARGETS_H
#define GAMEPLAY_PRIVATE_WORLD_TARGETS_H

#include "types.h"

#include "gameplay/world_targets_types.h"

#include "main/mc_types.h"

/// First node on the list of enemies the targeting passes track.
///
/// NULL when the list is empty. Lock-on, the reticle, the radar and the area
/// scans walk from here through `next`.
extern WorldTargetNode* gWorldTargetListHead;

void Gp_DrawTargetCursor(void);

void Gp_ResetLinkState(void);

/// Drops the `targeted` mark from every actor slot's current node, without
/// releasing the slot itself.
void Gp_ClearSlotNodeFlags(void);

s32 Gp_GrantLocationItems(InventoryItemRange* arg0);

void Gp_InitStateF0(void);

#endif // GAMEPLAY_PRIVATE_WORLD_TARGETS_H
