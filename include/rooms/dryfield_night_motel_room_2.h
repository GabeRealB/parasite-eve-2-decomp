#ifndef INCLUDE_ROOMS_DRYFIELD_NIGHT_MOTEL_ROOM_2_H
#define INCLUDE_ROOMS_DRYFIELD_NIGHT_MOTEL_ROOM_2_H

#include "types.h"

#include "gameplay/area.h"
#include "gameplay/direction.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/task_types.h"

extern AreaVariant D_dryfield_night_motel_room_2_801809A0[22];

/// Number of identity-mapped logical views and camera/sprite slots in night motel room 2.
enum { DRYFIELD_NIGHT_MOTEL_ROOM_2_VIEW_COUNT = 6 };

/// Night motel room 2's room-1 light collection with default ambient minima.
///
/// Index by room minus one. Four point lights and one cone light contribute in
/// every view; there are no directional lights. Their transforms and attenuation
/// remain writable. Borrow the record and its lights only while this overlay is loaded.
extern WorldCoordRoomLighting gDryfieldNightMotelRoom2RoomLighting[1];

/// Room 1's collision mesh, eight camera boundaries, six actions and two sight quads.
///
/// Index by room minus one; only room 1 is valid. The descriptor borrows mutable
/// overlay-owned resources. Keep the overlay loaded while its grid is active
/// or its trigger and occluder records remain linked.
extern WorldCollisionRoomResources gDryfieldNightMotelRoom2RoomCollisionResources[1];

/// Night motel room 2's room-1 mapping from logical views to camera/image/sprite slots.
///
/// Index by room minus one, then logical view minus one. The borrowed gameplay
/// identity map selects slots 1..6 unchanged; it has a separate capacity of 50.
/// Keep the room overlay loaded while its one-pointer directory is borrowed.
extern u8* gDryfieldNightMotelRoom2RoomViewMaps[1];

/// Night motel room 2's six-view reverse-search limit for room 1.
///
/// A signed halfword element count, indexed by room minus one. It limits the
/// logical-view search, independently of the borrowed map's capacity. Keep
/// the overlay loaded while the stage directory borrows this count array.
extern ViewCount gDryfieldNightMotelRoom2RoomViewCounts[1];

/// Warp endpoint 1 for night motel room 2's door to main street.
///
/// Index by warp ID minus one; only ID 1 is valid. Arrival places both actors
/// at (4666, 0, 2610) in world units with yaw 3072 (4096 per turn), selecting
/// view 2. Arrival/departure use this area's sound entries 2/1, with no blocked
/// sound or spawn flags. Map-marker nibble 485 records later arrivals. Consumers
/// copy the full entry; borrow it only while the room overlay is loaded.
extern DirectionWarpEntry gDryfieldNightMotelRoom2WarpEntries[1];

/// Night motel room 2's six cameras, indexed by mapped view minus one.
///
/// Rotations have 12 fractional bits, translations are negated camera origins
/// in world units, and projection distances are pixels. Keep the overlay loaded
/// until any queued camera application has copied the selected record.
extern ViewCamera gDryfieldNightMotelRoom2ViewCameras[DRYFIELD_NIGHT_MOTEL_ROOM_2_VIEW_COUNT];

/// Night motel room 2's sprite descriptors, indexed by mapped view minus one.
///
/// View 1 has no source sprites; views 2..6 select 92, 87, 47, 70 and 79
/// rectangles. Every view retains decoded background strips. Only view 5 clips
/// drawing, restoring the full buffer at sorting depth 835. Descriptors and
/// mutable source/batch arrays remain borrowed while this overlay is loaded.
extern SpriteView gDryfieldNightMotelRoom2SpriteViews[DRYFIELD_NIGHT_MOTEL_ROOM_2_VIEW_COUNT];

/// Night motel room 2's collision and footstep policies for surface classes 0..7.
///
/// Classes 0/5/6/7 share blocking, impact-enabled, silent properties. Classes
/// 1/2 also provide cues; class 3 passes probes and ignores weapon impacts
/// without cues; class 4 passes probes with impacts and class 2's cues. Every
/// class applies pushback. The grid uses classes 0..4, and setup reads all eight
/// entries. Borrow these records only while this overlay is loaded.
extern WorldCollisionSurfaceProperties* gDryfieldNightMotelRoom2SurfacePropertiesByClass[8];

/// Draws night motel room 2's flickering textured flares for the current view.
///
/// Room-local view slots 2 and 3 draw two distinct world points; slots 5 and 6
/// draw one other point. Other slots draw nothing. The room overlay must remain
/// loaded, with the view matrix, scratch stack, packet arena and ordering table
/// ready for the frame. Reserves two flare packets or one, even if depth clipping
/// rejects them. `unusedTask` is ignored; no task state or work is accessed.
void dryfieldNightMotelRoom2DrawFlaresTask(Task* unusedTask);

/// Runs night motel room 2's room-message task for one update.
///
/// Requires a live task: state 0 initializes, 1 idles and 2 releases the task.
/// The state index is unchecked. Keep this room overlay loaded while the task
/// or its registered message handlers can run; the final state ends its lifetime.
void dryfieldNightMotelRoom2RoomTask(Task* task);

#endif // INCLUDE_ROOMS_DRYFIELD_NIGHT_MOTEL_ROOM_2_H
