#ifndef INCLUDE_ROOMS_DRYFIELD_NIGHT_GARAGE_H
#define INCLUDE_ROOMS_DRYFIELD_NIGHT_GARAGE_H

#include "types.h"

#include "gameplay/area.h"
#include "gameplay/direction.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/task_types.h"

/// Garage movie launcher (0) and display-playback (1) task descriptors.
///
/// Both are bodyless, priority 192. Spawn argument 1 selects the primary movie
/// for zero or alternate for nonzero; argument 2 is unused. The launcher hands
/// presentation to playback, which supports skipping and restores game graphics.
/// Keep the garage overlay loaded through playback and restoration.
extern TaskDesc gDryfieldNightGarageMovieTasks[2];

/// Bodyless priority-192 display task for the night garage's shop session.
///
/// Gameplay queues this recipe by the saved stage/area. Spawn argument 1 is
/// the packed stock-profile/category selector; argument 2 is unused on entry
/// and then holds the owned UI root. The callback owns the UI tree and stage
/// primitive buffer until closure. Keep the garage and catalog resources
/// loaded through the session and its display-mode exit.
extern TaskDesc gDryfieldNightGarageShopSessionTaskDesc;

/// Placement/resource directory for the night garage, indexed by saved area variant.
///
/// Requires an index in 0..11. Slots 1..4 select Scavengers, Gary Douglas/Flint,
/// an empty companion layout and Zebra Stalkers; slots 0 and 5..11 are absent.
/// Gameplay pairs this directory with the garage's shared Dryfield save slot.
/// Placements belong to the loaded Dryfield map and recipes to this room overlay;
/// keep both owners and their selected actor resources live during loading/spawn.
extern AreaVariant gDryfieldNightGarageAreaVariants[12];

/// Lighting for night-garage rooms 1 and 2, indexed by room ID minus one.
///
/// Both entries borrow the same seven point lights and logical-view ambient
/// minima. The records and their resources remain valid while this room overlay
/// is loaded; light transforms and attenuation are writable query state.
extern WorldCoordRoomLighting gDryfieldNightGarageRoomLighting[2];

/// Collision resources for night-garage rooms 1 and 2, indexed by room ID minus one.
///
/// Each room has its own grid, view boundaries and action triggers; neither
/// has sight occluders. Setup borrows and links mutable room-owned resources,
/// which must stay loaded until their grid is inactive and triggers unlinked.
extern WorldCollisionRoomResources gDryfieldNightGarageRoomCollisionResources[2];

/// Logical-view maps for night-garage rooms 1 and 2, indexed by room ID minus one.
///
/// Each map accepts views 1..15 at index view minus one and returns a 1-based
/// camera, background-file and sprite-view index. Room 1 borrows gameplay's
/// identity map; room 2 remaps several views to alternate images. The directory
/// and room-2 map require the room overlay to remain loaded.
extern u8* gDryfieldNightGarageViewMaps[2];

/// Logical-view search counts for night-garage rooms 1 and 2.
///
/// Indexed by room ID minus one; both maps contain 15 valid logical views.
/// These signed element counts bound reverse lookup in the loaded room maps.
extern ViewCount gDryfieldNightGarageViewCounts[2];

/// The night garage's factory and junk-yard warp endpoints, in slots 1 and 2.
///
/// Indexed by warp ID minus one, with no terminator. Both player and companion
/// use the same placement at each endpoint, in whole room units and 4096-angle
/// units per turn. Slot 1 arrives in view 2 and fades departure; slot 2 arrives
/// in view 6 and has a blocked-departure sound. Borrow only while the overlay
/// is loaded; transition handling copies the selected record.
extern DirectionWarpEntry gDryfieldNightGarageWarpEntries[2];

/// Cameras for the night garage's mapped view files 1..15.
///
/// Indexed by mapped view minus one, after the current room's logical-view map.
/// Rotation coefficients use 4096 for unity; translations are negated camera
/// origins in world units and projection distances are pixels. Queued camera
/// application borrows a record until dispatch, requiring the loaded overlay.
extern ViewCamera gDryfieldNightGarageViewCameras[15];

/// Sprite resources for the night garage's mapped view files 1..15.
///
/// Indexed by mapped view minus one, independently of logical view numbers.
/// Each batch list ends with SPRITE_BATCH_END; views 1, 8, 10, 11, 13 and 14
/// have no sources, and every view retains decoded background strips. No view
/// supplies clipping areas. Sources and batches are mutable room-owned data
/// and must stay loaded while sprite packets are allocated or linked.
extern SpriteView gDryfieldNightGarageSpriteViews[15];

/// Collision policies for the night garage's eight surface classes, indexed 0..7.
///
/// Every class blocks probes and permits weapon impacts and pushback. Class 1
/// supplies footstep cues; classes 0 and 2..7 share a silent policy. Both room
/// grids use this directory. The borrowed records and sound data remain valid
/// only while the room overlay is loaded.
extern WorldCollisionSurfaceProperties* gDryfieldNightGarageSurfacePropertiesByClass[8];

/// Places the low collision box in the live night-garage mesh.
///
/// Replaces its first four faces, four normals and eight vertices with the box
/// template, then translates XYZ by (4715, -132, 5900) game units for zero
/// `useFarPosition`, or (4715, -132, 10000) for nonzero. Gary Douglas's setup
/// uses zero before the refueling event; both completion and skip scripts use
/// nonzero. Only XYZ and the face records are replaced; the vector fourth
/// components, remaining mesh and cell lists stay intact. Requires the loaded
/// room overlay and writable live grid pools, which the room owns.
void dryfieldNightGaragePlaceLowCollisionBox(s32 useFarPosition);

/// Draws the night garage's additive grey capsule glows for the current view.
///
/// Gameplay effect bank 6, slot 0x114. Views 3 and 15 draw four strips; views
/// 7 and 14 draw the first strip; view 11 draws two strips with half-turned
/// capsule caps, sharing one strip with views 3 and 15. Other views draw none.
/// The callback ignores `unusedTask` and the spawn arguments. Requires the
/// loaded room overlay, composed view matrix and the current frame's
/// initialized scratch stack, ordering table and GPU packet arena.
void dryfieldNightGarageDrawGlowsTask(Task* unusedTask);

/// Runs the night garage's persistent room-message task.
///
/// `task->state` must be 0 (initialize), 1 (idle for synchronous messages), or
/// 2 (kill). Initialization registers the task in `GAME_TASK_SLOT_ROOM` and
/// advances to idle. Spawned by the Dryfield map's stage-3/area-24 descriptor;
/// requires the room overlay and that variant's actor/map resources to be live.
void dryfieldNightGarageRoomTask(Task* task);

#endif // INCLUDE_ROOMS_DRYFIELD_NIGHT_GARAGE_H
