#ifndef ENERGYBALL_ENERGYBALL_H
#define ENERGYBALL_ENERGYBALL_H

#include <engine/task_types.h>

/// An Energy Ball (`spawnArg1`, the ball's place in its cast, picks its light): it charges, rising, then flies at the
/// player, steering. Hitting an enemy spawns three impact rings and bursts it; leaving battle fades it; casting another
/// spell or a room's PE cancellation retires it. The last ball to go stops the loop.
void energyballProjectileTask(Task* task);

/// A ring from an Energy Ball's hit, rolled by `spawnArg1`, growing and fading; it pauses with the room's PE.
void energyballImpactRingTask(Task* task);

/// The Energy Ball cast: one ball per level, fanned around the caster, as long as no more are in the air than the
/// limit; starts the loop if none was, and goes the next frame.
void energyballCastTask(Task* task);

#endif // ENERGYBALL_ENERGYBALL_H
