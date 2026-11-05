#ifndef PLASMA_PLASMA_H
#define PLASMA_PLASMA_H

#include <engine/task_types.h>

/// The Plasma cast: three bands rise and spread around the caster under stacked halos while it fades. Applies the spell
/// a few frames in; it holds while the room's PE pauses, and a held attachment or its cancellation frees it.
void plasmaCastTask(Task* task);

#endif // PLASMA_PLASMA_H
