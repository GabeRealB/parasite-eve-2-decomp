#ifndef INCLUDE_ROOMS_DRYFIELD_NIGHT_SOUVENIR_SHOP_H
#define INCLUDE_ROOMS_DRYFIELD_NIGHT_SOUVENIR_SHOP_H

#include "types.h"

#include "gameplay/area.h"
#include "gameplay/direction.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/task_types.h"

/// Number of retained placement-variant slots, including absent slots.
enum { DRYFIELD_NIGHT_SOUVENIR_SHOP_AREA_VARIANT_COUNT = 12 };

/// Actor placements and loading/spawn recipes indexed by `GameLocationKey.variant`.
///
/// Slots 1, 3, 11 are present; every other slot is absent (NULL, NULL).
/// Supply an index in 0..11; this directory has no terminator or bounds check.
/// The placement selector is independent of room and view selectors.
/// Placements belong to the stage map, recipes to this room. Keep both overlays
/// loaded while loading or using either table. Referenced actor overlays must
/// be loaded for spawning and model-policy updates.
extern AreaVariant gDryfieldNightSouvenirShopAreaVariants[DRYFIELD_NIGHT_SOUVENIR_SHOP_AREA_VARIANT_COUNT];

/// Room 1's six point lights and per-view ambient minima for the night shop.
///
/// The night Dryfield map publishes this array at area 6, indexed by room - 1.
/// Referenced lights stay writable and all room storage requires this overlay
/// to remain loaded while lighting queries or coordinate updates use it.
extern WorldCoordRoomLighting gDryfieldNightSouvenirShopRoomLighting[1];

/// Room 1's collision grid, view boundaries and action quads, without sight blockers.
///
/// Night Dryfield area 6 publishes this one-element array, indexed by room - 1.
/// Setup borrows and mutates the grid and both trigger lists. Keep this room
/// overlay loaded until its active resources are replaced and lists unlinked.
extern WorldCollisionRoomResources gDryfieldNightSouvenirShopRoomCollisionResources[1];

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

/// One-based doorway selector and retained endpoint count.
enum {
    DRYFIELD_NIGHT_SOUVENIR_SHOP_WARP_BACK_STREET = 1,
    DRYFIELD_NIGHT_SOUVENIR_SHOP_WARP_COUNT       = 1,
};

/// The shop's single doorway endpoint, indexed by 1-based warp ID minus one.
///
/// Night Dryfield area 6 uses warp 1 for the back-street door. Arrival places
/// the player at (2000, 0, -3550), yaw 0, and the companion at (1381, 0, -2750),
/// yaw 512, in room units and 4096-angle-unit turns; the initial view is 2.
/// Arrival/departure request distinct door sounds and arrival marks map flag
/// 469; blocked travel is silent and no scripted-player or departure-fade flag
/// is set. The complete entry is borrowed/copied while this overlay is loaded.
extern DirectionWarpEntry gDryfieldNightSouvenirShopWarpEntries[DRYFIELD_NIGHT_SOUVENIR_SHOP_WARP_COUNT];

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
