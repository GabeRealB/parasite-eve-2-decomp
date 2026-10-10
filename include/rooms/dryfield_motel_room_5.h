#ifndef INCLUDE_ROOMS_DRYFIELD_MOTEL_ROOM_5_H
#define INCLUDE_ROOMS_DRYFIELD_MOTEL_ROOM_5_H

#include "types.h"

#include "gameplay/direction.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/task_types.h"

/// Number of logical views and mapped camera/sprite records in daytime motel room 5.
enum { DRYFIELD_MOTEL_ROOM_5_VIEW_COUNT = 6 };

/// Number of entries in motel room 5's collision surface-class directory.
enum { DRYFIELD_MOTEL_ROOM_5_SURFACE_CLASS_COUNT = 8 };

/// Collision resources for room 1 of daytime Dryfield area 28.
///
/// The stage map indexes this array at room minus one. Borrows one collision
/// grid, eight view boundaries and one balcony warp trigger; there are no
/// sight occluders. Setup mutates the grid and links/enables the triggers.
/// Keep this overlay loaded until the resources are retired and unlinked.
extern WorldCollisionRoomResources gDryfieldMotelRoom5RoomCollisionResources[1];

/// Logical-view map for room 1 of daytime Dryfield area 28.
///
/// Indexed at room minus one. Borrows gameplay's `gViewIdentityMap`: logical
/// views 1..6 select the same camera/image and sprite indices. The directory
/// belongs to this loaded overlay; the byte map belongs to gameplay.
extern u8* gDryfieldMotelRoom5RoomViewMaps[1];

/// Reverse-lookup limit of six logical views for motel room 5's room 1.
///
/// Indexed at room minus one; bounds searches in `gDryfieldMotelRoom5RoomViewMaps`.
/// Keep this overlay loaded while its signed-halfword count is read.
extern ViewCount gDryfieldMotelRoom5RoomViewCounts[1];

/// Light collection for room 1 of daytime Dryfield area 28.
///
/// The stage map indexes this array at room minus one. Supplies twelve mutable
/// point lights admitted in every view, no directional/cone lights, and no
/// ambient table, selecting the default ambient minima. Keep this overlay
/// loaded while the collection or its lights are borrowed.
extern WorldCoordRoomLighting gDryfieldMotelRoom5RoomLighting[1];

/// Motel room 5's sole arrival endpoint, selected by warp ID 1.
///
/// Places player and companion at (4350, 0, 2426), yaw 3072/4096 of a turn,
/// and selects view 2. Arrival/departure cues use daytime area 28's bank;
/// later arrivals mark map nibble 464. The stage map borrows the array at
/// warp minus one; there is no terminator. Keep this overlay loaded for lookup.
extern DirectionWarpEntry gDryfieldMotelRoom5WarpEntries[1];

/// Authored cameras for mapped motel room 5 views 1..6.
///
/// Indexed at mapped view minus one. Rotation coefficients have 12 fractional
/// bits, translations are negated world-coordinate origins, and projection
/// distances are pixels. The stage map borrows this array while the overlay is loaded.
extern ViewCamera gDryfieldMotelRoom5ViewCameras[DRYFIELD_MOTEL_ROOM_5_VIEW_COUNT];

/// Image-background sprite resources for mapped motel room 5 views 1..6.
///
/// Indexed at mapped view minus one. Every view has a zero-count batch followed
/// by an end marker, no source sprites and no clipping areas. Source slots retain
/// each view's batch-list address without supplying sprite elements. The stage
/// map borrows these descriptors and lists while this overlay is loaded.
extern SpriteView gDryfieldMotelRoom5SpriteViews[DRYFIELD_MOTEL_ROOM_5_VIEW_COUNT];

/// Collision and footstep policies indexed by motel room 5 surface class 0..7.
///
/// Classes 0 and 4..7 share the default silent policy. Classes 1 and 2 supply
/// footstep cue bases; class 3 passes probes and ignores weapon impacts without
/// footsteps. Every class applies pushback. Class 3 is retained although this
/// room's mesh has no class-3 faces. Records and cues require this overlay loaded.
extern WorldCollisionSurfaceProperties* gDryfieldMotelRoom5SurfacePropertiesByClass[DRYFIELD_MOTEL_ROOM_5_SURFACE_CLASS_COUNT];

/// Runs the motel room 5's room-message receiver.
///
/// Start the borrowed task in state 0 to install this overlay's message table
/// and register `GAME_TASK_SLOT_ROOM`; state 1 idles and state 2 releases it.
/// The state index must be 0..2; dispatch is unchecked. Keep this overlay and
/// the registered task live while messages can arrive. Registration borrows
/// the task pointer and does not clear the slot on teardown.
void dryfieldMotelRoom5Task(Task* task);

#endif // INCLUDE_ROOMS_DRYFIELD_MOTEL_ROOM_5_H
