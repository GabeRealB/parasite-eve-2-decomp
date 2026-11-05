#ifndef METABOLISM_METABOLISM_H
#define METABOLISM_METABOLISM_H

#include <engine/task_types.h>

/// A sparkle thrown off by the Metabolism cast, sized by `spawnArg1`: it sinks through its eight cells, spinning, or
/// now and then at the higher levels plain and fading.
void metabolismSparkleTask(Task* task);

/// The Metabolism cast: a fan of wedges and a glow above the player that grow, throwing off sparkles, then fade.
/// Applies the spell and plays its cue on the first frame; a held attachment or a room's PE cancellation frees it.
void metabolismCastTask(Task* task);

#endif // METABOLISM_METABOLISM_H
