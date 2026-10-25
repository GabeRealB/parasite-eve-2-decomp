#ifndef ACTORS_ACTOR_460200_H
#define ACTORS_ACTOR_460200_H

#include "gameplay/evs.h"

#include "gameplay/evs.h"

#include "main/task_types.h"

/// Spawn recipe for Soldier A, placed as actor 2 in the Shelter 1F tent.
///
/// A single priority-96 TMD entry selected at index 0. The callback borrows a
/// live Enemy through spawnArg2; this package's model and animation data must
/// stay loaded until the spawned task is torn down.
extern TaskDesc gActor460200SoldierATaskDesc;

/// Spawn recipes for Soldier B's body (0) and optional carried rifle (1).
///
/// The Shelter 1F tent places descriptor 0 as actor 1 with a zero first spawn
/// payload, leaving him unarmed. A nonzero first payload creates descriptor 1
/// as his child, attached to body part 7 and borrowing the body's lighting.
/// Spawn the body through the enemy spawner, which supplies its live Enemy in
/// the second payload. Both entries use priority `TASK_PRIORITY_ACTOR` and suppress automatic
/// model buffers. Keep this package's code, models and clips loaded until the
/// tasks are released. The two-entry table has no terminator.
extern TaskDesc gActor460200SoldierBTasks[2];

/// Spawn recipe for Soldier C, placed as actor 0 in the Shelter 1F tent.
///
/// A single priority-96 TMD entry selected at index 0. The callback borrows a
/// live Enemy through spawnArg2; this package's model and animation data must
/// stay loaded until the spawned task is torn down.
extern TaskDesc gActor460200SoldierCTaskDesc;

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

/// First-arrival conversation and presentation in the Shelter 1F tent.
///
/// All 233 commands, including END, are borrowed. Selects CAP resource 2/command 1,
/// installs player extension clips, stages Soldiers C/B/A, and synchronizes the
/// conversation and negative-frame effect with CAP cues. Restores view 2, player
/// and soldier placement, weapons, room CAP and ambience, then selects scene event
/// 28 and ends player scripting. Pair with `gActor460200TentArrivalSkipScript`;
/// keep both overlays, live actors and borrowed clip/pad resources loaded until
/// the event and any retained playback finish.
extern EvsCommand gActor460200TentArrivalScript[233];

/// Skip path restoring the tent arrival's settled playable arrangement.
///
/// Twenty-two commands including END. Stops the two arrival sound cues, fades
/// across scene cleanup, restores view 2, places Aya and Soldier C, hides Soldier B,
/// shows Soldier A and restores weapons, room CAP and ambience. Sets scene event
/// 28 and ends player scripting. Requires the live scene and this package's
/// borrowed placement, clip and CAP resources through execution and playback.
extern EvsCommand gActor460200TentArrivalSkipScript[22];

#endif // ACTORS_ACTOR_460200_H
