#ifndef INCLUDE_ROOMS_DRYFIELD_R08_H
#define INCLUDE_ROOMS_DRYFIELD_R08_H

#include "types.h"

#include "gameplay/area.h"
#include "gameplay/direction.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/task_types.h"

/// Number of retained placement-variant slots, including absent slots.
enum { DRYFIELD_R08_AREA_VARIANT_COUNT = 13 };

/// Actor placements and loading/spawn recipes indexed by `GameLocationKey.variant`.
///
/// Slots 1 are present; every other slot is absent (NULL, NULL).
/// Supply an index in 0..12; this directory has no terminator or bounds check.
/// The placement selector is independent of room and view selectors.
/// Placements belong to the stage map, recipes to this room. Keep both overlays
/// loaded while loading or using either table. Referenced actor overlays must
/// be loaded for spawning and model-policy updates.
extern AreaVariant gDryfieldR08AreaVariants[DRYFIELD_R08_AREA_VARIANT_COUNT];

/// Collision resources for Dryfield R08's room variants 1 and 2.
///
/// Both entries borrow the same grid and have no boundary triggers, action
/// triggers or sight occluders. Indexed by room minus one through the Dryfield
/// map's area-8 directory; the room overlay must remain loaded while used.
extern WorldCollisionRoomResources gDryfieldR08RoomCollisionResources[2];

/// Logical-view maps for Dryfield R08's room variants 1 and 2.
///
/// Indexed by room minus one. Both borrow gameplay's `gViewIdentityMap`;
/// initially logical views 1..6 select cameras, images and sprite views 1..6.
/// The directory belongs to the loaded room overlay; the maps belong to gameplay.
extern u8* gDryfieldR08RoomViewMaps[2];

/// Six logical views in each of Dryfield R08's two room variants.
///
/// Signed halfword element counts indexed by room minus one, used to bound
/// reverse view-map searches. The map borrows them while this overlay is loaded.
extern ViewCount gDryfieldR08RoomViewCounts[2];

/// Light collections for Dryfield R08's room variants 1 and 2.
///
/// Indexed by room minus one. Variant 1 initially selects one white point light;
/// variant 2 selects two cyan and two red point lights. The selector can replace
/// variant 1's collection. NULL ambient tables use the gameplay default minima.
/// The descriptors and their mutable lights live in the loaded room overlay.
extern WorldCoordRoomLighting gDryfieldR08RoomLighting[2];

/// Dryfield R08's single warp endpoint, placing both actors facing +X in view 2.
///
/// The player and companion share world position (3360, 0, 2976) and a quarter-
/// turn yaw. Warp ID 1 selects this entry; sounds, flags and map marker are absent.
/// The stage map borrows the entry while the room overlay is loaded.
extern DirectionWarpEntry gDryfieldR08WarpEntries[1];

/// Cameras for Dryfield R08's mapped views 1..6, indexed by mapped view minus one.
///
/// Rotation coefficients have 12 fractional bits; translation is the negated
/// world-space camera origin and projection distance is in pixels. Views 2 and 6
/// share camera values. Queued camera application requires this overlay loaded.
extern ViewCamera gDryfieldR08ViewCameras[6];

/// Sprite resources for Dryfield R08's six mapped views.
///
/// Indexed by mapped view minus one. View 1 has no source sprites; views 2..6
/// borrow mutable source and terminated batch arrays. Every view keeps decoded
/// background strips and has no clipping-area list. View 4 has eleven individually
/// hideable lamp batches. Keep the overlay loaded during packet setup and drawing.
extern SpriteView gDryfieldR08SpriteViews[6];

/// Collision policy directory for Dryfield R08's surface classes 0..7.
///
/// All eight entries borrow one blocking record that allows weapon impacts and
/// pushback, with no footstep cues. There is no terminator or room-variant index.
/// The directory and record stay valid only while the room overlay is loaded.
extern WorldCollisionSurfaceProperties* gDryfieldR08SurfacePropertiesByClass[8];

/// Suppresses glows for the first `shatteredLampCount` lamps in the shattering order.
///
/// Counts must be nonnegative; 12 or greater suppresses every lamp glow.
/// The value is stored without validation or clamping. A negative count would
/// make the drawing task index before its lamp-point array.
/// The room glow task resets the count on its first update, so set it afterwards.
/// Only glow drawing changes; sprite visibility is controlled separately.
void dryfieldR08SetShatteredLampCount(s32 shatteredLampCount);

/// Shows or hides one lamp's cached sprites in Dryfield R08's fourth view.
///
/// `lampSpriteIndex` is 0..10; larger byte values do nothing. The seventh lamp
/// in the shattering order has no sprite batch: callers omit it and subtract
/// one from subsequent lamp indices. Zero `hidden` shows the batch; every
/// nonzero byte hides it. Requires the current stage/area sprite directory to
/// select the loaded Dryfield R08 view records; updates their mutable batches.
void dryfieldR08SetLampSpritesHidden(u8 lampSpriteIndex, u8 hidden);

/// Selects Dryfield R08's default or alternate room-light collection.
///
/// Zero `useAlternate` restores the default single point light; every nonzero
/// s16 selects the alternate four point lights. Changes only the first room
/// lighting descriptor's borrowed collection, for the loaded overlay's lifetime.
void dryfieldR08SelectLightingBank(s16 useAlternate);

/// Draws Dryfield R08's fixed room glows and unshattered lamp glows by view.
///
/// Effect-table task 0x1B6 resets the shattered-lamp count on its first update,
/// then draws each frame. Views 3 and 4 draw lamps from the cutoff through
/// lamp 11; views 2, 5 and 6 draw fixed room-glow subsets; other views draw none.
/// Requires a live task, composed view matrix, initialized scratch stack and
/// enough packet/ordering-table space. The callback neither retains nor frees
/// the task or allocates work; emitted packets live through the frame's GPU use.
void dryfieldR08LampGlowTask(Task* task);

/// Advances Dryfield R08's drifting sprite, with an unbanked redraw during suspension.
///
/// Requires a live coordinate-body task with initialized `EffectWork` in
/// `spawnArg2.pointer`. `spawnArg1.value` packs size in bits 0..11, period in
/// 12..14 (1 only when bits 12..15 are all zero), speed in 16..23 (0 means 64),
/// movement kind in 24..27, palette bank in 28..30, and alternate drawer in 31.
/// A nonzero period nibble must encode a nonzero period. Spin uses 4096 units
/// per turn; movement uses coordinate units per running update.
/// Initializes without drawing, then draws and advances through 12 banked or
/// 10 alternate cells. Nonzero effect control freezes updates and redraws the
/// banked sheet with palette zero; control >= 4 releases work and task after
/// that draw. Drawing requires composed coordinates, scratch and packet space.
void dryfieldR08SpriteDriftTask(Task* task);

#endif // INCLUDE_ROOMS_DRYFIELD_R08_H
