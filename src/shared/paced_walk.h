/* Animation requests and scripted travel for twenty-part NPCs whose work
 * block lives at Task::work. The walk-to message aims the model root and
 * records the remaining twelve-unit travel attempts. An update consumes a
 * blend or reset request before ordinary ticking. Tick updates advance the
 * root only for walk clip 4, counting an attempt even when actor freezing
 * suppresses movement, then advance slots 1 through 19. Arrival records idle
 * clip 1 and a ten-frame blend duration without changing the playing tracks
 * or request state; a later reseed request applies a clip change.
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
/// `PACED_WALK_PLACE` take the block at `Task::work` as.
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

#ifndef PACED_WALK_UPDATE
/// Function identifier selecting a paced walker's request and movement update.
///
/// Defaults to `_pacedWalkUpdate`, with signature `static void (Task* task)`.
/// The update requires a live `PacedWalkWork` at `Task::work`, independent of
/// `PACED_WALK_WORK_T`. Select the matching tick, reset and blend instances
/// with the three animation bindings before including the update fragment.
/// Bind before this header for its static declaration, or undefine and rebind
/// around a further fragment copy. Declare additional instances `static` in
/// the carrier's prologue before their callers. Restore the first binding
/// afterwards if later fragments call the first walker.
/// This object-like alias evaluates no arguments, captures no locals and uses
/// no stringification or token pasting. Header guards select the default once.
#define PACED_WALK_UPDATE _pacedWalkUpdate
#endif

static void PACED_WALK_UPDATE(Task* task);
static void PACED_WALK_TICK_ANIM(Task* task);
static void PACED_WALK_RESET_ANIM(Task* task);
static void PACED_WALK_BLEND_ANIM(Task* task);
s32         pacedWalkTo(Task* task, s32 arg1, ActorTransform* target, s32 arg3);

#ifndef PACED_WALK_PLACE
/// Selects the placement callback defined by a walker's placement fragment.
///
/// Defaults to `pacedWalkPlace`. Bind to a function identifier with signature
/// `s32 name(Task* task, s32 messageId, const ActorTransform* placement, s32 unusedArgument)`.
/// Bind before this header for the first instance's declaration, or undefine
/// and rebind around an additional placement fragment. Declare additional
/// instances in the carrier's prologue before their message tables; a static
/// declaration gives the fragment's definition internal linkage.
/// `PACED_WALK_WORK_T` must select the same receiver's allocated work type,
/// with a writable `ActorEnemyState st`. Restore both bindings afterwards.
/// This object-like alias evaluates no arguments, captures no locals and
/// uses no stringification or token pasting. Header guards select the default
/// only on the first inclusion.
#define PACED_WALK_PLACE pacedWalkPlace
#endif

s32 PACED_WALK_PLACE(Task* task, s32 messageId, const ActorTransform* placement, s32 unusedArgument);

void       pacedWalkFrame(Enemy* enemy, Task* task);
void       pacedWalkSpawn(Enemy* enemy, Task* task);
static s32 _pacedWalkPlayAnimation(Task* task, s32 messageId, const AnimationPlayRequest* request, s32 unusedArgument);
s32        pacedWalkShowPair(Task* task, s32 arg1, s32 flags, s32 arg3);

/* Defined by each package. */
void pacedWalkExit(Task* task);

#endif /* SRC_SHARED_PACED_WALK_H */
