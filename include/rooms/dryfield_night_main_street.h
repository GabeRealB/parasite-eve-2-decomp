#ifndef INCLUDE_ROOMS_DRYFIELD_NIGHT_MAIN_STREET_H
#define INCLUDE_ROOMS_DRYFIELD_NIGHT_MAIN_STREET_H

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

extern SVECTOR D_dryfield_night_main_street_801833F4[23];

extern SVECTOR D_dryfield_night_main_street_801834AC[196];

extern WorldCollisionGridFace D_dryfield_night_main_street_80183ACC[89];

extern WorldCollisionTrigger D_dryfield_night_main_street_8018824C[12];

extern GpAreaVariant D_dryfield_night_main_street_80188A08[13];

// dryfield_night_main_street
extern GpRoomObjRec D_dryfield_night_main_street_80182284[];

extern WorldCoordRoomLighting D_dryfield_night_main_street_801822B4[];

extern u8* D_dryfield_night_main_street_801822FC[];

extern GpViewCountRec D_dryfield_night_main_street_80182308[];

extern GpWarpRec D_dryfield_night_main_street_80182310[];

extern GpViewRec D_dryfield_night_main_street_80184564[];

extern SpriteView D_dryfield_night_main_street_801875E4[];

extern WorldCollisionSurfaceProperties* D_dryfield_night_main_street_80188B84[];

void func_dryfield_night_main_street_8017E484(Task* task);

void func_dryfield_night_main_street_8017F3B0(Task* task);

void func_dryfield_night_main_street_8017FA68(Task* task);

void func_dryfield_night_main_street_801807B0(Task* arg0);

void func_dryfield_night_main_street_80180B48(Task* arg0);

void func_dryfield_night_main_street_80181F58(Task* arg0);

void func_dryfield_night_main_street_8017E0C0(Task* task);

#endif // INCLUDE_ROOMS_DRYFIELD_NIGHT_MAIN_STREET_H
