#ifndef INCLUDE_ACTORS_ACTOR_450800_H
#define INCLUDE_ACTORS_ACTOR_450800_H

/// Starts the nursery's companion dialogue with scripted player and companion poses.
///
/// Requires this actor package and its CAP resources to be loaded, with the
/// player and companion tasks registered. The event script hides the HUD for
/// playback and restores it when finished.
void actor450800StartNurseryCompanionDialogue(void);

/// Starts Kyle Madigan's nursery scene pose and places him at its starting transform.
///
/// Requires this actor package to be loaded and placed scene actor 0 to have
/// a live model and animation rig. Both messages consume package-owned records
/// synchronously; the animation request precedes placement.
void actor450800PrepareNurseryKyleMadigan(void);

#endif // INCLUDE_ACTORS_ACTOR_450800_H
