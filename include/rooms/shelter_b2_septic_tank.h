#ifndef INCLUDE_ROOMS_SHELTER_B2_SEPTIC_TANK_H
#define INCLUDE_ROOMS_SHELTER_B2_SEPTIC_TANK_H

#include <psyq/sys/types.h>
#include <psyq/libgte.h>

#include "types.h"

#include "gameplay/area.h"
#include "gameplay/collision.h"
#include "gameplay/direction.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/task_types.h"

// Room data exported to the shared waypoint actor.
extern SVECTOR* D_shelter_b2_septic_tank_801836A4[4];

extern SVECTOR D_shelter_b2_septic_tank_801836B4[12];

/// Undisplaced water-surface Y in signed world units, shared with the room's actors.
extern s16 gShelterB2SepticTankWaterY;

extern AreaVariant D_shelter_b2_septic_tank_80186F40[22];

// shelter_b2_septic_tank
extern u8* D_shelter_b2_septic_tank_8018356C[];

extern ViewCount D_shelter_b2_septic_tank_80183570[];

extern DirectionWarpEntry D_shelter_b2_septic_tank_80183574[];

extern WorldCollisionGrid D_shelter_b2_septic_tank_80183E0C;

extern ViewCamera D_shelter_b2_septic_tank_80183E30[];

extern SpriteView D_shelter_b2_septic_tank_801866F4[];

extern WorldCoordRoomLights D_shelter_b2_septic_tank_80186A3C;

extern WorldCollisionTrigger D_shelter_b2_septic_tank_80186A54[];

extern WorldCollisionTrigger D_shelter_b2_septic_tank_80186C1C[];

extern WorldCollisionSurfaceProperties* D_shelter_b2_septic_tank_80187014[];

void func_shelter_b2_septic_tank_8017DB10(Task* task);

void func_shelter_b2_septic_tank_8017EB7C(Task* arg0);

void func_shelter_b2_septic_tank_8017F040(Task* task);

void func_shelter_b2_septic_tank_8017F4C8(Task* task);

void func_shelter_b2_septic_tank_80180BE0(Task* task);

void func_shelter_b2_septic_tank_80181644(Task* task);

void func_shelter_b2_septic_tank_80181F2C(Task* task);

#endif // INCLUDE_ROOMS_SHELTER_B2_SEPTIC_TANK_H
