#ifndef INCLUDE_ACTORS_ACTOR_535700_H
#define INCLUDE_ACTORS_ACTOR_535700_H

#include "main/task_types.h"

#include "actors/pair_walk.h"

#include "gameplay/evs.h"

/// Aya's encounter with three Pawn GOLEMs on entry to the night Dryfield R08 room.
///
/// Presents the scene actors while hiding the player, then drives their walks
/// and animation cues. A 300-update blackout precedes scene completion; input
/// skipping is disabled for the final 60-frame wait. Use the paired skip script
/// with `EVENT_SCRIPT_HUD_HIDE_RESTORE`. All 99 commands and their actor, CAP,
/// audio and callback resources are borrowed until the interpreter ends.
/// Completion returns to the night motel balcony. Keep this actor overlay and
/// the room's scene resources loaded throughout.
extern EvsCommand gActor535700PawnGolemEncounterScript[99];

/// Skip path that cleans up the Pawn GOLEM encounter and returns to the motel.
///
/// Fades across cleanup, clears the blackout, finishes the scene, waits sixty
/// frames and reveals play. All nine commands and their callbacks are borrowed
/// as the initial skip target of `gActor535700PawnGolemEncounterScript`.
extern EvsCommand gActor535700PawnGolemEncounterSkipScript[9];

/// Scene Eve's nineteen-part TMD walker with optional footstep sounds.
///
/// Spawn at index zero through the enemy spawner with a live owning `Enemy`
/// in argument 2. Priority `TASK_PRIORITY_CONTROL` uses automatic model primitive buffers. The
/// task owns its model and work and publishes singleton message state.
/// Keep actor_535700's code, model and clips loaded until task teardown;
/// singleton messages require that original task and work to remain live.
extern TaskDesc gActor535700EveWalkerTaskDesc;

/// Dryfield night R08's scripted Pawn GOLEM spawn descriptors: body 0, carried Beam Sword 1.
///
/// Both create TMD bodies at priority `TASK_PRIORITY_ACTOR` with automatic model buffers disabled.
/// The body creates and owns the sword child, which follows part 7 and borrows
/// the body's lighting. Spawn the body through its room placement; keep
/// actor_535700's callbacks, models and animation data loaded until teardown.
extern TaskDesc gActor535700PawnGolemTasks[PAIR_WALK_TASK_COUNT];

#endif // INCLUDE_ACTORS_ACTOR_535700_H
