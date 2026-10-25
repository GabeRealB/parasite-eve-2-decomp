#ifndef INCLUDE_ACTORS_ACTOR_450200_H
#define INCLUDE_ACTORS_ACTOR_450200_H

#include "main/task_types.h"

#include "gameplay/evs.h"

/// Observatory companion scene played by arrival action 1 when `GAME_FLAG_083` is zero.
///
/// Starts CAP command 7, installs both actors' scene clips and runs the companion
/// puff/vibration sequence. Finishes in view 3 with the companion obstacle
/// restored and scripted player control released. Pair with
/// `gActor450200CompanionSceneSkipScript`; the actor package, observatory room,
/// default CAP resources and both live actor models must remain loaded through
/// playback and while the installed animation banks borrow the package's clips.
extern EvsCommand gActor450200CompanionSceneScript[82];

/// Skip path restoring the companion scene's view, actors and obstacle.
///
/// Stops the tracked puff emitter, requests room-effect cancellation and restores
/// the actors' final placements and poses under a primary fade. Requires the
/// companion extension installed by `gActor450200CompanionSceneScript` or room
/// entry; keep the same actor, room and model resources live through restoration.
extern EvsCommand gActor450200CompanionSceneSkipScript[19];

/// Player-only observatory scene played by arrival action 1 when `GAME_FLAG_083` is nonzero.
///
/// Starts CAP command 1 and installs five player clips. Finishes in view 2 at
/// the setup placement, restoring weapons and releasing scripted player control.
/// Pair with `gActor450200SoloSceneSkipScript`; the actor package, observatory
/// room, default CAP resources and player model must remain loaded through
/// playback and while the installed bank borrows the package's clips.
extern EvsCommand gActor450200SoloSceneScript[40];

/// Skip path restoring the player-only scene's view, placement and native pose.
///
/// Restores weapons, clears the ambient override and releases scripted player
/// control under a primary fade. Requires the player and observatory resources
/// used by `gActor450200SoloSceneScript` to remain live through restoration.
extern EvsCommand gActor450200SoloSceneSkipScript[12];

/// Observatory action-2 scene that cross-fades the backdrop and selects room variant 2.
///
/// Starts CAP command 2, installs four player clips and walks between two
/// destinations before the backdrop and beam transition. Finishes in view 8,
/// releasing scripted player control. The physical event depicted is unproven.
/// Pair with `gActor450200RoomVariantSceneSkipScript`; keep the actor package,
/// observatory room, default CAP resources and player model loaded through
/// playback and while the installed bank borrows the package's clips.
extern EvsCommand gActor450200RoomVariantSceneScript[44];

/// Skip path selecting room variant 2, view 8 and the room-variant scene's final player pose.
///
/// Applies beam intensity 160, restores weapons, clears the ambient override,
/// resumes area music and releases scripted player control under a primary fade.
/// Requires the player and observatory resources used by
/// `gActor450200RoomVariantSceneScript` to remain live through restoration.
extern EvsCommand gActor450200RoomVariantSceneSkipScript[15];

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

/// Bodyless observatory scene tasks: 0 immediate stop, 1 companion puffs, 2 player head aim.
///
/// Loaded with the actor_450200 package through the observatory's area resources.
/// Direct spawns use entries 1 and 2; entry 0 kills its task on first dispatch.
/// The table has no terminator, and the task callbacks require the scene's actors
/// and room resources to remain loaded while they run.
extern TaskDesc gActor450200SceneTaskDescs[3];

#endif // INCLUDE_ACTORS_ACTOR_450200_H
