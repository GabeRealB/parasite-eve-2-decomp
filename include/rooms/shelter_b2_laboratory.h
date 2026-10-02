#ifndef INCLUDE_ROOMS_SHELTER_B2_LABORATORY_H
#define INCLUDE_ROOMS_SHELTER_B2_LABORATORY_H

#include "types.h"

#include "gameplay/area.h"
#include "gameplay/area_flags.h"
#include "gameplay/collision.h"
#include "gameplay/direction.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/task_types.h"
#include "main/tmd_types.h"

extern TmdSource gShelterB2LaboratoryAcropolisSanctuaryModel090F0;

extern GpAreaApplyRec D_shelter_b2_laboratory_80186488[5];

extern GpAreaApplyRec D_shelter_b2_laboratory_8018649C[2];

extern GpAreaVariant D_shelter_b2_laboratory_80186360[11];

// shelter_b2_laboratory
extern u8* D_shelter_b2_laboratory_80182C08[];

extern GpViewCountRec D_shelter_b2_laboratory_80182C0C[];

extern GpWarpRec D_shelter_b2_laboratory_80182C10[];

extern WorldCollisionGrid D_shelter_b2_laboratory_8018355C;

extern GpViewRec D_shelter_b2_laboratory_80183580[];

extern SpriteView D_shelter_b2_laboratory_801854D0[];

extern WorldCoordRoomLights D_shelter_b2_laboratory_80185944;

extern WorldCollisionTrigger D_shelter_b2_laboratory_8018595C[];

extern WorldCollisionTrigger D_shelter_b2_laboratory_80185D84[];

extern WorldCoordRoomAmbientEntry D_shelter_b2_laboratory_801863B8[];

extern WorldCollisionSurfaceProperties* D_shelter_b2_laboratory_80186468[];

void func_shelter_b2_laboratory_801804A4(Task* task);

// Called by the actor overlay's event scripts while this room is loaded.
void func_shelter_b2_laboratory_801804FC(void);

void func_shelter_b2_laboratory_80180548(Task* task);

void func_shelter_b2_laboratory_8017EAB4(Task* task);

#endif // INCLUDE_ROOMS_SHELTER_B2_LABORATORY_H
