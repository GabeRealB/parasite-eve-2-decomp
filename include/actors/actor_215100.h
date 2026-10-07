#ifndef INCLUDE_ACTORS_ACTOR_215100_H
#define INCLUDE_ACTORS_ACTOR_215100_H

/// Starts Pierce's next shooting-gallery conversation from persistent talk progress.
///
/// Requires this actor overlay and the gallery's conversation resources to be
/// loaded, with event playback available. Progress 0 starts the first scene
/// and becomes 1 before playback; progress 1 starts the second and becomes 2
/// after starting playback. Progress 2 repeats the final scene. Other values
/// do nothing. The event script hides the HUD and restores it on completion.
void actor215100StartPierceConversation(void);

#endif // INCLUDE_ACTORS_ACTOR_215100_H
