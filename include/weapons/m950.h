#ifndef INCLUDE_WEAPONS_M950_H
#define INCLUDE_WEAPONS_M950_H

#include "gameplay/animation.h"

#include "main/task_types.h"
#include "main/tmd_types.h"

/// Native player animation bank supplied by the loaded M950 package (asset weapons/m950/bank).
///
/// Character 1 selects bank index 4 for equipped weapon 3. Base slots 1..44
/// contain player clips; slots 0, 45 and 46 are NULL. The 32 raw extension words
/// at indices 47..78 start cleared and remain writable for scripted copies,
/// including non-pointer data. Play an extension entry only after installing a
/// valid clip pointer there. Keep M950 loaded while playback borrows this bank
/// or its native clips; copied clips also require their source data to remain
/// loaded.
extern AnimationSet* gM950PlayerAnimationBank[ANIMATION_BANK_SET_CAPACITY];

/// The held M950 weapon model attached beneath the player's hand anchor (asset weapons/m950/model).
///
/// Creation resolves draw handlers in the writable stream; the source and
/// geometry remain borrowed by live models. Keep the M950 package loaded until
/// its weapon models have been released.
extern TmdSource gM950WeaponModel;

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
