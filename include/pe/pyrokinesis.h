#ifndef INCLUDE_PE_PYROKINESIS_H
#define INCLUDE_PE_PYROKINESIS_H

#include "main/task_types.h"

void func_pyrokinesis_8012EF48(Task* arg0);

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

void func_pyrokinesis_8012FAC8(Task* arg0);

#endif // INCLUDE_PE_PYROKINESIS_H
