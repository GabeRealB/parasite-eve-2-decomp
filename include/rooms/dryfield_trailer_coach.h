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

void func_dryfield_trailer_coach_80181364(Task* task);

void func_dryfield_trailer_coach_80182950(Task* task);

#endif // INCLUDE_ROOMS_DRYFIELD_TRAILER_COACH_H
