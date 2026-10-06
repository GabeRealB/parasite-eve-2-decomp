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

// dryfield_night_souvenir_shop
extern WorldCoordRoomLighting D_dryfield_night_souvenir_shop_8017E0E4[];

extern WorldCollisionRoomResources D_dryfield_night_souvenir_shop_8017E0EC[];

extern u8* D_dryfield_night_souvenir_shop_8017E0FC[];

extern ViewCount D_dryfield_night_souvenir_shop_8017E100[];

extern DirectionWarpEntry D_dryfield_night_souvenir_shop_8017E104[];

extern ViewCamera D_dryfield_night_souvenir_shop_8017E628[];

extern SpriteView D_dryfield_night_souvenir_shop_8017EF08[];

extern WorldCollisionSurfaceProperties* D_dryfield_night_souvenir_shop_8017F6CC[];

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

void func_dryfield_night_souvenir_shop_8017D65C(Task* task);

#endif // INCLUDE_ROOMS_DRYFIELD_NIGHT_SOUVENIR_SHOP_H
