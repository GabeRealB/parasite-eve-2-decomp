#ifndef INCLUDE_ACTORS_ACTOR_450200_H
#define INCLUDE_ACTORS_ACTOR_450200_H

/// Starts the next observatory companion conversation.
///
/// The persistent selector advances through the first three conversations and
/// stays on the fourth; values outside 0..3 start nothing. Restores the HUD when
/// the script finishes. Requires the actor and observatory overlays, the player
/// and companion models, and their talk/head-aim setup to remain live through
/// playback. The room caller permits this only in view 2 with a companion.
void actor450200StartCompanionTalk(void);

#endif // INCLUDE_ACTORS_ACTOR_450200_H
