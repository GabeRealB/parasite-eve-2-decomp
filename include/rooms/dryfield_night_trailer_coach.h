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

/// Entry recipe for the nighttime trailer coach's bodyless shop-session task.
///
/// Gameplay queues this descriptor when the saved location selects this room.
/// `spawnArg1` supplies the packed stock profile/category selector; the second
/// argument is unused on entry. The callback owns the UI tree and stage primitive
/// buffer until closure, so this room's image must stay loaded throughout.
extern TaskDesc gDryfieldNightTrailerCoachShopSessionTaskDesc;

extern AreaVariant D_dryfield_night_trailer_coach_8018C15C[13];

extern TmdSource gDryfieldNightTrailerCoachAcropolisSanctuaryModel090F0;

/// Model lighting for the nighttime coach's sole room (room ID 1).
///
/// Night Dryfield area 27 borrows thirteen writable point lights and an
/// eight-view ambient table. Records and lights require this room to stay loaded.
extern WorldCoordRoomLighting gDryfieldNightTrailerCoachRoomLighting[1];

extern WorldCollisionRoomResources D_dryfield_night_trailer_coach_80189508[];

/// Logical-view map directory for the nighttime coach's sole room (room ID 1).
///
/// Borrows gameplay's `gViewIdentityMap`: logical views 1..8 retain the same
/// 1-based camera/image/sprite indices. The directory lives with this room overlay.
extern u8* gDryfieldNightTrailerCoachViewMaps[1];

extern ViewCount D_dryfield_night_trailer_coach_8018951C[];

extern DirectionWarpEntry D_dryfield_night_trailer_coach_80189520[];

extern ViewCamera D_dryfield_night_trailer_coach_80189A44[];

/// Sprite resources for the nighttime coach's mapped views 1..8.
///
/// Views 2..7 select 101, 160, 39, 5, 22 and 7 source rectangles respectively;
/// views 1 and 8 retain empty batch lists. Every view keeps its decoded
/// background and has no clipping areas. The mutable sources and batches
/// belong to this loaded room; empty source slots retain their batch addresses.
extern SpriteView gDryfieldNightTrailerCoachSpriteViews[8];

/// Nighttime coach collision policies indexed directly by surface class 0..7.
///
/// All classes block probes, permit weapon impacts and apply pushback.
/// Class 6 is the y=0 floor and supplies footstep sounds; class 5 retains
/// a separate silent policy. Classes 0..4 and 7 share the silent default.
/// The pointer table and its records require this room overlay to stay loaded.
extern WorldCollisionSurfaceProperties* gDryfieldNightTrailerCoachSurfacePropertiesByClass[8];

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
