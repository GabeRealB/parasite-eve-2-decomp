#ifndef INCLUDE_ACTORS_ACTOR_450200_H
#define INCLUDE_ACTORS_ACTOR_450200_H

/// Prepares the observatory companion's restored pose, obstacle and head aiming.
///
/// Requires this actor package and the observatory room resources. A nonzero
/// saved observatory event flag starts the restore script with the HUD kept;
/// otherwise the companion obstacle is disabled. If a companion is live,
/// starts its head-aim task and stores the returned handle, including NULL
/// on allocation failure.
/// The room invokes this at entry with companion models and resources loaded.
void actor450200InitializeObservatoryCompanion(void);

/// Starts the next observatory companion conversation.
///
/// The persistent selector advances through the first three conversations and
/// stays on the fourth; values outside 0..3 start nothing. Restores the HUD when
/// the script finishes. Requires the actor and observatory overlays, the player
/// and companion models, and their talk/head-aim setup to remain live through
/// playback. The room caller permits this only in view 2 with a companion.
void actor450200StartCompanionTalk(void);

#endif // INCLUDE_ACTORS_ACTOR_450200_H
