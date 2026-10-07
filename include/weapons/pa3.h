#ifndef INCLUDE_WEAPONS_PA3_H
#define INCLUDE_WEAPONS_PA3_H

#include "main/task_types.h"

/// Runs the PA3 player shot, contact handling, pump cue and recovery.
///
/// The SP12 package builds this implementation with its own weapon constants.
/// The pump cue follows a 31-tick countdown. Buckshot omits impact lookup;
/// Firefly uses the first contact position for its ammunition-specific report,
/// and other ammunition uses the common report for a selected room impact.
///
/// Requires live player `GameActor` work, its model and initialized native
/// animation slots, equipped weapon/contact storage and the matching weapon
/// overlay loaded throughout dispatch and owned effects. Phase 0 enters
/// normal mode state 4; later calls advance `GameActor::statePhase`. Frame counts
/// are dispatch ticks. Releases its scratch reservation before returning.
/// Reserves 80 scratch bytes for a temporary impact node, in addition to
/// the called helpers' reservations.
void pa3AttackState(Task* playerTask);

#endif // INCLUDE_WEAPONS_PA3_H
