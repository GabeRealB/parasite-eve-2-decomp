#ifndef INCLUDE_ROOMS_MINE_REFUGE_H
#define INCLUDE_ROOMS_MINE_REFUGE_H

#include "types.h"

#include "gameplay/area.h"
#include "gameplay/direction.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/task_types.h"

/// Number of retained placement-variant slots, including absent slots.
enum { MINE_REFUGE_AREA_VARIANT_COUNT = 11 };

/// Actor placements and loading/spawn recipes indexed by `GameLocationKey.variant`.
///
/// Slots 1 are present; every other slot is absent (NULL, NULL).
/// Slot 1 has no placed actors but retains its base-file loading recipe.
/// Supply an index in 0..10; this directory has no terminator or bounds check.
/// The placement selector is independent of room and view selectors.
/// Placements and recipes belong to this room overlay; keep it loaded while
/// loading or using either table. Referenced actor overlays must be loaded for
/// spawning and model-policy updates.
extern AreaVariant gMineRefugeAreaVariants[MINE_REFUGE_AREA_VARIANT_COUNT];

/// Number of logical and mapped camera/sprite views in the refuge's identity map.
enum { MINE_REFUGE_VIEW_COUNT = 7 };

/// Lighting descriptor for Mine Refuge room 1.
///
/// Indexed by room minus one. Borrows two mutable point lights and ambient
/// minima for logical views 1..7; all storage stays live with the refuge overlay.
extern WorldCoordRoomLighting gMineRefugeRoomLighting[1];

/// Collision resources for the single room, indexed by room minus one.
///
/// The single room borrows its grid, two view boundaries and six action triggers; there
/// are no sight occluders.
///
/// The stage map borrows this directory while the room overlay is loaded. Keep the grid
/// and its pools live while active, and the mutable trigger/occluder lists live until
/// unlinked. This directory has no terminator or bounds check.
extern WorldCollisionRoomResources gMineRefugeRoomCollisionResources[1];

/// Logical-view map directory for Mine Refuge room 1.
///
/// Indexed by room minus one, then logical view minus one. Borrows gameplay's
/// `gViewIdentityMap`; views 1..7 use the same camera, image and sprite indices.
/// The directory stays live with the refuge overlay; the map stays live with gameplay.
extern u8* gMineRefugeRoomViewMaps[1];

/// Reverse-lookup limit for room 1's seven logical views.
///
/// Indexed by room minus one. Counts readable view-map bytes independently of
/// the shared identity map's capacity; the array stays live with the refuge overlay.
extern ViewCount gMineRefugeRoomViewCounts[1];

/// A single warp endpoint record, selected by warp ID 1.
///
/// Slot 1 serves the Gorge connection, with arrival view 2 and paired arrival/departure
/// sounds. It has no map marker.
///
/// Positions use signed whole world coordinates; yaw uses 4096 units per turn. The two
/// byte spans have unproven roles and remain zero. The stage map borrows this array;
/// keep the room overlay loaded through departure or arrival setup. Consumers copy
/// complete entries and actor spawns borrow their transforms synchronously. There is
/// no terminator or bounds check.
extern DirectionWarpEntry gMineRefugeWarpEntries[1];

/// Camera transforms and pixel projection distances for refuge mapped views 1..7.
///
/// Indexed by mapped view minus one. Views 4 and 5 have equal camera records;
/// pointers borrowed by camera tasks require the refuge overlay until applied.
extern ViewCamera gMineRefugeViewCameras[MINE_REFUGE_VIEW_COUNT];

/// Sprite descriptors for refuge mapped views 1..7.
///
/// Indexed by mapped view minus one. View 2 has 79 source sprites and view 3
/// has 33; the other views retain empty batch lists. Every view keeps decoded
/// background strips and has no clipping areas. Descriptors and mutable sources
/// and batches stay live with the refuge overlay.
extern SpriteView gMineRefugeSpriteViews[MINE_REFUGE_VIEW_COUNT];

/// Collision and footstep properties for the refuge's eight surface classes.
///
/// Indexed directly by class 0..7, with no terminator. All classes block probes,
/// permit weapon impacts and apply pushback. Only class 1 has footstep cues;
/// classes 0 and 2..7 share silent properties. The grid uses classes 0 and 1.
/// The directory and its borrowed records stay live with the refuge overlay.
extern WorldCollisionSurfaceProperties* gMineRefugeSurfacePropertiesByClass[8];

/// Runs the refuge telephone's save menu and unlocked statistics panels.
///
/// Called each tick by the saved-area telephone dispatcher with the loaded
/// refuge overlay. Borrows the live `UiObject` in `task->spawnArg2` and owns
/// its child dialogs; their storage must remain live through dismissal.
/// Before a clear, normal play enters the save dialog directly. A cleared game
/// or statistics demo shows Save, Play Data, Weapon Data and PE Data choices.
void mineRefugeTelephoneMenuTask(Task* task);

/// Draws the Mine Refuge lights selected by the mapped camera view.
///
/// View 2 draws a textured flare and cyan star; view 6 draws a cyan burst.
/// Views 3..5 draw the layered panel glow only while the power-panel flag is 1,
/// with a smaller radius in view 3. Other views queue nothing.
/// The task argument is unused. Requires the room's composed view matrix,
/// current frame packet arena and depth ordering table, and initialized scratch
/// stack. Drawing a panel glow leaves one 16-byte scratch reservation active
/// until the enclosing stack reset. The normal game loop resets it each frame.
void mineRefugeDrawGlowsTask(Task* task);

/// Dispatches the refuge room task's initialization, idle and release states.
///
/// `task->state` must be 0..2. Initialization installs the room message table,
/// publishes the room task and enables CAP completion sound cues; idle keeps
/// those handlers available, and state 2 releases the task. The refuge overlay
/// must remain loaded while the task or its installed callbacks can run.
void mineRefugeRoomTask(Task* task);

#endif // INCLUDE_ROOMS_MINE_REFUGE_H
