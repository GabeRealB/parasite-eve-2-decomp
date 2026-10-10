#ifndef INCLUDE_WEAPONS_P08_H
#define INCLUDE_WEAPONS_P08_H

#include "main/task_types.h"
#include "main/tmd_types.h"

/// Rigid held P08 pistol model selected for both magazine variants.
///
/// One root part holds 28 vertices and 26 lighting normals. Instances borrow
/// the loaded P08 package's geometry and writable command stream; release them
/// before replacing the package. Creation resolves draw handlers in place and
/// copies the identity root transform. Each primitive-buffer half contains four
/// textured Gouraud triangles and 24 quads, all directly transformed.
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
