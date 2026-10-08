#ifndef INCLUDE_PE_PYROKINESIS_H
#define INCLUDE_PE_PYROKINESIS_H

#include "main/task_types.h"

/// Advances the collision-bearing Pyrokinesis flame through travel, stop and burst.
///
/// Bank-6 slot 0x10 requires cleared owned `EffectWork` in `spawnArg2.pointer`,
/// a coordinate body, attachment level digit 1..3, and spawn variant 0..2.
/// Initialization borrows the player rotation, chooses forward speed from the
/// level, seeds sixteen cone phases, and allocates task-owned damage/grid
/// spheres. The sound uses the original spawn variant; level 2 then forces
/// variant 1, while other levels change variant 1 to 0. `angle` holds radius,
/// `scale` RGB intensity, `period` sprite bearing and `move` flight velocity.
///
/// Travel draws the flame, optional cones and ground glow, emits trail puffs
/// and refreshes transient light slot zero. Enemy contact unlinks the damage
/// sphere and emits three tilted rings. A class-zero grid contact or the
/// level's travel extent ends motion and enters radius shrinkage. Age 31 ends
/// travel outright. Burst radius grows by 64 until the level limit; variant 2
/// then fades brightness by eight, with radius continuing to grow.
/// A held attachment or cancellation unlinks remaining bodies and releases
/// the work and task. Other non-running PE control values pause updates,
/// drawing and age; all coordinate parents and borrowed contacts must remain live.
void pyrokinesisCastTask(Task* task);

/// Expands and fades the flame cone at the Pyrokinesis launch point.
///
/// Bank-6 slot 0x11 requires a coordinate body and owned, counted `EffectWork`
/// in `spawnArg2.pointer`, initially in state 0. Keeps radius in `angle` and
/// RGB intensity in `scale`. The local radius grows from 256 by 64 coordinate
/// units per running tick; RGB intensity starts at 192 and falls by 16 after
/// drawing, giving twelve running ticks. Pauses for a
/// non-running PE effect control; cancellation or a held attachment releases
/// the work and task immediately.
void pyrokinesisLaunchConeTask(Task* task);

/// Animates one rising flame puff along the Pyrokinesis projectile's trail.
///
/// Bank-6 slot 0x69 requires a coordinate body and zeroed, counted `EffectWork`
/// owned in `spawnArg2.pointer`, initially in state 0. Chooses a local Y step
/// in -31..0 coordinate units per running tick and a fixed screen rotation in
/// 0..4095, with 4096 units per turn. Stores that rotation in `EffectWork::scale`
/// and the texture cell in `index`: cells 0..7 advance every second tick and
/// draw only on odd ticks, then the work and task are released on tick 16. Pauses for a
/// non-running PE effect control; cancellation or a held attachment releases
/// immediately.
void pyrokinesisFlamePuffTask(Task* task);

/// Expands and fades a tilted flame ring from a Pyrokinesis impact burst.
///
/// Bank-6 slot 0xF6 requires a coordinate body and owned, counted `EffectWork`
/// in `spawnArg2.pointer`, initially in state 0. Keeps radius in `angle` and
/// RGB intensity in `scale`. `spawnArg1.value` supplies the initial local Z
/// rotation in 4096 units per turn. The inner radius starts at 256 and grows
/// by 128 coordinate units per running tick; ring width is 256.
/// RGB intensity falls from 128 by 8 after drawing, ending below 9 after fifteen
/// running ticks. Pauses for a non-running PE effect control; cancellation or
/// a held attachment releases the work and task immediately.
void pyrokinesisFlameRingTask(Task* task);

/// Schedules three Pyrokinesis cast variants at running ages 1, 8 and 16.
///
/// Bank-6 slot 0xFC requires a coordinate body and cleared owned `EffectWork`
/// in `spawnArg2.pointer`. Spawns variants 0, 1 and 2 using unlimited effect
/// allocation; these casts keep their own task lifetimes. The controller
/// releases on its next running callback after the third spawn, or when the
/// attachment is held, battle ends or PE control cancels. Other non-running
/// PE control values pause age and scheduling.
void pyrokinesisVolleyTask(Task* task);

#endif // INCLUDE_PE_PYROKINESIS_H
