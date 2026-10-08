#ifndef INCLUDE_ROOMS_SHELTER_R49_H
#define INCLUDE_ROOMS_SHELTER_R49_H

#include "types.h"

#include "gameplay/area.h"
#include "gameplay/direction.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/task_types.h"

extern TaskDesc D_shelter_r49_8017DA00[2];

extern AreaVariant D_shelter_r49_8017DD74[13];

// shelter_r49
extern WorldCollisionRoomResources D_shelter_r49_8017DA18[];

extern u8* D_shelter_r49_8017DA28[];

extern ViewCount D_shelter_r49_8017DA2C[];

extern WorldCoordRoomLighting D_shelter_r49_8017DA30[];

extern DirectionWarpEntry D_shelter_r49_8017DA38[];

extern ViewCamera D_shelter_r49_8017DAD0[];

extern SpriteView D_shelter_r49_8017DCA0[];

extern WorldCollisionSurfaceProperties* D_shelter_r49_8017DDF8[];

/// Runs Shelter R49's room initialization, idle or teardown state.
///
/// The stage's room-task table starts this bodyless task in state 0. Initialization
/// installs its message handlers, publishes it in the room slot and starts the
/// room scene unless the live save's demo scene is 9. State 1 remains idle;
/// state 2 kills the task. The state must stay in 0..2; dispatch has no bounds
/// check. Requires the room, map and scene-actor overlays to remain loaded while
/// their callbacks or scripts can run. No work block or spawn payload is used.
void shelterR49RoomTask(Task* task);

/// No-op per-frame callback for Shelter R49's room-effect task.
///
/// Effect bank 6, slot 0x162 supplies a coordinate-body task. `unusedTask` is
/// ignored; the callback draws nothing and leaves the task and its resources
/// live for external teardown. The `shelter_r49` overlay must remain loaded
/// while this callback can run.
void shelterR49EffectNoopTask(Task* unusedTask);

#endif // INCLUDE_ROOMS_SHELTER_R49_H
