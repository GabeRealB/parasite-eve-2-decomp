#ifndef ACTORS_ACTOR_150400_H
#define ACTORS_ACTOR_150400_H

#include "main/task_types.h"

#include "gameplay/evs.h"

/// No. 9's freezer confrontation, including sliding models and the control-room movie handoff.
///
/// The control room starts this for layout 11 with HUD hide/restore and the
/// paired skip script. CAP cues place and move No. 9 and the two sliding models,
/// then hand off to the movie and reload the main corridor. All 33 commands
/// include END. Keep actor_150400, the room, player, scene actor and borrowed
/// CAP/animation/stream resources loaded through the handoff.
extern EvsCommand gActor150400FreezerSceneScript[33];

/// Skip path that hands the freezer scene to its movie and main-corridor reload.
///
/// Cleans up the live scene under the primary fade, performs the same movie
/// handoff, restores view 3 and player weapon effects, and releases scripted
/// control. All 14 commands include END. Requires the normal path's loaded
/// actor/room resources and live player until the handoff completes.
extern EvsCommand gActor150400FreezerSceneSkipScript[14];

/// Spawns the freezer scene’s two sliding models and retains their task handles.
///
/// Requires actor_150400 and its model descriptor to remain loaded. Copies 1
/// and 2 start at different Z positions and share later hold/slide commands.
/// Spawn failures are retained unchecked; later state commands require both
/// handles to be live. Call once per scene: another call replaces the handles.
void actor150400SpawnSlidingModels(void);

/// No. 9 GOLEM spawn recipes: 0 scripted walker, 1 carried Beam Sword.
///
/// Priority 96. Indices 0..1 have no terminator.
/// The body uses automatic primitive buffers; the sword skips them.
/// Spawn both through the enemy interface. The sword is reparented under
/// the walker and borrows body part 7 and its work-owned lighting.
/// Keep actor_150400 callbacks and borrowed model/animation resources loaded
/// while the spawned tasks use them.
extern TaskDesc gActor150400No9GolemTaskDescriptors[2];

#endif // ACTORS_ACTOR_150400_H
