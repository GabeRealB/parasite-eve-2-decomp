#ifndef INCLUDE_ROOMS_SHELTER_B3_GARBAGE_INCINERATOR_H
#define INCLUDE_ROOMS_SHELTER_B3_GARBAGE_INCINERATOR_H

#include "types.h"
#include "overlay.h"

#include "gameplay/area.h"
#include "gameplay/area_flags.h"
#include "gameplay/direction.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/task_types.h"

extern OverlayEncounterSpot D_shelter_b3_garbage_incinerator_801874C4[16];

extern u16 D_shelter_b3_garbage_incinerator_801855DE;

extern TaskDesc D_shelter_b3_garbage_incinerator_80187150[4];

extern u8 D_shelter_b3_garbage_incinerator_80187328[40];

extern AreaApplyRec D_shelter_b3_garbage_incinerator_8018FB6C[23];

extern u16 D_shelter_b3_garbage_incinerator_8018FBC8[2];

extern AreaVariant D_shelter_b3_garbage_incinerator_8018FA58[13];

// shelter_b3_garbage_incinerator
extern WorldCoordRoomLighting D_shelter_b3_garbage_incinerator_80187280[];

extern WorldCollisionRoomResources D_shelter_b3_garbage_incinerator_801872B8[];

extern u8* D_shelter_b3_garbage_incinerator_801873F0[];

extern ViewCount D_shelter_b3_garbage_incinerator_8018740C[];

extern DirectionWarpEntry D_shelter_b3_garbage_incinerator_8018741C[];

extern ViewCamera D_shelter_b3_garbage_incinerator_801883AC[];

extern SpriteView D_shelter_b3_garbage_incinerator_8018D100[];

extern WorldCollisionSurfaceProperties* D_shelter_b3_garbage_incinerator_8018FB4C[];

void func_shelter_b3_garbage_incinerator_8017DC7C(Task* task);

void func_shelter_b3_garbage_incinerator_80180FE4(s16 arg0, s16 arg1, s16 arg2);

void func_shelter_b3_garbage_incinerator_8018507C(void);

void func_shelter_b3_garbage_incinerator_80185220(void);

void func_shelter_b3_garbage_incinerator_8018110C(Task* task);

void shelterB3GarbageIncineratorEffectSpriteDriftTaskAimed(Task* task);

void shelterB3GarbageIncineratorEffectSpriteDebrisTask(Task* task);

#endif // INCLUDE_ROOMS_SHELTER_B3_GARBAGE_INCINERATOR_H
