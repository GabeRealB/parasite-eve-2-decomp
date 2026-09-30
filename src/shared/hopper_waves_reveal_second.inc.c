/* Part of the hopper waves library; see hopper_waves.h. */

/// Second-kind spawner: after 60 frames sets the enemy's texture page and CLUT
/// row, marks it active and sends the 0x2A00 command with the spawn argument,
/// then advances.
void hopperWaveRevealSecond(Task* arg0)
{
    OverlayEncounterSingleWork* work = (OverlayEncounterSingleWork*)arg0->work;
    GpEnemy*                    enemy;
    Task*                       task;
    TmdObject*                  obj;
    ActorCommand                msg;

    enemy = work->enemy;
    task  = enemy->task;
    if (++work->frames > 60) {
        obj                    = task->extra.tmd;
        obj->texturePageOffset = 2;
        obj->clutRowOffset     = 4;
        enemy->workType        = 0x900;
        msg.context.loc.stage  = 0;
        msg.context.loc.area   = 0x2A;
        msg.command            = arg0->spawnArg1.value;
        Gp_DispatchMsgPtr(task, ACTOR_COMMAND_MESSAGE_APPLY, &msg, 0);
        arg0->state++;
    }
}
