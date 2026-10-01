#ifndef INCLUDE_ROOMS_NEO_ARK_EVE_ACCESS_TUNNEL_H
#define INCLUDE_ROOMS_NEO_ARK_EVE_ACCESS_TUNNEL_H

#include "types.h"

#include "gameplay/area.h"
#include "gameplay/direction.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/task_types.h"

extern GpAreaVariant D_neo_ark_eve_access_tunnel_801806B8[13];

// neo_ark_eve_access_tunnel
extern GpRoomObjRec D_neo_ark_eve_access_tunnel_8017EB78[];

extern GpRoomCoordRec D_neo_ark_eve_access_tunnel_8017EB88[];

extern u8* D_neo_ark_eve_access_tunnel_8017EB90[];

extern GpViewCountRec D_neo_ark_eve_access_tunnel_8017EB94[];

extern GpWarpRec D_neo_ark_eve_access_tunnel_8017EB98[];

extern GpViewRec D_neo_ark_eve_access_tunnel_8017F080[];

extern GpSprtRec D_neo_ark_eve_access_tunnel_801800A0[];

extern WorldCollisionSurfaceProperties* D_neo_ark_eve_access_tunnel_80180780[];

/// Sets the tunnel's view flags: `arg0` picks the run to write (0 the single
/// view `field_1C` carries, 1 the `field_28` / `field_34` pair) and `arg1`
/// picks the value -- 0 sets each flag of that run to 1, 1 clears it to 0 --
/// while any other pair of arguments writes nothing.
/// `func_neo_ark_eve_access_tunnel_8017DFC0` calls it with (0, 0) and (1, 0)
/// together while the session is still below state 4, so both runs are set.
void func_neo_ark_eve_access_tunnel_8017E090(s32 arg0, s32 arg1);

void func_neo_ark_eve_access_tunnel_8017E15C(Task* unused);

void func_neo_ark_eve_access_tunnel_8017E038(Task* task);

#endif // INCLUDE_ROOMS_NEO_ARK_EVE_ACCESS_TUNNEL_H
