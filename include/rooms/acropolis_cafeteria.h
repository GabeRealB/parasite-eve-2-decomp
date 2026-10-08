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

/// Installs the cafeteria's room-effect messages and five wandering model effects.
///
/// Requires the coordinate body and counted `EffectWork` from `effectSpawn`.
/// On state zero, registers the room-effect task slot, clears the puff gate,
/// spawns three timed and two ambient wanderers at local game-coordinate offsets,
/// and selects the cafeteria flash, twin-trail and spark-burst IDs. Spawn failure
/// is ignored; later callbacks do nothing. Work and the registered receiver must
/// remain live while messages are dispatched and are released by effect teardown.
void acropolisCafeteriaInitRoomEffectsTask(Task* task);

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

/// Emits drifting cafeteria puffs while the puff gate is enabled in session view 9.
///
/// Requires the coordinate body and counted `EffectWork` from `effectSpawn`.
/// `EffectWork::scale` caches the previous session view: entry to view 9 emits
/// forty puffs with spawn bit 12 set to skip fade-in; later ticks emit two.
/// Offsets in the emitter's local frame span X 560..3179, Y -1900..-300 in
/// 400-unit steps, and Z 2816..3839. Spawn bits 0..11 hold a perspective size
/// factor of 384..639. Each attempt consumes four shared LCG draws, including
/// failed spawns. Other views retain the emitter; clearing the gate releases
/// its work and task. Independently spawned puffs manage their own lifetimes.
void acropolisCafeteriaPuffEmitterTask(Task* task);

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

/// Runs the cafeteria's impact flash followed by smoke or orange rings and sparks.
///
/// Requires the coordinate body and zero-aged counted `EffectWork` from
/// `effectSpawn`. Nonzero `spawnArg1.value` selects smoke; zero selects rings
/// and two bouncing sparks. Both enter release at active age seven and free
/// work on the next active tick. Nonzero room effect control pauses below four
/// and cancels at four or above. Child effects have independent lifetimes.
void acropolisCafeteriaRoomVisualEffectsSparkBurstTask(Task* task);

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

/// Dispatches one state of the cafeteria room task while its overlay is loaded.
///
/// Requires a live bodyless task with state 0 (register messages and restore
/// actors), 1 (player debug tick) or 2 (teardown); the state index is unchecked.
/// Initialization borrows loaded room/scene resources and registers the task
/// in `GAME_TASK_SLOT_ROOM`. The owner must keep it live while that slot is used.
/// State 1 requires a live player when debug mode is nonzero. State 2 invokes
/// `taskKill`, which may release the task before this callback returns.
void acropolisCafeteriaRoomTask(Task* task);

/// Lights and depth-orders the placed Mendel journal until it is collected.
///
/// Requires a TMD body and borrowed `Enemy` placement in `spawnArg2.pointer`;
/// its place-key low byte selects a valid current-stage object flag (4 in the
/// cafeteria table). State 2 hides active drawing; other states clear model
/// flags and select fixed lighting. Mapped views 12 and 24 add 7 and 4 OT entries
/// respectively, with -2 elsewhere. Model, buffer and overlay lighting storage
/// must remain live; this hook neither allocates nor retires the task.
void acropolisCafeteriaMendelPickupTask(Task* task);

/// Draws the placed Stim pickup in mapped view 9 and retires it after collection.
///
/// Requires a TMD body, borrowed `Enemy` placement in `spawnArg2.pointer`, and a
/// live auxiliary heap. The place-key low byte selects a valid current-stage
/// object flag (9 in the cafeteria table). Other views replace the draw flags
/// with active-pass exclusion and postpone teardown. In view 9, state 2 clears
/// flagged drawing and calls the task exit; other states select fixed lighting,
/// the flagged pass and zero depth bias, retrying primitive-buffer allocation.
/// Placement flag 10 additionally replaces X rotation with 1024 angular units
/// (a quarter turn); the current spawn table does not select that branch.
void acropolisCafeteriaStimPickupTask(Task* task);

#endif // INCLUDE_ROOMS_ACROPOLIS_CAFETERIA_H
