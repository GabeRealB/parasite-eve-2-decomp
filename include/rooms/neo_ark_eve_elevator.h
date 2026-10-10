#ifndef INCLUDE_ROOMS_NEO_ARK_EVE_ELEVATOR_H
#define INCLUDE_ROOMS_NEO_ARK_EVE_ELEVATOR_H

#include "types.h"

#include "gameplay/direction.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/task_types.h"

/// Number of logical views and mapped camera/sprite slots authored for the elevator.
enum { NEO_ARK_EVE_ELEVATOR_VIEW_COUNT = 4 };

/// Room 1's platform collision mesh and exit trigger, with no view boundaries or occluders.
///
/// The Neo Ark stage directory selects this array for area 9, room 1. Setup
/// borrows and mutates its resources; keep the elevator overlay loaded until
/// the active grid and trigger links are retired.
extern WorldCollisionRoomResources gNeoArkEveElevatorCollisionRooms[1];

/// Room 1's point-light collection, using the default ambient minima for every view.
///
/// Selected by area 9 and room minus one. The collection and its writable
/// light remain borrowed only while the elevator overlay is loaded.
extern WorldCoordRoomLighting gNeoArkEveElevatorRoomLighting[1];

/// Room 1's identity mapping from logical views 1..4 to camera, image and sprite slots.
///
/// The room directory borrows gameplay's `gViewIdentityMap`; reverse lookup
/// searches only the four entries specified by `gNeoArkEveElevatorViewCounts`.
extern u8* gNeoArkEveElevatorViewMaps[1];

/// Room 1's signed-halfword reverse-lookup limit of four logical views.
extern ViewCount gNeoArkEveElevatorViewCounts[1];

/// The elevator's sole arrival/departure endpoint, entered at view 2.
///
/// Warp IDs are 1-based: endpoint 1 supplies both actor placements, arrival
/// and departure sounds, and the Power Plant 2 map marker. No blocked sound
/// or departure fade is requested. Full-record copies preserve the unproven
/// byte spans; the owning room overlay must be loaded during lookup.
extern DirectionWarpEntry gNeoArkEveElevatorWarpEntries[1];

/// Cameras for mapped views 1..4: one overhead camera followed by three identical records.
///
/// Indexed by mapped view minus one. Rotation uses 4096 per unit, translation
/// is the negated world-space camera origin, and projection distance is pixels.
/// The elevator overlay must remain loaded until a queued camera is applied.
extern ViewCamera gNeoArkEveElevatorViewCameras[NEO_ARK_EVE_ELEVATOR_VIEW_COUNT];

/// Four mapped-view descriptors selecting image backgrounds without source sprites or clipping.
///
/// Indexed by mapped view minus one. Each descriptor retains its own empty
/// batch-list address in the source slot; zero counts keep it from being read
/// as sprite elements. Both descriptors and lists belong to the loaded overlay.
extern SpriteView gNeoArkEveElevatorSpriteViews[NEO_ARK_EVE_ELEVATOR_VIEW_COUNT];

/// Collision policies for surface classes 0..7; only the platform floor (class 1) has footsteps.
///
/// Every class blocks probes, permits weapon impacts and applies pushback.
/// The stage/area directory borrows these records until the elevator unloads.
extern WorldCollisionSurfaceProperties* gNeoArkEveElevatorSurfaceProperties[8];

/// Leaves the EVE elevator's room-effect task idle.
///
/// Gameplay effect slot 0x150 selects this per-frame callback. It ignores its
/// task and performs no drawing, state changes or cleanup.
void neoArkEveElevatorIdleEffectTask(Task* unusedTask);

/// Runs the EVE elevator's room-message task.
///
/// Requires a live task in state 0..2: install the room receiver, idle while
/// messages handle requests, then teardown. The room overlay must remain
/// loaded through dispatch.
void neoArkEveElevatorRoomTask(Task* task);

#endif // INCLUDE_ROOMS_NEO_ARK_EVE_ELEVATOR_H
