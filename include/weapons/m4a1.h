#ifndef INCLUDE_WEAPONS_M4A1_H
#define INCLUDE_WEAPONS_M4A1_H

#include "main/task_types.h"

/// Runs the M4A1 single shot or three-round burst, including both upgrade builds.
///
/// Primary input selects three rounds; other input selects one. Shots are four
/// dispatches apart, impacts resolve on the next dispatch, and recovery permits
/// action-button cancellation after nine ticks. The package's `WEAPON_ID`
/// selects its item, sound bank and flash. Reserves an 80-byte impact node.
///
/// Requires live player `GameActor` work, its model and initialized animation
/// slots, equipped weapon/contact storage, and the matching weapon overlay
/// loaded throughout dispatch and owned effects. Phase 0 enters normal mode
/// state 4; later calls advance `GameActor::statePhase`. Counts are dispatch
/// ticks. Scratch reservations are released before returning.
void m4a1AttackState(Task* playerTask);

/// Upgrade-package aliases of `m4a1AttackState`, with their own `WEAPON_ID`.
void func_m4a1_p1_8011D1C4(Task* arg0);
void func_m4a1_p2_8011D1C4(Task* arg0);

#endif // INCLUDE_WEAPONS_M4A1_H
