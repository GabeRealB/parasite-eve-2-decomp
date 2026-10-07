#ifndef INCLUDE_WEAPONS_M93R_H
#define INCLUDE_WEAPONS_M93R_H

#include "main/task_types.h"

/// Runs the M93R player attack, selecting a single round or a three-round burst.
///
/// Primary input selects three rounds; secondary input selects one. Rounds are
/// spaced two dispatch ticks apart, stopping when primary ammunition runs out.
/// Recovery can end on the held action input after ten cancel ticks.
///
/// Requires live player `GameActor` work, its model and initialized native
/// animation slots, equipped weapon/contact storage and the matching weapon
/// overlay loaded throughout dispatch and owned effects. Phase 0 enters
/// normal mode state 4; later calls advance `GameActor::statePhase`. Frame counts
/// are dispatch ticks. Releases its scratch reservation before returning.
/// Reserves 80 scratch bytes for a temporary impact node, in addition to
/// the called helpers' reservations.
void m93rAttackState(Task* playerTask);

#endif // INCLUDE_WEAPONS_M93R_H
