#ifndef INCLUDE_PE_PLASMA_H
#define INCLUDE_PE_PLASMA_H

#include "main/task_types.h"

/// Expands and fades the Plasma cast with three textured bands and stacked halos.
///
/// Bank-6 slot 0x14 requires a coordinate body, cleared owned `EffectWork` in
/// `spawnArg2.pointer`, and attachment level digit 1..3. Initializes brightness
/// 160, three sixteen-entry texture-phase columns, a level sound and vibration.
/// `index` holds the level row; `scale` brightness; `angle`, `period` and `step`
/// hold radius growth, height offset and band widening in coordinate units.
///
/// Running updates subtract eight brightness units, update the geometry and
/// apply attachment stats at age eight. All three bands draw at every level;
/// levels 2 and 3 add a third halo. Level 3 uses faster growth and different
/// halo height/width. Non-running PE control below cancellation holds age and
/// geometry while redrawing. Brightness below nine, a held attachment or
/// cancellation releases the work and task.
void plasmaCastTask(Task* task);

#endif // INCLUDE_PE_PLASMA_H
