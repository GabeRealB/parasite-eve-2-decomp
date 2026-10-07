/* The projectile task of the grenade launchers, used by the Grenade Pistol and
 * MM1 source and by Kyle's package. Spawning places the shell at a per-ammo
 * offset from the muzzle, aims it along the muzzle, gives it a per-ammo launch
 * speed and links a collision sphere plus a capsule that stretches with the
 * speed. In flight it steps along its direction under gravity and trails smoke
 * that thins as it slows. It detonates on world contact, on a solid wall
 * record or on timeout, spawning the explosion effect and sound and widening
 * the collision sphere to the ammo's blast radius, which stays live for a few
 * frames before the task ends. The M4A1 Grenade's launcher carries its own
 * spawn and flight states and shares the last two.
 *
 * Include this header in the prologue and each fragment at its function's
 * position.
 */

#ifndef SRC_SHARED_GRENADE_SHELL_H
#define SRC_SHARED_GRENADE_SHELL_H

#include "types.h"

#include "main/task_types.h"

void grenadeShellSpawn(Task* arg0);
void grenadeShellFly(Task* arg0);
void grenadeShellBlast(Task* task);

#endif /* SRC_SHARED_GRENADE_SHELL_H */
