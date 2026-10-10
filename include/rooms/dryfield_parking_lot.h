#ifndef INCLUDE_ROOMS_DRYFIELD_PARKING_LOT_H
#define INCLUDE_ROOMS_DRYFIELD_PARKING_LOT_H

#include "types.h"

#include "gameplay/area.h"
#include "gameplay/direction.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/task_types.h"

extern AreaVariant D_dryfield_parking_lot_8017FA74[13];

/// Room 1's collision mesh, view boundaries, interactions and sight blockers.
///
/// Dryfield's area directory selects this sole entry with room minus one.
/// Its mutable resources are borrowed while the daytime parking lot is loaded;
/// clear the active collision lists before unloading their storage.
extern WorldCollisionRoomResources gDryfieldParkingLotRoomCollisionResources[1];

/// Number of mapped camera and sprite views in the daytime parking lot.
enum { DRYFIELD_PARKING_LOT_VIEW_COUNT = 7 };

/// Number of entries indexed by the room-local collision surface class.
enum { DRYFIELD_PARKING_LOT_SURFACE_CLASS_COUNT = 8 };

/// Room 1's logical-view map, borrowing the gameplay identity mapping.
///
/// Views 1..7 select the same mapped camera, image and sprite index. The room
/// directory has one entry; both this overlay and gameplay must remain loaded.
extern u8* gDryfieldParkingLotRoomViewMaps[1];

/// Room 1's seven logical views, each mapped to the same camera/image index.
///
/// Indexed by room minus one with no terminator; only room 1 is present.
/// The signed halfword bounds reverse searches of the borrowed identity map.
/// Requires the daytime parking lot and gameplay overlays to remain loaded.
extern ViewCount gDryfieldParkingLotRoomViewCounts[1];

/// Room 1's authored point-light collection, with default ambient minima.
///
/// The stage table indexes this one-entry array by room minus one. Its mutable
/// light storage is borrowed only while the daytime parking lot overlay is loaded.
extern WorldCoordRoomLighting gDryfieldParkingLotRoomLighting[1];

/// One-based endpoint IDs in the daytime parking lot's warp table.
enum {
    DRYFIELD_PARKING_LOT_WARP_MAIN_STREET   = 1,
    DRYFIELD_PARKING_LOT_WARP_MOTEL_LOBBY   = 2,
    DRYFIELD_PARKING_LOT_WARP_TOILET        = 3,
    DRYFIELD_PARKING_LOT_WARP_SALOON_G_R    = 4,
    DRYFIELD_PARKING_LOT_WARP_MOTEL_BALCONY = 5,
    DRYFIELD_PARKING_LOT_WARP_COUNT         = 5,
};

/// Arrival placements, departure sounds and map markers for the five endpoints.
///
/// Indexed by endpoint ID minus one, with no terminator. Placements use whole
/// world units and yaw uses 4096 units per turn. The balcony endpoint starts
/// scripted player control and places the companion separately; other endpoints
/// share their player/companion placements. Keep this overlay loaded for lookups.
extern DirectionWarpEntry gDryfieldParkingLotWarpEntries[DRYFIELD_PARKING_LOT_WARP_COUNT];

/// Cameras for mapped daytime parking-lot views 1..7, indexed by view minus one.
///
/// View 1 looks straight down; the remaining views use perspective distance
/// 230 pixels. Transforms use Q12 rotations and negated camera origins in world units.
/// Queued camera application requires the overlay to remain loaded.
extern ViewCamera gDryfieldParkingLotViewCameras[DRYFIELD_PARKING_LOT_VIEW_COUNT];

/// Sprite sources, batches and clipping for mapped daytime parking-lot views 1..7.
///
/// Indexed by mapped view minus one. Every view keeps image strips; views 1 and
/// 7 have no source sprites, and only view 2 supplies a clipping list. Descriptors
/// and their mutable arrays are borrowed while this room overlay remains loaded.
extern SpriteView gDryfieldParkingLotSpriteViews[DRYFIELD_PARKING_LOT_VIEW_COUNT];

/// Collision policies and footstep cues indexed by surface class 0..7.
///
/// All classes apply pushback. Class 1 passes probes and ignores weapon impacts;
/// the others block probes and allow impacts. Classes 2, 3, 5 and 6 have footstep
/// cues; classes 0, 4 and 7 share a silent record. Pointers borrow this loaded overlay.
extern WorldCollisionSurfaceProperties* gDryfieldParkingLotSurfacePropertiesByClass[DRYFIELD_PARKING_LOT_SURFACE_CLASS_COUNT];

/// Updates the daytime parking lot's view gate for dust and related effects.
///
/// Enables the gate in mapped camera views 2 through 5 and disables it in
/// views 1, 6 and 7. The callback ignores `task` and runs without advancing or
/// killing it. Requires a live `gRoomEffectState`, the daytime parking lot
/// overlay and view tables loaded, and a mapped view index in 1 through 7.
void dryfieldParkingLotUpdateViewEffectGateTask(Task* task);

/// Runs the daytime parking lot's room initialization, message wait and teardown.
///
/// State 0 installs the room handlers; state 1 waits for messages;
/// state 2 kills the task.
/// Requires a live task with state in 0..2 and the room overlay loaded.
/// The initialized task must stay live while its published room slot is used.
void dryfieldParkingLotRoomTask(Task* task);

#endif // INCLUDE_ROOMS_DRYFIELD_PARKING_LOT_H
