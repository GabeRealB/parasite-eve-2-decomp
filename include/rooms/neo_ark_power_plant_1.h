#ifndef INCLUDE_ROOMS_NEO_ARK_POWER_PLANT_1_H
#define INCLUDE_ROOMS_NEO_ARK_POWER_PLANT_1_H

#include "types.h"

#include "gameplay/area.h"
#include "gameplay/direction.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/task_types.h"

extern AreaVariant D_neo_ark_power_plant_1_80181AF8[13];

/// Room 1's collision grid, view boundaries, interactions and sight-blocking quad.
///
/// The Neo Ark map borrows this one-room directory while the room is loaded.
/// Setup links and mutates its triggers and occluder; they must remain loaded until unlinked.
extern WorldCollisionRoomResources gNeoArkPowerPlant1CollisionRooms[1];

/// Power Plant 1's room-1 point-light collection and default ambient-light selection.
///
/// The Neo Ark map borrows this room-indexed table while the room is loaded.
/// Its lights remain writable for coordinate composition and shading queries.
extern WorldCoordRoomLighting gNeoArkPowerPlant1RoomLighting[1];

/// Room-1 mapping from logical views to camera, image and sprite indices.
///
/// Borrows `gViewIdentityMap`: logical views 1..9 select the same mapped indices.
/// The room owns this directory; the mapped bytes belong to loaded gameplay.
extern u8* gNeoArkPowerPlant1ViewMaps[1];

/// Room 1's nine logical views, bounding reverse searches of its identity map.
///
/// One signed halfword count, indexed by room minus one. The map borrows it
/// while this overlay is loaded; there is no sentinel entry.
extern ViewCount gNeoArkPowerPlant1ViewCounts[1];

/// Power Plant 1's sole arrival/departure endpoint, at room view 2.
///
/// Both actors arrive at the same room position facing yaw 3072 (4096 per turn).
/// Area scripts 2 and 1 sound on arrival and departure; blocked departures are
/// silent. Later arrivals set the Shrine map marker. Borrowed while the room is loaded.
extern DirectionWarpEntry gNeoArkPowerPlant1WarpEntries[1];

/// Cameras for Power Plant 1's mapped views 1..9, indexed by view minus one.
///
/// Transforms use `ONE`-scaled rotation and game-coordinate translation;
/// projection distance is in pixels. Storage is borrowed while the room is loaded.
extern ViewCamera gNeoArkPowerPlant1ViewCameras[9];

/// Sprite scenery for Power Plant 1's mapped views 1..9, indexed by view minus one.
///
/// Every view retains its decoded image strips; views 1 and 9 have no source
/// sprites. All clip lists are NULL. Sources and batches live in this room
/// overlay; Life Support visibility changes in batch 1 of views 6 and 7.
extern SpriteView gNeoArkPowerPlant1SpriteViews[9];

/// Neo Ark power plant 1 collision and footstep policies for surface classes 0..7.
///
/// Class 3 passes probes and ignores weapon impacts; every other class blocks
/// probes and allows impacts. All apply pushback. Classes 4 and 5 have separate
/// walking/running/scripted-jump cues; others are silent. Setup reads all eight
/// entries. Borrow the directory, records and cues while this overlay is loaded.
extern WorldCollisionSurfaceProperties* gNeoArkPowerPlant1SurfacePropertiesByClass[8];

/// Switches the first power plant's Life Support scenery batches off or on.
///
/// Uses the low byte of hidden: 0 shows batch 1 in views 6 and 7, 1 hides it,
/// and other byte values leave both intact. Requires the current area's sprite
/// table in its first variant; the generator hides these while its part is live
/// and restores them at teardown. Borrows room-owned sprite storage for the call.
void neoArkPowerPlant1SetLifeSupportSpritesHidden(s32 hidden);

/// Draws the first power plant's view-specific light glows and generator flashes.
///
/// Mapped views 2..8 select fixed world-space emitters; other low-byte views draw
/// nothing. In views 6 and 7 destruction of the generator's support part replaces
/// its glow with a flash on a random one-in-eight running tick until the plant clears.
/// Requires live room-effect state, view matrices, scratch stack and frame packet resources.
/// The task is unused; spawned effects borrow a point owned by this room overlay.
void neoArkPowerPlant1DrawLightGlowsTask(Task* unusedTask);

/// Runs the Neo Ark Power Plant 1 room controller for one tick.
///
/// `task` must be live with state 0..2 and this room overlay loaded.
/// Initialize room messages and battle state, update events and sound, then kill.
/// State 0 registers the borrowed task in `GAME_TASK_SLOT_ROOM`; state 2
/// requests teardown. The controller allocates no work or body of its own.
void neoArkPowerPlant1RoomTask(Task* task);

#endif // INCLUDE_ROOMS_NEO_ARK_POWER_PLANT_1_H
