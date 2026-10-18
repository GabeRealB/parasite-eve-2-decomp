/* The P08 family, one source (src/weapons/p08): the P08, its snail-magazine variant and the Mongoose. */

#ifndef INCLUDE_WEAPONS_P08_H
#define INCLUDE_WEAPONS_P08_H

#include "gameplay/animation.h"

#include "main/task_types.h"
#include "main/tmd_types.h"

/// Native player animation bank shared by the two P08 magazine variants (asset weapons/p08/bank).
///
/// The primary character selects this bank for no weapon, the snail-magazine
/// P08 and the standard P08 (bank indices 1, 2 and 5). Each loaded P08 package
/// supplies the same table: base slots 1..44 contain clip pointers, and slots
/// 0, 45 and 46 are NULL. The 32 extension words start cleared and are writable
/// for scripted copies, including payloads that are not clip pointers.
/// Play extension slots only when they contain clips. The bank and its borrowed
/// clip data remain valid only while the selected P08 package stays loaded;
/// the unarmed selection loads the snail-magazine package.
extern AnimationSet* gP08PlayerAnimationBank[ANIMATION_BANK_SET_CAPACITY];

/// Native player animation bank supplied by the loaded Mongoose package (asset weapons/mongoose/bank).
///
/// Character 1 selects bank index 10 for equipped weapon 9. Base slots 1..44
/// contain player clips; slots 0, 45 and 46 are NULL. The 32 raw extension
/// words at indices 47..78 start cleared and remain writable for scripted
/// copies, including non-pointer data. Play an extension entry only after
/// installing a valid clip pointer there. Keep Mongoose loaded while playback
/// borrows this bank or its native clips; copied clips also require their
/// source data to remain loaded.
extern AnimationSet* gMongoosePlayerAnimationBank[ANIMATION_BANK_SET_CAPACITY];

/// Rigid held P08 pistol model selected for both magazine variants (asset weapons/p08/model).
///
/// Instances borrow the loaded P08 package's geometry and writable command
/// stream; release them before replacing the package. Creation resolves draw
/// handlers in place and copies the identity root transform.
extern TmdSource gP08WeaponModel;

/// Runs one P08 player shot and its impact and recovery phases.
///
/// This source also implements the P08 snail-magazine and Mongoose packages,
/// with package-selected ammunition, report, flash and recovery cooldown.
/// The action input can end recovery after eleven cancel ticks.
///
/// Requires live player `GameActor` work, its model and initialized native
/// animation slots, equipped weapon/contact storage and the matching weapon
/// overlay loaded throughout dispatch and owned effects. Phase 0 enters
/// normal mode state 4; later calls advance `GameActor::statePhase`. Frame counts
/// are dispatch ticks. Releases its scratch reservation before returning.
/// Reserves 80 scratch bytes for a temporary impact node, in addition to
/// the called helpers' reservations.
void p08AttackState(Task* playerTask);

#endif // INCLUDE_WEAPONS_P08_H
