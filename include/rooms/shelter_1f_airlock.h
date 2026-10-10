#ifndef INCLUDE_ROOMS_SHELTER_1F_AIRLOCK_H
#define INCLUDE_ROOMS_SHELTER_1F_AIRLOCK_H

#include "types.h"

#include "gameplay/area.h"
#include "gameplay/direction.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/task_types.h"

extern AreaVariant D_shelter_1f_airlock_8017F758[12];

// shelter_1f_airlock
extern WorldCollisionRoomResources D_shelter_1f_airlock_8017E59C[];

/// Number of identity-mapped camera, image and sprite views in the airlock.
enum { SHELTER_1F_AIRLOCK_VIEW_COUNT = 5 };

/// Lighting for room 1 of Neo Ark area 5, with the default ambient minima.
///
/// Borrows the room's mutable point-light collection while this overlay is loaded.
extern WorldCoordRoomLighting gShelter1fAirlockRoomLighting[1];

/// Room 1's logical-view map, borrowing gameplay's `gViewIdentityMap`.
///
/// Views 1..5 select the same 1-based camera, image and sprite indices.
/// The one-pointer room directory stays valid while this overlay is loaded.
extern u8* gShelter1fAirlockViewMaps[1];

/// Room 1's five logical views, limiting reverse searches of its view map.
extern ViewCount gShelter1fAirlockViewCounts[1];

extern DirectionWarpEntry D_shelter_1f_airlock_8017E5BC[];

/// Cameras selected by the airlock's mapped 1-based view index minus one.
///
/// Each record holds world-to-camera rotation, the negated world origin and
/// projection distance in pixels. Borrow only while this overlay is loaded.
extern ViewCamera gShelter1fAirlockViewCameras[SHELTER_1F_AIRLOCK_VIEW_COUNT];

/// Sprite sources, batches and clipping lists for mapped airlock views 1..5.
///
/// Views 1 and 2 have no source sprites; views 3..5 select 24, 26 and 36.
/// All first batches have zero count, retaining the decoded image background.
/// The descriptors and their mutable lists stay borrowed while this overlay is loaded.
extern SpriteView gShelter1fAirlockSpriteViews[SHELTER_1F_AIRLOCK_VIEW_COUNT];

/// Collision policies indexed by room surface class 0..7, without a terminator.
///
/// Every class blocks probes, enables weapon impacts and applies pushback.
/// Class 1 is the floor with footstep cues; the others have no footstep table.
/// Gameplay reads all eight entries when caching pushback flags. Records and
/// sounds stay borrowed while the airlock overlay is loaded.
extern WorldCollisionSurfaceProperties* gShelter1fAirlockSurfaceProperties[8];

/// Runs the Airlock room controller for synchronous room messages.
///
/// Requires a live task with state 0 (register handlers), 1 (idle), or 2 (kill).
/// Keep the room and Neo Ark map overlays loaded while its task and borrowed
/// message table remain available.
void shelter1fAirlockRoomTask(Task* task);

/// Draws the airlock's flickering disc and capsule glows for the current mapped view.
///
/// Gameplay's room-effect task slot 0x14D calls this once per update. Views 3
/// and 4 draw grey discs and cyan capsules; view 5 draws a red disc. Other
/// views emit nothing. The task argument is unused, and the task remains live.
/// Requires this room overlay, composed view matrices, an initialized scratch
/// stack and space in the current frame's packet arena and depth ordering table.
/// Emits at most 80 Gouraud quads plus their additive blend-mode packets.
/// Packets remain borrowed from that arena until GPU completion.
void shelter1fAirlockDrawViewGlowsTask(Task* unusedTask);

#endif // INCLUDE_ROOMS_SHELTER_1F_AIRLOCK_H
