#ifndef INCLUDE_ROOMS_DRYFIELD_NIGHT_DILAPIDATED_HOUSE_H
#define INCLUDE_ROOMS_DRYFIELD_NIGHT_DILAPIDATED_HOUSE_H

#include "types.h"

#include "gameplay/area.h"
#include "gameplay/direction.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/task_types.h"

/// Dryfield night dilapidated house placement/resource directory indexed by `GameLocationKey.variant` 0..21.
///
/// Slots 3, 4, 21 publish paired tables; all other slots are absent (NULL, NULL).
/// This placement selector is independent of the room and logical-view selectors.
/// Placements belong to the stage map; resource recipes belong to this room.
/// Keep both overlays and the actors supplying task descriptors loaded during
/// loading, spawning and model-flag updates. Lookups require an index within
/// this array; there is no terminator or bounds check.
extern AreaVariant gDryfieldNightDilapidatedHouseAreaVariants[22];

/// Light collection and minimum ambient colours for the house's single room.
///
/// Indexed by room selector minus one (room 1 only). Borrows the mutable room
/// lights and the ambient table for logical views 1..11. The stage directory,
/// this descriptor and its resources are valid while this overlay is loaded.
extern WorldCoordRoomLighting gDryfieldNightDilapidatedHouseRoomLighting[1];

/// Collision resources for the nighttime house's single room selector.
///
/// Indexed by room ID minus one (room 1 only). Borrows the mutable room grid,
/// eight view boundaries, twelve action triggers and one sight occluder.
/// Gameplay binds these to the active view and links through each LAST marker;
/// keep this overlay loaded until the grid is retired and all lists unlinked.
extern WorldCollisionRoomResources gDryfieldNightDilapidatedHouseRoomCollisionResources[1];

/// Logical-view maps for the house's single room selector.
///
/// Entry zero borrows `gViewIdentityMap`; logical views 1..11 map to the same
/// camera, image and sprite indices. The directory belongs to this overlay
/// and must remain loaded during stage-map lookups.
extern u8* gDryfieldNightDilapidatedHouseViewMaps[1];

/// Reverse-lookup view count for room 1: eleven entries of its logical-view map.
///
/// The stage map borrows this signed element count while the room is loaded.
extern ViewCount gDryfieldNightDilapidatedHouseViewCounts[1];

/// Three arrival/departure endpoints for the nighttime house, indexed by warp ID minus one.
///
/// IDs 1 and 3 share the back-street placement and view 6; ID 2 uses the
/// warehouse placement and view 2. Only ID 1 supplies sounds and a map flag.
/// Positions use whole room units and yaw uses 4096 units per turn. Gameplay
/// copies complete entries from the stage directory while this overlay is loaded.
extern DirectionWarpEntry gDryfieldNightDilapidatedHouseWarpEntries[3];

/// Cameras for mapped house views 1..11, indexed by mapped view minus one.
///
/// Rotations use ONE for unit scale, translations are negated world-space
/// camera origins in game units, and projection distances are pixels.
/// The stage map borrows these records while this room overlay is loaded.
extern ViewCamera gDryfieldNightDilapidatedHouseViewCameras[11];

/// Foreground sprite resources for mapped house views 1..11.
///
/// Indexed by mapped view minus one. Every view keeps its decoded background;
/// views 1, 9 and 10 have no source sprites. Batch ranges count source elements,
/// and no view installs a clipping list. Descriptors and their mutable source
/// and batch arrays remain valid while this room overlay is loaded.
extern SpriteView gDryfieldNightDilapidatedHouseSpriteViews[11];

/// Surface-class directory for the nighttime house, indexed by class 0..7.
///
/// All classes block probes, allow weapon impacts and apply grid pushback.
/// Classes 1 and 2 supply footstep cues; classes 0 and 3..7 do not. The stage
/// map borrows this directory and its records only while this overlay is loaded.
extern WorldCollisionSurfaceProperties* gDryfieldNightDilapidatedHouseSurfacePropertiesByClass[8];

/// Draws the nighttime dilapidated house's view-selected additive light prisms.
///
/// Bank-6 effect 0x106 requires a live `TASK_BODY_COORD` task and the loaded
/// room overlay, with a logical view ID in 1..11. Refreshes the task's composed
/// transform and selects the three prisms by logical view, before camera mapping.
/// Each selected prism queues five Gouraud quads and additive blend commands.
/// The view projection, scratch stack, depth ordering table and frame packet
/// arena must be ready; queued packet storage must survive GPU drawing.
void dryfieldNightDilapidatedHouseDrawLightPrismsTask(Task* task);

/// Runs the nighttime dilapidated house's room message receiver and first-visit event.
///
/// Requires a live task and this room overlay loaded. The state must be 0 setup,
/// 1 idle or 2 teardown; dispatch has no bounds check and teardown releases the
/// task. Setup registers the receiver in `GAME_TASK_SLOT_ROOM` and advances
/// progress once, starting the event's normal/skip scripts only with a companion.
void dryfieldNightDilapidatedHouseRoomTask(Task* task);

#endif // INCLUDE_ROOMS_DRYFIELD_NIGHT_DILAPIDATED_HOUSE_H
