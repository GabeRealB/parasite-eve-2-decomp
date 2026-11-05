#ifndef HEALING_HEALING_H
#define HEALING_HEALING_H

#include <engine/task_types.h>

/// A spark rising from a Healing sparkle, sized by `spawnArg1`.
void healingRisingSparkTask(Task* task);

/// A sparkle sprayed by the Healing aura, sized by `spawnArg1`: it sinks, animates and fades, shedding rising sparks,
/// and is freed with the aura.
void healingSparkleTask(Task* task);

/// The Healing cast: a blue aura above the player that grows and spins, spraying sparkles, then fades and holds. Applies
/// the spell and plays its cue on the first frame (or applies it if cancelled before); a held attachment or a room's PE
/// cancellation frees it.
void healingAuraTask(Task* task);

#endif // HEALING_HEALING_H
