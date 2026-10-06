/* The nineteen-part NPC walker whose carried model shares its lighting and
 * draw flags. Animation, message and carried-model fragments have static
 * instances in each carrier. Common declarations live here; a carrier declares
 * any optional handlers it installs in its own prologue.
 *
 * _pairWalkUpdate has two variants: pair_walk_update.inc.c advances the root
 * by 17 parent-coordinate units per travel tick; pair_walk_update_model.inc.c
 * advances the model root by 12. The latter carriers also include the spawn
 * and walk-to fragments and define pairWalkExit themselves.
 *
 * Include this header in the prologue and each fragment at its function's
 * position.
 */

#ifndef SRC_SHARED_PAIR_WALK_H
#define SRC_SHARED_PAIR_WALK_H

#include <psyq/sys/types.h>
#include <psyq/libgte.h>

#include "common.h"

#include "actors/actor.h"

#include "gameplay/enemy.h"
#include "gameplay/message.h"

#include "main/task_types.h"

#include "actor_messages.h"

/// Parts driven by the walker and clip choices recorded by its walk step.
enum {
    PAIR_WALK_FIRST_ANIM_SLOT   = 1,
    PAIR_WALK_ANIM_IDLE         = 1,
    PAIR_WALK_ANIM_WALK         = 4,
    PAIR_WALK_IDLE_BLEND_FRAMES = 10,
};

/// Parent-coordinate units per travel tick of the model-step variant.
enum { PAIR_WALK_MODEL_STEP_UNITS = 12 };

/// Work block of a pair walker, allocated zeroed at its full size by the
/// walker's spawn state and kept at `Task::work`.
///
/// The model object borrows `light` and `color` for as long as the block
/// lives, and the second model the walker carries is lit through the same
/// two.
typedef struct {
    MATRIX          light;       // Light-direction matrix lent to the model object
    MATRIX          color;       // Light-colour matrix lent to the model object
    ActorAnimRig19  rig;         // Playback storage of the nineteen-part model; slots 1 to 18 are driven
    ActorEnemyState st;          // Animation request, heading last given the root and frames of walk left
    s16             blendFrames; // Whole frames the next blended reseed takes to reach the requested clip
    Task*           pairTask;    // Task of the second model the walker carries, shown and hidden with it
    Enemy*          enemy;       // Enemy the walker's task belongs to; recorded at spawn, never read
} PairWalkWork;
STATIC_ASSERT_SIZEOF(PairWalkWork, 0x4C0);

static void _pairWalkUpdate(Task* task);
static void _pairWalkTickAnim(Task* task);
static void _pairWalkResetAnim(Task* task);
static void _pairWalkReseedAnim(Task* task);
static s32  _pairWalkSetVisibility(Task* task, s32 messageId, s32 flags, s32 unusedArg);
static s32  _pairWalkPlace(Task* task, s32 messageId, const ActorTransform* placement, s32 unusedArg);

void pairWalkSpawn(Enemy* enemy, Task* task);

/* Defined by each package. */
void pairWalkExit(Task* task);

#endif /* SRC_SHARED_PAIR_WALK_H */
