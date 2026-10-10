#ifndef INCLUDE_ROOMS_MINE_MESA_H
#define INCLUDE_ROOMS_MINE_MESA_H

#include "types.h"

#include "gameplay/area.h"
#include "gameplay/direction.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/task_types.h"

/// Actor layouts for Mine Mesa (stage 4, area 1), indexed by placement variant.
///
/// Slots 1, 2, 3, 4, 7 and 11 contain sentinel-terminated placement/resource
/// pairs; slots 0, 5, 6 and 8..10 are empty. Variant 1 is the default layout.
/// Gameplay borrows the directory and lists while this room overlay is loaded;
/// spawned actors may retain placement pointers until they are released.
extern AreaVariant gMineMesaAreaVariants[12];

/// Collision resources for Mine Mesa's single room, indexed by room minus one.
///
/// The stage-4 area-1 directory borrows the live grid, eight view boundaries,
/// nineteen action triggers and two sight occluders. Lists are mutable and
/// sentinel-terminated; all resources stay loaded until deactivated/unlinked.
extern WorldCollisionRoomResources gMineMesaRoomCollisionResources[1];

/// Logical-view maps for Mine Mesa's single room.
///
/// Indexed by room minus one (room 1 only); borrows gameplay's identity map.
/// Logical views 1..11 map to the same camera, image and sprite indices. The
/// directory belongs to this room overlay and is valid only while it is loaded.
extern u8* gMineMesaRoomViewMaps[1];

/// Lighting resources for Mine Mesa's single room.
///
/// Indexed by room minus one (room 1 only). Borrows eight mutable point lights,
/// one mutable cone light and eleven per-view ambient entries with a count header.
/// The records and their light arrays stay valid only while this overlay is loaded.
extern WorldCoordRoomLighting gMineMesaRoomLighting[1];

/// Logical-view count for Mine Mesa's single room.
///
/// Indexed by room minus one (room 1 only). The identity map has eleven valid
/// views; the stage map borrows this signed count while the room is loaded.
extern ViewCount gMineMesaRoomViewCounts[1];

/// Two Mine Mesa warp endpoints, indexed by the area's 1-based warp ID minus one.
///
/// Entries use views 2 and 5 respectively; player and companion share each
/// endpoint's world placement. Warp 2 selects area-bank arrival/departure cues.
/// The Shelter map borrows the complete entries while this room stays loaded.
extern DirectionWarpEntry gMineMesaWarpEntries[2];

/// Cameras for Mine Mesa's eleven mapped views, indexed by mapped view minus one.
///
/// Rotation uses Q12 coefficients, translation is the negated world origin,
/// and projection distance is in pixels. The stage map borrows these records;
/// a queued camera application requires the room to remain loaded until dispatch.
extern ViewCamera gMineMesaViewCameras[11];

/// Sprite resources for Mine Mesa's eleven mapped views, indexed by view minus one.
///
/// Views 1, 7, 10 and 11 have background batches only. Other views borrow mutable
/// sprite sources; each batch list ends at `SPRITE_BATCH_END`. The stage map
/// borrows this directory and its lists while the room overlay is loaded.
extern SpriteView gMineMesaSpriteViews[11];

/// Mine Mesa's collision and footstep records, indexed by surface class 0..7.
///
/// Classes 0 and 5..7 share a silent blocking record; class 1 is separately
/// stored with the same policy. Class 2 blocks probes and provides footstep cues;
/// class 3 passes probes, ignores weapon impacts and provides footstep cues; class 4
/// passes probes without cues. All apply pushback. The directory and borrowed
/// records remain valid only while this room overlay is loaded.
extern WorldCollisionSurfaceProperties* gMineMesaSurfacePropertiesByClass[8];

/// Rebuilds the room grid's four reserved vertical wall faces from their base edges.
///
/// Requires Mine Mesa's loaded wall table and active collision grid, with
/// normals/faces through index 6 and vertices through index 27 writable.
/// `height` is in game-coordinate units, subtracted from the base Y of upper
/// corners because grid Y points down; vertex arithmetic narrows to s16.
/// Fills faces 3..6 with surface class 3 and normalized horizontal normals.
/// Keeps the existing cell lists; call only with the Mine Mesa room active.
void mineMesaBuildWalls(s32 height);

/// Charges a pink flash, tints the screen at its peak, then fades it as a star.
///
/// Requires a counted effect with a coordinate body and owned `EffectWork` in
/// `spawnArg2.pointer`. `spawnArg1.value` is a positive charge duration in
/// ticks, consumed as a countdown. Room effect control pauses it when nonzero
/// and cancels it at four or above; state 3 also releases the work and task.
void mineMesaRoomVisualEffectsFlashTask(Task* task);

/// Draws a blue beam trail between two offsets on the effect's parent coordinate.
///
/// Requires a counted effect's coordinate body and owned `EffectWork` in
/// `spawnArg2.pointer`, with its borrowed parent live throughout the effect.
/// Stores eight world-space snapshots per endpoint in owned `task->work`;
/// task teardown frees that history. `spawnArg1.value` is the active-tick
/// release age (2..32767 before age wraps, 0 indefinite). Initialization uses
/// age 1 without testing expiry. Control values below 2 update the trail;
/// values of 2 or above freeze it. History-allocation failure retries next tick.
void mineMesaRoomVisualEffectsTwinTrailTask(Task* task);

/// Runs an impact flash followed by smoke puffs or orange rings and sparks.
///
/// Requires a counted effect with a coordinate body and zero-aged owned
/// `EffectWork` in `spawnArg2.pointer`. Nonzero `spawnArg1.value` selects
/// smoke; zero selects rings and bouncing sparks. Release begins at active
/// age seven and frees work on the next active tick. Room effect control
/// pauses at nonzero values below four and cancels at four or above.
/// Spawned child effects run independently; their parent coordinate must live.
void mineMesaRoomVisualEffectsSparkBurstTask(Task* task);

/// Installs Mine Mesa's actor-effect IDs and draws the mapped view's fixed flares.
///
/// Requires a live room-effect controller and current camera/frame state.
/// State 0 installs the flash, twin-trail and spark-burst IDs and enables
/// view effects. Views 2, 4, 5, 6 and 8..11 draw fixed world-point flares;
/// other views draw none. Texture columns 0/1 use radius scales 768/512,
/// projected as scale * 39 / (camera Z / 4) pixels, with nonzero depth required.
void mineMesaDrawViewFlaresTask(Task* task);

/// Initializes and updates the loaded Mine Mesa room's scripted events.
///
/// Exported to the Shelter map's stage-4 area-1 task descriptor. Requires the
/// mesa room resources and active collision grid to remain loaded. `state`
/// must be 0..2: room setup, per-frame event checks, release. Start a bodyless
/// task at state 0; initialization installs its messages and room slot, sets
/// up first-arrival presentation and rebuilds collision walls. Owns no work.
void mineMesaRoomTask(Task* task);

#endif // INCLUDE_ROOMS_MINE_MESA_H
