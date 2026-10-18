/* The projectile task of the grenade launchers, used by the Grenade Pistol and
 * MM1 source and by Kyle's package. Spawning places the shell at a launcher-row
 * offset from the muzzle, aims it along the muzzle, seeds that launcher's flight
 * divisor and links a collision sphere plus a capsule. In flight it steps
 * along its direction under gravity and trails smoke
 * that thins as it slows. It detonates on world contact, on a solid wall
 * record or on timeout, spawning the explosion effect and sound and widening
 * the collision sphere to the ammo's blast radius, which stays live for a few
 * frames before the task ends. The M4A1 Grenade's launcher carries its own
 * spawn and flight states and shares the last two.
 *
 * Include this header in the prologue and each fragment at its function's
 * position.
 * The spawn/flight carriers declare their own static tuning tables
 * before those fragments: `_gLauncherGrenadeMuzzleOffsets` supplies SVECTOR rows
 * 0 Grenade Pistol and 1 MM1 in launcher-local integer coordinates;
 * `_gLauncherGrenadeInitialFlightDivisors` supplies positive u8 divisors for the
 * same rows; `_gLauncherGrenadeBlastRadii` supplies u16 coordinate-unit radii for
 * round indices GRENADE_ROUND_FRAGMENTATION..GRENADE_ROUND_RIOT relative to
 * GRENADE_ROUND_FIRST. The declarations live in each carrier's source prologue.
 */

#ifndef SRC_SHARED_PROJECTILES_LAUNCHER_GRENADE_H
#define SRC_SHARED_PROJECTILES_LAUNCHER_GRENADE_H

#include "main/task_types.h"

static void _launcherGrenadeFly(Task* task);
static void _launcherGrenadeBlast(Task* task);

#endif /* SRC_SHARED_PROJECTILES_LAUNCHER_GRENADE_H */
