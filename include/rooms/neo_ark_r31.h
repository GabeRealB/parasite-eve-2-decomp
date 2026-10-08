#ifndef INCLUDE_ROOMS_NEO_ARK_R31_H
#define INCLUDE_ROOMS_NEO_ARK_R31_H

#include "types.h"

#include "gameplay/area.h"
#include "gameplay/direction.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/task_types.h"

extern s32 D_neo_ark_r31_8017DC54;

extern TaskDesc D_neo_ark_r31_8017D9E8;

extern AreaVariant D_neo_ark_r31_8017DBB8[13];

// neo_ark_r31
extern u8* D_neo_ark_r31_8017DA1C[];

extern ViewCount D_neo_ark_r31_8017DA20[];

extern DirectionWarpEntry D_neo_ark_r31_8017DA24[];

extern ViewCamera D_neo_ark_r31_8017DA5C[];

extern SpriteView D_neo_ark_r31_8017DAF8[];

extern WorldCoordRoomLights D_neo_ark_r31_8017DB7C;

extern WorldCollisionSurfaceProperties* D_neo_ark_r31_8017DC34[];

/// Runs the room controller and keeps RGB16 background masking armed.
///
/// Requires a live task with state 0 (register handlers and start the scene),
/// 1 (refresh the one-image decode mode), or 2 (kill). Keep the room, actor-461800
/// scene resources and Neo Ark map overlay loaded while the controller runs.
void neoArkR31RoomTask(Task* task);

#endif // INCLUDE_ROOMS_NEO_ARK_R31_H
