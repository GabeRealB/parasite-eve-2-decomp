#ifndef INCLUDE_ROOMS_DRYFIELD_NIGHT_TRAILER_COACH_H
#define INCLUDE_ROOMS_DRYFIELD_NIGHT_TRAILER_COACH_H

#include "types.h"

#include "gameplay/area.h"
#include "gameplay/direction.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/task_types.h"
#include "main/tmd_types.h"

extern TaskDesc D_dryfield_night_trailer_coach_801846D0;

extern AreaVariant D_dryfield_night_trailer_coach_8018C15C[13];

extern TmdSource gDryfieldNightTrailerCoachAcropolisSanctuaryModel090F0;

// dryfield_night_trailer_coach
extern WorldCoordRoomLighting D_dryfield_night_trailer_coach_80189500[];

extern WorldCollisionRoomResources D_dryfield_night_trailer_coach_80189508[];

extern u8* D_dryfield_night_trailer_coach_80189518[];

extern ViewCount D_dryfield_night_trailer_coach_8018951C[];

extern DirectionWarpEntry D_dryfield_night_trailer_coach_80189520[];

extern ViewCamera D_dryfield_night_trailer_coach_80189A44[];

extern SpriteView D_dryfield_night_trailer_coach_8018B64C[];

extern WorldCollisionSurfaceProperties* D_dryfield_night_trailer_coach_8018C1E8[];

/// Updates the night trailer coach's telephone save menu and statistics panels.
///
/// Requires the loaded room and a live UI task owning its object through
/// `spawnArg2.pointer`. State 0 initializes the menu; states 1..3 handle save,
/// statistics and notice dismissal. Before a clear, normal play opens saving
/// directly; a clear or demo scene 1 enables the four-row menu. Child UI tasks
/// supply their results through this controller.
void dryfieldNightTrailerCoachTelephoneMenuTask(Task* task);

/// Draws the night trailer coach's glows for the current room view each frame.
///
/// View 3 draws both strip groups; view 4 draws the second group. Views 2, 5
/// and 7 draw a cyan diamond and four strips; view 8 draws a pulsing cyan disc.
/// Other views draw nothing. `unusedTask` only satisfies the task callback
/// signature. Requires the room overlay, composed view matrices and the
/// current frame's initialized scratch stack, ordering table and packet arena.
void dryfieldNightTrailerCoachDrawGlowsTask(Task* unusedTask);

/// Runs the night trailer coach's room receiver and view-depth updates.
///
/// Requires the loaded room and task state 0 (initialize), 1 (update), or 2
/// (kill); dispatch has no bounds check. Initialization registers the message
/// receiver and starts arrival scenes. Updates apply view 5's 1x depth scale
/// or 8x for other views. The task borrows the room's table for its lifetime;
/// scenes and message-spawned tasks also require the overlay to remain loaded.
void dryfieldNightTrailerCoachRoomTask(Task* task);

#endif // INCLUDE_ROOMS_DRYFIELD_NIGHT_TRAILER_COACH_H
