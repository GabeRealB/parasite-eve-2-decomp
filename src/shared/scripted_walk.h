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
 *   gScriptedWalkWork         the published work block
 *   gScriptedWalkMode         the mode of the last walk
 *   gScriptedWalkBlendFrames  the blend the next reseed uses
 *
 * A package that does not address the mode as a plain `s16 gScriptedWalkMode`
 * names that halfword through SCRIPTED_WALK_MODE before including this header.
 * actor_143900 and actor_461800 bind it to `gScriptedWalkModeValue`, the
 * halfword at the start of their four-byte mode symbol. A file with a second
 * walker, as actor_143900 has, includes the fragments again with the library
 * names defined to that walker's own.
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

#ifndef SCRIPTED_WALK_MODE
#define SCRIPTED_WALK_MODE gScriptedWalkMode
#endif

/// Work block of a scripted walker that carries two attachments, allocated
/// zeroed at its full size by the walker's spawn state and kept both at
/// `Task::work` and in the walker's `gScriptedWalkWork`.
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
/// reach the block through `gScriptedWalkWork` and take its type from the
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
/// Bind to a `void name(void)` function before this header, or undefine and
/// rebind around both the update and tick fragments for an additional walker.
/// The tick fragment defines the function; the update fragment calls it.
/// `gScriptedWalkWork` must select the same walker's live, initialized rig.
/// The default serves the sole or first walker in all five carriers;
/// actor_143900 binds its second copy to `_scriptedWalkTickSecondAnim`.
#define SCRIPTED_WALK_TICK_ANIM scriptedWalkTickAnim
#endif

void scriptedWalkUpdate(Task* task);
void SCRIPTED_WALK_TICK_ANIM(void);
void scriptedWalkResetAnim(void);
void scriptedWalkBlendAnim(void);
s32  scriptedWalkTo(Task* task, s32 arg1, VECTOR* target, s32 mode);

s32 scriptedWalkPlace(Task* task, s32 arg1, ActorTransform* placement, s32 arg3);

#endif /* SRC_SHARED_SCRIPTED_WALK_H */
