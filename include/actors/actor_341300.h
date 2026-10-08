#ifndef ACTORS_ACTOR_341300_H
#define ACTORS_ACTOR_341300_H

/// Clears the scene's saved temporary-task handle without stopping its task.
///
/// The walkway calls this after loading actor_341300 for its scene variant.
/// Player-facing and textured-quad tasks share the slot; this reset neither
/// validates nor releases a previously saved task. The actor overlay must be loaded.
void actor341300ResetScriptTaskHandle(void);

#endif // ACTORS_ACTOR_341300_H
