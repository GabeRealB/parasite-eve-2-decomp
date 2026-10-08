#ifndef INCLUDE_ROOMS_NEO_ARK_ALTAR_H
#define INCLUDE_ROOMS_NEO_ARK_ALTAR_H

#include "types.h"

#include "gameplay/direction.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/task_types.h"

// neo_ark_altar
extern WorldCollisionRoomResources D_neo_ark_altar_8017F094[];

extern WorldCoordRoomLighting D_neo_ark_altar_8017F0C4[];

extern u8* D_neo_ark_altar_8017F0EC[];

extern ViewCount D_neo_ark_altar_8017F0F8[];

extern DirectionWarpEntry D_neo_ark_altar_8017F0FC[];

extern ViewCamera D_neo_ark_altar_8017F5A0[];

extern SpriteView D_neo_ark_altar_8017FE38[];

extern WorldCollisionSurfaceProperties* D_neo_ark_altar_8018005C[];

/// Leaves the altar's bank-6 effect task (slot 0x15A) unchanged.
///
/// `unusedTask` is ignored: this callback draws nothing and does not end the task.
void neoArkAltarEffectNoopTask(Task* unusedTask);

/// Initializes the altar's room services, then keeps its message task alive.
///
/// `task` must be live with state 0..2: 0 installs the room message table,
/// updates the switch sprites from the saved choice and starts the tile
/// sequence controller; 1 waits for messages; 2 releases the task. The altar
/// overlay must remain loaded.
/// The map overlay selects this callback for Neo Ark area 20.
void neoArkAltarRoomTask(Task* task);

#endif // INCLUDE_ROOMS_NEO_ARK_ALTAR_H
