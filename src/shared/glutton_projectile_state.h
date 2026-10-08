#ifndef SRC_SHARED_GLUTTON_PROJECTILE_STATE_H
#define SRC_SHARED_GLUTTON_PROJECTILE_STATE_H

#include "glutton.h"

/// Records whether this tick enters a new projectile task state.
///
/// Requires live projectile work and a task state in 0..4, the dispatch range
/// of the chunk/glob callers. Writes a 0/1 entry flag, then saves that state in
/// the signed-halfword history field; no pointer is retained.
static __inline__ void _gluttonRecordProjectileTaskState(GluttonProjectileWork* work, const Task* task)
{
    if (work->prevState != task->state) {
        work->stateChanged = 1;
    } else {
        work->stateChanged = 0;
    }
    work->prevState = task->state;
}

#endif /* SRC_SHARED_GLUTTON_PROJECTILE_STATE_H */
