#ifndef INCLUDE_ROOMS_SHELTER_1F_HELIPORT_H
#define INCLUDE_ROOMS_SHELTER_1F_HELIPORT_H

#include "types.h"

#include "gameplay/area.h"
#include "gameplay/direction.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/task_types.h"

extern TaskDesc D_shelter_1f_heliport_80181188;

extern AreaVariant D_shelter_1f_heliport_80182BF4[13];

// shelter_1f_heliport
extern WorldCollisionRoomResources D_shelter_1f_heliport_801812D0[];

extern WorldCoordRoomLighting D_shelter_1f_heliport_801812E0[];

extern u8* D_shelter_1f_heliport_801812E8[];

extern ViewCount D_shelter_1f_heliport_801812EC[];

extern DirectionWarpEntry D_shelter_1f_heliport_801812F0[];

extern ViewCamera D_shelter_1f_heliport_80181998[];

extern SpriteView D_shelter_1f_heliport_80181EC0[];

extern WorldCollisionSurfaceProperties* D_shelter_1f_heliport_80182C78[];

void func_shelter_1f_heliport_80180768(Task* task);

// Called by the actor overlay's event scripts while this room is loaded.
void func_shelter_1f_heliport_801802AC(s32 arg0);

/// No-op per-frame callback for the heliport's room-effect slot.
///
/// Gameplay effect bank 6, slot 0x166 passes a live `unusedTask`, which is
/// ignored. The task and its resources remain live for external teardown.
/// The heliport overlay must remain loaded while the task can invoke this callback.
void shelter1fHeliportNoOpEffectTask(Task* unusedTask);

#endif // INCLUDE_ROOMS_SHELTER_1F_HELIPORT_H
