/* The walk of the 20-slot NPCs that cutscene scripts move around, over a
 * singleton work block the package publishes. The 'walk to' message (0x7DD)
 * turns the model toward its target (away from it in mode 1) and divides the
 * planar distance into the mode's step count: 60 for mode 0, 15 for mode 1 and
 * 25 otherwise. The per-frame update reseeds the rig in states 1 and 2, then
 * in state 3 walks forward at the mode's speed while the walk clip has travel
 * left, queues the idle clip with a 10-frame blend when it runs out, turns
 * 0x33 a frame while the turn clip has frames left, and ticks the rig.
 *
 * Include this header in the prologue and each fragment at its function's
 * position. The walker's state belongs to the package, which defines it at
 * its own positions under these names:
 *
 *   SCRIPTED_WALK_WORK        the borrowed, published work pointer
 *   SCRIPTED_WALK_MODE        the signed halfword mode of the last walk
 *   SCRIPTED_WALK_BLEND_FRAMES the latched duration of the next blended reseed
 *
 * The defaults select `_gScriptedWalkWork`, `_gScriptedWalkMode` and
 * `_gScriptedWalkBlendFrames`.
 * actor_143900 and actor_461800 bind the mode to `gScriptedWalkModeValue`,
 * a scalar view of the first halfword in `_gScriptedWalkModeStorage`.
 * Nothing accesses the other halfword, whose role remains unproven.
 * A file with a second walker, as actor_143900 has, includes the fragments
 * again with the bindings selecting that walker's functions and state.
 *
 * The update and the walk-to handler take the block from Task::work, so a
 * package whose walker allocates a block of its own type names that type
 * through SCRIPTED_WALK_WORK_T before including this header. actor_143900 and
 * actor_260400 do; actor_143900 rebinds it around its second walker's copies.
 */

#ifndef SRC_SHARED_SCRIPTED_WALK_H
#define SRC_SHARED_SCRIPTED_WALK_H

#include <psyq/sys/types.h>
#include <psyq/libgte.h>

#include "types.h"

#include "actors/actor.h"

#include "main/task_types.h"

#ifndef SCRIPTED_WALK_WORK
/// Selects the task-owned work block published for this walker instance.
///
/// Bind before this header to a side-effect-free pointer expression whose
/// pointee has `ActorAnimRig20 rig` and the state members its fragments use:
/// `st.state`, `st.animId`, `st.appliedAnimId` and, for placement, `st.yaw`.
/// The state type may differ by carrier. Parenthesize expressions more complex
/// than an identifier. The spawn routine publishes the allocation held in
/// `Task::work`; the task dispatcher republishes it before each state call.
/// Animation fragments use it without a task argument; placement uses it to
/// retain the heading. It must be non-NULL and live during use;
/// teardown releases the allocation without clearing the published pointer.
///
/// The default selects the sole or first walker in the five carriers.
/// actor_143900 rebinds it to `_gScriptedWalkSecondWork` around its second
/// update, tick, reset, blend and placement copies, then restores the default.
/// There are no arguments or captured locals. Each dereference evaluates the
/// binding again, including after animation calls; it does not cache the pointer.
#define SCRIPTED_WALK_WORK (_gScriptedWalkWork)
#endif

/// Approach modes stored in the walker's signed halfword.
///
/// Distances are model-parent coordinate units per update, also used to divide
/// the target's planar distance into a travel countdown. Other halfword values
/// retain a 25-unit divisor but produce no translation in the update.
enum {
    SCRIPTED_WALK_MODE_FORWARD       = 0, // Face the target and step forward by 60
    SCRIPTED_WALK_MODE_BACKWARD      = 1, // Face away from the target and step backward by 15
    SCRIPTED_WALK_MODE_FORWARD_SHORT = 2, // Face the target and step forward by 25
};

#ifndef SCRIPTED_WALK_MODE
/// Selects the walker's writable signed-halfword approach mode.
///
/// Bind to a side-effect-free `s16` lvalue before this header. The walk-to
/// fragment stores the low halfword of the message argument; the update reads
/// it as `SCRIPTED_WALK_MODE_*`. Rebind around both fragments for another walker
/// and restore afterwards. No arguments or local identifiers are captured.
#define SCRIPTED_WALK_MODE (_gScriptedWalkMode)
#endif

/// Transition durations in whole normal-rate frames.
enum {
    SCRIPTED_WALK_DEFAULT_BLEND_FRAMES = 8,
    SCRIPTED_WALK_IDLE_BLEND_FRAMES    = 10,
};

#ifndef SCRIPTED_WALK_BLEND_FRAMES
/// Selects the walker's latched transition duration in whole normal-rate frames.
///
/// Bind to a side-effect-free, writable `s16` lvalue before this header or
/// rebind around both the update and blend fragments for another walker.
/// The play-animation handler stores the low signed halfword of
/// `AnimationPlayRequest.blendFrames`; walk completion stores
/// `SCRIPTED_WALK_IDLE_BLEND_FRAMES`. Plain resets leave the latch intact.
/// The blend fragment promotes it to `s32` for each child slot's seek: zero
/// requests no transition time, and 0..2047 keeps the playback timer nonnegative.
/// There is no range check. No arguments or local identifiers are captured;
/// each access evaluates the binding again, including after animation calls.
///
/// The default selects the sole or first walker in the four blend carriers.
/// actor_143900 binds `_gScriptedWalkSecondBlendFrames` around its second
/// update and blend copies, then restores the default. The storage belongs
/// to the carrier and remains live for that overlay's lifetime.
#define SCRIPTED_WALK_BLEND_FRAMES (_gScriptedWalkBlendFrames)
#endif

/// Work block of a scripted walker that carries two attachments, allocated
/// zeroed at its full size by the walker's spawn state and kept both at
/// `Task::work` and in the pointer selected by `SCRIPTED_WALK_WORK`.
///
/// Each attachment is a task of its own that draws a one-part model and hangs
/// that model's coordinate off one part of the walker's rig, the part being
/// the task's first spawn argument, so the model follows the part. The spawn
/// state starts the two from entries 1 and 2 of the walker's spawn table, the
/// model-draw message shows and hides them with the walker, and the walker's
/// exit callback kills both. A spawn that fails leaves its member NULL, which
/// neither of those two checks for.
///
/// actor_461800's first walker carries its two hand models this way, on parts
/// 8 and 12. actor_143900's second carries one model on part 1 and one on part
/// 12, and its command message shows either one and hides the other.
typedef struct {
    MATRIX          light;       // Light-direction matrix lent to the model object
    MATRIX          color;       // Light-colour matrix lent to the model object
    ActorAnimRig20  rig;         // Playback storage of the twenty-part model; slots 1 to 19 are driven
    ActorEnemyState st;          // Animation request, heading last given the root and frames of walk left
    s16             turnFrames;  // Frames the update still turns the model for while the turn clip plays
    Task*           attachment1; // Task of the model started from spawn-table entry 1
    Task*           attachment2; // Task of the model started from spawn-table entry 2
} ScriptedWalkAttachmentsWork;
STATIC_ASSERT_SIZEOF(ScriptedWalkAttachmentsWork, 0x4F8);

#ifndef SCRIPTED_WALK_WORK_T
/// Type `scriptedWalkUpdate` and `scriptedWalkTo` take the block at
/// `Task::work` as.
///
/// The walkers' blocks are their packages' own types, so each includer binds
/// the type its walker allocates; the default is the block of a walker with
/// two attachments. The two functions reach the animation request, the heading
/// and the walk and turn countdowns, so a bound type has to declare an
/// `ActorEnemyState st` and an `s16 turnFrames`, as every walker that carries
/// them does behind its two light matrices and its rig. The other fragments
/// reach the block through `SCRIPTED_WALK_WORK` and take its type from the
/// package's declaration of that global.
///
/// Bind before this header. The binding persists across the fragments'
/// inclusions; a file whose walkers differ in block type undefines and
/// redefines it around the copies of the walker that differs.
#define SCRIPTED_WALK_WORK_T ScriptedWalkAttachmentsWork
#endif

#ifndef SCRIPTED_WALK_TICK_ANIM
/// Selects the no-argument function that ticks the published walker's part animation.
///
/// Bind to a TU-private `void name(void)` function before this header, or undefine and
/// rebind around both the update and tick fragments for an additional walker.
/// The tick fragment defines the function; the update fragment calls it.
/// `SCRIPTED_WALK_WORK` must select the same walker's live, initialized rig.
/// The default serves the sole or first walker in all five carriers;
/// actor_143900 binds its second copy to `_scriptedWalkTickSecondAnim`.
#define SCRIPTED_WALK_TICK_ANIM _scriptedWalkTickAnim
#endif

#ifndef SCRIPTED_WALK_RESET_ANIM
/// Selects the private function that restarts this walker's requested part tracks.
///
/// Bind to a TU-private `void name(void)` function identifier before this header.
/// The reset fragment defines it; the update fragment calls it for
/// `ACTOR_ENEMY_ANIM_RESET`, then changes the state to `ACTOR_ENEMY_ANIM_TICK`.
/// For another walker, declare its static prototype in the carrier prologue
/// and rebind around both fragment copies with `SCRIPTED_WALK_WORK` selecting
/// that walker's live, initialized rig. Restore both bindings afterwards.
///
/// The default serves the sole or first walker in all five carriers;
/// actor_143900 selects `_scriptedWalkResetSecondAnim` for its second walker.
/// This identifier binding has no arguments, captured locals or side effects.
#define SCRIPTED_WALK_RESET_ANIM _scriptedWalkResetAnim
#endif

#ifndef SCRIPTED_WALK_BLEND_ANIM
/// Selects the function that blends this walker's requested child-part tracks.
///
/// Bind to a TU-private `void name(void)` function identifier before this header. The
/// blend fragment defines it; the update fragment calls it for
/// `ACTOR_ENEMY_ANIM_BLEND`, then advances the state to `ACTOR_ENEMY_ANIM_TICK`.
/// Both inclusions must select the same initialized, live `SCRIPTED_WALK_WORK`
/// and signed-halfword `SCRIPTED_WALK_BLEND_FRAMES` latch. An additional private
/// instance needs a static prototype in the carrier prologue before its caller.
/// Rebind around both fragments and restore the first walker's binding afterwards.
///
/// The default serves the sole or first walker in the four blend carriers;
/// actor_143900 binds its second copy to `_scriptedWalkBlendSecondAnim`.
/// actor_420700 carries only the tick and reset fragments and selects its
/// own fixed-duration blend function for this declaration.
/// This identifier alias takes no arguments and captures no local variables.
#define SCRIPTED_WALK_BLEND_ANIM _scriptedWalkBlendAnim
#endif

void scriptedWalkUpdate(Task* task);

/// Advances the selected walker's non-root animation slots and applies their poses.
///
/// `SCRIPTED_WALK_WORK` must select a live work block whose animation context
/// remains bound to its twenty slots, pose buffer, model and loaded clip data.
/// Slots 1 through 19 must have been reset or seeded for blended playback;
/// each consumes its configured signed rate in sixteenths of a frame, subject
/// to the death-playback adjustment. Slot 0 keeps the separately placed root.
/// Scratch-stack capacity and GTE requirements are those of `animationTickSlot`.
static void SCRIPTED_WALK_TICK_ANIM(void);

static void SCRIPTED_WALK_RESET_ANIM(void);
static void SCRIPTED_WALK_BLEND_ANIM(void);
s32         scriptedWalkTo(Task* task, s32 arg1, VECTOR* target, s32 mode);

s32 scriptedWalkPlace(Task* task, s32 arg1, ActorTransform* placement, s32 arg3);

#endif /* SRC_SHARED_SCRIPTED_WALK_H */
