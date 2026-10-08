#ifndef INCLUDE_ROOMS_DRYFIELD_TRAILER_COACH_H
#define INCLUDE_ROOMS_DRYFIELD_TRAILER_COACH_H

#include "types.h"

#include "gameplay/area.h"
#include "gameplay/direction.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/task_types.h"
#include "main/tmd_types.h"

extern TaskDesc D_dryfield_trailer_coach_80183F84;

extern AreaVariant D_dryfield_trailer_coach_801876F0[13];

extern TmdSource gDryfieldTrailerCoachAcropolisSanctuaryModel090F0;

// dryfield_trailer_coach
extern WorldCollisionRoomResources D_dryfield_trailer_coach_801871CC[];

extern WorldCoordRoomLighting D_dryfield_trailer_coach_801871DC[];

extern u8* D_dryfield_trailer_coach_801871E4[];

extern ViewCount D_dryfield_trailer_coach_801871E8[];

extern DirectionWarpEntry D_dryfield_trailer_coach_801871EC[];

extern ViewCamera D_dryfield_trailer_coach_80187758[];

extern SpriteView D_dryfield_trailer_coach_801891D0[];

extern WorldCollisionSurfaceProperties* D_dryfield_trailer_coach_80189C30[];

/// Draws the trailer coach's pulsing glows for the current camera view.
///
/// Gameplay's effect slot 0xD9 supplies a live `TASK_BODY_COORD` body in
/// `task->extra.coordBody`. Views 2 and 8 draw a cyan diamond with diagonal
/// rays; view 10 draws a cyan disc with four rays. Other views draw nothing.
/// The active view must be a valid trailer-coach view (1..11). Both drawers
/// transform the same local point by the body's coordinate and reject depths
/// below camera Z / 4 = 17. They advance the pulse by 96 angle units per
/// animation frame, with 4096 units per turn.
///
/// Requires the current view matrices, scratch stack, ordering table and packet
/// arena. Queued primitives belong to that frame until GPU drawing completes;
/// the task keeps no additional work allocation or state between frames.
void dryfieldTrailerCoachDrawGlowsTask(Task* task);

/// Updates the daytime trailer coach's telephone save and statistics menu.
///
/// Gameplay dispatches this wrapper while this room overlay is loaded.
/// Requires a live menu task in state 0..3, its writable UiObject in
/// `spawnArg2.pointer`, and loaded UI resources. The parent owns the menu;
/// the implementation opens child dialogs/panels and reports dismissal through
/// the object's result rather than killing this task.
void dryfieldTrailerCoachTelephoneMenuTask(Task* task);

/// Runs the trailer-coach room's entry, depth-scale update or teardown state.
///
/// The stage map spawns this receiver for area 27. `task->state` must be 0..2:
/// entry installs the room table and starts the arrival presentation; state 1
/// keeps the receiver alive and selects the current view's ordering-table
/// depth scale; state 2 releases the task. Keep this overlay loaded while the
/// task is active. No additional work or spawn payload is consumed here.
void dryfieldTrailerCoachRoomTask(Task* task);

#endif // INCLUDE_ROOMS_DRYFIELD_TRAILER_COACH_H
