#ifndef LIFEDRAIN_LIFEDRAIN_H
#define LIFEDRAIN_LIFEDRAIN_H

#include <engine/task_types.h>

/// A spark rising from Life Drain's funnel or a mote, sized by `spawnArg1`.
void lifedrainRisingSparkTask(Task* task);

/// One of Life Drain's three rings, tilted by `spawnArg1`, growing, widening and fading; it pauses with the room's PE
/// and goes with its cancellation.
void lifedrainExpandingGlowBandTask(Task* task);

/// The Life Drain cast: a blue flash and three rings while its motes fly in, adding what they drained; then, if they
/// drained anything, it is given to the player and a funnel grows, shedding sparks, and fades. Applies the spell on the
/// first frame; a held attachment or a room's PE cancellation frees it, giving what was drained first.
void lifedrainCastTask(Task* task);

/// A Life Drain mote, carrying what a hit drained (`spawnArg1`) to the cast: it drifts from the hit, then homes on the
/// player, shedding sparks, and is freed with the cast.
void lifedrainMoteTask(Task* task);

#endif // LIFEDRAIN_LIFEDRAIN_H
