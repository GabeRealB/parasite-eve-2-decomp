#ifndef INCLUDE_ROOMS_DRYFIELD_JUNK_YARD_H
#define INCLUDE_ROOMS_DRYFIELD_JUNK_YARD_H

#include "types.h"

#include "gameplay/area.h"
#include "gameplay/direction.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/task_types.h"
#include "main/tmd_types.h"

extern AreaVariant D_dryfield_junk_yard_8017F558[13];

/// Models the Dryfield map UI overlay's enemy descriptors attach.
extern TmdSource gDryfieldJunkYardModel01378;

// dryfield_junk_yard
extern WorldCollisionRoomResources D_dryfield_junk_yard_8017ED04[];

extern WorldCoordRoomLighting D_dryfield_junk_yard_8017ED14[];

extern u8* D_dryfield_junk_yard_8017ED1C[];

extern ViewCount D_dryfield_junk_yard_8017ED20[];

extern DirectionWarpEntry D_dryfield_junk_yard_8017ED24[];

extern ViewCamera D_dryfield_junk_yard_8017F5C0[];

extern SpriteView D_dryfield_junk_yard_80180C28[];

extern WorldCollisionSurfaceProperties* D_dryfield_junk_yard_80181C28[];

/// Enables the junk yard's view effects, including dust from actor footsteps.
///
/// Gameplay dispatches this room callback as effect task 0xD8. The room-effect
/// controller must be live; the task argument is ignored and no state advances.
void dryfieldJunkYardEnableViewEffectsTask(Task* unusedTask);

/// Updates the placed Wire Rope pickup's visibility and ground shadow.
///
/// Requires a live TMD-bodied task with a model root and its live `Enemy`
/// metadata in `spawnArg2.pointer`. Its place-key low byte selects a current-stage
/// object state (0..63); the stage must be 1..5. State 2 suppresses model drawing,
/// automatic buffer allocation and the shadow. Other states clear the model
/// flags and ordering bias, drawing the shadow when a primitive buffer exists.
/// Keeps the task alive without advancing its state or allocating work.
/// The enemy metadata and model remain owned by the task until external teardown.
/// Visible buffered models require an initialized scratch stack, current view
/// matrices, and a frame packet arena and ordering table. Keep the junk-yard
/// overlay and those resources live while scheduled.
void dryfieldJunkYardWireRopeTask(Task* task);

/// Runs the junk yard's room receiver through entry setup, idle and teardown.
///
/// Requires a live bodyless task with state 0 (entry), 1 (idle) or 2 (kill).
/// Entry registers `GAME_TASK_SLOT_ROOM`; idle also runs the companion debug
/// hook when debug mode is enabled. Keep the junk-yard overlay loaded for
/// the task's lifetime.
void dryfieldJunkYardRoomTask(Task* task);

#endif // INCLUDE_ROOMS_DRYFIELD_JUNK_YARD_H
