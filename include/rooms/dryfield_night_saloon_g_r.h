#ifndef INCLUDE_ROOMS_DRYFIELD_NIGHT_SALOON_G_R_H
#define INCLUDE_ROOMS_DRYFIELD_NIGHT_SALOON_G_R_H

#include "types.h"

#include "gameplay/area.h"
#include "gameplay/direction.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/task_types.h"

extern AreaVariant D_dryfield_night_saloon_g_r_80188EE4[13];

/// Lighting for night-saloon rooms 1 and 2, sharing one mutable point-light collection.
///
/// Indexed by room - 1. Both entries use the default ambient minima; the map
/// directory borrows this array only while the saloon overlay is loaded.
extern WorldCoordRoomLighting gDryfieldNightSaloonGRRoomLighting[2];

extern WorldCollisionRoomResources D_dryfield_night_saloon_g_r_80185190[];

/// Logical-view maps for night-saloon rooms 1 and 2.
///
/// Indexed by room - 1. Both borrow gameplay's identity mapping for views 1..13;
/// each byte maps view - 1 to a 1-based camera/image/sprite index. The directory
/// lives with the saloon overlay; the mapping lives with gameplay.
extern u8* gDryfieldNightSaloonGRViewMaps[2];

extern ViewCount D_dryfield_night_saloon_g_r_801851B8[];

extern DirectionWarpEntry D_dryfield_night_saloon_g_r_801851BC[];

extern ViewCamera D_dryfield_night_saloon_g_r_80185B74[];

extern SpriteView D_dryfield_night_saloon_g_r_80187FC8[];

/// Night-saloon collision and footstep policies indexed by surface class 0..7.
///
/// Class 1 has its own footsteps; classes 2 and 3 share a pair. Class 3 passes
/// probes and disables weapon impacts. All classes apply pushback; classes 0
/// and 4..7 have no surface footstep cues. Records are borrowed while this
/// overlay is loaded.
extern WorldCollisionSurfaceProperties* gDryfieldNightSaloonGRSurfacePropertiesByClass[8];

/// Draws the night saloon's view-gated flares and light beams each frame.
///
/// Bank 6 type 0x10E requires a live single-coordinate task body. Composes its
/// transform for the local-space beams; the flare positions are world-space.
/// The loaded area's current logical view is 1..13. Requires the active view
/// matrices, scratch stack and frame packet arena; emitted packets live until
/// GPU completion. Does not advance task state or allocate persistent storage.
void dryfieldNightSaloonGRDrawGlowsTask(Task* task);

/// Runs the night saloon's room initialization, message wait and teardown.
///
/// State 0 installs the room handlers and hides the pending encounter
/// actors on variant 2; state 1 waits for messages; state 2 kills the task.
/// Requires a live task with state in 0..2 and the room overlay loaded.
/// The initialized task must stay live while its published room slot is used.
void dryfieldNightSaloonGRRoomTask(Task* task);

#endif // INCLUDE_ROOMS_DRYFIELD_NIGHT_SALOON_G_R_H
