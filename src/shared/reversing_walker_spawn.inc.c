/* Part of the reversing walker library; see reversing_walker.h. */

/// Spawn state of the enemy actor: allocates the 0x4C8-byte work block that
/// every later handler reads through `Task::work`, seeds the three -1 bytes
/// and three cleared words the work's own init expects, republishes the light
/// and colour matrices onto the display object, then installs the message
/// table and the exit handler. An allocation failure ends the task instead of
/// leaving a half-built actor behind.
void reverseWalkSpawn(Task* arg0)
{
    ReverseWalkWork* work;

    work = memCalloc(sizeof(ReverseWalkWork), false);
    if (work == NULL) {
        enemyTaskExit(arg0);
        return;
    }

    arg0->work               = work;
    work->model.animId       = ACTOR_MODEL_STATE_NONE;
    work->model.bank         = ACTOR_MODEL_STATE_NONE;
    work->freeCountdown      = -1;
    work->walk.carry[0].word = 0;
    work->walk.carry[1].word = 0;
    work->walk.carry[2].word = 0;

    reverseWalkBindLighting(arg0);

    arg0->msgTable     = gReverseWalkMessages;
    arg0->exitCallback = reverseWalkExit;
    arg0->state       += 1;
}
