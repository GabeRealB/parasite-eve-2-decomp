/* Part of the hopper waves library; see hopper_waves.h. */

/// Pair spawner: reveals the first hopper with the 0x2E00 command carrying the
/// spawn argument, restarts the frame counter and advances.
void hopperWavePairRevealFirst(Task* arg0)
{
    OverlayEncounterPairWork* work = (OverlayEncounterPairWork*)arg0->work;
    GpEnemy*                  enemy;
    Task*                     task;
    TmdObject*                obj;
    ActorCommand              msg;

    enemy = work->enemy0;
    if (enemy != NULL) {
        task                   = enemy->task;
        obj                    = task->extra.tmd;
        obj->texturePageOffset = 3;
        obj->clutRowOffset     = 5;
        enemy->workType        = 0x900;
        msg.context.loc.stage  = 0;
        msg.context.loc.area   = 0x2E;
        msg.command            = arg0->spawnArg1.value;
        Gp_DispatchMsgPtr(task, ACTOR_COMMAND_MESSAGE_APPLY, &msg, 0);
    }
    work->frames = 0;
    arg0->state++;
}
