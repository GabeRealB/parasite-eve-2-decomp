#ifndef INCLUDE_ROOMS_DRYFIELD_NIGHT_BREEZEWAY_H
#define INCLUDE_ROOMS_DRYFIELD_NIGHT_BREEZEWAY_H

#include "types.h"

#include "gameplay/area.h"
#include "gameplay/direction.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/task_types.h"

extern AreaVariant D_dryfield_night_breezeway_801803E4[22];

/// Lighting directory for the breezeway's single room, indexed by room minus one.
///
/// Borrows four directional and seven point lights, with default ambient minima.
/// The record and writable light transforms stay live while this overlay is loaded.
extern WorldCoordRoomLighting gDryfieldNightBreezewayRoomLighting[1];

extern WorldCollisionRoomResources D_dryfield_night_breezeway_8017E6EC[];

/// Logical-view map directory for the single breezeway room, indexed by room minus one.
///
/// Borrows `gViewIdentityMap`; views 1..6 select the same 1-based camera/image/sprite
/// indices. The directory lives with this overlay and the byte map with gameplay.
extern u8* gDryfieldNightBreezewayViewMaps[1];

/// Logical-view reverse-search count for the single breezeway room: six entries.
extern ViewCount gDryfieldNightBreezewayViewCounts[1];

extern DirectionWarpEntry D_dryfield_night_breezeway_8017E704[];

/// Six cameras selected by mapped view minus one while the breezeway overlay is loaded.
///
/// Rotations use 4096 per unit; translations are negated world-space origins and
/// projection distances are pixels. Views 1..6 map directly to these records.
extern ViewCamera gDryfieldNightBreezewayViewCameras[6];

/// Six sprite-view descriptors selected by mapped view minus one.
///
/// Views 2, 3 and 6 borrow 96, 46 and 60 sources; views 1, 4 and 5 are empty.
/// Every list preserves image strips and has no clipping-area commands. Sources
/// and terminated batch lists remain writable and live with the room overlay.
extern SpriteView gDryfieldNightBreezewaySpriteViews[6];

/// Collision policies for room-local surface classes 0..7.
///
/// Class 1 passes probes and suppresses weapon impacts; class 2 blocks probes,
/// permits impacts and supplies footsteps. Classes 0 and 3..7 share blocking,
/// impact-enabled properties without footsteps. All classes apply pushback.
/// The records and footstep cues are borrowed while this overlay is loaded.
extern WorldCollisionSurfaceProperties* gDryfieldNightBreezewaySurfacePropertiesByClass[8];

/// Draws the night breezeway's view-dependent light glows.
///
/// Enables room-view effects on every tick. View 2 queues a textured flare and
/// a shaft; view 3 queues an extra shaft before the pulsing red star and shaft
/// shared with view 4. Other views queue nothing. `task` is unused.
///
/// Requires the room overlay, session and room-effect state to remain live,
/// a composed view matrix, an initialized scratch stack and room in the current
/// frame's packet arena and ordering table. Queued packets live until GPU completion.
void dryfieldNightBreezewayDrawGlowsTask(Task* task);

/// Dispatches the night breezeway's room receiver lifecycle.
///
/// `state` must be 0 (register messages), 1 (idle), or 2 (kill). Start in state
/// 0; initialization publishes the task in `GAME_TASK_SLOT_ROOM` and enters
/// state 1. The room's borrowed message and state tables must remain loaded
/// through teardown. Dispatch copies the complete three-entry state table.
void dryfieldNightBreezewayRoomTask(Task* task);

#endif // INCLUDE_ROOMS_DRYFIELD_NIGHT_BREEZEWAY_H
