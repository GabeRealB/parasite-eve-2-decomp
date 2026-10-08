/* Part of the Generator library; see generator.h. */

/// Teardown handler of the part task (its state 2), ticking only while the
/// gameplay mode `gSceneCombatState.actorControl` is 0. The first tick unlinks the enemy's lock-on
/// node and the part's collision object, drops the enemy's `recs`, sends the
/// main task's sound id `runningSoundId` a type-7 event, and undoes what the spawn
/// did for this generator kind (chosen by `kind`): the same room call with 0
/// instead of 1, and game flag 0x147 or 0x148 set to 1 where the spawn set it
/// to 0. The enemy is destroyed once the counter `teardownFrames` reaches 0x3D.
void generatorLifeSupportTeardown(Enemy* arg0, Task* arg1)
{
    GeneratorLifeSupportWork* part;
    GeneratorWork*            parentWork;

    part       = arg1->work;
    parentWork = arg1->parent->work;
    if (gSceneCombatState.actorControl == SCENE_COMBAT_ACTORS_RUNNING) {
        part->teardownFrames++;
        if (part->teardownFrames == 1) {
            worldTargetUnlinkNode(&arg0->node);
            worldCollisionUnlinkBody(&part->body);
            arg0->recs = 0;
            sndEvtRequestScriptStop(parentWork->runningSoundId, SOUND_SCRIPT_STOP_KEEP_RELEASE);
            if (part->kind == GENERATOR_BETA) {
                neoArkPowerPlant2SetView6SpritesHidden(0);
                gameFlagSetNibble(GAME_FLAG_POWER_PLANT_2_GENERATOR_PART_DOWN, 1);
            } else {
                neoArkPowerPlant1SetLifeSupportSpritesHidden(0);
                gameFlagSetNibble(GAME_FLAG_POWER_PLANT_1_GENERATOR_PART_DOWN, 1);
            }
        }
        if (part->teardownFrames >= 0x3D) {
            enemyDestroy(arg0, arg1);
        }
    }
}
