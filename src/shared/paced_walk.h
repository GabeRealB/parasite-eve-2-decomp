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

#include "actor_messages.h"

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

#ifndef PACED_WALK_SET_WALK_TARGET
/// Function identifier selecting a paced walker's private walk-target callback.
///
/// Defaults to `_pacedWalkSetWalkTarget`, with signature
/// `static s32 (Task* task, s32 messageId, const ActorTransform* target, s32 unusedArgument)`.
/// The callback records heading and travel attempts without starting a clip.
/// Both the header declaration and fragment definition are static. Bind before
/// this header's first inclusion, or undefine and rebind around an additional
/// walk-target fragment. Declare further instances static in the carrier's
/// prologue before their message tables, using the same identifier in the
/// table and at the fragment inclusion. Each instance requires live
/// `PacedWalkWork` at `Task::work`, independent of `PACED_WALK_WORK_T`.
/// Restore the first binding afterwards if later fragments use that walker.
/// The header guard selects the default only once. This object-like alias
/// evaluates no arguments, captures no locals and uses no stringification
/// or token pasting.
#define PACED_WALK_SET_WALK_TARGET _pacedWalkSetWalkTarget
#endif

static s32 PACED_WALK_SET_WALK_TARGET(Task* task, s32 messageId, const ActorTransform* target, s32 unusedArgument);

#ifndef PACED_WALK_PLACE
/// Function identifier selecting a walker's private placement-message callback.
///
/// Defaults to `_pacedWalkPlace`, with signature
/// `static s32 (Task* task, s32 messageId, const ActorTransform* placement, s32 unusedArgument)`.
/// The header declares the first selected instance and the placement fragment
/// defines it, both `static`. Bind before this header's first inclusion, or
/// undefine and rebind around another placement fragment. Declare additional
/// instances `static` in the carrier's prologue before their message tables,
/// using the same identifier in the table and at the fragment inclusion.
/// At each definition, `PACED_WALK_WORK_T` must select the receiver's allocated
/// work type with `ActorEnemyState st`; placement writes its signed `st.yaw`.
/// Restore both bindings afterwards if later fragments use the first walker.
/// This object-like alias evaluates no arguments, captures no locals and
/// uses no stringification or token pasting. Header guards select the default
/// only on the first inclusion.
#define PACED_WALK_PLACE _pacedWalkPlace
#endif

static s32 PACED_WALK_PLACE(Task* task, s32 messageId, const ActorTransform* placement, s32 unusedArgument);

#ifndef PACED_WALK_SET_PAIR_MODEL_DRAW
/// Function identifier selecting a paced walker's private paired model-draw callback.
///
/// Defaults to `_pacedWalkSetPairModelDraw`, with signature
/// `static s32 (Task* task, s32 messageId, s32 requestFlags, s32 unusedArg)`.
/// Declare every selected instance `static` in the carrier's prologue before
/// its message table; the draw fragment also defines it `static`. Bind before
/// this header's first inclusion, or undefine and rebind around an additional
/// fragment copy. Use the same identifier in the table and at the definition,
/// then restore the first binding afterwards. actor_160600 carries the default;
/// actor_460200 carries the default and `_pacedWalkSetSoldierCModelDraw`.
/// A nonzero `Task::spawnArg1.value` requires live `PacedWalkWork` at `Task::work`
/// with a live paired TMD task, independent of `PACED_WALK_WORK_T`. Zero makes
/// both model pointers alias the receiver without dereferencing the work block.
/// `requestFlags` carries `ACTOR_MESSAGE_PAIR_*` bits, not TMD object flags.
/// This object-like alias evaluates no arguments, captures no locals and uses
/// no stringification or token pasting. Header guards select the default once.
#define PACED_WALK_SET_PAIR_MODEL_DRAW _pacedWalkSetPairModelDraw
#endif

static void _pacedWalkFrame(Enemy* unusedEnemy, Task* task);
static void _pacedWalkSpawn(Enemy* enemy, Task* task);
static s32  _pacedWalkPlayAnimation(Task* task, s32 messageId, const AnimationPlayRequest* request, s32 unusedArgument);

/* Private exit callback of each carrier that includes the spawn fragment. */
static void _pacedWalkExit(Task* task);

#endif /* SRC_SHARED_PACED_WALK_H */
