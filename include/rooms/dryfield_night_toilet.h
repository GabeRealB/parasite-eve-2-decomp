#ifndef INCLUDE_ROOMS_DRYFIELD_NIGHT_TOILET_H
#define INCLUDE_ROOMS_DRYFIELD_NIGHT_TOILET_H

#include "types.h"

#include "gameplay/area.h"
#include "gameplay/direction.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/task_types.h"

extern AreaVariant D_dryfield_night_toilet_8017F354[12];

/// Room 1's model-light collection, using the default ambient minima.
///
/// Indexed by room - 1. The collection and its mutable light transforms are
/// borrowed from this overlay and remain valid only while it is loaded.
extern WorldCoordRoomLighting gDryfieldNightToiletRoomLighting[1];

/// Room 1's collision grid, camera boundaries, interactions and sight blocker.
///
/// Indexed by room - 1. The stage map borrows this directory and its mutable
/// resources while the night toilet overlay is loaded; trigger and occluder
/// arrays include their final LAST records.
extern WorldCollisionRoomResources gDryfieldNightToiletRoomCollisionResources[1];

/// Room 1's logical-view map directory, borrowing gameplay's identity mapping.
///
/// Indexed by room - 1, then logical view - 1. Views 1..9 select the same
/// 1-based camera, image and sprite indices; the shared map's capacity is
/// independently `VIEW_IDENTITY_MAP_LENGTH`. Keep both overlays loaded.
extern u8* gDryfieldNightToiletViewMaps[1];

/// Room 1's nine logical views, limiting reverse searches of its view map.
///
/// Indexed by room - 1; this is a signed element count with no terminator.
/// The stage map borrows the array while this room overlay is loaded.
extern ViewCount gDryfieldNightToiletViewCounts[1];

/// The night toilet's single arrival/departure endpoint, selected by warp ID 1.
///
/// Indexed by warp ID - 1, without a terminator. Player and companion share
/// world placement (-1660, 0, 1653), yaw 2048 of 4096, and initial view 2.
/// Uses night-toilet sound entries 2 on arrival and 1 on departure, with map
/// marker 480. Borrowed through the stage map while this overlay is loaded.
extern DirectionWarpEntry gDryfieldNightToiletWarpEntries[1];

/// Camera transforms and projection distances for the night toilet's nine views.
///
/// Indexed by mapped view - 1. Rotation has 12 fractional bits, translation
/// is the negated origin in world units, and projection distance is in pixels.
/// The stage map borrows these records while this room overlay is loaded.
extern ViewCamera gDryfieldNightToiletViewCameras[9];

/// Sprite sources, drawing ranges and clipping for the night toilet's nine views.
///
/// Indexed by mapped view - 1. Views 2..5 select source sprites; views 1 and
/// 6..9 have empty batch lists. All views retain decoded background strips.
/// Only view 5 supplies a clipping list. The records and mutable source arrays
/// are borrowed while this room overlay is loaded.
extern SpriteView gDryfieldNightToiletSpriteViews[9];

/// Collision and footstep policies for the night toilet's surface classes 0..7.
///
/// Class 1 blocks probes and permits weapon impacts; class 2 passes probes
/// and suppresses weapon impacts. Both share footstep cues. Classes 0 and
/// 3..7 block probes, permit weapon impacts and have no footstep cues.
/// Every class applies pushback. The stage map borrows this directory, its
/// records and sounds while this room overlay is loaded.
extern WorldCollisionSurfaceProperties* gDryfieldNightToiletSurfacePropertiesByClass[8];

/// Draws the night toilet's flickering light flare selected by the camera view.
///
/// Effect-bank 6 callback for slot 0x10C. View 4 selects the negative-Z centre;
/// views 5 and 9 select the positive-Z centre, and every other view draws nothing. The task
/// is unused. Texture column 1 uses a perspective radius scale of 512, giving
/// a pixel half-extent of 512 * 39 / (camera Z / 4).
///
/// Requires the room overlay and flare texture loaded, a composed view matrix,
/// scratch space and a current GPU packet arena and ordering table. A selected
/// flare reserves one packet even when rejected by near clipping; queued
/// geometry borrows the frame arena until GPU completion. Retains no task data.
void dryfieldNightToiletDrawFlareTask(Task* unusedTask);

/// Runs the night toilet's room initialization, message wait and teardown.
///
/// State 0 installs the room handlers and starts the first-visit event on
/// variant 1; state 1 waits for messages; state 2 kills the task.
/// Requires a live task with state in 0..2 and the room overlay loaded.
/// The initialized task must stay live while its published room slot is used.
void dryfieldNightToiletRoomTask(Task* task);

#endif // INCLUDE_ROOMS_DRYFIELD_NIGHT_TOILET_H
