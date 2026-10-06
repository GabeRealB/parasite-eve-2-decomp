#ifndef INCLUDE_PE_OFUDA_H
#define INCLUDE_PE_OFUDA_H

#include "main/task_types.h"

/// Runs the Ofuda's expanding pink glow, delayed gameplay result and fading screen flash.
///
/// Bank 6, slot 0x198 requires a live coordinate-body task and one counted,
/// zero-initialized `EffectWork` owned through `Task::spawnArg2.pointer`.
/// Start at state 0; `spawnArg1.value` becomes the remaining growth updates.
/// The Ofuda overlay and the room-effect controller must remain live while
/// the callback runs. Its coordinate's view-space cache must be composed;
/// drawing requires the current scratch stack, primitive arena and ordering table.
///
/// After initialization, 30 updates grow the discs and contract two glow bands,
/// then request attachment-stat application. Subsequent updates shrink and dim
/// the discs while drawing two additive screen tints. `EffectWork::scale` is
/// byte brightness, `angle` is the radial drawers' signed sizing input and
/// `step` is the growth increment; `age` counts callback updates.
/// A held attachment or non-running PE control stops the sound and releases
/// immediately. Normal completion releases the counted work and task after
/// the brightness drops below 9. Do not use either after release.
void ofudaEffectTask(Task* task);

#endif // INCLUDE_PE_OFUDA_H
