/* The walk of the NPCs that cutscene scripts move around, in the version that
 * keeps its work block in Task::work. The 'walk to' message (0x7DD) aims the
 * model's root coordinate at a target and records the distance in twelfths;
 * the per-frame body reseeds the 20-slot rig in states 1 and 2, then in state
 * 3 steps the root 12 units forward per frame while the walk clip has travel
 * left, switches to the idle clip with a 10-frame blend when it runs out, and
 * ticks the rig.
 *
 * Include this header in the prologue and each fragment at its function's
 * position. A file with several walkers, as actor_460200 has, includes the
 * fragments again for each further walker with the library names defined to
 * that walker's own.
 *
 * A walker that keeps a work block of its own type and carries only the slot
 * tick, the slot reset, the blended reseed or the placement handler names that
 * type through PACED_WALK_WORK_T before including this header. actor_161500
 * (the stride walker's StrideWalkWork) and actor_450800 do; actor_460200
 * rebinds it to StrideWalkWork around its second walker's copies.
 */

#ifndef SRC_SHARED_PACED_WALK_H
#define SRC_SHARED_PACED_WALK_H

#include <psyq/sys/types.h>
#include <psyq/libgte.h>

#include "common.h"

#include "actors/actor.h"

#include "gameplay/enemy.h"
#include "gameplay/message.h"

#include "main/task_types.h"

/// Work block of a paced walker, allocated zeroed at its full size by the
/// walker's spawn state and kept at `Task::work`.
///
/// The model object borrows `light` and `color` for as long as the block
/// lives, and a sub-model carried on the walker is lit through the same two.
typedef struct {
    MATRIX          light;       // Light-direction matrix lent to the model object
    MATRIX          color;       // Light-colour matrix lent to the model object
    ActorAnimRig20  rig;         // Playback storage of the twenty-part model; slots 1 to 19 are driven
    ActorEnemyState st;          // Animation request, heading last given the root and frames of walk left
    s16             blendFrames; // Whole frames the next blended reseed takes to reach the requested clip
    s16             smoking;     // Nonzero once a script has set the walker smoking: the frame body then emits smoke puffs from its parts. Never cleared
    Task*           pairTask;    // Task of the sub-model the walker carries, shown and hidden with it; NULL in a package that spawns none
    Enemy*          enemy;       // Enemy the walker's task belongs to; recorded at spawn, never read
} PacedWalkWork;
STATIC_ASSERT_SIZEOF(PacedWalkWork, 0x4F8);

#ifndef PACED_WALK_WORK_T
/// Type `_pacedWalkTickAnim`, `PACED_WALK_RESET_ANIM`, `PACED_WALK_BLEND_ANIM` and
/// `pacedWalkPlace` take the block at `Task::work` as.
///
/// Walkers whose work block is a type of their own carry some of those four,
/// so each includer binds the type its walker allocates; the default is the
/// paced walker's own block. The tick, the reset and the placement reach only
/// the rig and the animation state, so a bound type has to declare an
/// `ActorAnimRig20 rig` and an `ActorEnemyState st`, as every walker of this
/// family does behind its two light matrices. An includer that also carries
/// the blended reseed binds a type with an `s16 blendFrames` as well, as the
/// stride walker's `StrideWalkWork` has.
///
/// Bind before this header. The binding persists across the fragments'
/// inclusions; a file whose walkers differ in block type undefines and
/// redefines it around the copies of the walker that differs.
#define PACED_WALK_WORK_T PacedWalkWork
#endif

#ifndef PACED_WALK_TICK_ANIM
/// Function identifier shared by a walker's private slot tick and update calls.
///
/// The default is `_pacedWalkTickAnim`. Bind to a function with signature
/// `void (Task* task)` before this header or around a further fragment copy.
/// The header declares the selected function `static`; declare further copies
/// `static` in the carrier's prologue before including their update fragments.
/// Use the same identifier at the update call and tick definition, and bind
/// `PACED_WALK_WORK_T` to that walker's allocated work type at the definition.
/// The header guard runs this default selection only on the first inclusion;
/// undefine the binding before selecting another walker. An object-like alias,
/// it evaluates no arguments and captures no local identifiers.
#define PACED_WALK_TICK_ANIM _pacedWalkTickAnim
#endif

#ifndef PACED_WALK_RESET_ANIM
/// Function identifier shared by a walker's private clip restart and update calls.
///
/// The default is `_pacedWalkResetAnim`, with signature `void (Task* task)`.
/// Bind before this header or around another reset fragment's inclusion.
/// The header and fragment declare the selected instance `static`; declare
/// further instances `static` in the carrier's prologue before their callers.
/// Select the same identifier at the reset definition and its update calls,
/// with `PACED_WALK_WORK_T` bound to that walker's allocated type at the
/// definition. Undefine before rebinding: the header guard selects the default
/// only on the first inclusion. This object-like alias evaluates no arguments,
/// captures no local identifiers and requires no token pasting or stringification.
#define PACED_WALK_RESET_ANIM _pacedWalkResetAnim
#endif

#ifndef PACED_WALK_BLEND_ANIM
/// Function identifier binding a walker's private buffered blend to its update calls.
///
/// Defaults to `_pacedWalkBlendAnim`; every selected instance has signature
/// `static void (Task* task)`. The header declares the first instance and the
/// blend fragment defines it. Bind before this header's first inclusion, or
/// undefine and rebind around another walker's update and blend fragments.
/// Declare additional instances `static` in the carrier's prologue before
/// their callers. Header guards select the default only once.
/// At each blend definition, `PACED_WALK_WORK_T` must name that walker's
/// allocated type with `ActorAnimRig20 rig`, `ActorEnemyState st` and
/// `s16 blendFrames`. This object-like alias evaluates no arguments, captures
/// no local identifiers and uses no stringification or token pasting.
#define PACED_WALK_BLEND_ANIM _pacedWalkBlendAnim
#endif

void        pacedWalkUpdate(Task* task);
static void PACED_WALK_TICK_ANIM(Task* task);
static void PACED_WALK_RESET_ANIM(Task* task);
static void PACED_WALK_BLEND_ANIM(Task* task);
s32         pacedWalkTo(Task* task, s32 arg1, ActorTransform* target, s32 arg3);

s32 pacedWalkPlace(Task* task, s32 arg1, ActorTransform* placement, s32 arg3);

void pacedWalkFrame(Enemy* enemy, Task* task);
void pacedWalkSpawn(Enemy* enemy, Task* task);
s32  pacedWalkPlayAnim(Task* task, s32 arg1, AnimationPlayRequest* args, s32 arg3);
s32  pacedWalkShowPair(Task* task, s32 arg1, s32 flags, s32 arg3);

/* Defined by each package. */
void pacedWalkExit(Task* task);

#endif /* SRC_SHARED_PACED_WALK_H */
