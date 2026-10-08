#ifndef INCLUDE_PE_INFERNO_H
#define INCLUDE_PE_INFERNO_H

#include "main/task_types.h"

/// Sequences Inferno's level-specific flame fans and red-amber screen wash.
///
/// Requires a counted coordinate-body effect with cleared, owned `EffectWork`
/// in `spawnArg2.pointer`, a live placement and active PE level 1..3. The level
/// selects the sound and initial state: level one starts two central fans;
/// level two adds four offset fans; level three starts a central fan followed
/// by six offset fans at age 12 and six drifting fans at age 24. Fan spawns
/// are unlimited and run independently of this task's teardown tree.
///
/// Requests stat application at the level's final burst and fades the screen
/// wash by eight channel units per tick before retiring. A held attachment or
/// PE cancellation ends it immediately. Inferno must remain loaded until its
/// cast and independent fan tasks have finished.
void infernoCastTask(Task* task);

/// Animates and draws Inferno's two concentric flame bands.
///
/// Bank-6 slot 0xDA requires a counted effect task with a live coordinate body
/// and initialized, owned `EffectWork` in `spawnArg2.pointer`. Start in state zero
/// with `spawnArg1.value` selecting a variant (0 grow then fade, 1 stationary
/// burst, 2 drifting burst, 3 wide ignition, 4 shallow ring, 5 fast ignition).
/// The spawn offset is a local displacement in coordinate units; variant 2
/// drifts each frame by its rotated Q12-scaled value (1/32 of the offset).
///
/// Owns twelve texture-phase bytes in `Task::work`, freed by task teardown.
/// Allocation failure retries initialization next frame with age reset.
/// The work starts with zero age and growth. `scale` is texture brightness;
/// `angle`, `period` and `step` are radius growth, rising-band lift growth and
/// additional upper-rim spread in coordinate units.
/// Retires the counted effect on its brightness floor, a held attachment
/// effect or PE cancellation. Inferno and its texture must remain loaded;
/// drawing needs frame-arena space for up to twelve textured quads.
void infernoFlameFanTask(Task* task);

#endif // INCLUDE_PE_INFERNO_H
