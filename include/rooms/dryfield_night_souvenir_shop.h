#ifndef INCLUDE_ROOMS_DRYFIELD_NIGHT_SOUVENIR_SHOP_H
#define INCLUDE_ROOMS_DRYFIELD_NIGHT_SOUVENIR_SHOP_H

#include "types.h"

#include "gameplay/area.h"
#include "gameplay/direction.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/task_types.h"

extern AreaVariant D_dryfield_night_souvenir_shop_8017F62C[12];

/// Room 1's six point lights and per-view ambient minima for the night shop.
///
/// The night Dryfield map publishes this array at area 6, indexed by room - 1.
/// Referenced lights stay writable and all room storage requires this overlay
/// to remain loaded while lighting queries or coordinate updates use it.
extern WorldCoordRoomLighting gDryfieldNightSouvenirShopRoomLighting[1];

extern WorldCollisionRoomResources D_dryfield_night_souvenir_shop_8017E0EC[];

/// Room 1's logical-view map, borrowing gameplay's unsigned 1-based identity map.
///
/// Indexed by room - 1, then logical view - 1. The shop provides camera and
/// sprite slots 1..4; reverse lookup scans only the first three map entries.
/// The room directory requires this overlay to remain loaded.
extern u8* gDryfieldNightSouvenirShopViewMaps[1];

/// Room 1's signed logical-view search limit: three entries of its view map.
///
/// Indexed by room - 1. This reverse-lookup limit is independent of the four
/// camera and sprite records; it does not describe the identity map's capacity.
extern ViewCount gDryfieldNightSouvenirShopViewCounts[1];

extern DirectionWarpEntry D_dryfield_night_souvenir_shop_8017E104[];

/// Four camera records selected by the shop's mapped view index minus one.
///
/// Matrices use Q12 rotation and signed world-unit translation; all four
/// perspective distances are 230 pixels. A queued camera task borrows a record
/// until application, so keep the room overlay loaded through that update.
extern ViewCamera gDryfieldNightSouvenirShopViewCameras[4];

/// Four mapped sprite views, retaining decoded image strips in every slot.
///
/// Indexed by mapped view minus one: slots 2/3 add 45/53 source rectangles;
/// slots 1/4 retain empty batch-list addresses in their source slots. All lists
/// end with SPRITE_BATCH_END, and no view supplies a clipping list. Descriptors,
/// source arrays and visibility bytes remain writable in the loaded room overlay.
extern SpriteView gDryfieldNightSouvenirShopSpriteViews[4];

/// Collision and footstep records indexed by the shop's surface class, 0..7.
///
/// Classes 1/2 select distinct footstep sets, class 3 passes probes and ignores
/// weapon impacts, and all classes enable pushback. Classes 0 and 4..7 have no
/// footstep set; 0 and 5..7 share one record. The grid uses classes 0..2, while
/// setup reads all eight entries. Keep this room overlay loaded while used.
extern WorldCollisionSurfaceProperties* gDryfieldNightSouvenirShopSurfacePropertiesByClass[8];

/// Draws the souvenir shop's two additive prism lights for the current frame.
///
/// Gameplay effect bank 6, slot 0x104 supplies a live `TASK_BODY_COORD` task.
/// Its coordinate and borrowed parents must remain writable. The callback
/// refreshes their composed transform, applies it to both eight-corner local
/// blocks, then projects the signed-16-bit results through `GsWSMATRIX`.
/// Requires the current view, an initialized scratch stack and frame packet
/// arena with room for ten Gouraud quads plus additive blend commands.
/// Queued packets live until GPU completion.
/// The room overlay must remain loaded; the callback keeps the task alive.
void dryfieldNightSouvenirShopPrismLightTask(Task* task);

/// Runs the night souvenir shop's room-message task for one update.
///
/// Requires a live task: state 0 initializes, 1 idles and 2 releases the task.
/// The state index is unchecked. Keep this room overlay loaded while the task
/// or its registered message handlers can run; the final state ends its lifetime.
void dryfieldNightSouvenirShopRoomTask(Task* task);

#endif // INCLUDE_ROOMS_DRYFIELD_NIGHT_SOUVENIR_SHOP_H
