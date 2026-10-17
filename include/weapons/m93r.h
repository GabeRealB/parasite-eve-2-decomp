#ifndef INCLUDE_WEAPONS_M93R_H
#define INCLUDE_WEAPONS_M93R_H

#include "gameplay/animation.h"

#include "main/task_types.h"
#include "main/tmd_types.h"

/// Native player animation bank supplied by the loaded M93R package.
///
/// The primary character selects bank index 3 for equipped weapon 2.
/// Base slots 1..44 contain nineteen-track player clips; slots 0, 45 and 46
/// are NULL. The 32 extension words start cleared and remain writable for
/// scripted copies, including payloads that are not clip pointers. Play an
/// extension slot only after installing a clip pointer there. Keep the bank
/// and its borrowed clip data loaded while playback or cached records refer to them.
extern AnimationSet* gM93rPlayerAnimationBank[ANIMATION_BANK_SET_CAPACITY];

/// Held M93R model attached to the player's weapon-hand anchor (asset weapons/m93r/model).
///
/// Objects borrow the geometry and writable command stream; the M93R package
/// must remain loaded until those objects are released. First creation resolves
/// the source's draw-handler slots in place.
extern TmdSource gM93rWeaponModel;

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
