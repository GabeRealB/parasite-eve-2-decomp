#ifndef INCLUDE_ROOMS_SHELTER_1F_GUARDROOM_H
#define INCLUDE_ROOMS_SHELTER_1F_GUARDROOM_H

#include "types.h"

#include "gameplay/direction.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/task_types.h"

// shelter_1f_guardroom
extern WorldCollisionRoomResources D_shelter_1f_guardroom_8017DA78[];

extern WorldCoordRoomLighting D_shelter_1f_guardroom_8017DA88[];

extern u8* D_shelter_1f_guardroom_8017DA90[];

extern ViewCount D_shelter_1f_guardroom_8017DA94[];

extern DirectionWarpEntry D_shelter_1f_guardroom_8017DA98[];

extern ViewCamera D_shelter_1f_guardroom_8017DC14[];

extern SpriteView D_shelter_1f_guardroom_8017DCE0[];

extern WorldCollisionSurfaceProperties* D_shelter_1f_guardroom_8017DFF4[];

void func_shelter_1f_guardroom_8017D880(Task* task);

/// Room-effect callback that leaves the guardroom's effect task idle.
///
/// Effect bank 6, slot 0x167 supplies a single-coordinate task. This callback
/// ignores the task, draws nothing and performs no teardown. The guardroom
/// overlay must remain loaded while the callback can be dispatched.
void shelter1fGuardroomEffectNoopTask(Task* unusedTask);

#endif // INCLUDE_ROOMS_SHELTER_1F_GUARDROOM_H
