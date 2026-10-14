/* Part of the Generator library; see generator.h. */

/// Dispatches the Life Support task's spawn, hit or teardown state.
///
/// Task::state must be 0..2 and spawnArg2.pointer must hold its live Enemy.
/// The three-entry handler table is copied by value before dispatch.
static void _generatorLifeSupportTask(Task* task)
{
    EnemyTaskFuncTable3 stateHandlers;

    stateHandlers = _gGeneratorLifeSupportStates;
    stateHandlers.funcs[task->state](task->spawnArg2.pointer, task);
}

/// Removes the broken Life Support sphere and restores its room presentation.
///
/// Requires live part work and parent Generator work, with teardownFrames
/// reset by the killing hit. Counts only running actor ticks. The first tick
/// unlinks target/collision records, stops the parent's running sound, shows
/// the variant's room sprites and sets its part-down flag. Tick 61 destroys
/// the child and releases its work; paused/hidden actor control does nothing.
static void _generatorLifeSupportTeardown(Enemy* enemy, Task* task)
{
    enum { GENERATOR_LIFE_SUPPORT_TEARDOWN_TICKS = 61 };
    GeneratorLifeSupportWork* part;
    GeneratorWork*            parentWork;

    part       = task->work;
    parentWork = task->parent->work;
    if (gSceneCombatState.actorControl == SCENE_COMBAT_ACTORS_RUNNING) {
        part->teardownFrames++;
        if (part->teardownFrames == 1) {
            worldTargetUnlinkNode(&enemy->node);
            worldCollisionUnlinkBody(&part->body);
            enemy->recs = NULL;
            sndEvtRequestScriptStop(parentWork->runningSoundId, SOUND_SCRIPT_STOP_KEEP_RELEASE);
            if (part->kind == GENERATOR_BETA) {
                neoArkPowerPlant2SetView6SpritesHidden(0);
                gameFlagSetNibble(GAME_FLAG_POWER_PLANT_2_GENERATOR_PART_DOWN, 1);
            } else {
                neoArkPowerPlant1SetLifeSupportSpritesHidden(0);
                gameFlagSetNibble(GAME_FLAG_POWER_PLANT_1_GENERATOR_PART_DOWN, 1);
            }
        }
        if (part->teardownFrames >= GENERATOR_LIFE_SUPPORT_TEARDOWN_TICKS) {
            enemyDestroy(enemy, task);
        }
    }
}

/// Latches script commands that release the body's death and battle-exit waits.
///
/// ACTOR_COMMAND_MESSAGE_APPLY borrows a readable ActorCommand for this call.
/// Commands 0..3 select none, death, battle exit or both; other values do
/// nothing. Existing bits remain set. The second payload is ignored; returns 0.
static s32 _generatorSetReleaseBits(Task* task, s32 messageId, const ActorCommand* command, s32 unusedArg3)
{
    GeneratorWork* work;

    work = task->work;
    switch (command->command) {
        case 0:
            break;
        case GENERATOR_RELEASE_DEATH:
            work->releaseBits |= GENERATOR_RELEASE_DEATH;
            break;
        case GENERATOR_RELEASE_BATTLE_EXIT:
            work->releaseBits |= GENERATOR_RELEASE_BATTLE_EXIT;
            break;
        case GENERATOR_RELEASE_DEATH | GENERATOR_RELEASE_BATTLE_EXIT:
            work->releaseBits |= GENERATOR_RELEASE_DEATH | GENERATOR_RELEASE_BATTLE_EXIT;
            break;
    }
    return 0;
}

/// Answers ACTOR_MESSAGE_IS_PRESENT with the generator body's alive latch.
///
/// Requires initialized GeneratorWork. Returns 1 before the killing hit and
/// 0 throughout the held death sequence; both payload words are ignored.
static s32 _generatorIsAlive(Task* task, s32 messageId, s32 unusedArg2, s32 unusedArg3)
{
    return ((GeneratorWork*)task->work)->alive;
}

/// Body handlers indexed by `Task::state`: spawn, active update and held death.
///
/// The dispatcher copies all three typed callbacks before selecting one.
static const EnemyTaskFuncTable3 _gGeneratorTaskStates = {
    {
        [GENERATOR_TASK_SPAWN]    = _generatorSpawn,
        [GENERATOR_TASK_ACTIVE]   = _generatorTickState,
        [GENERATOR_TASK_TEARDOWN] = _generatorDeathState,
    },
};

/// Dispatches the generator body's spawn, active or death state.
///
/// Task::state must be 0..2 and spawnArg2.pointer must hold its live Enemy.
/// The three-entry handler table is copied by value before dispatch.
static void _generatorTask(Task* task)
{
    EnemyTaskFuncTable3 stateHandlers;

    stateHandlers = _gGeneratorTaskStates;
    stateHandlers.funcs[task->state](task->spawnArg2.pointer, task);
}
