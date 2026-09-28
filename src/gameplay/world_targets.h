#ifndef GAMEPLAY_PRIVATE_WORLD_TARGETS_H
#define GAMEPLAY_PRIVATE_WORLD_TARGETS_H

#include "types.h"

#include "main/mc.h"

void Gp_DrawTargetCursor(void);

void Gp_ResetLinkState(void);

/// Drops the `targeted` mark from every actor slot's current node, without
/// releasing the slot itself.
void Gp_ClearSlotNodeFlags(void);

s32 Gp_GrantLocationItems(McItemScan* arg0);

void Gp_InitStateF0(void);

#endif // GAMEPLAY_PRIVATE_WORLD_TARGETS_H
