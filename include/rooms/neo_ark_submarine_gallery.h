#ifndef INCLUDE_ROOMS_NEO_ARK_SUBMARINE_GALLERY_H
#define INCLUDE_ROOMS_NEO_ARK_SUBMARINE_GALLERY_H

#include <psyq/sys/types.h>
#include <psyq/libgte.h>

#include "types.h"

#include "gameplay/area.h"
#include "gameplay/area_flags.h"
#include "gameplay/collision.h"
#include "gameplay/direction.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/task_types.h"

// Room data exported to the shared waypoint actor.
extern SVECTOR* D_neo_ark_submarine_gallery_80181B0C[4];

extern SVECTOR D_neo_ark_submarine_gallery_80181B1C[6];

extern s16 D_neo_ark_submarine_gallery_80181A48;

extern s16 D_neo_ark_submarine_gallery_801818B8;

extern TaskDesc D_neo_ark_submarine_gallery_801818BC[1];

extern AreaApplyRec D_neo_ark_submarine_gallery_8018590C[4];

extern TaskDesc D_neo_ark_submarine_gallery_8018186C;

extern AreaVariant D_neo_ark_submarine_gallery_80185860[13];

// neo_ark_submarine_gallery
extern u8* D_neo_ark_submarine_gallery_80181A08[];

extern ViewCount D_neo_ark_submarine_gallery_80181A0C[];

extern DirectionWarpEntry D_neo_ark_submarine_gallery_80181A10[];

extern WorldCollisionGrid D_neo_ark_submarine_gallery_8018239C;

extern ViewCamera D_neo_ark_submarine_gallery_801823C0[];

extern SpriteView D_neo_ark_submarine_gallery_80184D10[];

extern WorldCoordRoomLights D_neo_ark_submarine_gallery_80185284;

extern WorldCollisionTrigger D_neo_ark_submarine_gallery_8018529C[];

extern WorldCollisionTrigger D_neo_ark_submarine_gallery_801854FC[];

extern WorldCollisionSurfaceProperties* D_neo_ark_submarine_gallery_801858EC[];

void func_neo_ark_submarine_gallery_8017EFEC(Task* arg0);

void func_neo_ark_submarine_gallery_8017F288(Task* task);

void func_neo_ark_submarine_gallery_8017F710(Task* task);

void func_neo_ark_submarine_gallery_8017EBCC(Task* task);

#endif // INCLUDE_ROOMS_NEO_ARK_SUBMARINE_GALLERY_H
