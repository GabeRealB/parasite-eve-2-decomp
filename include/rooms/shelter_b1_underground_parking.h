#ifndef INCLUDE_ROOMS_SHELTER_B1_UNDERGROUND_PARKING_H
#define INCLUDE_ROOMS_SHELTER_B1_UNDERGROUND_PARKING_H

#include "common.h"

#include "gameplay/area.h"
#include "gameplay/direction.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/task_types.h"

/// Storage for this room's shop session descriptor.
///
/// `desc` is the spawn recipe the shop menu hands to the stage transition when
/// the player opens the shop from this room: a bodyless task running the shop
/// session. Every other room carrying the shop defines the same descriptor as
/// a bare `TaskDesc` with the next object directly behind it, so the four
/// bytes that follow here are not part of the shop's own data. They are zero
/// in the image and have no established access; whether they are a separate
/// unreferenced variable or padding at an original object boundary is
/// unproven. They stay in this allocation only to keep the data that follows
/// at its address.
typedef struct {
    TaskDesc desc;         // Spawn recipe for the shop session task; read as a single descriptor
    u8       unknown_C[4]; // Zero in the image; no access established and role unproven
} ShelterB1UndergroundParkingShopSessionTaskDescStorage;
STATIC_ASSERT_SIZEOF(ShelterB1UndergroundParkingShopSessionTaskDescStorage, 16);

/// Shop-session spawn recipe and four retained bytes of unproven role.
///
/// Gameplay borrows `desc` across the shop display transition. It starts a
/// bodyless priority-192 session with a packed stock selector in spawnArg1;
/// the session owns its root UI tree and stage primitive buffer until closing.
/// Keep this room overlay loaded throughout. The trailing bytes are never
/// accessed, and their object boundary is unproven.
extern ShelterB1UndergroundParkingShopSessionTaskDescStorage gShelterB1UndergroundParkingShopSessionTaskDescStorage;

/// Extents of the underground parking room directories and logical-view maps.
enum {
    SHELTER_B1_UNDERGROUND_PARKING_ROOM_COUNT         = 8,
    SHELTER_B1_UNDERGROUND_PARKING_VIEW_COUNT         = 24,
    SHELTER_B1_UNDERGROUND_PARKING_AREA_VARIANT_COUNT = 22,
};

/// Placement layouts for underground parking's saved variants 0..21.
///
/// Variants 1 and 5 publish empty terminated lists; variant 21 loads and places
/// Soldier B. All other slots contain two NULL pointers. Gameplay's area-20
/// directory indexes this array by placement variant, independently of room
/// number. The array, its lists and the selected actor's task table must remain
/// loaded while loading, spawning or accessing placed actors.
extern AreaVariant gShelterB1UndergroundParkingAreaVariants[SHELTER_B1_UNDERGROUND_PARKING_AREA_VARIANT_COUNT];

/// Lighting resources for underground parking rooms 1 through 8.
///
/// Indexed by room - 1 through the Shelter map. All rooms share the same ten
/// point lights and 24-view ambient table. The descriptors and referenced storage
/// belong to this overlay and must remain loaded while gameplay uses them.
extern WorldCoordRoomLighting gShelterB1UndergroundParkingRoomLighting[SHELTER_B1_UNDERGROUND_PARKING_ROOM_COUNT];

/// Collision resources for underground parking rooms 1 through 8.
///
/// The Shelter area-20 directory indexes by room - 1. Rooms 2/4/5 share a grid,
/// as do 6/7; each room selects its own mutable action-trigger list. Rooms 1..7
/// share view boundaries; room 8 selects separate boundaries. All occluders are
/// NULL. Keep the overlay loaded while its grid is active or triggers are linked.
extern WorldCollisionRoomResources gShelterB1UndergroundParkingRoomCollisionResources[SHELTER_B1_UNDERGROUND_PARKING_ROOM_COUNT];

/// Logical-view maps for underground parking rooms 1 through 8.
///
/// Index [room - 1][logicalView - 1] for logical views 1..24. Each unsigned byte
/// is a 1-based camera, image and sprite index in 1..24; there is no terminator.
/// Room 1 borrows gameplay's `gViewIdentityMap`; the other maps belong to this
/// overlay. Keep both overlays loaded while their maps are used.
extern u8* gShelterB1UndergroundParkingRoomViewMaps[SHELTER_B1_UNDERGROUND_PARKING_ROOM_COUNT];

/// Logical-view counts for underground parking rooms 1 through 8.
///
/// Indexed by room - 1 through the Shelter area's view-count directory. Every
/// signed halfword counts all 24 logical slots in that room's view map; there
/// is no terminator. The map directory borrows this overlay's storage.
extern ViewCount gShelterB1UndergroundParkingRoomViewCounts[SHELTER_B1_UNDERGROUND_PARKING_ROOM_COUNT];

/// Number of underground parking's warp endpoints; the array has no terminator.
enum { SHELTER_B1_UNDERGROUND_PARKING_WARP_COUNT = 2 };

/// Arrival placements and departure options for underground parking warps 1 and 2.
///
/// The Shelter area-20 directory indexes by warp - 1. Warp 1 connects to the
/// access tunnel, selects view 2 and has arrival/departure sounds. Warp 2 selects
/// view 3, plays its arrival sound and marks the parking area on the map. Positions
/// use room game units and yaw uses 4096 units per turn. Neither endpoint sets
/// scripted-player or departure-fade flags. Borrowed while this overlay is loaded.
extern DirectionWarpEntry gShelterB1UndergroundParkingWarpEntries[SHELTER_B1_UNDERGROUND_PARKING_WARP_COUNT];

/// Camera records for underground parking's 24 mapped views.
///
/// Indexed by mapped view - 1, independently of logical room-view numbering.
/// Rotation coefficients use Q12 (ONE is unity); translations are negated
/// world-space camera origins in game units, and screen distances are pixels.
/// The Shelter area-20 directory borrows this array while the room overlay is loaded.
extern ViewCamera gShelterB1UndergroundParkingViewCameras[SHELTER_B1_UNDERGROUND_PARKING_VIEW_COUNT];

/// Sprite resources for underground parking's 24 mapped views.
///
/// Indexed by mapped view - 1 through the Shelter area-20 directory. Every view
/// selects decoded background strips; views 3/7/11, 4/8, 10, 12, 13/14 and 17
/// also share or select source sprites. Empty views retain their batch-list
/// address in `sources.empty`. All draw-area pointers are NULL. Descriptors and
/// mutable source/batch lists remain borrowed while this room overlay is loaded.
extern SpriteView gShelterB1UndergroundParkingSpriteViews[SHELTER_B1_UNDERGROUND_PARKING_VIEW_COUNT];

/// Collision policies and footstep cues for underground parking surface classes 0..7.
///
/// Indexed by a grid face or contact's surface class, independently of room number.
/// Every class blocks probes, allows weapon impacts and applies pushback; only
/// class 1 has footstep cues. Classes 0 and 3..7 share the default record, and
/// class 2 retains its separate silent record. Borrowed storage lasts with this overlay.
extern WorldCollisionSurfaceProperties* gShelterB1UndergroundParkingSurfacePropertiesByClass[8];

/// Runs the underground parking room's initialization, interaction checks and shutdown.
///
/// The Shelter map's area-20 descriptor starts a bodyless task at state 0.
/// States 0, 1 and 2 initialize, check room interactions each frame and kill
/// the task, respectively; no state bounds check is performed. The room overlay
/// and its state table must remain loaded for the task's lifetime.
void shelterB1UndergroundParkingRoomTask(Task* task);

/// Updates this room's telephone save and play-data menu.
///
/// Gameplay routes the area-20 menu callback here. Requires a live UI object in
/// `task->spawnArg2.pointer` and this room overlay for the menu's lifetime.
/// State 0 opens saving directly during normal play; after a clear or in demo
/// scene 1 it offers saving and statistics. Child answers drive the notice and
/// reopening states, and cancellation is published on the owning UI object.
void shelterB1UndergroundParkingTelephoneMenuTask(Task* task);

/// Draws this room's light glows and beams for the current mapped view each frame.
///
/// Gameplay effect slot 0x133 calls this with an unused task argument. The
/// view mapping, view matrix, scratch stack, packet arena and depth ordering
/// table must be ready for the included glow drawers. Unlisted views emit no
/// packets. The cyan light uses the room's slow/fast pulse selection; the other
/// lights keep their frame-parity flicker. Queued packets borrow the current
/// frame arena until GPU completion; this callback owns no task work or body.
void shelterB1UndergroundParkingDrawGlowsTask(Task* unused);

#endif // INCLUDE_ROOMS_SHELTER_B1_UNDERGROUND_PARKING_H
