#ifndef INCLUDE_ROOMS_DRYFIELD_NIGHT_WAREHOUSE_H
#define INCLUDE_ROOMS_DRYFIELD_NIGHT_WAREHOUSE_H

#include "types.h"

#include "gameplay/area.h"
#include "gameplay/direction.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/task_types.h"

extern AreaVariant D_dryfield_night_warehouse_8017FB98[12];

/// Night warehouse room slots 1..3 share six point lights and four view ambient minima.
///
/// Indexed by room minus one for night Dryfield area 7. Light coordinates and
/// attenuation remain writable; ambient colours use 16 units per colour level.
/// Borrow these records and their resources only while the room overlay is loaded.
extern WorldCoordRoomLighting gDryfieldNightWarehouseRoomLighting[3];

extern WorldCollisionRoomResources D_dryfield_night_warehouse_8017E900[];

/// Room slots 1..3's logical-view maps, indexed by room minus one.
///
/// Each slot borrows gameplay's `gViewIdentityMap`: logical views 1..4 select
/// camera/image/sprite slots 1..4. Map capacity and reverse-search count are
/// independent. The directory lives with this room overlay; the maps with gameplay.
extern u8* gDryfieldNightWarehouseViewMaps[3];

/// Four logical views per room slot, bounding reverse-map searches in elements.
///
/// Signed halfword counts indexed by room minus one for slots 1..3. Borrowed
/// while this overlay is loaded; they do not describe the camera array's extent.
extern ViewCount gDryfieldNightWarehouseRoomViewCounts[3];

extern DirectionWarpEntry D_dryfield_night_warehouse_8017E944[];

/// Eight authored cameras; mapped view slots 1..4 use the first four records.
///
/// Indexed by mapped view minus one. Rotation uses `ONE` per unit, translation
/// is the negated world-space camera origin in game units, and projection
/// distance is pixels. Slots 5..8 are retained beyond the four-view tables.
/// Keep this overlay loaded until queued camera application copies its record.
extern ViewCamera gDryfieldNightWarehouseViewCameras[8];

/// Four mapped-view sprite descriptors, indexed by mapped view minus one.
///
/// View 1 has no sprites; views 2..4 select 23, 22 and 1 source rectangles.
/// All views retain decoded background strips and have no draw-area clipping.
/// Descriptors and writable lists are borrowed while this room overlay is loaded.
extern SpriteView gDryfieldNightWarehouseSpriteViews[4];

/// The warehouse's collision and footstep policies for surface classes 0..7.
///
/// Class 5 passes probes and suppresses weapon impacts; all others block probes
/// and allow impacts. Every class applies pushback. Only class 6 has footstep
/// cues. The mesh uses classes 4..6, while setup reads all eight policies.
/// Records and sound cues remain borrowed only while this room overlay is loaded.
extern WorldCollisionSurfaceProperties* gDryfieldNightWarehouseSurfacePropertiesByClass[8];

/// Draws the warehouse's pulsing prism and four circular light beams for the current view.
///
/// Bank-6 effect 0x105 requires a live coordinate body owned by `task` and this
/// room overlay loaded. Composes the coordinate before drawing; the drawers
/// borrow it and queue packets in the current frame's arena. The view matrices,
/// scratch stack, primitive arena and ordering table must be ready for drawing.
/// The room-local view ID must be valid for its tables and less than 31.
/// Views 2, 3, 6 and 9 draw the prism and beams 2..4; view 2 adds beam 1.
/// Views 4, 7 and 8 draw only beams 3 and 4. Other views queue no geometry.
void dryfieldNightWarehouseDrawGlowsTask(Task* task);

/// Runs the night warehouse's room-message task for one update.
///
/// Requires a live task: state 0 initializes, 1 idles and 2 releases the task.
/// The state index is unchecked. Keep this room overlay loaded while the task
/// or its registered message handlers can run; the final state ends its lifetime.
void dryfieldNightWarehouseRoomTask(Task* task);

#endif // INCLUDE_ROOMS_DRYFIELD_NIGHT_WAREHOUSE_H
