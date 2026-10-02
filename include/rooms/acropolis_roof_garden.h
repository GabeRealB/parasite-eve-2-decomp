#ifndef INCLUDE_ROOMS_ACROPOLIS_ROOF_GARDEN_H
#define INCLUDE_ROOMS_ACROPOLIS_ROOF_GARDEN_H

#include "types.h"

#include "gameplay/area.h"
#include "gameplay/direction.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/task_types.h"
#include "main/tmd_types.h"

extern GpAreaVariant D_acropolis_roof_garden_80185934[12];

extern TmdSource gAcropolisRoofGardenModel09868;

// acropolis_roof_garden
extern GpRoomObjRec D_acropolis_roof_garden_80184C8C[];

extern u8* D_acropolis_roof_garden_80184C9C[];

extern ViewCount D_acropolis_roof_garden_80184CA0[];

extern WorldCoordRoomLighting D_acropolis_roof_garden_80184CA4[];

extern GpWarpRec D_acropolis_roof_garden_80184CAC[];

extern SpriteView D_acropolis_roof_garden_80186648[];

extern ViewCamera D_acropolis_roof_garden_80186BF4[];

extern WorldCollisionSurfaceProperties* D_acropolis_roof_garden_80186DB0[];

void func_acropolis_roof_garden_8017DCDC(Task* task);

void func_acropolis_roof_garden_8017DE90(Task* arg0);

void func_acropolis_roof_garden_8017E29C(Task* arg0);

void func_acropolis_roof_garden_8017F10C(Task* task);

void func_acropolis_roof_garden_8017DC74(Task* task);

void func_acropolis_roof_garden_80180160(Task* task);

#endif // INCLUDE_ROOMS_ACROPOLIS_ROOF_GARDEN_H
