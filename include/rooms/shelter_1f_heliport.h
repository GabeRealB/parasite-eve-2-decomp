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

/// Runs the heliport room controller's setup, per-frame visibility or teardown state.
///
/// `task->state` must be 0..2. The map's room descriptor starts it at zero;
/// setup installs/publishes the room task and advances to state 1. That state
/// maintains placed-actor visibility; state 2 tears down the task. The room
/// overlay and its imported actor callbacks must remain loaded throughout.
void shelter1fHeliportRoomTask(Task* task);

/// Rebuilds the heliport's reserved companion obstacle from the current actor pose.
///
/// Uses the companion model when present, otherwise the required live player
/// model. Keeps the obstacle at that pose only while the companion exists and
/// the soldier request state is 1; otherwise offsets it by +10000 room Y units.
/// Requires loaded source geometry and a writable room grid with its reserved
/// obstacle entries. Borrows both during the call without rebuilding cell lists.
/// `unusedEventArg` is ignored event-script data; keep this room loaded when called.
void shelter1fHeliportUpdateCompanionObstacle(s32 unusedEventArg);

/// No-op per-frame callback for the heliport's room-effect slot.
///
/// Gameplay effect bank 6, slot 0x166 passes a live `unusedTask`, which is
/// ignored. The task and its resources remain live for external teardown.
/// The heliport overlay must remain loaded while the task can invoke this callback.
void shelter1fHeliportNoOpEffectTask(Task* unusedTask);

#endif // INCLUDE_ROOMS_SHELTER_1F_HELIPORT_H
