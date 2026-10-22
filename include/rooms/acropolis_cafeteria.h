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

/// Spawn recipes for the cafeteria's battle, follow-up scene and Stranger settling.
///
/// Slots 0..2 create bodyless controllers at priority 32; slot 3 is TASK_DESC_END.
/// Rupert's shooting cue uses slot 2 to settle the Stranger after recoil.
/// Keep the cafeteria overlay loaded while these recipes or their tasks are used.
extern TaskDesc gAcropolisCafeteriaSceneTasks[4];

extern TmdSource gAkropolisCafeteriaAcropolisCafeteriaModel077D8;

/// Cafeteria actor layouts indexed directly by saved placement variant 0..10.
///
/// Variants 1 and 2 supply the opening and battle/follow-up scenes; 3 and 4
/// supply one and two Odd Strangers; variant 5 supplies five Bone Sucklers,
/// and variant 7 a Rook Golem.
/// Slots 0, 6 and 8..10 are absent (both pointers NULL). There is no terminator
/// or index check. The gameplay area directory borrows this table and its
/// placement/resource lists while the cafeteria and actor overlays are loaded.
extern AreaVariant gAcropolisCafeteriaAreaVariants[11];

/// Models those descriptors attach.
extern TmdSource gAcropolisCafeteriaModel07CA8;

extern TmdSource gAcropolisCafeteriaModel08638;

extern TmdSource gAcropolisCafeteriaModel09090;

extern TmdSource gAcropolisCafeteriaModel09A20;

/// Models the Akropolis map UI overlay's enemy descriptors attach.
extern TmdSource gAcropolisCafeteriaModel0F7A4;

extern TmdSource gAkropolisCafeteriaAcropolisCafeteriaModel0FDFC;

/// Collision resources indexed by cafeteria room minus one (rooms 1..4).
///
/// All rooms share the mesh and two sight occluders. Rooms 1..3 share view
/// boundaries; room 4 routes through view 6. Room 1 has the initial action set,
/// rooms 2..4 the expanded follow-up set. Borrowed records and their writable
/// resources require the cafeteria overlay loaded through binding and unlinking.
extern WorldCollisionRoomResources gAcropolisCafeteriaCollisionRooms[4];

/// Logical-view maps indexed by cafeteria room minus one (rooms 1..4).
///
/// Each room uses 24 logical entries, indexed by view minus one, with 1-based
/// camera/image/sprite values. Room 1 borrows gameplay's identity map; rooms 2/3
/// share a remap and room 4 has its own. No terminator. Keep gameplay and the
/// room overlay loaded while the directory and its borrowed maps are used.
extern u8* gAcropolisCafeteriaViewMaps[4];

/// Reverse-search lengths for cafeteria rooms 1..4, each 24 logical views.
///
/// Indexed by room minus one. Signed halfwords count readable logical-map
/// byte entries, independently of mapped-resource extents or the identity map
/// capacity. No terminator; the map directory borrows this loaded-overlay array.
extern ViewCount gAcropolisCafeteriaViewCounts[4];

/// Lighting resources indexed by cafeteria room minus one (rooms 1..4).
///
/// All entries borrow the same live fifteen point lights, one cone light and
/// logical-view ambient table. Ambient element zero counts 24 following views;
/// queries mutate light caches. The records and borrowed arrays require the
/// cafeteria overlay loaded while lighting is bound or queried.
extern WorldCoordRoomLighting gAcropolisCafeteriaRoomLightingTable[4];

/// Cafeteria arrival/departure endpoints indexed by warp minus one (1..3).
///
/// Slots connect to hallway arrival 3, patio arrival 3 and hallway arrival 4,
/// respectively, with initial cafeteria logical views 7, 2 and 5. Player and
/// companion placements use integer world units and 4096 yaw units per turn.
/// Sound IDs select the cafeteria area bank; slot 3 also supplies a blocked
/// sound. No terminator; the map directory borrows this loaded-overlay array.
extern DirectionWarpEntry gAcropolisCafeteriaWarps[3];

/// Number of cafeteria camera/image/sprite slots selected by mapped view indices.
enum { ACROPOLIS_CAFETERIA_MAPPED_VIEW_COUNT = 24 };

/// Cafeteria background sprite resources indexed by mapped view minus one (1..24).
///
/// Every entry retains decoded background strips. Views 1 and 4 share an empty
/// batch list; 14 and 22 share source sprites and batches. Other source-less
/// views retain their own batch address in `sources.empty`. Only view 5 has
/// clipping commands. No terminator; the Acropolis map directory borrows this
/// table and its mutable resources while the cafeteria overlay is loaded.
extern SpriteView gAcropolisCafeteriaSpriteViews[ACROPOLIS_CAFETERIA_MAPPED_VIEW_COUNT];

/// Cafeteria cameras indexed by mapped view minus one (1..24).
///
/// Rotation coefficients use 12 fractional bits; translation is the negated
/// camera origin in world units, and projection distance is in pixels. No
/// terminator. The Acropolis map directory borrows this array while the room
/// overlay is loaded; queued camera application must finish before it unloads.
extern ViewCamera gAcropolisCafeteriaViewCameras[ACROPOLIS_CAFETERIA_MAPPED_VIEW_COUNT];

/// Cafeteria collision and footstep properties indexed by surface class 0..7.
///
/// The Acropolis map directory borrows this table while the room overlay is
/// loaded. Classes 0 and 5..7 share silent, probe-blocking properties. Classes
/// 1..3 block probes and allow weapon impacts with distinct footstep cues;
/// class 4 passes probes and suppresses weapon impacts, borrowing class 3's
/// cues. Every class applies collision pushback. The table has no terminator.
extern WorldCollisionSurfaceProperties* gAcropolisCafeteriaSurfacePropertiesByClass[8];

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
