#ifndef INCLUDE_ROOMS_SHELTER_B1_SLEEPING_QUARTERS_H
#define INCLUDE_ROOMS_SHELTER_B1_SLEEPING_QUARTERS_H

#include "types.h"

#include "gameplay/area.h"
#include "gameplay/collision.h"
#include "gameplay/direction.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/task_types.h"
#include "main/tmd_types.h"

extern TmdSource gShelterB1SleepingQuartersModel02DFC;

extern AreaVariant D_shelter_b1_sleeping_quarters_80183FDC[22];

// shelter_b1_sleeping_quarters
extern u8* D_shelter_b1_sleeping_quarters_80180658[];

extern ViewCount D_shelter_b1_sleeping_quarters_8018065C[];

extern DirectionWarpEntry D_shelter_b1_sleeping_quarters_80180660[];

extern WorldCollisionGrid D_shelter_b1_sleeping_quarters_801810D4;

extern ViewCamera D_shelter_b1_sleeping_quarters_801810F8[];

extern SpriteView D_shelter_b1_sleeping_quarters_80182E70[];

extern WorldCoordRoomLights D_shelter_b1_sleeping_quarters_80183234;

extern WorldCollisionTrigger D_shelter_b1_sleeping_quarters_8018324C[];

extern WorldCollisionOccluder D_shelter_b1_sleeping_quarters_801837A4[];

extern WorldCollisionTrigger D_shelter_b1_sleeping_quarters_801838D0[];

extern WorldCollisionSurfaceProperties* D_shelter_b1_sleeping_quarters_801840B0[];

/// Hides the sleeping quarters' placed model when its packed object state is 2.
///
/// Requires a live TMD-body task and its area-spawned `Enemy` work in
/// `spawnArg2.pointer`. The low byte of `Enemy::placeKey` selects an object-state
/// index in 0..63 in the session's current stage; the room's placement uses 20.
/// Other state values show the model. Only the active-draw flag changes; the
/// task keeps its model and work until its owner tears them down.
void shelterB1SleepingQuartersAreaObjectTask(Task* task);

/// Runs the sleeping quarters' room-message receiver.
///
/// Requires a live task with state 0 (register handlers), 1 (idle), or 2 (kill).
/// Keep the room and Shelter map overlays loaded while the task and its borrowed
/// message table remain available. The receiver allocates no work.
void shelterB1SleepingQuartersRoomTask(Task* task);

/// Draws the Sleeping Quarters' view-specific light beams and red glow disc.
///
/// Bank-6 effect task 0x12D. On state 0, selects this room's glow-disc,
/// flying-spark and orange-burst effects and advances to state 1; drawing
/// starts in the same tick and continues in every nonzero state. Uses the
/// current mapped camera index; only views 2..10 emit lights.
/// Requires a live task, the room overlay loaded, composed view matrices,
/// and the current frame's scratch stack, primitive arena and ordering table.
/// Queued primitives remain in the frame arena until GPU completion.
void shelterB1SleepingQuartersDrawViewLightsTask(Task* task);

void func_shelter_b1_sleeping_quarters_8017E6DC(Task* arg0);

/// Runs the sleeping quarters' orange burst with a growing glow and fading ring.
///
/// Requires a counted effect task with owned `EffectWork` in `spawnArg2.pointer`,
/// a coordinate body and initial state zero; `spawnArg1` is ignored. Running
/// updates expand the disc and layered glow, refresh a transient point light,
/// and fade the ring before the central disc. Room effect control 0 runs;
/// other values below 4 pause without drawing, and values 4 or above cancel.
/// Cancellation or completed fading releases the work and tears down the task.
/// The sleeping-quarters overlay must remain loaded while the task is live.
void shelterB1SleepingQuartersRoomVisualEffectsFlyingOrangeBurstTask(Task* task);

/// Runs the sleeping quarters' glowing-disc spark toward an initial target position.
///
/// Requires a counted effect task with owned `EffectWork` in `spawnArg2.pointer`,
/// a coordinate body, initial state zero, and zero work age and animation index.
/// `spawnArg1.pointer` borrows a `GfxCoord` through the first running update;
/// both cached matrices must be composed into the same view space then.
/// That update narrows the initial displacement to signed 16-bit local
/// components and fixes a parent-space step scaled by 204/4096. Later updates
/// move by that step and draw on odd ages; the target is not sampled again.
/// At age 20 the work is released and the task torn down. Room effect control
/// 0 runs; other values below 4 pause without drawing, and values 4 or above
/// cancel and release the effect.
/// The sleeping-quarters overlay must remain loaded while the task is live.
void shelterB1SleepingQuartersRoomVisualEffectsFlyingSparkTask(Task* task);

#endif // INCLUDE_ROOMS_SHELTER_B1_SLEEPING_QUARTERS_H
