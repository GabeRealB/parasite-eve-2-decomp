#ifndef INCLUDE_ROOMS_DRYFIELD_NIGHT_MOTEL_LOBBY_H
#define INCLUDE_ROOMS_DRYFIELD_NIGHT_MOTEL_LOBBY_H

#include "types.h"

#include "gameplay/area.h"
#include "gameplay/direction.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/task_types.h"

extern AreaVariant D_dryfield_night_motel_lobby_801843C4[11];

// dryfield_night_motel_lobby
extern WorldCollisionRoomResources D_dryfield_night_motel_lobby_80182908[];

extern WorldCoordRoomLighting D_dryfield_night_motel_lobby_80182918[];

extern u8* D_dryfield_night_motel_lobby_80182920[];

extern ViewCount D_dryfield_night_motel_lobby_80182924[];

extern DirectionWarpEntry D_dryfield_night_motel_lobby_80182928[];

extern ViewCamera D_dryfield_night_motel_lobby_80182DD8[];

extern SpriteView D_dryfield_night_motel_lobby_80183D1C[];

extern WorldCollisionSurfaceProperties* D_dryfield_night_motel_lobby_8018448C[];

void func_dryfield_night_motel_lobby_8017EAE0(Task* task);

void func_dryfield_night_motel_lobby_801812F8(Task* unused);

void func_dryfield_night_motel_lobby_8017FE38(Task* task);

#endif // INCLUDE_ROOMS_DRYFIELD_NIGHT_MOTEL_LOBBY_H
