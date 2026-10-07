/* A cutscene NPC walker that, when spawned with spawnArg1 set, carries a
 * second model hung off part 7 of its own (the pair-walk arrangement made
 * optional), keeps its work block in Task::work and walks fast: 30 units a
 * frame, distance counted in thirtieths. Each frame it relights itself from a
 * point 0x320 above its root, steps the animation, ramps a head-turn weight
 * toward the player and draws the walker shadow. Twelve scripted clips. Reuses
 * paced_walk's tick/reset/blend and walker's shadow.
 *
 * Include this header in the prologue and each fragment at its function's
 * position.
 */

#ifndef SRC_SHARED_STRIDE_WALK_H
#define SRC_SHARED_STRIDE_WALK_H

#include <psyq/sys/types.h>
#include <psyq/libgte.h>

#include "types.h"

#include "actors/actor.h"

#include "gameplay/enemy.h"
#include "gameplay/message.h"

#include "main/task_types.h"

#include "actor_messages.h"

/// Parent-coordinate units per attempted travel update and per target-distance step.
enum { STRIDE_WALK_STEP_DISTANCE = 30 };

/// What the walker's head does each frame, kept in `StrideWalkWork::turnMode`:
/// the value the turn command last carried. The talk scripts send
/// `STRIDE_WALK_TURN_PLAYER` when a scene opens and `STRIDE_WALK_TURN_RELEASE`
/// when it closes; any value other than `STRIDE_WALK_TURN_PLAYER` releases.
enum {
    STRIDE_WALK_TURN_RELEASE = 0, // Let the head settle back onto the animation's pose
    STRIDE_WALK_TURN_PLAYER  = 1, // Turn the head toward the player
};

/// Full weight of the head turn, `StrideWalkWork::turnWeight`: the head aims
/// straight at the player, within the turn's yaw and pitch limits.
#define STRIDE_WALK_TURN_WEIGHT_FULL 0x1000

/// Work block of a stride walker, allocated zeroed at its full size by the
/// walker's spawn state and kept at `Task::work`.
///
/// It opens as the paced walker's block does, through `blendFrames`, which is
/// what lets the walker carry the paced walk's slot tick, reset, blend and
/// placement over this type. The model object borrows `light` and `color` for
/// as long as the block lives, and the carried sub-model is lit through the
/// same two.
typedef struct {
    MATRIX          light;       // Light-direction matrix lent to the model object
    MATRIX          color;       // Light-colour matrix lent to the model object
    ActorAnimRig20  rig;         // Playback storage of the twenty-part model; slots 1 to 19 are driven
    ActorEnemyState st;          // Animation request, heading last given the root and frames of walk left
    s16             blendFrames; // Whole frames the next blended reseed takes to reach the requested clip
    s16             turnMode;    // What the head turn does each frame (`STRIDE_WALK_TURN_*`), as the turn command last set it
    s16             turnWeight;  // Share of the angle between the head's pose and the player it is turned through each frame, 0 to `STRIDE_WALK_TURN_WEIGHT_FULL`; rises while `turnMode` is `STRIDE_WALK_TURN_PLAYER`, falls otherwise
    Task*           pairTask;    // Task of the sub-model the walker carries, a child of the walker's task that is shown and hidden with it; NULL when the walker was spawned without one
    Enemy*          enemy;       // Enemy the walker's task belongs to; recorded at spawn, never read
} StrideWalkWork;
STATIC_ASSERT_SIZEOF(StrideWalkWork, 0x4FC);

void        strideWalkSpawn(Enemy* enemy, Task* task);
static void _strideWalkUpdate(Task* task);
static void _strideWalkFrame(Enemy* enemy, Task* task);
static s32  _strideWalkPlayAnimation(Task* task, s32 messageId, const AnimationPlayRequest* request, s32 secondArg);
static s32  _strideWalkSetModelDraw(Task* task, s32 messageId, s32 drawFlags, s32 secondArg);
static s32  _strideWalkSetWalkTarget(Task* task, s32 messageId, const ActorTransform* target, s32 secondArg);
static void _strideWalkSubModelTask(Task* task);

/* Defined by each package. */
void strideWalkExit(Task* task);

#endif /* SRC_SHARED_STRIDE_WALK_H */
