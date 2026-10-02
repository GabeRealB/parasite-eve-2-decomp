#ifndef INCLUDE_ROOMS_ACROPOLIS_CAFETERIA_H
#define INCLUDE_ROOMS_ACROPOLIS_CAFETERIA_H

#include "types.h"

#include "gameplay/area.h"
#include "gameplay/direction.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/task_types.h"
#include "main/tmd_types.h"

extern TaskDesc D_acropolis_cafeteria_80182AD8[4];

extern TmdSource gAcropolisCafeteriaModel077D8;

extern GpAreaVariant D_acropolis_cafeteria_80189DCC[11];

/// Models those descriptors attach.
extern TmdSource gAcropolisCafeteriaModel07CA8;

extern TmdSource gAcropolisCafeteriaModel08638;

extern TmdSource gAcropolisCafeteriaModel09090;

extern TmdSource gAcropolisCafeteriaModel09A20;

/// Models the Akropolis map UI overlay's enemy descriptors attach.
extern TmdSource gAcropolisCafeteriaModel0F7A4;

extern TmdSource gAcropolisCafeteriaModel0FDFC;

// acropolis_cafeteria
extern GpRoomObjRec D_acropolis_cafeteria_8018753C[];

extern u8* D_acropolis_cafeteria_801875AC[];

extern GpViewCountRec D_acropolis_cafeteria_801875BC[];

extern WorldCoordRoomLighting D_acropolis_cafeteria_801875C4[];

extern GpWarpRec D_acropolis_cafeteria_801875E4[];

extern SpriteView D_acropolis_cafeteria_8018C48C[];

extern GpViewRec D_acropolis_cafeteria_8018C5AC[];

extern WorldCollisionSurfaceProperties* D_acropolis_cafeteria_8018CA2C[];

void func_acropolis_cafeteria_8017E708(Task* task);

void func_acropolis_cafeteria_8017F390(Task* task);

void func_acropolis_cafeteria_8017E89C(Task* task);

void func_acropolis_cafeteria_8017F948(Task* task);

void func_acropolis_cafeteria_801803AC(Task* task);

void func_acropolis_cafeteria_80180C94(Task* task);

void func_acropolis_cafeteria_8017EA90(Task* task);

void func_acropolis_cafeteria_80181E70(Task* task);

void func_acropolis_cafeteria_8017E424(Task* task);

void func_acropolis_cafeteria_801827C4(Task* task);

void func_acropolis_cafeteria_8018286C(Task* task);

#endif // INCLUDE_ROOMS_ACROPOLIS_CAFETERIA_H
