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
 */

#ifndef SRC_SHARED_PACED_WALK_H
#define SRC_SHARED_PACED_WALK_H

#include <psyq/sys/types.h>
#include <psyq/libgte.h>

#include "types.h"

#include "actors/actor.h"

#include "gameplay/enemy.h"
#include "gameplay/message.h"

#include "main/task_types.h"

/// The head of a walker's work block as the slot helpers and the placement
/// handler see it.
///
/// `pacedWalkTickAnim`, `pacedWalkResetAnim` and `pacedWalkPlace` are also
/// carried by walkers whose work block is a type of their own. Each of those
/// blocks opens with these four members, and the three reach nothing after
/// them; what follows is the walker's own.
typedef struct {
    MATRIX          light; // Light-direction matrix lent to the model object
    MATRIX          color; // Light-colour matrix lent to the model object
    ActorAnimRig20  rig;   // Playback storage of the twenty-part model; slots 1 to 19 are driven
    ActorEnemyState st;    // Animation request, heading last given the root and frames of walk left
} PacedWalkAnimWork;
STATIC_ASSERT_SIZEOF(PacedWalkAnimWork, 0x4EC);

/// Work block of a paced walker, allocated zeroed at its full size by the
/// walker's spawn state and kept at `Task::work`.
///
/// The model object borrows `light` and `color` for as long as the block
/// lives, and a sub-model carried on the walker is lit through the same two.
/// The first four members are laid out as `PacedWalkAnimWork`, which is how
/// the slot helpers and the placement handler view the block.
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

void pacedWalkUpdate(Task* task);
void pacedWalkTickAnim(Task* task);
void pacedWalkResetAnim(Task* task);
void pacedWalkBlendAnim(Task* task);
s32  pacedWalkTo(Task* task, s32 arg1, ActorTransform* target, s32 arg3);

s32 pacedWalkPlace(Task* task, s32 arg1, ActorTransform* placement, s32 arg3);

void pacedWalkFrame(Enemy* enemy, Task* task);
void pacedWalkSpawn(Enemy* enemy, Task* task);
s32  pacedWalkPlayAnim(Task* task, s32 arg1, AnimationPlayRequest* args, s32 arg3);
s32  pacedWalkShowPair(Task* task, s32 arg1, s32 flags, s32 arg3);

/* Defined by each package. */
void pacedWalkExit(Task* task);

#endif /* SRC_SHARED_PACED_WALK_H */
