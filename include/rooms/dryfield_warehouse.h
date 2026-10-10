#ifndef INCLUDE_ROOMS_DRYFIELD_WAREHOUSE_H
#define INCLUDE_ROOMS_DRYFIELD_WAREHOUSE_H

#include "types.h"

#include "gameplay/area.h"
#include "gameplay/direction.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/task_types.h"

extern AreaVariant D_dryfield_warehouse_80182100[13];

// dryfield_warehouse
extern WorldCollisionRoomResources D_dryfield_warehouse_8017FBBC[];

/// Logical-view maps for the warehouse's three room variants.
///
/// Indexed by room ID minus one: room 1 uses `gViewIdentityMap`, room 2
/// remaps logical views 3/4 to indices 6/7 after the Monkey Wrench event,
/// and room 3 remaps them to 9/8. Other logical views keep their indices.
/// Each room supports logical views 1..9; entries contain 1-based camera,
/// image and sprite indices. Gameplay borrows these maps while the room
/// overlay is loaded. The gameplay condition selecting room 3 is unproven.
extern u8* gDryfieldWarehouseRoomViewMaps[3];

/// Logical-view search lengths for warehouse rooms 1..3, each nine entries.
///
/// Indexed by room ID minus one. These signed halfword counts bound reverse
/// searches in `gDryfieldWarehouseRoomViewMaps`; they exclude each private
/// map's three trailing zero bytes. The stage map borrows this array while
/// the warehouse overlay is loaded.
extern ViewCount gDryfieldWarehouseRoomViewCounts[3];

extern WorldCoordRoomLighting D_dryfield_warehouse_8017FC18[];

extern DirectionWarpEntry D_dryfield_warehouse_8017FC30[];

/// Cameras for the warehouse's nine mapped view indices.
///
/// Indexed by mapped view minus one, independently of the logical room view.
/// Indices 6/9 reuse camera 3's transform and projection; indices 7/8 reuse
/// camera 4's. Rotations use 4096 per unit, translations use game coordinates,
/// and projection distances use pixels. Gameplay borrows these records while
/// the warehouse overlay is loaded.
extern ViewCamera gDryfieldWarehouseViewCameras[9];

extern SpriteView D_dryfield_warehouse_80181638[];

extern WorldCollisionSurfaceProperties* D_dryfield_warehouse_80182194[];

/// Draws the daytime warehouse's view-selected grey prism and circular light beams.
///
/// `task` must have a `TASK_BODY_COORD` body with a valid coordinate whose `workm`
/// already maps the room's light geometry to camera space; this callback borrows
/// the coordinate without composing or changing it. Requires current view
/// matrices, scratch storage, ordering table and packet arena.
///
/// Room-local views are 1..9, before camera/image remapping. View 2 draws the
/// first beam; views 2, 3, 6 and 9 draw the prism and second beam; views 2, 3, 4
/// and 6..9 draw the last two beams. Publishes `ROOM_EFFECT_VIEW_ENABLED` on
/// every update, including views with no lights, to permit view-gated effects.
void dryfieldWarehouseDrawGlowsTask(Task* task);

/// Runs the warehouse room's registration, idle or teardown state.
///
/// The stage map spawns this receiver for area 7. `task->state` must be 0..2:
/// registration installs the message table and starts ambience, idle retains
/// the receiver, and state 2 releases the task. Keep this overlay loaded while
/// it can receive messages. No work allocation or spawn payload is read here.
void dryfieldWarehouseRoomTask(Task* task);

#endif // INCLUDE_ROOMS_DRYFIELD_WAREHOUSE_H
