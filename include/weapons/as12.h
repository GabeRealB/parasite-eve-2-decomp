#ifndef INCLUDE_WEAPONS_AS12_H
#define INCLUDE_WEAPONS_AS12_H

#include "main/task_types.h"

/// Runs the AS12 automatic fire cycle while input, ammunition and cooldown permit.
///
/// Each shot starts a 33-tick cooldown; the following tick resolves impacts
/// and starts a 22-tick recovery cancellation delay. Buckshot omits the impact
/// sound, Firefly uses the first contact point, and other loads use the picked
/// surface impact. Reserves 80 scratch bytes for the temporary impact node.
///
/// Requires live player `GameActor` work, its model and initialized animation
/// slots, equipped weapon/contact storage, and the matching weapon overlay
/// loaded throughout dispatch and owned effects. Phase 0 enters normal mode
/// state 4; later calls advance `GameActor::statePhase`. Counts are dispatch
/// ticks. Scratch reservations are released before returning.
void as12AttackState(Task* playerTask);

#endif // INCLUDE_WEAPONS_AS12_H
