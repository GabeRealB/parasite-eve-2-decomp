#ifndef INCLUDE_WEAPONS_M4A1_BAYONET_H
#define INCLUDE_WEAPONS_M4A1_BAYONET_H

#include "main/task_types.h"

/// Records the M4A1 bayonet's thrust and draws its fading blue-white ribbon.
///
/// Requires the bayonet overlay loaded, a live single-coordinate task body,
/// and its owned `EffectWork` in `spawnArg2.pointer`. The work's borrowed
/// parent is the weapon coordinate and must remain live. State 0 seeds both
/// eight-frame histories; state 1 records and draws seven ribbon segments.
/// Only running room effects advance age; paused or hidden effects retain
/// the task without drawing. Age 13 or cancellation frees its work and kills the task.
/// The histories belong to the overlay, so simultaneous instances share them.
/// Requires a current view, frame-arena room for seven quads and blend commands,
/// and an initialized scratch stack with 48 free bytes, released before return.
void m4a1BayonetTrailTask(Task* task);

/// Runs the M4A1 bayonet's three-round burst or timed thrust.
///
/// Primary input selects the burst, with shots four dispatches apart and
/// impacts resolved on the following tick. Secondary input selects an 18-tick
/// windup and ten-tick live thrust; contact sound can repeat each contact tick.
/// Recovery allows action-button cancellation after the attack's nine/ten-tick
/// delay. Reserves 80 scratch bytes for the temporary impact node.
///
/// Requires live player `GameActor` work, its model and initialized animation
/// slots, equipped weapon/contact storage, and the matching weapon overlay
/// loaded throughout dispatch and owned effects. Phase 0 enters normal mode
/// state 4; later calls advance `GameActor::statePhase`. Counts are dispatch
/// ticks. Scratch reservations are released before returning.
void m4a1BayonetAttackState(Task* playerTask);

#endif // INCLUDE_WEAPONS_M4A1_BAYONET_H
