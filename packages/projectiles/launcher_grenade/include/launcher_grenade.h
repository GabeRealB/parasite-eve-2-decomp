/* The grenade shell of the launchers: the Grenade Pistol's, the MM1's and Kyle's. Spawned at the launcher's muzzle
 * and aimed along it, the shell flies under gravity trailing smoke that thins as it slows, and detonates on contact, a
 * solid wall or timeout: the explosion effect and sound, then the collision sphere widened to the round's blast radius
 * for a few frames. The M4A1 Grenade's launcher has its own spawn and flight and shares the blast and exit.
 *
 * launcher_grenade_flight.inc.c holds the spawn and flight, launcher_grenade.inc.c the blast and exit. Before the
 * first the carrier defines, by launcher row (0 Grenade Pistol, 1 MM1), `_gLauncherGrenadeMuzzleOffsets` (SVECTOR,
 * launcher-local) and `_gLauncherGrenadeInitialFlightDivisors` (u8), and by round minus `GRENADE_ROUND_FIRST`,
 * `_gLauncherGrenadeBlastRadii` (u16, game units).
 */

#ifndef PROJECTILES_LAUNCHER_GRENADE_H
#define PROJECTILES_LAUNCHER_GRENADE_H

#include <engine/task_types.h>

enum {
    /// A shell's detonation in its launcher's banks (one per round), played for the weapon that fired it.
    LAUNCHER_GRENADE_SOUND_DETONATION = 5,
};

static void _launcherGrenadeFly(Task* task);
static void _launcherGrenadeBlast(Task* task);

#endif /* PROJECTILES_LAUNCHER_GRENADE_H */
