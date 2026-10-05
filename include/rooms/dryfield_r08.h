#ifndef INCLUDE_ROOMS_DRYFIELD_R08_H
#define INCLUDE_ROOMS_DRYFIELD_R08_H

#include "types.h"

#include "gameplay/area.h"
#include "gameplay/direction.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/task_types.h"

extern AreaVariant D_dryfield_r08_80180B88[13];

// dryfield_r08
extern WorldCollisionRoomResources D_dryfield_r08_8017F6DC[];

extern u8* D_dryfield_r08_8017F6FC[];

extern ViewCount D_dryfield_r08_8017F704[];

extern WorldCoordRoomLighting D_dryfield_r08_8017F708[];

extern DirectionWarpEntry D_dryfield_r08_8017F718[];

extern ViewCamera D_dryfield_r08_8017FBBC[];

extern SpriteView D_dryfield_r08_80180918[];

extern WorldCollisionSurfaceProperties* D_dryfield_r08_80180C04[];

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
