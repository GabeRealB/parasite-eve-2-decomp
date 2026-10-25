#ifndef INCLUDE_ROOMS_SHELTER_B1_ARMORY_H
#define INCLUDE_ROOMS_SHELTER_B1_ARMORY_H

#include "types.h"

#include "gameplay/area.h"
#include "gameplay/collision.h"
#include "gameplay/direction.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/task_types.h"

/// Bodyless shop-session recipe selected when opening the armory shop.
///
/// Runs at priority `TASK_PRIORITY_CONTROL` with the stock selector in spawnArg1 and zero in
/// spawnArg2. The session owns its UI tree and shared drawing buffer until
/// closure; keep this room's shop code and stock tables loaded throughout.
extern TaskDesc gShelterB1ArmoryShopSessionTaskDesc;

/// Number of retained placement-variant slots, including absent slots.
enum { SHELTER_B1_ARMORY_AREA_VARIANT_COUNT = 11 };

/// Shelter B1 armory placement/resource directory indexed by `GameLocationKey.variant`.
///
/// All eleven slots are absent (NULL, NULL); this room publishes no actor
/// placement or resource lists. The directory itself remains present.
/// Keep the room overlay loaded while the directory is borrowed.
/// Selectors must be in 0..10; there is no terminator or bounds check.
extern AreaVariant gShelterB1ArmoryAreaVariants[SHELTER_B1_ARMORY_AREA_VARIANT_COUNT];

/// Logical-view maps for the armory's single room, indexed by room ID minus one.
///
/// Room 1 borrows `gViewIdentityMap`: logical views 1..13 select the same
/// camera, image and sprite indices. The directory lives with this room overlay;
/// its shared byte map lives with gameplay. Neither level has a terminator.
extern u8* gShelterB1ArmoryRoomViewMaps[1];

/// Logical-view reverse-search limit for this area's sole room.
///
/// Room 1 searches 13 entries in its view map. Index zero corresponds to room ID 1.
/// Counts are signed halfword element counts, independent of map capacity.
/// The stage map borrows this array while the room overlay is loaded; there
/// is no terminator.
extern ViewCount gShelterB1ArmoryRoomViewCounts[1];

/// Warp endpoint records for slots 1..4, indexed by warp minus one.
///
/// Slots 1/2 serve the Main Corridor and Storeroom connections, with arrival views 2/5.
/// Slots 3/4 are opposing internal door endpoints in views 3/4, sharing
/// arrival/departure/blocked sounds and a marker. Slot 2 marks the storeroom door; slot
/// 1 has no marker.
///
/// Positions use signed whole world coordinates; yaw uses 4096 units per turn. The two
/// byte spans have unproven roles and remain zero. The stage map borrows this array;
/// keep the room overlay loaded through departure or arrival setup. Consumers copy
/// complete entries and actor spawns borrow their transforms synchronously. There is
/// no terminator or bounds check.
extern DirectionWarpEntry gShelterB1ArmoryWarpEntries[4];

/// The armory's room-space collision mesh and 4-by-3 XZ cell index.
///
/// Cells have 4000-unit sides and an origin at (-1400, -4400). The mesh has
/// 55 quad faces, 111 vertices and 11 unit normals scaled by `ONE`.
/// Faces use room-local surface classes 0 and 1. Activation binds the mutable
/// descriptor to the current view; it and its pools stay live with this overlay.
extern WorldCollisionGrid gShelterB1ArmoryCollisionGrid;

/// Authored cameras for mapped view indices 1..13, stored at index minus one.
///
/// Rotation coefficients use `ONE` (4096) per unit; translation is the negated
/// world-space camera origin in game units. Projection distance is in pixels.
/// The stage map borrows this array; keep the room overlay loaded until queued
/// camera tasks have applied their records. There is no terminator.
extern ViewCamera gShelterB1ArmoryViewCameras[13];

/// Shelter B1 armory's sprite resources, indexed by mapped view minus one.
///
/// Views 1 and 9..13 have no sources; views 2..8 borrow sprite pools.
/// The room uses an identity logical-view map.
/// Every slot retains decoded image strips and has no clipping list. Empty
/// source slots retain batch addresses without supplying SpriteSource elements.
/// Descriptors and mutable source/batch lists stay borrowed while the owning
/// room overlay is loaded; selectors must be nonzero and within this array.
extern SpriteView gShelterB1ArmorySpriteViews[13];

/// The armory's 22 point lights for model shading and room-light queries.
///
/// All lights are enabled in every view; there are no directional or cone
/// lights. Gameplay updates their coordinate caches and attenuation in place.
/// The descriptor and its borrowed array remain valid while this overlay is loaded.
extern WorldCoordRoomLights gShelterB1ArmoryRoomLights;

/// Directed view boundaries for logical views 2<->3, 2<->8, 4<->5, 4<->6, 6<->7.
///
/// All 10 records are live; the final one carries LAST, with no dummy
/// terminator. Room binding supplies the current view coordinate and enables
/// the quads. Origins, corner offsets and radii use room game units; normals
/// use 4096 per unit. Keep this mutable storage loaded until its list is cleared.
extern WorldCollisionTrigger gShelterB1ArmoryViewBoundaryTriggers[10];

/// Interaction and transition quads for this room.
///
/// Four door quads use departure slots 1..4; slots 3 and 4 both return to
/// this area at the opposite endpoint. CAP selectors 10 and 12 route through
/// the room command handler; room action 1 opens the shop. The two action-255
/// quads are retained even though the room action handler ignores that ID.
///
/// All 23 records are live; the final one carries LAST, with no dummy
/// terminator. Room binding supplies the current view coordinate and enables
/// the quads. Origins, corner offsets and radii use room game units; normals
/// use 4096 per unit. Keep this mutable storage loaded until its list is cleared.
extern WorldCollisionTrigger gShelterB1ArmoryActionTriggers[23];

/// The armory's collision and footstep policies, indexed by surface class 0..7.
///
/// All classes block probes, allow weapon impacts and apply pushback. Class 1
/// supplies paired walking/running footsteps and uses the walking base for
/// scripted jumps; classes 0 and 2..7 share a record without footstep sounds.
/// The directory and its borrowed records remain live with this room overlay.
extern WorldCollisionSurfaceProperties* gShelterB1ArmorySurfacePropertiesByClass[8];

/// Dispatches the armory controller's initialization, idle tick and teardown.
///
/// The Mine/Shelter area's room-task table starts it in state 0. Requires state
/// 0..2: initialization publishes the room task and its message table, state 1
/// waits for messages, and state 2 kills the task. The index is unchecked.
/// Keep this overlay loaded while its controller or spawned events are live.
void shelterB1ArmoryRoomTask(Task* task);

/// Draws the armory's view-dependent beams and glows for the current frame.
///
/// The lock indicator is green when the armory is unlocked and red otherwise.
/// The task argument is unused; no task state or spawn arguments are read.
/// Requires this room overlay to be loaded, a valid current view mapping and
/// composed view matrices, plus an initialized scratch stack and enough space
/// in the current frame's GPU packet arena and depth ordering table.
void shelterB1ArmoryDrawGlowsTask(Task* unusedTask);

#endif // INCLUDE_ROOMS_SHELTER_B1_ARMORY_H
