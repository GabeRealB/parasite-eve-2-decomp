#ifndef INCLUDE_ROOMS_DRYFIELD_FACTORY_H
#define INCLUDE_ROOMS_DRYFIELD_FACTORY_H

#include "types.h"

#include "gameplay/collision.h"
#include "gameplay/direction.h"
#include "gameplay/pad_script.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/task_types.h"

// dryfield_factory
extern GpRoomObjRec D_dryfield_factory_80186F10[];

extern u8* D_dryfield_factory_80186F44[];

extern WorldCoordRoomLighting D_dryfield_factory_80186F4C[];

extern GpViewCountRec D_dryfield_factory_80186F5C[];

extern GpWarpRec D_dryfield_factory_80186F60[];

extern GpViewRec D_dryfield_factory_80187C1C[];

extern SpriteView D_dryfield_factory_801895B0[];

extern WorldCollisionSurfaceProperties* D_dryfield_factory_8018A37C[];

void func_dryfield_factory_801825F0(Task* task);

void factoryDayEntryTask(Task* task);

#endif // INCLUDE_ROOMS_DRYFIELD_FACTORY_H
