#ifndef INCLUDE_ROOMS_DRYFIELD_DRIVEWAY_H
#define INCLUDE_ROOMS_DRYFIELD_DRIVEWAY_H

#include "types.h"

#include "gameplay/area.h"
#include "gameplay/direction.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/task_types.h"

extern AreaVariant D_dryfield_driveway_8017EDD4[11];

// dryfield_driveway
extern WorldCollisionRoomResources D_dryfield_driveway_8017E784[];

/// Logical-view maps for driveway room selectors 1 and 2.
///
/// Indexed by room minus one, then logical view minus one (views 1..7).
/// Room 1 uses the identity map; room 2 uses the after-rope map, sending
/// logical view 4 to resource slot 7 and logical view 7 to slot 4.
/// Results are 1-based camera/image/sprite indices. Borrow while this overlay is loaded.
extern u8* gDryfieldDrivewayRoomViewMaps[2];

/// Lighting choices for driveway room selectors 1 and 2.
///
/// Indexed by room minus one. Both borrow the same twelve point lights and
/// use the default ambient minima. Records and lights stay live with this overlay.
extern WorldCoordRoomLighting gDryfieldDrivewayRoomLighting[2];

extern ViewCount D_dryfield_driveway_8017E7C4[];

extern DirectionWarpEntry D_dryfield_driveway_8017E7C8[];

/// Seven driveway cameras, indexed by mapped view minus one.
///
/// Slots 4 and 7 share a camera pose for the before/after-rope backgrounds.
/// Borrowed records must remain loaded until a queued camera update applies them.
extern ViewCamera gDryfieldDrivewayViewCameras[7];

/// Sprite layers and clipping lists for the seven mapped driveway views.
///
/// Indexed by mapped view minus one, independently of the logical view.
/// Slots 1 and 6 contain no source sprites; slots 4 and 7 supply the
/// before/after-rope layers. Descriptors and backing lists stay live with this overlay.
extern SpriteView gDryfieldDrivewaySpriteViews[7];

/// Driveway collision policies indexed by surface class 0..7.
///
/// Class 1 passes probes and disables weapon impacts; class 2 supplies footstep
/// sounds. Other classes block probes and allow impacts without footstep sounds.
/// All classes apply pushback. Borrow these records while this overlay is loaded.
extern WorldCollisionSurfaceProperties* gDryfieldDrivewaySurfacePropertiesByClass[8];

/// Enables the current view's ambient effects on every room-effect update.
///
/// Requires the initialized room-effect controller at `gRoomEffectState`.
/// The task argument is ignored; the callback neither advances nor ends it.
void dryfieldDrivewayEnableAmbientEffectsTask(Task* unusedTask);

/// Runs the driveway room task's initialization, idle or teardown state.
///
/// Requires a live task with state 0 initialize, 1 idle, or 2 destroy. State 0
/// installs the room messages, registers `GAME_TASK_SLOT_ROOM` and enters 1.
/// Dispatch uses a by-value copy of the three handlers; no work is allocated.
void dryfieldDrivewayRoomTask(Task* task);

#endif // INCLUDE_ROOMS_DRYFIELD_DRIVEWAY_H
