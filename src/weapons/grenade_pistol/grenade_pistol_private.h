#ifndef SRC_WEAPONS_GRENADE_PISTOL_GRENADE_PISTOL_PRIVATE_H
#define SRC_WEAPONS_GRENADE_PISTOL_GRENADE_PISTOL_PRIVATE_H

#include <psyq/sys/types.h>
#include <psyq/libgte.h>

#include "types.h"

/// Impact clip id per attachment, indexed by `ammunitionIndex - GRENADE_ROUND_FIRST`.
extern u16 gGrenadeShellBlastRadii[4];

/// Per-ammo muzzle offset the spawn state places the projectile at, indexed by
/// the ammo nibble of `Task::spawnArg1`.
extern SVECTOR gGrenadeShellMuzzleOffsets[2];

/// Per-ammo launch speed, same index.
extern u8 gGrenadeShellSpeeds[4];

#endif // SRC_WEAPONS_GRENADE_PISTOL_GRENADE_PISTOL_PRIVATE_H
