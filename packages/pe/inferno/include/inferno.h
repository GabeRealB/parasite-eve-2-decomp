#ifndef INFERNO_INFERNO_H
#define INFERNO_INFERNO_H

#include <engine/task_types.h>

/// The Inferno cast: flame fans under a red-amber screen wash - at level 1 two at the center, at level 2 one with a
/// ring around it, at level 3 one followed by two bursting rings - then the wash fades. Applies the spell with the
/// level's last fans; a held attachment or a room's PE cancellation frees it. The fans run on their own.
void infernoCastTask(Task* task);

/// An Inferno flame fan: two flame bands around its point, growing and fading in the way its variant (`spawnArg1`, 0
/// to 5) sets, a drifting one moving away from the cast; a held attachment or a room's PE cancellation frees it.
void infernoFlameFanTask(Task* task);

#endif // INFERNO_INFERNO_H
