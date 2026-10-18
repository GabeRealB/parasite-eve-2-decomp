#ifndef INCLUDE_WEAPONS_PA3_H
#define INCLUDE_WEAPONS_PA3_H

#include "gameplay/animation.h"

#include "main/task_types.h"
#include "main/tmd_types.h"

/// Player animation bank used while the PA3 shotgun is equipped (asset weapons/pa3/bank).
///
/// Gameplay selects bank index 14 for character 1 with equipped weapon 13.
/// Base slots 1..44 reference 36 distinct native clips; slots 0, 45 and 46
/// are NULL. The 32 extension words at indices 47..78 start cleared and stay
/// writable for scripted copies, which may include non-pointer payloads.
/// Play an extension entry only after installing a valid clip pointer there.
/// Keep PA3 loaded while playback borrows the bank or its native clips;
/// copied clips also require their source overlays to remain loaded.
extern AnimationSet* gPa3PlayerAnimationBank[ANIMATION_BANK_SET_CAPACITY];

/// Single-part held PA3 shotgun model attached to the player's weapon hand (asset weapons/pa3/model).
///
/// Instances borrow the package's 26 vertices, 26 lighting normals and writable
/// command stream; keep PA3 loaded until all instances are released. Creation
/// resolves draw handlers in place and copies the identity root pose. Player
/// animation uses a separate bank and rig.
extern TmdSource gPa3WeaponModel;

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
