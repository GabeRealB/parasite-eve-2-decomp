#ifndef INCLUDE_ROOMS_DRYFIELD_MOTEL_ROOM_1_H
#define INCLUDE_ROOMS_DRYFIELD_MOTEL_ROOM_1_H

#include "types.h"

#include "gameplay/area.h"
#include "gameplay/direction.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/task_types.h"

/// Actor layouts and loading recipes for daytime motel room 1 (Dryfield area 11).
///
/// Index directly by placement variant in 0..12; 1, 2, 3, 7 and 10 are populated.
/// Variant 1 is empty; 2 has Bone Sucklers and an Odd Stranger; 3 has staged
/// and combat Bone Sucklers; 7 has Blood Sucklers and an Odd Stranger; 10 has
/// two staged and two combat Bone Sucklers at one shared initial transform.
/// Placements for 1, 2, 3 and 7 belong to the loaded Dryfield map; variant 10
/// and all resource recipes belong to this room overlay. Both owners must stay
/// loaded through loading, spawning and the lives of actors borrowing placements.
extern AreaVariant gDryfieldMotelRoom1PlacementVariants[13];

/// Collision resources shared by motel room 1's two room entries.
///
/// Index by room minus one (rooms 1 and 2). Both entries borrow the same grid,
/// eight view boundaries, eight action triggers and single sight occluder.
/// Binding mutates those resources; keep this overlay loaded until the grid
/// is retired and all trigger and occluder links have been cleared.
extern WorldCollisionRoomResources gDryfieldMotelRoom1RoomCollisionResources[2];

/// Motel room 1's two room entries sharing one logical-to-resource view map.
///
/// Index by room minus one (rooms 1 and 2). Logical views 1..9 select
/// camera, image and sprite indices 1, 9, 8, 4, 5, 6, 7, 3 and 2.
/// The directory and writable byte map are borrowed from this loaded overlay.
extern u8* gDryfieldMotelRoom1RoomViewMaps[2];

/// Nine logical views searched in each of motel room 1's two room entries.
///
/// Signed-halfword counts indexed by room minus one (rooms 1 and 2), with no
/// terminator. Both limit reverse lookup to the first nine bytes of the shared
/// logical-view map. Borrow only while this overlay remains loaded.
extern ViewCount gDryfieldMotelRoom1RoomViewCounts[2];

/// Motel room 1's two room lighting entries with the same five point lights.
///
/// Index by room minus one (rooms 1 and 2). Both entries use default ambient
/// minima and have no directional or cone lights. The point lights contribute
/// in every logical view; their transforms and attenuation remain writable.
/// Keep this overlay loaded while borrowing the records and their light array.
extern WorldCoordRoomLighting gDryfieldMotelRoom1RoomLighting[2];

/// The single motel room 1 warp endpoint, selected by warp ID 1.
///
/// Player and companion arrive at world (4369, 0, 3388), yaw 3/4 turn, in
/// logical view 2. Arrival/departure use room-bank sound entries 2/1; blocked
/// departure is silent. No spawn/fade options are set; arrival marks nibble
/// 487 visited. The entry has no terminator and is borrowed while this overlay
/// is loaded; transition consumers copy the complete record.
extern DirectionWarpEntry gDryfieldMotelRoom1WarpEntries[1];

/// Nine cameras selected by motel room 1's mapped resource index minus one.
///
/// Logical views 1..9 map to indices 1, 9, 8, 4, 5, 6, 7, 3 and 2. Rotation
/// has 12 fractional bits; translation is the negated world camera origin and
/// projection distance is in pixels. Keep the records loaded until queued
/// camera application completes; the array has no terminator.
extern ViewCamera gDryfieldMotelRoom1ViewCameras[9];

/// Nine sprite-view descriptors selected by mapped resource index minus one.
///
/// Logical views 1..9 map to indices 1, 9, 8, 4, 5, 6, 7, 3 and 2. Mapped
/// views 1..3 have no source sprites and retain their terminal batch addresses
/// in `sources.empty`; views 4..9 borrow source arrays through `sources.elements`.
/// All draw-area lists are NULL. Keep this overlay loaded while borrowing the
/// mutable sources and batches; the descriptor array has no terminator.
extern SpriteView gDryfieldMotelRoom1SpriteViews[9];

/// Motel room 1's collision and footstep policies for surface classes 0..7.
///
/// Classes 0 and 4..7 share silent, blocking, impact-enabled properties.
/// Classes 1 and 2 also supply distinct footstep cue pairs; class 3 passes
/// probes and ignores weapon impacts without cues. Every class applies pushback.
/// The mesh uses classes 0..3; setup reads all eight entries. Borrow the
/// directory, properties and cues only while this overlay remains loaded.
extern WorldCollisionSurfaceProperties* gDryfieldMotelRoom1SurfacePropertiesByClass[8];

/// Inert callback for motel room 1's room-effect slot 0xC9 in task bank 6.
///
/// Ignores `unusedTask`, draws nothing and leaves the task live without
/// advancing its state. The task's resources remain owned until external teardown.
void dryfieldMotelRoom1NoOpEffectTask(Task* unusedTask);

/// Runs motel room 1's room receiver through entry setup, idle and teardown.
///
/// Requires a live bodyless task with state 0 (entry), 1 (idle) or 2 (kill).
/// Entry registers `GAME_TASK_SLOT_ROOM`; variant 3 broadcasts command 0 to
/// placed actors until the room event has been seen. The scene manager must
/// be live on that path. Keep the room overlay loaded for the task's lifetime.
void dryfieldMotelRoom1Task(Task* task);

#endif // INCLUDE_ROOMS_DRYFIELD_MOTEL_ROOM_1_H
