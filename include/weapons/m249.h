#ifndef INCLUDE_WEAPONS_M249_H
#define INCLUDE_WEAPONS_M249_H

#include "gameplay/animation.h"

#include "main/task_types.h"
#include "main/tmd_types.h"

/// Native player animation bank supplied by the loaded M249 package.
///
/// The primary character selects bank index 18 for equipped weapon 17.
/// Base slots 1..44 contain nineteen-track player clips; slots 0, 45 and 46
/// are NULL. The 32 extension words start cleared and remain writable for
/// scripted copies, including payloads that are not clip pointers. Play an
/// extension slot only after installing a clip pointer there. Keep the bank
/// and its borrowed clip data loaded while playback or cached records refer to them.
extern AnimationSet* gM249PlayerAnimationBank[ANIMATION_BANK_SET_CAPACITY];

/// Single-part model of the player's equipped M249.
///
/// Weapon tasks borrow this source and its geometry; keep the M249 overlay
/// loaded until every referring model is released. First creation resolves
/// draw-handler addresses in the writable command stream.
extern TmdSource gM249WeaponModel;

/// Runs the M249 fire cycle and tracks the lock target on every dispatch.
///
/// Each round fires on the second shot-delay tick, resolves its impact on the
/// following tick, and repeats while held input and ammunition permit.
/// Reserves a 104-byte scratch block containing the temporary impact node.
///
/// Requires live player `GameActor` work, its model and initialized animation
/// slots, equipped weapon/contact storage, and the matching weapon overlay
/// loaded throughout dispatch and owned effects. Phase 0 enters normal mode
/// state 4; later calls advance `GameActor::statePhase`. Counts are dispatch
/// ticks. Scratch reservations are released before returning.
void m249AttackState(Task* playerTask);

#endif // INCLUDE_WEAPONS_M249_H
