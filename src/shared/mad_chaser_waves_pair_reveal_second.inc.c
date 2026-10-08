/* Part of the Mad Chaser waves library; see mad_chaser_waves.h. */

/// Pair spawner: drops dead Mad Chasers; 60 frames later reveals the second with
/// the same command, then advances.
void madChaserWavePairRevealSecond(Task* arg0)
{
    OverlayEncounterPairWork* work = arg0->work;
    Enemy*                    enemy;
    Task*                     task;
    TmdObject*                obj;
    ActorCommand              msg;

    enemy = work->enemy1;
    _overlayEncounterForgetDeadPairMembers(arg0);
    if (work->enemy1 != NULL) {
        if (++work->frames <= 60) {
            return;
        }
        task                   = work->enemy1->task;
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
