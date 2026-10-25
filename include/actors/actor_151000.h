#ifndef ACTORS_ACTOR_151000_H
#define ACTORS_ACTOR_151000_H

#include "main/task_types.h"

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

/// Garden variant-2 spawn recipe for the sound-enabled nineteen-part scene walker.
///
/// Creates a TMD body from the Eve model at priority `TASK_PRIORITY_CONTROL` with automatic
/// primitive buffers. Spawn at index 0 through the enemy spawner: argument 2
/// must be its live `Enemy`. The task owns its model and work and publishes
/// singleton message state. Keep actor_151000 code, model and clip data loaded
/// until teardown; singleton messages must stop when that task is released.
extern TaskDesc gActor151000WalkerTaskDesc;

#endif // ACTORS_ACTOR_151000_H
