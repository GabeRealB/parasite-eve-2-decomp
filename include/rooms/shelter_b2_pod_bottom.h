#ifndef INCLUDE_ROOMS_SHELTER_B2_POD_BOTTOM_H
#define INCLUDE_ROOMS_SHELTER_B2_POD_BOTTOM_H

#include "types.h"

#include "gameplay/area.h"
#include "gameplay/direction.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/task_types.h"
#include "main/tmd_types.h"

extern TmdSource gShelterB2PodBottomModel0A2A0;

extern TmdSource gShelterB2PodBottomModel0A68C;

extern TmdSource gShelterB2PodBottomModel0AA08;

extern TmdSource gShelterB2PodBottomModel0AE48;

extern AreaVariant D_shelter_b2_pod_bottom_80187678[11];

// shelter_b2_pod_bottom
extern WorldCollisionRoomResources D_shelter_b2_pod_bottom_80181D14[];

extern WorldCoordRoomLighting D_shelter_b2_pod_bottom_80181D24[];

extern u8* D_shelter_b2_pod_bottom_80181D2C[];

extern ViewCount D_shelter_b2_pod_bottom_80181D30[];

extern DirectionWarpEntry D_shelter_b2_pod_bottom_80181D34[];

extern ViewCamera D_shelter_b2_pod_bottom_80182B80[];

extern SpriteView D_shelter_b2_pod_bottom_80185904[];

extern WorldCollisionSurfaceProperties* D_shelter_b2_pod_bottom_80188770[];

void func_shelter_b2_pod_bottom_8017D708(Task* task);

void func_shelter_b2_pod_bottom_80181A48(Task* task);

void func_shelter_b2_pod_bottom_80181940(Task* arg0);

void shelterB2PodBottomEffectSpriteDriftTask(Task* task);

void func_shelter_b2_pod_bottom_80181B48(Task* arg0);

void func_shelter_b2_pod_bottom_8018016C(Task* task);

void shelterB2PodBottomEffectSpriteRiseTask(Task* task);

void func_shelter_b2_pod_bottom_80180F10(Task* arg0);

void func_shelter_b2_pod_bottom_8017D760(Task* task);

void func_shelter_b2_pod_bottom_8017EC78(Task* task);

void func_shelter_b2_pod_bottom_8017F448(Task* task);

#endif // INCLUDE_ROOMS_SHELTER_B2_POD_BOTTOM_H
