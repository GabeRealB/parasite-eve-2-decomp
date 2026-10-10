#ifndef INCLUDE_ROOMS_DRYFIELD_MOTEL_ROOM_4_H
#define INCLUDE_ROOMS_DRYFIELD_MOTEL_ROOM_4_H

#include "types.h"

#include "gameplay/direction.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/task_types.h"

/// Motel room 4's collision resources for room ID 1.
///
/// Borrows a 47-face grid, eight directed view boundaries and one Main Street
/// exit trigger; there are no sight occluders. The grid and trigger records
/// remain writable and must stay loaded until the active resources are retired.
extern WorldCollisionRoomResources gDryfieldMotelRoom4RoomCollisionResources[1];

/// Motel room 4's room directory of logical-view maps.
///
/// Room ID 1 borrows gameplay's `gViewIdentityMap`: views 1..6 select camera,
/// image and sprite indices 1..6. Index by room minus one and keep this overlay
/// loaded while borrowing the directory; the byte map belongs to gameplay.
extern u8* gDryfieldMotelRoom4RoomViewMaps[1];

/// Motel room 4's six-entry logical-view reverse-search limit for room ID 1.
///
/// A signed halfword count indexed by room minus one, with no terminator.
/// Keep the overlay loaded while the stage directory borrows the array.
extern ViewCount gDryfieldMotelRoom4RoomViewCounts[1];

/// Motel room 4's room-1 lighting with five point lights and default ambient minima.
///
/// Index by room minus one. There are no directional or cone lights, and each
/// point light contributes in every view. Borrow the record and its writable
/// light arrays only while this overlay remains loaded.
extern WorldCoordRoomLighting gDryfieldMotelRoom4RoomLighting[1];

/// Motel room 4's sole departure and arrival endpoint, selected by warp ID 1.
///
/// Places player and companion at (3400, 0, 675), yaw 0, and starts view 2.
/// Arrival/departure use this area's sound entries 2/1; no blocked sound or
/// special spawn/fade flags are set. Map-marker nibble 483 is updated on arrival.
/// Index by warp minus one and keep this overlay loaded until the record is copied.
extern DirectionWarpEntry gDryfieldMotelRoom4WarpEntries[1];

/// Motel room 4's six cameras, indexed by mapped view minus one.
///
/// Rotations have 12 fractional bits; translations are negated camera origins
/// in world units, and projection distances are pixels. Keep the overlay loaded
/// until any queued camera application has copied its selected record.
extern ViewCamera gDryfieldMotelRoom4ViewCameras[6];

/// Motel room 4's six sprite descriptors, indexed by mapped view minus one.
///
/// Every view selects decoded background strips and no source sprites or clip
/// commands. Each source slot retains its own mutable, zero-count batch list's
/// address. Keep the descriptors and lists loaded while the renderer borrows them.
extern SpriteView gDryfieldMotelRoom4SpriteViews[6];

/// Motel room 4's collision and footstep policies for surface classes 0..7.
///
/// Classes 0 and 4..7 share blocking, impact-enabled, silent properties.
/// Classes 1 and 2 also select separate footstep cue sets. Class 3 passes probes
/// and ignores weapon impacts without cues. Every class applies pushback.
/// The grid uses classes 0..2; setup reads all eight entries. Borrow the table
/// and its records only while this overlay remains loaded.
extern WorldCollisionSurfaceProperties* gDryfieldMotelRoom4SurfacePropertiesByClass[8];

/// Runs Dryfield motel room 4's room-message task.
///
/// `task` must be live with state 0..2: 0 registers its message table and room
/// slot, then advances to 1; 1 idles; 2 releases the task. No body or work is
/// allocated by this callback. The room overlay must remain loaded while its
/// callback or message table is used, and room-slot users require a live task.
void dryfieldMotelRoom4Task(Task* task);

#endif // INCLUDE_ROOMS_DRYFIELD_MOTEL_ROOM_4_H
