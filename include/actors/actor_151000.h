#ifndef ACTORS_ACTOR_151000_H
#define ACTORS_ACTOR_151000_H

#include "gameplay/evs.h"

/// Garden arrival scene for placement variant 2 and warp 3.
///
/// Starts CAP command 2, installs player clips 47..49, and coordinates the
/// player and scene walker through blackout, CAP cues and departure. Restores
/// garden view 3, equipment, lighting and player control on completion.
/// The room, actor_151000, equipped animation bank and CAP/sound resources
/// must remain loaded while this 47-command script and its payloads are borrowed.
extern EvsCommand gActor151000GardenArrivalScript[47];

/// Skip cleanup for `gActor151000GardenArrivalScript`.
///
/// Clears the scene controls and blackout, resets and hides the walker,
/// restores equipment and lighting, and places the player for view 3 before
/// releasing scripted control. Borrowed script and payloads require the
/// same loaded resources as the arrival script; ends after 15 commands.
extern EvsCommand gActor151000GardenArrivalSkipScript[15];

#endif // ACTORS_ACTOR_151000_H
