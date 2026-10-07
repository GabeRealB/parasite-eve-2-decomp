#ifndef INCLUDE_ROOMS_ACROPOLIS_CAFETERIA_H
#define INCLUDE_ROOMS_ACROPOLIS_CAFETERIA_H

#include "types.h"

#include "gameplay/area.h"
#include "gameplay/direction.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/task_types.h"
#include "main/tmd_types.h"

extern TaskDesc D_acropolis_cafeteria_80182AD8[4];

extern TmdSource gAcropolisCafeteriaModel077D8;

extern AreaVariant D_acropolis_cafeteria_80189DCC[11];

/// Models those descriptors attach.
extern TmdSource gAcropolisCafeteriaModel07CA8;

extern TmdSource gAcropolisCafeteriaModel08638;

extern TmdSource gAcropolisCafeteriaModel09090;

extern TmdSource gAcropolisCafeteriaModel09A20;

/// Models the Akropolis map UI overlay's enemy descriptors attach.
extern TmdSource gAcropolisCafeteriaModel0F7A4;

extern TmdSource gAcropolisCafeteriaModel0FDFC;

// acropolis_cafeteria
extern WorldCollisionRoomResources D_acropolis_cafeteria_8018753C[];

extern u8* D_acropolis_cafeteria_801875AC[];

extern ViewCount D_acropolis_cafeteria_801875BC[];

extern WorldCoordRoomLighting D_acropolis_cafeteria_801875C4[];

extern DirectionWarpEntry D_acropolis_cafeteria_801875E4[];

extern SpriteView D_acropolis_cafeteria_8018C48C[];

extern ViewCamera D_acropolis_cafeteria_8018C5AC[];

extern WorldCollisionSurfaceProperties* D_acropolis_cafeteria_8018CA2C[];

void func_acropolis_cafeteria_8017E708(Task* task);

/// Moves the cafeteria's model effect through random turns, short runs and departure.
///
/// Requires the TMD body and counted `EffectWork` installed by `effectSpawn`.
/// A zero `spawnArg1.value` selects timed wandering, departing from age 121;
/// nonzero selects ambient wandering. The model exits beyond local X 2816 or
/// 3472 respectively. Nonzero room effect control pauses it; control at four
/// or above cancels it and releases the effect work.
///
/// `EffectWork::index` is the movement phase (0 timed turn, 1 timed run,
/// 2 ambient turn, 3 ambient run, 4 departure). `scale` is yaw in 4096 units
/// per turn, `angle` is forward speed with four fractional bits, `period` is
/// the local X exit threshold, and `step` latches the departure sound (0 pending,
/// 1 played). Rotation columns use twelve fractional bits.
void acropolisCafeteriaModelWanderTask(Task* task);

void func_acropolis_cafeteria_8017E89C(Task* task);

/// Runs the cafeteria's charging pink flash, peak screen tint and fading star.
///
/// Requires a coordinate body and counted `EffectWork` from `effectSpawn`.
/// `spawnArg1.value` is a positive charge duration in callback ticks and is
/// consumed as a countdown. Nonzero room effect control pauses the flash;
/// control at four or above cancels it. Completion releases the work and task.
void acropolisCafeteriaRoomVisualEffectsFlashTask(Task* task);

/// Runs the cafeteria's twin trails from two offsets on the effect's parent.
///
/// Requires a coordinate body and counted `EffectWork` from `effectSpawn`.
/// `spawnArg1.value` is the lifetime in active ticks (2..32767); initialization
/// counts as the first tick. Owns two eight-coordinate histories in `Task::work`,
/// released with the effect on completion. Allocation failure retries with age
/// reset to zero. Room effect control at two or above holds both age and drawing.
void acropolisCafeteriaRoomVisualEffectsTwinTrailTask(Task* task);

void func_acropolis_cafeteria_80180C94(Task* task);

/// Animates the cafeteria's ten-cell drifting puff billboard.
///
/// Requires a coordinate body and counted `EffectWork` from `effectSpawn`.
/// Active in session view 9 while the room's puff gate is set. `spawnArg1`
/// bits 0..11 supply the perspective size factor; bit 12 starts age at ten
/// ticks, past the fade-in.
/// At age zero, projected depth above 16 seeds rotation (4096 units per turn),
/// Y/Z drift (4..19 world units per tick) and the cell period (3..6 ticks).
/// Drawing has no depth guard: the projected depth must be nonzero, and the
/// period must be positive before cell selection. `EffectWork::scale` holds
/// rotation, `angle` size, `step` cell period and `age` elapsed ticks.
/// Releases the work and task after all ten cells, or when the gate/view ends.
void acropolisCafeteriaPuffTask(Task* task);

/// Runs one state of a cafeteria loose prop that sinks, hops and slides on contact.
///
/// Requires a TMD task and the cafeteria overlay to remain loaded. `state`
/// must be 0 (initialize near the live player), 1 (update motion),
/// 2 (request exit) or 3 (unlink the sphere and kill the task). Initialization
/// owns collision work in `Task::work` and installs the sphere-unlinking exit
/// callback; the task releases that work during teardown.
void acropolisCafeteriaLoosePropTask(Task* task);

void func_acropolis_cafeteria_8017E424(Task* task);

void func_acropolis_cafeteria_801827C4(Task* task);

void func_acropolis_cafeteria_8018286C(Task* task);

#endif // INCLUDE_ROOMS_ACROPOLIS_CAFETERIA_H
