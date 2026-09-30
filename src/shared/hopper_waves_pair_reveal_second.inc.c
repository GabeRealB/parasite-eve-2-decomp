/* Part of the hopper waves library; see hopper_waves.h. */

/// Pair spawner: drops dead hoppers; 60 frames later reveals the second with
/// the same command, then advances.
void hopperWavePairRevealSecond(Task* arg0)
{
    OverlayEncounterPairWork* work = (OverlayEncounterPairWork*)arg0->work;
    Enemy*                    enemy;
    Task*                     task;
    TmdObject*                obj;
    ActorCommand              msg;

    enemy = work->enemy1;
    hopperWavePairDropDead(arg0);
    if (work->enemy1 != NULL) {
        if (++work->frames <= 0x3C) {
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
        Gp_DispatchMsgPtr(task, ACTOR_COMMAND_MESSAGE_APPLY, &msg, 0);
    }
    work->frames = 0;
    arg0->state++;
}
