#ifndef WEAPONS_SHARED_8011D3A0_H
#define WEAPONS_SHARED_8011D3A0_H

#include "common.h"

#include "main/task.h"

/// Spawn state shared by the grenade pistol and MM1: allocates the 0xA0 work
/// block (`M4a1GrenadeWork`), places the projectile from the per-ammo muzzle
/// offset table, parents it to world, and links its two collision nodes.
/// Unlike the M4A1 grenade spawn, the per-ammo offset lives in
/// `d4rec.field_C` (not `field_A`).
void WeaponsShared8011d3a0(Task* arg0);

#endif
