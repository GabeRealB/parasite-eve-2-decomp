#ifndef ROOMS_NEO_ARK_SUBMARINE_GALLERY_H
#define ROOMS_NEO_ARK_SUBMARINE_GALLERY_H

#include "gameplay/area.h"

#include "gameplay/area_flags.h"

#include "main/task_types.h"

#include "types.h"

#include "actors/waypoints.h"
#include <psyq/libgte.h>

// Room data exported to the shared waypoint actor.
extern SVECTOR* D_neo_ark_submarine_gallery_80181B0C[4];
extern SVECTOR D_neo_ark_submarine_gallery_80181B1C[6];
extern ActorWaypointHeight D_neo_ark_submarine_gallery_80181A48;

extern s16 D_neo_ark_submarine_gallery_801818B8;

extern TaskDesc D_neo_ark_submarine_gallery_801818BC[1];

extern GpAreaApplyRec D_neo_ark_submarine_gallery_8018590C[4];

extern TaskDesc D_neo_ark_submarine_gallery_8018186C;
void func_neo_ark_submarine_gallery_8017EFEC(Task* arg0);
void func_neo_ark_submarine_gallery_8017F288(Task* task);
void func_neo_ark_submarine_gallery_8017F710(Task* task);
extern GpAreaVariant D_neo_ark_submarine_gallery_80185860[13];

#endif
