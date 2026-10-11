#ifndef SRC_WEAPONS_GUNBLADE_GUNBLADE_PRIVATE_H
#define SRC_WEAPONS_GUNBLADE_GUNBLADE_PRIVATE_H

#include "types.h"

#include "main/task_types.h"

/// Requests a deferred charge flash on the active gunblade trail.
///
/// ammunitionIndex is PlayerStatus::weaponSlotItem, consumed unchanged by the
/// flash effect (13..15 select its three drawn grades). With no live trail this
/// is a no-op. Otherwise the trail's pending counter is incremented; its update
/// spawns the flash only when that counter equals one starting at age nine.
/// Keep the cached trail task/work live and the gunblade overlay loaded.
void gunbladeRequestChargeFlash(s32 ammunitionIndex);

#endif // SRC_WEAPONS_GUNBLADE_GUNBLADE_PRIVATE_H
