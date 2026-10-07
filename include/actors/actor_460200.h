#ifndef ACTORS_ACTOR_460200_H
#define ACTORS_ACTOR_460200_H

/// Restores the soldiers' idle arrangement in the Shelter 1F tent.
///
/// Requires the live scene manager and this actor package loaded. Places
/// Soldier C (actor 0) and starts his idle clip, hides Soldier B (actor 1)
/// with automatic model-buffer allocation disabled, and shows Soldier A
/// (actor 2) with his idle clip. Missing actors are skipped.
void actor460200SetupTentSoldiers(void);

/// Starts Soldier C's next tent conversation, repeating the fourth thereafter.
///
/// Requires this package's scripts and CAP data to remain loaded through
/// playback. Advances the saved conversation counter from 0 to 3; values
/// outside 0..3 do nothing. The event script hides and restores the HUD.
void actor460200TalkToSoldierC(void);

/// Starts Soldier A's next tent conversation, repeating the fourth thereafter.
///
/// Has `actor460200TalkToSoldierC`'s storage and counter contract, with its own
/// saved counter and scripts for placed actor 2.
void actor460200TalkToSoldierA(void);

#endif // ACTORS_ACTOR_460200_H
