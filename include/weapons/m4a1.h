#ifndef INCLUDE_WEAPONS_M4A1_H
#define INCLUDE_WEAPONS_M4A1_H

#include "gameplay/animation.h"

#include "main/task_types.h"
#include "main/tmd_types.h"

/// Native player animation bank shared by the M4A1 and both upgrade packages.
///
/// Character 1 selects bank indices 17, 21 and 22 for equipped weapons 16,
/// 20 and 21. Base slots 1..44 contain nineteen-track player clips; slots
/// 0, 45 and 46 are NULL. The 32 raw extension words at indices 47..78 start
/// cleared and remain writable for scripted copies, including non-pointer data.
/// Play an extension entry only after installing a valid clip pointer there.
/// Keep the selected family package loaded while playback borrows this bank or
/// its native clips; copied clips also require their source data to remain loaded.
extern AnimationBank gM4a1PlayerAnimationBank;

/// The held rifle model shared by the M4A1 and both upgrade packages.
///
/// One part with 58 vertices and normals, two textured triangles and 55
/// textured quads. Each primitive-buffer half is 2,940 bytes, all for direct
/// transformation. The initial hierarchy and vertex-count table are read-only;
/// draw-handler resolution writes the descriptor and command stream in place.
/// Keep the selected M4A1-family overlay loaded while a child model borrows it.
extern TmdSource gM4a1RifleModel;

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
