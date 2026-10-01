#ifndef INCLUDE_ROOMS_ACROPOLIS_FIRE_ESCAPE_H
#define INCLUDE_ROOMS_ACROPOLIS_FIRE_ESCAPE_H

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

extern SVECTOR D_acropolis_fire_escape_80181E74[10];

extern SVECTOR D_acropolis_fire_escape_80181EC4[55];

extern WorldCollisionGridFace D_acropolis_fire_escape_8018207C[23];

extern GpAreaVariant D_acropolis_fire_escape_8018294C[5];

// acropolis_fire_escape
extern GpRoomObjRec D_acropolis_fire_escape_80181DAC[];

extern u8* D_acropolis_fire_escape_80181DBC[];

extern GpViewCountRec D_acropolis_fire_escape_80181DC0[];

extern WorldCoordRoomLighting D_acropolis_fire_escape_80181DC4[];

extern GpWarpRec D_acropolis_fire_escape_80181DCC[];

extern SpriteView D_acropolis_fire_escape_80182E18[];

extern GpViewRec D_acropolis_fire_escape_80182E90[];

extern WorldCollisionSurfaceProperties* D_acropolis_fire_escape_80183020[];

void func_acropolis_fire_escape_80180B20(Task* task);

void func_acropolis_fire_escape_80180154(Task* task);

void func_acropolis_fire_escape_8017EA68(Task* task);

void func_acropolis_fire_escape_8017FF7C(Task* task);

void func_acropolis_fire_escape_8017FF24(Task* task);

#endif // INCLUDE_ROOMS_ACROPOLIS_FIRE_ESCAPE_H
