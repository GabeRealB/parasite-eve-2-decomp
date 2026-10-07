#ifndef INCLUDE_WEAPONS_M950_H
#define INCLUDE_WEAPONS_M950_H

#include "main/task_types.h"

/// Runs the M950 player attack and repeats fire while input and ammunition permit.
///
/// Each shot uses a four-tick delay; impact is resolved on the following tick.
/// Tracks the lock target every dispatch and allows early recovery after nine ticks.
///
/// Requires live player `GameActor` work, its model and initialized native
/// animation slots, equipped weapon/contact storage and the matching weapon
/// overlay loaded throughout dispatch and owned effects. Phase 0 enters
/// normal mode state 4; later calls advance `GameActor::statePhase`. Frame counts
/// are dispatch ticks. Releases its scratch reservation before returning.
/// Reserves 80 scratch bytes for a temporary impact node, in addition to
/// the called helpers' reservations.
void m950AttackState(Task* playerTask);

#endif // INCLUDE_WEAPONS_M950_H
