#ifndef ACTORS_ACTOR_150400_H
#define ACTORS_ACTOR_150400_H

/// Spawns the freezer scene’s two sliding models and retains their task handles.
///
/// Requires actor_150400 and its model descriptor to remain loaded. Copies 1
/// and 2 start at different Z positions and share later hold/slide commands.
/// Spawn failures are retained unchecked; later state commands require both
/// handles to be live. Call once per scene: another call replaces the handles.
void actor150400SpawnSlidingModels(void);

#endif // ACTORS_ACTOR_150400_H
