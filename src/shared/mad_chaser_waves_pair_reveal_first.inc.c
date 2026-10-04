/* Part of the Mad Chaser waves library; see mad_chaser_waves.h. */

/// Pair spawner: reveals the first Mad Chaser with the 0x2E00 command carrying the
/// spawn argument, restarts the frame counter and advances.
void madChaserWavePairRevealFirst(Task* arg0)
{
    OverlayEncounterPairWork* work = arg0->work;
    Enemy*                    enemy;
    Task*                     task;
    TmdObject*                obj;
    ActorCommand              msg;

    enemy = work->enemy0;
    if (enemy != NULL) {
        task                   = enemy->task;
        obj                    = task->extra.tmd;
        obj->texturePageOffset = 3;
        obj->clutRowOffset     = 5;
        enemy->workType        = ENEMY_WORK_PLAIN;
        msg.context.loc.stage  = 0;
        msg.context.loc.area   = 0x2E;
        msg.command            = arg0->spawnArg1.value;
        TASK_MESSAGE_DISPATCH_POINTER(task, ACTOR_COMMAND_MESSAGE_APPLY, &msg, 0);
    }
    work->frames = 0;
    arg0->state++;
}
