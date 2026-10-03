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

/// The head every walker's work block starts with: the model's light and
/// colour matrices, its rig and animation state, and the frames of turning
/// left while the turn clip plays. What follows is the package's own.
typedef struct ScriptedWalkWork {
    MATRIX          light;
    MATRIX          color;
    ActorAnimRig20  rig;
    ActorEnemyState st;
    s16             turnFrames;
} ScriptedWalkWork;

void scriptedWalkUpdate(Task* task);
void scriptedWalkTickAnim(void);
void scriptedWalkResetAnim(void);
void scriptedWalkBlendAnim(void);
s32  scriptedWalkTo(Task* task, s32 arg1, VECTOR* target, s32 mode);

s32 scriptedWalkPlace(Task* task, s32 arg1, ActorTransform* placement, s32 arg3);

#endif /* SRC_SHARED_SCRIPTED_WALK_H */
