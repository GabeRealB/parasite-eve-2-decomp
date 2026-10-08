#ifndef INCLUDE_WEAPONS_GRENADE_PISTOL_H
#define INCLUDE_WEAPONS_GRENADE_PISTOL_H

#include "main/task_types.h"

void func_mm1_8011DBD0(Task* arg0);

/// Dispatches the Grenade Pistol projectile's spawn, flight, blast and teardown states.
///
/// Requires task state 0..3 and the matching weapon/gameplay overlays loaded.
/// Spawn owns the `WeaponGrenadeWork` allocation and initializes both collision
/// bodies; flight and blast require it live. Teardown unlinks both bodies and
/// releases the task/work. The projectile's packed spawn argument supplies
/// ammunition and launcher-row selectors to the states. The MM1 build exports
/// its own alias of this dispatcher; its model and launch tuning remain separate.
void grenadePistolShellTask(Task* task);

#endif // INCLUDE_WEAPONS_GRENADE_PISTOL_H
