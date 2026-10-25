#ifndef INCLUDE_ACTORS_ACTOR_361100_H
#define INCLUDE_ACTORS_ACTOR_361100_H

#include "types.h"

#include "gameplay/evs.h"

#include "main/task_types.h"

/// Pod-bottom entry scene that presents the tentacle and Eve's body before combat.
///
/// Shelter pod-bottom placement variant 1 starts this 96-instruction stream
/// with `gActor361100PodSceneSkipScript` and `EVENT_SCRIPT_HUD_HIDE_RESTORE`.
/// Requires initialized player, CAP and scene tasks, with boss, tentacle and
/// Eve-body actors at placements 0, 1 and 2. actor_361100 and actor_403600,
/// their animation data and the selected CAP/scene resources must stay loaded
/// through playback and actor teardown. Installs player-bank clips 47..56,
/// waits for CAP cues, then releases scripted control and starts the boss fight.
/// The stream has no calls or jumps and ends with `EVENT_SCRIPT_OPCODE_END`.
extern EvsCommand gActor361100PodSceneScript[96];

/// Input-skip continuation that restores the pod scene's boss-fight state.
///
/// This 26-instruction stream is the initial skip target of
/// `gActor361100PodSceneScript`; it requires that scene's live tasks/resources.
/// Hides and releases the scripted models, places the player at the scene exit,
/// cancels scene playback and head aim, restores view/music/HUD state, starts
/// combat and exits the two scripted actors. Borrowed storage must remain loaded
/// until `EVENT_SCRIPT_OPCODE_END`; there are no calls or jumps.
extern EvsCommand gActor361100PodSceneSkipScript[26];

/// Water presentation recipe for the pod-bottom scene while actor data streams.
///
/// Creates a coordinate-body task at priority 192. Requires actor_361100 and
/// actor_403600 loaded, initialized graphics and the slot-2 streaming window.
/// View 11 refracts the framebuffer, 12 draws the ripple, and 10 ends the task.
/// The stream's free tail is borrowed for packets through GPU consumption.
extern TaskDesc gActor361100StreamWaterTaskDesc;

/// Spawn recipe of the pod scene's nineteen-part tentacle, room resource 45.
///
/// Runs at priority `TASK_PRIORITY_CONTROL` with a TMD body and deferred primitive-buffer allocation.
/// The scene supplies a live Enemy in spawnArg2; the task owns its rig/work and
/// borrows this package's model and animation data until teardown.
extern TaskDesc gActor361100TentacleTaskDesc;

/// Spawn recipe of Eve's nineteen-part scripted body, room resource 132.
///
/// Runs at priority `TASK_PRIORITY_CONTROL` with a TMD body and deferred primitive-buffer allocation.
/// The scene supplies a live Enemy in spawnArg2; the task owns its rig/work and
/// borrows this package's model and animation data until teardown.
extern TaskDesc gActor361100EveTaskDesc;

/// Clears the retained scene head-aim task handle before starting the pod event.
///
/// actor_361100 must be loaded. Call before spawning its head-aim task or after
/// prior tasks have been torn down: this only clears the handle and does not kill
/// a live task or free its work. unused is ignored; the room passes 0.
void actor361100ClearHeadAimTaskHandle(s32 unused);

#endif // INCLUDE_ACTORS_ACTOR_361100_H
