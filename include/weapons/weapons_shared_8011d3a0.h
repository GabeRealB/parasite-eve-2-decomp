#ifndef WEAPONS_SHARED_8011D3A0_H
#define WEAPONS_SHARED_8011D3A0_H

#include "common.h"

#include <psyq/libgte.h>

#include "main/task.h"

/// Per-ammo muzzle offset the shared spawn state places the projectile at,
/// indexed by the ammo nibble of `Task::spawnArg1`. Each overlay that carries
/// the shared body supplies its own copy.
extern SVECTOR WeaponsShared8011d3a0Vecs[2];

/// Per-ammo launch speed, same index.
extern u8 WeaponsShared8011d3a0Speeds[4];

/// Spawn state shared by the grenade pistol and MM1: allocates the 0xA0 work
/// block (`M4a1GrenadeWork`), places the projectile from the per-ammo muzzle
/// offset table, parents it to world, and links its two collision nodes.
/// Unlike the M4A1 grenade spawn, the per-ammo offset lives in
/// `d4rec.field_C` (not `field_A`).
void WeaponsShared8011d3a0(Task* arg0);

#endif
