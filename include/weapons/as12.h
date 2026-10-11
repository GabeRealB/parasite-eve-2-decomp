#ifndef INCLUDE_WEAPONS_AS12_H
#define INCLUDE_WEAPONS_AS12_H

#include "gameplay/animation.h"

#include "main/task_types.h"
#include "main/tmd_types.h"

/// Native player animation bank supplied by the loaded AS12 package.
///
/// The primary character selects bank index 16 for equipped weapon 15.
/// Base slots 1..44 contain nineteen-track player clips; slots 0, 45 and 46
/// are NULL. The 32 extension words start cleared and remain writable for
/// scripted copies, including payloads that are not clip pointers. Play an
/// extension slot only after installing a clip pointer there. The bank and
/// its borrowed clip data must stay loaded while playback refers to them.
extern AnimationBank gAs12PlayerAnimationBank;

/// Held AS12 shotgun model, with one rigid part and its textured geometry.
///
/// Model instances borrow the source and arrays from the loaded AS12 package.
/// Keep the package loaded until those instances are released. First creation
/// resolves draw handlers in the writable stream; the initial skeleton and
/// vertex-count table remain read-only. Each primitive-buffer half holds two
/// textured Gouraud triangles and 41 textured Gouraud quads (2212 bytes).
extern TmdSource gAs12Model;

/// Runs the AS12 automatic fire cycle while input, ammunition and cooldown permit.
///
/// Each shot starts a 33-tick cooldown; the following tick resolves impacts
/// and starts a 22-tick recovery cancellation delay. Buckshot omits the impact
/// sound, Firefly uses the first contact point, and other loads use the picked
/// surface impact. Reserves 80 scratch bytes for the temporary impact node.
///
/// Requires live player `GameActor` work, its model and initialized animation
/// slots, equipped weapon/contact storage, and the matching weapon overlay
/// loaded throughout dispatch and owned effects. Phase 0 enters normal mode
/// state 4; later calls advance `GameActor::statePhase`. Counts are dispatch
/// ticks. Scratch reservations are released before returning.
void as12AttackState(Task* playerTask);

#endif // INCLUDE_WEAPONS_AS12_H
