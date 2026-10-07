/* Part of the reversing walker library; see reversing_walker.h. */

/// Initializes the reversing walker's owned work, messages and teardown callback.
///
/// Call once in task state 0 with a live nineteen-part TMD model, no existing
/// work and an owned enemy record in `spawnArg2.pointer`. Allocates zeroed
/// `ReverseWalkWork` on the primary heap, initially idle with backward walks
/// selected. No animation context is bound until a play request.
/// The model borrows the work's light and colour matrices until teardown.
/// Success advances the task to update state 1; failure releases the enemy
/// and starts task teardown, so the caller must not use them afterwards.
static void _reverseWalkSpawn(Task* task)
{
    enum { REVERSE_WALK_BUFFER_RELEASE_DISABLED = -1 };
    ReverseWalkWork* work;

    work = memCalloc(sizeof(ReverseWalkWork), false);
    if (work == NULL) {
        enemyTaskExit(task);
        return;
    }

    task->work               = work;
    work->model.animId       = ACTOR_MODEL_STATE_NONE;
    work->model.bank         = ACTOR_MODEL_STATE_NONE;
    work->freeCountdown      = REVERSE_WALK_BUFFER_RELEASE_DISABLED;
    work->walk.carry[0].word = 0;
    work->walk.carry[1].word = 0;
    work->walk.carry[2].word = 0;

    _reverseWalkBindLighting(task);

    task->msgTable     = gReverseWalkMessages;
    task->exitCallback = _reverseWalkExit;
    task->state       += 1;
}
