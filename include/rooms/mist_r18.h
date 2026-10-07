#ifndef INCLUDE_ROOMS_MIST_R18_H
#define INCLUDE_ROOMS_MIST_R18_H

#include "types.h"

#include "gameplay/area.h"
#include "gameplay/direction.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/task_types.h"

extern AreaVariant D_mist_r18_80186BFC[13];

// mist_r18
extern WorldCollisionRoomResources D_mist_r18_8018660C[];

extern u8* D_mist_r18_8018661C[];

extern ViewCount D_mist_r18_80186620[];

extern WorldCoordRoomLighting D_mist_r18_80186624[];

extern DirectionWarpEntry D_mist_r18_8018662C[];

extern ViewCamera D_mist_r18_8018671C[];

extern SpriteView D_mist_r18_80186B60[];

extern WorldCollisionSurfaceProperties* D_mist_r18_80186E70[];

/// Runs the MIST briefing and its key-item menu before departure to Dryfield.
///
/// Requires a live room task with state 0 setup, 1 advance briefing scripts or
/// 2 teardown; no index check is made. Setup registers the task in the resident
/// room slot. State 1 waits for event/menu completion and repeats the key-item
/// prompt until the Dryfield map is opened. A separate script cursor stops
/// advancing once departure begins. The callback table is copied by value before
/// dispatch. The room and Acropolis map overlays must remain loaded while active.
void mistR18BriefingTask(Task* task);

#endif // INCLUDE_ROOMS_MIST_R18_H
