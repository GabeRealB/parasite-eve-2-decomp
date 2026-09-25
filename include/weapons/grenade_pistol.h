#ifndef WEAPONS_GRENADE_PISTOL_H
#define WEAPONS_GRENADE_PISTOL_H

#include "common.h"

#include <psyq/libgte.h>

#include "gameplay/3FB8.h"
#include "main/task.h"
#include "weapons/weapon.h"

/// Impact clip id per attachment, indexed by `sfx - 0xA`.
extern u16 D_grenade_pistol_8012B430[4];

/// Per-ammo muzzle offset the spawn state places the projectile at, indexed by
/// the ammo nibble of `Task::spawnArg1`.
extern SVECTOR D_grenade_pistol_8012B420[2];

/// Per-ammo launch speed, same index.
extern u8 D_grenade_pistol_8012B438[4];

void func_grenade_pistol_8011D6FC(Task* arg0);

#endif
