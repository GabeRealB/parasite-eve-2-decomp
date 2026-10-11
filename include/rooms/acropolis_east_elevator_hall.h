#ifndef INCLUDE_ROOMS_ACROPOLIS_EAST_ELEVATOR_HALL_H
#define INCLUDE_ROOMS_ACROPOLIS_EAST_ELEVATOR_HALL_H

#include "types.h"

#include "gameplay/area.h"
#include "gameplay/direction.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/task_types.h"

/// Extents of the hall's room directory and mapped camera/sprite resources.
enum {
    ACROPOLIS_EAST_ELEVATOR_HALL_ROOM_COUNT = 1,
    ACROPOLIS_EAST_ELEVATOR_HALL_VIEW_COUNT = 7,
    ACROPOLIS_EAST_ELEVATOR_HALL_WARP_COUNT = 1,
};

/// Actor layout variants for the Acropolis east elevator hall.
///
/// Indexed directly by `GameLocationKey.variant` (0..2); no terminator.
/// Slot 1 publishes a placement/resource pair; all other slots are NULL pairs.
/// The room owns recipes and the stage map owns placements. Keep the map
/// loaded while actors borrow placements.
/// Keep the room loaded during loading, spawning and model-policy updates,
/// and each selected actor overlay loaded throughout its tasks' lifetime.
extern AreaVariant gAcropolisEastElevatorHallAreaVariants[3];

/// Collision resources for the hall's single room, indexed by room minus one.
///
/// Supplies the grid, view boundaries and action triggers, with no sight
/// occluders. The Acropolis map borrows this directory and mutable resources
/// while the room overlay is loaded; keep it live until the triggers are unlinked.
extern WorldCollisionRoomResources gAcropolisEastElevatorHallRoomCollisionResources[ACROPOLIS_EAST_ELEVATOR_HALL_ROOM_COUNT];

/// Logical-view map directory for the hall's single room.
///
/// Room 1 borrows `gViewIdentityMap`: logical views 1..7 select resources 1..7.
/// The map overlay borrows this directory while the room overlay is loaded.
extern u8* gAcropolisEastElevatorHallViewMaps[ACROPOLIS_EAST_ELEVATOR_HALL_ROOM_COUNT];

/// Room 1's logical-view count, limiting reverse searches to seven map entries.
extern ViewCount gAcropolisEastElevatorHallViewCounts[ACROPOLIS_EAST_ELEVATOR_HALL_ROOM_COUNT];

/// Room-1 model lighting: four point lights and default ambient minima.
///
/// Indexed by room minus one. The stage map borrows this record and its mutable
/// light collection while the room overlay is loaded; no per-view ambient table
/// is supplied. The lights contribute in every view.
extern WorldCoordRoomLighting gAcropolisEastElevatorHallRoomLightingTable[ACROPOLIS_EAST_ELEVATOR_HALL_ROOM_COUNT];

/// The hall's warp-1 endpoint, borrowed by the Acropolis map's area-2 directory.
///
/// Player and companion arrive at (-5248, -31, 1068), yaw 1024 (4096 per turn),
/// in view 4. Supplies arrival/departure sounds, no blocked sound or warp flags,
/// and map marker 502. There is no terminator; keep the room overlay loaded
/// while the endpoint is read or copied.
extern DirectionWarpEntry gAcropolisEastElevatorHallWarpEntries[ACROPOLIS_EAST_ELEVATOR_HALL_WARP_COUNT];

/// Sprite resources for mapped hall views 1..7, indexed by view minus one.
///
/// Views 2..5 have source sprites; views 1, 6 and 7 have empty batch lists.
/// Every view retains decoded background strips and has no draw-area override.
/// Descriptors, source elements and batch lists remain borrowed and writable
/// while the room overlay is loaded.
extern SpriteView gAcropolisEastElevatorHallSpriteViews[ACROPOLIS_EAST_ELEVATOR_HALL_VIEW_COUNT];

/// Cameras for mapped hall views 1..7, indexed by view minus one.
///
/// Rotation coefficients use Q12, translations are negated world-space camera
/// origins in game units, and projection distances are pixels. Keep the overlay
/// loaded until a queued camera-application task has copied its selected record.
extern ViewCamera gAcropolisEastElevatorHallViewCameras[ACROPOLIS_EAST_ELEVATOR_HALL_VIEW_COUNT];

/// Collision policies and footstep cues indexed by hall surface class 0..7.
///
/// Class 1 blocks probes and applies pushback, without weapon impacts or footsteps.
/// The other seven classes share the ordinary impact and footstep policy. The
/// grid uses classes 0, 1 and 3; setup reads all eight entries. Records and sound
/// data remain borrowed while this room overlay is loaded.
extern WorldCollisionSurfaceProperties* gAcropolisEastElevatorHallSurfacePropertiesByClass[8];

/// Starts floor/mirror player reflections and refreshes the hall's six red beacons.
///
/// Bank-6 slot 0x21 starts in state 0 with a live coordinate body and player.
/// Spawns both reflection controllers once, then enters state 1, including
/// that first tick. While view 2 is active, spawns two small and four large
/// one-frame beacons at room offsets under the borrowed coordinate. Spawn
/// failures are ignored. The controller remains live until external teardown;
/// keep the room overlay and its coordinate hierarchy live for its lifetime.
void acropolisEastElevatorHallReflectionsAndBeaconsTask(Task* task);

/// Draws one frame of a pulsing additive red diamond, then ends the counted effect.
///
/// Bank-6 slot 0x22 requires a live coordinate body and owned `EffectWork` in
/// `spawnArg2.pointer`, distinct from `Task::work`. The low two bytes of
/// `spawnArg1` are unsigned pulse rate and size (0..255); its high half is ignored.
/// The frame counter times the rate wraps to an 8-bit phase folded into red
/// levels 0..128. Half extent is `size * 512 / depth` screen pixels.
///
/// Projects the composed translation through `GsWSMATRIX`, narrowing each
/// component to signed 16-bit game units. Depth is SZ3 / 4 (0..16383); values
/// below 17 queue nothing, and GTE FLAG is not tested. Requires initialized GTE
/// projection settings, one aligned `RoomGlowSpriteScratch` block and arena
/// room for two `POLY_G4`/`DR_TPAGE` pairs at accepted depth. The scaled depth
/// wraps into the current 1024-tag ordering table; packets live through GPU completion.
/// Scratch and effect work are released on every call. Keep this overlay loaded
/// through the callback; the room respawns its six beacons while view 2 is active.
void acropolisEastElevatorHallRedBeaconTask(Task* task);

/// Initializes and updates the player's floor or mirror-plane reflection in this room.
///
/// State must be 0 (start bodyless) or 1 (initialized model and work).
/// `spawnArg1.value` selects the floor (0) or room mirror plane (1).
/// Requires a live player with a TMD body. The task owns its cloned model and
/// work, borrows the player's geometry, and becomes a child of the player.
/// Keep this overlay and the player live while the reflection and its
/// attachment reflections run.
void acropolisEastElevatorHallPlayerReflectionTask(Task* reflectionTask);

/// Draws one additive gray pixel at the effect coordinate, then ends the effect.
///
/// Bank-6 slot 0x23 requires a live coordinate body and a counted room effect.
/// Its composed translation narrows to signed 16-bit game coordinates before
/// projection through `GsWSMATRIX`. Only SZ3 / 4 depths at least
/// `EFFECT_POINT_TILE_MIN_DEPTH` queue the tile; GTE FLAG is not tested.
/// The frame's word-aligned packet arena needs room for one `TILE_1` and, at
/// an accepted depth, one `DR_TPAGE`; packets remain live until GPU drawing.
///
/// Every call frees `spawnArg2.pointer` and tears down the task, even when
/// nothing is drawn. That argument must be NULL or an owned primary-heap
/// allocation, distinct from `Task::work`, with nested resources released.
/// Keep this overlay loaded through the callback.
void acropolisEastElevatorHallPointTileTask(Task* task);

/// Runs the east elevator hall's room messages and player debug display.
///
/// State 0 registers the receiver and initializes the placed actor; state 1
/// updates the player debug display; state 2 releases the task.
/// Start with a live bodyless task in state 0. The state must remain in 0..2;
/// dispatch performs no bounds check. Keep the room overlay and gameplay
/// resources loaded through the selected handler, which may release the task.
void acropolisEastElevatorHallRoomTask(Task* task);

#endif // INCLUDE_ROOMS_ACROPOLIS_EAST_ELEVATOR_HALL_H
