#ifndef INCLUDE_ROOMS_ACROPOLIS_HALLWAY_H
#define INCLUDE_ROOMS_ACROPOLIS_HALLWAY_H

#include "types.h"

#include "gameplay/area.h"
#include "gameplay/direction.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/task_types.h"
#include "main/tmd_types.h"

/// Actor layout variants for the Acropolis hallway.
///
/// Indexed directly by `GameLocationKey.variant` (0..12); no terminator.
/// Slots 1..4 publish placement/resource pairs; all other slots are NULL pairs.
/// The room owns recipes and the stage map owns placements. Keep the map
/// loaded while actors borrow placements.
/// Keep the room loaded during loading, spawning and model-policy updates,
/// and each selected actor overlay loaded throughout its tasks' lifetime.
extern AreaVariant gAcropolisHallwayAreaVariants[13];

extern TmdSource gAkropolisHallwayAcropolisHallwayModel01AE0;

/// Collision resources for hallway room 1: mesh, view boundaries and action quads.
///
/// The stage map borrows this one-record room directory. Activation binds and
/// links its mutable grid and triggers; there are no sight occluders. Keep the
/// hallway overlay loaded until the grid is retired and its lists are unlinked.
extern WorldCollisionRoomResources gAcropolisHallwayRoomCollisionResources[1];

/// Hallway room 1's logical-view map, borrowing gameplay's identity map.
///
/// Logical views 1..5 map directly to camera, image and sprite indices 1..5.
/// The shared map's capacity is independent of this room's five-view limit.
/// The directory requires the hallway overlay; the bytes require gameplay.
extern u8* gAcropolisHallwayViewMaps[1];

/// Hallway room 1's five logical views, limiting reverse map searches.
///
/// The signed-halfword element is indexed by room - 1, with no terminator.
/// It counts logical entries rather than the shared identity map's capacity;
/// the stage map borrows it while the hallway overlay is loaded.
extern ViewCount gAcropolisHallwayViewCounts[1];

/// Hallway room 1's three point lights and default ambient minima.
///
/// Indexed by room - 1. The NULL ambient table selects gameplay's defaults.
/// The descriptor and its writable light records require the hallway overlay
/// to remain loaded throughout coordinate updates and shading queries.
extern WorldCoordRoomLighting gAcropolisHallwayRoomLightingTable[1];

/// Four hallway warp endpoints, indexed by the 1-based warp ID minus one.
///
/// Slots 1..4 connect to the fountain, security room and the two cafeteria
/// exits. Player and companion share each room-space placement; yaw uses
/// 4096 per turn. Arrival views are 4, 4, 3 and 2, with map-marker nibbles
/// 497..500 and no blocked sound or departure fade. Retain the hallway
/// overlay while departure queries or arrival placement copy these records.
extern DirectionWarpEntry gAcropolisHallwayWarpEntries[4];

/// Hallway mapped views 1..5, selecting decoded backgrounds without source sprites.
///
/// Indexed by mapped view - 1. Each view keeps its own zero-count batch and
/// end marker, with the batch address retained in sources.empty and no clip
/// list. Keep the hallway overlay loaded while these descriptors are borrowed.
extern SpriteView gAcropolisHallwaySpriteViews[5];

/// Hallway cameras for mapped views 1..5, indexed by mapped view minus one.
///
/// Rotations use 4096 per unit; translations are negated world-space camera
/// origins in game units. Projection distances are 911 pixels for view 1 and
/// 207 for views 2..5. Queued camera tasks borrow these records until application;
/// the hallway overlay must remain loaded through that copy.
extern ViewCamera gAcropolisHallwayViewCameras[5];

/// Hallway surface responses for collision classes 0..7, with no terminator.
///
/// Class 1 passes probes and suppresses weapon impacts; class 4 blocks probes
/// and permits impacts. Both supply footstep cues. Other classes block, permit
/// impacts and have no footstep sounds. All classes apply pushback. The stage
/// map borrows the directory and its records while the hallway overlay is loaded.
extern WorldCollisionSurfaceProperties* gAcropolisHallwaySurfacePropertiesByClass[8];

/// Inert task callback for the hallway's effect-bank-6 slot 0x84.
///
/// Leaves the live task and its coordinate body untouched, including its state
/// and lifetime. Keep the hallway overlay loaded while this callback can run.
void acropolisHallwayEffectControlTask84(Task* unusedTask);

/// Runs the hallway's room-message receiver.
///
/// State 0 registers the receiver; state 1 idles with messages available;
/// state 2 releases the task.
/// Start with a live bodyless task in state 0. The state must remain in 0..2;
/// dispatch performs no bounds check. Keep the room overlay and gameplay
/// resources loaded through the selected handler, which may release the task.
void acropolisHallwayRoomTask(Task* task);

/// Shows the hallway Blue Key pickup only in mapped view 5 until collected.
///
/// Requires the placement spawner's live `Enemy` in `spawnArg2.pointer` and a
/// TMD body. Initializes the flagged draw pass and zero ordering-table offset
/// on state 0, then advances the state. Object state 2 always hides the model.
/// Borrows the enemy/model without changing their lifetime or freeing buffers;
/// keep the hallway overlay loaded while this map callback can execute.
void acropolisHallwayBlueKeyPickupTask(Task* task);

#endif // INCLUDE_ROOMS_ACROPOLIS_HALLWAY_H
