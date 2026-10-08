/* Part of the Generator library; see generator.h. */

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
