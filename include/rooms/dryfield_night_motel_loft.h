#ifndef INCLUDE_ROOMS_DRYFIELD_NIGHT_MOTEL_LOFT_H
#define INCLUDE_ROOMS_DRYFIELD_NIGHT_MOTEL_LOFT_H

#include "types.h"

#include "gameplay/area.h"
#include "gameplay/direction.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/task_types.h"
#include "main/tmd_types.h"

/// Placement/resource pairs selected by the nighttime loft's saved variant byte.
///
/// Slots 1, 3, 4 and 11 are populated; 0, 2, 5..10 and 12 are absent.
/// Selectors must be within 0..12. Placement lists belong to the Dryfield map;
/// resource recipes belong to this room and borrow the selected actor overlays'
/// task descriptors. Keep each owning overlay loaded while its data is used.
extern AreaVariant gDryfieldNightMotelLoftAreaVariants[13];

extern TmdSource gDryfieldNightMotelLoftActor135400Model071AC;

/// Lighting descriptors for room selectors 1 and 2, sharing one point-light collection.
///
/// Indexed by room minus one. Both use the default ambient minima; the room
/// overlay owns the records and borrowed lights, which remain live while loaded.
extern WorldCoordRoomLighting gDryfieldNightMotelLoftRoomLighting[2];

/// Collision resources for room selectors 1 and 2, indexed by room minus one.
///
/// Both borrow the same mutable mesh, view boundaries and action triggers;
/// neither has sight occluders. The room overlay must remain loaded until
/// these resources are unlinked and the active grid is replaced or cleared.
extern WorldCollisionRoomResources gDryfieldNightMotelLoftRoomCollisionResources[2];

/// Logical-to-mapped view byte arrays for the loft's two room selectors.
///
/// Indexed by room minus one, then logical view minus one (logical views 1..14).
/// Room 1 borrows the gameplay identity map; room 2 substitutes mapped views
/// 9, 10 and 11 at logical slots 2, 3 and 7 after the encounter switches rooms.
/// Mapped camera, image and sprite bounds are independent. Room-owned pointers
/// require this overlay to remain loaded.
extern u8* gDryfieldNightMotelLoftViewMaps[2];

/// Signed element counts for reverse-view lookup in room selectors 1 and 2.
///
/// Indexed by room minus one; each searches fourteen logical-map bytes.
/// These counts do not describe map capacity or camera/image/sprite extents.
/// The stage map borrows the array while this room overlay is loaded.
extern ViewCount gDryfieldNightMotelLoftViewCounts[2];

/// The loft's balcony-door warp endpoint, indexed by warp ID 1 minus one.
///
/// Player and companion arrive at (3953, 0, -2156) world units with yaw zero,
/// in logical view 2. Arrival/departure use this area's door sounds; there is
/// no blocked sound or departure fade. Map-marker nibble 466 tracks the door.
/// The stage directory borrows this complete record while the overlay is loaded.
extern DirectionWarpEntry gDryfieldNightMotelLoftWarpEntries[1];

/// Eleven cameras selected by mapped view index 1..11, stored at index minus one.
///
/// Rotations use ONE (4096) for unity; translations are negated world origins
/// in integer units and screen distances are pixels. Views 2/9, 3/10 and 7/11
/// share camera transforms. The fourteen-entry logical-view search does not
/// extend this camera array. The stage map borrows it while the overlay is loaded.
extern ViewCamera gDryfieldNightMotelLoftViewCameras[11];

/// Dryfield night motel loft's sprite resources, indexed by mapped view minus one.
///
/// Views 1, 2, 9 and 12..14 have no sources. Slots 9/10/11 reuse the
/// source/batch bindings of 2/3/7; both logical rooms use this directory.
/// Every slot retains decoded image strips and has no clipping list. Empty
/// source slots retain batch addresses without supplying SpriteSource elements.
/// Descriptors and mutable source/batch lists stay borrowed while the owning
/// room overlay is loaded; selectors must be nonzero and within this array.
extern SpriteView gDryfieldNightMotelLoftSpriteViews[14];

/// Nighttime motel loft collision and footstep policies for surface classes 0..7.
///
/// Every class blocks probes, allows weapon impacts and applies pushback.
/// Class 1 supplies walking/running/scripted-jump cue bases; all others share
/// silent properties. Setup reads all eight entries. The directory, records
/// and sound cues remain borrowed only while this room overlay is loaded.
extern WorldCollisionSurfaceProperties* gDryfieldNightMotelLoftSurfacePropertiesByClass[8];

/// Updates a tumbling grey shard that bounces on Y=0 and fades after its first bounce.
///
/// Handles `EFFECT_NIGHT_MOTEL_LOFT_FALLING_SHARD`. The effect spawner supplies
/// an owned `EffectWork` in `spawnArg2.pointer` and a coordinate body already
/// positioned at the spawn offset. The low twelve bits of `spawnArg1.value`
/// give the radius in coordinate units; the room spawns radii 16..79.
/// Positive local Y falls toward the floor. Each crossing rolls back the
/// movement and halves velocity, reflecting Y; subsequent bounces continue
/// during the fade. Drawing uses the coordinate's existing cached transform.
/// Control values 0 and 1 advance the task, 2 and 3 suppress update and draw,
/// and values >=4 release both the counted effect work and the task.
void dryfieldNightMotelLoftFallingShardTask(Task* task);

/// Draws the loft's light flares and emits two view-triggered shard bursts.
///
/// Gameplay room-effect task 0x11B, requiring the loft overlay and frame packet
/// arena live. Mapped views select six fixed flare anchors. Starting at state
/// 0, view 8 emits 48 shards and advances to 1; view 3 or 10 then emits 32 and
/// advances to 2. State 2 keeps drawing flares without further bursts.
/// Each attempt consumes four random draws, even if effect allocation fails;
/// the room's shard-spawn member supplies shared writable
/// spawn storage. Positions use the effect spawner's world-matrix input units;
/// radii are 16..79 coordinate units. The task is released by its external owner.
void dryfieldNightMotelLoftFlaresAndShardsTask(Task* task);

/// Dispatches the nighttime loft's room controller through states 0..2.
///
/// The Dryfield map spawns it for stage 3, area 31 with no body. State 0
/// publishes the room message table and controller slot and restores encounter
/// state; state 1 updates pickup collision and gates the Stalker event; state
/// 2 kills the task. The state must index these three handlers. Room tables,
/// session, live save and gameplay services must remain available while it runs.
void dryfieldNightMotelLoftRoomTask(Task* task);

#endif // INCLUDE_ROOMS_DRYFIELD_NIGHT_MOTEL_LOFT_H
