#ifndef INCLUDE_ROOMS_DRYFIELD_SOUVENIR_SHOP_H
#define INCLUDE_ROOMS_DRYFIELD_SOUVENIR_SHOP_H

#include "types.h"

#include "gameplay/area.h"
#include "gameplay/direction.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/task_types.h"

extern AreaVariant D_dryfield_souvenir_shop_8017F598[13];

/// Collision resources for room 1: one grid, two view boundaries and twelve action triggers.
///
/// Indexed by room minus one. The map overlay borrows this descriptor; the
/// grid and trigger storage stay mutable and live while this room is loaded.
extern WorldCollisionRoomResources gDryfieldSouvenirShopRoomCollisionResources[1];

/// Room-1 logical-view map, borrowing the shared gameplay identity map.
///
/// Indexed by room minus one, then logical view minus one. Views 1..3 map
/// directly to the same camera, image and sprite indices; the map has no sentinel.
extern u8* gDryfieldSouvenirShopRoomViewMaps[1];

/// Room-1 reverse-lookup count: three logical views in the identity map.
///
/// Indexed by room minus one. This element count limits map searches;
/// it does not describe the full capacity of the shared identity map.
extern ViewCount gDryfieldSouvenirShopRoomViewCounts[1];

/// Room-1 lighting descriptor, selecting six point lights and default ambient minima.
///
/// Indexed by room minus one. The map overlay borrows the record and its
/// mutable light collection while the souvenir-shop overlay remains loaded.
extern WorldCoordRoomLighting gDryfieldSouvenirShopRoomLighting[1];

/// The room's single door endpoint, arriving in logical view 2.
///
/// Indexed by warp ID minus one. Stores player and companion placements in
/// room units and yaw in 4096 units per turn, arrival/departure sound scripts,
/// no blocked sound or departure flags, and map-marker nibble 469.
/// Borrowed by the stage warp table while this room overlay remains loaded.
extern DirectionWarpEntry gDryfieldSouvenirShopWarpEntries[1];

/// Four stored cameras: mapped views 1..3 and one retained extra record.
///
/// Indexed by mapped view minus one; the current identity map and view count
/// select the first three. Rotations use 12 fractional bits, origins use
/// signed world units and all projection distances are 230 pixels.
/// The stage camera table borrows this array while the room remains loaded.
extern ViewCamera gDryfieldSouvenirShopViewCameras[4];

/// Three mapped-view sprite descriptors, retaining decoded background strips in every view.
///
/// Indexed by mapped view minus one. View 1 selects no sprites; views 2 and
/// 3 select 45 and 53 source rectangles. Batch lists are mutable and end at
/// SPRITE_BATCH_END; there are no draw-area lists. The stage sprite table
/// borrows these records and their arrays while the room remains loaded.
extern SpriteView gDryfieldSouvenirShopSpriteViews[3];

/// Collision and footstep policies for the room's eight surface classes.
///
/// Indexed directly by class 0..7, with no terminator. Classes 0 and 5..7
/// share the silent blocking record; 1 and 2 supply different footstep pairs;
/// 3 passes probes and ignores impacts; 4 is separately stored and silent.
/// All classes permit pushback. Records are borrowed while the room is loaded.
extern WorldCollisionSurfaceProperties* gDryfieldSouvenirShopSurfacePropertiesByClass[8];

/// Draws the Souvenir Shop's two pulsing grey light prisms each frame.
///
/// Gameplay effect slot 0xC6 requires a live `TASK_BODY_COORD` task whose
/// coordinate has a composed world matrix. The two eight-corner blocks are in
/// whole units of that coordinate's local space. Borrows the coordinate for
/// the call and queues ten additive Gouraud quads with their blend commands.
/// The view matrix, scratch stack, ordering table and packet arena must be
/// ready; queued packets remain live until the GPU finishes the frame.
void dryfieldSouvenirShopLightPrismsTask(Task* task);

/// Runs the souvenir shop's room-message task for one update.
///
/// Requires a live task: state 0 initializes, 1 idles and 2 releases the task.
/// The state index is unchecked. Keep this room overlay loaded while the task
/// or its registered message handlers can run; the final state ends its lifetime.
void dryfieldSouvenirShopRoomTask(Task* task);

#endif // INCLUDE_ROOMS_DRYFIELD_SOUVENIR_SHOP_H
